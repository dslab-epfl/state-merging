/*
 * TreeNodeInfo.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef TREENODEINFO_H_
#define TREENODEINFO_H_

#include "cloud9/ExecutionTree.h"
#include "cloud9/worker/ExecutionTrace.h"

#include <vector>

namespace klee {
class ExecutionState;
}

namespace cloud9 {

namespace worker {

class ExplorationJob;
class ExecutionTrace;

class TreeNodeInfo {
	friend class JobExecutor;
	friend class JobManager;
private:
	klee::ExecutionState *symState;
	ExecutionTrace trace;

	bool stats;
	bool breakpoint;
	ExplorationJob *job;
public:
	TreeNodeInfo() : symState(NULL), jobCount(0), stats(false),
		breakpoint(false), job(NULL) {};

	virtual ~TreeNodeInfo() {};

	klee::ExecutionState* getSymbolicState() const { return symState; }

	bool isStats() const { return stats; }

	bool isBreakpoint() const { return breakpoint; }

	ExplorationJob *getJob() const { return job; }

	const ExecutionTrace &getTrace() const { return trace; }
};

#define WORKER_LAYER_COUNT			3

#define WORKER_LAYER_JOBS			0
#define WORKER_LAYER_STATES			1
#define WORKER_LAYER_BREAKPOINTS	2

typedef ExecutionTree<TreeNodeInfo, WORKER_LAYER_COUNT, 2> WorkerTree; // Single layered, binary tree

}

}

#endif /* TREENODEINFO_H_ */
