/*
 * JobManagerBehaviors.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/worker/JobManagerBehaviors.h"
#include "cloud9/worker/ExplorationJob.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/Logger.h"

#include "klee/Internal/ADT/RNG.h"

namespace cloud9 {

namespace worker {

void RandomSelectionHandler::onJobEnqueued(ExplorationJob *job) {
	jobs.push_back(job);
}

void RandomSelectionHandler::onJobsExported() {
	int i = 0;
	int removed = 0;

	while (i < jobs.size()) {
		ExplorationJob *job = jobs[i];

		if (job->isFinished()) {
			jobs[i] = jobs.back();
			jobs.pop_back();
			removed++;
		} else {
			i++;
		}

	}

	CLOUD9_DEBUG("Removed " << removed << " jobs after export");
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
