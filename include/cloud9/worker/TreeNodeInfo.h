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

class ExecutionTrace;
class SymbolicState;
class ExecutionJob;

class TreeNodeInfo {
	friend class JobManager;
private:
	SymbolicState *symState;
	ExecutionJob *job;
	ExecutionTrace trace;
public:
	TreeNodeInfo() : symState(NULL), job(NULL) {};

	virtual ~TreeNodeInfo() {};

	SymbolicState* getSymbolicState() const { return symState; }
	ExecutionJob* getJob() const { return job; }

	const ExecutionTrace &getTrace() const { return trace; }
};

#define WORKER_LAYER_COUNT			4

#define WORKER_LAYER_JOBS			0
#define WORKER_LAYER_STATES			1
#define WORKER_LAYER_STATISTICS		2
#define WORKER_LAYER_BREAKPOINTS	3

typedef ExecutionTree<TreeNodeInfo, WORKER_LAYER_COUNT, 2> WorkerTree; // Single layered, binary tree

}

}

#endif /* TREENODEINFO_H_ */
