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
#include "klee/ForkTag.h"
#include "klee/AddressPool.h"
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
#include <sys/time.h>
#include <sys/mman.h>

using namespace llvm;
using namespace klee;

namespace { 
  cl::opt<bool>
  DebugLogStateMerge("debug-log-state-merge");
}

/***/

namespace klee {

ExecutionState::ExecutionState(Executor *_executor, KFunction *kf)
  : c9State(NULL),
    executor(_executor),
    fakeState(false),
    depth(0),
    multiplicity(1),
    forkDisabled(false),
    mergeDisabled(false),
    queryCost(0.), 
    weight(1),
    instsSinceCovNew(0),
    coveredNew(false),
    lastCoveredTime(sys::TimeValue::now()),
    ptreeNode(0),
    crtForkReason(KLEE_FORK_DEFAULT),
    crtSpecialFork(NULL),
    symbolicsHash(hashInit()),
    wlistCounter(1),
    preemptions(0),
    isDuplicate(false) {

  setupMain(kf);
  setupTime();
  setupAddressPool();
}

ExecutionState::ExecutionState(Executor *_executor, const std::vector<ref<Expr> > &assumptions)
  : c9State(NULL),
    executor(_executor),
    fakeState(true),
    queryCost(0.),
    lastCoveredTime(sys::TimeValue::now()),
    ptreeNode(0),
    globalConstraints(assumptions),
    wlistCounter(1),
    preemptions(0),
    isDuplicate(false) {

  setupMain(NULL);
}

void ExecutionState::setupTime() {
  stateTime = 1284138206L * 1000000L; // Yeah, ugly, but what else? :)
}

void ExecutionState::setupAddressPool() {
  void *startAddress = mmap((void*)addressPool.getStartAddress(), addressPool.getSize(),
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  assert(startAddress != MAP_FAILED);

  //CLOUD9_DEBUG("Address pool starts at " << startAddress <<
  //   " although it was requested at " << addressPool.getStartAddress());

  addressPool = AddressPool((uint64_t)startAddress, addressPool.getSize()); // Correct the address
}

void ExecutionState::setupMain(KFunction *kf) {
  Process mainProc = Process(2, 1);
  Thread mainThread = Thread(0, 2, kf);

  mainProc.threads.insert(mainThread.tuid);

  threads.insert(std::make_pair(mainThread.tuid, mainThread));
  processes.insert(std::make_pair(mainProc.pid, mainProc));

  crtThreadIt = threads.begin();
  crtProcessIt = processes.find(crtThreadIt->second.getPid());

  cowDomain.push_back(&crtProcessIt->second.addressSpace);

  crtProcessIt->second.addressSpace.cowDomain = &cowDomain;

  if(kf)
      setPC(kf->instructions);
}

void ExecutionState::setPC(const KInstIterator& newPC)
{
    crtThread().pc = newPC;
    if(crtThread().stack.empty()) {
        crtThread().execIndex = hashInit();
    } else {
        crtThread().execIndex = hashUpdate(
                crtThread().stack.back().execIndexStack.back().index,
                (uintptr_t) (KInstruction*) newPC);
    }
    crtThread().mergeIndex = hashUpdate(hashUpdate(crtThread().execIndex,
                                                   symbolicsHash),
                                        addressSpace().hash);
}

Thread& ExecutionState::createThread(thread_id_t tid, KFunction *kf) {
  Thread newThread = Thread(tid, crtProcess().pid, kf);
  crtProcess().threads.insert(newThread.tuid);

  threads.insert(std::make_pair(newThread.tuid, newThread));

  return threads.find(newThread.tuid)->second;
}

Process& ExecutionState::forkProcess(process_id_t pid) {
  for (processes_ty::iterator it = processes.begin(); it != processes.end(); it++) {
    it->second.addressSpace.cowKey++;
  }

  Process forked = Process(crtProcess());

  forked.pid = pid;
  forked.ppid = crtProcess().pid;
  forked.threads.clear();
  forked.children.clear();

  forked.forkPath.push_back(1); // Child
  crtProcess().forkPath.push_back(0); // Parent

  Thread forkedThread = Thread(crtThread());
  forkedThread.tuid = std::make_pair(0, forked.pid);

  forked.threads.insert(forkedThread.tuid);

  crtProcess().children.insert(forked.pid);

  threads.insert(std::make_pair(forkedThread.tuid, forkedThread));
  processes.insert(std::make_pair(forked.pid, forked));

  cowDomain.push_back(&processes.find(forked.pid)->second.addressSpace);
  processes.find(forked.pid)->second.addressSpace.cowDomain = &cowDomain;

  return processes.find(forked.pid)->second;
}

void ExecutionState::terminateThread(threads_ty::iterator thrIt) {
  CLOUD9_DEBUG("Terminating thread...");

  Process &proc = processes.find(thrIt->second.getPid())->second;

  assert(proc.threads.size() > 1);
  assert(thrIt != crtThreadIt); // We assume the scheduler found a new thread first
  assert(!thrIt->second.enabled);
  assert(thrIt->second.waitingList == 0);

  proc.threads.erase(thrIt->second.tuid);

  threads.erase(thrIt);

}

void ExecutionState::terminateProcess(processes_ty::iterator procIt) {
  CLOUD9_DEBUG("Terminating process " << procIt->second.pid);

  assert(processes.size() > 1);

  // Delete all process threads
  for (std::set<thread_uid_t>::iterator it = procIt->second.threads.begin();
      it != procIt->second.threads.end(); it++) {
    threads_ty::iterator thrIt = threads.find(*it);
    assert(thrIt != crtThreadIt);
    assert(!thrIt->second.enabled);
    assert(thrIt->second.waitingList == 0);

    threads.erase(thrIt);
  }

  // Update the process hierarchy
  if (procIt->second.ppid != 1) {
    Process &parent = processes.find(procIt->second.ppid)->second;

    parent.children.erase(procIt->second.pid);
  }

  if (procIt->second.children.size() > 0) {
    // Reassign the children to process 1
    for (std::set<process_id_t>::iterator it = procIt->second.children.begin();
        it != procIt->second.children.end(); it++) {
      processes.find(*it)->second.ppid = 1;
    }
  }

  // Update the state COW domain
  AddressSpace::cow_domain_t::iterator it =
      std::find(cowDomain.begin(), cowDomain.end(), &procIt->second.addressSpace);
  assert(it != cowDomain.end());
  cowDomain.erase(it);

  assert(procIt != crtProcessIt);

  processes.erase(procIt);
}

void ExecutionState::sleepThread(wlist_id_t wlist) {
  assert(crtThread().enabled);
  assert(wlist > 0);

  crtThread().enabled = false;
  crtThread().waitingList = wlist;

  std::set<thread_uid_t> &wl = waitingLists[wlist];

  wl.insert(crtThread().tuid);
}

void ExecutionState::notifyOne(wlist_id_t wlist, thread_uid_t tuid) {
  assert(wlist > 0);

  std::set<thread_uid_t> &wl = waitingLists[wlist];

  if (wl.erase(tuid) != 1) {
    assert(0 && "thread was not waiting");
  }

  Thread &thread = threads.find(tuid)->second;
  assert(!thread.enabled);
  thread.enabled = true;
  thread.waitingList = 0;

  if (wl.size() == 0)
    waitingLists.erase(wlist);
}

void ExecutionState::notifyAll(wlist_id_t wlist) {
  assert(wlist > 0);

  std::set<thread_uid_t> &wl = waitingLists[wlist];

  if (wl.size() > 0) {
    for (std::set<thread_uid_t>::iterator it = wl.begin(); it != wl.end(); it++) {
      Thread &thread = threads.find(*it)->second;
      thread.enabled = true;
      thread.waitingList = 0;
    }

    wl.clear();
  }

  waitingLists.erase(wlist);
}

ExecutionState::~ExecutionState() {
  for (threads_ty::iterator it = threads.begin(); it != threads.end(); it++) {
    Thread &t = it->second;
    while (!t.stack.empty())
      popFrame(t);
  }
}

ExecutionState *ExecutionState::branch(bool copy) {
  if (!copy)
    depth++;

  for (processes_ty::iterator it = processes.begin(); it != processes.end(); it++) {
    it->second.addressSpace.cowKey++;
  }

  ExecutionState *falseState = new ExecutionState(*this);

  falseState->duplicates.clear();

  if (!copy) {
    falseState->coveredNew = false;
    falseState->coveredLines.clear();
  }

  falseState->c9State = NULL;

  falseState->crtThreadIt = falseState->threads.find(crtThreadIt->second.tuid);
  falseState->crtProcessIt = falseState->processes.find(crtProcessIt->second.pid);

  falseState->cowDomain.clear();

  // Rebuilding the COW domain...

  for (processes_ty::iterator it = falseState->processes.begin();
      it != falseState->processes.end(); it++) {
    falseState->cowDomain.push_back(&it->second.addressSpace);
  }

  for (processes_ty::iterator it = falseState->processes.begin();
      it != falseState->processes.end(); it++) {
    it->second.addressSpace.cowDomain = &falseState->cowDomain;
  }

  if (!copy) {
    weight *= .5;
    falseState->weight -= weight;
  }

  return falseState;
}

  /*
void ExecutionState::pushFrame(KInstIterator caller, KFunction *kf) {
  stack.push_back(StackFrame(caller, getExecIndex(), kf));
}

void ExecutionState::popFrame() {
  StackFrame &sf = stack.back();
  for (std::vector<const MemoryObject*>::iterator it = sf.allocas.begin(),
         ie = sf.allocas.end(); it != ie; ++it)
    addressSpace.unbindObject(*it);
  stack.pop_back();
  }*/


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

ExecutionState* ExecutionState::merge(const ExecutionState &b, bool copy) {
  if (DebugLogStateMerge)
    std::cerr << "-- attempting merge of A:" 
               << this << " with B:" << &b << "--\n";

  if (pc() != b.pc()) {
    if (DebugLogStateMerge)
      std::cerr << "---- merge failed: program counters are different\n";
    return NULL;
  }

  // XXX is it even possible for these to differ? does it matter? probably
  // implies difference in object states?
  if (symbolics!=b.symbolics) {
    if (DebugLogStateMerge)
      std::cerr << "---- merge failed: symbolics sets are different\n";
    return NULL;
  }

  // We cannot merge if addresses would resolve differently in the
  // states. This means:
  //
  // 1. Any objects created since the branch in either object must
  // have been free'd.
  //
  // 2. We cannot have free'd any pre-existing object in one state
  // and not the other

  std::set<const MemoryObject*> mutated;

  {
    MemoryMap::iterator ai = addressSpace().objects.begin();
    MemoryMap::iterator bi = b.addressSpace().objects.begin();
    MemoryMap::iterator ae = addressSpace().objects.end();
    MemoryMap::iterator be = b.addressSpace().objects.end();
    for (; ai!=ae && bi!=be; ++ai, ++bi) {
      if (ai->first != bi->first) {
        if (DebugLogStateMerge) {
          if (ai->first < bi->first) {
            std::cerr << "\t\tB misses binding for: " << ai->first->id
                      << " (" << ai->first->name << ")\n";
          } else {
            std::cerr << "\t\tA misses binding for: " << bi->first->id
                      << " (" << bi->first->name << ")\n";
          }
        }
        if (DebugLogStateMerge)
          std::cerr << "---- merge failed: mappings are different\n";
        return NULL;
      }
      if (ai->second != bi->second) {
        //if (DebugLogStateMerge)
        //  std::cerr << "\t\tmutated: " << ai->first->id << "\n";
        mutated.insert(ai->first);
  #if 0
        // XXX: for now we refuse to merge states that has different concrete
        // values in the memory. This is wrong, but otherwise we are ending up
        // merging different iterations of the same loop
        const MemoryObject* mi = ai->first;
        const ObjectState* as = ai->second;
        const ObjectState* bs = bi->second;
        for(unsigned i=0; i<mi->size; ++i) {
          ref<Expr> av = as->read8(i);
          ref<Expr> bv = bs->read8(i);
          if(!av.isNull() && !bv.isNull() && av != bv)
            if(av->getKind() == Expr::Constant && bv->getKind() == Expr::Constant)
              return NULL;
        }
  #endif
      }
    }
    if (ai!=ae || bi!=be) {
      if (DebugLogStateMerge)
        std::cerr << "\t\tmappings differ\n";
      if (DebugLogStateMerge)
        std::cerr << "---- merge failed: mappings are different\n";
      return NULL;
    }
  }


  {

    std::vector<StackFrame>::const_iterator itA = stack().begin();
    std::vector<StackFrame>::const_iterator itB = b.stack().begin();
    std::vector<StackFrame>::const_iterator itAE = stack().end();
    std::vector<StackFrame>::const_iterator itBE = b.stack().end();
    while (itA!=itAE && itB!=itBE) {
      // XXX vaargs?
      if (itA->caller!=itB->caller || itA->kf!=itB->kf) {
        if (DebugLogStateMerge)
          std::cerr << "---- merge failed: call stacks are different\n";
        return NULL;
      }

#if 0
      // XXX: for now we refuse to merge states that has different concrete
      // values on the stack. This is wrong, but otherwise we are ending up
      // merging different iterations of the same loop
      //
      // XXX: try the same for memory objects
  
      for (unsigned i=0; i<itA->kf->numRegisters; i++) {
        const ref<Expr> &av = itA->locals[i].value;
        const ref<Expr> &bv = itB->locals[i].value;
        if(!av.isNull() && !bv.isNull() && av != bv)
          if(av->getKind() == Expr::Constant && bv->getKind() == Expr::Constant)
            return NULL;
      }
#endif

      ++itA;
      ++itB;
    }

    if (itA!=itAE || itB!=itBE) {
      if (DebugLogStateMerge)
        std::cerr << "---- merge failed: call stacks are different\n";
      return NULL;
    }
  }

  // Everything seems OK, we are going to merge the states

  ExecutionState *aPtr = this;
  if (copy)
    aPtr = this->branch(true);
  ExecutionState &a = *aPtr;

  std::set< ref<Expr> > aConstraints(a.constraints().begin(),
                                     a.constraints().end());
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
    std::cerr << "\tA constraint suffix: [";
    for (std::set< ref<Expr> >::iterator it = aSuffix.begin(), 
           ie = aSuffix.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
    std::cerr << "\tB constraint suffix: [";
    for (std::set< ref<Expr> >::iterator it = bSuffix.begin(), 
           ie = bSuffix.end(); it != ie; ++it)
      std::cerr << *it << ", ";
    std::cerr << "]\n";
  }

  ref<Expr> inA = ConstantExpr::alloc(1, Expr::Bool);
  ref<Expr> inB = ConstantExpr::alloc(1, Expr::Bool);
  for (std::set< ref<Expr> >::iterator it = aSuffix.begin(), 
         ie = aSuffix.end(); it != ie; ++it)
    inA = AndExpr::create(inA, *it);
  for (std::set< ref<Expr> >::iterator it = bSuffix.begin(), 
         ie = bSuffix.end(); it != ie; ++it)
    inB = AndExpr::create(inB, *it);

  bool useInA = (aSuffix.size() < bSuffix.size());

  // XXX should we have a preference as to which predicate to use?
  // it seems like it can make a difference, even though logically
  // they must contradict each other and so inA => !inB

  // merge stack

  std::vector<StackFrame>::iterator itA = a.stack().begin();
  std::vector<StackFrame>::iterator itAE = a.stack().end();
  std::vector<StackFrame>::const_iterator itB = b.stack().begin();
  int stackDifference = 0, stackObjects = 0;

  for (; itA!=itAE; ++itA, ++itB) {
    StackFrame &af = *itA;
    const StackFrame &bf = *itB;
    stackObjects += af.kf->numRegisters;
    for (unsigned i=0; i<af.kf->numRegisters; i++) {
      ref<Expr> &av = af.locals[i].value;
      const ref<Expr> &bv = bf.locals[i].value;
      if (av.isNull() || bv.isNull()) {
        // if one is null then by implication (we are at same pc)
        // we cannot reuse this local, so just ignore
      } else {
        if(av != bv) {
            av = useInA ? SelectExpr::create(inA, av, bv)
                        : SelectExpr::create(inB, bv, av);
            ++stackDifference;
        }
      }
    }
  }

  if(DebugLogStateMerge) {
      std::cerr << "\t\tfound " << stackDifference
                << " (of " << stackObjects
                << ") different values on stack\n";
  }

  int memDifference = 0, memObjects = 0;
  for (std::set<const MemoryObject*>::iterator it = mutated.begin(), 
         ie = mutated.end(); it != ie; ++it) {
    const MemoryObject *mo = *it;
    const ObjectState *os = a.addressSpace().findObject(mo);
    const ObjectState *otherOS = b.addressSpace().findObject(mo);
    assert(os && !os->readOnly && 
           "objects mutated but not writable in merging state");
    assert(otherOS);

    memObjects += mo->size;
    ObjectState *wos = a.addressSpace().getWriteable(mo, os);

    for (unsigned i=0; i<mo->size; i++) {
      ref<Expr> av = wos->read8(i);
      ref<Expr> bv = otherOS->read8(i);
      if(av != bv) {
          memDifference += 1;
          wos->write(i, useInA ? SelectExpr::create(inA, av, bv)
                               : SelectExpr::create(inB, bv, av));
      }
    }
  }

  if(DebugLogStateMerge) {
      std::cerr << "\t\tfound " << memDifference
                << " (of " << memObjects
                << ") different values in memory\n";
  }

  ConstraintManager::merge_conditions_ty aMergeConditions;
  aMergeConditions.swap(a.constraints().mergeConditions);

  a.constraints() = ConstraintManager();

  for (std::set< ref<Expr> >::iterator it = commonConstraints.begin(), 
         ie = commonConstraints.end(); it != ie; ++it)
    a.constraints().addConstraint(*it);
  a.constraints().addConstraint(OrExpr::create(inA, inB));

  a.constraints().mergeConditions.swap(aMergeConditions);
  a.constraints().mergeConditions.insert(b.constraints().mergeConditions.begin(),
                                         b.constraints().mergeConditions.end());
  a.constraints().mergeConditions.insert(useInA ? inA : inB);

  a.queryCost += b.queryCost;
  a.weight += b.weight;
  a.coveredNew |= b.coveredNew;
  a.multiplicity += b.multiplicity;
  if(a.instsSinceCovNew > b.instsSinceCovNew)
      a.instsSinceCovNew = b.instsSinceCovNew;
  for(std::map<const std::string*, std::set<unsigned> >::const_iterator
          it = b.coveredLines.begin(), ie = b.coveredLines.end(); it != ie; ++it) {
      a.coveredLines[it->first].insert(it->second.begin(), it->second.end());
  }

  if (DebugLogStateMerge)
    std::cerr << "---- merged successfully\n";

  return &a;
}

/***/

StackTrace ExecutionState::getStackTrace() const {
  StackTrace result;

  const KInstruction *target = prevPC();

  for (ExecutionState::stack_ty::const_reverse_iterator
         it = stack().rbegin(), ie = stack().rend();
       it != ie; ++it) {

    const StackFrame &sf = *it;

    StackTrace::position_t position = std::make_pair(sf.kf, target);
    std::vector<ref<Expr> > arguments;

    Function *f = sf.kf->function;
    unsigned index = 0;
    for (Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end();
         ai != ae; ++ai) {

      ref<Expr> value = sf.locals[sf.kf->getArgRegister(index++)].value;
      arguments.push_back(value);
    }

    result.contents.push_back(std::make_pair(position, arguments));

    target = sf.caller;
  }

  return result;
}

}
