/*
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
  struct sockaddr_in addr1, addr2;
  setupAddress(&addr1, 6666);
  setupAddress(&addr2, 7777);

  int fd;
  assert_return(fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(bind(fd, (const struct sockaddr*) &addr1, sizeof addr1));
  assert(bind(fd, (const struct sockaddr*) &addr2, sizeof addr2) == -1);
  assert(errno == EINVAL);

  assert_return(close(fd));

  assert(get_number_of_allocated_net_endpoints() == 0);

  return 0;
}

#undef _IS_LLVM
