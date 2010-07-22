/*
 * Threading.cpp
 *
 *  Created on: Jul 22, 2010
 *      Author: stefan
 */

#include "klee/Threading.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KModule.h"

namespace klee {

uint64_t Thread::tids = 0;

Thread::Thread(ref<Expr> _address, KFunction * kf) :
  enabled(true), joinState(false), joining(0xFFFFFFFF) {
  thread_ptr = _address;

  pushFrame(0, kf);

  pc = kf->instructions;
  prevPC = pc;

  tid = tids++;
}

void Thread::pushFrame(KInstIterator caller, KFunction *kf) {
  stack.push_back(StackFrame(caller, kf));
}

}
