/*
 * JobManager.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/JobManager.h"

namespace cloud9 {

namespace worker {

JobManager::JobManager(WorkerTree *tree, llvm::Module *module, int argc, char **argv) {
	this->tree = tree;

	setupExecutor(module, argc, argv);
}

JobManager::~JobManager() {
	// TODO Auto-generated destructor stub
}

void JobManager::setupExecutor(llvm::Module *module, int argc, char **argv) {
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
