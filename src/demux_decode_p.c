#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <libavutil/avutil.h>
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

static int video_stream_idx = -1;
static int audio_stream_idx = -1;

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
    write_memory_stream(memory_stream, buffer, buffer_size); 
  }
  return 0;
}

static int read_io(void *opaque, uint8_t *buffer, int buffer_size)
{
  MemoryStream *memory_stream = (MemoryStream *)opaque;
  buffer_size = FFMIN(buffer_size, get_available_of_memory_stream(memory_stream));
  if (!buffer_size) return AVERROR_EOF;
  printf("ptr: %p size: %zu\n", memory_stream->data, memory_stream->length);
  read_memory_stream(memory_stream, buffer, buffer_size);
  return buffer_size;
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
  io_ctx = avio_alloc_context(io_buf, IO_BUFFER_SIZE, 0, memory_stream, &read_io, NULL, NULL);
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

  // open audio decode context

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
      ret = decode_packet(pkt, AVMEDIA_TYPE_AUDIO);
    }
    av_packet_unref(pkt);
    if (ret < 0) break;
  }
end:
  return ret;
}

static int open_decoder(AVStream *stream, enum AVMediaType type)
{

}

static int decode_packet(AVPacket *pkt, AVCodecContext *ctx)
{
  int ret = 0;
  ret = avcodec_send_packet(dec,pkt);
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
      if (ret == AVERROR_EFO || ret == AVERROR(EAGAIN)) return 0;
      fprint(stderr, "error during decoding (%s)\n", av_err2str(ret));
      return ret;
    }
    if (ctx->codec->type == AVMEDIA_TYPE_VIDEO)
    { 
    }
    else 
    {

    }
    av_frame_unref(frame);
    if (ret < 0) return ret;
  }
  return 0;
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

  memory_stream = create_memory_stream(1024);
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
end:
  avcodec_free_context(&video_dec_ctx);
  avcodec_free_context(&audio_dec_ctx);
  avformat_close_input(&fmt_ctx);
  av_freep(io_ctx->buffer);
  avio_context_free(&io_ctx);
  if (video_dst_file)
      fclose(video_dst_file);
  if (audio_dst_file)
      fclose(audio_dst_file);
  av_packet_free(&pkt);
  av_frame_free(&frame);
  av_free(video_dst_data[0]);

  return ret;
}
