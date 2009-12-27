/*
 * JobManager.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef JOBMANAGER_H_
#define JOBMANAGER_H_

#include "ExplorationJob.h"
#include "JobExecutor.h"

#include <list>
#include <set>
#include <string>

namespace llvm {
class Module;
class Function;
}

namespace cloud9 {

namespace worker {


class JobManager {
private:
	WorkerTree* tree;
	JobExecutor *executor;

	std::set<ExplorationJob*> waitingPool;
	std::set<ExplorationJob*> executingPool;

	bool initialized;

	llvm::Module *origModule;
	const llvm::Module *finalModule;
	llvm::Function *mainFn;

	/*
	 *
	 */
	void explodeJob(ExplorationJob* job, std::set<ExplorationJob*> &newJobs);

	JobExecutor *createExecutor(llvm::Module *module, int argc, char **argv);

	JobManager(WorkerTree *tree, llvm::Module *module);
public:
	JobManager(llvm::Module *module);
	virtual ~JobManager();

	void setupStartingPoint(llvm::Function *mainFn, int argc, char **argv,
			char **envp);
	void setupStartingPoint(std::string mainFnName, int argc, char **argv,
			char **envp);

	WorkerTree *getTree() { return tree; }

	void submitJob(ExplorationJob* job);

	ExplorationJob *createJob(WorkerTree::Node *root);

	void processJobs();


};

}
}

#endif /* JOBMANAGER_H_ */
