/*
 * autobind.c
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
  char buf[128];
  assert_return(fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(sendto(fd, buf, sizeof buf, 0, (const struct sockaddr*)&addr, sizeof addr));

  assert(get_number_of_allocated_net_endpoints() == 0);

  assert_return(close(fd));

  return 0;
}

#undef _IS_LLVM
