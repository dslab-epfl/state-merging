/*
 * threadsync.c
 *
 *  Created on: Aug 3, 2010
 *      Author: stefan
 */

#include "multiprocess.h"

#include <pthread.h>
#include <errno.h>
#include <klee/klee.h>
#include <string.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// POSIX Mutexes
////////////////////////////////////////////////////////////////////////////////

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  unsigned int idx;
  LIST_ALLOC(__tsync.mutexes, idx);

  if (idx == MAX_MUTEXES) {
    errno = ENOMEM;
    return -1;
  }

  *((unsigned int*)mutex) = INDEX_TO_MUTEX(idx);

  mutex_data_t *mdata = &__tsync.mutexes[idx];
  mdata->wlist = klee_get_wlist();
  mdata->taken = 0;

  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
  unsigned int idx = MUTEX_TO_INDEX(*((unsigned int*)mutex));

  if (!LIST_CHECK(__tsync.mutexes, idx)) {
    errno = EINVAL;
    return -1;
  }

  LIST_CLEAR(__tsync.mutexes, idx);

  return 0;
}

static int _atomic_mutex_lock(pthread_mutex_t *mutex, char try) {
  // Check for statically initialized mutexes
  if (*((unsigned int*)mutex) == DEFAULT_MUTEX) {
    int res = pthread_mutex_init(mutex, 0);
    if (res != 0)
      return res;
  }

  unsigned int idx = MUTEX_TO_INDEX(*((unsigned int*)mutex));

  if (!LIST_CHECK(__tsync.mutexes, idx)) {
    errno = EINVAL;
    return -1;
  }

  mutex_data_t *mdata = &__tsync.mutexes[idx];

  if (mdata->queued > 0 || mdata->taken) {
    if (try) {
      errno = EBUSY;
      return -1;
    } else {
      mdata->queued++;
      klee_thread_sleep(mdata->wlist);
      mdata->queued--;
    }
  }
  mdata->taken = 1;
  mdata->owner = pthread_self();

  return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
  int res = _atomic_mutex_lock(mutex, 0);

  if (res == 0)
    klee_thread_preempt();

  return res;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  int res = _atomic_mutex_lock(mutex, 1);

  if (res == 0)
    klee_thread_preempt();

  return res;
}

static int _atomic_mutex_unlock(pthread_mutex_t *mutex) {
  unsigned int idx = MUTEX_TO_INDEX(*((unsigned int*)mutex));

  if (!LIST_CHECK(__tsync.mutexes, idx)) {
    errno = EINVAL;
    return -1;
  }

  mutex_data_t *mdata = &__tsync.mutexes[idx];

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
  int res = _atomic_mutex_unlock(mutex);

  klee_thread_preempt();

  return res;
}

////////////////////////////////////////////////////////////////////////////////
// POSIX Condition Variables
////////////////////////////////////////////////////////////////////////////////

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  unsigned int idx;
  LIST_ALLOC(__tsync.condvars, idx);

  if (idx == MAX_CONDVARS) {
    errno = ENOMEM;
    return -1;
  }

  *((unsigned int*)cond) = INDEX_TO_COND(idx);

  __tsync.condvars[idx].wlist = klee_get_wlist();

  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
  unsigned int idx = COND_TO_INDEX(*((unsigned int*)cond));

  if (!LIST_CHECK(__tsync.condvars, idx)) {
    errno = EINVAL;
    return -1;
  }

  LIST_CLEAR(__tsync.condvars, idx);

  return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
    const struct timespec *abstime) {
  assert(0 && "not implemented");
  return -1;
}

static int _atomic_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
  if (*((unsigned int*)cond) == DEFAULT_CONDVAR) {
    int res = pthread_cond_init(cond, 0);

    if (res != 0)
      return res;
  }

  unsigned int idx = COND_TO_INDEX(*((unsigned int*)cond));

  if (!LIST_CHECK(__tsync.condvars, idx)) {
    errno = EINVAL;
    return -1;
  }

  if (!LIST_CHECK(__tsync.mutexes, MUTEX_TO_INDEX(*((unsigned int*)mutex)))) {
    errno = EINVAL;
    return -1;
  }

  condvar_data_t *cdata = &__tsync.condvars[idx];

  if (cdata->queued > 0) {
    if (cdata->mutex != *((unsigned int*)mutex)) {
      errno = EINVAL;
      return -1;
    }
  } else {
    cdata->mutex = *((unsigned int*)mutex);
  }

  if (_atomic_mutex_unlock(mutex) != 0) {
    errno = EPERM;
    return -1;
  }

  cdata->queued++;
  klee_thread_sleep(cdata->wlist);
  cdata->queued--;

  if (_atomic_mutex_lock(mutex, 0) != 0) {
    errno = EPERM;
    return -1;
  }

  return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
  int res = _atomic_cond_wait(cond, mutex);

  if (res == 0)
    klee_thread_preempt();

  return res;
}

static int _atomic_cond_notify(pthread_cond_t *cond, char all) {
  unsigned int idx = COND_TO_INDEX(*((unsigned int*)cond));

  if (!LIST_CHECK(__tsync.condvars, idx)) {
    errno = EINVAL;
    return -1;
  }

  condvar_data_t *cdata = &__tsync.condvars[idx];

  if (cdata->queued > 0) {
    if (all)
      klee_thread_notify_all(cdata->wlist);
    else
      klee_thread_notify_one(cdata->wlist);
  }

  return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
  int res = _atomic_cond_notify(cond, 1);

  if (res == 0)
    klee_thread_preempt();

  return res;
}

int pthread_cond_signal(pthread_cond_t *cond) {
  int res = _atomic_cond_notify(cond, 0);

  if (res == 0)
    klee_thread_preempt();

  return res;
}
