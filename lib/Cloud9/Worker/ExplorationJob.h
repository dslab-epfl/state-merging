/*
 * ExplorationJob.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef EXPLORATIONJOB_H_
#define EXPLORATIONJOB_H_



#include "cloud9/ExecutionTree.h"

#include "TreeNodeInfo.h"

#include <list>
#include <vector>
#include <set>

namespace cloud9 {

namespace worker {

class ExplorationJob {
public:
	struct JobCompare {
	private:
		WorkerTree::NodeCompare nodeCompare;
	public:
		bool operator() (const ExplorationJob *a, const ExplorationJob *b) {
			return nodeCompare(a->jobRoot, b->jobRoot);
		}
	};
private:
	friend class JobExecutor;
	friend class JobManager;

	typedef std::set<WorkerTree::Node*, WorkerTree::NodeCompare> Frontier;

	WorkerTree *tree;

	int size;
	int depth;

	bool started;
	bool finished;

	WorkerTree::Node *jobRoot;
	Frontier frontier;

	ExplorationJob *parent;
	std::vector<ExplorationJob*> children;

public:
	ExplorationJob();
	virtual ~ExplorationJob();

	int getSize() { return size; }
	int getDepth() { return depth; }

	bool isStarted() { return started; }
	bool isFinished() { return finished; }
};

}
}

#endif /* EXPLORATIONJOB_H_ */
