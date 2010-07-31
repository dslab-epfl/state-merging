/*
 * threads.h
 *
 *  Created on: Jul 31, 2010
 *      Author: stefan
 */

#ifndef THREADS_H_
#define THREADS_H_

typedef uint64_t wlist_id_t;
typedef uint64_t thread_id_t;

#define MAX_THREADS     8
#define DEFAULT_THREAD  0

typedef struct {
  wlist_id_t wlist_id;
  void *ret_value;
  char allocated;
  char terminated;
} thread_data_t;

void klee_init_threads(void);


#endif /* THREADS_H_ */
