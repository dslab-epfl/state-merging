#include "klee/Thread.h"
#include "klee/Expr.h"
#include "klee/ScheduleTrace.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Module/KInstIterator.h"

using namespace llvm;
using namespace klee;

uint64_t  Thread::tids = 0;

Thread::Thread(ref<Expr> _address, ref<Expr> _value, KFunction * kf):
  enabled(true),
  joinState(false), 
  joining(0xFFFFFFFF)    
{
  thread_ptr = _address;
  value = _value;
  prevPC = 0;
  stack = new std::vector<StackFrame>();
  pushFrame(0, kf);
  pc = kf->instructions;
  tid = tids++;

  traceInfo.lclock = 0;
  traceInfo.op = 0;

}

Thread::Thread(const Thread &t):
  lockSet(t.lockSet),
  enabled(t.enabled),
  joinState(t.joinState), 
  joining(t.joining), 
  thread_ptr(t.thread_ptr), 
  value(t.value), 
  tid(t.tid), 
  pc(t.pc), 
  prevPC(t.prevPC),
  traceInfo(t.traceInfo), 
  tls(t.tls),
  _file(t._file),
  _line(t._line) {

  this->stack = new std::vector<StackFrame>();
  for (std::vector<StackFrame>::iterator it = t.stack->begin();
      it != t.stack->end(); it++) {

    StackFrame s(*it);
    this->stack->push_back(s);
  }
}
/// should only be used to intialize the main thread
Thread::Thread(KInstIterator statePC, std::vector<StackFrame> *stateStack):
  enabled(true),
  joinState(false),
  joining(0xFFFFFFFF)
{
  prevPC = 0;
  stack = stateStack;
  
  pc = statePC;
  thread_ptr = 0;
  tids = 0;
  tid = tids++;
  
  traceInfo.lclock = 0;
  traceInfo.op = 0;
}

// should implement this
// Thread::~Thread()
// {
// }


void Thread::pushFrame(KInstIterator caller, KFunction *kf) 
{
  stack->push_back(StackFrame(caller, kf));
}

