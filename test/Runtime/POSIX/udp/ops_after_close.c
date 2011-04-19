/*
 * udp_ops_after_close.c
 *
 *  Created on: Nov 26, 2010
 *      Author: art_haali
 */

#define _IS_LLVM


#include <assert.h>
#include <errno.h>

#include "udp_test_helpers.h"
#include "assert_fail.h"

int main(int argc, char **argv) {
  int port = 6666;

  struct sockaddr_in addr;
  setupAddress(&addr, port);

  int fd;
  assert_return(fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(close(fd));

  assert(bind(fd, (const struct sockaddr*)&addr, sizeof addr) == -1);
  assert(errno == EBADF);

  char buf[128];
  assert(send(fd, buf, sizeof buf, 0) == -1);
  assert(errno == EBADF);

  assert(recv(fd, buf, sizeof buf, 0) == -1);
  assert(errno == EBADF);

  assert(close(fd) == -1);
  assert(errno == EBADF);

  return 0;
}


#undef _IS_LLVM

