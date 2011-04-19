/*
 *
 * @author art_haali
 */

#define _IS_LLVM

#include "udp_test_helpers.h"
#include "assert_fail.h"

#define BUF_LENGTH 1024

int main(int argc, char **argv) {
  int client_port = 6666, server_port = 7777;

  struct sockaddr_in server_addr, client_addr;
  setupAddress(&server_addr, server_port);
  setupAddress(&client_addr, client_port);

  int clientFd;
  assert_return(clientFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(bind(clientFd, (const struct sockaddr*)&client_addr, sizeof client_addr));
  //no call to connect()

  int serverFd;
  assert_return(serverFd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP));
  assert_return(bind(serverFd, (const struct sockaddr*)&server_addr, sizeof server_addr));
  assert_return(connect(serverFd, (struct sockaddr*)&client_addr, sizeof client_addr));

  //sendto
  char buf[BUF_LENGTH];
  int datagram_size;
  assert_return(datagram_size = sprintf(buf, "This is packet %d\n", 666));
  my_assert(sendto(clientFd, buf, datagram_size, 0,
      (const struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) == datagram_size);

  //recv
  memset(buf, 0, BUF_LENGTH);
  my_assert(recv(serverFd, buf, BUF_LENGTH, 0) == datagram_size);
  printf("Received: %s\n", buf);

  //send to unknown address
  server_addr.sin_port += 1;
  my_assert(sendto(clientFd, buf, datagram_size, 0, (const struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))
      == datagram_size);
  my_assert(recv(serverFd, buf, BUF_LENGTH, 0) == -1);


  assert_return(close(clientFd));
  assert_return(close(serverFd));

  assert(get_number_of_allocated_net_endpoints() == 0);

  return 0;
}

#undef _IS_LLVM
