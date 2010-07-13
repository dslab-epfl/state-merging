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
