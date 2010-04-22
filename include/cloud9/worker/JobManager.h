/*
 * JobManager.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef JOBMANAGER_H_
#define JOBMANAGER_H_


#include "cloud9/Logger.h"

#include <boost/thread.hpp>
#include <list>
#include <set>
#include <string>

namespace llvm {
class Module;
class Function;
}

namespace klee {
class Interpreter;
class ExecutionState;
class Searcher;
}

namespace cloud9 {

namespace worker {

class SymbolicState {
	friend class JobManager;
private:
	klee::ExecutionState *kleeState;
	WorkerTree::NodePin node;
public:
	SymbolicState(klee::ExecutionState *state) :
		kleeState(state) {
			kleeState->setCloud9State(this);
	}

	virtual ~SymbolicState() { }

	klee::ExecutionState *getKleeState() const { return kleeState; }

	WorkerTree::NodePin &getNode() const { return kleeState->getWorkerNode(); }
};

class ExecutionJob {
	friend class JobManager;
private:
	WorkerTree::NodePin node;
public:
	ExecutionJob() {}
	virtual ~ExecutionJob() {}

	WorkerTree::NodePin &getNode() const { return node; }
};

class JobSelectionHandler {
public:
	JobSelectionHandler() {};
	virtual ~JobSelectionHandler() {};

public:
	virtual void onJobEnqueued(ExplorationJob *job) { };
	virtual void onJobsExported() { };

	virtual void onStateActivated(klee::ExecutionState *state) { };
	virtual void onStateDeactivated(klee::ExecutionState *state) { };

	virtual void onNextJobSelection(ExplorationJob *&job) = 0;
};


class JobManager {
private:
	/* Klee integration */
	klee::Interpreter *interpreter;
	SymbolicEngine *symbEngine;

	KleeHandler *kleeHandler;

	const klee::KModule *finalModule;


	WorkerTree* tree;

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
	 * Breakpoint management data structures
	 *
	 */
	std::set<WorkerTree::NodePin> pathBreaks;
	std::set<unsigned int> codeBreaks;

	/*
	 * Debugging and instrumentation
	 */
	int traceCounter;

	void dumpStateTrace(WorkerTree::Node *node);

	boost::mutex executorMutex;

	/*
	 *
	 */
	void explodeJob(ExplorationJob *job, std::set<ExplorationJob*> &newJobs);

	void submitJob(ExplorationJob* job);

	void externalsAndGlobalsCheck(const llvm::Module *m);

	const llvm::Module *getModule() const;

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
	ExplorationJob *createJob(WorkerTree::Node *root, bool foreign);

	JobExecutor *createExecutor(llvm::Module *module, int argc, char **argv);
	void terminateJobs(WorkerTree::Node *root);

	void initHandlers();
	void initInstrumentation();
	void initBreakpoints();

	void initRootState(llvm::Function *f, int argc,
			char **argv, char **envp);

	void exploreNode(WorkerTree::Node *node);

	void replayPath(WorkerTree::Node *pathEnd);

	JobManager(WorkerTree *tree, llvm::Module *module);

	void updateTreeOnBranch(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index);
	void updateTreeOnDestroy(klee::ExecutionState *state);

	void fireBreakpointHit(WorkerTree::Node *node);

	/*
	 * Breakpoint management
	 */

	void setCodeBreakpoint(int assemblyLine);
	void setPathBreakpoint(ExecutionPathPin path);
public:
	JobManager(llvm::Module *module);
	virtual ~JobManager();

	void setupStartingPoint(llvm::Function *mainFn, int argc, char **argv,
			char **envp);
	void setupStartingPoint(std::string mainFnName, int argc, char **argv,
			char **envp);

	WorkerTree *getTree() { return tree; }

	unsigned getModuleCRC() const;

	void finalizeExecution();

	/*
	 * Main methods
	 */
	void processJobs(unsigned int timeOut = 0);
	void processJobs(ExecutionPathSetPin paths, unsigned int timeOut = 0);

	void executeJob(ExplorationJob *job);

	void finalize();

	virtual void onStateBranched(klee::ExecutionState *state,
			klee::ExecutionState *parent, int index);
	virtual void onStateDestroy(klee::ExecutionState *state, bool &allow);
	virtual void onControlFlowEvent(klee::ExecutionState *state,
			ControlFlowEvent event);
	virtual void onDebugInfo(klee::ExecutionState *state,
			const std::string &message);
	virtual void onOutOfResources(klee::ExecutionState *destroyedState);

	/*
	 * Statistics methods
	 */

	void getStatisticsData(std::vector<int> &data,
			ExecutionPathSetPin &paths, bool onlyChanged);

	void setRefineStatistics() { refineStats = true; }

	void requestTermination() { terminationRequest = true; }

	/*
	 * Coverage related functionality
	 */

	void getUpdatedLocalCoverage(cov_update_t &data);
	void setUpdatedGlobalCoverage(const cov_update_t &data);
	uint32_t getCoverageIDCount() const;

	/*
	 * Job import/export methods
	 */
	void importJobs(ExecutionPathSetPin paths);
	ExecutionPathSetPin exportJobs(ExecutionPathSetPin seeds,
			std::vector<int> counts);
	void terminateJobs(WorkerTree::Node *root);
};

}
}

#endif /* JOBMANAGER_H_ */
