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
  typedef bool (*statecmp_fn)(std::pair<SymbolicState*, unsigned> p1,
      std::pair<SymbolicState*, unsigned> p2);
  typedef std::set<std::pair<SymbolicState*, unsigned>, statecmp_fn> state_set_t;

  WorkerTree *workerTree;

  std::map<SymbolicState*, unsigned> fiCounters;    // Fault injection counters

  state_set_t interesting;
  state_set_t uninteresting;

  unsigned countInjections(SymbolicState *s, WorkerTree::Node *root, bool &interesting);
public:
  FIStrategy(WorkerTree *_workerTree);
  virtual ~FIStrategy() { }

  virtual ExecutionJob* onNextJobSelection();

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);
};

}

}

#endif /* FISTRATEGY_H_ */
