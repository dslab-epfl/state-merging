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

TargetedStrategy::TargetedStrategy(WorkerTree *_workerTree) :
    workerTree(_workerTree) {

}

bool TargetedStrategy::isInteresting(klee::ForkTag forkTag) {
  if (forkTag.location->function->getNameStr() == "print_direc") {
    return true;
  } else {
    return false;
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
}

void TargetedStrategy::removeState(SymbolicState *state,
    state_container_t &cont) {
  if (cont.first.count(state) > 0) {
    cont.second[cont.first[state]] = cont.second.back();
    cont.second.pop_back();
    cont.first.erase(state);
  }
}

ExecutionJob* TargetedStrategy::onNextJobSelection() {
  if (interestingStates.first.size() > 0) {
    SymbolicState *state = selectRandom(interestingStates);

    return selectJob(workerTree, state);
  } else {
    if (uninterestingStates.first.size() == 0)
      return NULL;

    SymbolicState *state = selectRandom(uninterestingStates);

    removeState(state, uninterestingStates);
    insertState(state, interestingStates);

    return selectJob(workerTree, state);
  }
}

void TargetedStrategy::onStateActivated(SymbolicState *state) {
  WorkerTree::Node *pNode = state->getNode()->getParent();

  if (!pNode || isInteresting((**pNode).getForkTag())) {
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

}

}
