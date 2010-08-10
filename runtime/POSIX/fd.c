/*
 * fd.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "fd.h"

#include "lists.h"
#include "underlying.h"
#include "files.h"

#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// FD specific POSIX routines
////////////////////////////////////////////////////////////////////////////////

int close(int fd) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    int res = CALL_UNDERLYING(close, fde->concrete_fd);
    if (res == -1)
      errno = klee_get_errno();
    else
      STATIC_LIST_CLEAR(__fdt, fd);

    return res;
  }

  // Decrement the underlying IO object refcount
  assert(fde->io_object->refcount > 0);
  fde->io_object->refcount--;

  if (fde->io_object->refcount > 0) {
    // Just clear this FD
    STATIC_LIST_CLEAR(__fdt, fd);
    return 0;
  }

  // Check the type of the descriptor
  if (fde->attr & FD_IS_FILE) {
    _close_file((file_t*)fde->io_object);
  } else if (fde->attr & FD_IS_PIPE) {
    // XXX to do
  } else if (fde->attr & FD_IS_SOCKET) {
    // XXX to do
  }

  STATIC_LIST_CLEAR(__fdt, fd);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int dup3(int oldfd, int newfd, int flags) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)oldfd)) {
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

  if (STATIC_LIST_CHECK(__fdt, (unsigned)newfd)) {
    close(newfd);
  }

  __fdt[newfd] = __fdt[oldfd];

  fd_entry_t *fde = &__fdt[newfd];

  if (fde->attr & FD_IS_CONCRETE) {
    if (flags & FD_CLOSE_ON_EXEC) {
      fde->concrete_fd = CALL_UNDERLYING(fcntl, __fdt[oldfd].concrete_fd,
          F_DUPFD_CLOEXEC, 0);
    } else {
      fde->concrete_fd = CALL_UNDERLYING(fcntl, __fdt[oldfd].concrete_fd,
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
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)oldfd)) {
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
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)oldfd)) {
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

////////////////////////////////////////////////////////////////////////////////

int fcntl(int fd, int cmd, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
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
    int res = CALL_UNDERLYING(fcntl, __fdt[fd].concrete_fd, cmd, arg);
    if (res == -1) {
      errno = klee_get_errno();
      return -1;
    }

    return res;
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

int ioctl(int fd, unsigned long request, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  // For now, this works only on concrete FDs
  if (!(__fdt[fd].attr & FD_IS_CONCRETE)) {
    klee_warning("symbolic file, ioctl() unsupported (ENOTTY)");
    errno = ENOTTY;
    return -1;
  }

  char *argp;
  va_list ap;

  va_start(ap, request);
  argp = va_arg(ap, char*);

  va_end(ap);

  int ret = CALL_UNDERLYING(ioctl, __fdt[fd].concrete_fd, request, argp);

  if (ret == -1) {
    errno = klee_get_errno();
  }

  return ret;
}
