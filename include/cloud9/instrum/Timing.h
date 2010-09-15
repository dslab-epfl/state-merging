/*
 * Timing.h
 *
 *  Created on: Sep 12, 2010
 *      Author: stefan
 */

#ifndef TIMING_H_
#define TIMING_H_

#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

#include <ostream>
#include <cassert>

namespace cloud9 {

namespace instrum {

class Timer {
private:
  struct timeval startThreadTime;
  struct timeval startRealTime;

  struct timeval endThreadTime;
  struct timeval endRealTime;

  double getTime(const struct timeval &start, const struct timeval &end) const {
    struct timeval res;
    timersub(&end, &start, &res);

    return ((double)res.tv_sec + (double)res.tv_usec/1000000.0);
  }

  void convert(struct timeval &dst, struct timespec &src) {
    dst.tv_sec = src.tv_sec;
    dst.tv_usec = src.tv_nsec / 1000;
  }

public:
  Timer() { }
  ~Timer() { }

#if 0
  void start() {
    int res = gettimeofday(&startRealTime, 0);
    assert(res == 0);

    struct rusage ru;
    res = getrusage(RUSAGE_THREAD, &ru);
    assert(res == 0);

    timeradd(&ru.ru_utime, &ru.ru_stime, &startThreadTime);
  }

  void stop() {
    int res = gettimeofday(&endRealTime, 0);
    assert(res == 0);

    struct rusage ru;
    res = getrusage(RUSAGE_THREAD, &ru);
    assert(res == 0);

    timeradd(&ru.ru_utime, &ru.ru_stime, &endThreadTime);
  }
#else
  void start() {
    struct timespec tp;

    int res = clock_gettime(CLOCK_REALTIME, &tp);
    assert(res == 0);

    convert(startRealTime, tp);

    res = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
    assert(res == 0);

    convert(startThreadTime, tp);
  }

  void stop() {
    struct timespec tp;

    int res = clock_gettime(CLOCK_REALTIME, &tp);
    assert(res == 0);

    convert(endRealTime, tp);

    res = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp);
    assert(res == 0);

    convert(endThreadTime, tp);
  }
#endif

  double getThreadTime() const { return getTime(startThreadTime, endThreadTime); }
  double getRealTime() const { return getTime(startRealTime, endRealTime); }
};

static inline std::ostream &operator<<(std::ostream &os, const Timer &timer) {
  os << "Thread: " << timer.getThreadTime() <<
       " Real: " << timer.getRealTime();

  return os;
}

}

}


#endif /* TIMING_H_ */
