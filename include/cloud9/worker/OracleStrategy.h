/*
 * OracleStrategy.h
 *
 *  Created on: May 7, 2010
 *      Author: stefan
 */

#ifndef ORACLESTRATEGY_H_
#define ORACLESTRATEGY_H_

#include "cloud9/worker/CoreStrategies.h"

namespace cloud9 {

namespace worker {

class OracleStrategy: public BasicStrategy {
private:
  std::vector<unsigned int> goalPath;
  WorkerTree *tree;

  std::set<SymbolicState *> validStates;

  bool checkProgress(SymbolicState *state);
public:
  OracleStrategy(WorkerTree *_tree, std::vector<unsigned int> &_goalPath,
      JobManager *_jobManager);
  virtual ~OracleStrategy();

  virtual ExecutionJob* onNextJobSelection();

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);
};

}

}

#endif /* ORACLESTRATEGY_H_ */
