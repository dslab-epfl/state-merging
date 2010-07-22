//===-- Thread.h ----------------------------------------*- C++ -*-===//
#ifndef KLEE_THREAD_H
#define KLEE_THREAD_H

#include "klee/Expr.h"
#include "klee/Internal/Module/KInstIterator.h"
#include "cloud9/Logger.h"

#include <map>

namespace klee {
  class KFunction;
  class KInstruction;
  class ExecutionState;
  class StackFrame;

class Thread
{
  friend class Executor;
  friend class ExecutionState;
  friend class Mutex;
  friend class CondVar;
public:
  Thread(ref<Expr> _address, KFunction *start_function);

  static uint64_t tids;

  uint64_t getTID() { return  tid;}
  KInstIterator getPC() { return pc;}
  
private:
  bool enabled;
  bool joinState;
  // the thread we are joining
  uint64_t joining;
  ref<Expr> thread_ptr; //address of the thread variable
  uint64_t tid;
  KInstIterator pc, prevPC;

  std::vector<StackFrame> stack;

  std::map< ref<Expr>, ref<Expr> > tls;
  std::string _file; //XXX hack to store the top frame info for bktrace
  unsigned _line;
  void pushFrame(KInstIterator caller, KFunction *kf);
  bool isInJoin(){ return joinState;}
};

}
#endif

