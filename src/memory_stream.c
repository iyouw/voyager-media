#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "memory_stream.h"

int create_memory_stream(MemoryStream **memory_stream, size_t capacity, int is_stream)
{
  MemoryStream *ms;
  if (!(ms = malloc(sizeof(MemoryStream))))
  {
    return errno;
  }
  memset(ms, 0, sizeof(MemoryStream));
  
  if (!(ms->data = malloc(capacity)))
  {
    return errno;
  }
  ms->capacity = capacity;
  ms->is_stream = is_stream;
  *memory_stream = ms;
  return 0;
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

uint8_t *get_memory_stream_read_position_ptr(MemoryStream *const memory_stream)
{
  return memory_stream->data + memory_stream->position;
}

uint8_t *get_memory_stream_write_position_ptr(MemoryStream *const memory_stream) 
{
  return memory_stream->data + memory_stream->length;
}

uint8_t *ensure_memory_stream_write(MemoryStream *const memory_stream, size_t write_length)
{
  if(memory_stream->position > 0 && memory_stream->is_stream)
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
    uint8_t *src = get_memory_stream_read_position_ptr(memory_stream);
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
    uint8_t *src = get_memory_stream_read_position_ptr(memory_stream);
    memcpy(buf, src, size);
    memory_stream->position += size;
  }
  return size;
}

size_t write_memory_stream(MemoryStream *const memory_stream, const uint8_t *buf, size_t buf_size)
{
  ensure_memory_stream_write(memory_stream, buf_size);
  uint8_t *dest = get_memory_stream_write_position_ptr(memory_stream);
  memcpy(dest, buf, buf_size);
  memory_stream_did_write(memory_stream, buf_size);
  return buf_size;
}

long seek_memory_stream(MemoryStream *const memory_stream, long offset, int whence)
{
  int ret = -1;
  long pos = -1;
  if (SEEK_SET == whence)
  {
    pos = offset - memory_stream->recycle_length;
  }
  else if (SEEK_CUR == whence)
  {
    pos = memory_stream->position + offset;
  }
  else if (SEEK_END == whence)
  {
    pos = memory_stream->length - offset;
  }
  if (pos >=0 && pos <= memory_stream->length)
  {
    memory_stream->position = pos;
    ret = offset;
  }
  return ret;
}

void memory_stream_did_write(MemoryStream *const memory_stream, size_t write_length)
{
  memory_stream->length = V_MIN(memory_stream->capacity, memory_stream->length + write_length);
}