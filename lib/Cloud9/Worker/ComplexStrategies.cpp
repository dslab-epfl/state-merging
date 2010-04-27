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

////////////////////////////////////////////////////////////////////////////////
// Composed Strategy
////////////////////////////////////////////////////////////////////////////////

ComposedStrategy::ComposedStrategy(std::vector<JobSelectionStrategy*> &_underlying) : underlying(_underlying) {
	assert(underlying.size() > 0);
}

ComposedStrategy::~ComposedStrategy() {

}

#define FOR_EACH_STRATEGY(strat) \
		strat_vector::iterator __it; JobSelectionStrategy *strat; \
		for (__it = underlying.begin(), strat = *__it; __it != underlying.end(); ++__it, strat = *__it)


void ComposedStrategy::onJobAdded(ExecutionJob *job) {
	FOR_EACH_STRATEGY(strat) {
		strat->onJobAdded(job);
	}
}

void ComposedStrategy::onRemovingJob(ExecutionJob *job) {
	FOR_EACH_STRATEGY(strat) {
		strat->onRemovingJob(job);
	}
}

void ComposedStrategy::onRemovingJobs() {
	FOR_EACH_STRATEGY(strat) {
		strat->onRemovingJobs();
	}
}

void ComposedStrategy::onStateActivated(SymbolicState *state) {
	FOR_EACH_STRATEGY(strat) {
		strat->onStateActivated(state);
	}
}

void ComposedStrategy::onStateUpdated(SymbolicState *state) {
	FOR_EACH_STRATEGY(strat) {
		strat->onStateUpdated(state);
	}
}

void ComposedStrategy::onStateDeactivated(SymbolicState *state) {
	FOR_EACH_STRATEGY(strat) {
		strat->onStateDeactivated(state);
	}
}

#undef FOR_EACH_STRATEGY

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
