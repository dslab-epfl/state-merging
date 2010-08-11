/*
 * maxlimits.h
 *
 *  Created on: Aug 6, 2010
 *      Author: stefan
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <sys/types.h>

#define MAX_THREADS     8
#define MAX_PROCESSES   8

#define MAX_MUTEXES     16
#define MAX_CONDVARS    16

#define MAX_EVENTS      4

#define MAX_FDS         64
#define MAX_FILES       16

#define MAX_PATH_LEN    75

#define CALL_UNDERLYING(name, ...) \
    __klee_original_ ## name(__VA_ARGS__);

int klee_get_errno(void);

void *__concretize_ptr(const void *p);
size_t __concretize_size(size_t s);
const char *__concretize_string(const char *s);

#endif /* COMMON_H_ */
