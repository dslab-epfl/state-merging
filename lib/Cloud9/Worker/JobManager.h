/*
 * JobManager.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef JOBMANAGER_H_
#define JOBMANAGER_H_

#include "ExplorationJob.h"
#include "JobExecutor.h"

#include <list>
#include <set>

namespace cloud9 {

namespace worker {


class JobManager {
private:
	WorkerTree* tree;
	JobExecutor *executor;

	std::set<ExplorationJob*> waitingPool;
	std::set<ExplorationJob*> executingPool;

	/*
	 *
	 */
	void finalizeJob(ExplorationJob* job);
public:
	JobManager(WorkerTree *tree) {};
	virtual ~JobManager();

	WorkerTree *getTree() { return tree; }

	void submitJob(ExplorationJob* job);

	ExplorationJob *createJob(WorkerTree::Node *root);

	void processJobs();


};

}
}

#endif /* JOBMANAGER_H_ */
