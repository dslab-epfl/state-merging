/*
 * stubs.h
 *
 *  Created on: Sep 30, 2010
 *      Author: stefan
 */

#ifndef STUBS_H_
#define STUBS_H_

#include <stdio.h>

#ifdef  __cplusplus
extern "C" {
#endif

void klee_make_symbolic(void *addr, size_t nbytes, const char *name) __attribute__((weak));

#define KLEE_SIO_SYMREADS   0xfff00     // Enable symbolic reads for a socket

#ifdef  __cplusplus
}
#endif


#endif /* STUBS_H_ */
