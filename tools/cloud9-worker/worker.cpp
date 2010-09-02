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
#include "klee/Init.h"

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

/*
 *
 */
static void parseArguments(int argc, char **argv) {
	// TODO: Implement some filtering, or reading from a settings file, or
	// from stdin
	cl::ParseCommandLineOptions(argc, argv, "Cloud9 worker");
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
		exit(1); // XXX Replace this with pthread_exit() or with longjmp
	}
	Interrupted = true;
}

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

	// At this point, if the watchdog is enabled, we are in the child process of
	// the fork().

	// Take care of Ctrl+C requests
	sys::SetInterruptFunction(interrupt_handle);

	Module *mainModule = klee::loadByteCode();
	mainModule = klee::prepareModule(mainModule);

	int pArgc;
	char **pArgv;
	char **pEnvp;
	klee::readProgramArguments(pArgc, pArgv, pEnvp, envp);

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
