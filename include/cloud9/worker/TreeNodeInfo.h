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

class WorkerNodeInfo {
	friend class JobManager;
	friend class SymbolicState;
	friend class ExecutionJob;
private:
	SymbolicState *symState;
	ExecutionJob *job;
	ExecutionTrace trace;
	int forkReason;
public:
	WorkerNodeInfo() : symState(NULL), job(NULL), forkReason(0) { }

	virtual ~WorkerNodeInfo() { }

	SymbolicState* getSymbolicState() const { return symState; }
	ExecutionJob* getJob() const { return job; }
	int getForkReason() const { return forkReason; }

	const ExecutionTrace &getTrace() const { return trace; }
};

#define WORKER_LAYER_COUNT			4

#define WORKER_LAYER_JOBS			0
#define WORKER_LAYER_STATES			1
#define WORKER_LAYER_STATISTICS		2
#define WORKER_LAYER_BREAKPOINTS	3

typedef ExecutionTree<WorkerNodeInfo, WORKER_LAYER_COUNT, 2> WorkerTree; // Four layered, binary tree


class CompressedNodeInfo {
  friend class JobManager;
  friend class SymbolicState;
private:
  SymbolicState *symState;
public:
  CompressedNodeInfo() : symState(NULL) { }

  SymbolicState *getSymbolicState() const { return symState; }
};

#define COMPRESSED_LAYER_COUNT      2

#define COMPRESSED_LAYER_STATES     0
#define COMPRESSED_LAYER_ACTIVE     1

typedef ExecutionTree<CompressedNodeInfo, COMPRESSED_LAYER_COUNT, 2> CompressedTree; // Single layered, complete binary tree

}

}

#endif /* TREENODEINFO_H_ */
