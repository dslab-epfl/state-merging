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
#include "cloud9/worker/SymbolicEngine.h"


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

/*
 * Encapsulates a sequential symbolic execution engine.
 */
class JobExecutor: public SymbolicEngine::StateEventHandler {
public:
	class BehaviorHandler {
	public:
		BehaviorHandler() {};
		virtual ~BehaviorHandler() {};

	public:
		virtual void onJobStarted(ExplorationJob *job) {};
		virtual void onJobTerminated(ExplorationJob *job) {};

		virtual void onNodeExplored(WorkerTree::Node *node) {};
		virtual void onNodeDeleted(WorkerTree::Node *node) {};
	};

	class SizingHandler: public BehaviorHandler {
	public:
		SizingHandler() {};
		virtual ~SizingHandler() {};

	public:
		virtual void onTerminationQuery(ExplorationJob *job, bool &term) = 0;
	};

	class ExplorationHandler: public BehaviorHandler {
	public:
		ExplorationHandler() {};
		virtual ~ExplorationHandler() {};

	public:
		virtual void onNextStateQuery(ExplorationJob *job, WorkerTree::Node *&node) = 0;
	};

private:
	klee::Interpreter *interpreter;
	SymbolicEngine *symbEngine;

	KleeHandler *kleeHandler;
	const llvm::Module *finalModule;
	WorkerTree *tree;

	ExplorationJob *currentJob;

	// Behavior Handlers
	SizingHandler *sizingHandler;
	ExplorationHandler *expHandler;

	/*
	 * Returns the next node to be explored
	 */
	WorkerTree::Node *getNextNode();

	void exploreNode(WorkerTree::Node *node);

	void updateTreeOnBranch(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index);
	void updateTreeOnDestroy(klee::ExecutionState *state);

	void externalsAndGlobalsCheck(const llvm::Module *m);

	void fireJobStarted(ExplorationJob *job) {
		expHandler->onJobStarted(job);
		sizingHandler->onJobStarted(job);
	}

	void fireJobTerminated(ExplorationJob *job) {
		expHandler->onJobTerminated(job);
		sizingHandler->onJobTerminated(job);
	}

	void fireNodeExplored(WorkerTree::Node *node) {
		expHandler->onNodeExplored(node);
		sizingHandler->onNodeExplored(node);
	}

	void fireNodeDeleted(WorkerTree::Node *node) {
		expHandler->onNodeDeleted(node);
		sizingHandler->onNodeDeleted(node);
	}

public:
	JobExecutor(llvm::Module *module, WorkerTree *tree, int argc, char **argv);
	virtual ~JobExecutor();

	void initRootState(llvm::Function *f, int argc,
			char **argv, char **envp);
	void initHandlers();

	const llvm::Module *getModule() const { return finalModule; }

	ExplorationJob *getCurrentJob() const { return currentJob; }

	void executeJob(ExplorationJob *job);

	void replayPath(WorkerTree::Node *pathEnd);

	virtual void onStateBranched(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index);
	virtual void onStateDestroy(klee::ExecutionState *state, bool &allow);

};

}
}

#endif /* JOBEXECUTOR_H_ */
