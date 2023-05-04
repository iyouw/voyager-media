#ifndef MEMORY_STREAM_H
#define MEMORY_STREAM_H

#include <stddef.h>

#define V_MAX(a,b)  ((a) < (b) ? (b) : (a))

#define V_MIN(a,b) ((a) < (b) ? (a) : (b))

#define MEMORY_PAGE (64 * 1024)

typedef unsigned char uint8_t;

typedef void (*MemoryStreamWriteCallback)(void *opaque, uint8_t *ptr, size_t buf_size);

typedef struct MemoryStream
{
  uint8_t *data;
  size_t position;
  size_t length;
  size_t capacity;
  size_t recycle_length;
  int is_stream;
  int is_done;
} MemoryStream;

int memory_stream_create(MemoryStream **memory_stream, size_t capacity, int is_stream);

void memory_stream_free(MemoryStream **memory_stream);

size_t memory_stream_get_free(MemoryStream *memory_stream);

size_t memory_stream_get_available(MemoryStream *memory_stream);

uint8_t *memory_stream_get_read_position(MemoryStream *memory_stream);

uint8_t *memory_stream_get_write_position(MemoryStream *memory_stream);

void memory_stream_is_done(MemoryStream *memory_stream, int is_done);

uint8_t *memory_stream_ensure_write(MemoryStream *memory_stream, size_t write_length);

void memory_stream_collect(MemoryStream *memory_stream);

void memory_stream_resize(MemoryStream *memory_stream, size_t size);

size_t memory_stream_read(MemoryStream *memory_stream, uint8_t *buf, size_t buf_size);

void memory_stream_did_write(MemoryStream *memory_stream, size_t write_length);

size_t memory_stream_write(MemoryStream *memory_stream, const uint8_t *buf, size_t buf_size);

size_t memory_stream_write_callback(MemoryStream *memory_stream, void *opaque, size_t buf_size, MemoryStreamWriteCallback callback);

long memory_stream_seek(MemoryStream *memory_stream, long offset, int whence);
#endif