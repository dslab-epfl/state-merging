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
  std::set<SymbolicState*> states;
  std::set<SymbolicState*> activeStates;
  StateSelectionStrategy *strategy;
  bool active;
public:
  StatePartition(StateSelectionStrategy *_strategy)
    : strategy(_strategy), active(true) { }

  virtual ~StatePartition() { }
};

typedef unsigned int part_id_t;
typedef std::set<part_id_t> part_id_set_t;
typedef std::map<part_id_t, std::pair<unsigned, unsigned> > part_stats_t;
typedef std::map<part_id_t, unsigned> part_select_t;

class PartitioningStrategy: public StateSelectionStrategy {
private:
  std::map<SymbolicState*, part_id_t> states;
  std::map<part_id_t, StatePartition> partitions;
  std::set<SymbolicState*> activeStates;
  part_id_set_t nonEmpty;

  part_id_set_t::iterator nextPartition;

  WorkerTree *tree;
  SymbolicEngine *engine;

  unsigned forkQuota;

  StatePartition createPartition();
  void activateStateInPartition(SymbolicState *state, part_id_t partID,
      StatePartition &part);
  void deactivateStateInPartition(SymbolicState *state, part_id_t partID,
      StatePartition &part);
  part_id_t hashState(SymbolicState* state);
  void getInactiveSet(part_id_t partID, std::set<SymbolicState*> &inactiveStates);
public:
  PartitioningStrategy(WorkerTree *_tree, SymbolicEngine *_engine, unsigned _forkQuota = 0)
    : tree(_tree), engine(_engine), forkQuota(_forkQuota) {
    nextPartition = nonEmpty.begin();
  }
  virtual ~PartitioningStrategy() { }

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);

  virtual SymbolicState* onNextStateSelection();

  void getStatistics(part_stats_t &stats);
  void setActivation(std::set<part_id_t> &activation);
  ExecutionPathSetPin selectStates(part_select_t &counts);
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
