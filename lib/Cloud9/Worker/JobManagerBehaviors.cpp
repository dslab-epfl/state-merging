/*
 * JobManagerBehaviors.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/worker/JobManagerBehaviors.h"
#include "cloud9/worker/ExplorationJob.h"
#include "cloud9/worker/WorkerCommon.h"

#include "klee/Internal/ADT/RNG.h"

namespace cloud9 {

namespace worker {

void RandomSelectionHandler::onJobEnqueued(ExplorationJob *job) {
	jobs.push_back(job);
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

}

}
