/*
 * multiprocess_init.c
 *
 *  Created on: Aug 2, 2010
 *      Author: stefan
 */

#include "multiprocess.h"

#include "models.h"

#include <string.h>
#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Processes
////////////////////////////////////////////////////////////////////////////////

proc_data_t __pdata[MAX_PROCESSES];

void klee_init_processes(void) {
  STATIC_LIST_INIT(__pdata);
  klee_make_shared(&__pdata, sizeof(__pdata));

  proc_data_t *pdata = &__pdata[PID_TO_INDEX(DEFAULT_PROCESS)];
  pdata->allocated = 1;
  pdata->terminated = 0;
  pdata->parent = DEFAULT_PARENT;
  pdata->umask = DEFAULT_UMASK;
  pdata->wlist = klee_get_wlist();
  pdata->children_wlist = klee_get_wlist();

  klee_init_threads();
}

////////////////////////////////////////////////////////////////////////////////
// Threads
////////////////////////////////////////////////////////////////////////////////

tsync_data_t __tsync;

void klee_init_threads(void) {
  STATIC_LIST_INIT(__tsync.threads);
  STATIC_LIST_INIT(__tsync.mutexes);
  STATIC_LIST_INIT(__tsync.condvars);

  // Thread initialization
  thread_data_t *def_data = &__tsync.threads[DEFAULT_THREAD];
  def_data->allocated = 1;
  def_data->terminated = 0;
  def_data->ret_value = 0;
  def_data->joinable = 1; // Why not?
  def_data->wlist = klee_get_wlist();
}
