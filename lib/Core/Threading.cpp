/*
 * Threading.cpp
 *
 *  Created on: Jul 22, 2010
 *      Author: stefan
 */

#include "klee/Threading.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Module/Cell.h"

namespace klee {

thread_id_t Thread::tidCounter = 2;

/* StackFrame Methods */

StackFrame::StackFrame(KInstIterator _caller, KFunction *_kf)
  : caller(_caller), kf(_kf), callPathNode(0),
    minDistToUncoveredOnReturn(0), varargs(0) {
  locals = new Cell[kf->numRegisters];
}

StackFrame::StackFrame(const StackFrame &s)
  : caller(s.caller),
    kf(s.kf),
    callPathNode(s.callPathNode),
    allocas(s.allocas),
    minDistToUncoveredOnReturn(s.minDistToUncoveredOnReturn),
    varargs(s.varargs) {
  locals = new Cell[s.kf->numRegisters];
  for (unsigned i=0; i<s.kf->numRegisters; i++)
    locals[i] = s.locals[i];
}

StackFrame& StackFrame::operator=(const StackFrame &s) {
  if (this != &s) {
    caller = s.caller;
    kf = s.kf;
    callPathNode = s.callPathNode;
    allocas = s.allocas;
    minDistToUncoveredOnReturn = s.minDistToUncoveredOnReturn;
    varargs = s.varargs;

    if (locals)
      delete []locals;

    locals = new Cell[s.kf->numRegisters];
    for (unsigned i=0; i<s.kf->numRegisters; i++)
        locals[i] = s.locals[i];
  }

  return *this;
}

StackFrame::~StackFrame() {
  delete[] locals;
}

/* Thread class methods */

Thread::Thread(KFunction * kf) :
  enabled(true), joinState(false), joining(INVALID_THREAD_ID) {

  if (kf) {
    stack.push_back(StackFrame(0, kf));

    pc = kf->instructions;
    prevPC = pc;
  }

  tid = tidCounter++;
}

}
