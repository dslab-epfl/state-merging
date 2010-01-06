/*
 * JobExecutor.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/worker/JobExecutor.h"
#include "cloud9/worker/SymbolicEngine.h"
#include "cloud9/Common.h"

#include "klee/Interpreter.h"

#include "llvm/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Instructions.h"

#include "KleeHandler.h"

#include "../../Core/Common.h"

#include <map>
#include <set>

using namespace llvm;

namespace {
cl::opt<unsigned> MakeConcreteSymbolic("make-concrete-symbolic", cl::desc(
		"Rate at which to make concrete reads symbolic (0=off)"), cl::init(0));

cl::opt<bool> OptimizeModule("optimize", cl::desc("Optimize before execution"));

cl::opt<bool> CheckDivZero("check-div-zero", cl::desc(
		"Inject checks for division-by-zero"), cl::init(true));

cl::opt<bool> WarnAllExternals("warn-all-externals", cl::desc(
		"Give initial warning for all externals."));

}

namespace cloud9 {

namespace worker {

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

void JobExecutor::externalsAndGlobalsCheck(const llvm::Module *m) {
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

JobExecutor::JobExecutor(llvm::Module *module, WorkerTree *t,
		int argc, char **argv, SizingHandler *s, ExplorationHandler *e)
		: tree(t),
		  sizingHandler(s),
		  expHandler(e) {

	assert(s != NULL);
	assert(e != NULL);

	klee::Interpreter::InterpreterOptions iOpts;
	iOpts.MakeConcreteSymbolic = MakeConcreteSymbolic;


	llvm::sys::Path libraryPath(KLEE_LIBRARY_PATH);

	Interpreter::ModuleOptions mOpts(libraryPath.c_str(),
	/*Optimize=*/OptimizeModule,
	/*CheckDivZero=*/CheckDivZero);

	kleeHandler = new KleeHandler(argc, argv);
	interpreter = klee::Interpreter::create(iOpts, kleeHandler);
	kleeHandler->setInterpreter(interpreter);

	symbEngine = dynamic_cast<SymbolicEngine*>(interpreter);

	finalModule = interpreter->setModule(module, mOpts);
	externalsAndGlobalsCheck(finalModule);

	symbEngine->registerStateEventHandler(this);

	assert(tree->getDegree() == 2);
}

void JobExecutor::initRootState(llvm::Function *f, int argc,
			char **argv, char **envp) {
	klee::ExecutionState *state = symbEngine->initRootState(f, argc, argv, envp);
	WorkerTree::Node *node = tree->getRoot();

	(**node).symState = state;
	state->setCustomData(node);
}

JobExecutor::~JobExecutor() {
	if (symbEngine != NULL) {
		symbEngine->deregisterStateEventHandler(this);

		symbEngine->destroyStates();
	}
}

WorkerTree::Node *JobExecutor::getNextNode() {
	WorkerTree::Node *nextNode = NULL;

	expHandler->onNextStateQuery(currentJob, nextNode);

	return nextNode;
}

void JobExecutor::exploreNode(WorkerTree::Node *node) {
	// Execute instructions until the state is destroyed or branching occurs.
	// When a branching occurs, the node will become empty (as the state and its
	// fork move in its children)
	while ((**node).symState != NULL) {
		symbEngine->stepInState((**node).symState);
	}
}

void JobExecutor::updateTreeOnBranch(klee::ExecutionState *state,
		klee::ExecutionState *parent, int index) {

	WorkerTree::Node *pNode = (WorkerTree::Node*)parent->getCustomData();

	if ((**pNode).job != currentJob) {
		// It's not for us
		return;
	}

	WorkerTree::Node *newNode, *oldNode;

	// Obtain the new node pointers
	newNode = tree->getNode(pNode, index);
	oldNode = tree->getNode(pNode, 1 - index);

	// Update state -> node references
	if (state)
		state->setCustomData(newNode);
	parent->setCustomData(oldNode);

	// Update node -> state references
	(**pNode).symState = NULL;

	(**newNode).symState = state;
	(**newNode).job = currentJob;

	(**oldNode).symState = parent;
	(**newNode).job = currentJob;

	// Update frontier
	currentJob->removeFromFrontier(pNode);

	if (state) {
		currentJob->addToFrontier(newNode);
	}
	currentJob->addToFrontier(oldNode);
}

void JobExecutor::updateTreeOnDestroy(klee::ExecutionState *state) {

	WorkerTree::Node *pNode = (WorkerTree::Node*)state->getCustomData();

	if ((**pNode).job != currentJob) {
		return;
	}

	state->setCustomData(NULL);
	(**pNode).symState = NULL;

	currentJob->removeFromFrontier(pNode);

	// Delete the supporting branch of the state
	while (pNode->getParent()) { // Don't delete root
		if (pNode->getCount() > 0) // Stop when joining another branch
			break;

		WorkerTree::Node *temp = pNode;
		pNode = pNode->getParent();
		tree->removeNode(temp);
	}
}

void JobExecutor::onStateBranched(klee::ExecutionState *state,
		klee::ExecutionState *parent, int index) {

	assert(parent && parent->getCustomData());

	WorkerTree::Node *pNode = (WorkerTree::Node*)parent->getCustomData();

	updateTreeOnBranch(state, parent, index);

	fireNodeExplored(pNode);

}

void JobExecutor::onStateDestroy(klee::ExecutionState *state,
		bool &allow) {

	assert(state && state->getCustomData());

	WorkerTree::Node *pNode = (WorkerTree::Node*)state->getCustomData();

	updateTreeOnDestroy(state);

	fireNodeDeleted(pNode);
}

void JobExecutor::executeJob(ExplorationJob *job) {
	job->started = true;
	currentJob = job;
	fireJobStarted(job);

	while (!job->frontier.empty()) {
		// Select a new state to explore next
		WorkerTree::Node *node = getNextNode();
		assert(node);

		exploreNode(node);
	}

	job->finished = true;
	fireJobTerminated(job);
}

}
}
