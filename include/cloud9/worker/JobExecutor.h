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
#include "cloud9/worker/ExplorationJob.h"
#include "cloud9/worker/StateEventHandler.h"


namespace klee {
class Interpreter;
class ExecutionState;
}

namespace llvm {
class Module;
class Function;
}

namespace cloud9 {

namespace worker {

class KleeHandler;
class SymbolicEngine;

/*
 * Encapsulates a sequential symbolic execution engine.
 */
class JobExecutor {
private:
	class SEHandler: public StateEventHandler {
	private:
		ExplorationJob *job;
	public:
		virtual void onStateBranched(klee::ExecutionState *state,
				klee::ExecutionState *parent, int index);
		virtual void onStateDestroy(klee::ExecutionState *state, bool &allow);
	};

	klee::Interpreter *interpreter;
	SymbolicEngine *symbEngine;
	SEHandler seHandler;

	KleeHandler *kleeHandler;
	const llvm::Module *finalModule;

	ExplorationJob *currentJob;

	/*
	 * Returns the next node to be explored
	 */
	WorkerTree::Node *getNextNode();

	void exploreNode(WorkerTree::Node *node);

	void externalsAndGlobalsCheck(const llvm::Module *m);
public:
	JobExecutor(llvm::Module *module, int argc, char **argv);
	virtual ~JobExecutor();

	void initRootState(WorkerTree::Node *node, llvm::Function *f, int argc,
			char **argv, char **envp);

	const llvm::Module *getModule() const { return finalModule; }

	ExplorationJob *getCurrentJob() const { return currentJob; }

	void executeJob(ExplorationJob *job);

};

}
}

#endif /* JOBEXECUTOR_H_ */
