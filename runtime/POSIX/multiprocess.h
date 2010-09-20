/*
 * threads.h
 *
 *  Created on: Jul 31, 2010
 *      Author: stefan
 */

#ifndef THREADS_H_
#define THREADS_H_

#include <stdint.h>
#include <sys/types.h>

#include "common.h"

typedef uint64_t wlist_id_t;

#define DEFAULT_THREAD  0

#define DEFAULT_PROCESS 2
#define DEFAULT_PARENT  1
#define DEFAULT_UMASK   (S_IWGRP | S_IWOTH)

#define PID_TO_INDEX(pid)   ((pid) - 2)
#define INDEX_TO_PID(idx)   ((idx) + 2)

#define STATIC_MUTEX_VALUE      0
#define STATIC_CVAR_VALUE       0

typedef struct {
  wlist_id_t wlist;
  wlist_id_t children_wlist;

  pid_t parent;
  mode_t umask;

  int ret_value;

  char allocated;
  char terminated;
} proc_data_t;

extern proc_data_t __pdata[MAX_PROCESSES];

typedef struct {
  wlist_id_t wlist;

  void *ret_value;

  char allocated;
  char terminated;
  char joinable;
} thread_data_t;

typedef struct {
  wlist_id_t wlist;

  char taken;
  unsigned int owner;
  unsigned int queued;

  char allocated;
} mutex_data_t;

typedef struct {
  wlist_id_t wlist;

  mutex_data_t *mutex;
  unsigned int queued;
} condvar_data_t;

typedef struct {
  thread_data_t threads[MAX_THREADS];
} tsync_data_t;

extern tsync_data_t __tsync;

void klee_init_processes(void);
void klee_init_threads(void);


#endif /* THREADS_H_ */
