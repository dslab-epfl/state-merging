/*
 * unistd.c
 *
 *  Created on: Jul 29, 2010
 *      Author: stefan
 */

#include <sys/types.h>
#include <unistd.h>
#include <klee/klee.h>

pid_t getpid(void) {
  pid_t result;
  klee_get_context(0, &result, 0);

  return result;
}

pid_t getppid(void) {
  pid_t result;
  klee_get_context(0, 0, &result);

  return result;
}

void exit(int status) {
  klee_process_terminate();
}

void _exit(int status) {
  exit(status);
}
