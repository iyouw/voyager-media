#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "emscripten.h"
#include "memory_stream.h"

#define IO_BUFFER_SIZE (MEMORY_PAGE * 3)
#define STORE_SIZE (MEMORY_PAGE * 10)

typedef void (*VideoFrameParsedCallback)(uint8_t *ptr, long size);
typedef void (*AudioFrameParsedCallback)(uint8_t *ptr, long size);

// avio
static uint8_t *io_buffer;
static AVIOContext *io_ctx;
static MemoryStream *store;

// format & decode
static AVPacket *pkt;
static AVFrame *frame;
static AVStream *video_stream;
static AVStream *audio_stream;

static AVFormatContext *fmt_ctx;
static AVCodecContext *video_dec_ctx;
static AVCodecContext *audio_dec_ctx;

static uint8_t * video_frame_data[4] = { NULL };
static int video_frame_line_size[4];
static long video_frame_size;
static enum AVPixelFormat pix_fmt;

static pthread_t demux_decode_t;
static pthread_mutex_t mutex;
static pthread_cond_t cond;

static VideoFrameParsedCallback fireVideoFrameParsed;
static AudioFrameParsedCallback fireAudioFrameParsed;

static int opened = 0;

/*************************************************/
/*** internal section ****************************/
/*************************************************/
static int read_file_store(void *opaque, uint8_t *buffer, int buffer_size)
{
  MemoryStream *ms = opaque;

  if (pthread_mutex_lock(&mutex) != 0)
  {
    fprintf(stderr, "Failed to lock muted\n");
    return AVERROR(EINVAL);
  }

  if (!ms->is_done)
  {
    if (pthread_cond_wait(&cond, &mutex) != 0)
    {
      fprintf(stderr, "Could not wait cond!\n");
      return AVERROR(EINVAL);
    }
  }
  buffer_size = FFMIN(buffer_size, memory_stream_get_available(ms));
  if (buffer_size == 0)
  {
    return AVERROR_EOF;
  }
  int bytes_read = memory_stream_read(ms, buffer, buffer_size);
  if (pthread_mutex_unlock(&mutex) != 0)
  {
    fprintf(stderr, "Failed to unlock mutex!\n");
    return AVERROR(EINVAL);
  }
  return bytes_read;
}

static int read_stream_store(void *opaque, uint8_t *buffer, int buffer_size)
{
  int ret;
  MemoryStream *ms = opaque;

  if (pthread_mutex_lock(&mutex) != 0)
  {
    fprintf(stderr, "Failed to lock muted\n");
    return AVERROR(EINVAL);
  }
  buffer_size = FFMIN(buffer_size, memory_stream_get_available(ms));
  if (buffer_size == 0)
  {
    if (pthread_cond_wait(&cond, &mutex) != 0)
    {
      fprintf(stderr, "Could not wait cond!\n");
      return AVERROR(EINVAL);
    }
    ret = 0;
  }
  else
  {
    ret = memory_stream_read(ms, buffer, buffer_size);
  }
  if (pthread_mutex_unlock(&mutex) != 0)
  {
    fprintf(stderr, "Failed to unlock mutex!\n");
    return AVERROR(EINVAL);
  }
  return ret;
}

static int64_t seek_store(void *opaque, int64_t offset, int whence)
{
  int ret = 0;
  MemoryStream *ms = opaque;
  if ((ret = pthread_mutex_lock(&mutex)) != 0)
  {
    fprintf(stderr, "Failed to lock mutex!\n");
    return AVERROR(EINVAL);
  }
  ret = memory_stream_seek(ms, offset, whence);
  if (pthread_mutex_unlock(&mutex) != 0)
  {
    fprintf(stderr, "Failed to unlock mutex!\n");
    return AVERROR(EINVAL);
  }
  return ret;
}

static int open_codec_context(AVCodecContext **dec_ctx, AVStream **stream, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
  int ret;
  AVStream *st;
  const AVCodec *dec = NULL;
  const char *type_name = av_get_media_type_string(type);
  if((ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0)) < 0)
  {
    fprintf(stderr, "Could not find %s stream in media!\n", type_name);
    return ret;
  } 
  else
  {
    st = fmt_ctx->streams[ret];

    if (!(dec = avcodec_find_decoder(st->codecpar->codec_id)))
    {
      fprintf(stderr, "Failed to find %s codec!\n", type_name);
      return AVERROR(EINVAL);
    }

    if (!(*dec_ctx = avcodec_alloc_context3(dec)))
    {
      fprintf(stderr, "Failed to allocate the %s codec context\n", type_name);
      return AVERROR(ENOMEM);
    }

    if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0)
    {
      fprintf(stderr, "Failed to copy %s codec parameters to decoder context!\n", type_name);
      return ret;
    }

    if ((ret=avcodec_open2(*dec_ctx, dec, NULL)) < 0)
    {
      fprintf(stderr, "Failed to open %s codec!\n", type_name);
      return ret;
    }
    *stream = st;
  }

  return 0;
}

static int output_video_frame(AVFrame *frame)
{
  av_image_copy(video_frame_data, video_frame_line_size, 
                (const uint8_t **)(frame->data), frame->linesize, 
                pix_fmt, frame->width, frame->height);
  fireVideoFrameParsed(video_frame_data[0], video_frame_size);
  printf("output video\n");
  return 0;
}

static int output_audio_frame(AVFrame *frame)
{
  size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);

  fireAudioFrameParsed(frame->extended_data[0], unpadded_linesize);
  printf("output audio\n");
  return 0;
}

static int decode_packet(AVCodecContext *ctx, AVPacket *pkt)
{
  int ret = 0;
  
  if ((ret = avcodec_send_packet(ctx, pkt)) < 0)
  {
    fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
    return ret;
  }

  while (ret >= 0)
  {
    if ((ret = avcodec_receive_frame(ctx, frame)) < 0)
    {
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) return 0;
      fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
      return ret;
    }

    if (ctx->codec->type == AVMEDIA_TYPE_VIDEO)
    {
      ret = output_video_frame(frame);
    }
    else if (ctx->codec->type == AVMEDIA_TYPE_AUDIO)
    {
      ret = output_audio_frame(frame);
    }
    av_frame_unref(frame);
    if (ret < 0)
      return ret;
  }

  return 0;
}

static void *demux_decode(void *arg)
{
  int ret;
  // avio
  if (!(io_buffer = av_malloc(IO_BUFFER_SIZE)))
  {
    fprintf(stderr, "Could not allocate io buffer!\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(io_ctx = avio_alloc_context(io_buffer, IO_BUFFER_SIZE, 0, store,
    store->is_stream ? &read_stream_store:&read_file_store, 
    NULL, 
    store->is_stream ? NULL : &seek_store)))
  {
    fprintf(stderr, "Could not allocate io context!\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  // format
  if (!(fmt_ctx = avformat_alloc_context()))
  {
    fprintf(stderr, "Could not allocate format context!\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }
  fmt_ctx->pb = io_ctx;

  if ((ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL)) != 0)
  {
    fprintf(stderr, "Could not open input!\n");
    goto end;
  }

  if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) != 0)
  {
    fprintf(stderr, "Could not find stream information!\n");
    goto end;
  }

  if (open_codec_context(&video_dec_ctx, &video_stream, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0)
  {
    int width = video_dec_ctx->width;
    int height = video_dec_ctx->height;
    pix_fmt = video_dec_ctx->pix_fmt;
    if ((ret = av_image_alloc(video_frame_data, video_frame_line_size, width, height, pix_fmt, 1)) < 0)
    {
      fprintf(stderr, "Could not allocate raw video buffer\n");
      goto end;
    }
    video_frame_size = ret;
  }

  if (open_codec_context(&audio_dec_ctx, &audio_stream, fmt_ctx, AVMEDIA_TYPE_AUDIO) < 0)
  {
    goto end;
  }

  if (!video_stream && !audio_stream)
  {
    fprintf(stderr, "Could not find audio or video stream in the media, aborting\n");
    ret = 1;
    goto end;
  }

  if (!(frame = av_frame_alloc()))
  {
    fprintf(stderr, "Could not allocate frame!\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (!(pkt = av_packet_alloc()))
  {
    fprintf(stderr, "Could not allocate pakcet!\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  while(av_read_frame(fmt_ctx, pkt) >=0)
  {
    if (!opened)
    {
      av_packet_unref(pkt);
      goto end;
    }
    if(pkt->stream_index == video_stream->index)
    {
      ret = decode_packet(video_dec_ctx, pkt);
    }
    else if (pkt->stream_index == audio_stream->index)
    {
      ret = decode_packet(audio_dec_ctx, pkt);
    }
    av_packet_unref(pkt);
    if (ret < 0)
      break;
  }

end:
  avcodec_free_context(&video_dec_ctx);
  avcodec_free_context(&audio_dec_ctx);
  avformat_close_input(&fmt_ctx);
  av_packet_free(&pkt);
  av_frame_free(&frame);
  av_free(video_frame_data[0]);
  memory_stream_free(&store);
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
  opened = 0;
  pthread_exit(NULL);
  return NULL;
}

/*************************************************/
/*** api section *********************************/
/*************************************************/
EMSCRIPTEN_KEEPALIVE
void hello_wasm()
{
  printf("hello webassembly from demux decode for web media\n");
}

EMSCRIPTEN_KEEPALIVE
int open_dd(int is_stream, VideoFrameParsedCallback on_video_frame_parsed, AudioFrameParsedCallback on_audio_frame_parsed)
{
  int ret;
  if (opened) return 0;

  opened = 1;
  
  fireVideoFrameParsed = on_video_frame_parsed;
  fireAudioFrameParsed = on_audio_frame_parsed;

  if ((ret = pthread_mutex_init(&mutex, NULL)) != 0)
  {
    fprintf(stderr, "Could not init mutex!\n");
    return ret;
  }
  if ((ret = pthread_cond_init(&cond, NULL)) != 0)
  {
    fprintf(stderr, "Could not init cond!\n");
    return ret;
  }
  // memory stream
  if ((ret = memory_stream_create(&store, STORE_SIZE, is_stream)) != 0)
  {
    fprintf(stderr, "Could not aloocate store!\n");
    return ret;
  }

  // create thread
  if ((ret = pthread_create(&demux_decode_t, NULL, &demux_decode, NULL)) != 0)
  {
    fprintf(stderr, "Could not open demux decode thread\n!");
    return ret;
  }

  return 0;
}

EMSCRIPTEN_KEEPALIVE
int write_dd( size_t length, MemoryStreamWriteCallback did_write)
{
  int ret;
  if ((ret = pthread_mutex_lock(&mutex)) != 0)
  {
    fprintf(stderr, "Could not lock mutex!\n");
    return ret;
  }
  memory_stream_write_callback(store, NULL, length, did_write);
  if ((ret = pthread_mutex_unlock(&mutex)) != 0)
  {
    fprintf(stderr, "Could not unlock mutex!\n");
    return ret;
  }
  if (store->is_stream)
  {
    if ((ret = pthread_cond_signal(&cond)) != 0)
    {
      fprintf(stderr, "Could signal cond!\n");
      return ret;
    }
  }
  return 0;
}

EMSCRIPTEN_KEEPALIVE
void write_is_done()
{
  store->is_done = 1;
  if (!store->is_stream)
  {
    pthread_cond_signal(&cond);
  } 
}

EMSCRIPTEN_KEEPALIVE
int close_dd()
{
  opened = 0;
  return 0;
}
