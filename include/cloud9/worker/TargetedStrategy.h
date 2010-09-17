/*
 * TargetedStrategy.h
 *
 *  Created on: Sep 17, 2010
 *      Author: stefan
 */

#ifndef TARGETEDSTRATEGY_H_
#define TARGETEDSTRATEGY_H_

#include "cloud9/worker/CoreStrategies.h"
#include "klee/ForkTag.h"

#include <set>
#include <vector>

namespace cloud9 {

namespace worker {

class TargetedStrategy: public BasicStrategy {
private:
  typedef std::map<SymbolicState*, unsigned> state_set_t;
  typedef std::vector<SymbolicState*> state_vector_t;

  typedef std::pair<state_set_t, state_vector_t> state_container_t;

  WorkerTree *workerTree;

  state_container_t interestingStates;
  state_container_t uninterestingStates;

  SymbolicState *selectRandom(state_container_t &container);
  void insertState(SymbolicState *state, state_container_t &container);
  void removeState(SymbolicState *state, state_container_t &container);

  bool isInteresting(klee::ForkTag forkTag);
public:
  TargetedStrategy(WorkerTree *_workerTree);
  virtual ~TargetedStrategy() { }

  virtual ExecutionJob* onNextJobSelection();

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);
};

}

}

#endif /* TARGETEDSTRATEGY_H_ */
