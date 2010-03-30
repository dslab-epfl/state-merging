/*
 * JobManagerBehaviors.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/worker/JobManagerBehaviors.h"
#include "cloud9/worker/ExplorationJob.h"
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

static ExplorationJob *selectRandomPathJob(WorkerTree *tree) {
  WorkerTree::Node *crtNode = tree->getRoot();
  
  while ((**crtNode).getJob() == NULL) {
    int index = (int)theRNG.getBool();
    WorkerTree::Node *child = crtNode->getChild(index);
    
    if (child == NULL || (**child).getJobCount() == 0)
      child = crtNode->getChild(1 - index);
    
    assert(child && (**child).getJobCount() > 0);
    
    crtNode = child;
  }
  
  return (**crtNode).getJob();
}

////////////////////////////////////////////////////////////////////////////////
// KLEE Selection Handler
////////////////////////////////////////////////////////////////////////////////

class RandomPathSearcher: public Searcher {
private:
	WorkerTree *tree;
public:
	RandomPathSearcher(WorkerTree *t) :
		tree(t) {

	}

	~RandomPathSearcher() {};

	ExecutionState &selectState() {
		ExplorationJob *job = selectRandomPathJob(tree);

		return *(**(job->getJobRoot())).getSymbolicState();
	}

	void update(ExecutionState *current,
			const std::set<ExecutionState*> &addedStates, const std::set<
					ExecutionState*> &removedStates) {
		// Do nothing
	}

	bool empty() {
		WorkerTree::Node *root = tree->getRoot();

		return (**root).getJobCount() == 0;
	}
	void printName(std::ostream &os) {
		os << "RandomPathSearcher\n";
	}
};

KleeSelectionHandler::KleeSelectionHandler(SymbolicEngine *e) {
	// TODO
}

KleeSelectionHandler::~KleeSelectionHandler() {
	// TODO
}

void KleeSelectionHandler::onJobEnqueued(ExplorationJob *job) {
	// TODO
}

void KleeSelectionHandler::onJobsExported() {
	// TODO
}

void KleeSelectionHandler::onNextJobSelection(ExplorationJob *&job) {
	// TODO
}

////////////////////////////////////////////////////////////////////////////////
// Random Selection Handler
////////////////////////////////////////////////////////////////////////////////

void RandomSelectionHandler::onJobEnqueued(ExplorationJob *job) {
	jobs.push_back(job);
}

void RandomSelectionHandler::onJobsExported() {
	unsigned i = 0;

	while (i < jobs.size()) {
		ExplorationJob *job = jobs[i];

		if (job->isFinished()) {
			jobs[i] = jobs.back();
			jobs.pop_back();
		} else {
			i++;
		}

	}

	//CLOUD9_DEBUG("Removed " << removed << " jobs after export");
}

void RandomSelectionHandler::onNextJobSelection(ExplorationJob *&job) {
	if (jobs.empty()) {
		job = NULL;
		return;
	}

	int index = klee::theRNG.getInt32() % jobs.size();

	job = jobs[index];
	jobs[index] = jobs.back();

	jobs.pop_back();
}

////////////////////////////////////////////////////////////////////////////////
// Random Path Selection Handler
////////////////////////////////////////////////////////////////////////////////


void RandomPathSelectionHandler::onNextJobSelection(ExplorationJob *&job) {
  WorkerTree::Node *crtNode = tree->getRoot();
  
  if ((**crtNode).getJobCount() == 0) {
    job = NULL;
    return;
  }
  
  job = selectRandomPathJob(tree);
}

void WeightedRandomSelectionHandler::onJobEnqueued(ExplorationJob *job) {
  ExecutionState *es = (**(job->getJobRoot())).getSymbolicState();
  
  if(es == NULL) {
	  CLOUD9_DEBUG("Inserting null job");
    toReplayJobs.push_back(job);
    return;
  }
    
  CLOUD9_DEBUG("Inserting " << es);
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
  
  WorkerTree::Node *crtNode = tree->getRoot();
  if((**crtNode).getJobCount() == 0) {
    //if the root of the tree has no more jobs, we just return NULL
    //the worker will loop-wait until job != NULL
    job = NULL;
    return;
  }
  
  job = selectWeightedRandomJob(tree);
}


}

}
