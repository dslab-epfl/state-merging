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
		WorkerTree::NodeCompare nodeCompare;
	public:
		bool operator() (const ExplorationJob *a, const ExplorationJob *b) {
			return nodeCompare(a->jobRoot, b->jobRoot);
		}
	};

	typedef std::set<WorkerTree::Node*, WorkerTree::NodeCompare> frontier_t;
private:

	int size;
	int depth;

	bool started;
	bool finished;

	WorkerTree::Node *jobRoot;
	frontier_t frontier;

	ExplorationJob *parent;
	std::vector<ExplorationJob*> children;

	void addToFrontier(WorkerTree::Node *node);
	void removeFromFrontier(WorkerTree::Node *node);

	ExplorationJob(ExplorationJob *parent, WorkerTree::Node *jobRoot);

public:
	virtual ~ExplorationJob();

	int getSize() { return size; }
	int getDepth() { return depth; }

	frontier_t &getFrontier() { return frontier; }
	WorkerTree::Node *getJobRoot() { return jobRoot; }

	bool isStarted() const { return started; }
	bool isFinished() const { return finished; }
};

}
}

#endif /* EXPLORATIONJOB_H_ */
