/*
 * threads.h
 *
 *  Created on: Jul 31, 2010
 *      Author: stefan
 */

#ifndef THREADS_H_
#define THREADS_H_

typedef uint64_t wlist_id_t;

#define MAX_THREADS     8
#define DEFAULT_THREAD  0

#define MAX_PROCESSES   8
#define DEFAULT_PROCESS 2
#define DEFAULT_PARENT  1

typedef struct {
  wlist_id_t wlist;

  void *ret_value;

  char allocated;
  char terminated;
} thread_data_t;

typedef struct {
  wlist_id_t wlist;
  wlist_id_t children_wlist;

  pid_t parent;

  int ret_value;

  char allocated;
  char terminated;
} proc_data_t;

void klee_init_processes(void);
void klee_init_threads(void);


#endif /* THREADS_H_ */
