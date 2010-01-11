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

		virtual void onNextJobSelection(ExplorationJob *&job) = 0;
	};
private:
	WorkerTree* tree;
	JobExecutor *executor;

	boost::condition_variable jobsAvailabe;

	boost::mutex jobsMutex;

	std::set<WorkerTree::Node*> stats;
	bool statChanged;

	bool initialized;

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
		for (JobIterator it = begin; it != end; it++) {
			submitJob(*it);
		}

		jobsAvailabe.notify_all();
	}

	void finalizeJob(ExplorationJob *job);

	ExplorationJob* dequeueJob(boost::unique_lock<boost::mutex> &lock);

	JobExecutor *createExecutor(llvm::Module *module, int argc, char **argv);

	JobManager(WorkerTree *tree, llvm::Module *module);
public:
	JobManager(llvm::Module *module);
	virtual ~JobManager();

	void setupStartingPoint(llvm::Function *mainFn, int argc, char **argv,
			char **envp);
	void setupStartingPoint(std::string mainFnName, int argc, char **argv,
			char **envp);

	WorkerTree *getTree() { return tree; }

	ExplorationJob *createJob(WorkerTree::Node *root, bool foreign);

	void processJobs();

	/*
	 * Statistics methods
	 */

	void refineStatistics();
	void getStatisticsData(std::vector<int> &data,
			std::vector<ExecutionPath*> &paths, bool onlyChanged);


	/*
	 * Job import/export methods
	 */
	void importJobs(std::vector<ExecutionPath*> &paths);
	void exportJobs(int count, std::vector<ExecutionPath*> &paths);



};

}
}

#endif /* JOBMANAGER_H_ */
