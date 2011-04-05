/*
 * signals.h
 *
 * Created on: 1 April 2011
 * Author: calin
 */

#include <signal.h>
#include <stdint.h>

#ifndef SIGNALS_H_
#define SIGNALS_H_

void __handle_signal();
void __klee_thread_preempt(int yield); 
void __klee_thread_sleep(uint64_t wlist); 
void klee_init_signals();

int __syscall_rt_sigaction(int signum, const struct sigaction *act, 
                           struct sigaction *oldact, size_t _something)
     __attribute__((weak));

int sigaction(int signum, const struct sigaction *act, 
              struct sigaction *oldact) __attribute__((weak));

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler);
sighandler_t sigset(int signum, sighandler_t disp);
int kill(int pid, int signum);
int sighold(int signum);
int sigrelse(int signum);
int sigignore(int signum);
unsigned int alarm(unsigned int seconds);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
     __attribute__((weak));

#endif /* SIGNALS_H_ */
