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

static void __release_end_point(end_point_t *end_point) {
  assert(end_point->refcount > 0);
  end_point->refcount--;

  if (end_point->refcount == 0) {
    free(end_point->addr);
    memset(end_point, 0, sizeof(end_point_t));
  }
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

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->remote_end == NULL) {
    errno = ENOTCONN;
    return -1;
  }

  if (sock->domain == AF_INET) {
    socklen_t len = *addrlen;

    if (len > sizeof(struct sockaddr_in))
      len = sizeof(struct sockaddr_in);

    memcpy(addr, sock->remote_end->addr, len);
    *addrlen = sizeof(struct sockaddr_in);

    return 0;
  } else {
    assert(0 && "invalid socket");
  }
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

  sock->listen = (stream_buffer_t*)malloc(sizeof(stream_buffer_t));
  _stream_init(sock->listen, backlog*sizeof(socket_t*));

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
  klee_thread_sleep(sock->wlist);

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
  ssize_t res = _stream_read(sock->listen, (char*)&remote, sizeof(socket_t*));
  assert(res == sizeof(socket_t*));

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
  __fdt[fd].attr |= FD_IS_SOCKET;
  __fdt[fd].io_object = (file_base_t*)local;

  local->__bdata.flags |= O_RDWR;
  local->__bdata.refcount = 1;

  local->status = SOCK_STATUS_CONNECTED;
  local->domain = remote->domain;
  local->type = SOCK_STREAM;

  // Setup end points
  local->local_end = end_point;
  end_point->socket = local;
  remote->remote_end = end_point;

  local->remote_end = remote->local_end;

  // Setup streams
  local->in = (stream_buffer_t*)malloc(sizeof(stream_buffer_t));
  _stream_init(local->in, SOCKET_BUFFER_SIZE);
  remote->out = local->in;

  local->out = (stream_buffer_t*)malloc(sizeof(stream_buffer_t));
  _stream_init(local->out, SOCKET_BUFFER_SIZE);
  remote->in = local->out;

  // All is good for the remote socket
  remote->status = SOCK_STATUS_CONNECTED;

  // Now return in our process
  return fd;
}
