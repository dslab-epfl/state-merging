/*
 * JobManager.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/JobManager.h"

namespace cloud9 {

namespace worker {

JobManager::JobManager(WorkerTree *tree) {
	this->tree = tree;

	setupExecutor();
}

JobManager::~JobManager() {
	// TODO Auto-generated destructor stub
}

void JobManager::setupExecutor() {
	executor = new JobExecutor();
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
