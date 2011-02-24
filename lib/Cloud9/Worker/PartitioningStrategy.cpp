/*
 * PartitioningStrategy.cpp
 *
 *  Created on: Feb 18, 2011
 *      Author: stefan
 */

#include "cloud9/worker/PartitioningStrategy.h"
#include "cloud9/worker/ComplexStrategies.h"
#include "cloud9/worker/TreeObjects.h"
#include "cloud9/worker/WorkerCommon.h"

#include "klee/Internal/ADT/RNG.h"

using namespace klee;

namespace cloud9 {

namespace worker {

PartitioningStrategy::part_id_t
PartitioningStrategy::hashState(SymbolicState* state) {
  WorkerTree::Node *node = state->getNode().get();
  node = node->getParent();
  if (!node)
    return 0;

  ForkTag tag = (**node).getForkTag();

  return (part_id_t)tag.instrID;
}

bool PartitioningStrategy::isActive(part_id_t partition) {
  return true;
}

StatePartition PartitioningStrategy::createPartition() {
  std::vector<StateSelectionStrategy*> strategies;

  strategies.push_back(new WeightedRandomStrategy(
         WeightedRandomStrategy::CoveringNew,
         tree,
         engine));
  strategies.push_back(new ClusteredRandomPathStrategy(tree));

  return StatePartition(new LimitedFlowStrategy(
      new TimeMultiplexedStrategy(strategies),
      new RandomStrategy(),
      10));
}

void PartitioningStrategy::activateStateInPartition(SymbolicState *state,
    part_id_t partID, StatePartition &part) {
  part.active.insert(state);
  active.insert(state);
  nonEmpty.insert(partID);
}

void PartitioningStrategy::deactivateStateInPartition(SymbolicState *state,
    part_id_t partID, StatePartition &part) {
  part.active.erase(state);
  active.erase(state);
  if (part.active.empty()) {
    if (partID == *nextPartition) {
      nextPartition++;
    }
    nonEmpty.erase(partID);
  }
}

void PartitioningStrategy::onStateActivated(SymbolicState *state) {
  part_id_t key = hashState(state);
  states[state] = key;

  if (partitions.count(key) == 0) {
    partitions.insert(std::make_pair(key, createPartition()));
  }
  StatePartition &part = partitions.find(key)->second;

  part.states.insert(state);
  part.strategy->onStateActivated(state);

  // Now decide whether to activate the state or not
  if (isActive(key)) {
    activateStateInPartition(state, key, part);
  } else {
    if (part.active.count(state->getParent()) > 0) {
      if (theRNG.getBool()) {
        // Remove the parent from the active states, and replace it with the child
        deactivateStateInPartition(state->getParent(), key, part);
        activateStateInPartition(state, key, part);
      }
    } else {
      activateStateInPartition(state, key, part);
    }
  }
}

void PartitioningStrategy::onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) {
  WorkerTree::Node *newNode = state->getNode().get();

  if (newNode == oldNode) {
    part_id_t key = hashState(state);
    assert(partitions.count(key) > 0);
    partitions.find(key)->second.strategy->onStateUpdated(state, oldNode);
    return;
  }

  part_id_t newKey = hashState(state);
  part_id_t oldKey = states[state];
  if (newKey == oldKey) {
    partitions.find(newKey)->second.strategy->onStateUpdated(state, oldNode);
    return;
  }

  StatePartition &oldPart = partitions.find(oldKey)->second;
  states[state] = newKey;
  deactivateStateInPartition(state, oldKey, oldPart);
  oldPart.states.erase(state);
  oldPart.strategy->onStateDeactivated(state);

  if (partitions.count(newKey) == 0) {
    partitions.insert(std::make_pair(newKey, createPartition()));
  }

  StatePartition &newPart = partitions.find(newKey)->second;
  newPart.states.insert(state);
  newPart.strategy->onStateActivated(state);
  // By default, the state is active - it may be deactivated when the
  // child state is added
  activateStateInPartition(state, newKey, newPart);
}

void PartitioningStrategy::onStateDeactivated(SymbolicState *state) {
  part_id_t key = hashState(state);
  states.erase(state);

  StatePartition &part = partitions.find(key)->second;

  deactivateStateInPartition(state, key, part);
  part.states.erase(state);
  part.strategy->onStateDeactivated(state);
}

SymbolicState* PartitioningStrategy::onNextStateSelection() {
  if (nextPartition == nonEmpty.end()) {
    nextPartition = nonEmpty.begin();
  }

  if (nextPartition != nonEmpty.end()) {
    // Debug info
    std::stringstream ss;
    for (part_id_set_t::iterator it = nonEmpty.begin(); it != nonEmpty.end(); it++) {
      ss << '[' << *it << ':' << partitions.find(*it)->second.active.size() << "] ";
    }
    CLOUD9_DEBUG("Selecting: " << *nextPartition << " Partitioning: " << ss.str());

    StatePartition &part = partitions.find(*nextPartition)->second;
    SymbolicState *state = part.strategy->onNextStateSelection();
    assert(state != NULL);
    nextPartition++;

    return state;
  }

  return NULL;
}

}

}
