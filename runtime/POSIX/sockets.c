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

      struct sockaddr_in *crt = (struct sockaddr_in*)__net.end_points[i].addr;

      if (crt->sin_port == port) {
        __net.end_points[i].refcount++;
        return &__net.end_points[i];
      }
    }
  }

  STATIC_LIST_ALLOC(__net.end_points, i);

  if (i == MAX_PORTS)
    return NULL;

  __net.end_points[i].addr = (struct sockaddr*)malloc(sizeof(struct sockaddr_in));
  klee_make_shared(__net.end_points[i].addr, sizeof(struct sockaddr_in));
  __net.end_points[i].refcount = 1;
  __net.end_points[i].socket = NULL;

  struct sockaddr_in *newaddr = (struct sockaddr_in*)__net.end_points[i].addr;

  newaddr->sin_family = AF_INET;
  newaddr->sin_addr = __net.net_addr;
  newaddr->sin_port = port;

  return &__net.end_points[i];
}

static end_point_t *__get_unix_end(const struct sockaddr_un *addr) {
  unsigned int i;
  if (addr) {
    // Search for an existing one...
    for (i = 0; i < MAX_UNIX_EPOINTS; i++) {
      if (!STATIC_LIST_CHECK(__unix_net.end_points, i))
        continue;

      struct sockaddr_un *crt = (struct sockaddr_un*)__unix_net.end_points[i].addr;

      if (!crt)
        continue;

      if (strcmp(addr->sun_path, crt->sun_path) == 0) {
        __unix_net.end_points[i].refcount++;
        return &__unix_net.end_points[i];
      }
    }
  }

  // Create a new end point
  STATIC_LIST_ALLOC(__unix_net.end_points, i);

  if (i == MAX_UNIX_EPOINTS)
    return NULL;

  __unix_net.end_points[i].addr = NULL;
  __unix_net.end_points[i].refcount = 1;
  __unix_net.end_points[i].socket = NULL;

  if (addr) {
    size_t addrsize = sizeof(addr->sun_family) + strlen(addr->sun_path) + 1;
    __unix_net.end_points[i].addr = (struct sockaddr*)malloc(addrsize);
    klee_make_shared(__unix_net.end_points[i].addr, addrsize);

    struct sockaddr_un *newaddr = (struct sockaddr_un*)__unix_net.end_points[i].addr;

    newaddr->sun_family = AF_UNIX;
    strcpy(newaddr->sun_path, addr->sun_path);
  }

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

int _is_blocking_socket(socket_t *sock, int event) {
  if (sock->status == SOCK_STATUS_CREATED)
    return 0;

  if (sock->status == SOCK_STATUS_CONNECTED) {
    switch (event) {
    case EVENT_READ:
      return sock->in && _stream_is_empty(sock->in) && !sock->in->closed;
    case EVENT_WRITE:
      return sock->out && _stream_is_full(sock->out) && !sock->out->closed;
    default:
      assert(0 && "invalid event");
    }
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    switch (event) {
    case EVENT_READ:
      assert (!sock->listen->closed && "invalid socket state");
      return _stream_is_empty(sock->listen);
    case EVENT_WRITE:
      return 0;
    default:
      assert(0 && "invalid event");
    }
  }

  // We should never reach here...
  assert(0 && "invalid socket state");
}

int _register_events_socket(socket_t *sock, wlist_id_t wlist, int events) {
  if (sock->status == SOCK_STATUS_CONNECTED) {
    if ((events & EVENT_READ) && _stream_register_event(sock->in, wlist) == -1)
      return -1;

    if ((events & EVENT_WRITE) && _stream_register_event(sock->out, wlist) == -1) {
      _stream_clear_event(sock->in, wlist);
      return -1;
    }
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    return _stream_register_event(sock->listen, wlist);
  }

  // We should never reach here
  assert(0 && "invalid socket state");
}

void _deregister_events_socket(socket_t *sock, wlist_id_t wlist, int events) {
  if (sock->status == SOCK_STATUS_CONNECTED) {
    if ((events & EVENT_READ) && sock->in)
      _stream_clear_event(sock->in, wlist);

    if ((events & EVENT_WRITE) && sock->out)
      _stream_clear_event(sock->out, wlist);
  }

  if (sock->status == SOCK_STATUS_LISTENING) {
    _stream_clear_event(sock->listen, wlist);
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
  klee_make_shared(sock, sizeof(socket_t));
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

  end_point_t *end_point;

  if (sock->domain == AF_INET) {
    end_point = __get_inet_end((struct sockaddr_in*)addr);
  } else if (sock->domain == AF_UNIX) {
    end_point = __get_unix_end((struct sockaddr_un*)addr);
  } else {
    assert(0 && "invalid socket");
  }

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

// get{sock,peer}name() ////////////////////////////////////////////////////////

static void _get_endpoint_name(socket_t *sock, end_point_t *end_point,
    struct sockaddr *addr, socklen_t *addrlen) {
  if (sock->domain == AF_INET) {
    socklen_t len = *addrlen;

    if (len > sizeof(struct sockaddr_in))
      len = sizeof(struct sockaddr_in);

    memcpy(addr, end_point->addr, len);
    *addrlen = sizeof(struct sockaddr_in);
  } else if (sock->domain == AF_UNIX) {
    socklen_t len = *addrlen;

    if (end_point->addr) {
      struct sockaddr_un *ep_addr = (struct sockaddr_un*)end_point->addr;
      socklen_t ep_len = sizeof(ep_addr->sun_family) + strlen(ep_addr->sun_path) + 1;

      if (len > ep_len)
        len = ep_len;

      memcpy(addr, ep_addr, len);
      *addrlen = ep_len;
    } else {
      struct sockaddr_un tmp;
      tmp.sun_family = AF_UNIX;

      if (len > sizeof(tmp.sun_family))
        len = sizeof(tmp.sun_family);

      memcpy(addr, &tmp, sizeof(tmp.sun_family));
      *addrlen = sizeof(tmp.sun_family);
    }
  } else {
    assert(0 && "invalid socket");
  }
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->local_end == NULL) {
    errno = EINVAL;
    return -1;
  }

  _get_endpoint_name(sock, sock->local_end, addr, addrlen);

  return 0;
}

int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  CHECK_IS_SOCKET(sockfd);

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  if (sock->remote_end == NULL) {
    errno = ENOTCONN;
    return -1;
  }

  _get_endpoint_name(sock, sock->remote_end, addr, addrlen);

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

  sock->listen = _stream_create(backlog*sizeof(socket_t*), 1);

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

static int _unix_stream_reach(socket_t *sock, const struct sockaddr *addr, socklen_t addrlen) {
  if (sock->local_end == NULL) {
    // We obtain an anonymous UNIX bind point
    sock->local_end = __get_unix_end(NULL);

    if (sock->local_end == NULL) {
      errno = EAGAIN;
      return -1;
    }
  }

  assert(sock->remote_end == NULL);

  if (addr->sa_family != AF_UNIX || addrlen < sizeof(addr->sa_family) + 1) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  sock->remote_end = __get_unix_end((struct sockaddr_un*)addr);

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

  // Let's obtain the end points
  if (sock->domain == AF_INET) {
    if (_inet_stream_reach(sock, addr, addrlen) == -1)
      return -1;
  } else if (sock->domain == AF_UNIX) {
    if (_unix_stream_reach(sock, addr, addrlen) == -1)
      return -1;
  } else {
    assert(0 && "invalid socket");
  }

  if (sock->remote_end->socket == NULL ||
      sock->remote_end->socket->status != SOCK_STATUS_LISTENING ||
      sock->remote_end->socket->domain != sock->domain) {

    __release_end_point(sock->remote_end);
    sock->remote_end = NULL;
    errno = ECONNREFUSED;
    return -1;
  }

  socket_t *remote = sock->remote_end->socket;

  if (_stream_is_full(remote->listen)) {
    __release_end_point(sock->remote_end);
    sock->remote_end = NULL;
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
    sock->remote_end = NULL;
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
      errno = EINVAL;
      return -1;
    }
  } else if (sock->domain == AF_UNIX){
    end_point = __get_unix_end(NULL);

    if (end_point == NULL) {
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
  klee_make_shared(local, sizeof(socket_t));
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
  local->in = _stream_create(SOCKET_BUFFER_SIZE, 1);
  remote->out = local->in;

  local->out = _stream_create(SOCKET_BUFFER_SIZE, 1);
  remote->in = local->out;

  // All is good for the remote socket
  remote->status = SOCK_STATUS_CONNECTED;
  _get_endpoint_name(local, local->remote_end, addr, addrlen);

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
  CHECK_IS_SOCKET(sockfd);

  if (flags != 0) {
    klee_warning("send() flags unsupported for now");
    errno = EINVAL;
    return -1;
  }

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  return _write_socket(sock, buf, len);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
  CHECK_IS_SOCKET(sockfd);

  if (flags != 0) {
    klee_warning("recv() flags unsupported for now");
    errno = EINVAL;
    return -1;
  }

  socket_t *sock = (socket_t*)__fdt[sockfd].io_object;

  return _read_socket(sock, buf, len);
}

// {get,set}sockopt() //////////////////////////////////////////////////////////

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
  CHECK_IS_SOCKET(sockfd);

  klee_warning("unsupported getsockopt()");

  errno = EINVAL;
  return -1;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
  CHECK_IS_SOCKET(sockfd);

  klee_warning("unsupported setsockopt()");

  errno = EINVAL;
  return -1;
}

// socketpair() ////////////////////////////////////////////////////////////////

int socketpair(int domain, int type, int protocol, int sv[2]) {
  // Check the parameters

  if (domain != AF_UNIX) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  if (type != SOCK_STREAM) {
    klee_warning("unsupported socketpair() type");
    errno = EINVAL;
    return -1;
  }

  if (protocol != 0) {
    klee_warning("unsupported socketpair() protocol");
    errno = EPROTONOSUPPORT;
    return -1;
  }

  // Get two file descriptors
  int fd1, fd2;

  STATIC_LIST_ALLOC(__fdt, fd1);
  if (fd1 == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  STATIC_LIST_ALLOC(__fdt, fd2);
  if (fd2 == MAX_FDS) {
    STATIC_LIST_CLEAR(__fdt, fd1);

    errno = ENFILE;
    return -1;
  }

  // Get two anonymous UNIX end points
  end_point_t *ep1, *ep2;

  ep1 = __get_unix_end(NULL);
  ep2 = __get_unix_end(NULL);

  if (!ep1 || !ep2) {
    STATIC_LIST_CLEAR(__fdt, fd1);
    STATIC_LIST_CLEAR(__fdt, fd2);

    if (ep1)
      __release_end_point(ep1);

    if (ep2)
      __release_end_point(ep2);

    errno = ENOMEM;
    return -1;
  }

  // Create the first socket object
  socket_t *sock1 = (socket_t*)malloc(sizeof(socket_t));
  klee_make_shared(sock1, sizeof(socket_t));
  memset(sock1, 0, sizeof(socket_t));

  __fdt[fd1].attr |= FD_IS_SOCKET;
  __fdt[fd1].io_object = (file_base_t*)sock1;

  sock1->__bdata.flags = O_RDWR;
  sock1->__bdata.refcount = 1;
  sock1->__bdata.queued = 0;

  sock1->domain = domain;
  sock1->type = type;
  sock1->status = SOCK_STATUS_CONNECTED;

  sock1->local_end = ep1;
  ep1->socket = sock1;

  sock1->remote_end = ep2;

  sock1->in = _stream_create(SOCKET_BUFFER_SIZE, 1);
  sock1->out = _stream_create(SOCKET_BUFFER_SIZE, 1);

  // Create the second socket object
  socket_t *sock2 = (socket_t*)malloc(sizeof(socket_t));
  klee_make_shared(sock2, sizeof(socket_t));
  memset(sock2, 0, sizeof(socket_t));

  __fdt[fd2].attr |= FD_IS_SOCKET;
  __fdt[fd2].io_object = (file_base_t*)sock2;

  sock2->__bdata.flags = O_RDWR;
  sock2->__bdata.refcount = 1;
  sock2->__bdata.queued = 0;

  sock2->domain = domain;
  sock2->type = type;
  sock2->status = SOCK_STATUS_CONNECTED;

  sock2->local_end = ep2;
  ep2->socket = sock2;

  sock2->remote_end = ep1;

  sock2->in = sock1->out;
  sock2->out = sock1->in;

  // Finalize
  sv[0] = fd1; sv[1] = fd2;

  return 0;
}
