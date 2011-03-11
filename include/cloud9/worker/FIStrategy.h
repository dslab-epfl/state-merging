/*
 * FIStrategy.h
 *
 *  Created on: Sep 4, 2010
 *      Author: stefan
 */

#ifndef FISTRATEGY_H_
#define FISTRATEGY_H_

#include "cloud9/worker/CoreStrategies.h"

#include <map>
#include <queue>

namespace cloud9 {

namespace worker {

class FIStrategy: public BasicStrategy {
private:
  typedef std::map<SymbolicState*, unsigned> counters_set_t;
  typedef std::map<unsigned, std::set<SymbolicState*> > state_set_t;

  WorkerTree *workerTree;

  counters_set_t fiCounters;    // Fault injection counters

  state_set_t interesting;
  state_set_t uninteresting;

  unsigned countInjections(SymbolicState *s, WorkerTree::Node *root, bool &interesting);

  void mapState(SymbolicState *state, unsigned count, bool isInt);
  void unmapState(SymbolicState *state, unsigned count);
public:
  FIStrategy(WorkerTree *_workerTree, JobManager *_jobManager);
  virtual ~FIStrategy() { }

  virtual ExecutionJob* onNextJobSelection();

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);
};

}

}

#endif /* FISTRATEGY_H_ */
