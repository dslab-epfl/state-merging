/*
 * worker.cpp
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cassert>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "llvm/ModuleProvider.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/Constants.h"
#include "llvm/Instruction.h"
#include "llvm/Type.h"
#include "llvm/InstrTypes.h"
#include "llvm/Instructions.h"

#include "llvm/Bitcode/ReaderWriter.h"

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/System/Signals.h"

// All the KLEE includes below

#include "klee/Internal/System/Time.h"
#include "klee/Internal/Support/ModuleUtil.h"

#include "cloud9/Logger.h"
#include "cloud9/ExecutionTree.h"
#include "cloud9/ExecutionPath.h"
#include "cloud9/Protocols.h"
#include "cloud9/worker/TreeNodeInfo.h"
#include "cloud9/worker/JobManager.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/KleeCommon.h"
#include "cloud9/worker/CommManager.h"
#include "cloud9/Utils.h"
#include "cloud9/instrum/InstrumentationManager.h"

using namespace llvm;
using namespace cloud9::worker;

namespace {

cl::opt<bool>
		InitEnv("init-env",
				cl::desc("Create custom environment.  Options that can be passed as arguments to the programs are: --sym-argv <max-len>  --sym-argvs <min-argvs> <max-argvs> <max-len> + file model options"));


cl::opt<std::string> Environ("environ", cl::desc(
		"Parse environ from given file (in \"env\" format)"));

cl::list<std::string> InputArgv(cl::ConsumeAfter, cl::desc(
		"<program arguments>..."));

cl::opt<std::string> ReplayPath("c9-replay-path", cl::desc(
		"Instead of executing jobs, just do a replay of a path. No load balancer involved."));

}

static bool Interrupted = false;
extern cl::opt<double> MaxTime;

JobManager *theJobManager = NULL;

/*
 *
 */
static Module* loadByteCode() {
	ModuleProvider *mp = NULL;
	std::string errorMsg;

	if (MemoryBuffer *buffer = MemoryBuffer::getFileOrSTDIN(InputFile, &errorMsg)) {
		mp = getBitcodeModuleProvider(buffer, getGlobalContext(), &errorMsg);
		if (!mp)
			delete buffer;
	}

	if (!mp) {
		CLOUD9_EXIT("Error loading program " << InputFile.c_str() << ": " <<
				errorMsg.c_str());
	}

	// Load every function in the module
	Module *m = mp->materializeModule();
	// Release the module from ModuleProvider control
	mp->releaseModule();
	delete mp;

	if (!m) {
		CLOUD9_EXIT("Unable to materialize the module");
	}

	return m;
}

static void initEnv(Module *mainModule) {

	/*
	 nArgcP = alloc oldArgc->getType()
	 nArgvV = alloc oldArgv->getType()
	 store oldArgc nArgcP
	 store oldArgv nArgvP
	 klee_init_environment(nArgcP, nArgvP)
	 nArgc = load nArgcP
	 nArgv = load nArgvP
	 oldArgc->replaceAllUsesWith(nArgc)
	 oldArgv->replaceAllUsesWith(nArgv)
	 */

	Function *mainFn = mainModule->getFunction("main");

	if (mainFn->arg_size() < 2) {
		CLOUD9_EXIT("Cannot handle ""--init-env"" when main() has less than two arguments.");
	}

	Instruction* firstInst = mainFn->begin()->begin();

	Value* oldArgc = mainFn->arg_begin();
	Value* oldArgv = ++mainFn->arg_begin();

	AllocaInst* argcPtr = new AllocaInst(oldArgc->getType(), "argcPtr",
			firstInst);
	AllocaInst* argvPtr = new AllocaInst(oldArgv->getType(), "argvPtr",
			firstInst);

	/* Insert void klee_init_env(int* argc, char*** argv) */
	std::vector<const Type*> params;
	params.push_back(Type::getInt32Ty(getGlobalContext()));
	params.push_back(Type::getInt32Ty(getGlobalContext()));
	Function* initEnvFn = cast<Function> (mainModule->getOrInsertFunction(
			"klee_init_env", Type::getVoidTy(getGlobalContext()),
			argcPtr->getType(), argvPtr->getType(), NULL));
	assert(initEnvFn);
	std::vector<Value*> args;
	args.push_back(argcPtr);
	args.push_back(argvPtr);
	Instruction* initEnvCall = CallInst::Create(initEnvFn, args.begin(),
			args.end(), "", firstInst);
	Value *argc = new LoadInst(argcPtr, "newArgc", firstInst);
	Value *argv = new LoadInst(argvPtr, "newArgv", firstInst);

	oldArgc->replaceAllUsesWith(argc);
	oldArgv->replaceAllUsesWith(argv);

	new StoreInst(oldArgc, argcPtr, initEnvCall);
	new StoreInst(oldArgv, argvPtr, initEnvCall);
}

#ifndef KLEE_UCLIBC
static llvm::Module *linkWithUclibc(llvm::Module *mainModule) {
	CLOUD9_EXIT("Invalid libc, no uclibc support!");
}
#else
static llvm::Module *linkWithUclibc(llvm::Module *mainModule) {
	Function *f;
	// force import of __uClibc_main
	mainModule->getOrInsertFunction("__uClibc_main", FunctionType::get(
			Type::getVoidTy(getGlobalContext()), std::vector<const Type*>(),
			true));

	// force various imports
	if (WithPOSIXRuntime) {
		const llvm::Type *i8Ty = Type::getInt8Ty(getGlobalContext());
		mainModule->getOrInsertFunction("realpath",
				PointerType::getUnqual(i8Ty), PointerType::getUnqual(i8Ty),
				PointerType::getUnqual(i8Ty), NULL);
		mainModule->getOrInsertFunction("getutent",
				PointerType::getUnqual(i8Ty), NULL);
		mainModule->getOrInsertFunction("__fgetc_unlocked", Type::getInt32Ty(
				getGlobalContext()), PointerType::getUnqual(i8Ty), NULL);
		mainModule->getOrInsertFunction("__fputc_unlocked", Type::getInt32Ty(
				getGlobalContext()), Type::getInt32Ty(getGlobalContext()),
				PointerType::getUnqual(i8Ty), NULL);
	}

	f = mainModule->getFunction("__ctype_get_mb_cur_max");
	if (f)
		f->setName("_stdlib_mb_cur_max");

	// Strip of asm prefixes for 64 bit versions because they are not
	// present in uclibc and we want to make sure stuff will get
	// linked. In the off chance that both prefixed and unprefixed
	// versions are present in the module, make sure we don't create a
	// naming conflict.
	for (Module::iterator fi = mainModule->begin(), fe = mainModule->end(); fi
			!= fe;) {
		Function *f = fi;
		++fi;
		const std::string &name = f->getName();

		if(name.compare("__strdup") == 0 ) {
		  if (Function *StrDup = mainModule->getFunction("strdup")) {
		    f->replaceAllUsesWith(StrDup);
		    f->eraseFromParent();
		  } else {
		    f->setName("strdup");
		  }
		  continue;
		}

		if(name.compare("vprintf") == 0) {
		  f->deleteBody();
		  continue;
		}


		if (name[0] == '\01') {
			unsigned size = name.size();
			if (name[size - 2] == '6' && name[size - 1] == '4') {
				std::string unprefixed = name.substr(1);

				// See if the unprefixed version exists.
				if (Function *f2 = mainModule->getFunction(unprefixed)) {
					f->replaceAllUsesWith(f2);
					f->eraseFromParent();
				} else {
					f->setName(unprefixed);
				}
			}
		}
	}

	mainModule = klee::linkWithLibrary(mainModule, getUclibcPath().append("/lib/libc.a"));
	assert(mainModule && "unable to link with uclibc");

	// more sighs, this is horrible but just a temp hack
	//    f = mainModule->getFunction("__fputc_unlocked");
	//    if (f) f->setName("fputc_unlocked");
	//    f = mainModule->getFunction("__fgetc_unlocked");
	//    if (f) f->setName("fgetc_unlocked");

	Function *f2;
	f = mainModule->getFunction("open");
	f2 = mainModule->getFunction("__libc_open");
	if (f2) {
		if (f) {
			f2->replaceAllUsesWith(f);
			f2->eraseFromParent();
		} else {
			f2->setName("open");
			assert(f2->getName() == "open");
		}
	}

	f = mainModule->getFunction("fcntl");
	f2 = mainModule->getFunction("__libc_fcntl");
	if (f2) {
		if (f) {
			f2->replaceAllUsesWith(f);
			f2->eraseFromParent();
		} else {
			f2->setName("fcntl");
			assert(f2->getName() == "fcntl");
		}
	}

	// XXX we need to rearchitect so this can also be used with
	// programs externally linked with uclibc.

	// We now need to swap things so that __uClibc_main is the entry
	// point, in such a way that the arguments are passed to
	// __uClibc_main correctly. We do this by renaming the user main
	// and generating a stub function to call __uClibc_main. There is
	// also an implicit cooperation in that runFunctionAsMain sets up
	// the environment arguments to what uclibc expects (following
	// argv), since it does not explicitly take an envp argument.
	Function *userMainFn = mainModule->getFunction("main");
	assert(userMainFn && "unable to get user main");
	Function *uclibcMainFn = mainModule->getFunction("__uClibc_main");
	assert(uclibcMainFn && "unable to get uclibc main");
	userMainFn->setName("__user_main");

	const FunctionType *ft = uclibcMainFn->getFunctionType();
	assert(ft->getNumParams() == 7);

	std::vector<const Type*> fArgs;
	fArgs.push_back(ft->getParamType(1)); // argc
	fArgs.push_back(ft->getParamType(2)); // argv
	Function *stub = Function::Create(FunctionType::get(Type::getInt32Ty(
			getGlobalContext()), fArgs, false),
			GlobalVariable::ExternalLinkage, "main", mainModule);
	BasicBlock *bb = BasicBlock::Create(getGlobalContext(), "entry", stub);

	std::vector<llvm::Value*> args;
	args.push_back(llvm::ConstantExpr::getBitCast(userMainFn, ft->getParamType(
			0)));
	args.push_back(stub->arg_begin()); // argc
	args.push_back(++stub->arg_begin()); // argv
	args.push_back(Constant::getNullValue(ft->getParamType(3))); // app_init
	args.push_back(Constant::getNullValue(ft->getParamType(4))); // app_fini
	args.push_back(Constant::getNullValue(ft->getParamType(5))); // rtld_fini
	args.push_back(Constant::getNullValue(ft->getParamType(6))); // stack_end
	CallInst::Create(uclibcMainFn, args.begin(), args.end(), "", bb);

	new UnreachableInst(getGlobalContext(), bb);

	return mainModule;
}
#endif

static Module* prepareModule(Module *module) {
	if (WithPOSIXRuntime)
		InitEnv = true;

	if (InitEnv) {
		initEnv(module);
	}

	switch (Libc) {
	case NoLibc:
		break;
	case UcLibc:
		module = linkWithUclibc(module);
		break;
	}

	llvm::sys::Path libraryDir(getKleeLibraryPath());

	if (WithPOSIXRuntime) {
		sys::Path path(libraryDir);
		path.appendComponent("libkleeRuntimePOSIX.bca");
		CLOUD9_INFO("Using model: " << path.c_str());

		module = klee::linkWithLibrary(module, path.c_str());
		assert(module && "unable to link with simple model");
	}

	return module;
}

// This is a temporary hack. If the running process has access to
// externals then it can disable interrupts, which screws up the
// normal "nice" watchdog termination process. We try to request the
// interpreter to halt using this mechanism as a last resort to save
// the state data before going ahead and killing it.
/*
 * This function invokes haltExecution() via gdb.
 */
static void haltViaGDB(int pid) {
	char buffer[256];
	sprintf(buffer, "gdb --batch --eval-command=\"p haltExecution()\" "
		"--eval-command=detach --pid=%d &> /dev/null", pid);

	if (system(buffer) == -1)
		perror("system");
}

/*
 * This function gets executed by the watchdog, via gdb.
 */
// Pulled out so it can be easily called from a debugger.
extern "C" void haltExecution() {
	theJobManager->requestTermination();
}

static std::string strip(std::string &in) {
	unsigned len = in.size();
	unsigned lead = 0, trail = len;
	while (lead < len && isspace(in[lead]))
		++lead;
	while (trail > lead && isspace(in[trail - 1]))
		--trail;
	return in.substr(lead, trail - lead);
}

/*
 *
 */
static void parseArguments(int argc, char **argv) {
	// TODO: Implement some filtering, or reading from a settings file, or
	// from stdin
	cl::ParseCommandLineOptions(argc, argv, "Cloud9 worker");
}

static void readProgramArguments(int &pArgc, char **&pArgv, char **&pEnvp, char **envp) {
	if (Environ != "") {
		std::vector<std::string> items;
		std::ifstream f(Environ.c_str());
		if (!f.good())
			CLOUD9_EXIT("unable to open --environ file: " << Environ);
		else
			CLOUD9_INFO("Using custom environment variables from " << Environ);
		while (!f.eof()) {
			std::string line;
			std::getline(f, line);
			line = strip(line);
			if (!line.empty())
				items.push_back(line);
		}
		f.close();
		pEnvp = new char *[items.size() + 1];
		unsigned i = 0;
		for (; i != items.size(); ++i)
			pEnvp[i] = strdup(items[i].c_str());
		pEnvp[i] = 0;
	} else {
		pEnvp = envp;
	}

	pArgc = InputArgv.size() + 1;
	pArgv = new char *[pArgc];
	for (unsigned i = 0; i < InputArgv.size() + 1; i++) {
		std::string &arg = (i == 0 ? InputFile : InputArgv[i - 1]);
		unsigned size = arg.size() + 1;
		char *pArg = new char[size];

		std::copy(arg.begin(), arg.end(), pArg);
		pArg[size - 1] = 0;

		pArgv[i] = pArg;
	}
}

/*
 *
 */
static void interrupt_handle() {
	if (!Interrupted && theJobManager) {
		CLOUD9_INFO("Ctrl-C detected, requesting interpreter to halt.");
		haltExecution();
		sys::SetInterruptFunction(interrupt_handle);
	} else {
		CLOUD9_INFO("Ctrl+C detected, exiting.");
		exit(1);
	}
	Interrupted = true;
}

#ifdef CLOUD9_HAVE_WATCHDOG
static int watchdog(int pid) {
  CLOUD9_INFO("Watchdog: Watching " << pid);
  
  double nextStep = klee::util::getWallTime() + MaxTime * 1.1;
  int level = 0;
  
  // Simple stupid code...
  while (1) {
    sleep(1);
    
    int status, res = waitpid(pid, &status, WNOHANG);

    if (res < 0) {
      if (errno == ECHILD) { // No child, no need to watch but
	// return error since we didn't catch
	// the exit.
	perror("waitpid:");
	CLOUD9_INFO("Watchdog exiting (no child) @ " << klee::util::getWallTime());
	return 1;
      } else if (errno != EINTR) {
	perror("Watchdog waitpid");
	exit(1);
      }
    } else if (res == pid && WIFEXITED(status)) {
      return WEXITSTATUS(status);
    } else if (res == pid && WIFSIGNALED(status)) {
      CLOUD9_INFO("killed by signal " <<  WTERMSIG(status));
    } else if (res == pid && WIFSTOPPED(status)) {
      CLOUD9_INFO("stopped by signal " <<  WSTOPSIG(status));
    } else if ( res == pid && WIFCONTINUED(status)) {
      CLOUD9_INFO("continued\n");
    } else {
      double time = klee::util::getWallTime();

      if (time > nextStep) {
	++level;

	if (level == 1) {
	  CLOUD9_INFO("Watchdog: time expired, attempting halt via INT");
	  kill(pid, SIGINT);
	} else if (level == 2) {
	  CLOUD9_INFO("Watchdog: time expired, attempting halt via gdb");
	  haltViaGDB(pid);
	} else {
	  CLOUD9_INFO("Watchdog: kill(9)ing child (I tried to be nice)");
	  kill(pid, SIGKILL);
	  return 1; // what more can we do
	}

	// Ideally this triggers a dump, which may take a while,
	// so try and give the process extra time to clean up.
	nextStep = klee::util::getWallTime() + std::max(15., MaxTime
							* .1);
      }
    }
  }

  return 0;
}
#endif

int main(int argc, char **argv, char **envp) {
	// Make sure to clean up properly before any exit point in the program
	atexit(llvm::llvm_shutdown);

	GOOGLE_PROTOBUF_VERIFY_VERSION;

	cloud9::Logger::getLogger().setLogPrefix("Worker<   >: ");

	// JIT initialization
	llvm::InitializeNativeTarget();

	sys::PrintStackTraceOnErrorSignal();

	cloud9::initBreakSignal();

	// Fill up every global cl::opt object declared in the program
	parseArguments(argc, argv);

#ifdef CLOUD9_HAVE_WATCHDOG
	// Setup the watchdog process
	if (MaxTime == 0) {
		CLOUD9_INFO("No max time specified; running without watchdog");
	} else {
		int pid = fork();
		if (pid < 0) {
			CLOUD9_EXIT("Unable to fork watchdog");
		} else if (pid) {
			int returnCode = watchdog(pid);
			CLOUD9_INFO("Watchdog child exited with ret = " <<  returnCode);
			return returnCode;
			//return watchdog(pid);
		}
	}
#endif

	// At this point, if the watchdog is enabled, we are in the child process of
	// the fork().

	// Take care of Ctrl+C requests
	sys::SetInterruptFunction(interrupt_handle);

	Module *mainModule = loadByteCode();
	mainModule = prepareModule(mainModule);

	int pArgc;
	char **pArgv;
	char **pEnvp;
	readProgramArguments(pArgc, pArgv, pEnvp, envp);

	// Create the job manager
	theJobManager = new JobManager(mainModule, "main", pArgc, pArgv, envp);

	if (ReplayPath.size() > 0) {
		CLOUD9_INFO("Running in replay mode. No load balancer involved.");

		std::ifstream is(ReplayPath);

		if (is.fail()) {
			CLOUD9_EXIT("Could not open the replay file " << ReplayPath);
		}

		cloud9::ExecutionPathSetPin pathSet = cloud9::ExecutionPathSet::parse(is);

		theJobManager->processJobs(pathSet);
	} else {
		CommManager commManager(theJobManager); // Handle outside communication
		commManager.setup();

		theJobManager->processJobs((int)MaxTime.getValue()); // Blocking when no jobs are on the queue

		cloud9::instrum::theInstrManager.recordEvent(cloud9::instrum::TimeOut, "Timeout");

		// The order here matters, in order to avoid memory corruption
		commManager.finalize();
		theJobManager->finalize();
	}

	delete theJobManager;
	theJobManager = NULL;

	return 0;
}
