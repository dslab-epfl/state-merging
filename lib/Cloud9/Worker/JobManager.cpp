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
#include "llvm/System/TimeValue.h"

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

using llvm::sys::TimeValue;

namespace cloud9 {

namespace worker {

static bool isJob(WorkerTree::Node *node) {
	return (**node).getJob() != NULL;
}

JobManager::JobManager(llvm::Module *module) :
		initialized(false), terminationRequest(false), origModule(module) {

	tree = new WorkerTree();

	switch (JobSelection) {
	case RandomSel:
	  selHandler = new RandomSelectionHandler();
	  CLOUD9_INFO("Using random job selection strategy");
	  break;
	case RandomPathSel:
	  selHandler = new RandomPathSelectionHandler(tree);
	  CLOUD9_INFO("Using random path job selection strategy");
	  break;
	case CoverageOptimizedSel:
	  selHandler = 
	    new WeightedRandomSelectionHandler(WeightedRandomSelectionHandler::CoveringNew,
							  tree);
	  CLOUD9_INFO("Using weighted random job selection strategy");
	  break;
	default:
	  assert(0 && "undefined job selection strategy");
	}

	// Configure the root as a statistics node
	WorkerTree::NodePin rootPin = this->tree->getRoot()->pin(WORKER_LAYER_JOBS);

	stats.insert(rootPin);
	statChanged = true;
	refineStats = false;
}

JobManager::~JobManager() {
	if (executor != NULL) {
		delete executor;
	}
}

void JobManager::explodeJob(ExplorationJob* job, std::set<ExplorationJob*> &newJobs) {
	assert(job->isFinished());

	for (ExplorationJob::frontier_t::iterator it = job->frontier.begin();
			it != job->frontier.end(); it++) {
		WorkerTree::Node *node = tree->getNode(WORKER_LAYER_JOBS, *it);

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

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentQueueSize);

	//CLOUD9_DEBUG("Submitted job on level " << (job->jobRoot->getLevel()));
}

ExplorationJob* JobManager::dequeueJob(boost::unique_lock<boost::mutex> &lock,  unsigned int timeOut) {
	ExplorationJob *job;

	selHandler->onNextJobSelection(job);
	while (job == NULL) {
		CLOUD9_INFO("No jobs in the queue, waiting for...");
		cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "idle");

		bool result = true;
		if (timeOut > 0)
			result = jobsAvailabe.timed_wait(lock, boost::posix_time::seconds(timeOut));
		else
			jobsAvailabe.wait(lock);

		if (!result) {
			CLOUD9_INFO("Timeout while waiting for new jobs. Aborting.");
			return NULL;
		} else
			CLOUD9_INFO("More jobs available. Resuming exploration...");

		selHandler->onNextJobSelection(job);

		if (job != NULL)
			cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "working");
	}

	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize);

	return job;
}

ExplorationJob* JobManager::dequeueJob() {
	ExplorationJob *job;
	selHandler->onNextJobSelection(job);

	if (job != NULL) {
		cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize);
	}

	return job;
}

void JobManager::processLoop(bool allowGrowth, bool blocking, unsigned int timeOut) {
	ExplorationJob *job = NULL;

	boost::unique_lock<boost::mutex> lock(jobsMutex);

	TimeValue deadline = TimeValue::now() + TimeValue(timeOut, 0);

	// TODO: Put an abort condition here
	while (!terminationRequest) {
		TimeValue now = TimeValue::now();

		if (timeOut > 0 && now > deadline) {
			CLOUD9_INFO("Timeout reached. Suspending execution.");
			break;
		}

		if (blocking) {
			if (timeOut > 0) {
				TimeValue remaining = deadline - now;

				job = dequeueJob(lock, remaining.seconds());
			} else {
				job = dequeueJob(lock, 0);
			}
		} else {
			job = dequeueJob();
		}

		if (blocking && timeOut == 0) {
			assert(job != NULL);
		} else {
			if (job == NULL)
				break;
		}

		bool foreign = job->foreign;

		job->started = true;

		lock.unlock();
		//CLOUD9_DEBUG("Processing job: " << *(job->jobRoot));

		executor->executeJob(job);

		lock.lock();

		job->finished = true;

		std::set<ExplorationJob*> newJobs;

		if (allowGrowth)
			explodeJob(job, newJobs);

		delete job;

		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalProcJobs);

		if (foreign)
			cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentImportedPathCount);

		if (allowGrowth)
			submitJobs(newJobs.begin(), newJobs.end()); // XXX Memory leak here, if allowGrowth == false

		if (refineStats) {
			refineStatistics();
			refineStats = false;
		}
	}

	if (terminationRequest)
		CLOUD9_INFO("Termination was requested.");
}

void JobManager::finalize() {
	executor->finalizeExecution();

	CLOUD9_INFO("Finalized job execution.");
}

void JobManager::processJobs(unsigned int timeOut) {
	if (timeOut > 0) {
		CLOUD9_INFO("Processing jobs with a timeout of " << timeOut << " seconds.");
	}
	processLoop(true, true, timeOut);
}

void JobManager::processJobs(ExecutionPathSetPin paths, unsigned int timeOut) {
	// First, we need to import the jobs in the manager
	importJobs(paths);

	// Then we execute them, but only them (non blocking, don't allow growth),
	// until the queue is exhausted
	processLoop(false, false, timeOut);
}

void JobManager::refineStatistics() {
	std::set<WorkerTree::NodePin> newStats;

	for (std::set<WorkerTree::NodePin>::iterator it = stats.begin();
			it != stats.end(); it++) {
		const WorkerTree::NodePin &nodePin = *it;

		if (nodePin->getCount(WORKER_LAYER_JOBS) > 0) {
			// Move along the path

			WorkerTree::Node *left = nodePin->getChild(WORKER_LAYER_JOBS, 0);
			WorkerTree::Node *right = nodePin->getChild(WORKER_LAYER_JOBS, 1);

			if (left) {
				WorkerTree::NodePin leftPin = left->pin(WORKER_LAYER_JOBS);
				newStats.insert(leftPin);
			}

			if (right) {
				WorkerTree::NodePin rightPin = right->pin(WORKER_LAYER_JOBS);
				newStats.insert(rightPin);
			}
		} else {
			newStats.insert(nodePin);
		}
	}

	stats = newStats;
	statChanged = true;
}

void JobManager::cleanupStatistics() {

	for (std::set<WorkerTree::NodePin>::iterator it = stats.begin();
				it != stats.end();) {
		std::set<WorkerTree::NodePin>::iterator oldIt = it++;
		WorkerTree::NodePin nodePin = *oldIt;

		if (nodePin->getCount(WORKER_LAYER_JOBS) == 0 && (**nodePin).job == NULL &&
				nodePin->getParent() != NULL) {
			stats.erase(oldIt);
			statChanged = true;
		}
	}

	if (stats.empty()) {
		// Add back the root state in the statistics
		WorkerTree::NodePin rootPin = tree->getRoot()->pin(WORKER_LAYER_JOBS);
		stats.insert(rootPin);
		statChanged = true;
	}
}

void JobManager::getStatisticsData(std::vector<int> &data,
		ExecutionPathSetPin &paths, bool onlyChanged) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	cleanupStatistics();

	if (statChanged || !onlyChanged) {
		std::vector<WorkerTree::Node*> newStats;
		for (std::set<WorkerTree::NodePin>::iterator it = stats.begin();
					it != stats.end(); it++) {
			newStats.push_back((*it).get());
		}

		paths = tree->buildPathSet(newStats.begin(), newStats.end());
		statChanged = false;

		CLOUD9_DEBUG("Sent node set: " << getASCIINodeSet(newStats.begin(), newStats.end()));
	}

	data.clear();

	for (std::set<WorkerTree::NodePin>::iterator it = stats.begin();
			it != stats.end(); it++) {
		const WorkerTree::NodePin &crtNodePin = *it;
		unsigned int jobCount = tree->countLeaves(WORKER_LAYER_JOBS, crtNodePin.get(), &isJob);
		data.push_back(jobCount);
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

		if (node->getCount(WORKER_LAYER_JOBS) == 0) {
			ExplorationJob *job = (**node).job;

			if (job) {
				if (job->isStarted()) {
					CLOUD9_DEBUG("FOUND A STARTED JOB: " << *(job->jobRoot));
					continue;
				}

				jobSet.push_back(job);
				maxCount--;
			}
		} else {
			WorkerTree::Node *left = node->getChild(WORKER_LAYER_JOBS, 0);
			WorkerTree::Node *right = node->getChild(WORKER_LAYER_JOBS, 1);

			if (left) nodes.push(left);
			if (right) nodes.push(right);
		}
	}

	CLOUD9_DEBUG("Selected " << jobSet.size() << " jobs");

}

unsigned int JobManager::countJobs(WorkerTree::Node *root) {
	return tree->countLeaves(WORKER_LAYER_JOBS, root, &isJob);
}

void JobManager::importJobs(ExecutionPathSetPin paths) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::vector<WorkerTree::Node*> nodes;
	std::vector<ExplorationJob*> jobs;

	tree->getNodes(WORKER_LAYER_JOBS, paths, nodes);

	CLOUD9_DEBUG("Importing " << paths->count() << " jobs");

	for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin();
			it != nodes.end(); it++) {
		WorkerTree::Node *crtNode = *it;

		if (crtNode->getCount(WORKER_LAYER_JOBS) > 0) {
			CLOUD9_INFO("Discarding job as being obsolete: " << *crtNode);
		} else {
			// The exploration job object gets a pin on the node, thus
			// ensuring it will be released automatically after it's no
			// longer needed
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

ExecutionPathSetPin JobManager::exportJobs(ExecutionPathSetPin seeds,
		std::vector<int> counts) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::vector<WorkerTree::Node*> roots;
	std::vector<ExplorationJob*> jobs;
	std::vector<WorkerTree::Node*> jobRoots;

	tree->getNodes(WORKER_LAYER_JOBS, seeds, roots);

	assert(roots.size() == counts.size());

	for (unsigned int i = 0; i < seeds->count(); i++) {
		selectJobs(roots[i], jobs, counts[i]);
	}

	for (std::vector<ExplorationJob*>::iterator it = jobs.begin();
			it != jobs.end(); it++) {
		ExplorationJob *job = *it;
		jobRoots.push_back(job->jobRoot.get());
	}

	// Do this before de-registering the jobs, in order to keep the nodes pinned
	ExecutionPathSetPin paths = tree->buildPathSet(jobRoots.begin(), jobRoots.end());

	// De-register the jobs with the worker
	for (std::vector<ExplorationJob*>::iterator it = jobs.begin();
			it != jobs.end(); it++) {
		// Cancel each job
		ExplorationJob *job = *it;
		assert(!job->isStarted());
		assert(job->jobRoot->getCount(WORKER_LAYER_JOBS) == 0);

		job->started = true;
		job->finished = true;

		finalizeJob(job);
	}

	selHandler->onJobsExported();

	for (std::vector<ExplorationJob*>::iterator it = jobs.begin();
				it != jobs.end(); it++) {
		delete (*it);
	}


	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalExportedJobs,
			jobs.size());
	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize,
			jobs.size());

	return paths;
}

}
}
