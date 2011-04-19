/*
 * udp_send_recv_the_same_socket.c
 *
 *  Created on: Nov 25, 2010
 *      Author: art_haali
 */

#include "udp_test_helpers.h"
#include "assert_fail.h"
#include <assert.h>

#define BUFLEN 512


int main(int argc, char **argv) {
  int socketFd;
  assert_return(socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

  struct sockaddr_in addr_me;
  setupAddress(&addr_me, 7779);
  assert_return(bind(socketFd, (struct sockaddr*) &addr_me, sizeof(addr_me)));
  assert_return(connect(socketFd, (struct sockaddr*) &addr_me, sizeof addr_me));

  char buf[BUFLEN];
  assert (send(socketFd, buf, BUFLEN, 0) == BUFLEN);
  assert (recv(socketFd, buf, BUFLEN, 0) == BUFLEN); //todo: ah: use MSG_DONTWAIT

  assert_return (close(socketFd) );

  assert(get_number_of_allocated_net_endpoints() == 0);
}
