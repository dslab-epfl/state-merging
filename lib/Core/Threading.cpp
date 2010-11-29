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
#include "klee/Expr.h"

namespace klee {

/* StackFrame Methods */

StackFrame::StackFrame(KInstIterator _caller, uint32_t _callerExecIndex, KFunction *_kf)
  : caller(_caller), kf(_kf), callPathNode(0),
    minDistToUncoveredOnReturn(0), varargs(0),
    execIndexStack(1) {

  execIndexStack[0].loopID = uint32_t(-1);
  execIndexStack[0].index = hashUpdate(_callerExecIndex, (uintptr_t) _kf);

  locals = new Cell[kf->numRegisters];
}

StackFrame::StackFrame(const StackFrame &s)
  : caller(s.caller),
    kf(s.kf),
    callPathNode(s.callPathNode),
    allocas(s.allocas),
    minDistToUncoveredOnReturn(s.minDistToUncoveredOnReturn),
    varargs(s.varargs),
    execIndexStack(s.execIndexStack) {

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
    execIndexStack = s.execIndexStack;

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

Thread::Thread(thread_id_t tid, process_id_t pid, KFunction * kf) :
  enabled(true), waitingList(0), execIndex(hashInit()), mergeIndex(0) {

  tuid = std::make_pair(tid, pid);

  if (kf) {
    stack.push_back(StackFrame(0, execIndex, kf));

    pc = kf->instructions;
    prevPC = pc;
  }
}

}
