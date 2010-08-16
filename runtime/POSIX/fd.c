/*
 * fd.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "fd.h"

#include "common.h"
#include "models.h"
#include "files.h"
#include "sockets.h"
#include "pipes.h"
#include "buffers.h"

#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Internal routines
////////////////////////////////////////////////////////////////////////////////

void __adjust_fds_on_fork(void) {
  // Increment the ref. counters for all the open files pointed by the FDT.
  int fd;
  for (fd = 0; fd < MAX_FDS; fd++) {
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd))
      continue;
    if (__fdt[fd].attr & FD_IS_CONCRETE) {
      //printf("Duplicating fd %d: old concrete - %d ", fd, __fdt[fd].concrete_fd);
      __fdt[fd].concrete_fd = CALL_UNDERLYING(fcntl, __fdt[fd].concrete_fd,
          F_DUPFD, 0L);
      assert(__fdt[fd].concrete_fd != -1);
      //printf("new concrete - %d\n", __fdt[fd].concrete_fd);
    } else {
      __fdt[fd].io_object->refcount++;
    }
  }
}

void __close_fds(void) {
  int fd;
  for (fd = 0; fd < MAX_FDS; fd++) {
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd))
      continue;

    close(fd);
  }
}

////////////////////////////////////////////////////////////////////////////////
// FD specific POSIX routines
////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(ssize_t, read, int fd, void *buf, size_t count) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    buf = __concretize_ptr(buf);
    count = __concretize_size(count);
    /* XXX In terms of looking for bugs we really should do this check
       before concretization, at least once the routine has been fixed
       to properly work with symbolics. */
    klee_check_memory_access(buf, count);

    int res = CALL_UNDERLYING(read, fde->concrete_fd, buf, count);
    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  // Check for permissions
  if (!(fde->io_object->flags & (O_RDONLY | O_RDWR))) {
    errno = EBADF;
    return -1;
  }

  // It's OK, we pass the control to the specific implementations
  if (fde->attr & FD_IS_FILE) {
    return _read_file((file_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_PIPE) {
    return _read_pipe((pipe_end_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _read_socket((socket_t*)fde->io_object, buf, count);
  } else {
    assert(0 && "Invalid file descriptor");
    return -1;
  }
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(ssize_t, write, int fd, const void *buf, size_t count) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    //printf("Writing on fd %d (concrete value %d)\n", fd, fde->concrete_fd);
    buf = __concretize_ptr(buf);
    count = __concretize_size(count);
    /* XXX In terms of looking for bugs we really should do this check
      before concretization, at least once the routine has been fixed
      to properly work with symbolics. */
    klee_check_memory_access(buf, count);

    int res = CALL_UNDERLYING(write, fde->concrete_fd, buf, count);
    if (res == -1)
      errno = klee_get_errno();

    return res;
  }

  // Check for permissions
  if (!(fde->io_object->flags & (O_WRONLY | O_RDWR))) {
    errno = EBADF;
    return -1;
  }

  // Mkay, let's do it
  if (fde->attr & FD_IS_FILE) {
    return _write_file((file_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_PIPE) {
    return _write_pipe((pipe_end_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _write_socket((socket_t*)fde->io_object, buf, count);
  } else {
    assert(0 && "Invalid file descriptor");
    return -1;
  }
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, close, int fd) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    //printf("Closing fd %d (concrete %d)\n", fd, fde->concrete_fd);
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
    _close_pipe((pipe_end_t*)fde->io_object);
  } else if (fde->attr & FD_IS_SOCKET) {
    _close_socket((socket_t*)fde->io_object);
  } else {
    assert(0 && "Invalid file descriptor");
    return -1;
  }

  STATIC_LIST_CLEAR(__fdt, fd);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, fstat, int fd, struct stat *buf) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    int res = CALL_UNDERLYING(fstat, fde->concrete_fd, buf);

    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  if (fde->attr & FD_IS_FILE) {
    return _stat_file((file_t*)fde->io_object, buf);
  } else if (fde->attr & FD_IS_PIPE) {
    return _stat_pipe((pipe_end_t*)fde->io_object, buf);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _stat_socket((socket_t*)fde->io_object, buf);
  } else {
    assert(0 && "Invalid file descriptor");
    return -1;
  }
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, dup3, int oldfd, int newfd, int flags) {
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

DEFINE_MODEL(int, dup2, int oldfd, int newfd) {
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

DEFINE_MODEL(int, dup, int oldfd) {
  return _dup(oldfd, 0);
}

// select() ////////////////////////////////////////////////////////////////////

#define _FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define _FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define _FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define _FD_ZERO(p)  memset((char *)(p), '\0', sizeof(*(p)))

static int _validate_fd_set(int nfds, fd_set *fds) {
  int res = 0;

  int fd;
  for (fd = 0; fd < nfds; fd++) {
    if (!_FD_ISSET(fd, fds))
      continue;

    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd))
      return -1;

    if (__fdt[fd].attr & FD_IS_CONCRETE) {
      klee_warning("unsupported concrete FD in fd_set (EBADF)");
      return -1;
    }
    res++;
  }

  return res;
}

// XXX These functions could be refactored into implementations per object

static int _is_blocking(int fd, int event) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    return 0;
  }

  switch (event) {
  case EVENT_READ:
    if (!(__fdt[fd].io_object->flags & (O_RDWR | O_RDONLY))) {
      return 0;
    }
    break;
  case EVENT_WRITE:
    if (!(__fdt[fd].io_object->flags & (O_RDWR | O_WRONLY))) {
      return 0;
    }
    break;
  default:
    assert(0 && "invalid event");
  }

  if (__fdt[fd].attr & FD_IS_FILE) {
    return 0; // A file never blocks in our model
  } else if (__fdt[fd].attr & FD_IS_PIPE) {
    return _is_blocking_pipe((pipe_end_t*)__fdt[fd].io_object, event);
  } else if (__fdt[fd].attr & FD_IS_SOCKET) {
    return _is_blocking_socket((socket_t*)__fdt[fd].io_object, event);
  } else {
    assert(0 && "invalid fd");
  }
}

static int _register_events(int fd, wlist_id_t wlist, int events) {
  assert(STATIC_LIST_CHECK(__fdt, (unsigned)fd));

  if (__fdt[fd].attr & FD_IS_PIPE) {
    return _register_events_pipe((pipe_end_t*)__fdt[fd].io_object, wlist, events);
  } else if (__fdt[fd].attr & FD_IS_SOCKET) {
    return _register_events_socket((socket_t*)__fdt[fd].io_object, wlist, events);
  } else {
    assert(0 && "invalid fd");
  }
}

static void _deregister_events(int fd, wlist_id_t wlist, int events) {
  if(!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    // Silently exit
    return;
  }

  if (__fdt[fd].attr & FD_IS_PIPE) {
    _deregister_events_pipe((pipe_end_t*)__fdt[fd].io_object, wlist, events);
  } else if (__fdt[fd].attr & FD_IS_SOCKET) {
    _deregister_events_socket((socket_t*)__fdt[fd].io_object, wlist, events);
  } else {
    assert(0 && "invalid fd");
  }
}

// XXX Maybe we should break this into more pieces?
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
    struct timeval *timeout) {
  if (nfds < 0 || nfds > FD_SETSIZE) {
    errno = EINVAL;
    return -1;
  }

  int totalfds = 0;

  // Validating the fds
  if (readfds) {
    int res = _validate_fd_set(nfds, readfds);
    if (res ==  -1) {
      errno = EBADF;
      return -1;
    }

    totalfds += res;
  }

  if (writefds) {
    int res = _validate_fd_set(nfds, writefds);
    if (res == -1) {
      errno = EBADF;
      return -1;
    }

    totalfds += res;
  }

  if (exceptfds) {
    int res = _validate_fd_set(nfds, exceptfds);
    if (res == -1) {
      errno = EBADF;
      return -1;
    }

    totalfds += res;
  }

  if (totalfds == 0) // Nothing to watch for
    return 0;

  // Compute the minimum size of the FD set
  int setsize = ((nfds / NFDBITS) + ((nfds % NFDBITS) ? 1 : 0)) * (NFDBITS / 8);

  fd_set *out_readfds = NULL;
  fd_set *out_writefds = NULL;

  if (readfds) {
    out_readfds = (fd_set*)malloc(setsize);
    memset(out_readfds, 0, setsize);
  }
  if (writefds) {
    out_writefds = (fd_set*)malloc(setsize);
    memset(out_writefds, 0, setsize);
  }

  // No out_exceptfds here. This means that the thread will hang if select()
  // is called with FDs only in exceptfds.

  int fd;
  int res = 0;

  wlist_id_t wlist = 0;

  do {
    // First check to see if we have anything available
    for (fd = 0; fd < nfds; fd++) {
      if (readfds && _FD_ISSET(fd, readfds) && !_is_blocking(fd, EVENT_READ)) {
        _FD_SET(fd, out_readfds);
        res++;
      }
      if (writefds && _FD_ISSET(fd, writefds) && !_is_blocking(fd, EVENT_WRITE)) {
        _FD_SET(fd, out_writefds);
        res++;
      }
    }

    if (res == 0) { // Nope, bad luck...

      // We wait until at least one FD becomes non-blocking

      // In particular, if all FD blocked, then all of them would be
      // valid FDs (none of them closed in the mean time)
      if (wlist == 0)
        wlist = klee_get_wlist();

      int fail = 0;

      // Register ourselves to the relevant FDs
      for (fd = 0; fd < nfds; fd++) {
        int events = 0;
        if (readfds && _FD_ISSET(fd, readfds)) {
          events |= EVENT_READ;
        }
        if (writefds && _FD_ISSET(fd, writefds)) {
          events |= EVENT_WRITE;
        }

        if (events != 0) {
          if (_register_events(fd, wlist, events) == -1) {
            fail = 1;
            break;
          }
        }
      }

      if (!fail)
        klee_thread_sleep(wlist);

      // Now deregister, in order to avoid useless notifications
      for (fd = 0; fd < nfds; fd++) {
        int events = 0;
        if (readfds && _FD_ISSET(fd, readfds)) {
          events |= EVENT_READ;
        }
        if (writefds && _FD_ISSET(fd, writefds)) {
          events |= EVENT_WRITE;
        }

        if (events != 0) {
          _deregister_events(fd, wlist, events);
        }
      }

      if (fail) {
        errno = ENOMEM;
        return -1;
      }
    }

  } while (res == 0);

  if (readfds) {
    memcpy(readfds, out_readfds, setsize);
    free(out_readfds);
  }

  if (writefds) {
    memcpy(writefds, out_writefds, setsize);
    free(out_writefds);
  }

  if (exceptfds) {
    memset(exceptfds, 0, setsize);
  }

  return res;
}

#undef _FD_SET
#undef _FD_CLR
#undef _FD_ISSET
#undef _FD_ZERO

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, fcntl, int fd, int cmd, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  long arg;

  if (cmd == F_GETFD || cmd == F_GETFL || cmd == F_GETOWN || cmd == F_GETSIG
      || cmd == F_GETLEASE || cmd == F_NOTIFY) {
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

DEFINE_MODEL(int, ioctl, int fd, unsigned long request, ...) {
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
