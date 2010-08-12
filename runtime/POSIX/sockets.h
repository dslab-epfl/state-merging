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

#define SOCK_STATUS_CREATED     (1 << 0)
#define SOCK_STATUS_LISTENING   (1 << 1)
#define SOCK_STATUS_CONNECTED   (1 << 2)
#define SOCK_STATUS_CLOSED      (1 << 3) // Transient state due to concurrency

struct socket;

typedef struct {
  struct sockaddr *addr;

  struct socket *socket;

  char allocated;
}  end_point_t;

typedef struct {
  // For TCP/IP sockets
  struct in_addr net_addr;  // The IP address of the virtual machine

  in_port_t next_port;

  end_point_t end_points[MAX_PORTS];
} network_t;

extern network_t __net;


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


#endif /* SOCKETS_H_ */
