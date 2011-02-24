/*
 * PartitioningStrategy.h
 *
 *  Created on: Feb 18, 2011
 *      Author: stefan
 */

#ifndef PARTITIONINGSTRATEGY_H_
#define PARTITIONINGSTRATEGY_H_

#include "cloud9/worker/CoreStrategies.h"

#include <vector>
#include <set>

namespace cloud9 {

namespace worker {

class PartitioningStrategy;

class StatePartition {
  friend class PartitioningStrategy;
private:
  StateSelectionStrategy *strategy;
  std::set<SymbolicState*> states;
  std::set<SymbolicState*> active;
  unsigned forkCount;
public:
  StatePartition(StateSelectionStrategy *_strategy)
    : strategy(_strategy), forkCount(0) { }

  virtual ~StatePartition() {
    delete strategy;
  }

  unsigned getForkCount() const { return forkCount; }
};

class PartitioningStrategy: public StateSelectionStrategy {
private:
  typedef unsigned int part_id_t;
  std::map<SymbolicState*, part_id_t> states;
  std::map<part_id_t, StatePartition> partitions;
  std::set<SymbolicState*> active;
  std::set<part_id_t> nonEmpty;

  WorkerTree *tree;
  SymbolicEngine *engine;

  StatePartition createPartition();
  void activateStateInPartition(SymbolicState *state, part_id_t partID,
      StatePartition &part);
  void deactivateStateInPartition(SymbolicState *state, part_id_t partID,
      StatePartition &part);

protected:
  virtual bool isActive(part_id_t partition);
  virtual part_id_t hashState(SymbolicState* state);
public:
  PartitioningStrategy(WorkerTree *_tree, SymbolicEngine *_engine)
    : tree(_tree), engine(_engine) { }
  virtual ~PartitioningStrategy() { }

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);

  virtual SymbolicState* onNextStateSelection();
};

}

}

#endif /* PARTITIONINGSTRATEGY_H_ */
