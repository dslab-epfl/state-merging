/*
 * JobManagerBehaviors.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/worker/CoreStrategies.h"
#include "cloud9/worker/TreeObjects.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/SymbolicEngine.h"
#include "cloud9/Logger.h"

#include "klee/Internal/ADT/RNG.h"
#include "klee/Searcher.h"
#include "klee/Statistics.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
///XXX: ugly, remove this dependency
#include "../../lib/Core/CallPathManager.h"
#include "../../lib/Core/StatsTracker.h"
#include "../../Core/CoreStats.h"


using namespace klee;

namespace cloud9 {

namespace worker {

static ExecutionJob *selectRandomPathJob(WorkerTree *tree) {
	WorkerTree::Node *node = tree->selectRandomLeaf(WORKER_LAYER_JOBS, tree->getRoot(), theRNG);
	ExecutionJob *job = (**node).getJob();

	assert(job != NULL || node == tree->getRoot());

	return job;
}

////////////////////////////////////////////////////////////////////////////////
// Random Strategy
////////////////////////////////////////////////////////////////////////////////

void RandomStrategy::onJobAdded(ExecutionJob *job) {
	indices[job] = jobs.size();
	jobs.push_back(job);
}

ExecutionJob* RandomStrategy::onNextJobSelection() {
	if (jobs.empty()) {
		return NULL;
	}

	int index = klee::theRNG.getInt32() % jobs.size();

	return jobs[index];
}

void RandomStrategy::onRemovingJob(ExecutionJob *job) {
	unsigned i = indices[job];
	assert(job->isRemoving());

	jobs[i] = jobs.back();
	indices[jobs[i]] = i;
	jobs.pop_back();
}

void RandomStrategy::onRemovingJobs() {
	unsigned i = 0;

	while (i < jobs.size()) {
		ExecutionJob *job = jobs[i];

		if (job->isRemoving()) {
			jobs[i] = jobs.back();
			indices[jobs[i]] = i;
			jobs.pop_back();
		} else {
			i++;
		}

	}

}

////////////////////////////////////////////////////////////////////////////////
// Random Path Strategy
////////////////////////////////////////////////////////////////////////////////

ExecutionJob* RandomPathStrategy::onNextJobSelection() {
	return selectRandomPathJob(tree);
}

////////////////////////////////////////////////////////////////////////////////
// Klee Imported Strategy
////////////////////////////////////////////////////////////////////////////////

KleeStrategy::KleeStrategy(WorkerTree *_tree, klee::Searcher *_searcher) :
		tree(_tree), searcher(_searcher) {

}

KleeStrategy::~KleeStrategy() {

}

void KleeStrategy::onStateActivated(SymbolicState *state) {
	std::set<klee::ExecutionState*> added;
	added.insert(state->getKleeState());

	searcher->update(NULL, added, std::set<klee::ExecutionState*>());
}

void KleeStrategy::onStateUpdated(SymbolicState *state) {
	searcher->update(state->getKleeState(),
			std::set<klee::ExecutionState*>(), std::set<klee::ExecutionState*>());
}

void KleeStrategy::onStateDeactivated(SymbolicState *state) {
	std::set<klee::ExecutionState*> removed;
	removed.insert(state->getKleeState());

	searcher->update(NULL, std::set<klee::ExecutionState*>(), removed);
}

ExecutionJob* KleeStrategy::onNextJobSelection() {
	if (searcher->empty())
		return NULL;

	klee::ExecutionState &kState = searcher->selectState();
	SymbolicState *state = kState.getCloud9State();
	WorkerTree::Node *node = state->getNode().get();

	assert(node->layerExists(WORKER_LAYER_JOBS));

	WorkerTree::Node *jobNode = tree->selectRandomLeaf(WORKER_LAYER_JOBS, node, theRNG);
	ExecutionJob *job = (**jobNode).getJob();

	assert(job != NULL);

	return job;
}

#if 0

////////////////////////////////////////////////////////////////////////////////
// Weighted Random Selection Handler
////////////////////////////////////////////////////////////////////////////////

void WeightedRandomSelectionHandler::onJobEnqueued(ExplorationJob *job) {
  ExecutionState *es = (**(job->getJobRoot())).getSymbolicState();
  
  if(es == NULL) {
	  //CLOUD9_DEBUG("Inserting null job");
    toReplayJobs.push_back(job);
    return;
  }
    
  //CLOUD9_DEBUG("Inserting " << es);
  states->insert(es, getWeight(es));
  //we also add the job to the job queue
  jobs.push_back(job);
}


void WeightedRandomSelectionHandler::onJobsExported() {
  unsigned i = 0;
  
  while (i < toReplayJobs.size()) {
    ExplorationJob *job = toReplayJobs[i];
    if (job->isFinished()) {
      toReplayJobs[i] = toReplayJobs.back();
      toReplayJobs.pop_back();
    } else {
      i++;
    }
  }
  
  i = 0;
  while (i < jobs.size()) {
    ExplorationJob *job = jobs[i];
    if (job->isFinished()) {
      onJobRemoved(job);
      jobs[i] = jobs.back();
      jobs.pop_back();
    } else {
      i++;
    }
  }
}

//should be called when states are removed as a result of jobs being exported
void WeightedRandomSelectionHandler::onJobRemoved(ExplorationJob *job) {
  ExecutionState *es = (**(job->getJobRoot())).getSymbolicState();
  assert(es != NULL && "job has no symbolic state");
  states->remove(es);
}

  
WeightedRandomSelectionHandler::WeightedRandomSelectionHandler(WeightType _type, WorkerTree* _tree) 
  : states(new klee::DiscretePDF<ExecutionState*>()),
    type(_type), 
    tree(_tree) {
  switch(type) {
  case Depth: 
    updateWeights = false;
    break;
  case InstCount:
  case CPInstCount:
  case QueryCost:
  case MinDistToUncovered:
  case CoveringNew:
    updateWeights = true;
    break;
  default:
    assert(0 && "invalid weight type");
  }
}


WeightedRandomSelectionHandler::~WeightedRandomSelectionHandler() {
  delete states;
}

bool WeightedRandomSelectionHandler::empty() {
  //should we check the toReplayJobs too? - check out callsites for empty()
  return states->empty();
}

double WeightedRandomSelectionHandler::getWeight(ExecutionState *es) {
  switch(type) {
  default:
  case Depth: 
    return es->weight;
  case InstCount: {
    uint64_t count = theStatisticManager->getIndexedValue(stats::instructions,
                                                          es->pc->info->id);
    double inv = 1. / std::max((uint64_t) 1, count);
    return inv * inv;
  }
  case CPInstCount: {
    StackFrame &sf = es->stack.back();
    uint64_t count = sf.callPathNode->statistics.getValue(stats::instructions);
    double inv = 1. / std::max((uint64_t) 1, count);
    return inv;
  }
  case QueryCost:
    return (es->queryCost < .1) ? 1. : 1./es->queryCost;
  case CoveringNew:
  case MinDistToUncovered: {
    uint64_t md2u = computeMinDistToUncovered(es->pc,
                                              es->stack.back().minDistToUncoveredOnReturn);
    
    double invMD2U = 1. / (md2u ? md2u : 10000);
    if (type==CoveringNew) {
      double invCovNew = 0.;
      if (es->instsSinceCovNew)
        invCovNew = 1. / std::max(1, (int) es->instsSinceCovNew - 1000);
      return (invCovNew * invCovNew + invMD2U * invMD2U);
    } else {
      return invMD2U * invMD2U;
    }
  }
  }
}

ExplorationJob * WeightedRandomSelectionHandler::selectWeightedRandomJob(WorkerTree *tree) {

  //choose between jobs to be replayed and already expanded jobs, with
  //a CLOUD9_CHOOSE_NEW_JOBS probability for expanded jobs
  if(states->empty() || (klee::theRNG.getDouble() > CLOUD9_CHOOSE_NEW_JOBS &&
     toReplayJobs.size() > 0)) {
    int index = klee::theRNG.getInt32() % toReplayJobs.size();
    //CLOUD9_DEBUG("index = " << index);
    ExplorationJob *job = toReplayJobs[index];
    toReplayJobs[index] = toReplayJobs.back();
    toReplayJobs.pop_back();
    return job;
  } else {
    ExecutionState *es = states->choose(theRNG.getDoubleL());
    assert(es != NULL && "job has no symbolic state");
    //do we need to call update anymore? normally we should update after each executed instruction
    //to use the coverage optimized searcher efficiently
    //CLOUD9_DEBUG("\n updating " << es );
    //if (es && updateWeights)
    //states->update(es, getWeight(es));

    states->remove(es);
    const WorkerTree::Node *node = es->getWorkerNode().get();
    ExplorationJob *job = (**node).getJob();

    bool found = false;

	for (std::vector<ExplorationJob*>::iterator it = jobs.begin(); it
			!= jobs.end(); it++) {
		if (job == *it) {
			jobs.erase(it);
			found = true;
			break;
		}
	}

	assert(found);

    return job;
  }
}

void WeightedRandomSelectionHandler::onNextJobSelection(ExplorationJob *&job) {
  
  WorkerTree::Node *root = tree->getRoot();

  if (root->getCount(WORKER_LAYER_JOBS) == 0 && (**root).getJob() == NULL) {
	  job = NULL;
	  return;
  }
  
  job = selectWeightedRandomJob(tree);
}

#endif


}

}
