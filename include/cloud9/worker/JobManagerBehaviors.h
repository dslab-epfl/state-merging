/*
 * JobManagerBehaviors.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef JOBMANAGERBEHAVIORS_H_
#define JOBMANAGERBEHAVIORS_H_

#include "cloud9/worker/JobManager.h"

namespace cloud9 {

namespace worker {

class ExplorationJob;

class RandomSelectionHandler: public JobManager::SelectionHandler {
public:
	RandomSelectionHandler() {};
	virtual ~RandomSelectionHandler() {};
public:
	virtual void onJobEnqueued(ExplorationJob *job);
	virtual void onJobExecutionStarted(ExplorationJob *job);
	virtual void onJobExecutionFinished(ExplorationJob *job);

	virtual void onNextJobSelection(ExplorationJob *&job);
};

}

}

#endif /* JOBMANAGERBEHAVIORS_H_ */
