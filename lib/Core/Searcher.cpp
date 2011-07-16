//===-- Searcher.cpp ------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Common.h"

#include "klee/Searcher.h"

#include "CoreStats.h"
#include "klee/Executor.h"
#include "PTree.h"
#include "StatsTracker.h"

#include "klee/ExecutionState.h"
#include "klee/Statistics.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/ADT/DiscretePDF.h"
#include "klee/Internal/ADT/RNG.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "klee/Internal/System/Time.h"

#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"

#include <cassert>
#include <fstream>
#include <climits>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

using namespace klee;
using namespace llvm;

namespace {
  cl::opt<bool>
  DebugLogMerge("debug-log-merge");

  cl::opt<unsigned>
  MaxStateMultiplicity("max-state-multiplicity",
            cl::desc("Maximum number of states merged into one"),
            cl::init(0));

  cl::opt<bool>
  DebugCheckpointUpdates("debug-checkpoint-updates",
       cl::desc("Displays the number of updates received vs. updates forwarded when states are checkpointed"),
       cl::init(false));

  cl::opt<bool>
  DebugMaxInstrCountDelta("debug-maxinstr-delta",
      cl::desc("Displays rejected forwarding opportunities due to exceeded maximum instruction count delta"),
      cl::init(false));

  cl::opt<unsigned>
  MaxInstrDifference("max-inst-difference",
      cl::desc("Maximum difference between instruction counters for forwarding states"),
      cl::init(0));
}

namespace klee {
  extern RNG theRNG;
}

Searcher::~Searcher() {
}

///

ExecutionState &DFSSearcher::selectState() {
  return *states.back();
}

void DFSSearcher::update(ExecutionState *current,
                         const std::set<ExecutionState*> &addedStates,
                         const std::set<ExecutionState*> &removedStates) {
  states.insert(states.end(),
                addedStates.begin(),
                addedStates.end());
  for (std::set<ExecutionState*>::const_iterator it = removedStates.begin(),
         ie = removedStates.end(); it != ie; ++it) {
    ExecutionState *es = *it;
    if (es == states.back()) {
      states.pop_back();
    } else {
      bool ok = false;

      for (std::vector<ExecutionState*>::iterator it = states.begin(),
             ie = states.end(); it != ie; ++it) {
        if (es==*it) {
          states.erase(it);
          ok = true;
          break;
        }
      }

      assert(ok && "invalid state removed");
    }
  }
}

///

ExecutionState &RandomSearcher::selectState() {
  return *states[theRNG.getInt32()%states.size()];
}

void RandomSearcher::update(ExecutionState *current,
                            const std::set<ExecutionState*> &addedStates,
                            const std::set<ExecutionState*> &removedStates) {
  states.insert(states.end(),
                addedStates.begin(),
                addedStates.end());
  for (std::set<ExecutionState*>::const_iterator it = removedStates.begin(),
         ie = removedStates.end(); it != ie; ++it) {
    ExecutionState *es = *it;
    bool ok = false;

    for (std::vector<ExecutionState*>::iterator it1 = states.begin(),
           ie1 = states.end(); it1 != ie1; ++it1) {
      if (es==*it1) {
        states.erase(it1);
        ok = true;
        break;
      }
    }
    
    assert(ok && "invalid state removed");
  }
}

///

WeightedRandomSearcher::WeightedRandomSearcher(Executor &_executor,
                                               WeightType _type) 
  : executor(_executor),
    states(new DiscretePDF<ExecutionState*>()),
    type(_type) {
  switch(type) {
  case Depth: 
    updateWeights = false;
    break;
  case InstCount:
  case CPInstCount:
  case QueryCost:
  case MinDistToUncovered:
  case CoveringNew:
    updateWeights = true;
    break;
  default:
    assert(0 && "invalid weight type");
  }
}

WeightedRandomSearcher::~WeightedRandomSearcher() {
  delete states;
}

ExecutionState &WeightedRandomSearcher::selectState() {
  return *states->choose(theRNG.getDoubleL());
}

double WeightedRandomSearcher::getWeight(ExecutionState *es) {
  switch(type) {
  default:
  case Depth: 
    return es->weight;
  case InstCount: {
    uint64_t count = theStatisticManager->getIndexedValue(stats::instructions,
                                                          es->pc()->info->id);
    double inv = 1. / std::max((uint64_t) 1, count);
    return inv * inv;
  }
  case CPInstCount: {
    StackFrame &sf = es->stack().back();
    uint64_t count = sf.callPathNode->statistics.getValue(stats::instructions);
    double inv = 1. / std::max((uint64_t) 1, count);
    return inv;
  }
  case QueryCost:
    return (es->queryCost < .1) ? 1. : 1./es->queryCost;
  case CoveringNew:
  case MinDistToUncovered: {
    uint64_t md2u = computeMinDistToUncovered(es->pc(),
                                              es->stack().back().minDistToUncoveredOnReturn);

    double invMD2U = 1. / (md2u ? md2u : 10000);
    if (type==CoveringNew) {
      double invCovNew = 0.;
      if (es->instsSinceCovNew)
        invCovNew = 1. / std::max(1, (int) es->instsSinceCovNew - 1000);
      return (invCovNew * invCovNew + invMD2U * invMD2U);
    } else {
      return invMD2U * invMD2U;
    }
  }
  }
}

void WeightedRandomSearcher::update(ExecutionState *current,
                                    const std::set<ExecutionState*> &addedStates,
                                    const std::set<ExecutionState*> &removedStates) {
  if (current && updateWeights && !removedStates.count(current))
    states->update(current, getWeight(current));
  
  for (std::set<ExecutionState*>::const_iterator it = addedStates.begin(),
         ie = addedStates.end(); it != ie; ++it) {
    ExecutionState *es = *it;
    states->insert(es, getWeight(es));
  }

  for (std::set<ExecutionState*>::const_iterator it = removedStates.begin(),
         ie = removedStates.end(); it != ie; ++it) {
    states->remove(*it);
  }
}

bool WeightedRandomSearcher::empty() { 
  return states->empty(); 
}

///

RandomPathSearcher::RandomPathSearcher(Executor &_executor)
  : executor(_executor) {
}

RandomPathSearcher::~RandomPathSearcher() {
}

ExecutionState &RandomPathSearcher::selectState() {
  unsigned flips=0, bits=0;
  PTree::Node *n = executor.processTree->root;
  assert(n->active);
  
  while (!n->data) {
    if (!n->left || !n->left->active) {
      n = n->right;
    } else if (!n->right || !n->right->active) {
      n = n->left;
    } else {
      if (bits==0) {
        flips = theRNG.getInt32();
        bits = 32;
      }
      --bits;
      n = (flips&(1<<bits)) ? n->left : n->right;
    }
    assert(n && n->active);
  }

  return *n->data;
}

void RandomPathSearcher::update(ExecutionState *current,
                                const std::set<ExecutionState*> &addedStates,
                                const std::set<ExecutionState*> &removedStates) {
}

bool RandomPathSearcher::empty() { 
  return executor.states.empty(); 
}

///

BumpMergingSearcher::BumpMergingSearcher(Executor &_executor, Searcher *_baseSearcher) 
  : executor(_executor),
    baseSearcher(_baseSearcher),
    mergeFunction(executor.kmodule->kleeMergeFn) {
}

BumpMergingSearcher::~BumpMergingSearcher() {
  delete baseSearcher;
}

///

Instruction *BumpMergingSearcher::getMergePoint(ExecutionState &es) {  
  if (mergeFunction) {
    Instruction *i = es.pc()->inst;

    if (i->getOpcode()==Instruction::Call) {
      CallSite cs(cast<CallInst>(i));
      if (mergeFunction==cs.getCalledFunction())
        return i;
    }
  }

  return 0;
}

ExecutionState &BumpMergingSearcher::selectState() {
entry:
  // out of base states, pick one to pop
  if (baseSearcher->empty()) {
    std::map<llvm::Instruction*, ExecutionState*>::iterator it = 
      statesAtMerge.begin();
    ExecutionState *es = it->second;
    statesAtMerge.erase(it);
    es->setPC(es->pc().next());

    baseSearcher->addState(es);
  }

  ExecutionState &es = baseSearcher->selectState();

  if (Instruction *mp = getMergePoint(es)) {
    std::map<llvm::Instruction*, ExecutionState*>::iterator it = 
      statesAtMerge.find(mp);

    baseSearcher->removeState(&es);

    if (it==statesAtMerge.end()) {
      statesAtMerge.insert(std::make_pair(mp, &es));
    } else {
      ExecutionState *mergeWith = it->second;
      if (executor.merge(*mergeWith, es)) {
        // hack, because we are terminating the state we need to let
        // the baseSearcher know about it again
        baseSearcher->addState(&es);
        executor.terminateState(es, true);
      } else {
        it->second = &es; // the bump
        mergeWith->setPC(mergeWith->pc().next());

        baseSearcher->addState(mergeWith);
      }
    }

    goto entry;
  } else {
    return es;
  }
}

void BumpMergingSearcher::update(ExecutionState *current,
                                 const std::set<ExecutionState*> &addedStates,
                                 const std::set<ExecutionState*> &removedStates) {
  baseSearcher->update(current, addedStates, removedStates);
}

///

MergingSearcher::MergingSearcher(Executor &_executor, Searcher *_baseSearcher) 
  : executor(_executor),
    baseSearcher(_baseSearcher),
    mergeFunction(executor.kmodule->kleeMergeFn) {
}

MergingSearcher::~MergingSearcher() {
  delete baseSearcher;
}

///

Instruction *MergingSearcher::getMergePoint(ExecutionState &es) {
  if (mergeFunction) {
    Instruction *i = es.pc()->inst;

    if (i->getOpcode()==Instruction::Call) {
      CallSite cs(cast<CallInst>(i));
      if (mergeFunction==cs.getCalledFunction())
        return i;
    }
  }

  return 0;
}

ExecutionState &MergingSearcher::selectState() {
  while (!baseSearcher->empty()) {
    ExecutionState &es = baseSearcher->selectState();
    if (getMergePoint(es)) {
      baseSearcher->removeState(&es, &es);
      statesAtMerge.insert(&es);
    } else {
      return es;
    }
  }
  
  // build map of merge point -> state list
  std::map<Instruction*, std::vector<ExecutionState*> > merges;
  for (std::set<ExecutionState*>::const_iterator it = statesAtMerge.begin(),
         ie = statesAtMerge.end(); it != ie; ++it) {
    ExecutionState &state = **it;
    Instruction *mp = getMergePoint(state);
    
    merges[mp].push_back(&state);
  }
  
  if (DebugLogMerge)
    std::cerr << "-- all at merge --\n";
  for (std::map<Instruction*, std::vector<ExecutionState*> >::iterator
         it = merges.begin(), ie = merges.end(); it != ie; ++it) {
    if (DebugLogMerge) {
      std::cerr << "\tmerge: " << it->first << " [";
      for (std::vector<ExecutionState*>::iterator it2 = it->second.begin(),
             ie2 = it->second.end(); it2 != ie2; ++it2) {
        ExecutionState *state = *it2;
        std::cerr << state << ", ";
      }
      std::cerr << "]\n";
    }

    // merge states
    std::set<ExecutionState*> toMerge(it->second.begin(), it->second.end());
    while (!toMerge.empty()) {
      ExecutionState *base = *toMerge.begin();
      toMerge.erase(toMerge.begin());
      
      std::set<ExecutionState*> toErase;
      for (std::set<ExecutionState*>::iterator it = toMerge.begin(),
             ie = toMerge.end(); it != ie; ++it) {
        ExecutionState *mergeWith = *it;
        
        if (executor.merge(*base, *mergeWith)) {
          toErase.insert(mergeWith);
        }
      }
      if (DebugLogMerge && !toErase.empty()) {
        std::cerr << "\t\tmerged: " << base << " with [";
        for (std::set<ExecutionState*>::iterator it = toErase.begin(),
               ie = toErase.end(); it != ie; ++it) {
          if (it!=toErase.begin()) std::cerr << ", ";
          std::cerr << *it;
        }
        std::cerr << "]\n";
      }
      for (std::set<ExecutionState*>::iterator it = toErase.begin(),
             ie = toErase.end(); it != ie; ++it) {
        std::set<ExecutionState*>::iterator it2 = toMerge.find(*it);
        assert(it2!=toMerge.end());
        executor.terminateState(**it, true);
        toMerge.erase(it2);
      }

      // step past merge and toss base back in pool
      statesAtMerge.erase(statesAtMerge.find(base));
      base->setPC(base->pc().next());
      baseSearcher->addState(base);
    }  
  }
  
  if (DebugLogMerge)
    std::cerr << "-- merge complete, continuing --\n";
  
  return selectState();
}

void MergingSearcher::update(ExecutionState *current,
                             const std::set<ExecutionState*> &addedStates,
                             const std::set<ExecutionState*> &removedStates) {
  if (!removedStates.empty()) {
    std::set<ExecutionState *> alt = removedStates;
    for (std::set<ExecutionState*>::const_iterator it = removedStates.begin(),
           ie = removedStates.end(); it != ie; ++it) {
      ExecutionState *es = *it;
      std::set<ExecutionState*>::const_iterator it2 = statesAtMerge.find(es);
      if (it2 != statesAtMerge.end()) {
        statesAtMerge.erase(it2);
        alt.erase(alt.find(es));
      }
    }    
    baseSearcher->update(current, addedStates, alt);
  } else {
    baseSearcher->update(current, addedStates, removedStates);
  }
}

///

LazyMergingSearcher::LazyMergingSearcher(Executor &_executor, Searcher *_baseSearcher) 
  : executor(_executor),
    baseSearcher(_baseSearcher) {
}

LazyMergingSearcher::~LazyMergingSearcher() {
  delete baseSearcher;
}

///

inline bool LazyMergingSearcher::canFastForwardState(const ExecutionState* state) const {
  if (state->mergeDisabled())
    return false;

  if (MaxStateMultiplicity && state->multiplicity >= MaxStateMultiplicity)
    return false;

  StatesTrace::const_iterator it = statesTrace.find(state->getMergeIndex());

  if (it == statesTrace.end())
    return false;

  // This loop could have at most two iterations
  for (StatePosMap::const_iterator it1 = it->second->begin(),
                      ie1 = it->second->end(); it1 != ie1; ++it1) {
    if (it1->first != state) {
      if (MaxInstrDifference) {
        if (it1->first->instsTotal - it1->second <= MaxInstrDifference)
          return true;
        CLOUD9_DEBUG("Forward opportunity declined due to large instruction count difference (" <<
            it1->first->instsTotal << " and " << it1->second << ")");
        /*
        if (it1->second >= state->instsTotal && it1->second - state->instsTotal <= MaxInstrDifference)
          return true;
        if (it1->second < state->instsTotal && state->instsTotal - it1->second <= MaxInstrDifference)
          return true;
        CLOUD9_DEBUG("Forward opportunity declined due to large instruction count difference (" <<
            it1->second << " and " << state->instsTotal << ")");
          */
      } else {
        return true;
      }
    }
  }

  return false;
}

ExecutionState &LazyMergingSearcher::selectState() {
  ExecutionState *state = NULL, *merged = NULL;
  while (!statesToForward.empty()) {
    // TODO: do not fast-forward state if there are other
    // states that could be merged with state first (i.e., select
    // smartly what state to fast-forward first).
#if 0
    state = *statesToForward.begin();
    uint32_t mergeIndex = state->getMergeIndex();
    StatesTrace::iterator traceIt = statesTrace.find(mergeIndex);
    unsigned candidates = (traceIt == statesTrace.end() ? 0
                  : traceIt->second->size() - traceIt->second->count(state));

    if (candidates == 0) {
      // State can no longer be fast-forwarded
      statesToForward.erase(state);
      stats::fastForwardsFail += 1;
      continue;
    }

#else
    uint32_t mergeIndex = 0;
    unsigned candidates = 0;
    StatesTrace::iterator traceIt;

    /* Find the state that has maximum number of potential targets to merge */
    for (StatesSet::iterator it = statesToForward.begin(),
                        ie = statesToForward.end(); it != ie;) {
      unsigned _candidates = 0;
      uint32_t _mergeIndex;
      StatesTrace::iterator _traceIt;
      if (!(*it)->mergeDisabled()) {
        _mergeIndex = (*it)->getMergeIndex();
        _traceIt = statesTrace.find(_mergeIndex);
        _candidates = (_traceIt == statesTrace.end() ? 0
                       : _traceIt->second->size() - _traceIt->second->count(*it));
      }

      if (_candidates == 0) {
          // State can no longer be fast-forwarded, perhaps it branched
          // in a different direction that it's merge target

          // XXX: StatesSet does not support erase by iterator!
          // Moreover, erasing seems to invalidate iterators.
          statesToForward.erase(*it);
          stats::fastForwardsFail += 1;

          // Restart iteration from the begining since our iterators
          // are invalidated by calling erase
          it = statesToForward.begin();

      } else {
          if (_candidates > candidates) {
            state = *it;
            mergeIndex = _mergeIndex;
            candidates = _candidates;
            traceIt = _traceIt;
          }
          ++it;
      }
    }

    if (state == NULL) {
        // All states were removed from statesToForward
        break;
    }

#endif

    assert(!MaxStateMultiplicity || state->multiplicity < MaxStateMultiplicity);
    assert(!state->mergeDisabled());

    // Check wether we can already merge
    for (StatePosMap::iterator it = traceIt->second->begin(),
                             ie = traceIt->second->end(); it != ie; ++it) {
      ExecutionState *state1 = it->first;
      unsigned oldInstrCount = it->second;
      assert(!MaxStateMultiplicity || state1->multiplicity < MaxStateMultiplicity);
      //assert(!state1->mergeDisabled);

      if (state1 != state && !state1->mergeDisabled() &&
                state1->getMergeIndex() == mergeIndex) {
        // State is at the same execution index as state1, let's try merging
        merged = executor.merge(*state1, *state);
        if (merged) {
          // We've merged !

          // Any of the merged states could be followed for fast forwards.
          // Make traces that was pointing to state to point to state1
          for (StatesTrace::iterator it1 = statesTrace.begin(),
                                     ie1 = statesTrace.end(); it1 != ie1; ++it1) {
            if (MaxStateMultiplicity && merged->multiplicity >= MaxStateMultiplicity) {
              it1->second->erase(state1);
              it1->second->erase(state);
              if (merged != state1)
                it1->second->erase(merged);
            } else {
              bool erased = it1->second->erase(state);

              if (merged != state1)
                erased |= it1->second->erase(state1);
              if (erased) {
                it1->second->insert(std::make_pair(merged, oldInstrCount));
              }
            }
          }

          // Terminate merged state
          statesToForward.erase(state);
          executor.terminateState(*state, true);

          if (merged != state1) {
            if (statesToForward.erase(state1))
              statesToForward.insert(merged);
            executor.terminateState(*state1, true);
          }

          if (MaxStateMultiplicity && merged->multiplicity >= MaxStateMultiplicity) {
            statesToForward.erase(merged);
          }

          state = NULL;
          break;
        }
      }
    }

    if (state) {
      // The state was not merged right now. Let us fast-forward it.
      return *state;
    }
  }

  assert(state == NULL);

  // At this point we might have terminated states, but the base searcher is
  // unaware about it. We can not call it since it may crash. Instead, we
  // simply return the last merged state.
  if (merged)
    return *merged;

  // Nothing to fast-forward
  // Get state from base searcher
  state = &baseSearcher->selectState();

  if (canFastForwardState(state)) {
    statesToForward.insert(state);
    stats::fastForwardsStart += 1;
    return selectState(); // recursive
  }

  return *state;
}

void LazyMergingSearcher::update(ExecutionState *current,
                             const std::set<ExecutionState*> &addedStates,
                             const std::set<ExecutionState*> &removedStates) {
  // At this point, the pc of current state corresponds to the instruction
  // that is not yet executed. It will be executed when the state is selected.
  if (current && !current->mergeDisabled() &&
        removedStates.count(current) == 0 &&
        (!MaxStateMultiplicity || current->multiplicity < MaxStateMultiplicity)) {
    uint32_t mergeIndex = current->getMergeIndex();
    StatesTrace::iterator it = statesTrace.find(mergeIndex);
    if (it == statesTrace.end()) {
        it = statesTrace.insert(std::make_pair(mergeIndex, new StatePosMap)).first;
    }
    it->second->insert(std::make_pair(current, current->instsTotal));

    // XXX for some reason the following causes a slowdown
    /*
    if (statesToForward.empty() && it->second->size() > 1) {
        // We are not currently fast-forwarding, but the current
        // state can now be fast-forwarded
        statesToForward.insert(current);
        stats::fastForwardsStart += 1;
    }
    */
  }

  // TODO: we could add every newly created state to fast-forward track,
  // that would be more aggressive. This can be done be removing the following
  // 'if' condition. Worth trying and evaluating.
  if (!statesToForward.empty()) {
    // States created during fast-forward are also candidates for fast-forward
    // We would like to check it as soon as possible
    for (std::set<ExecutionState*>::const_iterator it = addedStates.begin(),
                                   ie = addedStates.end(); it != ie; ++it) {
      if (canFastForwardState(*it)) {
        statesToForward.insert(*it);
        stats::fastForwardsStart += 1;
      }
    }
  }

  for (std::set<ExecutionState*>::const_iterator it = removedStates.begin(),
                                 ie = removedStates.end(); it != ie; ++it) {
    ExecutionState *state = *it;
    statesToForward.erase(const_cast<ExecutionState*>(state));

    // Terminated states are useless for merge, remove them from traces
    for (StatesTrace::iterator it1 = statesTrace.begin(),
                               ie1 = statesTrace.end(); it1 != ie1;) {
      it1->second->erase(const_cast<ExecutionState*>(state));
      if (it1->second->empty()) {
        delete it1->second;
        statesTrace.erase(it1++);
      } else {
        ++it1;
      }
    }
  }

  baseSearcher->update(current, addedStates, removedStates);
}

///

BatchingSearcher::BatchingSearcher(Searcher *_baseSearcher,
                                   double _timeBudget,
                                   unsigned _instructionBudget)
  : baseSearcher(_baseSearcher),
    timeBudget(_timeBudget),
    instructionBudget(_instructionBudget),
    lastState(0) {
  
}

BatchingSearcher::~BatchingSearcher() {
  delete baseSearcher;
}

ExecutionState &BatchingSearcher::selectState() {
  if (!lastState || 
      (util::getWallTime()-lastStartTime)>timeBudget ||
      (stats::instructions-lastStartInstructions)>instructionBudget) {
    if (lastState) {
      double delta = util::getWallTime()-lastStartTime;
      if (delta>timeBudget*1.1) {
        std::cerr << "KLEE: increased time budget from " << timeBudget << " to " << delta << "\n";
        timeBudget = delta;
      }
    }
    lastState = &baseSearcher->selectState();
    lastStartTime = util::getWallTime();
    lastStartInstructions = stats::instructions;
    return *lastState;
  } else {
    return *lastState;
  }
}

void BatchingSearcher::update(ExecutionState *current,
                              const std::set<ExecutionState*> &addedStates,
                              const std::set<ExecutionState*> &removedStates) {
  if (removedStates.count(lastState))
    lastState = 0;
  baseSearcher->update(current, addedStates, removedStates);
}

/***/

CheckpointSearcher::CheckpointSearcher(Searcher *_baseSearcher) :
  baseSearcher(_baseSearcher), activeState(NULL), addedUnchecked(),
  addedChecked(), aggregateCount(0), totalUpdatesRecv(0),
  totalUpdatesSent(0) {

}

CheckpointSearcher::~CheckpointSearcher() {
  delete baseSearcher;
}

bool CheckpointSearcher::isCheckpoint(ExecutionState *state) {
  Instruction *inst = state->pc()->inst;
  return inst == &inst->getParent()->front();
}

ExecutionState &CheckpointSearcher::selectState() {
  if ((activeState && !isCheckpoint(activeState)) || !addedUnchecked.empty()) {
    aggregateCount++;

    if (activeState)
      return *activeState;
    else {
      ExecutionState *state = *(addedUnchecked.begin());
      return *state;
    }
  }

  if (activeState || !addedChecked.empty()) {
    std::set<ExecutionState*> added;
    for (StatesSet::iterator it = addedChecked.begin(); it != addedChecked.end();
        it++)
      added.insert(*it);

    baseSearcher->update(activeState, added, std::set<ExecutionState*>());
    totalUpdatesSent++;

    addedChecked.clear();
  }

  //CLOUD9_DEBUG("Aggregated " << aggregateCount << "states");

  aggregateCount = 1;

  activeState = &baseSearcher->selectState();

  assert(isCheckpoint(activeState) && "State in the underlying strategy not checkpointed");

  return *activeState;
}

bool CheckpointSearcher::empty() {
  if (!activeState && addedUnchecked.empty() && addedChecked.empty())
    return baseSearcher->empty();

  return false;
}

void CheckpointSearcher::update(ExecutionState *current,
    const std::set<ExecutionState*> &addedStates,
    const std::set<ExecutionState*> &removedStates) {

  totalUpdatesRecv++;

  std::set<ExecutionState*> newRemoved;

  if (current && isCheckpoint(current)) {
    if (addedUnchecked.erase(current))
      addedChecked.insert(current);
  }

  for (std::set<ExecutionState*>::iterator it = addedStates.begin();
      it != addedStates.end(); it++) {
    if (!isCheckpoint(*it))
      addedUnchecked.insert(*it);
    else
      addedChecked.insert(*it);
  }

  for (std::set<ExecutionState*>::iterator it = removedStates.begin();
      it != removedStates.end(); it++) {
    if (activeState == *it)
      activeState = NULL;

    bool found = addedUnchecked.erase(*it);
    found |= addedChecked.erase(*it);

    if (!found)
      newRemoved.insert(*it);
  }

  if (newRemoved.size() > 0) {
    baseSearcher->update(NULL, std::set<ExecutionState*>(), newRemoved);
    totalUpdatesSent++;
  }

  if (DebugCheckpointUpdates && totalUpdatesRecv % 100 == 0) {
    CLOUD9_DEBUG("Checkpoint searcher: Updates recv - " << totalUpdatesRecv << " sent - " << totalUpdatesSent);
  }
}

/***/

IterativeDeepeningTimeSearcher::IterativeDeepeningTimeSearcher(Searcher *_baseSearcher)
  : baseSearcher(_baseSearcher),
    time(1.) {
}

IterativeDeepeningTimeSearcher::~IterativeDeepeningTimeSearcher() {
  delete baseSearcher;
}

ExecutionState &IterativeDeepeningTimeSearcher::selectState() {
  ExecutionState &res = baseSearcher->selectState();
  startTime = util::getWallTime();
  return res;
}

void IterativeDeepeningTimeSearcher::update(ExecutionState *current,
                                            const std::set<ExecutionState*> &addedStates,
                                            const std::set<ExecutionState*> &removedStates) {
  double elapsed = util::getWallTime() - startTime;

  if (!removedStates.empty()) {
    std::set<ExecutionState *> alt = removedStates;
    for (std::set<ExecutionState*>::const_iterator it = removedStates.begin(),
           ie = removedStates.end(); it != ie; ++it) {
      ExecutionState *es = *it;
      std::set<ExecutionState*>::const_iterator it2 = pausedStates.find(es);
      if (it2 != pausedStates.end()) {
        pausedStates.erase(it);
        alt.erase(alt.find(es));
      }
    }    
    baseSearcher->update(current, addedStates, alt);
  } else {
    baseSearcher->update(current, addedStates, removedStates);
  }

  if (current && !removedStates.count(current) && elapsed>time) {
    pausedStates.insert(current);
    baseSearcher->removeState(current);
  }

  if (baseSearcher->empty()) {
    time *= 2;
    std::cerr << "KLEE: increasing time budget to: " << time << "\n";
    baseSearcher->update(0, pausedStates, std::set<ExecutionState*>());
    pausedStates.clear();
  }
}

/***/

InterleavedSearcher::InterleavedSearcher(const std::vector<Searcher*> &_searchers)
  : searchers(_searchers),
    index(1) {
}

InterleavedSearcher::~InterleavedSearcher() {
  for (std::vector<Searcher*>::const_iterator it = searchers.begin(),
         ie = searchers.end(); it != ie; ++it)
    delete *it;
}

ExecutionState &InterleavedSearcher::selectState() {
  Searcher *s = searchers[--index];
  if (index==0) index = searchers.size();
  return s->selectState();
}

void InterleavedSearcher::update(ExecutionState *current,
                                 const std::set<ExecutionState*> &addedStates,
                                 const std::set<ExecutionState*> &removedStates) {
  for (std::vector<Searcher*>::const_iterator it = searchers.begin(),
         ie = searchers.end(); it != ie; ++it)
    (*it)->update(current, addedStates, removedStates);
}

/***/

ForkCapSearcher::ForkCapSearcher(Executor &_executor,
                                 Searcher *_baseSearcher,
                                 unsigned long _forkCap,
                                 unsigned long _hardForkCap)
  : executor(_executor), baseSearcher(_baseSearcher),
    forkCap(_forkCap), hardForkCap(_hardForkCap) {
}

ForkCapSearcher::~ForkCapSearcher() {
  foreach (const ForkMap::value_type &forkMapItem, forkMap)
    delete forkMapItem.second;

  delete baseSearcher;
}

ExecutionState &ForkCapSearcher::selectState() {
  assert(!baseSearcher->empty() && !statesMap.empty());
  return baseSearcher->selectState();
}

void ForkCapSearcher::update(ExecutionState *current,
                             const std::set<ExecutionState *> &addedStates,
                             const std::set<ExecutionState *> &removedStates) {

  std::set<ExecutionState*> newAddedStates;
  std::set<ExecutionState*> newRemovedStates;

  /* Remove the removedStates from forkMap and check for caps */
  foreach (ExecutionState* state, removedStates) {
    StatesMap::iterator statesMapIter = statesMap.find(state);
    if (statesMapIter == statesMap.end()) {
      assert(disabledStates.count(state));
      continue;
    }

    StatesAtFork* statesAtFork = statesMapIter->second;
    statesMap.erase(statesMapIter);

    if (statesAtFork->active.erase(state)) {
      /* The removed state was active. Check whether there are states to resume
         in place of the removed state */
      if (!statesAtFork->paused.empty()) {
        /* For now we resume the state that happened to be first
           in the set ordering. TODO: resume random state or define
           a heuristic to determine which state to resume. */
        ExecutionState *resumedState = *statesAtFork->paused.begin();
        statesAtFork->paused.erase(resumedState);
        statesAtFork->active.insert(resumedState);

        /* Resumed state could be amoung states to be deleted, so check for it */
        /* XXX: avoid digging into removedStates again */
        if (removedStates.count(resumedState) == 0) {
          newAddedStates.insert(resumedState);
          executor.processTree->markActive(resumedState->ptreeNode);
        }
      }

      /* The state was active, which means the baseSearcher knows about it */
      newRemovedStates.insert(state);

    } else {
      /* The states was paused. Just remove it and forget. */
      bool ok = statesAtFork->paused.erase(state);
      assert(ok);
    }
  }

  if (!addedStates.empty()) {
    /* Get StatesAtFork for the current fork point */
    KInstruction *forkPoint = current ? current->prevPC() :
                                    (*addedStates.begin())->prevPC();
    ForkMap::iterator forkMapIter = forkMap.find(forkPoint);
    if (forkMapIter == forkMap.end())
      forkMapIter = forkMap.insert(
                std::make_pair(forkPoint, new StatesAtFork)).first;

    StatesAtFork* statesAtFork = forkMapIter->second;

    if (current) {
      /* XXX: does KLEE ever removes current like this ? */
      assert(removedStates.count(current) == 0);

      StatesMap::iterator statesMapIter = statesMap.find(current);
      assert(statesMapIter != statesMap.end());

      StatesAtFork* statesAtOldFork = statesMapIter->second;
      assert(statesAtOldFork->active.count(current));

      if (statesAtOldFork != statesAtFork) {
        /* We assume that addedStates are non-empty because the current state
           have forked. It means it should be removed from previous forkpoint and
           readded to a new one. */
        /* TODO: verify this */

        /* Remove current state from the previous fork point */
        statesAtOldFork->active.erase(current);

        if (!statesAtOldFork->paused.empty()) {
          /* For now we resume the state that happened to be first
             in the set ordering. TODO: resume random state or define
             a heuristic to determine which state to resume. */
          ExecutionState *resumedState = *statesAtOldFork->paused.begin();
          statesAtOldFork->paused.erase(resumedState);
          statesAtOldFork->active.insert(resumedState);
          newAddedStates.insert(resumedState);
          executor.processTree->markActive(resumedState->ptreeNode);
        }

        /* Add current state to the new fork point */
        statesAtFork->totalForks += 1;
        if (!hardForkCap || statesAtFork->totalForks <= hardForkCap) {
          statesMapIter->second = statesAtFork;
          if (statesAtFork->active.size() < forkCap) {
            statesAtFork->active.insert(current);
          } else {
            statesAtFork->paused.insert(current);
            newRemovedStates.insert(current);
            executor.processTree->markInactive(current->ptreeNode);
          }
        } else {
          statesMap.erase(statesMapIter);
          disabledStates.insert(current);
          newRemovedStates.insert(current);
          executor.processTree->markInactive(current->ptreeNode);
        }
      }
    }

    /* Add all other states */
    foreach (ExecutionState *state, addedStates) {
      assert(state->prevPC() == forkPoint);

      statesAtFork->totalForks += 1;
      if (!hardForkCap || statesAtFork->totalForks <= hardForkCap) {
        statesMap.insert(std::make_pair(state, statesAtFork));
        if (statesAtFork->active.size() < forkCap) {
          statesAtFork->active.insert(state);
          newAddedStates.insert(state);
        } else {
          statesAtFork->paused.insert(state);
          executor.processTree->markInactive(state->ptreeNode);
        }
      } else {
        disabledStates.insert(state);
        executor.processTree->markInactive(state->ptreeNode);
      }
    }
  }

  baseSearcher->update(current, newAddedStates, newRemovedStates);
}
