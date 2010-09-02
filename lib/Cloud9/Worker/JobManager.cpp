/*
 * JobManager.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

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
 */

#include "cloud9/worker/JobManager.h"
#include "cloud9/worker/TreeObjects.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/KleeCommon.h"
#include "cloud9/worker/CoreStrategies.h"
#include "cloud9/worker/ComplexStrategies.h"
#include "cloud9/worker/StrategyPortfolio.h"
#include "cloud9/worker/OracleStrategy.h"
#include "cloud9/Logger.h"
#include "cloud9/Common.h"
#include "cloud9/ExecutionTree.h"
#include "cloud9/instrum/InstrumentationManager.h"
#include "cloud9/instrum/LocalFileWriter.h"

#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Instructions.h"
#include "llvm/System/TimeValue.h"
#include "llvm/System/Path.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "klee/Interpreter.h"
#include "klee/Statistics.h"
#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/System/Time.h"
#include "klee/Internal/ADT/RNG.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/KleeHandler.h"
#include "klee/Init.h"

#include "../../Core/Common.h"

#include <boost/io/ios_state.hpp>
#include <boost/crc.hpp>
#include <boost/bind.hpp>
#include <stack>
#include <map>
#include <set>
#include <fstream>
#include <iostream>
#include <iomanip>

using llvm::sys::TimeValue;
using namespace llvm;

namespace klee {
namespace stats {
extern Statistic locallyCoveredInstructions;
extern Statistic globallyCoveredInstructions;
extern Statistic locallyUncoveredInstructions;
extern Statistic globallyUncoveredInstructions;
}
}

namespace {
cl::opt<unsigned> MakeConcreteSymbolic("make-concrete-symbolic", cl::desc(
  "Rate at which to make concrete reads symbolic (0=off)"), cl::init(0));

cl::opt<bool> OptimizeModule("optimize", cl::desc("Optimize before execution"));

cl::opt<bool> CheckDivZero("check-div-zero", cl::desc(
  "Inject checks for division-by-zero"), cl::init(true));

cl::opt<bool> WarnAllExternals("warn-all-externals", cl::desc(
  "Give initial warning for all externals."));

cl::list<unsigned int> CodeBreakpoints("c9-code-bp", cl::desc(
  "Breakpoints in the LLVM assembly file"));

cl::opt<bool> BreakOnReplayBroken("c9-bp-on-replaybr", cl::desc(
  "Break on last valid position if a broken replay occurrs."));

cl::opt<bool>
  DumpStateTraces("c9-dump-traces",
  cl::desc("Dump state traces when a breakpoint or any other relevant event happens during execution"),
  cl::init(false));

cl::opt<bool>
  DumpInstrTraces("c9-dump-instr",
  cl::desc("Dump instruction traces for each finished state. Designed to be used for concrete executions."),
  cl::init(false));

cl::opt<std::string>
  OraclePath("c9-oracle-path", cl::desc("The file used by the oracle strategy to get the path."));

cl::opt<double>
  JobQuanta("c9-job-quanta", cl::desc("The maximum quantum of time for a job"),
  cl::init(1.0));

}

using namespace klee;

namespace cloud9 {

namespace worker {

/*******************************************************************************
 * HELPER FUNCTIONS FOR THE JOB MANAGER
 ******************************************************************************/

bool JobManager::isJob(WorkerTree::Node *node) {
  return (**node).getJob() != NULL;
}

bool JobManager::isExportableJob(WorkerTree::Node *node) {
  ExecutionJob *job = (**node).getJob();
  if (!job)
    return false;

  if (job == currentJob)
    return false;

  return true;
}

void JobManager::serializeInstructionTrace(std::ostream &s,
    WorkerTree::Node *node) {
  std::vector<int> path;

  tree->buildPath(WORKER_LAYER_STATES, node, tree->getRoot(), path);

  const WorkerTree::Node *crtNode = tree->getRoot();
  unsigned int count = 0;

  bool enabled = false;

  for (unsigned int i = 0; i <= path.size(); i++) {
    const ExecutionTrace &trace = (**crtNode).getTrace();

    for (ExecutionTrace::const_iterator it = trace.getEntries().begin();
        it != trace.getEntries().end(); it++) {

      if (InstructionTraceEntry *instEntry = dynamic_cast<InstructionTraceEntry*>(*it)) {
        if (enabled) {
          unsigned int opcode = instEntry->getInstruction()->inst->getOpcode();
          s.write((char*)&opcode, sizeof(unsigned int));
        }
        count++;
      } else if (BreakpointEntry *brkEntry = dynamic_cast<BreakpointEntry*>(*it)) {
        if (!enabled && brkEntry->getID() == 42) {
          CLOUD9_DEBUG("Starting to serialize. Skipped " << count << " instructions.");
          enabled = true;
        }
      }
    }

    if (i < path.size()) {
      crtNode = crtNode->getChild(WORKER_LAYER_STATES, path[i]);
      assert(crtNode);
    }
  }

  CLOUD9_DEBUG("Serialized " << count << " instructions.");
}

void JobManager::parseInstructionTrace(std::istream &s,
    std::vector<unsigned int> &dest) {

  dest.clear();

  while (!s.eof()) {
    unsigned int instID;

    s.read((char*)&instID, sizeof(unsigned int));

    if (!s.fail()) {
      dest.push_back(instID);
    }
  }

  CLOUD9_DEBUG("Parsed " << dest.size() << " instructions.");
}

void JobManager::serializeExecutionTrace(std::ostream &os,
    WorkerTree::Node *node) { // XXX very slow - read the .ll file and use it instead
  std::vector<int> path;

  tree->buildPath(WORKER_LAYER_STATES, node, tree->getRoot(), path);

  WorkerTree::Node *crtNode = tree->getRoot();
  const llvm::BasicBlock *crtBasicBlock = NULL;
  const llvm::Function *crtFunction = NULL;

  llvm::raw_os_ostream raw_os(os);

  for (unsigned int i = 0; i <= path.size(); i++) {
    const ExecutionTrace &trace = (**crtNode).getTrace();
    // Output each instruction in the node
    for (ExecutionTrace::const_iterator it = trace.getEntries().begin(); it
        != trace.getEntries().end(); it++) {
      if (InstructionTraceEntry *instEntry = dynamic_cast<InstructionTraceEntry*>(*it)) {
        klee::KInstruction *ki = instEntry->getInstruction();
        bool newBB = false;
        bool newFn = false;

        if (ki->inst->getParent() != crtBasicBlock) {
          crtBasicBlock = ki->inst->getParent();
          newBB = true;
        }

        if (crtBasicBlock != NULL && crtBasicBlock->getParent() != crtFunction) {
          crtFunction = crtBasicBlock->getParent();
          newFn = true;
        }

        if (newFn) {
          os << std::endl;
          os << "   Function '"
              << ((crtFunction != NULL) ? crtFunction->getNameStr() : "")
              << "':" << std::endl;
        }

        if (newBB) {
          os << "----------- "
              << ((crtBasicBlock != NULL) ? crtBasicBlock->getNameStr() : "")
              << " ----" << std::endl;
        }

        boost::io::ios_all_saver saver(os);
        os << std::setw(9) << ki->info->assemblyLine << ": ";
        saver.restore();
        ki->inst->print(raw_os, NULL);
        os << std::endl;
      } else if (DebugLogEntry *logEntry = dynamic_cast<DebugLogEntry*>(*it)) {
        os << logEntry->getMessage() << std::endl;
      }
    }

    if (i < path.size()) {
      crtNode = crtNode->getChild(WORKER_LAYER_STATES, path[i]);
    }
  }
}

void JobManager::serializeExecutionTrace(std::ostream &os, SymbolicState *state) {
  WorkerTree::Node *node = state->getNode().get();
  serializeExecutionTrace(os, node);
}

/*******************************************************************************
 * JOB MANAGER METHODS
 ******************************************************************************/

/* Initialization Methods *****************************************************/

JobManager::JobManager(llvm::Module *module, std::string mainFnName, int argc,
    char **argv, char **envp) :
  terminationRequest(false), currentJob(NULL), replaying(false), jobCount(0), traceCounter(0) {

  tree = new WorkerTree();
  cTree = new CompressedTree();

  llvm::Function *mainFn = module->getFunction(mainFnName);

  collectTraces = DumpStateTraces || DumpInstrTraces;

  initialize(module, mainFn, argc, argv, envp);
}

void JobManager::initialize(llvm::Module *module, llvm::Function *_mainFn,
    int argc, char **argv, char **envp) {
  mainFn = _mainFn;
  assert(mainFn);

  klee::Interpreter::InterpreterOptions iOpts;
  iOpts.MakeConcreteSymbolic = MakeConcreteSymbolic;

  llvm::sys::Path libraryPath(getKleeLibraryPath());

  klee::Interpreter::ModuleOptions mOpts(libraryPath.c_str(),
  /*Optimize=*/OptimizeModule,
  /*CheckDivZero=*/CheckDivZero);

  kleeHandler = new klee::KleeHandler(argc, argv);
  interpreter = klee::Interpreter::create(iOpts, kleeHandler);
  kleeHandler->setInterpreter(interpreter);

  symbEngine = dynamic_cast<SymbolicEngine*> (interpreter);
  interpreter->setModule(module, mOpts);

  kleeModule = symbEngine->getModule();

  klee::externalsAndGlobalsCheck(kleeModule->module);

  symbEngine->registerStateEventHandler(this);

  theStatisticManager->trackChanges(stats::locallyCoveredInstructions);

  initStrategy();
  initStatistics();
  initBreakpoints();

  initRootState(mainFn, argc, argv, envp);
}

void JobManager::initRootState(llvm::Function *f, int argc, char **argv,
    char **envp) {
  klee::ExecutionState *kState = symbEngine->createRootState(f);
  SymbolicState *state = new SymbolicState(kState);

  state->rebindToNode(tree->getRoot());
  state->rebindToCompressedNode(cTree->getRoot());

  symbEngine->initRootState(kState, argc, argv, envp);
}

void JobManager::initStrategy() {
  std::vector<JobSelectionStrategy*> strategies;

  switch (JobSelection) {
  case RandomSel:
    selStrategy = new RandomStrategy();
    CLOUD9_INFO("Using random job selection strategy");
    break;
  case RandomPathSel:
    selStrategy = new RandomPathStrategy(tree, cTree);
    CLOUD9_INFO("Using random path job selection strategy");
    break;
  case CoverageOptimizedSel:
    strategies.push_back(new WeightedRandomStrategy(
        WeightedRandomStrategy::CoveringNew, tree, symbEngine));
    strategies.push_back(new RandomPathStrategy(tree, cTree));
    selStrategy = new TimeMultiplexedStrategy(strategies);
    CLOUD9_INFO("Using weighted random job selection strategy");
    break;
  case PortfolioSel:
    selStrategy = createStrategyPortfolio();
    CLOUD9_INFO("Using the strategy portfolio");
    break;
  case OracleSel:
    selStrategy = createOracleStrategy();
    CLOUD9_INFO("Using the oracle");
    break;

  default:
    assert(0 && "undefined job selection strategy");
  }

  // Wrap this in a batching strategy, to speed up things
  //selStrategy = new BatchingStrategy(selStrategy);
}

StrategyPortfolio *JobManager::createStrategyPortfolio() {
  std::map<unsigned int, JobSelectionStrategy*> strategies;

  strategies[RANDOM_PATH_STRATEGY] = new RandomPathStrategy(tree, cTree);
  strategies[WEIGHTED_RANDOM_STRATEGY] = new WeightedRandomStrategy(
      WeightedRandomStrategy::CoveringNew, tree, symbEngine);
  strategies[RANDOM_STRATEGY] = new RandomStrategy();

  StrategyPortfolio *result = new StrategyPortfolio(this, strategies);

  return result;
}

OracleStrategy *JobManager::createOracleStrategy() {
  std::vector<unsigned int> goalPath;

  std::ifstream ifs(OraclePath);

  assert(!ifs.fail());

  parseInstructionTrace(ifs, goalPath);

  OracleStrategy *result = new OracleStrategy(tree, goalPath);

  return result;
}

void JobManager::initStatistics() {
  // Configure the root as a statistics node
  WorkerTree::NodePin rootPin = tree->getRoot()->pin(WORKER_LAYER_STATISTICS);

  stats.insert(rootPin);
  statChanged = true;
  refineStats = false;
}

void JobManager::initBreakpoints() {
  // Register code breakpoints
  for (unsigned int i = 0; i < CodeBreakpoints.size(); i++) {
    setCodeBreakpoint(CodeBreakpoints[i]);
  }
}

/* Finalization Methods *******************************************************/

JobManager::~JobManager() {
  if (symbEngine != NULL) {
    delete symbEngine;
  }

  cloud9::instrum::theInstrManager.stop();
}

void JobManager::finalize() {
  symbEngine->deregisterStateEventHandler(this);
  symbEngine->destroyStates();

  CLOUD9_INFO("Finalized job execution.");
}

/* Misc. Methods **************************************************************/

unsigned JobManager::getModuleCRC() const {
  std::string moduleContents;
  llvm::raw_string_ostream os(moduleContents);

  kleeModule->module->print(os, NULL);

  os.flush();

  boost::crc_ccitt_type crc;
  crc.process_bytes(moduleContents.c_str(), moduleContents.size());

  return crc.checksum();
}

WorkerTree::Node *JobManager::getCurrentNode() {
  if (!currentJob)
    return NULL;

  return currentJob->getNode().get();
}

/* Job Manipulation Methods ***************************************************/

void JobManager::processJobs(unsigned int timeOut) {
  if (timeOut > 0) {
    CLOUD9_INFO("Processing jobs with a timeout of " << timeOut << " seconds.");
  }
  processLoop(true, true, timeOut);
}

void JobManager::processJobs(ExecutionPathSetPin paths, unsigned int timeOut) {
  // First, we need to import the jobs in the manager
  importJobs(paths, NULL);

  // Then we execute them, but only them (non blocking, don't allow growth),
  // until the queue is exhausted
  processLoop(false, false, timeOut);
}

void JobManager::processLoop(bool allowGrowth, bool blocking,
    unsigned int timeOut) {
  ExecutionJob *job = NULL;

  boost::unique_lock<boost::mutex> lock(jobsMutex);

  TimeValue deadline = TimeValue::now() + TimeValue(timeOut, 0);

  while (!terminationRequest) {
    TimeValue now = TimeValue::now();

    if (timeOut > 0 && now > deadline) {
      CLOUD9_INFO("Timeout reached. Suspending execution.");
      break;
    }

    if (blocking) {
      if (timeOut > 0) {
        TimeValue remaining = deadline - now;

        job = selectNextJob(lock, remaining.seconds());
      } else {
        job = selectNextJob(lock, 0);
      }
    } else {
      job = selectNextJob();
    }

    if (blocking && timeOut == 0) {
      assert(job != NULL);
    } else {
      if (job == NULL)
        break;
    }

    executeJobsBatch(lock, job, allowGrowth);

    if (refineStats) {
      refineStatistics();
      refineStats = false;
    }
  }

  if (terminationRequest)
    CLOUD9_INFO("Termination was requested.");
}

ExecutionJob* JobManager::selectNextJob(boost::unique_lock<boost::mutex> &lock,
    unsigned int timeOut) {
  ExecutionJob *job = selStrategy->onNextJobSelection();

  while (job == NULL) {
    CLOUD9_INFO("No jobs in the queue, waiting for...");
    cloud9::instrum::theInstrManager.recordEvent(
        cloud9::instrum::JobExecutionState, "idle");

    bool result = true;
    if (timeOut > 0)
      result = jobsAvailabe.timed_wait(lock,
          boost::posix_time::seconds(timeOut));
    else
      jobsAvailabe.wait(lock);

    if (!result) {
      CLOUD9_INFO("Timeout while waiting for new jobs. Aborting.");
      return NULL;
    } else
      CLOUD9_INFO("More jobs available. Resuming exploration...");

    job = selStrategy->onNextJobSelection();

    if (job != NULL)
      cloud9::instrum::theInstrManager.recordEvent(
          cloud9::instrum::JobExecutionState, "working");
  }

  return job;
}

ExecutionJob* JobManager::selectNextJob() {
  ExecutionJob *job = selStrategy->onNextJobSelection();

  return job;
}

void JobManager::submitJob(ExecutionJob* job, bool activateStates) {
  WorkerTree::Node *node = job->getNode().get();
  assert((**node).symState || job->isImported());

  fireAddJob(job);

  if (activateStates) {
    // Check for the state on the supporting branch
    while (node) {
      SymbolicState *state = (**node).getSymbolicState();

      if (state) {
        fireActivateState(state);
        break;
      }

      node = node->getParent();
    }
  }

  jobCount++;

}

void JobManager::finalizeJob(ExecutionJob *job, bool deactivateStates) {
  WorkerTree::Node *node = job->getNode().get();

  job->removing = true;

  if (deactivateStates) {
    while (node) {
      if (node->getCount(WORKER_LAYER_JOBS) > 1)
        break; // We reached a junction

      SymbolicState *state = (**node).getSymbolicState();

      if (state) {
        fireDeactivateState(state);
        break;
      }

      node = node->getParent();
    }
  }

  fireRemovingJob(job);

  delete job;

  jobCount--;
}

void JobManager::selectJobs(WorkerTree::Node *root,
    std::vector<ExecutionJob*> &jobSet, int maxCount) {

  std::vector<WorkerTree::Node*> nodes;

  tree->getLeaves(WORKER_LAYER_JOBS, root, boost::bind(
      &JobManager::isExportableJob, this, _1), maxCount, nodes);

  for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin(); it
      != nodes.end(); it++) {
    WorkerTree::Node* node = *it;
    jobSet.push_back((**node).getJob());
  }

  CLOUD9_DEBUG("Selected " << jobSet.size() << " jobs");

}

unsigned int JobManager::countJobs(WorkerTree::Node *root) {
  return tree->countLeaves(WORKER_LAYER_JOBS, root, &isJob);
}

void JobManager::importJobs(ExecutionPathSetPin paths,
    std::vector<unsigned int> *strategies) {
  boost::unique_lock<boost::mutex> lock(jobsMutex);

  std::vector<WorkerTree::Node*> nodes;
  std::vector<ExecutionJob*> jobs;

  tree->getNodes(WORKER_LAYER_JOBS, paths, nodes);

  CLOUD9_DEBUG("Importing " << paths->count() << " jobs");

  for (unsigned int i = 0; i < nodes.size(); i++) {
    WorkerTree::Node *crtNode = nodes[i];
    assert(crtNode->getCount(WORKER_LAYER_JOBS) == 0
        && "Job duplication detected");
    assert((!crtNode->layerExists(WORKER_LAYER_STATES) || crtNode->getCount(WORKER_LAYER_STATES) == 0)
        && "Job before the state frontier");

    if (crtNode->getCount(WORKER_LAYER_JOBS) > 0) {
      CLOUD9_INFO("Discarding job as being obsolete: " << *crtNode);
    } else {
      // The exploration job object gets a pin on the node, thus
      // ensuring it will be released automatically after it's no
      // longer needed
      ExecutionJob *job = new ExecutionJob(crtNode, true);

      if (strategies != NULL)
        job->_strategy = (*strategies)[i];

      jobs.push_back(job);
    }
  }

  submitJobs(jobs.begin(), jobs.end(), true);

  cloud9::instrum::theInstrManager.incStatistic(
      cloud9::instrum::TotalImportedJobs, jobs.size());
  cloud9::instrum::theInstrManager.incStatistic(
      cloud9::instrum::TotalTreePaths, jobs.size());
}

ExecutionPathSetPin JobManager::exportJobs(ExecutionPathSetPin seeds,
    std::vector<int> &counts, std::vector<unsigned int> *strategies) {
  boost::unique_lock<boost::mutex> lock(jobsMutex);

  std::vector<WorkerTree::Node*> roots;
  std::vector<ExecutionJob*> jobs;
  std::vector<WorkerTree::Node*> jobRoots;

  tree->getNodes(WORKER_LAYER_JOBS, seeds, roots);

  assert(roots.size() == counts.size());

  for (unsigned int i = 0; i < seeds->count(); i++) {
    selectJobs(roots[i], jobs, counts[i]);
  }

  for (std::vector<ExecutionJob*>::iterator it = jobs.begin(); it != jobs.end(); it++) {
    ExecutionJob *job = *it;
    jobRoots.push_back(job->getNode().get());

    if (strategies != NULL)
      strategies->push_back(job->_strategy);
  }

  // Do this before de-registering the jobs, in order to keep the nodes pinned
  ExecutionPathSetPin paths = tree->buildPathSet(jobRoots.begin(),
      jobRoots.end());

  // De-register the jobs with the worker
  for (std::vector<ExecutionJob*>::iterator it = jobs.begin(); it != jobs.end(); it++) {
    // Cancel each job
    ExecutionJob *job = *it;
    assert(currentJob != job);

    job->exported = true;
    job->removing = true;

    finalizeJob(job, true);
  }

  cloud9::instrum::theInstrManager.incStatistic(
      cloud9::instrum::TotalExportedJobs, paths->count());
  cloud9::instrum::theInstrManager.decStatistic(
      cloud9::instrum::TotalTreePaths, paths->count());

  return paths;
}

/* Strategy Handler Triggers *************************************************/

void JobManager::fireActivateState(SymbolicState *state) {
  if (!state->_active) {
    state->_active = true;
    state->cActiveNodePin = cTree->getNode(COMPRESSED_LAYER_ACTIVE,
        state->cNodePin.get())->pin(COMPRESSED_LAYER_ACTIVE);

    selStrategy->onStateActivated(state);
    cloud9::instrum::theInstrManager.incStatistic(
        cloud9::instrum::CurrentActiveStateCount);
  }
}

void JobManager::fireDeactivateState(SymbolicState *state) {
  if (state->_active) {
    state->_active = false;
    state->cActiveNodePin.reset();

    selStrategy->onStateDeactivated(state);
    cloud9::instrum::theInstrManager.decStatistic(
        cloud9::instrum::CurrentActiveStateCount);
  }
}

void JobManager::fireUpdateState(SymbolicState *state) {
  if (state->_active) {
    selStrategy->onStateUpdated(state);
  }
}

void JobManager::fireAddJob(ExecutionJob *job) {
  selStrategy->onJobAdded(job);
  cloud9::instrum::theInstrManager.incStatistic(
      cloud9::instrum::CurrentJobCount);
}

void JobManager::fireRemovingJob(ExecutionJob *job) {
  selStrategy->onRemovingJob(job);
  cloud9::instrum::theInstrManager.decStatistic(
      cloud9::instrum::CurrentJobCount);
}

/* Job Execution Methods ******************************************************/

void JobManager::executeJobsBatch(boost::unique_lock<boost::mutex> &lock,
      ExecutionJob *origJob, bool spawnNew) {
  WorkerTree::NodePin nodePin = origJob->getNode();

  if ((**nodePin).getSymbolicState() == NULL) {
    // Replay job
    executeJob(lock, origJob, spawnNew);
    return;
  }

  double startTime = klee::util::getUserTime();
  double currentTime = startTime;

  unsigned int count = 0;

  while (currentTime - startTime < JobQuanta) {
    executeJob(lock, (**nodePin).getJob(), spawnNew);
    count++;

    if ((**nodePin).getJob() == NULL) {
      WorkerTree::Node *newNode = tree->selectRandomLeaf(WORKER_LAYER_JOBS, nodePin.get(), theRNG);

      if ((**newNode).getJob() == NULL)
        break;

      nodePin = (**newNode).getJob()->getNode();
    }

    currentTime = klee::util::getUserTime();
  }

  CLOUD9_DEBUG("Batched " << count << " jobs");
}

void JobManager::executeJob(boost::unique_lock<boost::mutex> &lock,
    ExecutionJob *job, bool spawnNew) {
  WorkerTree::NodePin nodePin = job->getNode(); // Keep the node around until we finish with it

  currentJob = job;

  if ((**nodePin).symState == NULL) {
    if (!job->isImported()) {
      CLOUD9_INFO("Replaying path for non-foreign job. Most probably this job will be lost.");
    }

    cloud9::instrum::theInstrManager.recordEvent(
        cloud9::instrum::JobExecutionState, "startReplay");

    replayPath(lock, nodePin.get());

    cloud9::instrum::theInstrManager.recordEvent(
        cloud9::instrum::JobExecutionState, "endReplay");

    cloud9::instrum::theInstrManager.incStatistic(
        cloud9::instrum::TotalReplayedJobs);
  } else {
    if (job->isImported()) {
      CLOUD9_INFO("Foreign job with no replay needed. Probably state was obtained through other neighbor replays.");
    }
  }

  job->imported = false;

  if ((**nodePin).symState == NULL) {
    CLOUD9_INFO("Job canceled before start");
    cloud9::instrum::theInstrManager.incStatistic(
        cloud9::instrum::TotalDroppedJobs);
  } else {
    stepInNode(lock, nodePin.get(), false);
  }

  currentJob = NULL;

  if ((**nodePin).symState == NULL) {
    // Save the job strategy - it is inherited by the new jobs
    unsigned int strategy = job->_strategy;

    // Job finished here, need to remove it
    finalizeJob(job, false);

    cloud9::instrum::theInstrManager.incStatistic(
        cloud9::instrum::TotalProcJobs);

    // Spawn new jobs if there are states left
    if (nodePin->layerExists(WORKER_LAYER_STATES)) {
      std::vector<WorkerTree::Node*> nodes;
      tree->getLeaves(WORKER_LAYER_STATES, nodePin.get(), nodes);

      //CLOUD9_DEBUG("New jobs: " << nodes.size());

      for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin(); it
          != nodes.end(); it++) {
        WorkerTree::Node *node = tree->getNode(WORKER_LAYER_JOBS, *it);
        assert((**node).symState != NULL);
        ExecutionJob *newJob = new ExecutionJob(node, false);
        newJob->_strategy = strategy;

        submitJob(newJob, false);
      }

      if (nodes.size() > 0) {
        cloud9::instrum::theInstrManager.incStatistic(
            cloud9::instrum::TotalTreePaths, nodes.size() - 1);
      }
    }
  } else {
    // Just mark the state as updated
    fireUpdateState((**nodePin).symState);
  }
}

void JobManager::stepInNode(boost::unique_lock<boost::mutex> &lock,
    WorkerTree::Node *node, bool exhaust) {
  assert((**node).symState != NULL);

  // Keep the node alive until we finish with it
  WorkerTree::NodePin nodePin = node->pin(WORKER_LAYER_STATES);

  while ((**node).symState != NULL) {

    SymbolicState *state = (**node).symState;

    if (!codeBreaks.empty()) {
      if (codeBreaks.find(state->getKleeState()->pc()->info->assemblyLine)
          != codeBreaks.end()) {
        // We hit a breakpoint
        fireBreakpointHit(node);
      }
    }

    CLOUD9_DEBUG("Stepping in instruction " << state->getKleeState()->pc()->info->assemblyLine);

    if (state->collectProgress) {
      state->_instrProgress.push_back(state->getKleeState()->pc());
    }

    // Execute the instruction
    lock.unlock();

    symbEngine->stepInState(state->getKleeState());
    lock.lock();

    cloud9::instrum::theInstrManager.incStatistic(
        cloud9::instrum::TotalProcInstructions);
    if (replaying)
      cloud9::instrum::theInstrManager.incStatistic(
          cloud9::instrum::TotalReplayInstructions);

    if (!exhaust)
      break;
  }
}

void JobManager::replayPath(boost::unique_lock<boost::mutex> &lock,
    WorkerTree::Node *pathEnd) {
  std::vector<int> path;

  WorkerTree::Node *crtNode = pathEnd;

  //CLOUD9_DEBUG("Replaying path: " << *crtNode);

  while (crtNode != NULL && (**crtNode).symState == NULL) {
    path.push_back(crtNode->getIndex());

    crtNode = crtNode->getParent();
  }

  if (crtNode == NULL) {
    CLOUD9_ERROR("Cannot find the seed execution state.");
    return;
  }

  std::reverse(path.begin(), path.end());

  //CLOUD9_DEBUG("Started path replay at position: " << *crtNode);

  replaying = true;

  // Perform the replay work
  for (unsigned int i = 0; i < path.size(); i++) {
    if (!crtNode->layerExists(WORKER_LAYER_STATES)) {
      // We have a broken replay
      break;
    }

    if ((**crtNode).symState != NULL) {
      stepInNode(lock, crtNode, true);
    }

    crtNode = crtNode->getChild(WORKER_LAYER_JOBS, path[i]);
    assert(crtNode != NULL);
  }

  replaying = false;

  if (!crtNode->layerExists(WORKER_LAYER_STATES)) {
    assert((**crtNode).symState == NULL);
    CLOUD9_ERROR("Replay broken, NULL state at the end of the path.");
    if (BreakOnReplayBroken) {
      fireBreakpointHit(crtNode->getParent());
    }
  }
}

/* Symbolic Engine Callbacks **************************************************/

bool JobManager::onStateBranching(klee::ExecutionState *state, int reason) {
  return true; // For now...
}

void JobManager::onStateBranched(klee::ExecutionState *kState,
    klee::ExecutionState *parent, int index, int reason) {
  boost::unique_lock<boost::mutex> lock(jobsMutex);

  assert(parent);

  //if (kState)
  //	CLOUD9_DEBUG("State branched: " << parent->getCloud9State()->getNode());

  updateTreeOnBranch(kState, parent, index);
  updateCompressedTreeOnBranch(kState ? kState->getCloud9State() : NULL,
      parent->getCloud9State());

  if (kState) {
    SymbolicState *state = kState->getCloud9State();

    if (parent->getCloud9State()->collectProgress) {
      state->collectProgress = true;
      state->_instrProgress = parent->getCloud9State()->_instrProgress;
      state->_instrPos = parent->getCloud9State()->_instrPos;
    }

    if (state->getNode()->layerExists(WORKER_LAYER_JOBS) || !replaying) {
      fireActivateState(state);
    }

    //CLOUD9_DEBUG("State forked at level " << state->getNode()->getLevel());
  }

  SymbolicState *pState = parent->getCloud9State();

  if (pState->getNode()->layerExists(WORKER_LAYER_JOBS) || !replaying) {
    fireUpdateState(pState);
  } else {
    fireDeactivateState(pState);
  }

}

void JobManager::onStateDestroy(klee::ExecutionState *kState) {
  boost::unique_lock<boost::mutex> lock(jobsMutex);

  assert(kState);

  SymbolicState *state = kState->getCloud9State();

  if (DumpInstrTraces) {
    dumpInstructionTrace(state->getNode().get());
  }

  fireDeactivateState(state);

  updateCompressedTreeOnDestroy(kState->getCloud9State());
  updateTreeOnDestroy(kState);
}

void JobManager::onOutOfResources(klee::ExecutionState *destroyedState) {
  // TODO: Implement a job migration mechanism
  CLOUD9_INFO("Executor ran out of resources. Dropping state.");
}

void JobManager::onBreakpoint(klee::ExecutionState *kState,
          unsigned int id) {
  WorkerTree::Node *node = kState->getCloud9State()->getNode().get();

  if (collectTraces) {
    (**node).trace.appendEntry(new BreakpointEntry(id));
  }

  if (id == 42) {
    kState->getCloud9State()->collectProgress = true; // Enable progress collection in the manager
  }
}

void JobManager::onControlFlowEvent(klee::ExecutionState *kState,
    ControlFlowEvent event) {
  WorkerTree::Node *node = kState->getCloud9State()->getNode().get();

  // Add the instruction to the node trace
  if (collectTraces) {
    switch (event) {
    case STEP:
      (**node).trace.appendEntry(new InstructionTraceEntry(kState->pc()));
      break;
    case BRANCH_FALSE:
    case BRANCH_TRUE:
      //(**node).trace.appendEntry(new ConstraintLogEntry(state));
      (**node).trace.appendEntry(new ControlFlowEntry(true, false, false));
      break;
    case CALL:
      break;
    case RETURN:
      break;
    }

  }
}

void JobManager::onDebugInfo(klee::ExecutionState *kState,
    const std::string &message) {
  WorkerTree::Node *node = kState->getCloud9State()->getNode().get();

  if (collectTraces) {
    (**node).trace.appendEntry(new DebugLogEntry(message));
  }
}

void JobManager::fireBreakpointHit(WorkerTree::Node *node) {
  SymbolicState *state = (**node).symState;

  CLOUD9_INFO("Breakpoint hit!");
  CLOUD9_DEBUG("State at position: " << *node);

  if (state) {
    CLOUD9_DEBUG("State stack trace: " << *state);
    klee::ExprPPrinter::printConstraints(std::cerr,
        state->getKleeState()->constraints());
    dumpStateTrace(node);
  }

  // Also signal a breakpoint, for stopping GDB
  cloud9::breakSignal();
}

void JobManager::updateTreeOnBranch(klee::ExecutionState *kState,
    klee::ExecutionState *parent, int index) {
  WorkerTree::NodePin pNodePin = parent->getCloud9State()->getNode();

  WorkerTree::Node *newNode, *oldNode;

  // Obtain the new node pointers
  oldNode = tree->getNode(WORKER_LAYER_STATES, pNodePin.get(), 1 - index);
  parent->getCloud9State()->rebindToNode(oldNode);

  if (kState) {
    newNode = tree->getNode(WORKER_LAYER_STATES, pNodePin.get(), index);
    SymbolicState *state = new SymbolicState(kState);
    state->rebindToNode(newNode);

  }
}

void JobManager::updateTreeOnDestroy(klee::ExecutionState *kState) {
  SymbolicState *state = kState->getCloud9State();
  state->rebindToNode(NULL);

  kState->setCloud9State(NULL);
  delete state;
}

void JobManager::updateCompressedTreeOnBranch(SymbolicState *state,
    SymbolicState *parent) {
  if (state == NULL) {
    // Ignore "degenerated" branches
    return;
  }

  CompressedTree::NodePin pNodePin = parent->getCompressedNode();

  CompressedTree::Node *oldNode = cTree->getNode(COMPRESSED_LAYER_STATES,
      pNodePin.get(), parent->getNode()->getIndex());
  CompressedTree::Node *newNode = cTree->getNode(COMPRESSED_LAYER_STATES,
      pNodePin.get(), state->getNode()->getIndex());

  if (parent->_active)
    oldNode = cTree->getNode(COMPRESSED_LAYER_ACTIVE, oldNode);

  if (state->_active)
    newNode = cTree->getNode(COMPRESSED_LAYER_ACTIVE, newNode);

  parent->rebindToCompressedNode(oldNode);
  state->rebindToCompressedNode(newNode);
}

void JobManager::updateCompressedTreeOnDestroy(SymbolicState *state) {
  CompressedTree::Node *parent = state->getCompressedNode()->getParent();

  state->rebindToCompressedNode(NULL);

  if (parent != NULL) {
    // The parent now should have a single child
    cTree->collapseNode(parent);
  }
}

/* Statistics Management ******************************************************/

void JobManager::refineStatistics() {
  std::set<WorkerTree::NodePin> newStats;

  for (std::set<WorkerTree::NodePin>::iterator it = stats.begin(); it
      != stats.end(); it++) {
    const WorkerTree::NodePin &nodePin = *it;

    if (!nodePin->layerExists(WORKER_LAYER_JOBS)) {
      // Statistic node invalidated
      continue;
    }

    if (nodePin->getCount(WORKER_LAYER_JOBS) > 0) {
      // Move along the path

      WorkerTree::Node *left = nodePin->getChild(WORKER_LAYER_JOBS, 0);
      WorkerTree::Node *right = nodePin->getChild(WORKER_LAYER_JOBS, 1);

      if (left) {
        WorkerTree::NodePin leftPin = tree->getNode(WORKER_LAYER_STATISTICS,
            left)->pin(WORKER_LAYER_STATISTICS);
        newStats.insert(leftPin);
      }

      if (right) {
        WorkerTree::NodePin rightPin = tree->getNode(WORKER_LAYER_STATISTICS,
            right)->pin(WORKER_LAYER_STATISTICS);
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

  for (std::set<WorkerTree::NodePin>::iterator it = stats.begin(); it
      != stats.end();) {
    std::set<WorkerTree::NodePin>::iterator oldIt = it++;
    WorkerTree::NodePin nodePin = *oldIt;

    if (!nodePin->layerExists(WORKER_LAYER_JOBS)) {
      stats.erase(oldIt);
      statChanged = true;
    }
  }

  if (stats.empty()) {
    // Add back the root state in the statistics
    WorkerTree::NodePin rootPin = tree->getRoot()->pin(WORKER_LAYER_STATISTICS);
    stats.insert(rootPin);
    statChanged = true;
  }
}

#if 0
void JobManager::getStatisticsData(std::vector<int> &data,
    ExecutionPathSetPin &paths, bool onlyChanged) {
  boost::unique_lock<boost::mutex> lock(jobsMutex);

  cleanupStatistics();

  if (statChanged || !onlyChanged) {
    std::vector<WorkerTree::Node*> newStats;
    for (std::set<WorkerTree::NodePin>::iterator it = stats.begin(); it
        != stats.end(); it++) {
      newStats.push_back((*it).get());
    }

    paths = tree->buildPathSet(newStats.begin(), newStats.end());
    statChanged = false;

    //CLOUD9_DEBUG("Sent node set: " << getASCIINodeSet(newStats.begin(), newStats.end()));
  }

  data.clear();

  for (std::set<WorkerTree::NodePin>::iterator it = stats.begin(); it
      != stats.end(); it++) {
    const WorkerTree::NodePin &crtNodePin = *it;
    unsigned int jobCount = countJobs(crtNodePin.get());
    data.push_back(jobCount);
  }

  //CLOUD9_DEBUG("Sent data set: " << getASCIIDataSet(data.begin(), data.end()));
}
#else
void JobManager::getStatisticsData(std::vector<int> &data,
    ExecutionPathSetPin &paths, bool onlyChanged) {
  boost::unique_lock<boost::mutex> lock(jobsMutex);

  std::vector<WorkerTree::Node*> dummy;
  dummy.push_back(tree->getRoot());

  paths = tree->buildPathSet(dummy.begin(), dummy.end());

  data.clear();

  data.push_back(jobCount);
}
#endif

/* Coverage Management ********************************************************/

void JobManager::getUpdatedLocalCoverage(cov_update_t &data) {
  theStatisticManager->collectChanges(stats::locallyCoveredInstructions, data);
  theStatisticManager->resetChanges(stats::locallyCoveredInstructions);
}

void JobManager::setUpdatedGlobalCoverage(const cov_update_t &data) {
  for (cov_update_t::const_iterator it = data.begin(); it != data.end(); it++) {
    assert(it->second != 0 && "code uncovered after update");

    uint32_t index = it->first;

    if (!theStatisticManager->getIndexedValue(
        stats::globallyCoveredInstructions, index)) {
      theStatisticManager->incrementIndexedValue(
          stats::globallyCoveredInstructions, index, 1);
      theStatisticManager->incrementIndexedValue(
          stats::globallyUncoveredInstructions, index, (uint64_t) -1);

      assert(!theStatisticManager->getIndexedValue(stats::globallyUncoveredInstructions, index));
    }
  }
}

uint32_t JobManager::getCoverageIDCount() const {
  return symbEngine->getModule()->infos->getMaxID();
}

/* Debugging Support **********************************************************/

void JobManager::setPathBreakpoint(ExecutionPathPin path) {
  assert(0 && "Not yet implemented"); // TODO: Implement this as soon as the
  // tree data structures are refactored
}

void JobManager::setCodeBreakpoint(int assemblyLine) {
  CLOUD9_INFO("Code breakpoint at assembly line " << assemblyLine);
  codeBreaks.insert(assemblyLine);
}

void JobManager::dumpStateTrace(WorkerTree::Node *node) {
  if (!collectTraces) {
    // We have nothing collected, why bother?
    return;
  }

  // Get a file to dump the path into
  char fileName[256];
  snprintf(fileName, 256, "pathDump%05d.txt", traceCounter);
  traceCounter++;

  CLOUD9_INFO("Dumping state trace in file '" << fileName << "'");

  std::ostream *os = kleeHandler->openOutputFile(fileName);
  assert(os != NULL);

  (*os) << (*node) << std::endl;

  SymbolicState *state = (**node).symState;
  assert(state != NULL);

  serializeExecutionTrace(*os, state);

  delete os;
}

void JobManager::dumpInstructionTrace(WorkerTree::Node *node) {
  if (!collectTraces)
    return;

  char fileName[256];
  snprintf(fileName, 256, "instrDump%05d.txt", traceCounter);
  traceCounter++;

  CLOUD9_INFO("Dumping instruction trace in file " << fileName);

  std::ostream *os = kleeHandler->openOutputFile(fileName);
  assert(os != NULL);

  serializeInstructionTrace(*os, node);

  delete os;
}

}
}
