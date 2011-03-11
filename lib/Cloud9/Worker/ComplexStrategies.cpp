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

ComposedStrategy::ComposedStrategy(std::vector<StateSelectionStrategy*> &_underlying) : underlying(_underlying) {
	assert(underlying.size() > 0);
}

ComposedStrategy::~ComposedStrategy() {

}

void ComposedStrategy::onStateActivated(SymbolicState *state) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		StateSelectionStrategy *strat = *it;
		strat->onStateActivated(state);
	}
}

void ComposedStrategy::onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		StateSelectionStrategy *strat = *it;
		strat->onStateUpdated(state, oldNode);
	}
}

void ComposedStrategy::onStateDeactivated(SymbolicState *state) {
	for (strat_vector::iterator it = underlying.begin(); it != underlying.end(); it++) {
		StateSelectionStrategy *strat = *it;
		strat->onStateDeactivated(state);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Time Multiplexed Strategy
////////////////////////////////////////////////////////////////////////////////


TimeMultiplexedStrategy::TimeMultiplexedStrategy(std::vector<StateSelectionStrategy*> strategies) :
	ComposedStrategy(strategies), position(0) {

}

TimeMultiplexedStrategy::~TimeMultiplexedStrategy() {

}

SymbolicState* TimeMultiplexedStrategy::onNextStateSelection() {
  SymbolicState *state = underlying[position]->onNextStateSelection();

  position = (position + 1) % underlying.size();

  return state;
}

}

}
