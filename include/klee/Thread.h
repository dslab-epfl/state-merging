//===-- Thread.h ----------------------------------------*- C++ -*-===//
#ifndef KLEE_THREAD_H
#define KLEE_THREAD_H

#include "klee/Expr.h"
#include "klee/StackFrame.h"
#include "klee/Internal/Module/KInstIterator.h"
#include "klee/ScheduleTrace.h"
#include <map>

namespace klee {
  class KFunction;
  class KInstruction;
  class ExecutionState;

typedef std::vector<uint64_t> LockList;

class Thread
{
  friend class Executor;
  friend class ExecutionState;
  friend class Mutex;
  friend class CondVar;
  friend class GoalOrientedSearcher;
  friend class DeadlockSearcher;
public:
  //Thread();
  Thread(ref<Expr> _address, ref<Expr> value, KFunction *start_function);
  Thread(KInstIterator pc, std::vector<StackFrame> *stack);
  Thread(const Thread &t);
  static uint64_t tids;
  // ~Thread(); //XXX should really implement this at some point
  LockList lockSet;
  uint64_t getTID() { return  tid;}
  KInstIterator getPC() { return pc;}
  
private:
  bool enabled;
  bool joinState;
  // the thread we are joining
  uint64_t joining;
  ref<Expr> thread_ptr; //address of the thread variable
  ref<Expr> value;  //value of the pthread_t thread identifier located at thread_ptr
  uint64_t tid;
  KInstIterator pc, prevPC;
  SchedThreadTraceInfo traceInfo;
  std::vector<StackFrame> *stack;
  std::map< ref<Expr>, ref<Expr> > tls;
  std::string _file; //XXX hack to store the top frame info for bktrace
  unsigned _line;
  void pushFrame(KInstIterator caller, KFunction *kf);
  bool isInJoin(){ return joinState;}
};

}
#endif

