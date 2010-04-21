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
#include "cloud9/Logger.h"

#include <boost/thread.hpp>
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
		virtual void onJobsExported() = 0;

		virtual void onNextJobSelection(ExplorationJob *&job) = 0;
	};
private:
	WorkerTree* tree;
	JobExecutor *executor;

	boost::condition_variable jobsAvailabe;

	boost::mutex jobsMutex;

	std::set<WorkerTree::NodePin> stats;
	bool statChanged;
	bool refineStats;

	bool initialized;
	bool terminationRequest;

	llvm::Module *origModule;
	const llvm::Module *finalModule;
	llvm::Function *mainFn;

	SelectionHandler *selHandler;

	/*
	 *
	 */
	void explodeJob(ExplorationJob *job, std::set<ExplorationJob*> &newJobs);

	void submitJob(ExplorationJob* job);

	template<typename JobIterator>
	void submitJobs(JobIterator begin, JobIterator end) {
		int count = 0;
		for (JobIterator it = begin; it != end; it++) {
			submitJob(*it);
			count++;
		}

		jobsAvailabe.notify_all();

		//CLOUD9_DEBUG("Submitted " << count << " jobs to the local queue");
	}


	ExplorationJob* dequeueJob(boost::unique_lock<boost::mutex> &lock, unsigned int timeOut);
	ExplorationJob* dequeueJob();

	void finalizeJob(ExplorationJob *job);

	void processLoop(bool allowGrowth, bool blocking, unsigned int timeOut);

	void refineStatistics();
	void cleanupStatistics();

	void selectJobs(WorkerTree::Node *root,
			std::vector<ExplorationJob*> &jobSet, int maxCount);

	unsigned int countJobs(WorkerTree::Node *root);

	JobExecutor *createExecutor(llvm::Module *module, int argc, char **argv);
public:
	JobManager(llvm::Module *module);
	virtual ~JobManager();

	void setupStartingPoint(llvm::Function *mainFn, int argc, char **argv,
			char **envp);
	void setupStartingPoint(std::string mainFnName, int argc, char **argv,
			char **envp);

	WorkerTree *getTree() { return tree; }

	JobExecutor *getJobExecutor() { return executor; }

	/*
	 * Main methods
	 */
	void processJobs(unsigned int timeOut = 0);
	void processJobs(ExecutionPathSetPin paths, unsigned int timeOut = 0);

	void finalize();

	/*
	 * Statistics methods
	 */

	void getStatisticsData(std::vector<int> &data,
			ExecutionPathSetPin &paths, bool onlyChanged);

	void setRefineStatistics() { refineStats = true; }

	void requestTermination() { terminationRequest = true; }

	/*
	 * Job import/export methods
	 */
	void importJobs(ExecutionPathSetPin paths);
	ExecutionPathSetPin exportJobs(ExecutionPathSetPin seeds,
			std::vector<int> counts);

};

}
}

#endif /* JOBMANAGER_H_ */
