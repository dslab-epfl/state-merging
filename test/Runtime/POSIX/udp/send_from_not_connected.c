/*
 * send_from_not_connected.c
 *
 *  Created on: Nov 26, 2010
 *      Author: art_haali
 */

#define _IS_LLVM

#include <errno.h>

#include "udp_test_helpers.h"
#include "assert_fail.h"

int main(int argc, char **argv) {
  struct sockaddr_in addr;
  setupAddress(&addr, 6666);

  int fd;
  assert_return(fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));

  char buf[100];
  assert(send(fd, buf, sizeof buf, 0) == -1);
  assert(errno == EDESTADDRREQ);
  return 0;
}
