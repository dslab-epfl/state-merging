/*
 * pthread.c
 *
 *  Created on: Jul 29, 2010
 *      Author: stefan
 */

#include <pthread.h>

#include <klee/klee.h>

pthread_t pthread_self(void) {
  pthread_t result;

  klee_get_context(&result, 0, 0);

  return result;
}
