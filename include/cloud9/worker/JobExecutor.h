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


namespace klee {
class Interpreter;
}

namespace llvm {
class Module;
}

namespace cloud9 {

namespace worker {

class KleeHandler;

/*
 * Encapsulates a sequential symbolic execution engine.
 */
class JobExecutor {
private:
	klee::Interpreter *interpreter;
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

	ExplorationJob *getCurrentJob() { return currentJob; }

	void executeJob(ExplorationJob *job);
};

}
}

#endif /* JOBEXECUTOR_H_ */
