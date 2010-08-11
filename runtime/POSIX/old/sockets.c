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

#define PORT(addr) ( \
    ((struct sockaddr *)(addr))->sa_family == PF_INET  ? ((struct sockaddr_in  *)(addr))->sin_port : \
    ((struct sockaddr *)(addr))->sa_family == PF_INET6 ? ((struct sockaddr_in6 *)(addr))->sin6_port : \
    (assert(0 && "unsupported domain"), 0))

/* checks only the port numbers */
#define HAS_ADDR(a) (assert((a)->addr), PORT((a)->addr) != 0)

#define ADDR_COMPATIBLE(addr, addrlen, a) \
    ((addrlen) == (a)->addrlen && (addr)->sa_family == (a)->addr->ss_family)


/* Just to set dfile as a non-NULL value for symbolic datagram sockets */
static exe_disk_file_t dummy_dfile = {0, NULL, NULL, NULL, NULL};


static exe_disk_file_t *__get_sym_stream() {
  if (__exe_fs.n_sym_streams_used >= __exe_fs.n_sym_streams)
    return NULL;
  return &__exe_fs.sym_streams[__exe_fs.n_sym_streams_used++];
}


static exe_disk_file_t *__get_sym_dgram() {
  if (__exe_fs.n_sym_dgrams_used >= __exe_fs.n_sym_dgrams)
    return NULL;
  return &__exe_fs.sym_dgrams[__exe_fs.n_sym_dgrams_used++];
}


int __fd_connect(unsigned long *args) {
  int os_r;
  int sockfd = args[0];
  const struct sockaddr *addr = (const struct sockaddr *) args[1];
  socklen_t addrlen = args[2];

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
    assert(f->foreign->addr);
    if (!(f->flags & eDgramSocket)) {
      assert(HAS_ADDR(f->foreign));
      errno = EISCONN;  /* Transport endpoint is already connected */
      return -1;
    }
    if (!ADDR_COMPATIBLE(addr, addrlen, f->foreign)) {
      errno = EINVAL;   /* The addrlen is wrong */
      return -1;
    }
    if (PORT(addr) == 0) {
      errno = EADDRNOTAVAIL;  /* Cannot assign requested address */
    }
    if (PORT(f->local.addr) == 0) {
      /* TODO: choose an ephemeral port */
    }
    /* assign the address to the socket */
    memcpy(f->foreign->addr, addr, addrlen);
  }

  else {
    /* concrete file descriptor */
    args[0] = f->fd;
    os_r = syscall(__NR_socket, SYS_CONNECT, args);
    args[0] = sockfd;
    if (os_r < 0) {
      errno = klee_get_errno();
      return -1;
    }
  }

  return 0;
}


int __fd_listen(unsigned long *args) {
  int os_r;
  int sockfd = args[0];
  /*int backlog = args[1];*/

  exe_file_t *f = __get_file(sockfd);

  if (!f) {
    errno = EBADF;      /* Bad file number */
    return -1;
  }

  if (!(f->flags & eSocket)) {
    errno = ENOTSOCK;   /* Socket operation on non-socket */
    return -1;
  }

  if (f->flags & eDgramSocket) {
    errno = EOPNOTSUPP; /* Operation not supported on transport endpoint */
    return -1;
  }

  if (f->dfile) {
    /* assume success for symbolic socket */
    os_r = 0;
  }
  else {
    args[0] = f->fd;
    os_r = 0 /* syscall(__NR_socketcall, SYS_LISTEN, args) */;
    args[0] = sockfd;
    if (os_r < 0) {
      errno = klee_get_errno();
      return -1;
    }
  }

  f->flags |= eListening;

  return os_r;
}


int __fd_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  int connfd;
  exe_file_t *connf;
  exe_file_t *f = __get_file(sockfd);

  if (!f) {
    errno = EBADF;      /* Bad file number */
    return -1;
  }

  if (!(f->flags & eSocket)) {
    errno = ENOTSOCK;   /* Socket operation on non-socket */
    return -1;
  }

  if (f->flags & eDgramSocket) {
    errno = EOPNOTSUPP; /* Operation not supported on transport endpoint */
    return -1;
  }

  if (!(f->flags & eListening)) {
    errno = EINVAL;     /* socket is not listening for connections */
    return -1;
  }

  if (0 /* queue empty */) {
    if (0 /* non-blocking socket */) {
      errno = EAGAIN;
      return -1;
    }
    /* wait for a pending connection */
  }

  /* (pretend to) get a pending connection */
  connfd = __get_new_fd(&connf);
  if (connfd < 0) return connfd;
  connf->flags |= eSocket;

  connf->dfile = __get_sym_stream();
  if (!connf->dfile) {
    __undo_get_new_fd(connf);
    errno = ENFILE;
    return -1;
  }

  /* Allocate the local address */
  if (!__allocate_sockaddr(f->domain, &connf->local)) {
    --__exe_fs.n_sym_streams_used;  /* undo __get_sym_stream() */
    __undo_get_new_fd(connf);
    errno = ENOMEM;
    return -1;
  }
  /* TODO: Fill the local address */

  /* Adjust the source address to the local address */
  connf->dfile->src->addrlen         = connf->local.addrlen;
  connf->dfile->src->addr->ss_family = connf->local.addr->ss_family;

  /* For TCP, foreign simply points to dfile->src */
  connf->foreign = connf->dfile->src;

  /* Fill the peer socket address */
  if (addr) {
    klee_check_memory_access(addr, *addrlen);
    if (*addrlen < connf->foreign->addrlen) {
      free(connf->foreign->addr);
      free(connf->local.addr);
      --__exe_fs.n_sym_streams_used;  /* undo __get_sym_stream() */
      __undo_get_new_fd(connf);
      errno = EINVAL;
      return -1;
    }
    memcpy(addr, connf->foreign->addr, connf->foreign->addrlen);
    *addrlen = connf->foreign->addrlen; /* man page: when addr is NULL, nothing is filled in */
  }  

  connf->flags |= eReadable | eWriteable;
  /* XXX Should check access against mode / stat / possible deletion. */

  return connfd;
}

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

