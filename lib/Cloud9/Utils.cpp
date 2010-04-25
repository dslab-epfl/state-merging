/*
 * Utils.cpp
 *
 *  Created on: Mar 17, 2010
 *      Author: stefan
 */

#include "cloud9/Utils.h"

#include <cstdlib>

#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>


#define BREAK_SIGNAL	SIGUSR2

namespace cloud9 {

void initBreakSignal() {
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	act.sa_restorer = NULL;

	int result = sigaction(BREAK_SIGNAL, &act, NULL);

	assert(result == 0);
}

void breakSignal() {
	pid_t pid = getpid();

	int result = kill(pid, BREAK_SIGNAL);

	assert(result == 0);
}

}
