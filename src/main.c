#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "memory_stream.h"

#define READ_FILE_BUFFER_SIZE (7 * 1024)
#define WRITE_FILE_BUFFER_SIZE (3 * 1024)

typedef struct 
{
  const char *fileName;
  struct MemoryStream *const ms;
} Context;

int isDone = 0;
pthread_mutex_t mutex;

static void *read_file_thread(void *args)
{
  int ret = 0;
  Context *ctx = (Context *)args;
  FILE *file = fopen(ctx->fileName, "rb");
  if (file == NULL) {
    ret = 100;
    goto fail;
  }

  uint8_t buffer[READ_FILE_BUFFER_SIZE];
  size_t bytes_read;

  while(!feof(file)) {   
    bytes_read = fread(buffer, sizeof(uint8_t), READ_FILE_BUFFER_SIZE, file);
    ret = pthread_mutex_lock(&mutex);
    if (ret != 0) goto fail;
    memory_stream_write(ctx->ms, buffer, bytes_read);
    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) goto fail;
  }
  memory_stream_is_done(ctx->ms, 1);
  if((ret = fclose(file)) != 0)
  {
    perror("Could not close input file!\n");
  }
fail:
  pthread_exit(&ret);
}


static void *write_file_thread(void *args)
{
  int ret = 0;
  Context *ctx = (Context *)args;
  FILE *file = fopen(ctx->fileName, "wb");
  if (file == NULL) {
    ret = 101;
    goto fail;
  }
  uint8_t buffer[WRITE_FILE_BUFFER_SIZE];
  size_t bytes_read, bytes_write;
  int done;
  while(1) {
    ret = pthread_mutex_lock(&mutex);
    if (ret != 0) goto fail;
    bytes_read = memory_stream_read(ctx->ms, buffer, WRITE_FILE_BUFFER_SIZE);
    done = ctx->ms->is_done;
    ret = pthread_mutex_unlock(&mutex);
    if (ret != 0) goto fail;
    bytes_write = fwrite(buffer, sizeof(uint8_t), bytes_read, file);
    if (bytes_write != bytes_read) {
      ret = 102;
      break;
    }
    if (bytes_read < WRITE_FILE_BUFFER_SIZE && done) goto fail;
  }
  if ((ret = fclose(file)) != 0)
  {
    perror("Could not close output file!\n");
  }
fail:
  pthread_exit(&ret);
}


int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    fprintf(stderr, "usage %s input file output file\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int ret = 0;
  const char *in = argv[1];
  const char *out = argv[2];
  MemoryStream *ms;
  if((ret = memory_stream_create(&ms, 1024, 0)) != 0)
  {
    goto fail;
  }
  ret = pthread_mutex_init(&mutex, NULL);
  if (ret != 0) {
    perror("init mutex failed!");
    goto fail;
  }

  Context read_context = {
    .fileName = in,
    ms = ms
  };
  pthread_t read_thread;
  ret = pthread_create(&read_thread, NULL, read_file_thread, &read_context);
  if (ret != 0){
    perror("create read thread failed");
    goto fail;
  }

  Context write_context = {
    .fileName = out,
    .ms = ms
  };
  pthread_t write_thread;
  ret = pthread_create(&write_thread, NULL, write_file_thread, &write_context);
  if (ret != 0) {
    perror("create write thread failed");
    goto fail;
  }

  ret = pthread_join(read_thread, NULL);
  if (ret != 0) {
    perror("read thread join failed");
    goto fail;
  }

  ret = pthread_join(write_thread, NULL);
  if (ret != 0){
    perror("write thread join failed");
    goto fail;
  }
fail:
  memory_stream_free(&ms);
  pthread_mutex_destroy(&mutex);
  return ret;
}