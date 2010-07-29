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
  klee_get_thread_info(0, &result);

  return result;
}
