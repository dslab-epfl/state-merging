/*
 * underlying.c
 *
 *  Created on: Aug 10, 2010
 *      Author: stefan
 */

#define __FORCE_USE_MODELS
#include "models.h"

void _exit(int status);
void pthread_exit(void *value_ptr);

FORCE_IMPORT(_exit);
FORCE_IMPORT(pthread_exit);

__attribute__((used)) void __force_model_linkage(void) {
  // Just do nothing
}
