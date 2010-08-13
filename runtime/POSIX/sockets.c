/*
 * sockets.c
 *
 *  Created on: Aug 11, 2010
 *      Author: stefan
 */

#include "sockets.h"

#include "fd.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <assert.h>
#include <stdlib.h>

#include <klee/klee.h>

#define CHECK_IS_SOCKET(fd) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, (unsigned)fd)) { \
    errno = EBADF; \
    return -1; \
    } \
    if (!(__fdt[fd].attr & FD_IS_SOCKET)) { \
      errno = ENOTSOCK; \
      return -1; \
    } \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
// Internal Routines
////////////////////////////////////////////////////////////////////////////////

static in_port_t __get_unused_port(void) {
  unsigned int i;
  char found = 0;

  while (!found) {
    for (i = 0; i < MAX_PORTS; i++) {
      if (!STATIC_LIST_CHECK(__net.end_points, i))
        continue;

      struct sockaddr_in *addr = (struct sockaddr_in*)__net.end_points[i].addr;

      if (__net.next_port == addr->sin_port) {
        __net.next_port = htons(ntohs(__net.next_port) + 1);
        break;
      }
    }

    found = 1;
  }

  in_port_t res = __net.next_port;

  __net.next_port = htons(ntohs(__net.next_port) + 1);

  return res;
}

static end_point_t *__get_inet_end(const struct sockaddr_in *addr) {
  if (addr->sin_addr.s_addr != INADDR_ANY && addr->sin_addr.s_addr != INADDR_LOOPBACK &&
      addr->sin_addr.s_addr != __net.net_addr.s_addr)
    return NULL;

  unsigned int i;
  in_port_t port = addr->sin_port;

  if (port == 0) {
    port = __get_unused_port();
  } else {
    for (i = 0; i < MAX_PORTS; i++) {
      if (!STATIC_LIST_CHECK(__net.end_points, i))
        continue;

      struct sockaddr_in *addr = (struct sockaddr_in*)__net.end_points[i].addr;

      if (addr->sin_port == port) {
        __net.end_points[i].refcount++;
        return &__net.end_points[i];
      }
    }
  }

  STATIC_LIST_ALLOC(__net.end_points, i);

  if (i == MAX_PORTS)
    return NULL;

  __net.end_points[i].addr = (struct sockaddr*)malloc(sizeof(struct sockaddr_in));
  __net.end_points[i].refcount = 1;
  __net.end_points[i].socket = NULL;

  struct sockaddr_in *newaddr = (struct sockaddr_in*)__net.end_points[i].addr;

  newaddr->sin_family = AF_INET;
  newaddr->sin_addr = __net.net_addr;
  newaddr->sin_port = port;

  return &__net.end_points[i];
}

static end_point_t *__get_unix_end(const struct sockaddr_un *addr) {

}

static void __release_end_point(end_point_t *end_point) {
  assert(end_point->refcount > 0);
  end_point->refcount--;

  if (end_point->refcount == 0) {
    free(end_point->addr);
    memset(end_point, 0, sizeof(end_point_t));
  }
}

// close() /////////////////////////////////////////////////////////////////////

static void _shutdown(socket_t *sock, int how);

void _close_socket(socket_t *sock) {
  if (sock->status == SOCK_STATUS_CONNECTED) {
    _shutdown(sock, SHUT_RDWR);
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    while (!_stream_is_empty(sock->listen)) {
      socket_t *remote;
      ssize_t res = _stream_read(sock->listen, (char*)&remote, sizeof(socket_t*));
      assert(res == sizeof(socket_t*));

      remote->__bdata.queued--;

      if (remote->status == SOCK_STATUS_CLOSED) {
        if (remote->__bdata.queued == 0)
          free(remote);

      } else {
        klee_thread_notify_all(remote->wlist);
      }
    }

    _stream_destroy(sock->listen);
  }

  if (sock->wlist) {
    klee_thread_notify_all(sock->wlist);
  }

  if (sock->local_end) {
    sock->local_end->socket = NULL;
    __release_end_point(sock->local_end);
    sock->local_end = NULL;
  }

  if (sock->remote_end) {
    sock->remote_end->socket = NULL;
    __release_end_point(sock->remote_end);
    sock->remote_end = NULL;
  }

  sock->status = SOCK_STATUS_CLOSED;

  if (sock->__bdata.queued == 0) {
    free(sock);
  }
}

// read() //////////////////////////////////////////////////////////////////////

ssize_t _read_socket(socket_t *sock, void *buf, size_t count) {
  if (sock->status != SOCK_STATUS_CONNECTED) {
    errno = EINVAL;
    return -1;
  }

  if (sock->in == NULL || count == 0) {
    // The socket is shut down for reading
    return 0;
  }

  sock->__bdata.queued++;
  ssize_t res = _stream_read(sock->in, buf, count);
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSED) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  if (res == -1) {
    // The stream was shut down in the mean time
    errno = EINVAL;
  }

  return res;
}

// write() /////////////////////////////////////////////////////////////////////

ssize_t _write_socket(socket_t *sock, const void *buf, size_t count) {
  if (sock->status != SOCK_STATUS_CONNECTED) {
    errno = EINVAL;
    return -1;
  }

  if (sock->out == NULL || sock->out->closed) {
    // The socket is shut down for writing
    errno = EPIPE;
    return -1;
  }

  if (count == 0) {
    return 0;
  }

  sock->__bdata.queued++;
  ssize_t res = _stream_write(sock->out, buf, count);
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSED) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  if (res == -1) {
    // The stream was shut down in the mean time
    errno = EINVAL;
  }

  return res;
}

// fstat() /////////////////////////////////////////////////////////////////////

int _stat_socket(socket_t *sock, struct stat *buf) {
  assert(0 && "not implemented");
}

////////////////////////////////////////////////////////////////////////////////
// The Sockets API
////////////////////////////////////////////////////////////////////////////////

int socket(int domain, int type, int protocol) {
  // Check the validity of the request
  switch (domain) {
  case AF_INET:
  case AF_UNIX:
    switch (type) {
    case SOCK_STREAM:
    //case SOCK_DGRAM:
      if (protocol != 0) {
        klee_warning("unsupported protocol");
        errno = EPROTONOSUPPORT;
        return -1;
      }
      break;
    default:
      klee_warning("unsupported socket type");
      errno = EPROTONOSUPPORT;
      return -1;
    }
    break;
  default:
    klee_warning("unsupported socket domain");
    errno = EINVAL;
    return -1;
  }

  // Now let's obtain a new file descriptor
  int fd;
  STATIC_LIST_ALLOC(__fdt, fd);

  if (fd == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];
  fde->attr |= FD_IS_SOCKET;

  // Create the socket object
  socket_t *sock = (socket_t*)malloc(sizeof(socket_t));
  memset(sock, 0, sizeof(socket_t));

  sock->__bdata.flags = O_RDWR;
  sock->__bdata.refcount = 1;
  sock->__bdata.queued = 0;

  sock->status = SOCK_STATUS_CREATED;
  sock->type = type;
  sock->domain = domain;

  fde->io_object = (file_base_t*)sock;

  return fd;
}

// bind() //////////////////////////////////////////////////////////////////////

static int _bind_inet(socket_t *sock, struct sockaddr_in *addr) {
  end_point_t* end_point = __get_inet_end(addr);

  if (end_point == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (end_point->socket != NULL) {
    __release_end_point(end_point);
    errno = EADDRINUSE;
    return -1;
  }

  sock->local_end = end_point;
  end_point->socket = sock;

  return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->local_end != NULL) {
    errno = EINVAL;
    return -1;
  }

  if (addr->sa_family != sock->domain) {
    errno = EINVAL;
    return -1;
  }

  if (sock->domain == AF_INET) {
    return _bind_inet(sock, (struct sockaddr_in*)addr);
  } else {
    assert(0 && "invalid socket");
  }
}

// get{sock,peer}name() ////////////////////////////////////////////////////////

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->local_end == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (sock->domain == AF_INET) {
    socklen_t len = *addrlen;

    if (len > sizeof(struct sockaddr_in))
      len = sizeof(struct sockaddr_in);

    memcpy(addr, sock->local_end->addr, len);
    *addrlen = sizeof(struct sockaddr_in);

    return 0;
  } else {
    assert(0 && "invalid socket");
  }
}

static void _getpeername(socket_t *sock, struct sockaddr *addr, socklen_t *addrlen) {
  if (sock->domain == AF_INET) {
    socklen_t len = *addrlen;

    if (len > sizeof(struct sockaddr_in))
      len = sizeof(struct sockaddr_in);

    memcpy(addr, sock->remote_end->addr, len);
    *addrlen = sizeof(struct sockaddr_in);
  } else {
    assert(0 && "invalid socket");
  }
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->remote_end == NULL) {
    errno = ENOTCONN;
    return -1;
  }

  _getpeername(sock, addr, addrlen);

  return 0;
}

// listen() ////////////////////////////////////////////////////////////////////

int listen(int sockfd, int backlog) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->status != SOCK_STATUS_CREATED || sock->local_end == NULL) {
    errno = EOPNOTSUPP;
    return -1;
  }

  if (sock->type != SOCK_STREAM) {
    errno = EOPNOTSUPP;
    return -1;
  }

  // Create the listening queue
  sock->status = SOCK_STATUS_LISTENING;

  sock->listen = _stream_create(backlog*sizeof(socket_t*));

  return 0;
}

// connect() ///////////////////////////////////////////////////////////////////

static int _inet_stream_reach(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen) {
  if (sock->local_end == NULL) {
    // We obtain a local bind point
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr = __net.net_addr;
    local_addr.sin_port = 0; // Get an unused port

    sock->local_end = __get_inet_end(&local_addr);

    if (sock->local_end == NULL) {
      errno = EAGAIN;
      return -1;
    }
  }

  assert(sock->remote_end == NULL);

  if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  sock->remote_end = __get_inet_end((struct sockaddr_in*)addr);
  if (!sock->remote_end) {
    errno = EINVAL;
    return -1;
  }

  return 0;
}

static int _stream_connect(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen) {
  if (sock->status != SOCK_STATUS_CREATED) {
    if (sock->status == SOCK_STATUS_CONNECTED) {
      errno = EISCONN;
      return -1;
    } else {
      errno = EINVAL;
      return -1;
    }
  }

  if (sock->domain == AF_INET) {
    // Let's obtain the end points
    if (_inet_stream_reach(sock, addr, addrlen) == -1)
      return -1;
  } else {
    assert(0 && "invalid socket");
  }

  if (sock->remote_end->socket == NULL ||
      sock->remote_end->socket->status != SOCK_STATUS_LISTENING ||
      sock->remote_end->socket->domain != sock->domain) {

    __release_end_point(sock->remote_end);
    errno = ECONNREFUSED;
    return -1;
  }

  socket_t *remote = sock->remote_end->socket;

  if (_stream_is_full(remote->listen)) {
    __release_end_point(sock->remote_end);
    errno = ECONNREFUSED;
    return -1;
  }

  // We queue ourselves in the list...
  if (!sock->wlist)
    sock->wlist = klee_get_wlist();

  ssize_t res = _stream_write(remote->listen, (char*)&sock, sizeof(socket_t*));
  assert(res == sizeof(socket_t*));

  // ... and we wait for a notification
  sock->__bdata.queued += 2;
  klee_thread_sleep(sock->wlist);
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSED) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  sock->wlist = 0;

  if (sock->status != SOCK_STATUS_CONNECTED) {
    __release_end_point(sock->remote_end);
    errno = ECONNREFUSED;
    return -1;
  }

  return 0;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->type == SOCK_STREAM) {
    return _stream_connect(sock, addr, addrlen);
  } else if (sock->type == SOCK_DGRAM) {
    klee_warning("datagrams not yet supported in connect");
    errno = EINVAL;
    return -1;
  } else {
    assert(0 && "invalid socket");
  }
}

// accept() ////////////////////////////////////////////////////////////////////

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->type != SOCK_STREAM) {
    errno = EOPNOTSUPP;
    return -1;
  }

  if (sock->status != SOCK_STATUS_LISTENING) {
    errno = EINVAL;
    return -1;
  }

  socket_t *remote, *local;
  end_point_t *end_point;

  sock->__bdata.queued++;
  ssize_t res = _stream_read(sock->listen, (char*)&remote, sizeof(socket_t*));
  sock->__bdata.queued--;

  if (sock->status == SOCK_STATUS_CLOSED) {
    if (sock->__bdata.queued == 0)
      free(sock);

    errno = EINVAL;
    return -1;
  }

  assert(res == sizeof(socket_t*));

  remote->__bdata.queued--;

  if (remote->status == SOCK_STATUS_CLOSED) {
    if (remote->__bdata.queued == 0) {
      free(remote);
    }

    errno = ECONNABORTED;
    return -1;
  }

  klee_thread_notify_all(remote->wlist);

  // Get a new local port for the connection
  if (sock->domain == AF_INET) {
    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr = __net.net_addr;
    local_addr.sin_port = 0; // Find an unused port

    end_point = __get_inet_end(&local_addr);

    if (end_point == NULL) {
      __release_end_point(end_point);
      errno = EINVAL;
      return -1;
    }
  } else {
    assert(0 && "invalid socket");
  }

  // Get a new file descriptor for the new socket
  int fd;
  STATIC_LIST_ALLOC(__fdt, fd);
  if (fd == MAX_FDS) {
    __release_end_point(end_point);
    errno = ENFILE;
    return -1;
  }

  // Setup socket attributes
  local = (socket_t*)malloc(sizeof(socket_t));
  memset(local, 0, sizeof(socket_t));

  __fdt[fd].attr |= FD_IS_SOCKET;
  __fdt[fd].io_object = (file_base_t*)local;

  local->__bdata.flags |= O_RDWR;
  local->__bdata.refcount = 1;
  local->__bdata.queued = 0;

  local->status = SOCK_STATUS_CONNECTED;
  local->domain = remote->domain;
  local->type = SOCK_STREAM;

  // Setup end points
  local->local_end = end_point;
  end_point->socket = local;
  remote->remote_end = end_point;

  __release_end_point(local->remote_end);
  local->remote_end = remote->local_end;
  remote->local_end->refcount++;

  // Setup streams
  local->in = _stream_create(SOCKET_BUFFER_SIZE);
  remote->out = local->in;

  local->out = _stream_create(SOCKET_BUFFER_SIZE);
  remote->in = local->out;

  // All is good for the remote socket
  remote->status = SOCK_STATUS_CONNECTED;
  _getpeername(local, addr, addrlen);

  // Now return in our process
  return fd;
}

// shutdown() //////////////////////////////////////////////////////////////////

static void _shutdown(socket_t *sock, int how) {
  if ((how == SHUT_RD || how == SHUT_RDWR) && sock->in != NULL) {
    // Shutting down the reading part...
    if (sock->in->closed) {
      _stream_destroy(sock->in);
    } else {
      _stream_close(sock->in);
    }

    sock->in = NULL;
  }

  if ((how == SHUT_WR || how == SHUT_RDWR) && sock->out != NULL) {
    // Shutting down the writing part...
    if (sock->out->closed) {
      _stream_destroy(sock->out);
    } else {
      _stream_close(sock->out);
    }

    sock->out = NULL;
  }
}

int shutdown(int sockfd, int how) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->status != SOCK_STATUS_CONNECTED) {
    errno = ENOTCONN;
    return -1;
  }

  _shutdown(sock, how);

  return 0;
}

// Socket specific I/O /////////////////////////////////////////////////////////

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
  if (flags != 0) {
    klee_warning("send() flags unsupported for now");
    errno = EINVAL;
    return -1;
  }

  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  return _write_socket(sock, buf, len);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
  if (flags != 0) {
    klee_warning("recv() flags unsupported for now");
    errno = EINVAL;
    return -1;
  }

  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  return _read_socket(sock, buf, len);
}
