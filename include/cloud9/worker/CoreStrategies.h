//===-- CoreStrategies.h ----------------------------------------*- C++ -*-===//
/*
 * CoreStrategies.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef CORESTRATEGIES_H_
#define CORESTRATEGIES_H_

#include "cloud9/worker/JobManager.h"

#include <vector>
#include <map>
#include "klee/ExecutionState.h"
#include "klee/Internal/ADT/DiscretePDF.h"

namespace klee {
class Searcher;
class ExecutionState;
}

namespace cloud9 {

namespace worker {

#define CLOUD9_CHOOSE_NEW_JOBS 0.8


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
	virtual void onRemovingJobs() = 0;

	virtual void onStateActivated(SymbolicState *state) = 0;
	virtual void onStateUpdated(SymbolicState *state) = 0;
	virtual void onStateDeactivated(SymbolicState *state) = 0;
};

class BasicStrategy : public JobSelectionStrategy {
public:
	BasicStrategy() {};
	virtual ~BasicStrategy() {};

public:
	virtual void onJobAdded(ExecutionJob *job) { };
	virtual ExecutionJob* onNextJobSelection() = 0;
	virtual void onRemovingJob(ExecutionJob *job) { };
	virtual void onRemovingJobs() { };

	virtual void onStateActivated(SymbolicState *state) { };
	virtual void onStateUpdated(SymbolicState *state) { };
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
	virtual void onRemovingJobs();
};

class RandomPathStrategy: public BasicStrategy {
private:
	WorkerTree *tree;
public:
	RandomPathStrategy(WorkerTree *t) : tree(t) {};
	virtual ~RandomPathStrategy() {};

	virtual ExecutionJob* onNextJobSelection();
};

class KleeStrategy: public BasicStrategy {
private:
	WorkerTree *tree;
	klee::Searcher *searcher;
public:
	KleeStrategy(WorkerTree *_tree, klee::Searcher *_searcher);
	virtual ~KleeStrategy();

	virtual void onStateActivated(SymbolicState *state);
	virtual void onStateUpdated(SymbolicState *state);
	virtual void onStateDeactivated(SymbolicState *state);

	virtual ExecutionJob* onNextJobSelection();
};

#if 0

class WeightedRandomSelectionHandler: public BasicStrategy {
public:
  enum WeightType {
    Depth,
    QueryCost,
    InstCount,
    CPInstCount,
    MinDistToUncovered,
    CoveringNew
  };
private:
  std::vector<ExecutionJob*> jobs;
  std::vector<ExecutionJob*> toReplayJobs;
  klee::DiscretePDF<klee::ExecutionState*> *states;
  WeightType type;
  WorkerTree *tree;
  bool updateWeights;

  void onJobRemoved(ExplorationJob *job);

  ExplorationJob * selectWeightedRandomJob(WorkerTree *tree);
	bool empty();
	double getWeight(klee::ExecutionState *state);
	void printName(std::ostream &os) {
	  os << "WeightedRandomSelectionHandler::";
	  switch(type) {
	  case Depth              : os << "Depth\n"; return;
	  case QueryCost          : os << "QueryCost\n"; return;
	  case InstCount          : os << "InstCount\n"; return;
	  case CPInstCount        : os << "CPInstCount\n"; return;
	  case MinDistToUncovered : os << "MinDistToUncovered\n"; return;
	  case CoveringNew        : os << "CoveringNew\n"; return;
	  default                 : os << "<unknown type>\n"; return;
	  }};
public:
  WeightedRandomSelectionHandler(WeightType _type, WorkerTree *_tree);
  virtual ~WeightedRandomSelectionHandler();

  virtual void onJobEnqueued(ExplorationJob *job);
  virtual void onJobsExported() ;
  virtual void onNextJobSelection(ExplorationJob *&job);
};
  
class KleeSelectionHandler: public BasicStrategy {
private:
	klee::Searcher *kleeSearcher;
	std::vector<ExplorationJob*> jobs;
public:
	KleeSelectionHandler(SymbolicEngine *e);
	virtual ~KleeSelectionHandler();

	virtual void onJobEnqueued(ExplorationJob *job);
	virtual void onJobsExported();

	virtual void onNextJobSelection(ExplorationJob *&job);
};

#endif

}

}

#endif /* CORESTRATEGIES_H_ */
