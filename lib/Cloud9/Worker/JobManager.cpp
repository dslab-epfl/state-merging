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
#include "cloud9/Logger.h"
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
#include "klee/util/ExprPPrinter.h"
#include "KleeHandler.h"

#include "../../Core/Common.h"

#include <stack>
#include <boost/io/ios_state.hpp>
#include <boost/crc.hpp>
#include <map>
#include <set>
#include <fstream>
#include <iostream>
#include <iomanip>

#define CLOUD9_STATS_FILE_NAME		"c9-stats.txt"
#define CLOUD9_EVENTS_FILE_NAME		"c9-events.txt"

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

cl::list<unsigned int> CodeBreakpoints("c9-code-bp",
		cl::desc("Breakpoints in the LLVM assembly file"));

cl::opt<bool> BreakOnReplayBroken("c9-bp-on-replaybr",
		cl::desc("Break on last valid position if a broken replay occurrs."));

cl::opt<bool> DumpStateTraces("c9-dump-traces",
		cl::desc("Dump state traces when a breakpoint or any other relevant event happens during execution"));

}

namespace cloud9 {

namespace worker {

/*******************************************************************************
 * HELPER FUNCTIONS FOR THE JOB MANAGER
 ******************************************************************************/

static void serializeExecutionTrace(std::ostream &os, const WorkerTree::Node *node) { // XXX very slow - read the .ll file and use it instead
	assert(node->layerExists(WORKER_LAYER_STATES));
	std::vector<int> path;
	const WorkerTree::Node *crtNode = node;

	while (crtNode->getParent() != NULL) {
		path.push_back(crtNode->getIndex());
		crtNode = crtNode->getParent();
	}

	std::reverse(path.begin(), path.end());


	const llvm::BasicBlock *crtBasicBlock = NULL;
	const llvm::Function *crtFunction = NULL;

	llvm::raw_os_ostream raw_os(os);


	for (unsigned int i = 0; i <= path.size(); i++) {
		const ExecutionTrace &trace = (**crtNode).getTrace();
		// Output each instruction in the node
		for (ExecutionTrace::const_iterator it = trace.getEntries().begin();
				it != trace.getEntries().end(); it++) {
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
					os << "   Function '" << ((crtFunction != NULL) ? crtFunction->getNameStr() : "") << "':" << std::endl;
				}

				if (newBB) {
					os << "----------- " << ((crtBasicBlock != NULL) ? crtBasicBlock->getNameStr() : "") << " ----" << std::endl;
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

static void serializeExecutionTrace(std::ostream &os, SymbolicState *state) {
	const WorkerTree::Node *node = state->getNode().get();
	serializeExecutionTrace(os, node);
}

/*******************************************************************************
 * KLEE INITIALIZATION CODE
 ******************************************************************************/

// This is a terrible hack until we get some real modelling of the
// system. All we do is check the undefined symbols and m and warn about
// any "unrecognized" externals and about any obviously unsafe ones.

// Symbols we explicitly support
static const char *modelledExternals[] = {
		"_ZTVN10__cxxabiv117__class_type_infoE",
		"_ZTVN10__cxxabiv120__si_class_type_infoE",
		"_ZTVN10__cxxabiv121__vmi_class_type_infoE",

		// special functions
		"_assert", "__assert_fail", "__assert_rtn", "calloc", "_exit", "exit",
		"free", "abort", "klee_abort", "klee_assume",
		"klee_check_memory_access", "klee_define_fixed_object",
		"klee_get_errno", "klee_get_value", "klee_get_obj_size",
		"klee_is_symbolic", "klee_make_symbolic", "klee_mark_global",
		"klee_merge", "klee_prefer_cex", "klee_print_expr", "klee_print_range",
		"klee_report_error", "klee_set_forking", "klee_silent_exit",
		"klee_warning", "klee_warning_once", "klee_alias_function",
		"llvm.dbg.stoppoint", "llvm.va_start", "llvm.va_end", "malloc",
		"realloc", "_ZdaPv", "_ZdlPv", "_Znaj", "_Znwj", "_Znam", "_Znwm", };
// Symbols we aren't going to warn about
static const char *dontCareExternals[] = {
#if 0
		// stdio
		"fprintf",
		"fflush",
		"fopen",
		"fclose",
		"fputs_unlocked",
		"putchar_unlocked",
		"vfprintf",
		"fwrite",
		"puts",
		"printf",
		"stdin",
		"stdout",
		"stderr",
		"_stdio_term",
		"__errno_location",
		"fstat",
#endif

		// static information, pretty ok to return
		"getegid", "geteuid", "getgid", "getuid", "getpid", "gethostname",
		"getpgrp", "getppid", "getpagesize", "getpriority", "getgroups",
		"getdtablesize", "getrlimit", "getrlimit64", "getcwd", "getwd",
		"gettimeofday", "uname",

		// fp stuff we just don't worry about yet
		"frexp", "ldexp", "__isnan", "__signbit", };

// Extra symbols we aren't going to warn about with uclibc
static const char *dontCareUclibc[] = { "__dso_handle",

// Don't warn about these since we explicitly commented them out of
		// uclibc.
		"printf", "vprintf" };
// Symbols we consider unsafe
static const char *unsafeExternals[] = { "fork", // oh lord
		"exec", // heaven help us
		"error", // calls _exit
		"raise", // yeah
		"kill", // mmmhmmm
		};

#define NELEMS(array) (sizeof(array)/sizeof(array[0]))

static void externalsAndGlobalsCheck(const llvm::Module *m) {
	std::map<std::string, bool> externals;
	std::set<std::string> modelled(modelledExternals, modelledExternals
			+NELEMS(modelledExternals));
	std::set<std::string> dontCare(dontCareExternals, dontCareExternals
			+NELEMS(dontCareExternals));
	std::set<std::string> unsafe(unsafeExternals, unsafeExternals
			+NELEMS(unsafeExternals));

	switch (Libc) {
	case UcLibc:
		dontCare.insert(dontCareUclibc, dontCareUclibc + NELEMS(dontCareUclibc));
		break;
	case NoLibc: /* silence compiler warning */
		break;
	}

	if (WithPOSIXRuntime)
		dontCare.insert("syscall");

	for (Module::const_iterator fnIt = m->begin(), fn_ie = m->end(); fnIt
			!= fn_ie; ++fnIt) {
		if (fnIt->isDeclaration() && !fnIt->use_empty())
			externals.insert(std::make_pair(fnIt->getName(), false));
		for (Function::const_iterator bbIt = fnIt->begin(), bb_ie = fnIt->end(); bbIt
				!= bb_ie; ++bbIt) {
			for (BasicBlock::const_iterator it = bbIt->begin(), ie =
					bbIt->end(); it != it; ++it) {
				if (const CallInst *ci = dyn_cast<CallInst>(it)) {
					if (isa<InlineAsm> (ci->getCalledValue())) {
						klee_warning_once(&*fnIt,
								"function \"%s\" has inline asm",
								fnIt->getName().data());
					}
				}
			}
		}
	}
	for (Module::const_global_iterator it = m->global_begin(), ie =
			m->global_end(); it != ie; ++it)
		if (it->isDeclaration() && !it->use_empty())
			externals.insert(std::make_pair(it->getName(), true));
	// and remove aliases (they define the symbol after global
	// initialization)
	for (Module::const_alias_iterator it = m->alias_begin(), ie =
			m->alias_end(); it != ie; ++it) {
		std::map<std::string, bool>::iterator it2 = externals.find(
				it->getName());
		if (it2 != externals.end())
			externals.erase(it2);
	}

	std::map<std::string, bool> foundUnsafe;
	for (std::map<std::string, bool>::iterator it = externals.begin(), ie =
			externals.end(); it != ie; ++it) {
		const std::string &ext = it->first;
		if (!modelled.count(ext) && (WarnAllExternals || !dontCare.count(ext))) {
			if (unsafe.count(ext)) {
				foundUnsafe.insert(*it);
			} else {
				klee_warning("undefined reference to %s: %s",
						it->second ? "variable" : "function", ext.c_str());
			}
		}
	}

	for (std::map<std::string, bool>::iterator it = foundUnsafe.begin(), ie =
			foundUnsafe.end(); it != ie; ++it) {
		const std::string &ext = it->first;
		klee_warning("undefined reference to %s: %s (UNSAFE)!",
				it->second ? "variable" : "function", ext.c_str());
	}
}

/*******************************************************************************
 * JOB MANAGER METHODS
 ******************************************************************************/


/* Initialization Methods *****************************************************/

JobManager::JobManager(llvm::Module *module, std::string mainFnName, int argc,
		char **argv, char **envp) : terminationRequest(false),
		currentJob(NULL), replaying(false) {

	tree = new WorkerTree();

	llvm::Function *mainFn = module->getFunction(mainFnName);

	initialize(module, mainFn, argc, argv, envp);
}

void JobManager::initialize(llvm::Module *module, llvm::Function *_mainFn, int argc, char **argv,
		char **envp) {
	mainFn = _mainFn;
	assert(mainFn);

	klee::Interpreter::InterpreterOptions iOpts;
	iOpts.MakeConcreteSymbolic = MakeConcreteSymbolic;

	llvm::sys::Path libraryPath(getKleeLibraryPath());

	Interpreter::ModuleOptions mOpts(libraryPath.c_str(),
	/*Optimize=*/OptimizeModule,
	/*CheckDivZero=*/CheckDivZero);

	kleeHandler = new KleeHandler(argc, argv);
	interpreter = klee::Interpreter::create(iOpts, kleeHandler);
	kleeHandler->setInterpreter(interpreter);

	symbEngine = dynamic_cast<SymbolicEngine*>(interpreter);
	interpreter->setModule(module, mOpts);

	kleeModule = symbEngine->getModule();

	externalsAndGlobalsCheck(kleeModule->module);

	symbEngine->registerStateEventHandler(this);

	theStatisticManager->trackChanges(stats::locallyCoveredInstructions);

	initStrategy();
	initStatistics();
	initInstrumentation();
	initBreakpoints();

	initRootState(mainFn, argc, argv, envp);
}

void JobManager::initRootState(llvm::Function *f, int argc,
			char **argv, char **envp) {
	klee::ExecutionState *kState = symbEngine->createRootState(f);
	SymbolicState *state = new SymbolicState(kState);

	state->rebindToNode(tree->getRoot());

	symbEngine->initRootState(kState, argc, argv, envp);
}

void JobManager::initStrategy() {
	switch (JobSelection) {
	case RandomSel:
	  selStrategy = new RandomStrategy();
	  CLOUD9_INFO("Using random job selection strategy");
	  break;
	case RandomPathSel:
	  selStrategy = new RandomPathStrategy(tree);
	  CLOUD9_INFO("Using random path job selection strategy");
	  break;
	case CoverageOptimizedSel:
	  selStrategy =
		new WeightedRandomStrategy(WeightedRandomStrategy::CoveringNew,
							  tree, symbEngine);
	  CLOUD9_INFO("Using weighted random job selection strategy");
	  break;
	default:
	  assert(0 && "undefined job selection strategy");
	}
}

void JobManager::initStatistics() {
	// Configure the root as a statistics node
	WorkerTree::NodePin rootPin = tree->getRoot()->pin(WORKER_LAYER_STATISTICS);

	stats.insert(rootPin);
	statChanged = true;
	refineStats = false;
}

void JobManager::initInstrumentation() {
	std::string statsFileName = kleeHandler->getOutputFilename(CLOUD9_STATS_FILE_NAME);
	std::string eventsFileName = kleeHandler->getOutputFilename(CLOUD9_EVENTS_FILE_NAME);

	std::ostream *instrStatsStream = new std::ofstream(statsFileName.c_str());
	std::ostream *instrEventsStream = new std::ofstream(eventsFileName.c_str());
	cloud9::instrum::InstrumentationWriter *writer =
			new cloud9::instrum::LocalFileWriter(*instrStatsStream,
					*instrEventsStream);

	cloud9::instrum::theInstrManager.registerWriter(writer);
	cloud9::instrum::theInstrManager.start();
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

/* Job Manipulation Methods ***************************************************/

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

void JobManager::processLoop(bool allowGrowth, bool blocking, unsigned int timeOut) {
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

		bool foreign = job->imported;

		executeJob(lock, job, allowGrowth);

		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalProcJobs);

		if (foreign)
			cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentImportedPathCount);

		if (refineStats) {
			refineStatistics();
			refineStats = false;
		}
	}

	if (terminationRequest)
		CLOUD9_INFO("Termination was requested.");
}

ExecutionJob* JobManager::selectNextJob(boost::unique_lock<boost::mutex> &lock,  unsigned int timeOut) {
	ExecutionJob *job = selStrategy->onNextJobSelection();

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

		job = selStrategy->onNextJobSelection();

		if (job != NULL)
			cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "working");
	}

	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize);

	return job;
}

ExecutionJob* JobManager::selectNextJob() {
	ExecutionJob *job = selStrategy->onNextJobSelection();

	if (job != NULL) {
		cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize);
	}

	return job;
}

void JobManager::submitJob(ExecutionJob* job, bool activateStates) {
	WorkerTree::Node *node = job->getNode().get();
	assert((**node).symState || job->isImported());

	selStrategy->onJobAdded(job);

	if (activateStates) {
		// Check for the state on the supporting branch
		while (node) {
			SymbolicState *state = (**node).getSymbolicState();

			if (state) {
				activateState(state);
				break;
			}

			node = node->getParent();
		}
	}

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentQueueSize);
}

void JobManager::finalizeJob(ExecutionJob *job, bool deactivateStates, bool notifySearcher) {
	WorkerTree::Node *node = job->getNode().get();

	job->removing = true;

	if (deactivateStates) {
		while (node) {
			if (node->getCount(WORKER_LAYER_JOBS) > 1)
				break; // We reached a junction

			SymbolicState *state = (**node).getSymbolicState();

			if (state) {
				deactivateState(state);
				break;
			}

			node = node->getParent();
		}
	}

	if (notifySearcher)
		selStrategy->onRemovingJob(job);
}

void JobManager::selectJobs(WorkerTree::Node *root,
		std::vector<ExecutionJob*> &jobSet, int maxCount) {
	/// XXX: Prevent node creation
	std::stack<WorkerTree::Node*> nodes;

	nodes.push(root);

	while (!nodes.empty() && maxCount > 0) {
		WorkerTree::Node *node = nodes.top();
		nodes.pop();

		if (node->getCount(WORKER_LAYER_JOBS) == 0) {
			ExecutionJob *job = (**node).job;

			if (job) {
				if (job == currentJob) {
					CLOUD9_DEBUG("FOUND A STARTED JOB: " << job->getNode());
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
	return tree->countLeaves(WORKER_LAYER_JOBS, root);
}

void JobManager::importJobs(ExecutionPathSetPin paths) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::vector<WorkerTree::Node*> nodes;
	std::vector<ExecutionJob*> jobs;

	tree->getNodes(WORKER_LAYER_JOBS, paths, nodes);

	CLOUD9_DEBUG("Importing " << paths->count() << " jobs");

	for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin();
			it != nodes.end(); it++) {
		WorkerTree::Node *crtNode = *it;
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
			ExecutionJob *job = new ExecutionJob(*it, true);
			jobs.push_back(job);
		}
	}

	submitJobs(jobs.begin(), jobs.end(), true);

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalImportedJobs,
			jobs.size());
	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentImportedPathCount,
			jobs.size());
}

ExecutionPathSetPin JobManager::exportJobs(ExecutionPathSetPin seeds,
		std::vector<int> counts) {
	boost::unique_lock<boost::mutex> lock(jobsMutex);

	std::vector<WorkerTree::Node*> roots;
	std::vector<ExecutionJob*> jobs;
	std::vector<WorkerTree::Node*> jobRoots;

	tree->getNodes(WORKER_LAYER_JOBS, seeds, roots);

	assert(roots.size() == counts.size());

	for (unsigned int i = 0; i < seeds->count(); i++) {
		selectJobs(roots[i], jobs, counts[i]);
	}

	for (std::vector<ExecutionJob*>::iterator it = jobs.begin();
			it != jobs.end(); it++) {
		ExecutionJob *job = *it;
		jobRoots.push_back(job->getNode().get());
	}

	// Do this before de-registering the jobs, in order to keep the nodes pinned
	ExecutionPathSetPin paths = tree->buildPathSet(jobRoots.begin(), jobRoots.end());

	// De-register the jobs with the worker
	for (std::vector<ExecutionJob*>::iterator it = jobs.begin();
			it != jobs.end(); it++) {
		// Cancel each job
		ExecutionJob *job = *it;
		assert(currentJob != job);

		job->exported = true;

		finalizeJob(job, true, false);
	}

	selStrategy->onRemovingJobs();

	for (std::vector<ExecutionJob*>::iterator it = jobs.begin();
				it != jobs.end(); it++) {
		delete (*it);
	}


	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalExportedJobs,
			jobs.size());
	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentQueueSize,
			jobs.size());

	return paths;
}

/* State Manipulation Methods *************************************************/

void JobManager::activateState(SymbolicState *state) {
	if (!state->_active) {
		state->_active = true;
		selStrategy->onStateActivated(state);
	}
}

void JobManager::deactivateState(SymbolicState *state) {
	if (state->_active) {
		state->_active = false;
		selStrategy->onStateDeactivated(state);
	}
}

void JobManager::updateState(SymbolicState *state) {
	if (state->_active) {
		selStrategy->onStateUpdated(state);
	}
}

/* Job Execution Methods ******************************************************/

void JobManager::executeJob(boost::unique_lock<boost::mutex> &lock, ExecutionJob *job, bool spawnNew) {
	currentJob = job;
	lock.unlock();

	WorkerTree::NodePin nodePin = job->getNode(); // Keep the node around until we finish with it

	if ((**nodePin).symState == NULL) {
		if (!job->isImported()) {
			CLOUD9_INFO("Replaying path for non-foreign job. Most probably this job will be lost.");
		}

		cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "startReplay");

		replayPath(nodePin.get());

		cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::JobExecutionState, "endReplay");
	} else {
		if (job->isImported()) {
			CLOUD9_INFO("Foreign job with no replay needed. Probably state was obtained through other neighbor replays.");
		}
	}

	job->imported = false;

	if ((**nodePin).symState == NULL) {
		CLOUD9_INFO("Job canceled before start");
		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalDroppedJobs);
	} else {
		stepInNode(nodePin.get(), false);
	}

	lock.lock();
	currentJob = NULL;

	if ((**nodePin).symState == NULL) {
		// Job finished here, need to remove it
		finalizeJob(job, false, true);

		delete job;

		// Spawn new jobs if there are states left
		if (nodePin->layerExists(WORKER_LAYER_STATES)) {
			std::vector<WorkerTree::Node*> nodes;
			tree->getLeaves(WORKER_LAYER_STATES, nodePin.get(), nodes);

			//CLOUD9_DEBUG("New jobs: " << nodes.size());

			for (std::vector<WorkerTree::Node*>::iterator it = nodes.begin();
					it != nodes.end(); it++) {
				WorkerTree::Node *node = tree->getNode(WORKER_LAYER_JOBS, *it);
				assert((**node).symState != NULL);
				ExecutionJob *newJob = new ExecutionJob(node, false);

				submitJob(newJob, false);
			}
		}
	} else {
		// Just mark the state as updated
		updateState((**nodePin).symState);
	}
}

void JobManager::stepInNode(WorkerTree::Node *node, bool exhaust) {
	assert((**node).symState != NULL);

	// Keep the node alive until we finish with it
	WorkerTree::NodePin nodePin = node->pin(WORKER_LAYER_STATES);

	while ((**node).symState != NULL) {

		SymbolicState *state = (**node).symState;

		//CLOUD9_DEBUG("Stepping in instruction " << state->pc->info->assemblyLine);
		if (!codeBreaks.empty()) {
			if (codeBreaks.find(state->getKleeState()->pc->info->assemblyLine) !=
					codeBreaks.end()) {
				// We hit a breakpoint
				fireBreakpointHit(node);
			}
		}

		// Execute the instruction
		symbEngine->stepInState(state->getKleeState());
		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalProcInstructions);

		if (!replaying) {
			cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalNewInstructions);
		}

		if (!exhaust)
			break;
	}
}

void JobManager::replayPath(WorkerTree::Node *pathEnd) {
	std::vector<int> path;

	WorkerTree::Node *crtNode = pathEnd;

	CLOUD9_DEBUG("Replaying path: " << *crtNode);

	while (crtNode != NULL && (**crtNode).symState == NULL) {
		path.push_back(crtNode->getIndex());

		crtNode = crtNode->getParent();
	}

	if (crtNode == NULL) {
		CLOUD9_ERROR("Cannot find the seed execution state.");
		return;
	}

	std::reverse(path.begin(), path.end());

	CLOUD9_DEBUG("Started path replay at position: " << *crtNode);
	WorkerTree::Node *lastValidState = NULL;

	replaying = true;

	// Perform the replay work
	for (unsigned int i = 0; i < path.size(); i++) {
		if ((**crtNode).symState != NULL) {
			lastValidState = crtNode;
			stepInNode(crtNode, true);
		} else {
			CLOUD9_DEBUG("Potential fast-forward at position " << i <<
					" out of " << path.size() << " in the path.");
		}

		crtNode = crtNode->getChild(WORKER_LAYER_JOBS, path[i]);
		assert(crtNode != NULL);
	}

	replaying = false;

	if ((**crtNode).symState == NULL) {
		CLOUD9_ERROR("Replay broken, NULL state at the end of the path. Maybe the state went past the job root?");
		if (BreakOnReplayBroken) {
			assert(lastValidState != NULL);
			fireBreakpointHit(lastValidState);
		}
	}
}


/* Symbolic Engine Callbacks **************************************************/

void JobManager::onStateBranched(klee::ExecutionState *kState,
		klee::ExecutionState *parent, int index) {

	assert(parent);

	//if (kState)
	//	CLOUD9_DEBUG("State branched: " << parent->getCloud9State()->getNode());

	updateTreeOnBranch(kState, parent, index);

	if (kState) {
		SymbolicState *state = kState->getCloud9State();

		if (state->getNode()->layerExists(WORKER_LAYER_JOBS) || !replaying) {
			activateState(state);
		}
	}

	SymbolicState *pState = parent->getCloud9State();

	if (pState->getNode()->layerExists(WORKER_LAYER_JOBS) || !replaying) {
		updateState(pState);
	} else {
		deactivateState(pState);
	}


}

void JobManager::onStateDestroy(klee::ExecutionState *kState) {

	assert(kState);

	CLOUD9_DEBUG("State destroyed: " << kState->getCloud9State()->getNode());

	SymbolicState *state = kState->getCloud9State();

	deactivateState(state);

	//CLOUD9_DEBUG("State destroyed! " << *state);

	updateTreeOnDestroy(kState);
}

void JobManager::onOutOfResources(klee::ExecutionState *destroyedState) {
	// TODO: Implement a job migration mechanism
	CLOUD9_INFO("Executor ran out of resources. Dropping state.");
}

void JobManager::onControlFlowEvent(klee::ExecutionState *kState,
				ControlFlowEvent event) {
	WorkerTree::Node *node = kState->getCloud9State()->getNode().get();

	// Add the instruction to the node trace
	if (DumpStateTraces) {
		switch (event) {
		case STEP:
			(**node).trace.appendEntry(new InstructionTraceEntry(kState->pc));
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

	if (DumpStateTraces) {
		(**node).trace.appendEntry(new DebugLogEntry(message));
	}
}

void JobManager::fireBreakpointHit(WorkerTree::Node *node) {
	SymbolicState *state = (**node).symState;

	CLOUD9_INFO("Breakpoint hit!");
	CLOUD9_DEBUG("State at position: " << *node);

	if (state) {
		CLOUD9_DEBUG("State stack trace: " << *state);
		klee::ExprPPrinter::printConstraints(std::cerr, state->getKleeState()->constraints);
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

		if (!replaying) {
			cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalNewPaths);
		}

		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalPathsStarted);
		cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::CurrentPathCount);
	}
}

void JobManager::updateTreeOnDestroy(klee::ExecutionState *kState) {
	SymbolicState *state = kState->getCloud9State();
	state->rebindToNode(NULL);

	kState->setCloud9State(NULL);
	delete state;

	cloud9::instrum::theInstrManager.incStatistic(cloud9::instrum::TotalPathsFinished);
	cloud9::instrum::theInstrManager.decStatistic(cloud9::instrum::CurrentPathCount);
}

/* Statistics Management ******************************************************/

void JobManager::refineStatistics() {
	std::set<WorkerTree::NodePin> newStats;

	for (std::set<WorkerTree::NodePin>::iterator it = stats.begin();
			it != stats.end(); it++) {
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
		unsigned int jobCount = tree->countLeaves(WORKER_LAYER_JOBS, crtNodePin.get());
		data.push_back(jobCount);
	}

	CLOUD9_DEBUG("Sent data set: " << getASCIIDataSet(data.begin(), data.end()));
}

/* Coverage Management ********************************************************/

void JobManager::getUpdatedLocalCoverage(cov_update_t &data) {
	theStatisticManager->collectChanges(stats::locallyCoveredInstructions, data);
	theStatisticManager->resetChanges(stats::locallyCoveredInstructions);
}

void JobManager::setUpdatedGlobalCoverage(const cov_update_t &data) {
	for (cov_update_t::const_iterator it = data.begin(); it != data.end(); it++) {
		assert(it->second != 0 && "code uncovered after update");

		uint32_t index = it->first;

		if (!theStatisticManager->getIndexedValue(stats::globallyCoveredInstructions, index)) {
			theStatisticManager->incrementIndexedValue(stats::globallyCoveredInstructions, index, 1);
			theStatisticManager->incrementIndexedValue(stats::globallyUncoveredInstructions, index, (uint64_t) -1);

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
	if (!DumpStateTraces) {
		// Do nothing
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

}
}
