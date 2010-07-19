//===-- Mutex.h ----------------------------------------*- C++ -*-===//
#ifndef KLEE_MUTEX_H
#define KLEE_MUTEX_H

#include "ScheduleTrace.h"
#include "Expr.h"
#include <vector>

namespace klee {

class Mutex
{
  friend class Executor;
  friend class ExecutionState;
  friend class Thread;
public:
  Mutex(ref<Expr> address);
private:
  ref<Expr> address;
  bool taken;
  int type;
  // the thread that holds this lock
  uint64_t thread;
  std::vector<uint64_t> waiting;
  SchedSyncTraceInfo traceInfo;
};

}
#endif
