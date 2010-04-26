/*
 * ComplexStrategies.cpp
 *
 *  Created on: Apr 24, 2010
 *      Author: stefan
 */

#include "cloud9/worker/ComplexStrategies.h"

namespace cloud9 {

namespace worker {


////////////////////////////////////////////////////////////////////////////////
// Batching Strategy
////////////////////////////////////////////////////////////////////////////////

BatchingStrategy::BatchingStrategy(JobSelectionStrategy *_underlying) :
	underlying(_underlying), currentJob(NULL) {

}

BatchingStrategy::~BatchingStrategy() {

}

void BatchingStrategy::onJobAdded(ExecutionJob *job) {
	underlying->onJobAdded(job);
}

ExecutionJob* BatchingStrategy::onNextJobSelection() {
	if (currentJob != NULL)
		return currentJob;

	currentJob = underlying->onNextJobSelection();

	return currentJob;
}

void BatchingStrategy::onRemovingJob(ExecutionJob *job) {
	if (job == currentJob)
		currentJob = NULL;

	underlying->onRemovingJob(job);
}

void BatchingStrategy::onRemovingJobs() {
	// Play it safe here
	currentJob = NULL;

	underlying->onRemovingJobs();
}

void BatchingStrategy::onStateActivated(SymbolicState *state) {
	underlying->onStateActivated(state);
}

void BatchingStrategy::onStateUpdated(SymbolicState *state) {
	underlying->onStateUpdated(state);
}

void BatchingStrategy::onStateDeactivated(SymbolicState *state) {
	underlying->onStateDeactivated(state);
}

}

}
