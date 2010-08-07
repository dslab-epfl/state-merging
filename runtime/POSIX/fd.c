/*
 * fd.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "fd.h"

#include "lists.h"

#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include <klee/klee.h>

#define _WRAP_FD_SYSCALL_ERROR(call, ...) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, fd)) { \
      errno = EBADF; \
      return -1; \
    } \
    if (!(__fdt[fd]).flags & FD_IS_CONCRETE) { \
      klee_warning("symbolic file, " #call "unsupported (EBADF)"); \
      errno = EBADF; \
      return -1; \
    } \
    int ret = klee_call_underlying_i32(#call, ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

#define _WRAP_FD_SYSCALL_IGNORE(call, ...) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, fd)) { \
      errno = EBADF; \
      return -1; \
    } \
    if (!(__fdt[fd]).flags & FD_IS_CONCRETE) { \
      return 0; \
    } \
    int ret = klee_call_underlying_i32(#call, ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

int ioctl(int fd, int requrest, ...) {
  if (!STATIC_LIST_CHECK(__fdt, fd)) {
    errno = EBADF;
    return -1;
  }

  // For now, this works only on concrete FDs
  if (!(__fdt[fd].flags & FD_IS_CONCRETE)) {
    klee_warning("symbolic file, ioctl() unsupported (ENOTTY)");
    errno = ENOTTY;
    return -1;
  }

  char *argp;
  va_list ap;

  va_start(ap, request);
  argp = va_arg(ap, char*);

  va_end(ap);

  int ret = klee_call_underlying_i32("ioctl", __fdt[fd].concrete_fd, request, argp);

  if (ret == -1) {
    errno = klee_get_errno();
  }

  return ret;
}

int fsync(int fd) {
  _WRAP_FD_SYSCALL_IGNORE(fsync);
}

int fdatasync(int fd) {
  _WRAP_FD_SYSCALL_IGNORE(fdatasync);
}
