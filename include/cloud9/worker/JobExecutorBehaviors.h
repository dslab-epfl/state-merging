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

class RandomExplorationHandler: public JobExecutor::ExplorationHandler {
public:
	RandomExplorationHandler() {};
	virtual ~RandomExplorationHandler() {};

	virtual void onNextStateQuery(ExplorationJob *job, WorkerTree::Node *&node);
};

}
}


#endif /* JOBEXECUTORBEHAVIORS_H_ */
