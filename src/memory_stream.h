#ifndef MEMORY_STREAM_H
#define MEMORY_STREAM_H

#include <stddef.h>

#define V_MAX(a,b)  ((a) < (b) ? (b) : (a))

#define V_MIN(a,b) ((a) < (b) ? (a) : (b))

#define MEMORY_PAGE (64 * 1024)

typedef unsigned char uint8_t;

typedef struct MemoryStream
{
  uint8_t *data;
  size_t position;
  size_t length;
  size_t capacity;
  size_t recycle_length;
} MemoryStream;

MemoryStream *create_memory_stream(size_t capacity);

void free_memory_stream(MemoryStream *const memory_stream);

size_t get_free_of_memory_stream(MemoryStream *const memory_stream);

size_t get_available_of_memory_stream(MemoryStream *const memory_stream);

uint8_t *get_memory_stream_read_position_ptr(MemoryStream *const memory_stream);

uint8_t *get_memory_stream_write_position_prt(MemoryStream *const memory_stream);

uint8_t *ensure_memory_stream_write(MemoryStream *const memory_stream, size_t write_length);

void collect_memory_stream(MemoryStream *const memory_stream);

void resize_memory_stream(MemoryStream *const memory_stream, size_t size);

size_t read_memory_stream(MemoryStream *const memory_stream, uint8_t *buf, size_t buf_size);

size_t write_memory_stream(MemoryStream *const memory_stream, const uint8_t *buf, size_t buf_size);

int64_t seek_memory_stream(MemoryStream *const memory_stream, int64_t offset, int whence);

void memory_stream_did_write(MemoryStream *memory_stream, size_t write_length);

#endif