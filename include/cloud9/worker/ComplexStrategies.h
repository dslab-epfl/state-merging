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

class ComposedStrategy: public JobSelectionStrategy {
protected:
	typedef std::vector<JobSelectionStrategy*> strat_vector;
	strat_vector underlying;
public:
	ComposedStrategy(std::vector<JobSelectionStrategy*> &_underlying);
	virtual ~ComposedStrategy();

	virtual void onJobAdded(ExecutionJob *job);
	virtual void onRemovingJob(ExecutionJob *job);

	virtual void onStateActivated(SymbolicState *state);
	virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
	virtual void onStateDeactivated(SymbolicState *state);
};

class TimeMultiplexedStrategy: public ComposedStrategy {
private:
	unsigned int position;
public:
	TimeMultiplexedStrategy(std::vector<JobSelectionStrategy*> strategies);
	virtual ~TimeMultiplexedStrategy();

	virtual ExecutionJob* onNextJobSelection();
};

#if 0

class SpaceMultiplexedStrategy: public BasicStrategy {

};

#endif

}

}




#endif /* COMPLEXSTRATEGIES_H_ */
