/*
 * misc.c
 *
 *  Created on: Sep 11, 2010
 *      Author: stefan
 */

#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Sleeping Operations
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// Misc. API
////////////////////////////////////////////////////////////////////////////////

int sched_yield(void) {
  klee_thread_preempt(1);
  return 0;
}

int getrusage(int who, struct rusage *usage) {
  if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN && who != RUSAGE_THREAD) {
    errno = EINVAL;
    return -1;
  }

  memset(usage, 0, sizeof(*usage)); // XXX Refine this as further needed

  return 0;
}
