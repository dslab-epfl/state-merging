/*
 *
 * @author art_haali
 */

#define _IS_LLVM

#include "udp_test_helpers.h"
#include "assert_fail.h"

#define BUF_LENGTH 1024

int main(int argc, char **argv) {
  int sender_port = 6666, recv_port = 7777;

  struct sockaddr_in recv_addr, sender_addr;
  setupAddress(&recv_addr, recv_port);
  setupAddress(&sender_addr, sender_port);

  int senderFd;
  assert_return(senderFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(bind(senderFd, (const struct sockaddr*)&sender_addr, sizeof sender_addr));

  int recvFd;
  assert_return(recvFd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP));
  assert_return(bind(recvFd, (const struct sockaddr*)&recv_addr, sizeof recv_addr));

  //sendto
  char buf[BUF_LENGTH];
  int datagram_size;
  assert_return(datagram_size = sprintf(buf, "This is packet %d\n", 666));
  my_assert(sendto(senderFd, buf, datagram_size, 0,
      (const struct sockaddr*)&recv_addr, sizeof(struct sockaddr_in)) == datagram_size);

  //recvfrom
  socklen_t addr_len = 10000000;
  struct sockaddr_in actual_sender_addr;
  int res = recvfrom(recvFd, buf, BUF_LENGTH, 0, (struct sockaddr*)&actual_sender_addr, &addr_len);
  my_assert(res == datagram_size);

  assert(addr_len == sizeof sender_addr);
  assert(actual_sender_addr.sin_port == sender_addr.sin_port);
  assert(actual_sender_addr.sin_family == sender_addr.sin_family);

  assert_return(close(recvFd));
  assert_return(close(senderFd));

  assert(get_number_of_allocated_net_endpoints() == 0);

  return 0;
}

#undef _IS_LLVM
