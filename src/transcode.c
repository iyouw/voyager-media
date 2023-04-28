#include <stdlib.h>

#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "emscripten.h"
// #include <emscripten/html5.h>
#include "memory_stream.h"

#define IO_BUFFER_SIZE 4096
#define STORE_SIZE (MEMORY_PAGE * 10)

// store
static MemoryStream *store = NULL;

// avio context
static uint8_t *io_buffer = NULL;
static AVIOContext *io_ctx = NULL;

// avformat context
static AVFormatContext *fmt_ctx = NULL;
static AVStream *video_stream = NULL;
static AVStream *audio_stream = NULL;

// codec context
static AVCodecContext *video_dec_ctx = NULL;
static AVCodecContext *audio_dec_ctx = NULL;

// data
static AVPacket *pkt = NULL;
static AVFrame *frame = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int video_dst_linesize[4];
static int video_dst_bufsize;

/*************************************************/
/*** test section ******************************/
/*************************************************/
EMSCRIPTEN_KEEPALIVE
void hello_wasm()
{
    printf("hello webassembly from transcode for web media\n");
}

/*************************************************/
/*** store section *******************************/
/*************************************************/ 
// store api
EMSCRIPTEN_KEEPALIVE
int open_store()
{
    return create_memory_stream(&store, STORE_SIZE, 0);
}

EMSCRIPTEN_KEEPALIVE
uint8_t *ensure_store_write_capacity(size_t length)
{
    ensure_memory_stream_write(store, length);
    return get_memory_stream_write_position_ptr(store);
}
EMSCRIPTEN_KEEPALIVE
void did_write_store(size_t length)
{
    memory_stream_did_write(store, length);
}

EMSCRIPTEN_KEEPALIVE
void close_store()
{
   if (store) free_memory_stream(store);
}

/*************************************************/
/*** demux section ******************************/
/*************************************************/
typedef void (*PacketParsedCallback)(int stream_index);

typedef void (*StreamSelectedCallback)();

static int io_read_packet(void *opaque, uint8_t *buffer, int buffer_size)
{
    MemoryStream *ms = (MemoryStream *)opaque;
    buffer_size = FFMIN(buffer_size, get_available_of_memory_stream(ms));
    if (!buffer_size) return AVERROR_EOF;
    read_memory_stream(ms, buffer, buffer_size);
    return buffer_size;
}

static int64_t io_seek(void *opaque, int64_t offset, int whence)
{
    MemoryStream *ms = (MemoryStream *)opaque;
    return seek_memory_stream(ms, offset, whence);
}

static int find_best_stream_by_type(AVStream **stream, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret = 0;
    if ((ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0)) >= 0)
    {
        *stream = fmt_ctx->streams[ret];
    }
    return ret;
}

static int infinite_parse_packet(double time, void *data)
{
    if (av_read_frame(fmt_ctx, pkt) >= 0)
    {

    }
    return 0;
}

// demux api
EMSCRIPTEN_KEEPALIVE
int open_demuxer(StreamSelectedCallback on_stream_selected, PacketParsedCallback on_packet_parsed)
{
    int ret;

    if (!(io_buffer = av_malloc(IO_BUFFER_SIZE)))
    {
        return AVERROR(ENOMEM);
    }

    if (!(io_ctx = avio_alloc_context(io_buffer, IO_BUFFER_SIZE, 0, store, &io_read_packet, NULL, &io_seek)))
    {
        return AVERROR(ENOMEM);
    }

    if (!(fmt_ctx = avformat_alloc_context()))
    {
        return AVERROR(ENOMEM);
    }
    fmt_ctx->pb = io_ctx;

    if((ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL)) < 0)
    {
        fprintf(stderr, "Could not open input\n");
        return ret;
    }

    if((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        fprintf(stderr, "Could not find stream information\n");
        return ret;
    }

    if ((ret = find_best_stream_by_type(&video_stream, fmt_ctx, AVMEDIA_TYPE_VIDEO)) < 0)
    {
        fprintf(stderr, "Could not find stream %s\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
    }

    if ((ret = find_best_stream_by_type(&audio_stream, fmt_ctx, AVMEDIA_TYPE_AUDIO)) < 0)
    {
        fprintf(stderr, "Could not find stream %s\n", av_get_media_type_string(AVMEDIA_TYPE_AUDIO));
    }

    // trigger event for select stream end
    if (!video_stream && !audio_stream)
    {
        fprintf(stderr, "Could not find video and audio streams\n");
        return AVERROR(ENOSTR);
    }

    (*on_stream_selected)();

    // infinite parse packet
    if (!(pkt = av_packet_alloc()))
    {
        return AVERROR(ENOMEM);
    }

    if (!(frame = av_frame_alloc()))
    {
        return AVERROR(ENOMEM);
    }
    
    // emscripten_request_animation_frame_loop(&infinite_parse_packet, NULL);

    while(av_read_frame(fmt_ctx, pkt) >=0)
    {
        (*on_packet_parsed)(pkt->stream_index);
        // release packet
        av_packet_unref(pkt);
    }

    return 0;
}

EMSCRIPTEN_KEEPALIVE
void close_demuxer()
{
    if (fmt_ctx) avformat_close_input(&fmt_ctx);
    if (io_ctx)
    {
        av_freep(&io_ctx->buffer);
        avio_context_free(&io_ctx);
    }
}

/*************************************************/
/*** decode section ******************************/
/*************************************************/
typedef void (*OutputVideoFrameCallback)(uint8_t *data, size_t size);

typedef void (*OutputAudioFrameCallback)(uint8_t *data, size_t size);

static int open_decoder_by_type(AVCodecContext **ctx, AVStream *stream, enum AVMediaType type)
{
    int ret;
    const AVCodec *dec;
    const char *type_str = av_get_media_type_string(type);

    if (!(dec = avcodec_find_decoder(stream->codecpar->codec_id)))
    {
        fprintf(stderr, "Could not find %s codec\n", type_str);
        return AVERROR(EINVAL);
    }

    if (!(*ctx = avcodec_alloc_context3(dec)))
    {
        fprintf(stderr, "Failed to allocate the %s codec context\n", type_str);
        return AVERROR(ENOMEM);
    }

    if ((ret = avcodec_parameters_to_context(*ctx, stream->codecpar)) < 0)
    {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", type_str);
        return ret;
    }

    if ((ret = avcodec_open2(*ctx, dec, NULL)) < 0)
    {
        fprintf(stderr, "Failed to open %s codec\n", type_str);
        return ret;
    }

    return 0;
}

static void output_video_frame(AVFrame *frame, OutputVideoFrameCallback on_output_video_frame)
{
    av_image_copy(video_dst_data, video_dst_linesize, 
        (const uint8_t **)(frame->data), 
        frame->linesize, 
        frame->format, 
        frame->width,
        frame->height);
    (*on_output_video_frame)(video_dst_data[0], video_dst_bufsize);
}

static void output_audio_frame(AVFrame *frame, OutputAudioFrameCallback on_output_audio_frame)
{
    size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
    (*on_output_audio_frame)(frame->extended_data[0], unpadded_linesize);
}

// decode api
EMSCRIPTEN_KEEPALIVE
int open_decoder()
{
    int ret;
    if (video_stream)
    {
        if ((ret = open_decoder_by_type(&video_dec_ctx, video_stream, AVMEDIA_TYPE_VIDEO)) < 0)
        {
            return ret;
        }
        video_dst_bufsize = av_image_alloc(
            video_dst_data,
            video_dst_linesize,
            video_dec_ctx->width,
            video_dec_ctx->height,
            video_dec_ctx->pix_fmt,
            1
        );
    }

    if (audio_stream)
    {
        if ((ret = open_decoder_by_type(&audio_dec_ctx, audio_stream, AVMEDIA_TYPE_AUDIO)) < 0)
        {
            return ret;
        }
    }

    return 0;
}

EMSCRIPTEN_KEEPALIVE
int decode(int stream_index, OutputVideoFrameCallback on_output_video_frame, OutputAudioFrameCallback on_output_audio_frame)
{
    int ret = 0;
    AVCodecContext *ctx = NULL;
    if ( stream_index == video_stream->index)
    {
        ctx = video_dec_ctx;
    } 
    else if (stream_index == audio_stream->index)
    {
        ctx = audio_dec_ctx;
    }

    if (!ctx) return ret;

    if ((ret = avcodec_send_packet(ctx, pkt)) < 0)
    {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    while (ret >=  0)
    {
        if((ret = avcodec_receive_frame(ctx, frame)) < 0)
        {
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) return 0;
            fprintf(stderr, "Error during decoding %s\n", av_err2str(ret));
            return ret;
        }

        if (ctx->codec->type == AVMEDIA_TYPE_VIDEO)
        {
            output_video_frame(frame, on_output_video_frame);
        }
        else if (ctx->codec->type == AVMEDIA_TYPE_AUDIO)
        {
            output_audio_frame(frame, on_output_audio_frame);
        }
        // release frame
        av_frame_unref(frame);
    }

    return 0;
}

EMSCRIPTEN_KEEPALIVE
void close_decoder()
{
    if (video_dec_ctx) avcodec_free_context(&video_dec_ctx);
    if (audio_dec_ctx) avcodec_free_context(&audio_dec_ctx);
    if (pkt) av_packet_free(&pkt);
    if (frame) av_frame_free(&frame);
}