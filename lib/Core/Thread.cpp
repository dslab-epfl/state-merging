#include "klee/Thread.h"
#include "klee/Expr.h"
#include "klee/ScheduleTrace.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Module/KInstIterator.h"

#include "cloud9/Logger.h"

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

  if (!value.isNull())
    CLOUD9_DEBUG("New thread with value: " << value);
  else
    CLOUD9_DEBUG("New thread with value NULL");

  pushFrame(0, kf);

  pc = kf->instructions;
  prevPC = pc;

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
  stack.push_back(StackFrame(caller, kf));
}

