//===-- CoreStrategies.h ----------------------------------------*- C++ -*-===//
/*
 * CoreStrategies.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef CORESTRATEGIES_H_
#define CORESTRATEGIES_H_

#include "cloud9/worker/TreeNodeInfo.h"

#include <vector>
#include <map>

namespace klee {
class Searcher;
class ExecutionState;
}

namespace cloud9 {

namespace worker {

class ExecutionJob;
class SymbolicEngine;
class SymbolicState;

////////////////////////////////////////////////////////////////////////////////
// Basic Building Blocks
////////////////////////////////////////////////////////////////////////////////

/*
 * The abstract base class for all job strategies
 */
class JobSelectionStrategy {
public:
	JobSelectionStrategy() {};
	virtual ~JobSelectionStrategy() {};

public:
	virtual void onJobAdded(ExecutionJob *job) = 0;
	virtual ExecutionJob* onNextJobSelection() = 0;
	virtual void onRemovingJob(ExecutionJob *job) = 0;

	virtual void onStateActivated(SymbolicState *state) = 0;
	virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) = 0;
	virtual void onStateDeactivated(SymbolicState *state) = 0;
};

class StateSelectionStrategy {
public:
  StateSelectionStrategy() { }
  virtual ~StateSelectionStrategy() { }

public:
  virtual void onStateActivated(SymbolicState *state) { };
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) { };
  virtual void onStateDeactivated(SymbolicState *state) { };

  virtual SymbolicState* onNextStateSelection() = 0;
};

class BasicStrategy : public JobSelectionStrategy {
protected:
  ExecutionJob *selectJob(WorkerTree *tree, SymbolicState* state);
public:
	BasicStrategy() {};
	virtual ~BasicStrategy() {};

public:
	virtual void onJobAdded(ExecutionJob *job) { };
	virtual ExecutionJob* onNextJobSelection() = 0;
	virtual void onRemovingJob(ExecutionJob *job) { };

	virtual void onStateActivated(SymbolicState *state) { };
	virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) { };
	virtual void onStateDeactivated(SymbolicState *state) { };
};

class RandomJobFromStateStrategy: public BasicStrategy {
private:
  WorkerTree *tree;
  StateSelectionStrategy *stateStrat;
public:
  RandomJobFromStateStrategy(WorkerTree *_tree, StateSelectionStrategy *_stateStrat) :
    tree(_tree), stateStrat(_stateStrat) { }

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);

  virtual ExecutionJob* onNextJobSelection();
};

class RandomPathStrategy: public StateSelectionStrategy {
private:
  WorkerTree *tree;
public:
  RandomPathStrategy(WorkerTree *t) :
    tree(t) { };

  virtual ~RandomPathStrategy() { };

  virtual SymbolicState* onNextStateSelection();
};

////////////////////////////////////////////////////////////////////////////////
// State Search Strategies
////////////////////////////////////////////////////////////////////////////////

class ClusteredRandomPathStrategy: public StateSelectionStrategy {
private:
  typedef std::set<SymbolicState*> state_set_t;
  WorkerTree *tree;
  state_set_t states;
public:
  ClusteredRandomPathStrategy(WorkerTree *t) :
    tree(t) { };

  virtual ~ClusteredRandomPathStrategy() { };

  virtual SymbolicState* onNextStateSelection();
  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateDeactivated(SymbolicState *state);
};

class RandomStrategy: public StateSelectionStrategy {
private:
    std::vector<SymbolicState*> states;
    std::map<SymbolicState*, unsigned> indices;
public:
    RandomStrategy() {};
    virtual ~RandomStrategy() {};

    virtual SymbolicState* onNextStateSelection();
    virtual void onStateActivated(SymbolicState *state);
    virtual void onStateDeactivated(SymbolicState *state);
};

class KleeStrategy: public StateSelectionStrategy {
protected:
    WorkerTree *tree;
    klee::Searcher *searcher;

    KleeStrategy(WorkerTree *_tree);
public:
    KleeStrategy(WorkerTree *_tree, klee::Searcher *_searcher);
    virtual ~KleeStrategy();

    virtual void onStateActivated(SymbolicState *state);
    virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
    virtual void onStateDeactivated(SymbolicState *state);

    virtual SymbolicState* onNextStateSelection();
};

class WeightedRandomStrategy: public KleeStrategy {
public:
    enum WeightType {
        Depth,
        QueryCost,
        InstCount,
        CPInstCount,
        MinDistToUncovered,
        CoveringNew
      };
public:
    WeightedRandomStrategy(WeightType _type, WorkerTree *_tree, SymbolicEngine *_engine);
    virtual ~WeightedRandomStrategy();

};

class LimitedFlowStrategy: public StateSelectionStrategy {
private:
  StateSelectionStrategy *underStrat;
  StateSelectionStrategy *workingStrat;

  unsigned maxCount;
  std::set<SymbolicState*> activeStates;
public:
  LimitedFlowStrategy(StateSelectionStrategy *_underStrat,
      StateSelectionStrategy *_workingStrat, unsigned _maxCount) :
    underStrat(_underStrat), workingStrat(_workingStrat), maxCount(_maxCount) { }

  virtual SymbolicState* onNextStateSelection();
  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateDeactivated(SymbolicState *state);
};

}

}

#endif /* CORESTRATEGIES_H_ */
