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
#include <fcntl.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// FD specific POSIX routines
////////////////////////////////////////////////////////////////////////////////

int dup3(int oldfd, int newfd, int flags) {
  if (!STATIC_LIST_CHECK(__fdt, oldfd)) {
    errno = EBADF;
    return -1;
  }

  if (newfd >= MAX_FDS) {
    errno = EBADF;
    return -1;
  }

  if (newfd == oldfd) {
    errno = EINVAL;
    return -1;
  }

  if (STATIC_LIST_CHECK(__fdt, newfd)) {
    close(newfd);
  }

  __fdt[newfd] = __fdt[oldfd];

  fd_entry_t *fde = &__fdt[newfd];

  if (fde->attr & FD_IS_CONCRETE) {
    if (flags & FD_CLOSE_ON_EXEC) {
      fde->concrete_fd = klee_call_underlying_i32("fcntl", __fdt[oldfd].concrete_fd,
          F_DUPFD_CLOEXEC, 0);
    } else {
      fde->concrete_fd = klee_call_underlying_i32("fcntl", __fdt[oldfd].concrete_fd,
          F_DUPFD, 0);
    }

    if (fde->concrete_fd < 0) {
      STATIC_LIST_CLEAR(__fdt, newfd);
      errno = klee_get_errno();
      return -1;
    }

  } else {
    fde->io_object->refcount++;

    if (flags & O_CLOEXEC) {
      fde->attr |= FD_CLOSE_ON_EXEC;
    } else {
      fde->attr &= ~FD_CLOSE_ON_EXEC;
    }
  }

  return newfd;
}

int dup2(int oldfd, int newfd) {
  if (!STATIC_LIST_CHECK(__fdt, oldfd)) {
    errno = EBADF;
    return -1;
  }

  if (newfd >= MAX_FDS) {
    errno = EBADF;
    return -1;
  }

  if (newfd == oldfd)
    return 0;

  return dup3(oldfd, newfd, 0);
}

static int _dup(int oldfd, int startfd) {
  if (!STATIC_LIST_CHECK(__fdt, oldfd)) {
    errno = EBADF;
    return -1;
  }

  if (startfd < 0 || startfd >= MAX_FDS) {
    errno = EBADF;
    return -1;
  }

  // Find the lowest unused file descriptor
  int fd;
  for (fd = startfd; fd < MAX_FDS; fd++) {
    if (!__fdt[fd].allocated)
      break;
  }

  if (fd == MAX_FDS) {
    errno = EMFILE;
    return -1;
  }

  return dup2(oldfd, fd);
}

int dup(int oldfd) {
  return _dup(oldfd, 0);
}

int fcntl(int fd, int cmd, ...) {
  if (!STATIC_LIST_CHECK(__fdt, fd)) {
    errno = EBADF;
    return -1;
  }

  long arg;

  if (cmd & (F_GETFD | F_GETFL | F_GETOWN | F_GETSIG | F_GETLEASE | F_NOTIFY)) {
    arg = 0;
  } else {
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);
  }

  if (__fdt[fd].attr & FD_IS_CONCRETE) {
    int res = klee_call_underlying_i32("fcntl", __fdt[fd].concrete_fd, cmd, arg);
    if (res == -1) {
      errno = klee_get_errno();
      return -1;
    }
  } else {
    fd_entry_t *fde = &__fdt[fd];
    int res;

    switch (cmd) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
      res = _dup(fd, arg);
      if (res < -1) {
        return res;
      }

      if (cmd == F_DUPFD_CLOEXEC) {
        __fdt[res].attr |= FD_CLOEXEC;
      }
      break;

    case F_SETFD:
      if (arg & FD_CLOEXEC) {
        fde->attr |= FD_CLOSE_ON_EXEC;
      }
      // Go through the next case
    case F_GETFD:
      res = (fde->attr & FD_CLOSE_ON_EXEC) ? FD_CLOEXEC : 0;
      break;

    case F_SETFL:
      fde->io_object->flags |= (arg & (O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME | O_NONBLOCK));
      // Go through the next case
    case F_GETFL:
      res = fde->io_object->flags;
      break;

    default:
      klee_warning("symbolic file, ignoring (EINVAL)");
      errno = EINVAL;
      return -1;
    }

    return res;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Forwarded / unsupported calls
////////////////////////////////////////////////////////////////////////////////

#define _WRAP_FD_SYSCALL_ERROR(call, ...) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, fd)) { \
      errno = EBADF; \
      return -1; \
    } \
    if (!((__fdt[fd]).flags & FD_IS_CONCRETE)) { \
      klee_warning("symbolic file, " #call " unsupported (EBADF)"); \
      errno = EBADF; \
      return -1; \
    } \
    int ret = klee_call_underlying_i32(#call, __fdt[fd].concrete_fd, ##__VA_ARGS__); \
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
    if (!((__fdt[fd]).flags & FD_IS_CONCRETE)) { \
      return 0; \
    } \
    int ret = klee_call_underlying_i32(#call, __fdt[fd].concrete_fd, ##__VA_ARGS__); \
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

int fchdir(int fd) {
  _WRAP_FD_SYSCALL_ERROR(fchdir);
}

int fchown(int fd, uid_t owner, gid_t group) {
  _WRAP_FD_SYSCALL_ERROR(fchown, owner, group);
}
