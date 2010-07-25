//===-- ExecutionState.cpp ------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/ExecutionState.h"

#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/util/ExprPPrinter.h"
#include "cloud9/Logger.h"

#include "klee/Expr.h"

#include "Memory.h"

#include "llvm/Function.h"
#include "llvm/Support/CommandLine.h"

#include <iostream>
#include <iomanip>
#include <cassert>
#include <map>
#include <set>
#include <stdarg.h>

using namespace llvm;
using namespace klee;

namespace { 
  cl::opt<bool>
  DebugLogStateMerge("debug-log-state-merge");
}

namespace klee {

/***/



/***/

ExecutionState::ExecutionState(Executor *_executor, KFunction *kf)
  : c9State(NULL),
    executor(_executor),
    fakeState(false),
    depth(0),
    queryCost(0.), 
    weight(1),
    instsSinceCovNew(0),
    coveredNew(false),
    lastCoveredTime(sys::TimeValue::now()),
    forkDisabled(false),
    ptreeNode(0),
    preemptions(0) {

  setupMain(kf);
}

ExecutionState::ExecutionState(Executor *_executor, const std::vector<ref<Expr> > &assumptions)
  : c9State(NULL),
    executor(_executor),
    fakeState(true),
    globalConstraints(assumptions),
    queryCost(0.),
    lastCoveredTime(sys::TimeValue::now()),
    ptreeNode(0),
    preemptions(0) {

  setupMain(NULL);


}

void ExecutionState::setupMain(KFunction *kf) {
  Process mainProc = Process();
  Thread mainThread = Thread(kf);

  mainThread.pid = mainProc.pid;
  mainProc.threads.insert(mainThread.tid);
  mainProc.addressSpace.pid = mainProc.pid;

  threads.insert(std::make_pair(mainThread.tid, mainThread));
  processes.insert(std::make_pair(mainProc.pid, mainProc));

  crtThreadIt = threads.begin();
  crtProcessIt = processes.find(crtThreadIt->second.pid);
}

Thread& ExecutionState::createThread(KFunction *kf) {
  Thread newThread = Thread(kf);
  newThread.pid = crtProcess().pid;
  crtProcess().threads.insert(newThread.tid);

  threads.insert(std::make_pair(newThread.tid, newThread));

  return threads.find(newThread.tid)->second;
}

Process& ExecutionState::forkProcess() {
  Process forked = Process(crtProcess());

  forked.pid = Process::pidCounter++;
  forked.threads.clear();
  forked.addressSpace.cowKey = crtProcess().addressSpace.cowKey;
  forked.addressSpace.pid = forked.pid;

  forked.forkPath.push_back(1); // Child
  crtProcess().forkPath.push_back(0); // Parent

  Thread forkedThread = Thread(crtThread());
  forkedThread.pid = forked.pid;
  forkedThread.tid = Thread::tidCounter++;

  forked.threads.insert(forkedThread.tid);

  threads.insert(std::make_pair(forkedThread.tid, forkedThread));
  processes.insert(std::make_pair(forked.pid, forked));

  return processes.find(forked.pid)->second;
}

void ExecutionState::terminateThread() {
  assert(threads.size() > 1);

  if (crtProcess().threads.size() == 1)
    processes.erase(crtProcess().pid);
  else
    crtProcess().threads.erase(crtThread().tid);

  threads_ty::iterator oldIt = crtThreadIt;

  scheduleNext(nextThread(crtThreadIt));

  threads.erase(oldIt);
}

void ExecutionState::terminateThread(threads_ty::iterator it) {
  if (it == crtThreadIt)
    terminateThread();
  else {
    Process &proc = processes.find(it->second.pid)->second;
    if (proc.threads.size() == 1)
      processes.erase(proc.pid);
    else
      proc.threads.erase(it->second.tid);

    threads.erase(it);
  }
}

ExecutionState::~ExecutionState() {
  for (threads_ty::iterator it = threads.begin(); it != threads.end(); it++) {
    Thread &t = it->second;
    while (!t.stack.empty())
      popFrame(t);
  }
}

ExecutionState *ExecutionState::branch() {
  depth++;

  ExecutionState *falseState = new ExecutionState(*this);
  
  falseState->coveredNew = false;
  falseState->coveredLines.clear();
  falseState->c9State = NULL;

  falseState->crtThreadIt = falseState->threads.find(crtThreadIt->second.tid);
  falseState->crtProcessIt = falseState->processes.find(crtProcessIt->second.pid);

  weight *= .5;
  falseState->weight -= weight;

  return falseState;
}

///

std::string ExecutionState::getFnAlias(std::string fn) {
  std::map < std::string, std::string >::iterator it = fnAliases.find(fn);
  if (it != fnAliases.end())
    return it->second;
  else return "";
}

void ExecutionState::addFnAlias(std::string old_fn, std::string new_fn) {
  fnAliases[old_fn] = new_fn;
}

void ExecutionState::removeFnAlias(std::string fn) {
  fnAliases.erase(fn);
}

/**/
namespace c9 {

std::ostream &printStateStack(std::ostream &os, const ExecutionState &state) {
	for (ExecutionState::stack_ty::const_iterator it = state.stack().begin();
			it != state.stack().end(); it++) {
		if (it != state.stack().begin()) {
			os << '(' << it->caller->info->assemblyLine << ',' << it->caller->info->file << ':' << it->caller->info->line << ')';
			os << "]/[";
		} else {
			os << "[";
		}
		os << it->kf->function->getName().str();
	}
	os << '(' << state.pc()->info->assemblyLine << ',' << state.pc()->info->file << ':' << state.pc()->info->line << ')';
	os << "]";

	return os;
}


std::ostream &printStateConstraints(std::ostream &os, const ExecutionState &state) {
	ExprPPrinter::printConstraints(os, state.constraints());

	return os;
}

std::ostream &printStateMemorySummary(std::ostream &os,
		const ExecutionState &state) {
	const MemoryMap &mm = state.addressSpace().objects;

	os << "{";
	MemoryMap::iterator it = mm.begin();
	MemoryMap::iterator ie = mm.end();
	if (it != ie) {
		os << "MO" << it->first->id << ":" << it->second;
		for (++it; it != ie; ++it)
			os << ", MO" << it->first->id << ":" << it->second;
	}
	os << "}";
	return os;
}

}

std::ostream &operator<<(std::ostream &os, const MemoryMap &mm) {
	os << "{";
	MemoryMap::iterator it = mm.begin();
	MemoryMap::iterator ie = mm.end();
	if (it != ie) {
		os << "MO" << it->first->id << ":" << it->second;
		for (++it; it != ie; ++it)
			os << ", MO" << it->first->id << ":" << it->second;
	}
	os << "}";
	return os;
}

std::ostream &operator<<(std::ostream &os, const ExecutionState &state) {
	return klee::c9::printStateStack(os, state);
}




bool ExecutionState::merge(const ExecutionState &b) {
  if (DebugLogStateMerge)
    std::cerr << "-- attempting merge of A:" 
               << this << " with B:" << &b << "--\n";
  if (pc() != b.pc())
    return false;

  // XXX is it even possible for these to differ? does it matter? probably
  // implies difference in object states?
  if (symbolics!=b.symbolics)
    return false;

  {
    std::vector<StackFrame>::const_iterator itA = stack().begin();
    std::vector<StackFrame>::const_iterator itB = b.stack().begin();
    while (itA!=stack().end() && itB!=b.stack().end()) {
      // XXX vaargs?
      if (itA->caller!=itB->caller || itA->kf!=itB->kf)
        return false;
      ++itA;
      ++itB;
    }
    if (itA!=stack().end() || itB!=b.stack().end())
      return false;
  }

  std::set< ref<Expr> > aConstraints(constraints().begin(), constraints().end());
  std::set< ref<Expr> > bConstraints(b.constraints().begin(),
                                     b.constraints().end());
  std::set< ref<Expr> > commonConstraints, aSuffix, bSuffix;
  std::set_intersection(aConstraints.begin(), aConstraints.end(),
                        bConstraints.begin(), bConstraints.end(),
                        std::inserter(commonConstraints, commonConstraints.begin()));
  std::set_difference(aConstraints.begin(), aConstraints.end(),
                      commonConstraints.begin(), commonConstraints.end(),
                      std::inserter(aSuffix, aSuffix.end()));
  std::set_difference(bConstraints.begin(), bConstraints.end(),
                      commonConstraints.begin(), commonConstraints.end(),
                      std::inserter(bSuffix, bSuffix.end()));
  if (DebugLogStateMerge) {
    std::cerr << "\tconstraint prefix: [";
    for (std::set< ref<Expr> >::iterator it = commonConstraints.begin(), 
           ie = commonConstraints.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
    std::cerr << "\tA suffix: [";
    for (std::set< ref<Expr> >::iterator it = aSuffix.begin(), 
           ie = aSuffix.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
    std::cerr << "\tB suffix: [";
    for (std::set< ref<Expr> >::iterator it = bSuffix.begin(), 
           ie = bSuffix.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
  }

  // We cannot merge if addresses would resolve differently in the
  // states. This means:
  // 
  // 1. Any objects created since the branch in either object must
  // have been free'd.
  //
  // 2. We cannot have free'd any pre-existing object in one state
  // and not the other

  if (DebugLogStateMerge) {
    std::cerr << "\tchecking object states\n";
    std::cerr << "A: " << addressSpace().objects << "\n";
    std::cerr << "B: " << b.addressSpace().objects << "\n";
  }
    
  std::set<const MemoryObject*> mutated;
  MemoryMap::iterator ai = addressSpace().objects.begin();
  MemoryMap::iterator bi = b.addressSpace().objects.begin();
  MemoryMap::iterator ae = addressSpace().objects.end();
  MemoryMap::iterator be = b.addressSpace().objects.end();
  for (; ai!=ae && bi!=be; ++ai, ++bi) {
    if (ai->first != bi->first) {
      if (DebugLogStateMerge) {
        if (ai->first < bi->first) {
          std::cerr << "\t\tB misses binding for: " << ai->first->id << "\n";
        } else {
          std::cerr << "\t\tA misses binding for: " << bi->first->id << "\n";
        }
      }
      return false;
    }
    if (ai->second != bi->second) {
      if (DebugLogStateMerge)
        std::cerr << "\t\tmutated: " << ai->first->id << "\n";
      mutated.insert(ai->first);
    }
  }
  if (ai!=ae || bi!=be) {
    if (DebugLogStateMerge)
      std::cerr << "\t\tmappings differ\n";
    return false;
  }
  
  // merge stack

  ref<Expr> inA = ConstantExpr::alloc(1, Expr::Bool);
  ref<Expr> inB = ConstantExpr::alloc(1, Expr::Bool);
  for (std::set< ref<Expr> >::iterator it = aSuffix.begin(), 
         ie = aSuffix.end(); it != ie; ++it)
    inA = AndExpr::create(inA, *it);
  for (std::set< ref<Expr> >::iterator it = bSuffix.begin(), 
         ie = bSuffix.end(); it != ie; ++it)
    inB = AndExpr::create(inB, *it);

  // XXX should we have a preference as to which predicate to use?
  // it seems like it can make a difference, even though logically
  // they must contradict each other and so inA => !inB

  std::vector<StackFrame>::iterator itA = stack().begin();
  std::vector<StackFrame>::const_iterator itB = b.stack().begin();
  for (; itA!=stack().end(); ++itA, ++itB) {
    StackFrame &af = *itA;
    const StackFrame &bf = *itB;
    for (unsigned i=0; i<af.kf->numRegisters; i++) {
      ref<Expr> &av = af.locals[i].value;
      const ref<Expr> &bv = bf.locals[i].value;
      if (av.isNull() || bv.isNull()) {
        // if one is null then by implication (we are at same pc)
        // we cannot reuse this local, so just ignore
      } else {
        av = SelectExpr::create(inA, av, bv);
      }
    }
  }

  for (std::set<const MemoryObject*>::iterator it = mutated.begin(), 
         ie = mutated.end(); it != ie; ++it) {
    const MemoryObject *mo = *it;
    const ObjectState *os = addressSpace().findObject(mo);
    const ObjectState *otherOS = b.addressSpace().findObject(mo);
    assert(os && !os->readOnly && 
           "objects mutated but not writable in merging state");
    assert(otherOS);

    ObjectState *wos = addressSpace().getWriteable(mo, os);
    for (unsigned i=0; i<mo->size; i++) {
      ref<Expr> av = wos->read8(i);
      ref<Expr> bv = otherOS->read8(i);
      wos->write(i, SelectExpr::create(inA, av, bv));
    }
  }

  constraints() = ConstraintManager();
  for (std::set< ref<Expr> >::iterator it = commonConstraints.begin(), 
         ie = commonConstraints.end(); it != ie; ++it)
    constraints().addConstraint(*it);
  constraints().addConstraint(OrExpr::create(inA, inB));

  return true;
}

/***/


void ExecutionState::dumpStack(std::ostream &out) const {
  unsigned idx = 0;
  const KInstruction *target = prevPC();
  for (ExecutionState::stack_ty::const_reverse_iterator
         it = stack().rbegin(), ie = stack().rend();
       it != ie; ++it) {
    const StackFrame &sf = *it;
    Function *f = sf.kf->function;
    const InstructionInfo &ii = *target->info;
    out << "\t#" << idx++ 
        << " " << std::setw(8) << std::setfill('0') << ii.assemblyLine
        << " in " << f->getNameStr() << " (";
    // Yawn, we could go up and print varargs if we wanted to.
    unsigned index = 0;
    for (Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end();
         ai != ae; ++ai) {
      if (ai!=f->arg_begin()) out << ", ";

      out << ai->getNameStr();
      // XXX should go through function
      ref<Expr> value = sf.locals[sf.kf->getArgRegister(index++)].value; 
      if (isa<ConstantExpr>(value))
        out << "=" << value;
    }
    out << ")";
    if (ii.file != "")
      out << " at " << ii.file << ":" << ii.line;
    out << "\n";
    target = sf.caller;
  }
}

}
