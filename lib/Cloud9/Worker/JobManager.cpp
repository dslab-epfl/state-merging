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

#include <stack>


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

	// Configure the root as a statistics node
	WorkerTree::Node *root = this->tree->getRoot();
	(**root).stats = true;

	stats.insert(root);
	statChanged = true;
	refineStats = false;
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
	(**crtNode).job = job;

	while (crtNode != NULL) {
		(**crtNode).jobCount++;

		if ((**crtNode).stats)
			break;

		crtNode = crtNode->getParent();
	}

	//CLOUD9_DEBUG("Submitted job: " << *(job->jobRoot));
}

ExplorationJob *JobManager::createJob(WorkerTree::Node *root, bool foreign) {
	return new ExplorationJob(root, foreign);
}

void JobManager::finalizeJob(ExplorationJob *job) {
	// Update job statistics
	WorkerTree::Node *crtNode = job->jobRoot;
	(**crtNode).job = NULL;

	bool emptyJob = (job->frontier.size() == 0);

	while (crtNode != NULL) {
		(**crtNode).jobCount--;

		if ((**crtNode).stats) {
			assert(stats.find(crtNode) != stats.end());

			if  ((**crtNode).jobCount == 0 && emptyJob) {
				// The supporting branch will disappear, make sure stats
				// doesn't catch a bad pointer
				stats.erase(crtNode);
				statChanged = true;
			}

			break;
		}

		crtNode = crtNode->getParent();
	}

	assert(crtNode != NULL);

	if (emptyJob) {
		// The empty job doesn't have any inner supporting branches as well
		assert(job->jobRoot->getCount() == 0);

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

		job->started = true;

		lock.unlock();
		//CLOUD9_DEBUG("Processing job: " << *(job->jobRoot));

		executor->executeJob(job);

		lock.lock();

		job->finished = true;
		finalizeJob(job);

		std::set<ExplorationJob*> newJobs;
		explodeJob(job, newJobs);

		delete job;

		submitJobs(newJobs.begin(), newJobs.end());

		if (refineStats) {
			refineStatistics();
			refineStats = false;
		}
	}
}

void JobManager::refineStatistics() {
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
			assert((**left).jobCount > 0);

			(**left).stats = true;

			newStats.insert(left);
		}

		if (right) {
			assert(!(**right).stats);
			assert((**right).jobCount > 0);

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

	if (statChanged || !onlyChanged) {
		tree->buildPathSet(stats.begin(), stats.end(), paths);
		statChanged = false;

		CLOUD9_DEBUG("Sent node set: " << getASCIINodeSet(stats.begin(), stats.end()));
	}

	data.clear();

	for (std::set<WorkerTree::Node*>::iterator it = stats.begin();
			it != stats.end(); it++) {
		WorkerTree::Node *crtNode = *it;
		data.push_back((**crtNode).jobCount);
	}

	CLOUD9_DEBUG("Sent data set: " << getASCIIDataSet(data.begin(), data.end()));
}

void JobManager::selectJobs(WorkerTree::Node *root,
		std::vector<ExplorationJob*> &jobSet, int maxCount) {
	/// XXX: Prevent node creation
	std::stack<WorkerTree::Node*> nodes;

	nodes.push(root);

	while (!nodes.empty() && maxCount > 0) {
		WorkerTree::Node *node = nodes.top();
		nodes.pop();

		if ((**node).job != NULL) {
			ExplorationJob *job = (**node).job;

			if (job->isStarted()) {
				CLOUD9_DEBUG("FOUND A STARTED JOB: " << *(job->jobRoot));
				continue;
			}

			assert(node->getCount() == 0);

			jobSet.push_back(job);
			maxCount--;
		} else {
			WorkerTree::Node *left = node->getChild(0);
			WorkerTree::Node *right = node->getChild(1);

			assert(left || right);

			if (left) {
				nodes.push(left);
			}

			if (right) {
				nodes.push(right);
			}
		}
	}

	CLOUD9_DEBUG("Selected " << jobSet.size() << " jobs");

}

void JobManager::importJobs(std::vector<ExecutionPath*> &paths) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::vector<WorkerTree::Node*> nodes;
	std::vector<ExplorationJob*> jobs;

	tree->getNodes(paths.begin(), paths.end(), nodes);

	CLOUD9_DEBUG("Importing " << paths.size() << " jobs");

	for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin();
			it != nodes.end(); it++) {
		WorkerTree::Node *crtNode = *it;

		if (crtNode->getCount() > 0) {
			CLOUD9_INFO("Discarding job as being obsolete: " << *crtNode);
		} else {
			ExplorationJob *job = new ExplorationJob(*it, true);
			jobs.push_back(job);
		}
	}

	submitJobs(jobs.begin(), jobs.end());
}

void JobManager::exportJobs(std::vector<ExecutionPath*> &seeds,
		std::vector<int> &counts, std::vector<ExecutionPath*> &paths) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::vector<WorkerTree::Node*> roots;
	std::vector<ExplorationJob*> jobs;
	std::vector<WorkerTree::Node*> jobRoots;

	tree->getNodes(seeds.begin(), seeds.end(), roots);

	assert(roots.size() == counts.size());

	for (int i = 0; i < seeds.size(); i++) {
		selectJobs(roots[i], jobs, counts[i]);
	}

	for (std::vector<ExplorationJob*>::iterator it = jobs.begin();
			it != jobs.end(); it++) {
		ExplorationJob *job = *it;
		jobRoots.push_back(job->jobRoot);
	}

	tree->buildPathSet(jobRoots.begin(), jobRoots.end(), paths);

	// De-register the jobs with the worker
	for (std::vector<ExplorationJob*>::iterator it = jobs.begin();
			it != jobs.end(); it++) {
		// Cancel each job
		ExplorationJob *job = *it;
		assert(!job->isStarted());
		assert(job->jobRoot->getCount() == 0);

		job->started = true;
		job->finished = true;
		job->frontier.clear();

		finalizeJob(job);
	}

	selHandler->onJobsExported();
}

}
}
