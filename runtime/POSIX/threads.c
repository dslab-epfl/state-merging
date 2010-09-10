/*
 * threads.c
 *
 *  Created on: Jul 29, 2010
 *      Author: stefan
 */

#include "multiprocess.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sched.h>
#include <sys/time.h>

#include <klee/klee.h>

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

  if (INJECT_FAULT(pthread_create, EAGAIN)) {
    return -1;
  }

  unsigned int newIdx;
  STATIC_LIST_ALLOC(__tsync.threads, newIdx);

  if (newIdx == MAX_THREADS) {
    errno = EAGAIN;
    return -1;
  }

  thread_data_t *tdata = &__tsync.threads[newIdx];
  tdata->terminated = 0;
  tdata->joinable = 1; // TODO: Read this from an attribute
  tdata->wlist = klee_get_wlist();

  klee_thread_create(newIdx, start_routine, arg);

  *thread = newIdx;

  return 0;
}

void pthread_exit(void *value_ptr) {
  unsigned int idx = pthread_self();
  thread_data_t *tdata = &__tsync.threads[idx];

  if (tdata->joinable) {
    tdata->terminated = 1;
    tdata->ret_value = value_ptr;

    klee_thread_notify_all(tdata->wlist);
  } else {
    STATIC_LIST_CLEAR(__tsync.threads, idx);
  }

  klee_thread_terminate(); // Does not return
}


int pthread_join(pthread_t thread, void **value_ptr) {
  if (thread >= MAX_THREADS) {
    errno = ESRCH;
    return -1;
  }

  if (thread == pthread_self()) {
    errno = EDEADLK;
    return -1;
  }

  thread_data_t *tdata = &__tsync.threads[thread];

  if (!tdata->allocated) {
    errno = ESRCH;
    return -1;
  }

  if (!tdata->joinable) {
    errno = EINVAL;
    return -1;
  }

  if (!tdata->terminated)
    klee_thread_sleep(tdata->wlist);

  if (value_ptr) {
    *value_ptr = tdata->ret_value;
  }

  STATIC_LIST_CLEAR(__tsync.threads, thread);

  return 0;
}

int pthread_detach(pthread_t thread) {
  if (thread >= MAX_THREADS) {
    errno = ESRCH;
  }

  thread_data_t *tdata = &__tsync.threads[thread];

  if (!tdata->allocated) {
    errno = ESRCH;
    return -1;
  }

  if (!tdata->joinable) {
    errno = EINVAL;
    return -1;
  }

  if (tdata->terminated) {
    STATIC_LIST_CLEAR(__tsync.threads, thread);
  } else {
    tdata->joinable = 0;
  }

  return 0;
}

int pthread_attr_init(pthread_attr_t *attr) {
  klee_warning("pthread_attr_init does nothing");
  return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
  klee_warning("pthread_attr_destroy does nothing");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Misc. API
////////////////////////////////////////////////////////////////////////////////

int sched_yield(void) {
  klee_thread_preempt(1);
  return 0;
}

int usleep(useconds_t usec) {
  klee_warning("yielding instead of usleep()-ing");

  uint64_t tstart = klee_get_time();
  klee_thread_preempt(1);
  uint64_t tend = klee_get_time();

  if (tend - tstart < (uint64_t)usec)
    klee_set_time(tstart + usec);

  return 0;
}

unsigned int sleep(unsigned int seconds) {
  klee_warning("yielding instead of sleep()-ing");

  uint64_t tstart = klee_get_time();
  klee_thread_preempt(1);
  uint64_t tend = klee_get_time();

  if (tend - tstart < ((uint64_t)seconds)*1000000) {
    klee_set_time(tstart + ((uint64_t)seconds) * 1000000);
  }

  return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  if (tv) {
    uint64_t ktime = klee_get_time();
    tv->tv_sec = ktime / 1000000;
    tv->tv_usec = ktime % 1000000;
  }

  if (tz) {
    tz->tz_dsttime = 0;
    tz->tz_minuteswest = 0;
  }

  return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
  if (tv) {
    uint64_t ktime = tv->tv_sec * 1000000 + tv->tv_usec;
    klee_set_time(ktime);
  }

  if (tz) {
    klee_warning("ignoring timezone set request");
  }

  return 0;
}
