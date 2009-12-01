/*
 * worker.cpp
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */



#include <iostream>

#include <cstdlib>
#include <unistd.h>

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetSelect.h"

#include "cloud9/Logger.h"

///////////////////
/* Definitions */


#ifdef CLOUD9_HAVE_WATCHDOG
static void watchdog(int pid) {
	fprintf(stderr, "KLEE: WATCHDOG: watching %d\n", pid);
	fflush( stderr);

	double nextStep = util::getWallTime() + MaxTime * 1.1;
	int level = 0;

	// Simple stupid code...
	while (1) {
		sleep(1);

		int status, res = waitpid(pid, &status, WNOHANG);

		if (res < 0) {
			if (errno == ECHILD) { // No child, no need to watch but
				// return error since we didn't catch
				// the exit.
				fprintf(stderr, "KLEE: watchdog exiting (no child)\n");
				return 1;
			} else if (errno != EINTR) {
				perror("watchdog waitpid");
				exit(1);
			}
		} else if (res == pid && WIFEXITED(status)) {
			return WEXITSTATUS(status);
		} else {
			double time = util::getWallTime();

			if (time > nextStep) {
				++level;

				if (level == 1) {
					fprintf(stderr,
							"KLEE: WATCHDOG: time expired, attempting halt via INT\n");
					kill(pid, SIGINT);
				} else if (level == 2) {
					fprintf(stderr,
							"KLEE: WATCHDOG: time expired, attempting halt via gdb\n");
					halt_via_gdb( pid);
				} else {
					fprintf(stderr,
							"KLEE: WATCHDOG: kill(9)ing child (I tried to be nice)\n");
					kill(pid, SIGKILL);
					return 1; // what more can we do
				}

				// Ideally this triggers a dump, which may take a while,
				// so try and give the process extra time to clean up.
				nextStep = util::getWallTime() + std::max(15., MaxTime * .1);
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
		klee_error("--watchdog used without --max-time");
	}

	int pid = fork();
	if (pid < 0) {
		klee_error("unable to fork watchdog");
	} else if (pid) {
		watchdog(pid)
	}
#endif

	return 0;
}
