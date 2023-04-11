#include <stdio.h>
#include <stdlib.h>
#include "memory_stream.h"

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    fprintf(stderr, "usage %s input file output file\n", argv[0]);
    return -1;
  }

  int ret = 0;
  const char *in = argv[1];
  const char *out = argv[2];

  MemoryStream *ms = create_memory_stream(100);

  FILE *is = fopen(in, "rb");
  FILE *os = fopen(out, "wb");
  if (is == NULL)
  {
    fprintf(stderr, "can not open file: %s", in);
    ret = -1;
    goto fail;
  }

  if (os == NULL)
  {
    fprintf(stderr, "can not open or create file: %s", out);
    ret = -2;
    goto fail;
  }

  fseek(is, 0, SEEK_END);
  const size_t size = ftell(is);

  uint8_t read_buffer[100];

  uint8_t buffer[200];

  const size_t sz = size - 200;
  uint8_t *buffer2 = malloc(sz);

  fread(buffer, sizeof(uint8_t), sizeof(buffer), is);
  write_memory_stream(ms, buffer, 200);

  size_t rds = 0;
  while ((rds = read_memory_stream(ms, read_buffer, 100) > 0))
  {
    fwrite(read_buffer, sizeof(uint8_t), rds, os);
  }

  fread(buffer2, sizeof(uint8_t), sz, is);
  write_memory_stream(ms, buffer2, sz);

  while ((rds = read_memory_stream(ms, read_buffer, 100) > 0))
  {
    fwrite(read_buffer, sizeof(uint8_t), rds, os);
  }
fail:
  if (is) fclose(is);
  if (os) fclose(os);
  if (buffer2) free(buffer2);
  if (ms) free_memory_stream(ms);
  return ret;
}