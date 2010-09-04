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
#include "cloud9/worker/CoreStrategies.h"

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
class KleeHandler;
}

namespace cloud9 {

namespace worker {

class SymbolicState;
class ExecutionJob;
class StrategyPortfolio;
class OracleStrategy;

class JobManager: public StateEventHandler {
private:
  /***************************************************************************
   * Initialization
   **************************************************************************/
  void initialize(llvm::Module *module, llvm::Function *mainFn, int argc,
      char **argv, char **envp);

  void initKlee();
  void initBreakpoints();
  void initStatistics();
  void initStrategy();

  StrategyPortfolio *createStrategyPortfolio();
  OracleStrategy *createOracleStrategy();

  void initRootState(llvm::Function *f, int argc, char **argv, char **envp);

  /***************************************************************************
   * KLEE integration
   **************************************************************************/
  klee::Interpreter *interpreter;
  SymbolicEngine *symbEngine;

  klee::KleeHandler *kleeHandler;
  klee::KModule *kleeModule;

  llvm::Function *mainFn;

  /*
   * Symbolic tree
   */
  WorkerTree* tree;
  CompressedTree *cTree;

  boost::condition_variable jobsAvailabe;
  boost::mutex jobsMutex;
  bool terminationRequest;

  unsigned int jobCount;

  JobSelectionStrategy *selStrategy;

  /*
   * Job execution state
   */
  ExecutionJob *currentJob;
  bool replaying;

  /*
   * Statistics
   */

  std::set<WorkerTree::NodePin> stats;
  bool statChanged;
  bool refineStats;

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

  bool collectTraces;

  void dumpStateTrace(WorkerTree::Node *node);
  void dumpInstructionTrace(WorkerTree::Node *node);

  void serializeInstructionTrace(std::ostream &s, WorkerTree::Node *node);
  void parseInstructionTrace(std::istream &s, std::vector<unsigned int> &dest);

  void serializeExecutionTrace(std::ostream &os, WorkerTree::Node *node);
  void serializeExecutionTrace(std::ostream &os, SymbolicState *state);

  void fireActivateState(SymbolicState *state);
  void fireDeactivateState(SymbolicState *state);
  void fireUpdateState(SymbolicState *state);
  void fireAddJob(ExecutionJob *job);
  void fireRemovingJob(ExecutionJob *job);

  void submitJob(ExecutionJob* job, bool activateStates);
  void finalizeJob(ExecutionJob *job, bool deactivateStates);

  template<typename JobIterator>
  void submitJobs(JobIterator begin, JobIterator end, bool activateStates) {
    int count = 0;
    for (JobIterator it = begin; it != end; it++) {
      submitJob(*it, activateStates);
      count++;
    }

    jobsAvailabe.notify_all();

    //CLOUD9_DEBUG("Submitted " << count << " jobs to the local queue");
  }

  ExecutionJob* selectNextJob(boost::unique_lock<boost::mutex> &lock,
      unsigned int timeOut);
  ExecutionJob* selectNextJob();

  static bool isJob(WorkerTree::Node *node);
  bool isExportableJob(WorkerTree::Node *node);

  void executeJob(boost::unique_lock<boost::mutex> &lock, ExecutionJob *job,
      bool spawnNew);
  void executeJobsBatch(boost::unique_lock<boost::mutex> &lock,
      ExecutionJob *origJob, bool spawnNew);

  void stepInNode(boost::unique_lock<boost::mutex> &lock,
      WorkerTree::Node *node, bool exhaust);
  void replayPath(boost::unique_lock<boost::mutex> &lock,
      WorkerTree::Node *pathEnd);

  void processLoop(bool allowGrowth, bool blocking, unsigned int timeOut);

  void refineStatistics();
  void cleanupStatistics();

  void selectJobs(WorkerTree::Node *root, std::vector<ExecutionJob*> &jobSet,
      int maxCount);

  unsigned int countJobs(WorkerTree::Node *root);

  void updateTreeOnBranch(klee::ExecutionState *state,
      klee::ExecutionState *parent, int index, klee::ForkTag forkTag);
  void updateTreeOnDestroy(klee::ExecutionState *state);

  void updateCompressedTreeOnBranch(SymbolicState *state, SymbolicState *parent);
  void updateCompressedTreeOnDestroy(SymbolicState *state);

  void fireBreakpointHit(WorkerTree::Node *node);

  /*
   * Breakpoint management
   */

  void setCodeBreakpoint(int assemblyLine);
  void setPathBreakpoint(ExecutionPathPin path);
public:
  JobManager(llvm::Module *module, std::string mainFnName, int argc,
      char **argv, char **envp);
  virtual ~JobManager();

  WorkerTree *getTree() {
    return tree;
  }

  WorkerTree::Node *getCurrentNode();

  JobSelectionStrategy *getStrategy() {
    return selStrategy;
  }

  void lockJobs() {
    jobsMutex.lock();
  }
  void unlockJobs() {
    jobsMutex.unlock();
  }

  unsigned getModuleCRC() const;

  void processJobs(bool standAlone, unsigned int timeOut = 0);
  void replayJobs(ExecutionPathSetPin paths, unsigned int timeOut = 0);

  void finalize();

  virtual bool onStateBranching(klee::ExecutionState *state, klee::ForkTag forkTag);
  virtual void onStateBranched(klee::ExecutionState *state,
      klee::ExecutionState *parent, int index, klee::ForkTag forkTag);
  virtual void onStateDestroy(klee::ExecutionState *state);
  virtual void onControlFlowEvent(klee::ExecutionState *state,
      ControlFlowEvent event);
  virtual void onDebugInfo(klee::ExecutionState *state,
      const std::string &message);
  virtual void onOutOfResources(klee::ExecutionState *destroyedState);
  virtual void onBreakpoint(klee::ExecutionState *state,
          unsigned int id);

  /*
   * Statistics methods
   */

  void getStatisticsData(std::vector<int> &data, ExecutionPathSetPin &paths,
      bool onlyChanged);

  unsigned int getJobCount() const { return jobCount; }

  void setRefineStatistics() {
    refineStats = true;
  }

  void requestTermination() {
    terminationRequest = true;
    // Wake up the manager
    jobsAvailabe.notify_all();
  }

  /*
   * Coverage related functionality
   */

  void getUpdatedLocalCoverage(cov_update_t &data);
  void setUpdatedGlobalCoverage(const cov_update_t &data);
  uint32_t getCoverageIDCount() const;

  /*
   * Job import/export methods
   */
  void importJobs(ExecutionPathSetPin paths,
      std::vector<unsigned int> *strategies);
  ExecutionPathSetPin exportJobs(ExecutionPathSetPin seeds,
      std::vector<int> &counts, std::vector<unsigned int> *strategies);
};

}
}

#endif /* JOBMANAGER_H_ */
