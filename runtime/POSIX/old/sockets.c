#include "sockets.h"
#include "fd.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>  /* __NR_* constants */
#include <linux/net.h>    /* SYS_* constants */
#include <klee/klee.h>

void klee_warning(const char*);
void klee_warning_once(const char*);
int klee_get_errno(void);

int __fd_shutdown(unsigned long *args) {
  int sockfd = args[0];
  int how = args[1];
  exe_file_t *f = __get_file(sockfd);

  if (!f) {
    errno = EBADF;      /* Bad file number */
    return -1;
  }

  if (!(f->flags & eSocket)) {
    errno = ENOTSOCK;   /* Socket operation on non-socket */
    return -1;
  }

  if (f->dfile) {
    /* symbolic file descriptor */

    switch (how) {
    case SHUT_RD:
      f->flags &= ~eReadable; break;
    case SHUT_WR:
      f->flags &= ~eWriteable; break;
    case SHUT_RDWR:
      f->flags &= ~(eReadable | eWriteable); break;
    default:
      errno = EINVAL;
      return -1;
    }
  }
  else {
    /* concrete file descriptor */
    int os_r;

    args[0] = f->fd;
    os_r = syscall(__NR_socket, SYS_SHUTDOWN, args);
    args[0] = sockfd;
    if (os_r == -1) {
      errno = klee_get_errno();
      return -1;
    }
  }

  return 0;
}


ssize_t __fd_send(int fd, const void *buf, size_t len, int flags)
{
  return __fd_sendto(fd, buf, len, flags, NULL, 0);
}


ssize_t __fd_recv(int fd, void *buf, size_t len, int flags)
{
  return __fd_recvfrom(fd, buf, len, flags, NULL, NULL);
}


ssize_t __fd_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
  struct iovec iov;
  struct msghdr msg;

  iov.iov_base = (void *) buf;            /* const_cast */
  iov.iov_len = len;

  msg.msg_name = (struct sockaddr *) to;  /* const_cast */
  msg.msg_namelen = tolen;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = flags;

  return __fd_sendmsg(fd, &msg, flags);
}


ssize_t __fd_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
  struct iovec iov;
  struct msghdr msg;
  ssize_t s;

  if (from != NULL && fromlen == NULL) {
    errno = EFAULT;
    return -1;
  }

  iov.iov_base = buf;
  iov.iov_len = len;

  msg.msg_name = from;
  msg.msg_namelen = fromlen ? *fromlen : /* In this case, from should be NULL */ 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = flags;

  s = __fd_recvmsg(fd, &msg, flags);

  if (fromlen) *fromlen = msg.msg_namelen;
  return s;
}


ssize_t __fd_sendmsg(int fd, struct msghdr *msg, int flags)
{
  exe_file_t *f = __get_file(fd);
  if (!f) {
    errno = EBADF;      /* Bad file number */
    return -1;
  }

  if (!(f->flags & eSocket)) {
    errno = ENOTSOCK;   /* Socket operation on non-socket */
    return -1;
  }

  if (msg == NULL) {
    errno = EFAULT;     /* Bad address */
    return -1;
  }

  if (!f->dfile) {
    /* concrete socket */
    assert(0 && "not supported yet");
  }

  else {
    /* symbolic socket */
    if (!(f->flags & eDgramSocket)) {
      assert(f->foreign->addr);
      if (!HAS_ADDR(f->foreign)) {
        errno = ENOTCONN; /* Transport endpoint is not connected */
        return -1;
      }
    }
    else {
      if (!HAS_ADDR(f->foreign) && msg->msg_name == NULL) {
        errno = ENOTCONN; /* Transport endpoint is not connected */
        return -1;
      }

      if (HAS_ADDR(f->foreign) && msg->msg_name != NULL) {
        errno = EISCONN;  /* Transport endpoint is already connected */
        return -1;
      }
    }

    if (msg->msg_name != NULL) {
      klee_check_memory_access(msg->msg_name, msg->msg_namelen);
      /* ignore the destination */
    }

    if (flags != 0)
      klee_warning("flags is not zero, ignoring");

    return __fd_gather_write(f, msg->msg_iov, msg->msg_iovlen);
  }
}


ssize_t __fd_recvmsg(int fd, struct msghdr *msg, int flags)
{
  exe_file_t *f = __get_file(fd);
  if (!f) {
    errno = EBADF;      /* Bad file number */
    return -1;
  }

  if (!(f->flags & eSocket)) {
    errno = ENOTSOCK;   /* Socket operation on non-socket */
    return -1;
  }

  if (msg == NULL) {
    errno = EFAULT;     /* Bad address */
    return -1;
  }

  if (!f->dfile) {
    /* concrete socket */
    assert(0 && "not supported yet");
  }

  else {
    /* symbolic socket */
    if (!(f->flags & eDgramSocket)) {
      if (!HAS_ADDR(f->foreign)) {
        errno = ENOTCONN; /* Transport endpoint is not connected */
        return -1;
      }
    }
    else {
      /* assign a new symbolic datagram source for datagram sockets */
      f->off = 0;
      f->dfile = __get_sym_dgram();
      if (!f->dfile) {
        /* no more datagrams; won't happen in the real world */
        f->dfile = &dummy_dfile;
        errno = ENFILE;
        return -1;
      }
    }

    if (msg->msg_name != NULL) {
      klee_check_memory_access(msg->msg_name, msg->msg_namelen);
      memcpy(msg->msg_name, f->dfile->src->addr, f->dfile->src->addrlen);
    }
    msg->msg_namelen = f->dfile->src->addrlen;

    if (flags != 0)
      klee_warning("flags is not zero, ignoring");

    return __fd_scatter_read(f, msg->msg_iov, msg->msg_iovlen);
  }
}

