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
#include <sys/stat.h>

#include "lists.h"
#include "common.h"

typedef uint64_t wlist_id_t;

#define DEFAULT_THREAD  0

#define DEFAULT_PROCESS 2
#define DEFAULT_PARENT  1
#define DEFAULT_UMASK   (S_IWGRP | S_IWOTH)

#define PID_TO_INDEX(pid)   ((pid) - 2)
#define INDEX_TO_PID(idx)   ((idx) + 2)

#define DEFAULT_MUTEX   0

#define MUTEX_TO_INDEX(mtx) ((mtx) - 1)
#define INDEX_TO_MUTEX(idx) ((idx) + 1)

#define DEFAULT_CONDVAR 0

#define COND_TO_INDEX(cond) ((cond) - 1)
#define INDEX_TO_COND(idx)  ((idx) + 1)

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

  char allocated;
  unsigned int mutex;
  unsigned int queued;
} condvar_data_t;

typedef struct {
  thread_data_t threads[MAX_THREADS];

  mutex_data_t mutexes[MAX_MUTEXES];
  condvar_data_t condvars[MAX_CONDVARS];
} tsync_data_t;

extern tsync_data_t __tsync;

void klee_init_processes(void);
void klee_init_threads(void);


#endif /* THREADS_H_ */
