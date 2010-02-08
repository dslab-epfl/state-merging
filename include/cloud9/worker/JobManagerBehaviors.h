/*
 * JobManagerBehaviors.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef JOBMANAGERBEHAVIORS_H_
#define JOBMANAGERBEHAVIORS_H_

#include "cloud9/worker/JobManager.h"

#include <vector>

namespace cloud9 {

namespace worker {

class ExplorationJob;

class RandomSelectionHandler: public JobManager::SelectionHandler {
private:
	std::vector<ExplorationJob*> jobs;
public:
	RandomSelectionHandler() {};
	virtual ~RandomSelectionHandler() {};

	virtual void onJobEnqueued(ExplorationJob *job);
	virtual void onJobsExported();

	virtual void onNextJobSelection(ExplorationJob *&job);
};

class KleeSelectionHandler: public JobManager::SelectionHandler {
public:
	KleeSelectionHandler() {};
	virtual ~KleeSelectionHandler() {};

	virtual void onJobEnqueued(ExplorationJob *job);
	virtual void onJobsExported();

	virtual void onNextJobSelection(ExplorationJob *&job);
};

}

}

#endif /* JOBMANAGERBEHAVIORS_H_ */
