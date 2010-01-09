/*
 * JobExecutorBehaviors.h
 *
 *  Created on: Jan 4, 2010
 *      Author: stefan
 */

#ifndef JOBEXECUTORBEHAVIORS_H_
#define JOBEXECUTORBEHAVIORS_H_

#include "cloud9/worker/JobExecutor.h"


namespace cloud9 {

namespace worker {

class UnlimitedSizingHandler: public JobExecutor::SizingHandler {
public:
	UnlimitedSizingHandler() {};
	virtual ~UnlimitedSizingHandler() {};

	virtual void onTerminationQuery(ExplorationJob *job, bool &term) {
		term = false; // Never ask to terminate
	}
};

class FixedSizingHandler: public JobExecutor::SizingHandler {
private:
	int maxSize;
	int maxDepth;
	int maxOperations;
public:
	FixedSizingHandler(int maxS = 0, int maxD = 0, int maxO = 0)
		: maxSize(maxS), maxDepth(maxD), maxOperations(maxO) {

	};

	int getMaxSize() const { return maxSize; }
	int getMaxDepth() const { return maxDepth; }
	int getMaxOperations() const { return maxOperations; }

	virtual ~FixedSizingHandler() {};

	virtual void onTerminationQuery(ExplorationJob *job, bool &term) {
		term = false;

		if (maxSize > 0 && job->getSize() >= maxSize) {
			term = true;
		}

		if (maxDepth > 0 && job->getDepth() >= maxDepth) {
			term = true;
		}

		if (maxOperations > 0 && job->getOperations() >= maxOperations) {
			term = true;
		}
	}
};

class RandomExplorationHandler: public JobExecutor::ExplorationHandler {
public:
	RandomExplorationHandler() {};
	virtual ~RandomExplorationHandler() {};

	virtual void onNextStateQuery(ExplorationJob *job, WorkerTree::Node *&node);
};

}
}


#endif /* JOBEXECUTORBEHAVIORS_H_ */
