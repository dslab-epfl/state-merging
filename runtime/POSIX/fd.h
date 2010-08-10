/*
 * fd.h
 *
 *  Created on: Aug 6, 2010
 *      Author: stefan
 */

#ifndef FD_H_
#define FD_H_

#include "common.h"

#define FD_IS_CONCRETE      (1 << 0)    // The calls get forwarded to the underlying implementation
#define FD_IS_DUMMY         (1 << 1)    // The data read is symbolic, and the data written is discarded
#define FD_IS_FILE          (1 << 3)    // The fd points to a disk file
#define FD_IS_SOCKET        (1 << 4)    // The fd points to a socket
#define FD_IS_PIPE          (1 << 5)    // The fd points to a pipe
#define FD_CLOSE_ON_EXEC    (1 << 6)    // The fd should be closed at exec() time (ignored)

typedef struct {
  unsigned int refcount;
  int flags;
} file_base_t;

typedef struct {
  unsigned int attr;

  int concrete_fd;

  file_base_t *io_object;

  char allocated;
} fd_entry_t;

extern fd_entry_t __fdt[MAX_FDS];

#define _WRAP_FD_SYSCALL_ERROR(call, ...) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) { \
      errno = EBADF; \
      return -1; \
    } \
    if (!((__fdt[fd]).attr & FD_IS_CONCRETE)) { \
      klee_warning("symbolic file, " #call " unsupported (EBADF)"); \
      errno = EBADF; \
      return -1; \
    } \
    int ret = CALL_UNDERLYING(call, __fdt[fd].concrete_fd, ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

#define _WRAP_FD_SYSCALL_IGNORE(call, ...) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) { \
      errno = EBADF; \
      return -1; \
    } \
    if (!((__fdt[fd]).attr & FD_IS_CONCRETE)) { \
      klee_warning("symbolic file, " #call " does nothing"); \
      return 0; \
    } \
    int ret = CALL_UNDERLYING(call, __fdt[fd].concrete_fd, ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

void klee_init_fds(unsigned n_files, unsigned file_length,
                   int sym_stdout_flag);


#endif /* FD_H_ */
