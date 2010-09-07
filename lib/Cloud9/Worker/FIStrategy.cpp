/*
 * FIStrategy.cpp
 *
 *  Created on: Sep 4, 2010
 *      Author: stefan
 */

#include "cloud9/worker/FIStrategy.h"

#include "cloud9/worker/TreeObjects.h"

namespace cloud9 {

namespace worker {

FIStrategy::FIStrategy(WorkerTree *_workerTree) : workerTree(_workerTree) {

}

unsigned FIStrategy::countInjections(SymbolicState *s, WorkerTree::Node *root, bool &interesting) {
  interesting = true;
  unsigned count = 0;

  WorkerTree::Node *crtNode = s->getNode().get();
  assert(crtNode->layerExists(WORKER_LAYER_STATES));

  while (crtNode != root) {
    WorkerTree::Node *pNode = crtNode->getParent();

    klee::ForkTag tag = (**pNode).getForkTag();

    if (tag.forkClass == klee::KLEE_FORK_FAULTINJ) {
      if (crtNode->getIndex() == 1) {
        count++;

        if (!tag.fiVulnerable)
          interesting = false;
      }
    }

    crtNode = pNode;
  }

  return count;
}

ExecutionJob* FIStrategy::onNextJobSelection() {
  if (interesting.size() > 0) {
    //CLOUD9_DEBUG("Selecting interesting state with FI counter " << interesting.begin()->first);

    assert(interesting.begin()->second.size() > 0);
    return selectJob(workerTree, *interesting.begin()->second.begin());
  } else if (uninteresting.size() > 0) {
    //CLOUD9_DEBUG("Selecting uninteresting state with FI counter " << uninteresting.begin()->first);

    assert(uninteresting.begin()->second.size() > 0);
    return selectJob(workerTree, *uninteresting.begin()->second.begin());
  } else {
    //CLOUD9_DEBUG("No more states to select...");
    return NULL;
  }
}

void FIStrategy::mapState(SymbolicState *state, unsigned count, bool isInt) {
  if (isInt) {
    interesting[count].insert(state);
    //CLOUD9_DEBUG("Interesting state mapped on " << count);
  } else {
    uninteresting[count].insert(state);
    //CLOUD9_DEBUG("Uninteresting state mapped on " << count);
  }
}

void FIStrategy::unmapState(SymbolicState *state, unsigned count) {
  unsigned res = 0;
  res += interesting[count].erase(state);
  res += uninteresting[count].erase(state);
  assert(res == 1);

  if (interesting[count].empty())
    interesting.erase(count);
  if (uninteresting[count].empty())
    uninteresting.erase(count);
}

void FIStrategy::onStateActivated(SymbolicState *state) {
  bool isInt;
  unsigned count = countInjections(state, workerTree->getRoot(), isInt);
  fiCounters[state] = count;

  mapState(state, count, isInt);
}

void FIStrategy::onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) {
  bool isInt;

  unsigned count = countInjections(state, oldNode, isInt);

  if (count == 0)
    return;

  //CLOUD9_DEBUG("State updated!");

  unsigned oldCount = fiCounters[state];
  count += oldCount;

  fiCounters[state] = count;

  isInt = isInt && (interesting[count].count(state) > 0);

  unmapState(state, oldCount);
  mapState(state, count, isInt);
}

void FIStrategy::onStateDeactivated(SymbolicState *state) {
  //CLOUD9_DEBUG("Removing state...");

  unsigned count = fiCounters[state];
  fiCounters.erase(state);

  unmapState(state, count);
}

}

}
