/*
 * JobManager.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef JOBMANAGER_H_
#define JOBMANAGER_H_


#include "cloud9/Logger.h"
#include "cloud9/worker/TreeNodeInfo.h"
#include "cloud9/worker/SymbolicEngine.h"

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
class KModule;
}

namespace cloud9 {

namespace worker {

class SymbolicState;
class ExecutionJob;
class KleeHandler;
class JobSelectionStrategy;


class JobManager {
private:
	/*
	 * KLEE integration
	 */
	klee::Interpreter *interpreter;
	SymbolicEngine *symbEngine;

	KleeHandler *kleeHandler;
	klee::KModule *kleeModule;

	llvm::Function *mainFn;


	/*
	 * Symbolic tree
	 */
	WorkerTree* tree;

	boost::condition_variable jobsAvailabe;

	boost::mutex jobsMutex;

	std::set<WorkerTree::NodePin> stats;
	bool statChanged;
	bool refineStats;

	bool terminationRequest;

	JobSelectionStrategy *selStrategy;

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

	void submitJob(ExecutionJob* job);

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


	ExecutionJob* selectJob(boost::unique_lock<boost::mutex> &lock, unsigned int timeOut);
	ExecutionJob* selectJob();

	void finalizeJob(ExecutionJob *job);

	void processLoop(bool allowGrowth, bool blocking, unsigned int timeOut);

	void refineStatistics();
	void cleanupStatistics();

	void selectJobs(WorkerTree::Node *root,
			std::vector<ExecutionJob*> &jobSet, int maxCount);

	unsigned int countJobs(WorkerTree::Node *root);
	ExecutionJob *createJob(WorkerTree::Node *root, bool foreign);


	void initialize(llvm::Module *module, llvm::Function *mainFn, int argc, char **argv,
			char **envp);

	void initKlee();
	void initInstrumentation();
	void initBreakpoints();
	void initStatistics();
	void initStrategy();

	void initRootState(llvm::Function *f, int argc,
			char **argv, char **envp);

	void exploreNode(WorkerTree::Node *node);

	void replayPath(WorkerTree::Node *pathEnd);

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
	JobManager(llvm::Module *module, std::string mainFnName, int argc, char **argv,
			char **envp);
	virtual ~JobManager();

	WorkerTree *getTree() { return tree; }

	unsigned getModuleCRC() const;

	void finalizeExecution();

	/*
	 * Main methods
	 */
	void processJobs(unsigned int timeOut = 0);
	void processJobs(ExecutionPathSetPin paths, unsigned int timeOut = 0);

	void executeJob(ExecutionJob *job);

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
};

}
}

#endif /* JOBMANAGER_H_ */
