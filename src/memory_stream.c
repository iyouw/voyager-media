#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "memory_stream.h"

MemoryStream *create_memory_stream(size_t capacity)
{
  MemoryStream *res = (MemoryStream *)malloc(sizeof(MemoryStream));
  memset(res, 0, sizeof(MemoryStream));
  res->capacity = capacity;
  res->data = (uint8_t *)malloc(capacity);
  return res;
}

void free_memory_stream(MemoryStream *const memory_stream)
{
  free(memory_stream->data);
  free(memory_stream);
}

size_t get_free_of_memory_stream(MemoryStream *const memory_stream)
{
  return memory_stream->capacity - memory_stream->length;
}

size_t get_available_of_memory_stream(MemoryStream *const memory_stream)
{
  return memory_stream->length - memory_stream->position;
}

uint8_t *get_memory_stream_position_ptr(MemoryStream *const memory_stream)
{
  return memory_stream->data + memory_stream->position;
}

uint8_t *ensure_memory_stream_write(MemoryStream *const memory_stream, size_t write_length)
{
  if(memory_stream->position > 0)
  {
    collect_memory_stream(memory_stream);
  }
  size_t free = get_free_of_memory_stream(memory_stream);
  if ((free + memory_stream->position) < write_length)
  {
    resize_memory_stream(memory_stream, write_length);
  }
  return memory_stream->data + memory_stream->position;
}

void collect_memory_stream(MemoryStream *const memory_stream)
{
  size_t size = memory_stream->length - memory_stream->position;
  if (size > 0) {
    uint8_t *src = get_memory_stream_position_ptr(memory_stream);
    memmove(memory_stream->data, src, size);
  }
  memory_stream->length -= memory_stream->position;
  memory_stream->recycle_length += memory_stream->position;
  memory_stream->position = 0;
}

void resize_memory_stream(MemoryStream *const memory_stream, const size_t size)
{
  size_t pages = (size_t)ceil(((double)size) / MEMORY_PAGE);
  size_t sz = V_MAX(memory_stream->capacity * 2, pages * MEMORY_PAGE + memory_stream->length);
  memory_stream->data = (uint8_t *)realloc(memory_stream->data, sz);
  memory_stream->capacity = sz;
}

size_t read_memory_stream(MemoryStream *const memory_stream, uint8_t *buf, size_t buf_size)
{
  int size = V_MIN(get_available_of_memory_stream(memory_stream), buf_size);
  if (size > 0)
  {
    uint8_t *src = get_memory_stream_position_ptr(memory_stream);
    memcpy(buf, src, size);
    memory_stream->position += size;
  }
  return size;
}

size_t write_memory_stream(MemoryStream *const memory_stream, const uint8_t *buf, size_t buf_size)
{
  ensure_memory_stream_write(memory_stream, buf_size);
  uint8_t *dest = get_memory_stream_position_ptr(memory_stream);
  memcpy(dest, buf, buf_size);
  memory_stream_did_write(memory_stream, buf_size);
  return buf_size;
}

void memory_stream_did_write(MemoryStream *const memory_stream, size_t write_length)
{
  memory_stream->length = V_MIN(memory_stream->capacity, memory_stream->length + write_length);
} 


