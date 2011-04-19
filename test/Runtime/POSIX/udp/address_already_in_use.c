/*
 * address_already_in_use.c
 *
 *  Created on: Nov 26, 2010
 *      Author: art_haali
 */

#include <assert.h>
#include <errno.h>

#define _IS_LLVM


#include "assert_fail.h"
#include "udp_test_helpers.h"


int main(int argc, char **argv) {
  struct sockaddr_in addr;
  setupAddress(&addr, 6666);

  int fd;
  assert_return(fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(bind(fd, (const struct sockaddr*) &addr, sizeof addr));

  int fd2;
  assert_return(fd2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert(bind(fd2, (const struct sockaddr*) &addr, sizeof addr) == -1);
  assert(errno == EADDRINUSE);

  assert_return(close(fd));

  return 0;
}

#undef _IS_LLVM
