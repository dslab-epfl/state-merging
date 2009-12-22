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
private:
	klee::ExecutionState *symState;
	ExplorationJob *job;
public:
	TreeNodeInfo();
	virtual ~TreeNodeInfo();

	klee::ExecutionState* getSymbolicState() { return symState; }
	ExplorationJob *getJob() { return job; }
};

typedef ExecutionTree<TreeNodeInfo> WorkerTree;

}

}

#endif /* TREENODEINFO_H_ */
