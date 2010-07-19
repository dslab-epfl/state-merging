//===-- ExecutionState.h ----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTIONSTATE_H
#define KLEE_EXECUTIONSTATE_H

#include "klee/Constraints.h"
#include "klee/Expr.h"
#include "klee/Internal/ADT/TreeStream.h"

// FIXME: We do not want to be exposing these? :(
#include "../../lib/Core/AddressSpace.h"
#include "klee/Internal/Module/KInstIterator.h"

#include "llvm/System/TimeValue.h"

#include "klee/Thread.h"
#include "klee/Mutex.h"
#include "klee/CondVar.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;

namespace cloud9 {
namespace worker {
class SymbolicState;
}
}

namespace klee {
  class Array;
  class CallPathNode;
  struct Cell;
  struct KFunction;
  struct KInstruction;
  class MemoryObject;
  class PTreeNode;
  struct InstructionInfo;
  class Executor;

  class ExecutionState;

namespace c9 {
std::ostream &printStateStack(std::ostream &os, const ExecutionState &state);
std::ostream &printStateConstraints(std::ostream &os, const ExecutionState &state);
std::ostream &printStateMemorySummary(std::ostream &os, const ExecutionState &state);
}

std::ostream &operator<<(std::ostream &os, const ExecutionState &state); // XXX Cloud9 hack
std::ostream &operator<<(std::ostream &os, const MemoryMap &mm);


class ExecutionState {
	friend class ObjectState;

public:
  typedef std::vector<StackFrame> stack_ty;

private:
  // unsupported, use copy constructor
  ExecutionState &operator=(const ExecutionState&); 
  std::map< std::string, std::string > fnAliases;

  cloud9::worker::SymbolicState *c9State;

public:
  Executor *executor;

  bool fakeState;
  // Are we currently underconstrained?  Hack: value is size to make fake
  // objects.
  unsigned underConstrained;
  unsigned depth;
  
  // pc - pointer to current instruction stream
  KInstIterator pc, prevPC;
  stack_ty *stack;
  ConstraintManager constraints;
  mutable double queryCost;
  double weight;
  AddressSpace addressSpace;
  TreeOStream pathOS, symPathOS;
  unsigned instsSinceCovNew;

  bool coveredNew;
  sys::TimeValue lastCoveredTime;

  void setCoveredNew() {
	  coveredNew = true;
	  lastCoveredTime = sys::TimeValue::now();
  }

  /// Disables forking, set by user code.
  bool forkDisabled;

  std::map<const std::string*, std::set<unsigned> > coveredLines;
  PTreeNode *ptreeNode;

  /// ordered list of symbolics: used to generate test cases. 
  //
  // FIXME: Move to a shared list structure (not critical).
  std::vector< std::pair<const MemoryObject*, const Array*> > symbolics;

  // Used by the checkpoint/rollback methods for fake objects.
  // FIXME: not freeing things on branch deletion.
  MemoryMap shadowObjects;

  unsigned incomingBBIndex;

  // For a multi threded ExecutionState
  std::vector<Thread*> threads;
  Thread* crtThread;
  std::map<ref<Expr>, Mutex*> mutexes;
  std::map<ref<Expr>, CondVar*> cond_vars;
  unsigned int preemptions;
  std::vector<TraceItem> trace;
  std::map< ref<Expr> , KFunction*> tls_keys;
  uint64_t TLSKeyGen;
  ref<Expr> nextTLSKey(); 
  bool deadlock;
  std::map<Mutex*, ExecutionState*> backtrackingStates;

  int executedInstructions;

  std::string getFnAlias(std::string fn);
  void addFnAlias(std::string old_fn, std::string new_fn);
  void removeFnAlias(std::string fn);
  
  //Thread schedule trace management
  
  /// Add a trace item to the trace
  void addTraceItem(SchedSyncTraceInfo mtrace,
		    SchedThreadTraceInfo ttrace,
		    uint64_t tid);
  /// Mutex operations
  void updateTraceInfo(Mutex *m, Thread *t);
  /// Condition variable operations
  void updateTraceInfo(CondVar *cv, Thread *t);
  /// Thread create operations
  void addPthreadCreateTraceItem(int tid);

private:
  ExecutionState(Executor *_executor) : c9State(NULL), executor(_executor),
  fakeState(false), underConstrained(0), addressSpace(this),
  lastCoveredTime(sys::TimeValue::now()), ptreeNode(0) {}

public:
  ExecutionState(Executor *_executor, KFunction *kf);

  // XXX total hack, just used to make a state so solver can
  // use on structure
  ExecutionState(Executor *_executor, const std::vector<ref<Expr> > &assumptions);

  ExecutionState(const ExecutionState &state);

  ~ExecutionState();
  
  ExecutionState *branch();

  void pushFrame(KInstIterator caller, KFunction *kf);
  void popFrame();

  void addSymbolic(const MemoryObject *mo, const Array *array) { 
    symbolics.push_back(std::make_pair(mo, array));
  }
  void addConstraint(ref<Expr> e) { 
    constraints.addConstraint(e); 
  }

  bool merge(const ExecutionState &b);

  cloud9::worker::SymbolicState *getCloud9State() const { return c9State; }
  void setCloud9State(cloud9::worker::SymbolicState *state) { c9State = state; }

  void dumpStack(std::ostream &out) const;
};

}

#endif
