#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "memory_stream.h"

int memory_stream_create(MemoryStream **memory_stream, size_t capacity, int is_stream)
{
  MemoryStream *ms;

  if (!(ms = malloc(sizeof(MemoryStream))))
  {
    return errno;
  }
  if (!(ms->data = malloc(capacity)))
  {
    return errno;
  }
  ms->capacity = capacity;
  ms->is_stream = is_stream;
  ms->recycle_length = 0;
  ms->length = 0;
  ms->position = 0;
  ms->is_done = 0;

  *memory_stream = ms;

  return 0;
}

void memory_stream_free(MemoryStream **memory_stream)
{
  if (*memory_stream == NULL) return;
  free((*memory_stream)->data);
  free(*memory_stream);
}

size_t memory_stream_get_free(MemoryStream *const memory_stream)
{
  return memory_stream->capacity - memory_stream->length;
}

size_t memory_stream_get_available(MemoryStream *const memory_stream)
{
  return memory_stream->length - memory_stream->position;
}

uint8_t *memory_stream_get_read_position(MemoryStream *const memory_stream)
{
  return memory_stream->data + memory_stream->position;
}

uint8_t *memory_stream_get_write_position(MemoryStream *const memory_stream) 
{
  return memory_stream->data + memory_stream->length;
}

void memory_stream_is_done(MemoryStream *memory_stream, int is_done)
{
  memory_stream->is_done = is_done;
}

void memory_stream_collect(MemoryStream *const memory_stream)
{
  size_t size = memory_stream->length - memory_stream->position;
  if (size > 0) {
    uint8_t *src = memory_stream_get_read_position(memory_stream);
    memmove(memory_stream->data, src, size);
  }
  memory_stream->length -= memory_stream->position;
  memory_stream->recycle_length += memory_stream->position;
  memory_stream->position = 0;
}

void memory_stream_resize(MemoryStream *const memory_stream, const size_t size)
{
  size_t pages = (size_t)ceil(((double)size) / MEMORY_PAGE);
  size_t sz = V_MAX(memory_stream->capacity * 2, pages * MEMORY_PAGE + memory_stream->length);
  memory_stream->data = (uint8_t *)realloc(memory_stream->data, sz);
  memory_stream->capacity = sz;
}

uint8_t *memory_stream_ensure_write(MemoryStream *const memory_stream, size_t write_length)
{
  if(memory_stream->is_stream && memory_stream->position > 0)
  {
    memory_stream_collect(memory_stream);
  }
  size_t free = memory_stream_get_free(memory_stream);
  if ((free + memory_stream->position) < write_length)
  {
    memory_stream_resize(memory_stream, write_length);
  }
  return memory_stream->data + memory_stream->length;
}

size_t memory_stream_read(MemoryStream *const memory_stream, uint8_t *buf, size_t buf_size)
{
  int size = V_MIN(memory_stream_get_available(memory_stream), buf_size);
  if (size > 0)
  {
    uint8_t *src = memory_stream_get_read_position(memory_stream);
    memcpy(buf, src, size);
    memory_stream->position += size;
  }
  return size;
}

void memory_stream_did_write(MemoryStream *const memory_stream, size_t write_length)
{
  memory_stream->length = V_MIN(memory_stream->capacity, memory_stream->length + write_length);
}

size_t memory_stream_write(MemoryStream *const memory_stream, const uint8_t *buf, size_t buf_size)
{
  uint8_t *dest = memory_stream_ensure_write(memory_stream, buf_size);
  memcpy(dest, buf, buf_size);
  memory_stream_did_write(memory_stream, buf_size);
  return buf_size;
}

size_t memory_stream_write_callback(MemoryStream *memory_stream, void *opaque, size_t buf_size, MemoryStreamWriteCallback callback)
{
  uint8_t *dest = memory_stream_ensure_write(memory_stream, buf_size);
  (*callback)(opaque, dest, buf_size);
  memory_stream_did_write(memory_stream, buf_size);
  return buf_size;
}

long memory_stream_seek(MemoryStream *const memory_stream, long offset, int whence)
{
  int ret = -1;
  long pos = -1;
  if (SEEK_SET == whence)
  {
    pos = offset;
  }
  else if (SEEK_CUR == whence)
  {
    pos = memory_stream->position + offset;
  }
  else if (SEEK_END == whence)
  {
    pos = memory_stream->length + offset;
  }
  if (pos >=0 && pos <= memory_stream->length)
  {
    memory_stream->position = pos;
    ret = pos;
  }
  return ret;
}