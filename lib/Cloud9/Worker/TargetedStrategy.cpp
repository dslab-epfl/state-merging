/*
 * TargetedStrategy.cpp
 *
 *  Created on: Sep 17, 2010
 *      Author: stefan
 */

#include "cloud9/worker/TargetedStrategy.h"

#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/TreeObjects.h"

#include "llvm/Function.h"

#include "klee/Internal/ADT/RNG.h"
#include "klee/Internal/Module/KModule.h"

using namespace klee;

namespace cloud9 {

namespace worker {

TargetedStrategy::interests_t TargetedStrategy::anything = interests_t();

TargetedStrategy::TargetedStrategy(WorkerTree *_workerTree) :
    workerTree(_workerTree), adoptionRate(10) {

}

bool TargetedStrategy::isInteresting(klee::ForkTag forkTag, interests_t &_interests) {
  if (_interests.empty())
    return true;

  if (_interests.count(forkTag.location->function->getNameStr()) > 0)
    return true;

  return false;
}

bool TargetedStrategy::isInteresting(SymbolicState *state, interests_t &_interests) {
  WorkerTree::Node *pNode = state->getNode()->getParent();

  if (!pNode || isInteresting((**pNode).getForkTag(), _interests))
    return true;
  else
    return false;
}

void TargetedStrategy::adoptStates() {
  unsigned int count = adoptionRate * uninterestingStates.first.size() / 100;
  if (!count)
    count = 1;

  for (unsigned int i = 0; i < count; i++) {
    SymbolicState *state = selectRandom(uninterestingStates);

    removeState(state, uninterestingStates);
    insertState(state, interestingStates);
  }
}

SymbolicState *TargetedStrategy::selectRandom(state_container_t &cont) {
  assert(cont.second.size() > 0);

  int index = klee::theRNG.getInt32() % cont.second.size();

  return cont.second[index];
}

void TargetedStrategy::insertState(SymbolicState *state,
    state_container_t &cont) {
  if (cont.first.count(state) == 0) {
    cont.second.push_back(state);
    cont.first[state] = cont.second.size() - 1;
  }
  assert(cont.first.size() == cont.second.size());
}

void TargetedStrategy::removeState(SymbolicState *state,
    state_container_t &cont) {
  if (cont.first.count(state) > 0) {
    cont.first[cont.second.back()] = cont.first[state];
    cont.second[cont.first[state]] = cont.second.back();
    cont.second.pop_back();

    cont.first.erase(state);
  }
  assert(cont.first.size() == cont.second.size());
}

ExecutionJob* TargetedStrategy::onNextJobSelection() {
  if (interestingStates.first.size() == 0) {
    if (uninterestingStates.first.size() == 0)
      return NULL;

    adoptStates();
  }

  SymbolicState *state = selectRandom(interestingStates);
  return selectJob(workerTree, state);
}

void TargetedStrategy::onStateActivated(SymbolicState *state) {
  if (isInteresting(state, localInterests)) {
    insertState(state, interestingStates);
  } else {
    insertState(state, uninterestingStates);
  }
}

void TargetedStrategy::onStateUpdated(SymbolicState *state,
    WorkerTree::Node *oldNode) {
  // Do nuthin'
}

void TargetedStrategy::onStateDeactivated(SymbolicState *state) {
  removeState(state, interestingStates);
  removeState(state, uninterestingStates);
}

void TargetedStrategy::updateInterests(interests_t &_interests) {
  localInterests = _interests;

  // Now we need to rehash states
  state_container_t newInteresting;
  state_container_t newUninteresting;

  for (unsigned i = 0; i < interestingStates.second.size(); i++) {
    SymbolicState *state = interestingStates.second[i];

    if (isInteresting(state, localInterests))
      insertState(state, newInteresting);
    else
      insertState(state, newUninteresting);
  }

  for (unsigned i = 0; i < uninterestingStates.second.size(); i++) {
    SymbolicState *state = uninterestingStates.second[i];

    if (isInteresting(state, localInterests))
      insertState(state, newInteresting);
    else
      insertState(state, newUninteresting);
  }

  interestingStates = newInteresting;
  uninterestingStates = newUninteresting;
}

unsigned int TargetedStrategy::selectForExport(state_container_t &container,
      interests_t &interests, std::vector<SymbolicState*> &states,
      unsigned int maxCount) {
  unsigned int result = 0;

  for (unsigned int i = 0; i < container.second.size(); i++) {
    SymbolicState *state = container.second[i];

    if (maxCount > 0 && isInteresting(state, interests)) {
      states.push_back(state);
      result++;
      maxCount--;
    }

    if (maxCount == 0)
      break;
  }

  return result;
}

unsigned int TargetedStrategy::selectForExport(interests_t &interests,
    std::vector<SymbolicState*> &states, unsigned int maxCount) {
  // First, seek among the uninteresting ones
  unsigned int result = 0;

  result += selectForExport(uninterestingStates, interests, states, maxCount-result);
  if (result == maxCount)
    return maxCount;

  // Second, seek among the interesting ones
  result += selectForExport(interestingStates, interests, states, maxCount-result);
  if (result == maxCount)
    return maxCount;

  // Third, pick any uninteresting
  result += selectForExport(uninterestingStates, anything, states, maxCount-result);
  if (result == maxCount)
    return maxCount;

  // Fourth, pick any interesting
  result += selectForExport(interestingStates, anything, states, maxCount-result);

  return result;
}

}

}
