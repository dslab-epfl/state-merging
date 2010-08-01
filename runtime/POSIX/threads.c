/*
 * pthread.c
 *
 *  Created on: Jul 29, 2010
 *      Author: stefan
 */

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <klee/klee.h>

#include "multiprocess.h"

thread_data_t __tdata[MAX_THREADS];

static inline thread_id_t _new_tdata() {
  int tid;
  for (tid = 0; tid < MAX_THREADS; tid++) {
    if (!__tdata[tid].allocated)
      return tid;
  }

  return MAX_THREADS;
}

static inline void _clear_tdata(thread_id_t tid) {
  memset(__tdata[tid], 0, sizeof(thread_data_t));
}

void klee_init_threads(void) {
  memset(&__tdata, 0, sizeof(__tdata));

  thread_data_t *def_data = &__tdata[DEFAULT_THREAD];
  def_data->allocated = 1;
  def_data->terminated = 0;
  def_data->ret_value = 0;
  def_data->wlist_id = klee_get_wlist();
}

////////////////////////////////////////////////////////////////////////////////
// The PThreads API
////////////////////////////////////////////////////////////////////////////////

pthread_t pthread_self(void) {
  pthread_t result;

  klee_get_context(&result, 0, 0);

  return result;
}

int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr,
    void *(*start_routine)(void*), void *restrict arg) {
  return EPERM;
}

void pthread_exit(void *value_ptr) {
  klee_thread_terminate(); // Does not return
}


int pthread_join(pthread_t thread, void **value_ptr) {
  return EINVAL;
}

int pthread_detach(pthread_t thread) {

}
