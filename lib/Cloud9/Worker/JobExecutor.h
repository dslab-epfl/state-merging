/*
 * JobExecutor.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef JOBEXECUTOR_H_
#define JOBEXECUTOR_H_

#include "klee/Interpreter.h"

#include "cloud9/ExecutionTree.h"

#include "ExplorationJob.h"

namespace cloud9 {

namespace worker {

/*
 * Encapsulates a sequential symbolic execution engine.
 */
class JobExecutor {
private:
	klee::Interpreter *interpreter;

	ExplorationJob *currentJob;

	/*
	 * Returns the next node to be explored
	 */
	WorkerTree::Node *getNextNode();

	void exploreNode(WorkerTree::Node *node);
public:
	JobExecutor();
	virtual ~JobExecutor();

	ExplorationJob *getCurrentJob() { return currentJob; }

	void executeJob(ExplorationJob *job);
};

}
}

#endif /* JOBEXECUTOR_H_ */
