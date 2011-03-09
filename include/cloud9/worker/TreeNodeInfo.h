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
#include "klee/ForkTag.h"

#include <vector>

namespace klee {
class ExecutionState;
}

namespace cloud9 {

namespace worker {

class ExecutionTrace;
class SymbolicState;
class ExecutionJob;
class WorkerNodeInfo;

#define WORKER_LAYER_COUNT          6

#define WORKER_LAYER_JOBS           1
#define WORKER_LAYER_STATES         2
#define WORKER_LAYER_STATISTICS     3
#define WORKER_LAYER_BREAKPOINTS    4
#define WORKER_LAYER_MERGED_STATES       5

class WorkerNodeInfo {
	friend class JobManager;
	friend class SymbolicState;
	friend class ExecutionJob;
public:
	typedef TreeNode<WorkerNodeInfo, WORKER_LAYER_COUNT, 2> worker_tree_node_t;
	typedef std::vector<std::pair<std::pair<unsigned long, unsigned long>, worker_tree_node_t::Pin> > merge_points_t;
private:
	SymbolicState *symState;
	ExecutionJob *job;
	ExecutionTrace trace;
	klee::ForkTag forkTag;
	merge_points_t mergePoints;
public:
	WorkerNodeInfo() : symState(NULL), job(NULL), forkTag(klee::KLEE_FORK_DEFAULT) { }

	virtual ~WorkerNodeInfo() { }

	SymbolicState* getSymbolicState() const { return symState; }
	ExecutionJob* getJob() const { return job; }
	klee::ForkTag getForkTag() const { return forkTag; }

	const ExecutionTrace &getTrace() const { return trace; }
	merge_points_t &getMergePoints() { return mergePoints; }
};

typedef ExecutionTree<WorkerNodeInfo, WORKER_LAYER_COUNT, 2> WorkerTree; // Four layered, binary tree

}

}

#endif /* TREENODEINFO_H_ */
