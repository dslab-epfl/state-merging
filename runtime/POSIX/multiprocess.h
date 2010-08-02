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

typedef uint64_t wlist_id_t;

#define MAX_THREADS     8
#define DEFAULT_THREAD  0

#define MAX_PROCESSES   8
#define DEFAULT_PROCESS 2
#define DEFAULT_PARENT  1

#define PID_TO_INDEX(pid)   ((pid) - 2)
#define INDEX_TO_PID(idx)   ((idx) + 2)

typedef struct {
  wlist_id_t wlist;
  wlist_id_t children_wlist;

  pid_t parent;

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
} thread_data_t;

extern thread_data_t __tdata[MAX_THREADS];

void klee_init_processes(void);
void klee_init_threads(void);


#endif /* THREADS_H_ */
