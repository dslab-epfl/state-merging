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

#include <ostream>
#include <cassert>

namespace cloud9 {

namespace instrum {

class Timer {
private:
  struct timeval startUserTime;
  struct timeval startSystemTime;
  struct timeval startRealTime;

  struct timeval endUserTime;
  struct timeval endSystemTime;
  struct timeval endRealTime;

  double getTime(const struct timeval &start, const struct timeval &end) const {
    struct timeval res;
    timersub(&end, &start, &res);

    return ((double)res.tv_sec + (double)res.tv_usec/1000000.0);
  }

public:
  Timer() { }
  ~Timer() { }

  void start() {
    int res = gettimeofday(&startRealTime, 0);
    assert(res == 0);

    struct rusage ru;
    res = getrusage(RUSAGE_THREAD, &ru);
    assert(res == 0);

    startUserTime = ru.ru_utime;
    startSystemTime = ru.ru_stime;
  }

  void stop() {
    int res = gettimeofday(&endRealTime, 0);
    assert(res == 0);

    struct rusage ru;
    res = getrusage(RUSAGE_THREAD, &ru);
    assert(res == 0);

    endUserTime = ru.ru_utime;
    endSystemTime = ru.ru_stime;
  }

  double getUserTime() const { return getTime(startUserTime, endUserTime); }
  double getSystemTime() const { return getTime(startSystemTime, endSystemTime); }
  double getRealTime() const { return getTime(startRealTime, endRealTime); }
};

static inline std::ostream &operator<<(std::ostream &os, const Timer &timer) {
  os << "User: " << timer.getUserTime() <<
       " System: " << timer.getSystemTime() <<
       " Real: " << timer.getRealTime();

  return os;
}

}

}


#endif /* TIMING_H_ */
