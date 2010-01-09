/*
 * JobManager.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef JOBMANAGER_H_
#define JOBMANAGER_H_

#include "cloud9/worker/ExplorationJob.h"
#include "cloud9/worker/JobExecutor.h"

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
public:
	class SelectionHandler {
	public:
		SelectionHandler() {};
		virtual ~SelectionHandler() {};

	public:
		virtual void onJobEnqueued(ExplorationJob *job) = 0;
		virtual void onJobExecutionStarted(ExplorationJob *job) = 0;
		virtual void onJobExecutionFinished(ExplorationJob *job) = 0;

		virtual void onNextJobSelection(ExplorationJob *&job) = 0;
	};
private:
	WorkerTree* tree;
	JobExecutor *executor;

	//std::set<ExplorationJob*> waitingPool;
	//std::set<ExplorationJob*> executingPool;

	bool initialized;

	llvm::Module *origModule;
	const llvm::Module *finalModule;
	llvm::Function *mainFn;

	SelectionHandler *selHandler;

	/*
	 *
	 */
	void explodeJob(ExplorationJob *job, std::set<ExplorationJob*> &newJobs);

	void consumeJob(ExplorationJob *job);

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
