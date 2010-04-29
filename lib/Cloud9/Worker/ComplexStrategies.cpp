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

void BatchingStrategy::onStateActivated(SymbolicState *state) {
	underlying->onStateActivated(state);
}

void BatchingStrategy::onStateUpdated(SymbolicState *state) {
	underlying->onStateUpdated(state);
}

void BatchingStrategy::onStateDeactivated(SymbolicState *state) {
	underlying->onStateDeactivated(state);
}

////////////////////////////////////////////////////////////////////////////////
// Composed Strategy
////////////////////////////////////////////////////////////////////////////////

ComposedStrategy::ComposedStrategy(std::vector<JobSelectionStrategy*> &_underlying) : underlying(_underlying) {
	assert(underlying.size() > 0);
}

ComposedStrategy::~ComposedStrategy() {

}


void ComposedStrategy::onJobAdded(ExecutionJob *job) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		JobSelectionStrategy *strat = *it;
		strat->onJobAdded(job);
	}
}

void ComposedStrategy::onRemovingJob(ExecutionJob *job) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		JobSelectionStrategy *strat = *it;
		strat->onRemovingJob(job);
	}
}

void ComposedStrategy::onStateActivated(SymbolicState *state) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		JobSelectionStrategy *strat = *it;
		strat->onStateActivated(state);
	}
}

void ComposedStrategy::onStateUpdated(SymbolicState *state) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		JobSelectionStrategy *strat = *it;
		strat->onStateUpdated(state);
	}
}

void ComposedStrategy::onStateDeactivated(SymbolicState *state) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		JobSelectionStrategy *strat = *it;
		strat->onStateDeactivated(state);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Time Multiplexed Strategy
////////////////////////////////////////////////////////////////////////////////


TimeMultiplexedStrategy::TimeMultiplexedStrategy(std::vector<JobSelectionStrategy*> strategies) :
	ComposedStrategy(strategies), position(0) {

}

TimeMultiplexedStrategy::~TimeMultiplexedStrategy() {

}

ExecutionJob* TimeMultiplexedStrategy::onNextJobSelection() {
	ExecutionJob *job = underlying[position]->onNextJobSelection();

	position = (position + 1) % underlying.size();

	return job;
}

}

}
