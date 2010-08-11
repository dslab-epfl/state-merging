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

      if (addr->sin_port == port)
        return &__net.end_points[i];
    }
  }

  STATIC_LIST_ALLOC(__net.end_points, i);

  if (i == MAX_PORTS)
    return NULL;

  __net.end_points[i].addr = (struct sockaddr*)malloc(sizeof(struct sockaddr_in));

  struct sockaddr_in *newaddr = (struct sockaddr_in*)__net.end_points[i].addr;

  newaddr->sin_family = AF_INET;
  newaddr->sin_addr = __net.net_addr;
  newaddr->sin_port = port;

  __net.end_points[i].socket = NULL;

  return &__net.end_points[i];
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
  sock->__bdata.flags = O_RDWR;
  sock->__bdata.refcount = 1;

  sock->status = SOCK_STATUS_CREATED;
  sock->type = type;
  sock->domain = domain;

  fde->io_object = (file_base_t*)sock;

  return fd;
}

////////////////////////////////////////////////////////////////////////////////

static int _bind_inet(socket_t *sock, struct sockaddr_in *addr) {
  end_point_t* end_point = __get_inet_end(addr);

  if (end_point == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (end_point->socket != NULL) {
    errno = EADDRINUSE;
    return -1;
  }

  sock->status = SOCK_STATUS_BOUND;
  sock->local_end = end_point;

  return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->status & SOCK_STATUS_BOUND) {
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
    return -1;
  }
}

////////////////////////////////////////////////////////////////////////////////

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (!(sock->status & SOCK_STATUS_BOUND)) {
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
    return -1;
  }
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (!(sock->status & SOCK_STATUS_CONNECTED)) {
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
    return -1;
  }
}
