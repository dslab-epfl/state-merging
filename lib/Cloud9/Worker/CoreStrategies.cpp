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
#include "klee/Executor.h"
#include "klee/ExecutionState.h"
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

////////////////////////////////////////////////////////////////////////////////
// Random Path Strategy
////////////////////////////////////////////////////////////////////////////////

ExecutionJob* RandomPathStrategy::onNextJobSelection() {
	return selectRandomPathJob(tree);
}

////////////////////////////////////////////////////////////////////////////////
// Klee Imported Strategy
////////////////////////////////////////////////////////////////////////////////

KleeStrategy::KleeStrategy(WorkerTree *_tree) : tree(_tree), searcher(NULL) {

}

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

	if ((**node).getJob() != NULL) {
	  return (**node).getJob();
	}

	WorkerTree::Node *jobNode = tree->selectRandomLeaf(WORKER_LAYER_JOBS, node, theRNG);
	ExecutionJob *job = (**jobNode).getJob();

	assert(job != NULL);

	return job;
}


////////////////////////////////////////////////////////////////////////////////
// Weighted Random Strategy
////////////////////////////////////////////////////////////////////////////////

WeightedRandomStrategy::WeightedRandomStrategy(WeightType _type,
		WorkerTree *_tree, SymbolicEngine *_engine) : KleeStrategy(_tree) {

	klee::Executor *executor = dynamic_cast<klee::Executor*>(_engine); // XXX I should be ashamed of this
	searcher = new WeightedRandomSearcher(*executor,
			static_cast<klee::WeightedRandomSearcher::WeightType>(_type)); // XXX This is truly ugly

}

WeightedRandomStrategy::~WeightedRandomStrategy() {
	delete searcher;
}


}

}
