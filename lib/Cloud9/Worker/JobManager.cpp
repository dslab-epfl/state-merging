/*
 * JobManager.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/JobManager.h"

#include "llvm/Function.h"
#include "llvm/Module.h"

#include "klee/Interpreter.h"

namespace cloud9 {

namespace worker {

JobManager::JobManager(WorkerTree *tree, llvm::Module *module) {
	this->tree = tree;
	this->origModule = module;

	initialized = false;
}

JobManager::JobManager(llvm::Module *module) {
	this->tree = new WorkerTree(2);
	this->origModule = module;

	initialized = false;
}

JobManager::~JobManager() {
	// TODO Auto-generated destructor stub
}

void JobManager::explodeJob(ExplorationJob* job, std::set<ExplorationJob*> &newJobs) {
	assert(job->isFinished());

	for (ExplorationJob::frontier_t::iterator it = job->frontier.begin();
			it != job->frontier.end(); it++) {
		WorkerTree::Node *node = *it;

		ExplorationJob *newJob = new ExplorationJob(job, node);
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

}

ExplorationJob *JobManager::createJob(WorkerTree::Node *root) {
	return NULL;
}

void JobManager::processJobs() {

}

}
}
