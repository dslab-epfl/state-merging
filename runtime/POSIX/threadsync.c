/*
 * Cloud9 Parallel Symbolic Execution Engine
 *
 * Copyright (c) 2011, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * All contributors are listed in CLOUD9-AUTHORS file.
 *
 */

#include "multiprocess.h"
#include "signals.h"

#include <pthread.h>
#include <errno.h>
#include <klee/klee.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// POSIX Mutexes
////////////////////////////////////////////////////////////////////////////////

static void _mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  mutex_data_t *mdata = (mutex_data_t*)malloc(sizeof(mutex_data_t));
  memset(mdata, 0, sizeof(mutex_data_t));

  *((mutex_data_t**)mutex) = mdata;

  mdata->wlist = klee_get_wlist();
  mdata->taken = 0;
}

static mutex_data_t *_get_mutex_data(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = *((mutex_data_t**)mutex);

  if (mdata == STATIC_MUTEX_VALUE) {
    _mutex_init(mutex, 0);

    mdata = *((mutex_data_t**)mutex);
  }

  return mdata;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  if (INJECT_FAULT(pthread_mutex_init, ENOMEM, EPERM)) {
    return -1;
  }

  _mutex_init(mutex, attr);

  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  free(mdata);

  return 0;
}

static int _atomic_mutex_lock(mutex_data_t *mdata, char try) {
  if (mdata->queued > 0 || mdata->taken) {
    if (try) {
      errno = EBUSY;
      return -1;
    } else {
      mdata->queued++;
      __klee_thread_sleep(mdata->wlist);
      mdata->queued--;
    }
  }
  mdata->taken = 1;
  mdata->owner = pthread_self();

  return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_mutex_lock(mdata, 0);

  if (res == 0)
    __klee_thread_preempt(0);

  return res;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_mutex_lock(mdata, 1);

  if (res == 0)
    __klee_thread_preempt(0);

  return res;
}

static int _atomic_mutex_unlock(mutex_data_t *mdata) {
  if (!mdata->taken || mdata->owner != pthread_self()) {
    errno = EPERM;
    return -1;
  }

  mdata->taken = 0;

  if (mdata->queued > 0)
    klee_thread_notify_one(mdata->wlist);

  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_mutex_unlock(mdata);

  __klee_thread_preempt(0);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
// POSIX Condition Variables
////////////////////////////////////////////////////////////////////////////////

static void _cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  condvar_data_t *cdata = (condvar_data_t*)malloc(sizeof(condvar_data_t));
  memset(cdata, 0, sizeof(condvar_data_t));

  *((condvar_data_t**)cond) = cdata;

  cdata->wlist = klee_get_wlist();
}

static condvar_data_t *_get_condvar_data(pthread_cond_t *cond) {
  condvar_data_t *cdata = *((condvar_data_t**)cond);

  if (cdata == STATIC_CVAR_VALUE) {
    _cond_init(cond, 0);

    cdata = *((condvar_data_t**)cond);
  }

  return cdata;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  if (INJECT_FAULT(pthread_cond_init, ENOMEM, EAGAIN)) {
    return -1;
  }

  _cond_init(cond, attr);

  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
  condvar_data_t *cdata = _get_condvar_data(cond);

  free(cdata);

  return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
    const struct timespec *abstime) {
  assert(0 && "not implemented");
  return -1;
}

static int _atomic_cond_wait(condvar_data_t *cdata, mutex_data_t *mdata) {
  if (cdata->queued > 0) {
    if (cdata->mutex != mdata) {
      errno = EINVAL;
      return -1;
    }
  } else {
    cdata->mutex = mdata;
  }

  if (_atomic_mutex_unlock(mdata) != 0) {
    errno = EPERM;
    return -1;
  }

  cdata->queued++;
  __klee_thread_sleep(cdata->wlist);
  cdata->queued--;

  if (_atomic_mutex_lock(mdata, 0) != 0) {
    errno = EPERM;
    return -1;
  }

  return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
  condvar_data_t *cdata = _get_condvar_data(cond);
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_cond_wait(cdata, mdata);

  if (res == 0)
    __klee_thread_preempt(0);

  return res;
}

static int _atomic_cond_notify(condvar_data_t *cdata, char all) {
  if (cdata->queued > 0) {
    if (all)
      klee_thread_notify_all(cdata->wlist);
    else
      klee_thread_notify_one(cdata->wlist);
  }

  return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
  condvar_data_t *cdata = _get_condvar_data(cond);

  int res = _atomic_cond_notify(cdata, 1);

  if (res == 0)
    __klee_thread_preempt(0);

  return res;
}

int pthread_cond_signal(pthread_cond_t *cond) {
  condvar_data_t *cdata = _get_condvar_data(cond);

  int res = _atomic_cond_notify(cdata, 0);

  if (res == 0)
    __klee_thread_preempt(0);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
// POSIX Barriers
////////////////////////////////////////////////////////////////////////////////

static void _barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
  barrier_data_t *bdata = (barrier_data_t*)malloc(sizeof(barrier_data_t));
  memset(bdata, 0, sizeof(barrier_data_t));

  *((barrier_data_t**)barrier) = bdata;

  bdata->wlist = klee_get_wlist();
  bdata->curr_event = 0;
  bdata->init_count = count;
  bdata->left = count;
}

static barrier_data_t *_get_barrier_data(pthread_barrier_t *barrier) {
  barrier_data_t *bdata = *((barrier_data_t**)barrier);

  if (bdata == STATIC_BARRIER_VALUE) {
    _barrier_init(barrier, 0, 0);

    bdata = *((barrier_data_t**)barrier);
  }

  return bdata;
}

int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
  if (INJECT_FAULT(pthread_barrier_init, ENOMEM, EPERM)) {
    return -1;
  }

  _barrier_init(barrier, attr, count);

  return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
  barrier_data_t *bdata = _get_barrier_data(barrier);

  free(bdata);

  return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
  barrier_data_t *bdata = _get_barrier_data(barrier);
  int result = 0;

  bdata->left--;

  if (bdata->left == 0) {
    bdata->curr_event++;
    bdata->left = bdata->init_count;

    klee_thread_notify_all(bdata->wlist);

    result = PTHREAD_BARRIER_SERIAL_THREAD;
  }
  else {
    klee_thread_sleep(bdata->wlist);
  }

  return result;
}
