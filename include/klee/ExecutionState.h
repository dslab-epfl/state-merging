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

#include "klee/Threading.h"
#include "klee/MultiProcess.h"

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
  /* System-level parameters */
  Executor *executor;

  bool fakeState;
  // Are we currently underconstrained?  Hack: value is size to make fake
  // objects.
  unsigned depth;

  /// Disables forking, set by user code.
  bool forkDisabled;


  mutable double queryCost;
  double weight;

  TreeOStream pathOS, symPathOS;
  unsigned instsSinceCovNew;

  bool coveredNew;
  sys::TimeValue lastCoveredTime;

  void setCoveredNew() {
	  coveredNew = true;
	  lastCoveredTime = sys::TimeValue::now();
  }

  std::map<const std::string*, std::set<unsigned> > coveredLines;

  PTreeNode *ptreeNode;

  /// ordered list of symbolics: used to generate test cases. 
  //
  // FIXME: Move to a shared list structure (not critical).
  std::vector< std::pair<const MemoryObject*, const Array*> > symbolics;


  // For a multi threded ExecutionState
  std::vector<Thread*> threads;
  std::vector<Process*> processes;

  unsigned int preemptions;


  /* Shortcut methods */

  Thread &crtThread() { return *threads.front(); }
  const Thread &crtThread() const { return *threads.front(); }

  // pc - pointer to current instruction stream
   KInstIterator& pc() { return crtThread().pc; }
   const KInstIterator& pc() const { return crtThread().pc; }

   KInstIterator& prevPC() { return crtThread().prevPC; }
   const KInstIterator& prevPC() const { return crtThread().prevPC; }

   stack_ty& stack() { return crtThread().stack; }
   const stack_ty& stack() const { return crtThread().stack; }

  std::string getFnAlias(std::string fn);
  void addFnAlias(std::string old_fn, std::string new_fn);
  void removeFnAlias(std::string fn);

public:
  ExecutionState(Executor *_executor, KFunction *kf);

  // XXX total hack, just used to make a state so solver can
  // use on structure
  ExecutionState(Executor *_executor, const std::vector<ref<Expr> > &assumptions);

  ~ExecutionState();
  
  ExecutionState *branch();

  void pushFrame(KInstIterator caller, KFunction *kf) {
    crtThread().pushFrame(caller, kf);
  }
  void popFrame() {
    crtThread().popFrame();
  }

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
