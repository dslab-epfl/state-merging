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
	this->module = module;

	initialized = false;
}

JobManager::JobManager(llvm::Module *module) {
	this->tree = new WorkerTree();
	this->module = module;

	initialized = false;
}

JobManager::~JobManager() {
	// TODO Auto-generated destructor stub
}

void JobManager::setupStartingPoint(llvm::Function *mainFn, int argc, char **argv, char **envp) {
	assert(!initialized);
	assert(mainFn);

	this->mainFn = mainFn;

	setupExecutor(module, argc, argv, envp);

	initialized = true;

	executor->interpreter->runFunctionAsMain(mainFn, argc, argv, envp);
}

void JobManager::setupStartingPoint(std::string mainFnName, int argc, char **argv, char **envp) {
	llvm::Function *mainFn = module->getFunction(mainFnName);

	setupStartingPoint(mainFn, argc, argv, envp);
}

void JobManager::setupExecutor(llvm::Module *module, int argc, char **argv, char **envp) {
	executor = new JobExecutor(module, argc, argv);
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
