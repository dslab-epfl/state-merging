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

static bool compareStates(std::pair<SymbolicState*, unsigned> p1,
      std::pair<SymbolicState*, unsigned> p2) {
  return p1.second < p2.second;
}

FIStrategy::FIStrategy(WorkerTree *_workerTree) : workerTree(_workerTree),
    interesting(&compareStates), uninteresting(&compareStates) {

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
    return selectJob(workerTree, interesting.begin()->first);
  } else if (uninteresting.size() > 0) {
    return selectJob(workerTree, uninteresting.begin()->first);
  } else {
    return NULL;
  }
}

void FIStrategy::onStateActivated(SymbolicState *state) {
  bool isInt;
  unsigned count = countInjections(state, workerTree->getRoot(), isInt);
  fiCounters[state] = count;

  if (isInt)
    interesting.insert(std::make_pair(state, count));
  else
    uninteresting.insert(std::make_pair(state, count));
}

void FIStrategy::onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) {
  bool isInt;

  unsigned count = countInjections(state, oldNode, isInt);

  if (count == 0)
    return;

  unsigned oldCount = fiCounters[state];
  count += oldCount;

  fiCounters[state] = count;

  unsigned res = 0;
  res += interesting.erase(std::make_pair(state, oldCount));

  isInt = isInt && (res != 0);

  res += uninteresting.erase(std::make_pair(state, oldCount));
  assert(res == 1);

  if (isInt)
    interesting.insert(std::make_pair(state, count));
  else
    uninteresting.insert(std::make_pair(state, count));
}

void FIStrategy::onStateDeactivated(SymbolicState *state) {
  unsigned count = fiCounters[state];
  fiCounters.erase(state);

  unsigned res = 0;
  res += interesting.erase(std::make_pair(state, count));
  res += uninteresting.erase(std::make_pair(state, count));
  assert(res == 1);
}

}

}
