/* client sends a message to a server
 *
 * compile:
 * art_haali@mobile:~/projects/klee_Cloud9/test/Runtime/POSIX$ llvm-gcc --emit-llvm -c -g -I../../../include/ -I../../../runtime/POSIX/ udp_should_work_test.c
 *
 * run:
 * art_haali@mobile:~/projects/klee_Cloud9/test/Runtime/POSIX$ ../../../Release/bin/klee --posix-runtime --libc=uclibc --simplify-sym-indices --output-module --disable-inlining --allow-external-sym-calls --only-output-states-covering-new --max-instruction-time=3600. --max-memory-inhibit=false udp_should_work_test.o
 *
 *
 * @author art_haali
 */

#define _IS_LLVM

#include "udp_test_helpers.h"
#include "assert_fail.h"

#define BUF_LENGTH 1024

void doTest(int call_send_recv) {
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
  assert_return(connect(serverFd, (struct sockaddr*)&client_addr, sizeof client_addr));

  //send
  char buf[BUF_LENGTH];
  int datagram_size;
  assert_return(datagram_size = sprintf(buf, "This is packet %d\n", 666));
  if (call_send_recv == 1) {
    my_assert(send(clientFd, buf, datagram_size, 0) == datagram_size);
  } else {
    my_assert(write(clientFd, buf, datagram_size) == datagram_size);
  }

  //recv
  memset(buf, 0, BUF_LENGTH);
  if (call_send_recv == 1) {
    my_assert(recv(serverFd, buf, BUF_LENGTH, 0) == datagram_size);
  } else {
    my_assert(read(serverFd, buf, BUF_LENGTH) == datagram_size);
  }
  printf("Received: %s\n", buf);

  assert_return(close(clientFd));
  assert_return(close(serverFd));

  assert(get_number_of_allocated_net_endpoints() == 0);
}

int main(int argc, char **argv) {
  printf("Starting send/recv test..\n");
  doTest(1);

  printf("Starting write/read test..\n");
  doTest(0);

  return 0;
}

#undef _IS_LLVM
