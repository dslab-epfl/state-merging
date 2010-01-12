/*
 * JobManager.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/JobManager.h"
#include "cloud9/worker/ExplorationJob.h"
#include "cloud9/worker/WorkerCommon.h"
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
		CLOUD9_INFO("Using random job selection strategy");
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

		ExplorationJob *newJob = new ExplorationJob(node, false);
		//(**node).job = newJob;

		newJobs.insert(newJob);
	}
}

void JobManager::setupStartingPoint(llvm::Function *mainFn, int argc,
		char **argv, char **envp) {

	assert(!initialized);
	assert(mainFn);

	this->mainFn = mainFn;

	executor = createExecutor(origModule, argc, argv);

	// Update the module with the optimizations
	finalModule = executor->getModule();
	executor->initRootState(mainFn, argc, argv, envp);

	initialized = true;
}

void JobManager::setupStartingPoint(std::string mainFnName, int argc,
		char **argv, char **envp) {

	llvm::Function *mainFn = origModule->getFunction(mainFnName);

	setupStartingPoint(mainFn, argc, argv, envp);
}

JobExecutor *JobManager::createExecutor(llvm::Module *module, int argc, char **argv) {
	return new JobExecutor(module, tree, argc, argv);
}

void JobManager::submitJob(ExplorationJob* job) {
	selHandler->onJobEnqueued(job);

	// Update job statistics
	WorkerTree::Node *crtNode = job->jobRoot;

	while (crtNode != NULL) {
		(**crtNode).jobCount++;

		crtNode = crtNode->getParent();
	}

	CLOUD9_DEBUG("Submitted job: " << *(job->jobRoot));
}

ExplorationJob *JobManager::createJob(WorkerTree::Node *root, bool foreign) {
	return new ExplorationJob(root, foreign);
}

void JobManager::finalizeJob(ExplorationJob *job) {
	// Update job statistics
	WorkerTree::Node *crtNode = job->jobRoot;

	bool hitStats = false;

	while (crtNode != NULL) {
		(**crtNode).jobCount--;

		if (!hitStats && (**crtNode).stats && (**crtNode).jobCount == 0) {
			hitStats = true;
			assert(stats.find(crtNode) != stats.end());

			stats.erase(crtNode);
			statChanged = true;
		}

		crtNode = crtNode->getParent();
	}

	if (job->frontier.size() == 0) {
		// No more jobs to explore from this point
		// Remove the supporting branch
		tree->removeSupportingBranch(job->jobRoot, NULL);
	}

}

ExplorationJob* JobManager::dequeueJob(boost::unique_lock<boost::mutex> &lock) {
	ExplorationJob *job;

	selHandler->onNextJobSelection(job);
	while (job == NULL) {
		CLOUD9_INFO("No jobs in the queue, waiting for...");
		jobsAvailabe.wait(lock);
		CLOUD9_INFO("More jobs available. Resuming exploration...");

		selHandler->onNextJobSelection(job);
	}

	return job;
}

void JobManager::processJobs() {
	ExplorationJob *job = NULL;

	boost::unique_lock<boost::mutex> lock(jobsMutex);

	// TODO: Put an abort condition here
	while (true) {
		job = dequeueJob(lock);

		lock.unlock();

		//CLOUD9_DEBUG("Processing job: " << *(job->jobRoot));

		executor->executeJob(job);

		lock.lock();

		finalizeJob(job);

		std::set<ExplorationJob*> newJobs;
		explodeJob(job, newJobs);

		submitJobs(newJobs.begin(), newJobs.end());
	}
}

void JobManager::refineStatistics() {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::set<WorkerTree::Node*> newStats;

	for (std::set<WorkerTree::Node*>::iterator it = stats.begin();
			it != stats.end(); it++) {
		WorkerTree::Node *node = *it;

		assert((**node).jobCount > 0);
		assert((**node).stats);

		WorkerTree::Node *left = node->getChild(0);
		WorkerTree::Node *right = node->getChild(1);

		if (left) {
			assert(!(**left).stats);
			(**left).stats = true;

			newStats.insert(left);
		}

		if (right) {
			assert(!(**right).stats);
			(**right).stats = true;

			newStats.insert(right);
		}

	}

	stats = newStats;
	statChanged = true;
}

void JobManager::getStatisticsData(std::vector<int> &data,
		std::vector<ExecutionPath*> &paths, bool onlyChanged) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);
}

void JobManager::importJobs(std::vector<ExecutionPath*> &paths) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::vector<WorkerTree::Node*> nodes;
	std::vector<ExplorationJob*> jobs;

	tree->getNodes(paths, nodes);

	CLOUD9_DEBUG("Importing jobs: " << getASCIINodeSet(nodes.begin(), nodes.end()));

	for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin();
			it != nodes.end(); it++) {
		ExplorationJob *job = new ExplorationJob(*it, true);
		jobs.push_back(job);
	}

	submitJobs(jobs.begin(), jobs.end());
}

void JobManager::exportJobs(int count, std::vector<ExecutionPath*> &paths) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);


}

}
}
