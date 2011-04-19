/*
 * reconnect_socket.c
 *
 *  Created on: Nov 26, 2010
 *      Author: art_haali
 */

#include <errno.h>

#define _IS_LLVM

#include "udp_test_helpers.h"
#include "assert_fail.h"


int main(int argc, char **argv) {
  struct sockaddr_in server_addr1, server_addr2, client_addr;
  setupAddress(&server_addr2, 7778);
  setupAddress(&server_addr1, 7777);
  setupAddress(&client_addr, 6666);

  int clientFd;
  assert_return(clientFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(bind(clientFd, (const struct sockaddr*)&client_addr, sizeof client_addr));
  assert_return(connect(clientFd, (struct sockaddr*)&server_addr1, sizeof server_addr1));
  assert_return(connect(clientFd, (struct sockaddr*)&server_addr2, sizeof server_addr2));

  assert_return(close(clientFd));
  assert(get_number_of_allocated_net_endpoints() == 0);

  return 0;
}
