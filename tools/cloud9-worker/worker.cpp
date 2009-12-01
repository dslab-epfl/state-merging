/*
 * worker.cpp
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */



#include <iostream>

#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetSelect.h"

#include "klee/Internal/System/Time.h"

#include "cloud9/common.h"
#include "cloud9/Logger.h"

// This is a temporary hack. If the running process has access to
// externals then it can disable interrupts, which screws up the
// normal "nice" watchdog termination process. We try to request the
// interpreter to halt using this mechanism as a last resort to save
// the state data before going ahead and killing it.
static void halt_via_gdb(int pid) {
  char buffer[256];
  sprintf(buffer,
          "gdb --batch --eval-command=\"p halt_execution()\" "
          "--eval-command=detach --pid=%d &> /dev/null",
          pid);

  if (system(buffer)==-1)
    perror("system");
}


#ifdef CLOUD9_HAVE_WATCHDOG
static void watchdog(int pid) {
	CLOUD9_DEBUG("Watchdog: Watching " << pid);

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
				CLOUD9_DEBUG("Watchdog exiting (no child)");
				return 1;
			} else if (errno != EINTR) {
				perror("Watchdog waitpid");
				exit(1);
			}
		} else if (res == pid && WIFEXITED(status)) {
			return WEXITSTATUS(status);
		} else {
			double time = klee::util::getWallTime();

			if (time > nextStep) {
				++level;

				if (level == 1) {
					CLOUD9_WARNING("Watchdog: time expired, attempting halt via INT");
					kill(pid, SIGINT);
				} else if (level == 2) {
					CLOUD9_WARNING("Watchdog: time expired, attempting halt via gdb");
					halt_via_gdb(pid);
				} else {
					CLOUD9_WARNING("Watchdog: kill(9)ing child (I tried to be nice)");
					kill(pid, SIGKILL);
					return 1; // what more can we do
				}

				// Ideally this triggers a dump, which may take a while,
				// so try and give the process extra time to clean up.
				nextStep = klee::util::getWallTime() + std::max(15., MaxTime * .1);
			}
		}
	}

	return 0;
}
#endif

int main(int argc, char **argv, char **envp) {
	atexit(llvm::llvm_shutdown);

	llvm::InitializeNativeTarget();

	CLOUD9_DEBUG("Bubu " << 17 << " " << 3.3);

#ifdef CLOUD9_HAVE_WATCHDOG
	if (MaxTime == 0) {
		CLOUD9_EXIT("--watchdog used without --max-time");
	}

	int pid = fork();
	if (pid < 0) {
		CLOUD9_EXIT("unable to fork watchdog");
	} else if (pid) {
		watchdog(pid);
	}
#endif

	return 0;
}
