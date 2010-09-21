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
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <klee/klee.h>

// For multiplexing multiple syscalls into a single _read/_write op. set
#define _IO_TYPE_SCATTER_GATHER  0x1
#define _IO_TYPE_POSITIONAL      0x2

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

static int _is_blocking(int fd, int event) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    return 0;
  }

  switch (event) {
  case EVENT_READ:
    if ((__fdt[fd].io_object->flags & O_ACCMODE) == O_WRONLY) {
      return 0;
    }
    break;
  case EVENT_WRITE:
    if ((__fdt[fd].io_object->flags & O_ACCMODE) == O_RDONLY) {
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

static ssize_t _clean_read(int fd, void *buf, size_t count) {
  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_FILE) {
    return _read_file((file_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_PIPE) {
    return _read_pipe((pipe_end_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _read_socket((socket_t*)fde->io_object, buf, count);
  } else {
    assert(0 && "Invalid file descriptor");
  }
}

static ssize_t _clean_write(int fd, const void *buf, size_t count) {
  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_FILE) {
    return _write_file((file_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_PIPE) {
    return _write_pipe((pipe_end_t*)fde->io_object, buf, count);
  } else if (fde->attr & FD_IS_SOCKET) {
    return _write_socket((socket_t*)fde->io_object, buf, count);
  } else {
    assert(0 && "Invalid file descriptor");
  }
}

////////////////////////////////////////////////////////////////////////////////

ssize_t _scatter_read(int fd, const struct iovec *iov, int iovcnt) {
  size_t count = 0;

  int i;
  for (i = 0; i < iovcnt; i++) {
    if (iov[i].iov_len == 0)
      continue;

    if (count > 0 && _is_blocking(fd, EVENT_READ))
      return count;

    ssize_t res = _clean_read(fd, iov[i].iov_base, iov[i].iov_len);

    if (res == 0 || res == -1) {
      assert(count == 0);
      return res;
    }

    count += res;
  }

  return count;
}

ssize_t _gather_write(int fd, const struct iovec *iov, int iovcnt) {
  size_t count = 0;

  int i;
  for (i = 0; i < iovcnt; i++) {
    if (iov[i].iov_len == 0)
      continue;

    // If we have something written, but now we blocked, we just return
    // what we have
    if (count > 0 && _is_blocking(fd, EVENT_WRITE))
      return count;

    ssize_t res = _clean_write(fd, iov[i].iov_base, iov[i].iov_len);

    if (res == 0 || res == -1) {
      assert(count == 0);
      return res;
    }

    count += res;
  }

  return count;
}

////////////////////////////////////////////////////////////////////////////////
// FD specific POSIX routines
////////////////////////////////////////////////////////////////////////////////

static ssize_t _read(int fd, int type, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    if (type & _IO_TYPE_SCATTER_GATHER) {
      klee_warning("unsupported concrete scatter gather");

      errno = EINVAL;
      return -1;
    }

    va_list ap;
    va_start(ap, type);
    void *buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);
    va_end(ap);

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

  //fprintf(stderr, "Reading %lu bytes of data from FD %d\n", count, fd);

  // Check for permissions
  if ((fde->io_object->flags & O_ACCMODE) == O_WRONLY) {
    klee_debug("Permission error (flags: %o)\n", fde->io_object->flags);
    errno = EBADF;
    return -1;
  }

  if (INJECT_FAULT(read, EINTR)) {
    return -1;
  }

  if (fde->attr & FD_IS_FILE) {
    if (INJECT_FAULT(read, EIO)) {
      return -1;
    }
  } else if (fde->attr & FD_IS_SOCKET) {
    socket_t *sock = (socket_t*)fde->io_object;

    if (sock->status == SOCK_STATUS_CONNECTED &&
        INJECT_FAULT(read, ECONNRESET)) {
      return -1;
    }
  }

  // Now perform the real thing
  if (type & _IO_TYPE_SCATTER_GATHER) {
    va_list ap;
    va_start(ap, type);
    struct iovec *iov = va_arg(ap, struct iovec*);
    int iovcnt = va_arg(ap, int);
    va_end(ap);

    return _scatter_read(fd, iov, iovcnt);
  } else {
    va_list ap;
    va_start(ap, type);
    void *buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);
    va_end(ap);

    return _clean_read(fd, buf, count);
  }
}

DEFINE_MODEL(ssize_t, read, int fd, void *buf, size_t count) {
  return _read(fd, 0, buf, count);
}

DEFINE_MODEL(ssize_t, readv, int fd, const struct iovec *iov, int iovcnt) {
  return _read(fd, _IO_TYPE_SCATTER_GATHER, iov, iovcnt);
}

////////////////////////////////////////////////////////////////////////////////

static ssize_t _write(int fd, int type, ...) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    if (type & _IO_TYPE_SCATTER_GATHER) {
      klee_warning("unsupported concrete scatter gather");

      errno = EINVAL;
      return -1;
    }

    va_list ap;
    va_start(ap, type);
    void *buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);
    va_end(ap);

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
  if ((fde->io_object->flags & O_ACCMODE) == O_RDONLY) {
    errno = EBADF;
    return -1;
  }

  if (INJECT_FAULT(write, EINTR)) {
    return -1;
  }

  if (fde->attr & FD_IS_FILE) {
    if (INJECT_FAULT(write, EIO, EFBIG, ENOSPC)) {
      return -1;
    }
  } else if (fde->attr & FD_IS_SOCKET) {
    socket_t *sock = (socket_t*)fde->io_object;

    if (sock->status == SOCK_STATUS_CONNECTED &&
        INJECT_FAULT(write, ECONNRESET, ENOMEM)) {
      return -1;
    }
  }

  if (type & _IO_TYPE_SCATTER_GATHER) {
    va_list ap;
    va_start(ap, type);
    struct iovec *iov = va_arg(ap, struct iovec*);
    int iovcnt = va_arg(ap, int);
    va_end(ap);

    return _gather_write(fd, iov, iovcnt);
  } else {
    va_list ap;
    va_start(ap, type);
    void *buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);
    va_end(ap);

    return _clean_write(fd, buf, count);
  }
}

DEFINE_MODEL(ssize_t, write, int fd, const void *buf, size_t count) {
  return _write(fd, 0, buf, count);
}

DEFINE_MODEL(ssize_t, writev, int fd, const struct iovec *iov, int iovcnt) {
  return _write(fd, _IO_TYPE_SCATTER_GATHER, iov, iovcnt);
}

////////////////////////////////////////////////////////////////////////////////

DEFINE_MODEL(int, close, int fd) {
  if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
    errno = EBADF;
    return -1;
  }

  //klee_debug("Closing FD %d (pid %d)\n", fd, getpid());

  fd_entry_t *fde = &__fdt[fd];

  if (fde->attr & FD_IS_CONCRETE) {
    //fprintf(stderr, "Closing concrete fd: %d\n", fde->concrete_fd);
    //int res = CALL_UNDERLYING(close, fde->concrete_fd);
    //if (res == -1)
    //  errno = klee_get_errno();
    //else

    STATIC_LIST_CLEAR(__fdt, fd);

    return 0;
  }

  if (INJECT_FAULT(close, EINTR)) {
    return -1;
  }

  // Decrement the underlying IO object refcount
  assert(fde->io_object->refcount > 0);

  if (fde->io_object->refcount > 1) {
    fde->io_object->refcount--;
    // Just clear this FD
    STATIC_LIST_CLEAR(__fdt, fd);
    return 0;
  }

  int res;

  // Check the type of the descriptor
  if (fde->attr & FD_IS_FILE) {
    res = _close_file((file_t*)fde->io_object);
  } else if (fde->attr & FD_IS_PIPE) {
    res = _close_pipe((pipe_end_t*)fde->io_object);
  } else if (fde->attr & FD_IS_SOCKET) {
    res = _close_socket((socket_t*)fde->io_object);
  } else {
    assert(0 && "Invalid file descriptor");
    return -1;
  }

  if (res == 0)
    STATIC_LIST_CLEAR(__fdt, fd);

  return res;
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

  if (INJECT_FAULT(dup, EMFILE, EINTR)) {
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
  klee_debug("New duplicate of %d: %d\n", oldfd, newfd);
  if (__fdt[newfd].attr & FD_IS_CONCRETE) {
    klee_debug("New concrete duplicate of %d: %d\n", __fdt[oldfd].concrete_fd, __fdt[newfd].concrete_fd);
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

  return CALL_MODEL(dup3, oldfd, newfd, 0);
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

  return CALL_MODEL(dup2, oldfd, fd);
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

    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) {
      klee_warning("unallocated FD");
      return -1;
    }

    if (__fdt[fd].attr & FD_IS_CONCRETE) {
      klee_warning("unsupported concrete FD in fd_set (EBADF)");
      return -1;
    }
    //fprintf(stderr, "Watching for fd %d\n", fd);
    res++;
  }

  return res;
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

  if (INJECT_FAULT(select, ENOMEM)) {
    return -1;
  }

  if (timeout != NULL && totalfds == 0) {
    klee_warning("simulating timeout");
    // We just return timeout
    if (timeout->tv_sec != 0 || timeout->tv_usec != 0)
      klee_thread_preempt(1);

    return 0;
  }

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

  wlist_id_t wlist = 0;
  int res = 0;

  do {
    int fd;
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

    if (res > 0)
      break;

    // Nope, bad luck...

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
  } while (1);

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
      if (arg & (O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME)) {
        klee_warning("unsupported fcntl flags");
        errno = EINVAL;
        return -1;
      }
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
