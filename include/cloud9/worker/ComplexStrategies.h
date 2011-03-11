/*
 * ComplexStrategies.h
 *
 *  Created on: Apr 24, 2010
 *      Author: stefan
 */

#ifndef COMPLEXSTRATEGIES_H_
#define COMPLEXSTRATEGIES_H_


#include "cloud9/worker/CoreStrategies.h"

#include <vector>

namespace cloud9 {

namespace worker {

class ComposedStrategy: public StateSelectionStrategy {
protected:
	typedef std::vector<StateSelectionStrategy*> strat_vector;
	strat_vector underlying;
public:
	ComposedStrategy(std::vector<StateSelectionStrategy*> &_underlying);
	virtual ~ComposedStrategy();

	virtual void onStateActivated(SymbolicState *state);
	virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
	virtual void onStateDeactivated(SymbolicState *state);
};

class TimeMultiplexedStrategy: public ComposedStrategy {
private:
	unsigned int position;
public:
	TimeMultiplexedStrategy(std::vector<StateSelectionStrategy*> strategies);
	virtual ~TimeMultiplexedStrategy();

	virtual SymbolicState* onNextStateSelection();
};

}

}




#endif /* COMPLEXSTRATEGIES_H_ */
