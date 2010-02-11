/*
 * ExplorationJob.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef EXPLORATIONJOB_H_
#define EXPLORATIONJOB_H_



#include "cloud9/ExecutionTree.h"

#include "cloud9/worker/TreeNodeInfo.h"

#include <list>
#include <vector>
#include <set>

namespace cloud9 {

namespace worker {

class ExplorationJob {
	friend class JobExecutor;
	friend class JobManager;
public:
	struct JobCompare {
	private:
		WorkerTree::NodeBreadthCompare nodeCompare;
	public:
		bool operator() (const ExplorationJob *a, const ExplorationJob *b) {
			return nodeCompare(a->jobRoot.get(), b->jobRoot.get());
		}
	};

	typedef std::set<WorkerTree::Node*, WorkerTree::NodeBreadthCompare> frontier_t;
private:

	int size;
	int depth;
	int operations;

	bool started;
	bool finished;

	bool foreign;

	WorkerTree::NodePin jobRoot;
	frontier_t frontier;

	void addToFrontier(WorkerTree::Node *node);
	void removeFromFrontier(WorkerTree::Node *node);

	ExplorationJob(WorkerTree::Node *jobRoot, bool foreign);

public:
	virtual ~ExplorationJob();

	int getSize() { return size; }
	int getDepth() { return depth; }
	int getOperations() { return operations; }

	frontier_t &getFrontier() { return frontier; }
	WorkerTree::Node *getJobRoot() { return jobRoot.get(); }

	bool isStarted() const { return started; }
	bool isFinished() const { return finished; }
};

}
}

#endif /* EXPLORATIONJOB_H_ */
