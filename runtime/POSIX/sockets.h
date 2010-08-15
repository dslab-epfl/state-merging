/*
 * sockets.h
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#ifndef SOCKETS_H_
#define SOCKETS_H_

#include "fd.h"
#include "buffers.h"
#include "multiprocess.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define DEFAULT_UNUSED_PORT     32768
#define DEFAULT_NETWORK_ADDR    ((((((192 << 8) | 168) << 8) | 1) << 8) | 1)

#define SOCK_STATUS_CREATED     (1 << 0)
#define SOCK_STATUS_LISTENING   (1 << 1)
#define SOCK_STATUS_CONNECTED   (1 << 2)
#define SOCK_STATUS_CLOSED      (1 << 3) // Transient state due to concurrency

struct socket;

typedef struct {
  struct sockaddr *addr;

  struct socket *socket;

  unsigned int refcount;
  char allocated;
}  end_point_t;

typedef struct {
  // For TCP/IP sockets
  struct in_addr net_addr;  // The IP address of the virtual machine

  in_port_t next_port;

  end_point_t end_points[MAX_PORTS];
} network_t;

typedef struct {
  end_point_t end_points[MAX_UNIX_EPOINTS];
} unix_t;

extern network_t __net;
extern unix_t __unix_net;


typedef struct socket {
  file_base_t __bdata;

  int status;
  int type;
  int domain;

  end_point_t *local_end;
  end_point_t *remote_end;

  // For TCP connections
  stream_buffer_t *out;     // The output buffer
  stream_buffer_t *in;      // The input buffer
  wlist_id_t wlist;         // The waiting list for the connected notif.

  // For TCP listening
  stream_buffer_t *listen;
} socket_t;

void _close_socket(socket_t *sock);
ssize_t _read_socket(socket_t *sock, void *buf, size_t count);
ssize_t _write_socket(socket_t *sock, const void *buf, size_t count);
int _stat_socket(socket_t *sock, struct stat *buf);

int _is_blocking_socket(socket_t *sock, int event);
int _register_events_socket(socket_t *sock, wlist_id_t wlist, int events);
void _deregister_events_socket(socket_t *sock, wlist_id_t wlist, int events);


#endif /* SOCKETS_H_ */
