//===-- SpecialFunctionHandler.cpp ----------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Common.h"

#include "Memory.h"
#include "SpecialFunctionHandler.h"
#include "TimingSolver.h"

#include "klee/ExecutionState.h"
#include "klee/util/ExprPPrinter.h"

#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KModule.h"

#include "klee/Executor.h"
#include "MemoryManager.h"

#include "llvm/Module.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"

#include <errno.h>

using namespace llvm;
using namespace klee;

/// \todo Almost all of the demands in this file should be replaced
/// with terminateState calls.

///

struct HandlerInfo {
  const char *name;
  SpecialFunctionHandler::Handler handler;
  bool doesNotReturn; /// Intrinsic terminates the process
  bool hasReturnValue; /// Intrinsic has a return value
  bool doNotOverride; /// Intrinsic should not be used if already defined
};

// FIXME: We are more or less committed to requiring an intrinsic
// library these days. We can move some of this stuff there,
// especially things like realloc which have complicated semantics
// w.r.t. forking. Among other things this makes delayed query
// dispatch easier to implement.
HandlerInfo handlerInfo[] = {
#define add(name, handler, ret) { name, \
                                  &SpecialFunctionHandler::handler, \
                                  false, ret, false }
#define addDNR(name, handler) { name, \
                                &SpecialFunctionHandler::handler, \
                                true, false, false }
  addDNR("__assert_rtn", handleAssertFail),
  addDNR("__assert_fail", handleAssertFail),
  addDNR("_assert", handleAssert),
  addDNR("abort", handleAbort),
  addDNR("_exit", handleExit),
  { "exit", &SpecialFunctionHandler::handleExit, true, false, true },
  addDNR("klee_abort", handleAbort),
  addDNR("klee_silent_exit", handleSilentExit),  
  addDNR("klee_report_error", handleReportError),

  add("calloc", handleCalloc, true),
  add("free", handleFree, false),
  add("klee_assume", handleAssume, false),
  add("klee_breakpoint", handleBreakpoint, false),
  add("klee_check_memory_access", handleCheckMemoryAccess, false),
  add("klee_get_valuef", handleGetValue, true),
  add("klee_get_valued", handleGetValue, true),
  add("klee_get_valuel", handleGetValue, true),
  add("klee_get_valuell", handleGetValue, true),
  add("klee_get_value_i32", handleGetValue, true),
  add("klee_get_value_i64", handleGetValue, true),
  add("klee_define_fixed_object", handleDefineFixedObject, false),
  add("klee_get_obj_size", handleGetObjSize, true),
  add("klee_get_errno", handleGetErrno, true),
  add("klee_is_symbolic", handleIsSymbolic, true),
  add("klee_make_symbolic", handleMakeSymbolic, false),
  add("klee_mark_global", handleMarkGlobal, false),
  add("klee_merge", handleMerge, false),
  add("klee_prefer_cex", handlePreferCex, false),
  add("klee_print_expr", handlePrintExpr, false),
  add("klee_print_range", handlePrintRange, false),
  add("klee_set_forking", handleSetForking, false),
  add("klee_stack_trace", handleStackTrace, false),
  add("klee_make_shared", handleMakeShared, false),
  add("klee_bind_shared", handleBindShared, false),
  add("klee_get_context", handleGetContext, false),
  add("klee_get_thread_info", handleGetThreadInfo, true),
  add("klee_get_process_info", handleGetProcessInfo, true),
  add("klee_get_wlist", handleGetWList, true),
  add("klee_thread_preempt", handleThreadPreempt, false),
  add("klee_thread_sleep", handleThreadSleep, false),
  add("klee_thread_notify", handleThreadNotify, false),
  add("klee_warning", handleWarning, false),
  add("klee_warning_once", handleWarningOnce, false),
  add("klee_alias_function", handleAliasFunction, false),
  add("malloc", handleMalloc, true),
  add("realloc", handleRealloc, true),
  add("valloc", handleValloc, true),

  // operator delete[](void*)
  add("_ZdaPv", handleDeleteArray, false),
  // operator delete(void*)
  add("_ZdlPv", handleDelete, false),

  // operator new[](unsigned int)
  add("_Znaj", handleNewArray, true),
  // operator new(unsigned int)
  add("_Znwj", handleNew, true),

  // FIXME-64: This is wrong for 64-bit long...

  // operator new[](unsigned long)
  add("_Znam", handleNewArray, true),
  // operator new(unsigned long)
  add("_Znwm", handleNew, true),


  //pthreads functions
  add("pthread_create", handlePthreadCreate, true),
  add("pthread_join", handlePthreadJoin, true),
  add("pthread_mutex_lock", handlePthreadMutexLock, true),
  add("pthread_mutex_unlock", handlePthreadMutexUnlock, true),
  add("pthread_mutex_init", handlePthreadMutexInit, true),
  add("pthread_mutex_destroy", handlePthreadMutexDestroy, true),
  add("pthread_exit", handlePthreadExit, true),
  add("pthread_cond_wait", handlePthreadCondWait, true),
  add("pthread_cond_signal", handlePthreadCondSignal, true),
  add("pthread_cond_broadcast", handlePthreadCondBroadcast, true),
  add("pthread_cond_init", handlePthreadCondInit, true),
  add("pthread_cond_destroy", handlePthreadCondDestroy, true),

  //multiprocess functions
  add("fork", handleFork, true),


#undef addDNR
#undef add  
};

SpecialFunctionHandler::SpecialFunctionHandler(Executor &_executor) 
  : executor(_executor) {}


void SpecialFunctionHandler::prepare() {
  unsigned N = sizeof(handlerInfo)/sizeof(handlerInfo[0]);

  for (unsigned i=0; i<N; ++i) {
    HandlerInfo &hi = handlerInfo[i];
    Function *f = executor.kmodule->module->getFunction(hi.name);
    
    // No need to create if the function doesn't exist, since it cannot
    // be called in that case.
  
    if (f && (!hi.doNotOverride || f->isDeclaration())) {
      // Make sure NoReturn attribute is set, for optimization and
      // coverage counting.
      if (hi.doesNotReturn)
        f->addFnAttr(Attribute::NoReturn);

      // Change to a declaration since we handle internally (simplifies
      // module and allows deleting dead code).
      if (!f->isDeclaration())
        f->deleteBody();
    }
  }
}

void SpecialFunctionHandler::bind() {
  unsigned N = sizeof(handlerInfo)/sizeof(handlerInfo[0]);

  for (unsigned i=0; i<N; ++i) {
    HandlerInfo &hi = handlerInfo[i];
    Function *f = executor.kmodule->module->getFunction(hi.name);
    
    if (f && (!hi.doNotOverride || f->isDeclaration()))
      handlers[f] = std::make_pair(hi.handler, hi.hasReturnValue);
  }
}


bool SpecialFunctionHandler::handle(ExecutionState &state, 
                                    Function *f,
                                    KInstruction *target,
                                    std::vector< ref<Expr> > &arguments) {
  handlers_ty::iterator it = handlers.find(f);
  if (it != handlers.end()) {    
    Handler h = it->second.first;
    bool hasReturnValue = it->second.second;
     // FIXME: Check this... add test?
    if (!hasReturnValue && !target->inst->use_empty()) {
      executor.terminateStateOnExecError(state, 
                                         "expected return value from void special function");
    } else {
      (this->*h)(state, target, arguments);
    }
    return true;
  } else {
    return false;
  }
}

void SpecialFunctionHandler::processMemoryLocation(ExecutionState &state,
    ref<Expr> address, ref<Expr> size,
    const std::string &name, resolutions_ty &resList) {
  Executor::ExactResolutionList rl;
  executor.resolveExact(state, address, rl, name);

  for (Executor::ExactResolutionList::iterator it = rl.begin(),
         ie = rl.end(); it != ie; ++it) {
    const MemoryObject *mo = it->first.first;
    const ObjectState *os = it->first.second;
    ExecutionState *s = it->second;

    // FIXME: Type coercion should be done consistently somewhere.
    bool res;
    bool success =
      executor.solver->mustBeTrue(*s,
                                  EqExpr::create(ZExtExpr::create(size,
                                                                  Context::get().getPointerWidth()),
                                                 mo->getSizeExpr()),
                                  res);
    assert(success && "FIXME: Unhandled solver failure");

    if (res) {
      resList.push_back(std::make_pair(std::make_pair(mo, os), s));
    } else {
      executor.terminateStateOnError(*s,
                                     "wrong size given to memory operation",
                                     "user.err");
    }
  }
}

bool SpecialFunctionHandler::writeConcreteValue(ExecutionState &state,
        ref<Expr> address, uint64_t value, Expr::Width width) {
  ObjectPair op;

  if (!state.addressSpace().resolveOne(cast<ConstantExpr>(address), op)) {
    executor.terminateStateOnError(state, "invalid pointer for writing concrete value into", "user.err");
    return false;
  }

  ObjectState *os = state.addressSpace().getWriteable(op.first, op.second);

  os->write(op.first->getOffsetExpr(address), ConstantExpr::create(value, width));

  return true;
}

/****/

// reads a concrete string from memory
std::string 
SpecialFunctionHandler::readStringAtAddress(ExecutionState &state, 
                                            ref<Expr> addressExpr) {
  ObjectPair op;
  addressExpr = executor.toUnique(state, addressExpr);
  ref<ConstantExpr> address = cast<ConstantExpr>(addressExpr);
  if (!state.addressSpace().resolveOne(address, op))
    assert(0 && "XXX out of bounds / multiple resolution unhandled");
  bool res;
  assert(executor.solver->mustBeTrue(state, 
                                     EqExpr::create(address, 
                                                    op.first->getBaseExpr()),
                                     res) &&
         res &&
         "XXX interior pointer unhandled");
  const MemoryObject *mo = op.first;
  const ObjectState *os = op.second;

  char *buf = new char[mo->size];

  unsigned i;
  for (i = 0; i < mo->size - 1; i++) {
    ref<Expr> cur = os->read8(i);
    cur = executor.toUnique(state, cur);
    assert(isa<ConstantExpr>(cur) && 
           "hit symbolic char while reading concrete string");
    buf[i] = cast<ConstantExpr>(cur)->getZExtValue(8);
  }
  buf[i] = 0;
  
  std::string result(buf);
  delete[] buf;
  return result;
}

/****/

void SpecialFunctionHandler::handleAbort(ExecutionState &state,
                           KInstruction *target,
                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==0 && "invalid number of arguments to abort");


  executor.terminateStateOnError(state, "abort failure", "abort.err");
}

void SpecialFunctionHandler::handleExit(ExecutionState &state,
                           KInstruction *target,
                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to exit");

  if (state.processes.size() == 1)
    executor.terminateStateOnExit(state);
  else
    executor.executeProcessExit(state, target);
}

void SpecialFunctionHandler::handleSilentExit(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to exit");

  if (state.processes.size() == 1)
    executor.terminateState(state);
  else
    executor.executeProcessExit(state, target);
}

void SpecialFunctionHandler::handleAliasFunction(ExecutionState &state,
						 KInstruction *target,
						 std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 && 
         "invalid number of arguments to klee_alias_function");
  std::string old_fn = readStringAtAddress(state, arguments[0]);
  std::string new_fn = readStringAtAddress(state, arguments[1]);
  //std::cerr << "Replacing " << old_fn << "() with " << new_fn << "()\n";
  if (old_fn == new_fn)
    state.removeFnAlias(old_fn);
  else state.addFnAlias(old_fn, new_fn);
}

void SpecialFunctionHandler::handleAssert(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==3 && "invalid number of arguments to _assert");  
  

  executor.terminateStateOnError(state,
                                 "ASSERTION FAIL: " + readStringAtAddress(state, arguments[0]),
                                 "assert.err");
}

void SpecialFunctionHandler::handleAssertFail(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==4 && "invalid number of arguments to __assert_fail");
  

  executor.terminateStateOnError(state,
                                 "ASSERTION FAIL: " + readStringAtAddress(state, arguments[0]),
                                 "assert.err");
}

void SpecialFunctionHandler::handleReportError(ExecutionState &state,
                                               KInstruction *target,
                                               std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==4 && "invalid number of arguments to klee_report_error");
  
  // arguments[0], arguments[1] are file, line
  
  executor.terminateStateOnError(state,
                                 readStringAtAddress(state, arguments[2]),
                                 readStringAtAddress(state, arguments[3]).c_str());
}

void SpecialFunctionHandler::handleMerge(ExecutionState &state,
                           KInstruction *target,
                           std::vector<ref<Expr> > &arguments) {
  // nop
}

void SpecialFunctionHandler::handleNew(ExecutionState &state,
                         KInstruction *target,
                         std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to new");

  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleDelete(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  // FIXME: Should check proper pairing with allocation type (malloc/free,
  // new/delete, new[]/delete[]).

  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to delete");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleNewArray(ExecutionState &state,
                              KInstruction *target,
                              std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to new[]");
  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleDeleteArray(ExecutionState &state,
                                 KInstruction *target,
                                 std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to delete[]");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleMalloc(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to malloc");
  executor.executeAlloc(state, arguments[0], false, target);
}


void SpecialFunctionHandler::handleValloc(ExecutionState &state, 
					  KInstruction *target, 
					  std::vector<ref<Expr> > &arguments) {
  
  // XXX ignoring for now the "multiple of page size " requirement 
  //- executing the regular alloc
  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to valloc");
  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleAssume(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to klee_assume");
  
  ref<Expr> e = arguments[0];
  
  if (e->getWidth() != Expr::Bool)
    e = NeExpr::create(e, ConstantExpr::create(0, e->getWidth()));
  
  bool res;
  bool success = executor.solver->mustBeFalse(state, e, res);
  assert(success && "FIXME: Unhandled solver failure");
  if (res) {
    executor.terminateStateOnError(state, 
                                   "invalid klee_assume call (provably false)",
                                   "user.err");
  } else {
    executor.addConstraint(state, e);
  }
}

void SpecialFunctionHandler::handleIsSymbolic(ExecutionState &state,
                                KInstruction *target,
                                std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to klee_is_symbolic");

  executor.bindLocal(target, state, 
                     ConstantExpr::create(!isa<ConstantExpr>(arguments[0]),
                                          Expr::Int32));
}

void SpecialFunctionHandler::handlePreferCex(ExecutionState &state,
                                             KInstruction *target,
                                             std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_prefex_cex");

  ref<Expr> cond = arguments[1];
  if (cond->getWidth() != Expr::Bool)
    cond = NeExpr::create(cond, ConstantExpr::alloc(0, cond->getWidth()));

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "prefex_cex");
  
  assert(rl.size() == 1 &&
         "prefer_cex target must resolve to precisely one object");

  rl[0].first.first->cexPreferences.push_back(cond);
}

void SpecialFunctionHandler::handlePrintExpr(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_print_expr");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  std::cerr << msg_str << ":" << arguments[1] << "\n";
}

void SpecialFunctionHandler::handleSetForking(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_set_forking");
  ref<Expr> value = executor.toUnique(state, arguments[0]);
  
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(value)) {
    state.forkDisabled = CE->isZero();
  } else {
    executor.terminateStateOnError(state, 
                                   "klee_set_forking requires a constant arg",
                                   "user.err");
  }
}

void SpecialFunctionHandler::handleStackTrace(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  state.dumpStack(std::cout);
}

void SpecialFunctionHandler::handleWarning(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to klee_warning");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  klee_warning("%s: %s", state.stack().back().kf->function->getName().data(),
               msg_str.c_str());
}

void SpecialFunctionHandler::handleWarningOnce(ExecutionState &state,
                                               KInstruction *target,
                                               std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_warning_once");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  klee_warning_once(0, "%s: %s", state.stack().back().kf->function->getName().data(),
                    msg_str.c_str());
}

void SpecialFunctionHandler::handlePrintRange(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_print_range");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  std::cerr << msg_str << ":" << arguments[1];
  if (!isa<ConstantExpr>(arguments[1])) {
    // FIXME: Pull into a unique value method?
    ref<ConstantExpr> value;
    bool success = executor.solver->getValue(state, arguments[1], value);
    assert(success && "FIXME: Unhandled solver failure");
    bool res;
    success = executor.solver->mustBeTrue(state, 
                                          EqExpr::create(arguments[1], value), 
                                          res);
    assert(success && "FIXME: Unhandled solver failure");
    if (res) {
      std::cerr << " == " << value;
    } else { 
      std::cerr << " ~= " << value;
      std::pair< ref<Expr>, ref<Expr> > res =
        executor.solver->getRange(state, arguments[1]);
      std::cerr << " (in [" << res.first << ", " << res.second <<"])";
    }
  }
  std::cerr << "\n";
}

void SpecialFunctionHandler::handleGetObjSize(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_get_obj_size");
  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "klee_get_obj_size");
  for (Executor::ExactResolutionList::iterator it = rl.begin(), 
         ie = rl.end(); it != ie; ++it) {
    executor.bindLocal(target, *it->second, 
                       ConstantExpr::create(it->first.first->size, Expr::Int32));
  }
}

void SpecialFunctionHandler::handleGetErrno(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==0 &&
         "invalid number of arguments to klee_get_obj_size");
  executor.bindLocal(target, state,
                     ConstantExpr::create(errno, Expr::Int32));
}

// pthreads handlers

void SpecialFunctionHandler::handlePthreadCreate(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments) 
{
  assert(arguments.size() == 4 &&
	 "invalid number of arguments to pthread_create");
  
  //for now we ignore the attribute arguments to pthread_create
  executor.executePthreadCreate(state, target, arguments[0], arguments[1],
      arguments[2], arguments[3]);
}

void SpecialFunctionHandler::handlePthreadJoin(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==2 &&
	 "invalid number of arguments to pthread_join");
  
  executor.executePthreadJoin(state, target, arguments[0], arguments[1]);  
}

void SpecialFunctionHandler::handlePthreadMutexLock(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==1 && 
	 "invalid number of arguments to pthread_mutex_lock");
  
  executor.executePthreadMutexLock(state, target, arguments[0]);  
}

void SpecialFunctionHandler::handlePthreadMutexUnlock(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==1 &&
	 "invalid number of arguments to pthread_mutex_unlock");
  
  executor.executePthreadMutexUnlock(state, target, arguments[0]);  
}

void SpecialFunctionHandler::handlePthreadMutexInit(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==2 &&
	 "invalid number of arguments to pthread_mutex_init");
  
  executor.executePthreadMutexInit(state, target, arguments[0], arguments[1]);  
}

void SpecialFunctionHandler::handlePthreadMutexDestroy(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==1 &&
	 "invalid number of arguments to pthread_mutex_destroy");
  
  executor.executePthreadMutexDestroy(state, target, arguments[0]);  
}


void SpecialFunctionHandler::handlePthreadExit(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==1 &&
	 "invalid number of arguments to pthread_exit");
  
  executor.executePthreadExit(state, target, arguments[0]);  
}

void SpecialFunctionHandler::handlePthreadCondWait(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==2 && 
	 "invalid number of arguments to pthread_cond_wait");
  
  executor.executePthreadCondWait(state, target, arguments[0], arguments[1]);  
}

void SpecialFunctionHandler::handlePthreadCondSignal(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==1 &&
	 "invalid number of arguments to pthread_signal");
  
  executor.executePthreadCondSignal(state, target, arguments[0]);  
}

void SpecialFunctionHandler::handlePthreadCondBroadcast(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==1 &&
	 "invalid number of arguments to pthread_broadcast");
  
  executor.executePthreadCondBroadcast(state, target, arguments[0]);  
}

void SpecialFunctionHandler::handlePthreadCondInit(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==2 &&
	 "invalid number of arguments to pthread_cond_init");
  
  executor.executePthreadCondInit(state, target, arguments[0], arguments[1]);  
}

void SpecialFunctionHandler::handlePthreadCondDestroy(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments)
{
  assert(arguments.size()==1 &&
	 "invalid number of arguments to pthread_cond_destroy");
  
  executor.executePthreadCondDestroy(state, target, arguments[0]);  
}

void SpecialFunctionHandler::handleCalloc(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==2 &&
         "invalid number of arguments to calloc");

  ref<Expr> size = MulExpr::create(arguments[0],
                                   arguments[1]);
  executor.executeAlloc(state, size, false, target, true);
}

void SpecialFunctionHandler::handleRealloc(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==2 &&
         "invalid number of arguments to realloc");
  ref<Expr> address = arguments[0];
  ref<Expr> size = arguments[1];

  Executor::StatePair zeroSize = executor.fork(state, 
                                               Expr::createIsZero(size), 
                                               true);
  
  if (zeroSize.first) { // size == 0
    executor.executeFree(*zeroSize.first, address, target);   
  }
  if (zeroSize.second) { // size != 0
    Executor::StatePair zeroPointer = executor.fork(*zeroSize.second, 
                                                    Expr::createIsZero(address), 
                                                    true);
    
    if (zeroPointer.first) { // address == 0
      executor.executeAlloc(*zeroPointer.first, size, false, target);
    } 
    if (zeroPointer.second) { // address != 0
      Executor::ExactResolutionList rl;
      executor.resolveExact(*zeroPointer.second, address, rl, "realloc");
      
      for (Executor::ExactResolutionList::iterator it = rl.begin(), 
             ie = rl.end(); it != ie; ++it) {
        executor.executeAlloc(*it->second, size, false, target, false, 
                              it->first.second);
      }
    }
  }
}

void SpecialFunctionHandler::handleFree(ExecutionState &state,
                          KInstruction *target,
                          std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 &&
         "invalid number of arguments to free");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleBindShared(ExecutionState &state,
                          KInstruction *target,
                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
      "invalid number of arguments to klee_bind_shared");

  ref<Expr> address = executor.toUnique(state, arguments[0]);
  ref<Expr> size = executor.toUnique(state, arguments[1]);

  if (!isa<ConstantExpr>(address) || !isa<ConstantExpr>(size)) {
    executor.terminateStateOnError(state,
                                   "klee_bind_shared requires constant args",
                                   "user.err");
    return;
  }

  ObjectPair referenceOp;

  if (state.addressSpace().resolveOne(cast<ConstantExpr>(address), referenceOp)) {
    executor.terminateStateOnError(state, "klee_bind_shared attempted on bound object",
        "user.err");
    return;
  }

  for (ExecutionState::processes_ty::iterator it = state.processes.begin();
      it != state.processes.end(); it++) {
    if (it == state.crtProcessIt)
      continue;

    ObjectPair op;

    if (!it->second.addressSpace.resolveOne(cast<ConstantExpr>(address), op))
      continue;

    if (!op.second->isShared) {
      executor.terminateStateOnError(state, "klee_bind_shared requires shared object",
          "user.err");
      return;
    }

    if (!referenceOp.first)
      referenceOp = op;
    else {
      if (op != referenceOp) {
        executor.terminateStateOnError(state, "inconsistent shared memory state",
            "user.err");
        return;
      }
    }
  }

  if (!referenceOp.first) {
    executor.terminateStateOnError(state, "invalid address for klee_bind_shared",
        "user.err");
    return;
  }

  state.addressSpace().bindSharedObject(referenceOp.first,
      const_cast<ObjectState*>(referenceOp.second));
}

void SpecialFunctionHandler::handleMakeShared(ExecutionState &state,
                          KInstruction *target,
                          std::vector<ref<Expr> > &arguments) {

  assert(arguments.size() == 2 &&
        "invalid number of arguments to klee_make_shared");

  resolutions_ty resList;

  processMemoryLocation(state, arguments[0], arguments[1], "make_shared", resList);

  for (resolutions_ty::iterator it = resList.begin(); it != resList.end();
      it++) {
    const MemoryObject *mo = it->first.first;
    const ObjectState *os = it->first.second;
    ExecutionState *s = it->second;

    if (mo->isLocal) {
      executor.terminateStateOnError(*s,
                                     "cannot share local object",
                                     "user.err");
      continue;
    }

    unsigned int bindCount = 0;
    for (ExecutionState::processes_ty::iterator pit = s->processes.begin();
        pit != s->processes.end(); pit++) {
      if (pit->second.addressSpace.findObject(mo) != NULL)
        bindCount++;
    }

    if (bindCount != 1) {
      executor.terminateStateOnError(*s, "cannot shared already forked object",
           "user.err");
      continue;
    }

    ObjectState *newOS = state.addressSpace().getWriteable(mo, os);
    newOS->isShared = true;
  }
}

void SpecialFunctionHandler::handleGetContext(ExecutionState &state,
                          KInstruction *target,
                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 3 &&
      "invalid number of arguments to klee_get_context");

  ref<Expr> tidAddr = executor.toUnique(state, arguments[0]);
  ref<Expr> pidAddr = executor.toUnique(state, arguments[1]);
  ref<Expr> ppidAddr = executor.toUnique(state, arguments[2]);

  if (!isa<ConstantExpr>(tidAddr) || !isa<ConstantExpr>(pidAddr) ||
      !isa<ConstantExpr>(ppidAddr)) {
    executor.terminateStateOnError(state,
                                   "klee_get_context requires constant args",
                                   "user.err");
    return;
  }

  if (!tidAddr->isZero()) {
    if (!writeConcreteValue(state, tidAddr, state.crtThread().tid,
        executor.getWidthForLLVMType(Type::getInt64Ty(getGlobalContext()))))
      return;
  }

  if (!pidAddr->isZero()) {
    if (!writeConcreteValue(state, pidAddr, state.crtProcess().pid,
        executor.getWidthForLLVMType(Type::getInt32Ty(getGlobalContext()))))
      return;
  }

  if (!ppidAddr->isZero()) {
    if (!writeConcreteValue(state, ppidAddr, state.crtProcess().ppid,
        executor.getWidthForLLVMType(Type::getInt32Ty(getGlobalContext()))))
      return;
  }

}

void SpecialFunctionHandler::handleGetThreadInfo(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 && "invalid number of arguments to klee_get_thread_info");

  ref<Expr> tidExpr = executor.toUnique(state, arguments[0]);
  ref<Expr> wlistAddr = executor.toUnique(state, arguments[1]);

  if (!isa<ConstantExpr>(tidExpr) || !isa<ConstantExpr>(wlistAddr)) {
    executor.terminateStateOnError(state, "klee_get_thread_info", "user.err");
    return;
  }

  uint64_t tid = cast<ConstantExpr>(tidExpr)->getZExtValue();

  if (state.threads.count(tid) == 0) {
    executor.bindLocal(target, state, ConstantExpr::create(0,
        executor.getWidthForLLVMType(target->inst->getType())));
    return;
  }

  Thread &t = state.threads.find(tid)->second;
  Process &p = state.processes.find(t.pid)->second;

  if (!wlistAddr->isZero()) {
    if (!writeConcreteValue(state, wlistAddr, p.threads[tid],
        executor.getWidthForLLVMType(Type::getInt64Ty(getGlobalContext()))))
      return;
  }

  executor.bindLocal(target, state, ConstantExpr::create(1,
      executor.getWidthForLLVMType(target->inst->getType())));
}

void SpecialFunctionHandler::handleGetProcessInfo(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 3 && "invalid number of arguments to klee_get_process_info");

  ref<Expr> pidExpr = executor.toUnique(state, arguments[0]);
  ref<Expr> plistAddr = executor.toUnique(state, arguments[1]);
  ref<Expr> clistAddr = executor.toUnique(state, arguments[2]);

  if (!isa<ConstantExpr>(pidExpr) || !isa<ConstantExpr>(plistAddr) ||
      !isa<ConstantExpr>(clistAddr)) {
    executor.terminateStateOnError(state, "klee_get_process_info", "user.err");
    return;
  }

  int32_t pid = cast<ConstantExpr>(pidExpr)->getZExtValue();

  if (state.processes.count(pid) == 0) {
    executor.bindLocal(target, state, ConstantExpr::create(0,
        executor.getWidthForLLVMType(target->inst->getType())));
    return;
  }

  Process &p = state.processes.find(pid)->second;
  Process &pp = state.processes.find(p.ppid)->second;

  if (!plistAddr->isZero()) {
    if (!writeConcreteValue(state, plistAddr, pp.children[pid],
        executor.getWidthForLLVMType(Type::getInt64Ty(getGlobalContext()))))
      return;
  }

  if (!clistAddr->isZero()) {
    if (!writeConcreteValue(state, clistAddr, p.anyChild,
        executor.getWidthForLLVMType(Type::getInt64Ty(getGlobalContext()))))
      return;
  }

  executor.bindLocal(target, state, ConstantExpr::create(1,
      executor.getWidthForLLVMType(target->inst->getType())));
}

void SpecialFunctionHandler::handleGetWList(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.empty() && "invalid number of arguments to klee_get_wlist");

  wlist_id_t id = state.getWaitingList();

  executor.bindLocal(target, state, ConstantExpr::create(id,
      executor.getWidthForLLVMType(target->inst->getType())));
}

void SpecialFunctionHandler::handleThreadPreempt(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {

}

void SpecialFunctionHandler::handleThreadSleep(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {

}

void SpecialFunctionHandler::handleThreadNotify(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {

}

void SpecialFunctionHandler::handleCheckMemoryAccess(ExecutionState &state,
                                                     KInstruction *target,
                                                     std::vector<ref<Expr> > 
                                                       &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_check_memory_access");

  ref<Expr> address = executor.toUnique(state, arguments[0]);
  ref<Expr> size = executor.toUnique(state, arguments[1]);
  if (!isa<ConstantExpr>(address) || !isa<ConstantExpr>(size)) {
    executor.terminateStateOnError(state, 
                                   "check_memory_access requires constant args",
                                   "user.err");
  } else {
    ObjectPair op;

    if (!state.addressSpace().resolveOne(cast<ConstantExpr>(address), op)) {
      executor.terminateStateOnError(state,
                                     "check_memory_access: memory error",
                                     "ptr.err",
                                     executor.getAddressInfo(state, address));
    } else {
      ref<Expr> chk = 
        op.first->getBoundsCheckPointer(address, 
                                        cast<ConstantExpr>(size)->getZExtValue());
      if (!chk->isTrue()) {
        executor.terminateStateOnError(state,
                                       "check_memory_access: memory error",
                                       "ptr.err",
                                       executor.getAddressInfo(state, address));
      }
    }
  }
}

void SpecialFunctionHandler::handleGetValue(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_get_value");

  executor.executeGetValue(state, arguments[0], target);
}

void SpecialFunctionHandler::handleDefineFixedObject(ExecutionState &state,
                                                     KInstruction *target,
                                                     std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_define_fixed_object");
  assert(isa<ConstantExpr>(arguments[0]) &&
         "expect constant address argument to klee_define_fixed_object");
  assert(isa<ConstantExpr>(arguments[1]) &&
         "expect constant size argument to klee_define_fixed_object");
  
  uint64_t address = cast<ConstantExpr>(arguments[0])->getZExtValue();
  uint64_t size = cast<ConstantExpr>(arguments[1])->getZExtValue();
  MemoryObject *mo = executor.memory->allocateFixed(address, size, state.prevPC()->inst);
  executor.bindObjectInState(state, mo, false);
  mo->isUserSpecified = true; // XXX hack;
}

void SpecialFunctionHandler::handleMakeSymbolic(ExecutionState &state,
                                                KInstruction *target,
                                                std::vector<ref<Expr> > &arguments) {
  std::string name;

  // FIXME: For backwards compatibility, we should eventually enforce the
  // correct arguments.
  if (arguments.size() == 2) {
    name = "unnamed";
  } else {
    // FIXME: Should be a user.err, not an assert.
    assert(arguments.size()==3 &&
           "invalid number of arguments to klee_make_symbolic");  
    name = readStringAtAddress(state, arguments[2]);
  }

  resolutions_ty resList;

  processMemoryLocation(state, arguments[0], arguments[1], "make_symbolic", resList);

  for (resolutions_ty::iterator it = resList.begin(); it != resList.end();
      it++) {
    const MemoryObject *mo = it->first.first;
    const ObjectState *os = it->first.second;
    ExecutionState *s = it->second;

    mo->setName(name);

    if (os->readOnly) {
      executor.terminateStateOnError(*s,
                                     "cannot make readonly object symbolic",
                                     "user.err");
    } else {
      executor.executeMakeSymbolic(*s, mo, os->isShared);
    }
  }
}

void SpecialFunctionHandler::handleBreakpoint(ExecutionState &state,
                                                KInstruction *target,
                                                std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to klee_breakpoint");

  if (ConstantExpr *id = dyn_cast<ConstantExpr>(arguments[0])) {
    executor.executeBreakpoint(state, (unsigned int)id->getZExtValue());
  } else {
    executor.terminateStateOnError(state,
                                   "klee_breakpoint requires a constant arg",
                                   "user.err");
  }
}

void SpecialFunctionHandler::handleFork(ExecutionState &state,
    KInstruction *target, std::vector<ref<Expr> > &arguments) {
  assert(arguments.empty() && "fork does not take any arguments");

  executor.executeProcessFork(state, target);
}

void SpecialFunctionHandler::handleMarkGlobal(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_mark_global");  

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "mark_global");
  
  for (Executor::ExactResolutionList::iterator it = rl.begin(), 
         ie = rl.end(); it != ie; ++it) {
    const MemoryObject *mo = it->first.first;
    assert(!mo->isLocal);
    mo->isGlobal = true;
  }
}
