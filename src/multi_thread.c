#include <stdio.h>
#include <pthread.h>

#include "emscripten.h"

static pthread_t child_t;

static void *child_thread(void *arg)
{
  while (1)
  {
    printf("hello child thread!\n");
  }
}

EMSCRIPTEN_KEEPALIVE
void open()
{
  int ret;
  printf("hello main thread\n");
  if ((ret = pthread_create(&child_t, NULL, &child_thread, NULL)) != 0)
  {
    perror("can not create child thread\n");
  }

  if ((ret = pthread_join(child_t, NULL)) != 0)
  {
    perror("Could not join child thread\n");
  }

  printf("thread is done\n");
}


