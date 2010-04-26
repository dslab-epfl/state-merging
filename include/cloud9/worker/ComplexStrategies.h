/*
 * ComplexStrategies.h
 *
 *  Created on: Apr 24, 2010
 *      Author: stefan
 */

#ifndef COMPLEXSTRATEGIES_H_
#define COMPLEXSTRATEGIES_H_


#include "cloud9/worker/CoreStrategies.h"

namespace cloud9 {

namespace worker {

class BatchingStrategy: public JobSelectionStrategy {
private:
	JobSelectionStrategy *underlying;
	ExecutionJob *currentJob;
public:
	BatchingStrategy(JobSelectionStrategy *_underlying);

	virtual ~BatchingStrategy();

	virtual void onJobAdded(ExecutionJob *job);
	virtual ExecutionJob* onNextJobSelection();
	virtual void onRemovingJob(ExecutionJob *job);
	virtual void onRemovingJobs();

	virtual void onStateActivated(SymbolicState *state);
	virtual void onStateUpdated(SymbolicState *state);
	virtual void onStateDeactivated(SymbolicState *state);
};

#if 0

class TimeMultiplexedStrategy: public BasicStrategy {

};

class SpaceMultiplexedStrategy: public BasicStrategy {

};

#endif

}

}




#endif /* COMPLEXSTRATEGIES_H_ */
