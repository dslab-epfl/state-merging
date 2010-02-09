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
#include "cloud9/instrum/InstrumentationManager.h"

#include "llvm/Function.h"
#include "llvm/Module.h"

#include "klee/Interpreter.h"

#include <stack>

/*
 * Implementation invariants, between two consecutive job executions, when the
 * job lock is set:
 * - Every time in the tree, there is a full frontier of symbolic states.
 *
 * - A job can be either on the frontier, or ahead of it (case in which
 * replaying needs to be done). XXX: Perform a more clever way of replay.
 *
 * - An exported job will leave the frontier intact.
 *
 * - A job import cannot happen in such a way that a job lies within the limits
 * of the frontier.
 *
 * - Statistics cannot grow past the frontier of symbolic states
 */


namespace cloud9 {

namespace worker {

JobManager::JobManager(llvm::Module *module) :
		initialized(false), origModule(module) {

	tree = new WorkerTree(2);

	switch (JobSelection) {
	case RandomSel:
		selHandler = new RandomSelectionHandler();
		CLOUD9_INFO("Using random job selection strategy");
		break;
	case RandomPathSel:
		selHandler = new RandomPathSelectionHandler(tree);
		CLOUD9_INFO("Using random path job selection strategy");
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
	assert((**job->jobRoot).symState || job->foreign);

	selHandler->onJobEnqueued(job);

	// Update job statistics
	WorkerTree::Node *crtNode = job->jobRoot;
	(**crtNode).job = job;

	while (crtNode != NULL) {
		(**crtNode).jobCount++;

		crtNode = crtNode->getParent();
	}

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentQueueSize);

	//CLOUD9_DEBUG("Submitted job on level " << (job->jobRoot->getLevel()));
}

ExplorationJob *JobManager::createJob(WorkerTree::Node *root, bool foreign) {
	return new ExplorationJob(root, foreign);
}

void JobManager::finalizeJob(ExplorationJob *job) {
	// Update job statistics
	WorkerTree::Node *crtNode = job->jobRoot;
	(**crtNode).job = NULL;

	bool emptyJob = (job->frontier.size() == 0);

	if (emptyJob) {
		assert(job->jobRoot->getCount() == 0);
	}

	bool hitStats = false;

	while (crtNode != NULL) {
		(**crtNode).jobCount--;

		if (crtNode->getCount() == 0 && (**crtNode).symState == NULL) {
			assert((**crtNode).jobCount == 0);

			if ((**crtNode).stats && !hitStats) {
				hitStats = true;
				assert(stats.find(crtNode) != stats.end());

				stats.erase(crtNode);
				statChanged = true;
			}

			WorkerTree::Node *temp = crtNode;
			crtNode = crtNode->getParent();
			tree->removeNode(temp);
		} else {
			crtNode = crtNode->getParent();
		}
	}
}

ExplorationJob* JobManager::dequeueJob(boost::unique_lock<boost::mutex> &lock) {
	ExplorationJob *job;

	selHandler->onNextJobSelection(job);
	while (job == NULL) {
		CLOUD9_INFO("No jobs in the queue, waiting for...");
		cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "idle");
		jobsAvailabe.wait(lock);
		CLOUD9_INFO("More jobs available. Resuming exploration...");

		selHandler->onNextJobSelection(job);

		if (job != NULL)
			cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "working");
	}

	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize);

	return job;
}

void JobManager::processJobs() {
	ExplorationJob *job = NULL;

	boost::unique_lock<boost::mutex> lock(jobsMutex);

	// TODO: Put an abort condition here
	for(;;) {
		job = dequeueJob(lock);
		bool foreign = job->foreign;

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

		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalProcJobs);

		if (foreign)
			cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentImportedPathCount);

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

		assert((**node).stats);

		if((**node).jobCount == 0) {
			newStats.insert(node);
			continue;
		}

		if ((**node).symState != NULL) {
			newStats.insert(node);
			continue;
		}

		WorkerTree::Node *left = node->getChild(0);
		WorkerTree::Node *right = node->getChild(1);

		if (left == NULL && right == NULL) {
			newStats.insert(node);
			continue;
		}

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

			if (left == NULL && right == NULL) {
				if ((**node).symState == NULL) {
					CLOUD9_ERROR("Invalid path found while selecting jobs for export");
				}
			}

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

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalImportedJobs,
			jobs.size());
	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentImportedPathCount,
			jobs.size());
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

		finalizeJob(job);
	}

	selHandler->onJobsExported();

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalExportedJobs,
			jobs.size());
	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize,
			jobs.size());
}

}
}
