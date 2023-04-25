#include <stdlib.h>

#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "emscripten.h"
#include "memory_stream.h"

#define IO_BUFFER_SIZE 4096

// store
static MemoryStream *store = NULL;

// avio context
static uint8_t *io_buffer = NULL;
static AVIOContext *io_context = NULL;

// avformat context
static AVFormatContext *fmt_ctx = NULL;
static AVStream *video_stream = NULL;
static AVStream *audio_stream = NULL;

//  codec context
static AVCodecContext *video_dec_ctx = NULL;
static AVCodecContext *audio_dec_ctx = NULL;

// data
static AVPacket *pkt = NULL;
static AVFrame *frame = NULL;

// store api
EMSCRIPTEN_KEEPALIVE
static void open_store()
{
    store = create_memory_stream(MEMORY_PAGE * 10);
}

EMSCRIPTEN_KEEPALIVE
static size_t ensure_store_write_capacity(size_t length)
{
    ensure_memory_stream_write(store, length);
    return get_memory_stream_write_position_prt(store);
}

EMSCRIPTEN_KEEPALIVE
static void close_store()
{
   if (store) free_memory_stream(store);
}

// demux api


// decode api