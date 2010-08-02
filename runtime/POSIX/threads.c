/*
 * pthread.c
 *
 *  Created on: Jul 29, 2010
 *      Author: stefan
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <klee/klee.h>

#include "multiprocess.h"

static inline unsigned int _new_tdata() {
  int tid;
  for (tid = 0; tid < MAX_THREADS; tid++) {
    if (!__tdata[tid].allocated)
      return tid;
  }

  return MAX_THREADS;
}

static inline void _clear_tdata(unsigned int tid) {
  memset(&__tdata[tid], 0, sizeof(thread_data_t));
}

////////////////////////////////////////////////////////////////////////////////
// The PThreads API
////////////////////////////////////////////////////////////////////////////////

pthread_t pthread_self(void) {
  pthread_t result;

  klee_get_context(&result, 0);

  return result;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_routine)(void*), void *arg) {
  errno = EPERM;
  return -1;
}

void pthread_exit(void *value_ptr) {
  klee_thread_terminate(); // Does not return
}


int pthread_join(pthread_t thread, void **value_ptr) {
  errno = EINVAL;
  return -1;
}

int pthread_detach(pthread_t thread) {
  errno = EINVAL;
  return -1;
}
