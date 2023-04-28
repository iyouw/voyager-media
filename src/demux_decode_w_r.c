#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "memory_stream.h"

#define IO_BUFFER_SIZE (MEMORY_PAGE * 3)
#define STORE_SIZE (MEMORY_PAGE * 10)

typedef void (*VideoFrameParsedCallback)(uint8_t *ptr, long size);
typedef void (*AudioFrameParsedCallback)(uint8_t *ptr, long size);

typedef enum { TRUE, FALSE } Boolean;

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
static pthread_cond_t read_cond;
static VideoFrameParsedCallback fireVideoFrameParsed = NULL;
static AudioFrameParsedCallback fireAudioFrameParsed = NULL;
static Boolean opened = FALSE;

/*************************************************/
/*** internal section ****************************/
/*************************************************/
static int read_store(void *opaque, uint8_t *buffer, int buffer_size)
{
  int ret = 0;
  if ((ret = pthread_mutex_lock(&mutex)) != 0)
  {
    fprintf(stderr, "Failed to lock muted\n");
    return 0;
  }
  MemoryStream *ms = opaque;
  if (!ms->is_stream)
  {
    if ((ret = pthread_cond_wait(&read_cond, &mutex)) != 0)
    {
      fprintf(stderr, "Failed to wait cond!\n");
    }
    if (pthread_mutex_unlock(&mutex) != 0)
    {
      fprintf(stderr, "Failed to unlock mutex!\n");
      return 0;
    }
    return 0;
  }
  buffer_size = FFMIN(buffer_size, get_available_of_memory_stream(ms));
  if (buffer_size == 0)
  {
    if ((ret = pthread_cond_wait(&read_cond, &mutex)) != 0)
    {
      fprintf(stderr, "Failed to wait cond!\n");
    }
    if (pthread_mutex_unlock(&mutex) != 0)
    {
      fprintf(stderr, "Failed to unlock mutex!\n");
      return 0;
    }
    return 0;
  }
  ret = read_memory_stream(ms, buffer, buffer_size);
  if (pthread_mutex_unlock(&mutex) != 0)
  {
    fprintf(stderr, "Failed to unlock mutex!\n");
    return 0;
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
    return -1;
  }
  ret = seek_memory_stream(ms, offset, whence);
  if (pthread_mutex_unlock(&mutex) != 0)
  {
    fprintf(stderr, "Failed to unlock mutex!\n");
    return -1;
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
  (*fireVideoFrameParsed)(video_frame_data[0], video_frame_size);
  return 0;
}

static int output_audio_frame(AVFrame *frame)
{
  size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
  (*fireAudioFrameParsed)(frame->extended_data[0], unpadded_linesize);
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

  if (!(io_ctx = avio_alloc_context(io_buffer,IO_BUFFER_SIZE,0,store,&read_store,NULL,&seek_store)))
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
    if (opened == FALSE)
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
  free_memory_stream(store);
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&read_cond);
  pthread_exit(NULL);
  return NULL;
}

/*************************************************/
/*** api section *********************************/
/*************************************************/
void hello_wasm()
{
  printf("hello webassembly from transcode for web media\n");
}

int open_dd(VideoFrameParsedCallback on_video_frame_parsed, AudioFrameParsedCallback on_audio_frame_parsed)
{
  int ret;
  if (opened == TRUE) return 0;

  opened = TRUE;

  fireVideoFrameParsed = on_video_frame_parsed;
  fireAudioFrameParsed = on_audio_frame_parsed;

  if ((ret = pthread_mutex_init(&mutex, NULL)) != 0)
  {
    fprintf(stderr, "Could not init mutex!\n");
    return ret;
  }

  if ((ret = pthread_cond_init(&read_cond, NULL)) != 0)
  {
    fprintf(stderr, "Could not init read cond!\n");
    return ret;
  }

  // memory stream
  if ((ret = create_memory_stream(&store, STORE_SIZE, 0)) != 0)
  {
    fprintf(stderr, "Could not aloocate store!\n");
    return AVERROR(ENOMEM);
  }

  // create thread
  if ((ret = pthread_create(&demux_decode_t, NULL, &demux_decode, NULL)) != 0)
  {
    fprintf(stderr, "Could not open demux decode thread\n!");
    return ret;
  }

  return 0;
}

uint8_t *ensure_write_data_size_of_dd(long size)
{
  int ret;
  if ((ret = pthread_mutex_lock(&mutex)) != 0)
  {
    fprintf(stderr, "Could not lock mutex!\n");
    return NULL;
  }
  return ensure_memory_stream_write(store, size);
}

int did_write_data_to_dd(long size)
{
  int ret;
  memory_stream_did_write(store, size);
  if ((ret = pthread_mutex_unlock(&mutex)) != 0)
  {
    fprintf(stderr, "Could not unlock mutex!\n");
    return ret;
  }
  if (store->is_stream)
  {
    if ((ret = pthread_cond_signal(&read_cond)) != 0)
    {
      fprintf(stderr, "Could not signal cond!\n");
      return ret;
    }
  }
  return 0;
}


int end_data()
{
  int ret;
  if ((ret = pthread_cond_signal(&read_cond)) != 0)
  {
    fprintf(stderr, "Could not signal cond!\n");
    return ret;
  }
  return 0;
}

int close_dd()
{
  opened = FALSE;
  return 0;
}


/*************************************************/
/*** client section *********************************/
/*************************************************/

static long video_frame_count = 0;
static long audio_frame_count = 0;

void video_callback(uint8_t *ptr, long size)
{
  printf("video frame: %ld!", ++video_frame_count);
}

void audio_callback(uint8_t *ptr, long size)
{
  printf("audio frame: %ld!", ++audio_frame_count);
}

int main(int argc, const char *argv[])
{
  int ret;
  if (argc != 4) {
      fprintf(stderr, "usage: %s  input_file video_output_file audio_output_file\n"
              "API example program to show how to read frames from an input file.\n"
              "This program reads frames from a file, decodes them, and writes decoded\n"
              "video frames to a rawvideo file named video_output_file, and decoded\n"
              "audio frames to a rawaudio file named audio_output_file.\n",
              argv[0]);
      exit(1);
  }

  const char *file_name = argv[1];
  const int size = 4096;
  uint8_t buffer[size];
  FILE *file = fopen(file_name, "rb");
  int bytes_read;

  if ((ret = open_dd(&video_callback, &audio_callback)) != 0)
  {
    fprintf(stderr, "Failed to open dd\n");
    close_dd();
  }
  
  while(!feof(file))
  {
    bytes_read = fread(buffer, 1, size, file);
    ensure_write_data_size_of_dd(bytes_read);
    uint8_t *pos = get_memory_stream_write_position_ptr(store);
    memcpy(pos, buffer, bytes_read);
    did_write_data_to_dd(bytes_read);
  }
  
  pthread_join(demux_decode_t, NULL);
}