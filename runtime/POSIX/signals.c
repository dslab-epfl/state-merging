/*
 * signals.c
 *
 * Created on: 1 April 2011
 * Author: calin
 */

#include "multiprocess.h"
#include "models.h"

#include "signals.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <bits/signum.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Internal routines
////////////////////////////////////////////////////////////////////////////////
/*
 * Main singal handler routine. Called when a pending signal is
 * detected.
 */
void __handle_signal() {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  if(pdata->sig_handlers[pdata->curr_signal] == SIG_DFL)
      klee_debug("Default signal handler called for signal %d\n", pdata->curr_signal);
    else if(pdata->sig_handlers[pdata->curr_signal] == SIG_IGN)
      klee_debug("Ignoring signal %d\n", pdata->curr_signal);
      else
        ((void(*)(int))(pdata->sig_handlers[pdata->curr_signal]))(pdata->curr_signal);
  pdata->signaled = 0;
  pdata->curr_signal = -1;
} 

/*
 * Wrapper over the klee_thread_preempt() call.
 * This is done to simulate checking for received
 * signals when being first planned.
 */
void __klee_thread_preempt(int yield) {
  klee_thread_preempt(yield);
  if((&__pdata[PID_TO_INDEX(getpid())])->signaled)
      __handle_signal();
}

/*
 * Wrapper over the klee_thread_sleep() call.
 * This is done to simulate checking for received
 * signals when being first planned.
 */
void __klee_thread_sleep(uint64_t wlist) {
  klee_thread_sleep(wlist);
  if((&__pdata[PID_TO_INDEX(getpid())])->signaled)
      __handle_signal();
}

void klee_init_signals() {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];

  pdata->signaled = 0;
  pdata->curr_signal = -1;
  memset(pdata->sig_handlers, 0, 32); /* XXX: Magic constant will be removed */
 
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// The POSIX API
////////////////////////////////////////////////////////////////////////////////
int __syscall_rt_sigaction(int signum, const struct sigaction *act, 
                           struct sigaction *oldact, size_t _something) {
  klee_warning("Received sigaction syscall");
  return 0;
}


int sigaction(int signum, const struct sigaction *act, 
              struct sigaction *oldact) {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  pdata->sig_handlers[signum] = act->sa_handler;
  return 0;
}

int kill(int pid, int signum) {

  proc_data_t *pdata = &__pdata[PID_TO_INDEX(pid)];

  if(pdata->signaled) {
    klee_warning("Process already signaled. Multiple pending signals not yet implemented.");
  } else {
    pdata->signaled = 1;
    pdata->curr_signal = signum;
  }
  return 0;
}

sighandler_t signal(int signum, sighandler_t handler) {
  return 0;
}

sighandler_t sigset(int sig, sighandler_t disp) {
  return 0;
}

int sighold(int sig) {
  return 0;
}

int sigrelse(int sig) {
  return 0;
}

int sigignore(int sig) {
  return 0;
}

unsigned int alarm(unsigned int seconds) {
  return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  return 0;
}
