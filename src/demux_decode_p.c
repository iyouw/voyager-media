#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>

#include "memory_stream.h"

#define IO_BUFFER_SIZE 4096

static const char *src_file_name = NULL;
static const char *video_dst_file_name = NULL;
static const char *audio_dst_file_name = NULL;

static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

static uint8_t *io_buf;
static AVIOContext *io_ctx = NULL;

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static AVCodecContext *audio_dec_ctx = NULL;

static AVPacket *pkt = NULL;
static AVFrame *frame = NULL;

static AVStream *video_stream = NULL;
static AVStream *audio_stream = NULL;

static int video_frame_count = 0;
static int audio_frame_count = 0;

static int width;
static int height;
static enum AVPixelFormat pix_fmt;
static uint8_t *video_dst_data[4] = { NULL };
static int video_dst_linesize[4];
static int video_dst_bufsize;

static MemoryStream *memory_stream = NULL;


static int open_decoder(AVStream *stream, enum AVMediaType type)
{
  int ret;
  const AVCodec *dec = NULL;
  AVCodecContext *ctx = NULL;
  const char * type_string = av_get_media_type_string(type);
  dec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!dec) {
    fprintf(stderr, "Failed to find %s codec\n", type_string);
    return AVERROR(EINVAL);
  }
  ctx = avcodec_alloc_context3(dec);
  if (!ctx)
  {
    fprintf(stderr, "Failed to allocate the %s codec context\n", type_string);
    return AVERROR(ENOMEM);
  }
  ret = avcodec_parameters_to_context(ctx, stream->codecpar);
  if (ret < 0)
  {
    fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", type_string);
    return ret;
  }
  if ((ret = avcodec_open2(ctx, dec, NULL)) < 0)
  {
    fprintf(stderr, "Fail to open %s codec\n", type_string);
    return ret;
  }
  if (type == AVMEDIA_TYPE_VIDEO)
  {
    video_dec_ctx = ctx;
  }
  else 
  {
    audio_dec_ctx = ctx;
  }
  return 0;
}

static int output_video_frame(AVFrame *frame)
{
  if (frame->width != width || frame->height != height ||
      frame->format != pix_fmt) {
      /* To handle this change, one could call av_image_alloc again and
        * decode the following frames into another rawvideo file. */
      fprintf(stderr, "Error: Width, height and pixel format have to be "
              "constant in a rawvideo file, but the width, height or "
              "pixel format of the input video changed:\n"
              "old: width = %d, height = %d, format = %s\n"
              "new: width = %d, height = %d, format = %s\n",
              width, height, av_get_pix_fmt_name(pix_fmt),
              frame->width, frame->height,
              av_get_pix_fmt_name(frame->format));
      return -1;
  }

  printf("video_frame n:%d\n",
          video_frame_count++);

  /* copy decoded frame to destination buffer:
    * this is required since rawvideo expects non aligned data */
  av_image_copy(video_dst_data, video_dst_linesize,
                (const uint8_t **)(frame->data), frame->linesize,
                pix_fmt, width, height);

  /* write to rawvideo file */
  fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
  return 0;
}

static int output_audio_frame(AVFrame *frame)
{
  size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
  printf("audio_frame n:%d nb_samples:%d pts:%s\n",
          audio_frame_count++, frame->nb_samples,
          av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

  /* Write the raw audio data samples of the first plane. This works
    * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
    * most audio decoders output planar audio, which uses a separate
    * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
    * In other words, this code will write only the first audio channel
    * in these cases.
    * You should use libswresample or libavfilter to convert the frame
    * to packed data. */
  fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);

  return 0;
}

static int decode_packet(AVPacket *pkt, AVCodecContext *ctx)
{
  int ret = 0;
  ret = avcodec_send_packet(ctx, pkt);
  if (ret < 0)
  {
    fprintf(stderr, "Error submitting packet");
    return ret;
  }
  while(ret >= 0)
  {
    ret = avcodec_receive_frame(ctx, frame);
    if (ret < 0)
    {
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) return 0;
      fprintf(stderr, "error during decoding (%s)\n", av_err2str(ret));
      return ret;
    }
    if (ctx->codec->type == AVMEDIA_TYPE_VIDEO)
    {
      ret = output_video_frame(frame);
    }
    else 
    {
      ret = output_audio_frame(frame);
    }
    av_frame_unref(frame);
    if (ret < 0) return ret;
  }
  return 0;
}

static int load_media_thread()
{
  FILE *file = fopen(src_file_name, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open source file %s\n", src_file_name);
    exit(1);
  }
  const size_t buffer_size = 1024;
  uint8_t buffer[buffer_size];
  size_t bytes_read;
  while((bytes_read = fread(buffer, 1, buffer_size, file)) > 0)
  {
    memory_stream_write(memory_stream, buffer, bytes_read); 
  }
  return 0;
}

static int read_io(void *opaque, uint8_t *buffer, int buffer_size)
{
  MemoryStream *memory_stream = (MemoryStream *)opaque;
  buffer_size = FFMIN(buffer_size, memory_stream_get_available(memory_stream));
  if (!buffer_size) return AVERROR_EOF;
  memory_stream_read(memory_stream, buffer, buffer_size);
  return buffer_size;
}

static int64_t seek_io(void *opaque, int64_t offset, int whence)
{
  MemoryStream *memory_stream = (MemoryStream *)opaque;
  int ret = memory_stream_seek(memory_stream, offset, whence);
  return ret;
}

static int demux_thread()
{
  int ret = 0;

  // allocat io buffer
  io_buf = av_malloc(IO_BUFFER_SIZE);
  if (!io_buf)
  {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  // allocat io context
  io_ctx = avio_alloc_context(io_buf, IO_BUFFER_SIZE, 0, memory_stream, &read_io, NULL, &seek_io);
  if (!io_ctx)
  {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  // allocat format context
  fmt_ctx = avformat_alloc_context();
  if (!fmt_ctx)
  {
    ret = AVERROR(ENOMEM);
    goto end;
  }
  fmt_ctx->pb = io_ctx;

  // open input
  ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "Could not open source file %s\n", src_file_name);
    goto end;
  }

  // find streams
  ret = avformat_find_stream_info(fmt_ctx, NULL);
  if (ret < 0)
  {
    fprintf(stderr, "Could not find streams\n");
    goto end;
  }

  // select video stream
  ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (ret < 0)
  {
    fprintf(stderr, "Could not find stream %s stream in input file\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
    goto end;
  }
  video_stream = fmt_ctx->streams[ret];

  // select audio stream
  ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (ret < 0)
  {
    fprintf(stderr, "Could not find stream %s stream in input file", av_get_media_type_string(AVMEDIA_TYPE_AUDIO));
    goto end;
  }
  audio_stream = fmt_ctx->streams[ret];

  // open video decode context
  if (open_decoder(video_stream, AVMEDIA_TYPE_VIDEO)>=0)
  {
    video_dst_file = fopen(video_dst_file_name, "wb");
    if (!video_dst_file)
    {
      fprintf(stderr, "Could not open destination file %s\n", video_dst_file_name);
      ret = 1;
      goto end;
    }
    // allocate image where the decoded image will be put
    width = video_dec_ctx->width;
    height = video_dec_ctx->height;
    pix_fmt = video_dec_ctx->pix_fmt;
    ret = av_image_alloc(video_dst_data, video_dst_linesize, width, height, pix_fmt, 1);
    if (ret < 0)
    {
      fprintf(stderr, "Could not allocate raw video buffer\n");
      goto end;
    }
    video_dst_bufsize = ret;
  }

  // open audio decode context
  if (open_decoder(audio_stream, AVMEDIA_TYPE_AUDIO) >= 0)
  {
    audio_dst_file = fopen(audio_dst_file_name, "wb");
    if (!audio_dst_file)
    {
      fprintf(stderr, "Could not open destination file %s\n", audio_dst_file_name);
      ret = 1;
      goto end;
    }
  }

  // dump file
  av_dump_format(fmt_ctx, 0, src_file_name, 0);

  // allocate frame
  frame = av_frame_alloc();
  if (!frame) 
  {
    fprintf(stderr, "Could not allocate frame\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  // allocate packet
  pkt = av_packet_alloc();
  if (!pkt)
  {
    fprintf(stderr, "Could not allocate frame\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  // demux
  while (av_read_frame(fmt_ctx, pkt) >= 0)
  {
    if (pkt->stream_index == video_stream->index)
    {
      ret = decode_packet(pkt, video_dec_ctx);
    } else if (pkt->stream_index == audio_stream->index) {
      ret = decode_packet(pkt, audio_dec_ctx);
    }
    av_packet_unref(pkt);
    if (ret < 0) break;
  }
end:
  return ret;
}

// get audio format
static int get_format_from_sample_fmt(const char **fmt, enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr,
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

int main(int argc, char *argv[])
{
  int ret = 0;
  if (argc < 4)
  {
    fprintf(stderr, "usage: %s  input_file video_output_file audio_output_file\n"
            "API example program to show how to read frames from an input file.\n"
            "This program reads frames from a file, decodes them, and writes decoded\n"
            "video frames to a rawvideo file named video_output_file, and decoded\n"
            "audio frames to a rawaudio file named audio_output_file.\n",
            argv[0]);
    exit(1);
  }

  src_file_name = argv[1];
  video_dst_file_name = argv[2];
  audio_dst_file_name = argv[3];

  memory_stream_create(&memory_stream, 1024, 0);
  if (memory_stream == NULL)
  {
    fprintf(stderr, "Could not allocate memory_stream\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  ret = load_media_thread();
  if (ret < 0){
    fprintf(stderr, "Could not load media\n");
    goto end;
  }

  ret = demux_thread();
  if (ret < 0) goto end;

  printf("Demuxing succeeded.\n");

  if (video_stream) {
    printf("Play the output video file with the command:\n"
            "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
            av_get_pix_fmt_name(pix_fmt), width, height,
            video_dst_file_name);
  }

  if (audio_stream) {
    enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
    int n_channels = audio_dec_ctx->ch_layout.nb_channels;
    const char *fmt;

    if (av_sample_fmt_is_planar(sfmt)) {
        const char *packed = av_get_sample_fmt_name(sfmt);
        printf("Warning: the sample format the decoder produced is planar "
                "(%s). This example will output the first channel only.\n",
                packed ? packed : "?");
        sfmt = av_get_packed_sample_fmt(sfmt);
        n_channels = 1;
    }

    if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
        goto end;

    printf("Play the output audio file with the command:\n"
            "ffplay -f %s -ac %d -ar %d %s\n",
            fmt, n_channels, audio_dec_ctx->sample_rate,
            audio_dst_file_name);
  }
end:
  avcodec_free_context(&video_dec_ctx);
  avcodec_free_context(&audio_dec_ctx);

  av_freep(&io_ctx->buffer);
  avio_context_free(&io_ctx);
  avformat_close_input(&fmt_ctx);
 
  if (video_dst_file)
      fclose(video_dst_file);
  if (audio_dst_file)
      fclose(audio_dst_file);
  av_packet_free(&pkt);
  av_frame_free(&frame);
  av_free(video_dst_data[0]);

  return ret;
}
