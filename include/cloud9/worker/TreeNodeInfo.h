/*
 * TreeNodeInfo.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef TREENODEINFO_H_
#define TREENODEINFO_H_

#include "cloud9/ExecutionTree.h"

#include "klee/ExecutionState.h"

namespace cloud9 {

namespace worker {

class ExplorationJob;

class TreeNodeInfo {
	friend class JobExecutor;
	friend class JobManager;
private:
	klee::ExecutionState *symState;

	int jobCount;
	bool stats;
	ExplorationJob *job;
public:
	TreeNodeInfo() : symState(NULL), jobCount(0), stats(false), job(NULL) {};
	virtual ~TreeNodeInfo() {};

	klee::ExecutionState* getSymbolicState() const { return symState; }
	int getJobCount() const { return jobCount; }

	bool isStats() const { return stats; }

	ExplorationJob *getJob() { return job; }
};

typedef ExecutionTree<TreeNodeInfo> WorkerTree;

}

}

#endif /* TREENODEINFO_H_ */
