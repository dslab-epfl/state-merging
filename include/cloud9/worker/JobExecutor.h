/*
 * JobExecutor.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef JOBEXECUTOR_H_
#define JOBEXECUTOR_H_

#include "llvm/System/Path.h"

#include "cloud9/ExecutionTree.h"
#include "cloud9/worker/SymbolicEngine.h"

#include <iostream>
#include <boost/thread.hpp>

namespace llvm {
class Module;
class Function;
}

namespace cloud9 {

namespace worker {

class KleeHandler;
class JobManager;

/*
 * Encapsulates a sequential symbolic execution engine.
 */
class JobExecutor: public StateEventHandler {
private:
	WorkerTree *tree;


	/*
	 * Returns the next node to be explored
	 */
	WorkerTree::Node *getNextNode();

	void exploreNode(WorkerTree::Node *node);

	void updateTreeOnBranch(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index);
	void updateTreeOnDestroy(klee::ExecutionState *state);

	void fireBreakpointHit(WorkerTree::Node *node);


public:

	void initRootState(llvm::Function *f, int argc,
			char **argv, char **envp);

	void finalizeExecution();

	unsigned getModuleCRC() const;

	ExplorationJob *getCurrentJob() const { return currentJob; }

	void executeJob(ExplorationJob *job);

	void replayPath(WorkerTree::Node *pathEnd);


};

}
}

#endif /* JOBEXECUTOR_H_ */
