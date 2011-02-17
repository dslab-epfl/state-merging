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

class RandomStrategy: public BasicStrategy {
private:
	std::vector<ExecutionJob*> jobs;
	std::map<ExecutionJob*, unsigned> indices;
public:
	RandomStrategy() {};
	virtual ~RandomStrategy() {};

	virtual void onJobAdded(ExecutionJob *job);
	virtual ExecutionJob* onNextJobSelection();
	virtual void onRemovingJob(ExecutionJob *job);
};

class RandomPathStrategy: public BasicStrategy {
private:
  WorkerTree *tree;
public:
  RandomPathStrategy(WorkerTree *t) :
    tree(t) { };

  virtual ~RandomPathStrategy() { };

  virtual ExecutionJob* onNextJobSelection();
};

class ClusteredRandomPathStrategy: public BasicStrategy {
private:
  typedef std::set<SymbolicState*> state_set_t;
  WorkerTree *tree;
  state_set_t states;
public:
  ClusteredRandomPathStrategy(WorkerTree *t) :
    tree(t) { };

  virtual ~ClusteredRandomPathStrategy() { };

  virtual ExecutionJob* onNextJobSelection();
  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateDeactivated(SymbolicState *state);
};

class KleeStrategy: public BasicStrategy {
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

	virtual ExecutionJob* onNextJobSelection();
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

}

}

#endif /* CORESTRATEGIES_H_ */
