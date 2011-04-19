/*
 * @author art_haali
 */

#define _IS_LLVM

#include "udp_test_helpers.h"
#include "assert_fail.h"

#include <sys/select.h>



#undef FD_SET
#undef FD_CLR
#undef FD_ISSET
#undef FD_ZERO

#define FD_SET(n, p)    (__FDS_BITS(p)[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p)    (__FDS_BITS(p)[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)  (__FDS_BITS(p)[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)  memset((char *)(p), '\0', sizeof(*(p)))

#define BUF_LENGTH 1024


static size_t get_total_len(struct msghdr* msg) {
  size_t total_bytes = 0;
  int i = 0;
  for(; i< msg->msg_iovlen; ++i) {
    total_bytes += msg->msg_iov[i].iov_len;
  }
  return total_bytes;
}

//void *nope_func(void* arg) {
//#ifdef _IS_LLVM
//  sleep(1000);
//#endif
//}

int main(int argc, char **argv) {
//  pthread_t thread;
//  assert_return(pthread_create(&thread, NULL, nope_func, NULL));

  int client_port = 6666, server_port = 7777;

  struct sockaddr_in server_addr, client_addr;
  setupAddress(&server_addr, server_port);
  setupAddress(&client_addr, client_port);

  int clientFd;
  assert_return(clientFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(bind(clientFd, (const struct sockaddr*)&client_addr, sizeof client_addr));
  assert_return(connect(clientFd, (struct sockaddr*)&server_addr, sizeof server_addr));

  int serverFd;
  assert_return(serverFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  assert_return(bind(serverFd, (const struct sockaddr*)&server_addr, sizeof server_addr));

  fd_set fds;
  struct timeval tv;
  int retval;

  //write access
  FD_ZERO(&fds);
  FD_SET(serverFd, &fds);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  int select_result = select(serverFd + 1, NULL, &fds, NULL, &tv);
  my_assert(select_result == 1);

  //todo: ah: cannot check it since klee reports false deadlock error
  //read access - nothing to read
//  FD_ZERO(&fds);
//  FD_SET(serverFd, &fds);
//  tv.tv_sec = 1;
//  tv.tv_usec = 0;
//  select_result = select(serverFd + 1, &fds, NULL, NULL, &tv);
//  my_assert(select_result == 0);

  //send
  char buf[BUF_LENGTH];
  int datagram_size;
  assert_return(datagram_size = sprintf(buf, "This is packet %d\n", 666));
  my_assert(send(clientFd, buf, datagram_size, 0) == datagram_size);

  //read access - smth to read
  FD_ZERO(&fds);
  FD_SET(serverFd, &fds);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  select_result = select(serverFd + 1, &fds, NULL, NULL, &tv);
  my_assert(select_result == 1);
  assert(FD_ISSET(serverFd, &fds));

  assert_return(close(clientFd));
  assert_return(close(serverFd));

  assert(get_number_of_allocated_net_endpoints() == 0);
}

#undef _IS_LLVM
