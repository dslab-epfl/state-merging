/*
 * JobManager.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/JobManager.h"
#include "cloud9/worker/ExplorationJob.h"
#include "cloud9/Common.h"
#include "cloud9/worker/JobManagerBehaviors.h"
#include "cloud9/Logger.h"
#include "cloud9/ExecutionTree.h"

#include "llvm/Function.h"
#include "llvm/Module.h"

#include "klee/Interpreter.h"


namespace cloud9 {

namespace worker {

JobManager::JobManager(llvm::Module *module) :
		initialized(false), origModule(module) {

	this->tree = new WorkerTree(2);

	switch (JobSelection) {
	case RandomSel:
		selHandler = new RandomSelectionHandler();
		CLOUD9_INFO("Using RandomSelectionHandler");
		break;
	default:
		assert(0);
	}
}

JobManager::~JobManager() {
	// TODO Auto-generated destructor stub
}

void JobManager::explodeJob(ExplorationJob* job, std::set<ExplorationJob*> &newJobs) {
	assert(job->isFinished());

	for (ExplorationJob::frontier_t::iterator it = job->frontier.begin();
			it != job->frontier.end(); it++) {
		WorkerTree::Node *node = *it;

		ExplorationJob *newJob = new ExplorationJob(node);
		(**node).job = newJob;

		newJobs.insert(newJob);
	}
}

void JobManager::setupStartingPoint(llvm::Function *mainFn, int argc, char **argv, char **envp) {
	assert(!initialized);
	assert(mainFn);

	this->mainFn = mainFn;

	executor = createExecutor(origModule, argc, argv);

	// Update the module with the optimizations
	finalModule = executor->getModule();
	executor->initRootState(mainFn, argc, argv, envp);

	initialized = true;
}

void JobManager::setupStartingPoint(std::string mainFnName, int argc, char **argv, char **envp) {
	llvm::Function *mainFn = origModule->getFunction(mainFnName);

	setupStartingPoint(mainFn, argc, argv, envp);
}

JobExecutor *JobManager::createExecutor(llvm::Module *module, int argc, char **argv) {
	return new JobExecutor(module, tree, argc, argv);
}

void JobManager::submitJob(ExplorationJob* job) {
	selHandler->onJobEnqueued(job);

	CLOUD9_DEBUG("Submitted job: " << *(job->jobRoot));
}

ExplorationJob *JobManager::createJob(WorkerTree::Node *root) {
	return new ExplorationJob(root);
}

void JobManager::processJobs() {
	ExplorationJob *job = NULL;
	selHandler->onNextJobSelection(job);

	while (job != NULL) {
		CLOUD9_DEBUG("Processing job: " << *(job->jobRoot));

		selHandler->onJobExecutionStarted(job);
		executor->executeJob(job);
		selHandler->onJobExecutionFinished(job);

		std::set<ExplorationJob*> newJobs;
		explodeJob(job, newJobs);

		for (std::set<ExplorationJob*>::iterator it = newJobs.begin();
				it != newJobs.end(); it++) {
			submitJob(*it);
		}

		selHandler->onNextJobSelection(job);
	}
}

}
}
