/*
 * processes.c
 *
 *  Created on: Jul 29, 2010
 *      Author: stefan
 */

#include "multiprocess.h"
#include "fd.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <klee/klee.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

////////////////////////////////////////////////////////////////////////////////
// The POSIX API
////////////////////////////////////////////////////////////////////////////////

pid_t getpid(void) {
  pid_t pid;
  klee_get_context(0, &pid);

  return pid;
}

pid_t getppid(void) {
  pid_t pid;
  klee_get_context(0, &pid);

  return __pdata[PID_TO_INDEX(pid)].parent;
}

mode_t umask(mode_t mask) {
  proc_data_t *pdata = &__pdata[PID_TO_INDEX(getpid())];
  mode_t res = pdata->umask;

  pdata->umask = mask & 0777;

  return res;
}

void _exit(int status) {
  pid_t pid = getpid();

  // Checking for zombie processes
  unsigned int idx;
  for (idx = 0; idx < MAX_PROCESSES; idx++) {
    if (__pdata[idx].allocated && __pdata[idx].parent == pid) {
      assert(0 && "zombie process");
    }
  }

  proc_data_t *pdata = &__pdata[PID_TO_INDEX(pid)];
  pdata->terminated = 1;
  pdata->ret_value = status;
  klee_thread_notify_all(pdata->wlist);

  if (pdata->parent != DEFAULT_PARENT) {
    proc_data_t *ppdata = &__pdata[PID_TO_INDEX(pdata->parent)];
    klee_thread_notify_all(ppdata->children_wlist);
  }

  klee_process_terminate();
}

pid_t fork(void) {
  unsigned int newIdx;
  STATIC_LIST_ALLOC(__pdata, newIdx);

  if (newIdx == MAX_PROCESSES) {
    errno = ENOMEM;
    return -1;
  }

  proc_data_t *ppdata = &__pdata[PID_TO_INDEX(getpid())];

  proc_data_t *pdata = &__pdata[newIdx];
  pdata->terminated = 0;
  pdata->wlist = klee_get_wlist();
  pdata->children_wlist = klee_get_wlist();
  pdata->parent = getpid();
  pdata->umask = ppdata->umask;

  int res = klee_process_fork(INDEX_TO_PID(newIdx)); // Here we split our ways...

  if (res == 0) {
    // We're in the child. Re-initialize the threading structures
    klee_init_threads();

    __adjust_fds_on_fork();
  }

  return res;
}

pid_t vfork(void) {
  pid_t pid = fork();

  if (pid > 0) {
    waitpid(pid, 0, 0);
  }

  return pid;
}

pid_t wait(int *status) {
  return waitpid(-1, status, 0);
}

pid_t waitpid(pid_t pid, int *status, int options) {
  if (pid < -1 || pid == 0) {
    klee_warning("unsupported process group wait() call");

    errno = EINVAL;
    return -1;
  }

  if ((WUNTRACED | WCONTINUED) & options) {
    klee_warning("unsupported waitpid() options");

    errno = EINVAL;
    return -1;
  }

  pid_t ppid = getpid();

  unsigned int idx = MAX_PROCESSES; // The index of the terminated child
  proc_data_t *pdata = 0;

  if (pid == -1) {
    do {
      // Look up children
      int i;
      int hasChildren = 0;

      for (i = 0; i < MAX_PROCESSES; i++) {
        if (!__pdata[i].allocated || __pdata[i].parent != ppid)
          continue;

        hasChildren = 1;
        if (__pdata[i].terminated) {
          idx = i;
          pdata = &__pdata[i];
          break;
        }
      }

      if (idx != MAX_PROCESSES)
        break;

      if (!hasChildren) {
        errno = ECHILD;
        return -1;
      }

      if (WNOHANG & options)
        return 0;

      klee_thread_sleep(__pdata[PID_TO_INDEX(ppid)].children_wlist);

    } while (1);

  } else {
    idx = PID_TO_INDEX(pid);

    if (idx >= MAX_PROCESSES) {
      errno = ECHILD;
      return -1;
    }

    pdata = &__pdata[idx];

    if (!pdata->allocated || pdata->parent != ppid) {
      errno = ECHILD;
      return -1;
    }

    if (!pdata->terminated) {
      if (WNOHANG & options)
        return 0;

      klee_thread_sleep(pdata->wlist);
    }
  }

  if (status) {
    // XXX Is this documented stuff?
    *status = (pdata->ret_value & 0xff) << 8;
  }

  // Now we can safely clean up the process structure
  STATIC_LIST_CLEAR(__pdata, idx);

  return INDEX_TO_PID(idx);
}

int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options) {
  assert(0 && "not implemented");
  return -1;
}
