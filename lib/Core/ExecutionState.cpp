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

// Update the index with new value.
uint32_t LoopExecIndex::newIndex(void* updateID) const {
  uint32_t newIndex = index;
  const char* buf = reinterpret_cast<const char*>(&updateID);
  for (unsigned i = 0; i < sizeof(void*); ++i) {
    newIndex ^= static_cast<size_t>(buf[i]);
    newIndex *= static_cast<size_t>(16777619UL);
  }
  return newIndex;
}

/***/

StackFrame::StackFrame(KInstIterator _caller, uint32_t _callerExecIndex,
                       KFunction *_kf)
  : caller(_caller), kf(_kf), callPathNode(0), 
    minDistToUncoveredOnReturn(0), varargs(0),
    execIndexStack(1) {

  execIndexStack[0].loopID = uint32_t(-1);
  execIndexStack[0].index = _callerExecIndex;
  execIndexStack[0].updateIndex(_kf);

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

StackFrame::~StackFrame() {
  delete[] locals; 
}

/***/

ExecutionState::ExecutionState(KFunction *kf) 
  : fakeState(false),
    underConstrained(false),
    depth(0),
    pc(kf->instructions),
    prevPC(pc),
    queryCost(0.), 
    weight(1),
    instsSinceCovNew(0),
    coveredNew(false),
    forkDisabled(false),
    ptreeNode(0) {
  pushFrame(0, kf);
}

ExecutionState::ExecutionState(const std::vector<ref<Expr> > &assumptions) 
  : fakeState(true),
    underConstrained(false),
    constraints(assumptions),
    queryCost(0.),
    ptreeNode(0) {
}

ExecutionState::~ExecutionState() {
  while (!stack.empty()) popFrame();
}

ExecutionState *ExecutionState::branch() {
  depth++;

  ExecutionState *falseState = new ExecutionState(*this);
  falseState->coveredNew = false;
  falseState->coveredLines.clear();

  weight *= .5;
  falseState->weight -= weight;

  return falseState;
}

void ExecutionState::pushFrame(KInstIterator caller, KFunction *kf) {
  stack.push_back(StackFrame(caller, getExecIndex(), kf));
}

void ExecutionState::popFrame() {
  StackFrame &sf = stack.back();
  for (std::vector<const MemoryObject*>::iterator it = sf.allocas.begin(), 
         ie = sf.allocas.end(); it != ie; ++it)
    addressSpace.unbindObject(*it);
  stack.pop_back();
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

std::ostream &klee::operator<<(std::ostream &os, const MemoryMap &mm) {
  os << "{";
  MemoryMap::iterator it = mm.begin();
  MemoryMap::iterator ie = mm.end();
  if (it!=ie) {
    os << "MO" << it->first->id << ":" << it->second;
    for (++it; it!=ie; ++it)
      os << ", MO" << it->first->id << ":" << it->second;
  }
  os << "}";
  return os;
}

bool ExecutionState::merge(const ExecutionState &b) {
  if (DebugLogStateMerge)
    std::cerr << "-- attempting merge of A:" 
               << this << " with B:" << &b << "--\n";

  if (pc != b.pc) {
    if (DebugLogStateMerge)
      std::cerr << "---- merge failed: program counters are different\n";
    return false;
  }

  // XXX is it even possible for these to differ? does it matter? probably
  // implies difference in object states?
  if (symbolics!=b.symbolics) {
    if (DebugLogStateMerge)
      std::cerr << "---- merge failed: symbolics sets are different\n";
    return false;
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
    MemoryMap::iterator ai = addressSpace.objects.begin();
    MemoryMap::iterator bi = b.addressSpace.objects.begin();
    MemoryMap::iterator ae = addressSpace.objects.end();
    MemoryMap::iterator be = b.addressSpace.objects.end();
    for (; ai!=ae && bi!=be; ++ai, ++bi) {
      if (ai->first != bi->first) {
        if (DebugLogStateMerge) {
          if (ai->first < bi->first) {
            std::cerr << "\t\tB misses binding for: " << ai->first->id << "\n";
          } else {
            std::cerr << "\t\tA misses binding for: " << bi->first->id << "\n";
          }
        }
        if (DebugLogStateMerge)
          std::cerr << "---- merge failed: mappings are different\n";
        return false;
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
              return false;
        }
  #endif
      }
    }
    if (ai!=ae || bi!=be) {
      if (DebugLogStateMerge)
        std::cerr << "\t\tmappings differ\n";
      if (DebugLogStateMerge)
        std::cerr << "---- merge failed: mappings are different\n";
      return false;
    }
  }


  {
    std::vector<StackFrame>::const_iterator itA = stack.begin();
    std::vector<StackFrame>::const_iterator itB = b.stack.begin();
    std::vector<StackFrame>::const_iterator itAE = stack.end();
    std::vector<StackFrame>::const_iterator itBE = b.stack.end();
    while (itA!=itAE && itB!=itBE) {
      // XXX vaargs?
      if (itA->caller!=itB->caller || itA->kf!=itB->kf) {
        if (DebugLogStateMerge)
          std::cerr << "---- merge failed: call stacks are different\n";
        return false;
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
            return false;
      }
#endif

      ++itA;
      ++itB;
    }
    if (itA!=itAE || itB!=itBE) {
      if (DebugLogStateMerge)
        std::cerr << "---- merge failed: call stacks are different\n";
      return false;
    }
  }

  std::set< ref<Expr> > aConstraints(constraints.begin(), constraints.end());
  std::set< ref<Expr> > bConstraints(b.constraints.begin(), 
                                     b.constraints.end());
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

  std::vector<StackFrame>::iterator itA = stack.begin();
  std::vector<StackFrame>::iterator itAE = stack.end();
  std::vector<StackFrame>::const_iterator itB = b.stack.begin();
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
            av = SelectExpr::create(inA, av, bv);
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
    const ObjectState *os = addressSpace.findObject(mo);
    const ObjectState *otherOS = b.addressSpace.findObject(mo);
    assert(os && !os->readOnly && 
           "objects mutated but not writable in merging state");
    assert(otherOS);

    memObjects += mo->size;
    ObjectState *wos = addressSpace.getWriteable(mo, os);
    for (unsigned i=0; i<mo->size; i++) {
      ref<Expr> av = wos->read8(i);
      ref<Expr> bv = otherOS->read8(i);
      if(av != bv) {
          memDifference += 1;
          wos->write(i, SelectExpr::create(inA, av, bv));
      }
    }
  }

  if(DebugLogStateMerge) {
      std::cerr << "\t\tfound " << memDifference
                << " (of " << memObjects
                << ") different values in memory\n";
  }

  constraints = ConstraintManager();
  for (std::set< ref<Expr> >::iterator it = commonConstraints.begin(), 
         ie = commonConstraints.end(); it != ie; ++it)
    constraints.addConstraint(*it);
  constraints.addConstraint(OrExpr::create(inA, inB));

  queryCost += b.queryCost;
  weight += b.weight;
  coveredNew |= b.coveredNew;
  if(instsSinceCovNew > b.instsSinceCovNew)
      instsSinceCovNew = b.instsSinceCovNew;
  for(std::map<const std::string*, std::set<unsigned> >::const_iterator
          it = b.coveredLines.begin(), ie = b.coveredLines.end(); it != ie; ++it) {
      coveredLines[it->first].insert(it->second.begin(), it->second.end());
  }

  if (DebugLogStateMerge)
    std::cerr << "---- merged successfully\n";

  return true;
}

uint32_t ExecutionState::getExecIndex() const {
  //LoopExecIndex e = {0, 2166136261UL};
  //return e.newIndex((void*) (KInstruction*) pc);
  if(stack.empty())
    return 2166136261UL;
  else
    return stack.back().execIndexStack.back().newIndex(
                                  (void*) (KInstruction*) pc);
}

void ExecutionState::dumpStack(std::ostream &out) const {
  unsigned idx = 0;
  const KInstruction *target = prevPC;
  for (ExecutionState::stack_ty::const_reverse_iterator
         it = stack.rbegin(), ie = stack.rend();
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
