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
public:
  StatePartition(StateSelectionStrategy *_strategy)
    : strategy(_strategy) { }

  virtual ~StatePartition() { }
};

class PartitioningStrategy: public StateSelectionStrategy {
private:
  typedef unsigned int part_id_t;
  typedef std::set<part_id_t> part_id_set_t;

  std::map<SymbolicState*, part_id_t> states;
  std::map<part_id_t, StatePartition> partitions;
  std::set<SymbolicState*> active;
  part_id_set_t nonEmpty;

  part_id_set_t::iterator nextPartition;

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
    : tree(_tree), engine(_engine) {
    nextPartition = nonEmpty.begin();
  }
  virtual ~PartitioningStrategy() { }

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);

  virtual SymbolicState* onNextStateSelection();
};

class KleeForkCapStrategy: public KleeStrategy {
public:
  KleeForkCapStrategy(unsigned long _forkCap, unsigned long _hardForkCap,
      WorkerTree *_tree, SymbolicEngine *_engine);
  virtual ~KleeForkCapStrategy();
};

}

}

#endif /* PARTITIONINGSTRATEGY_H_ */
