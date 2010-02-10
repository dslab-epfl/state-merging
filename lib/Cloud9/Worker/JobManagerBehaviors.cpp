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
	int i = 0;

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

}

}
