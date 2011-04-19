/*
 *
 *  Created on: Nov 13, 2010
 *      Author: art_haali
 */

#ifndef UDP_TESTS_H_
#define UDP_TESTS_H_


#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>  //printf
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <string.h> //memset

#ifdef _IS_LLVM
#include "sockets.h" //__net, __unix_net
#include "config.h"  //MAX_PORTS, MAX_UNIX_EPOINTS
#endif

#define SRV_IP "127.0.0.1"

void setupAddress(struct sockaddr_in *addr, int port)
{
  memset((char*)addr, 0, sizeof (*addr));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  assert(inet_aton(SRV_IP, &addr->sin_addr) != 0);
}

#ifdef _IS_LLVM
static int __get_number_of_allocated_endpoints(end_point_t* end_points, size_t size) {

  int i = 0;
  int number_of_allocated_endpoints = 0;

  for (; i < size; ++i) {
    if(end_points[i].allocated == 1)
      ++number_of_allocated_endpoints;
  }

  return number_of_allocated_endpoints;
}
#endif

int get_number_of_allocated_net_endpoints() {
#ifdef _IS_LLVM
  return __get_number_of_allocated_endpoints(__net.end_points, MAX_PORTS);
#else
  printf("warning: __get_number_of_allocated_endpoints: if compile with gcc -> always returns 0\n");
  return 0;
#endif
}

int get_number_of_allocated_unix_endpoints() {
#ifdef _IS_LLVM
  return __get_number_of_allocated_endpoints(__unix_net.end_points, MAX_UNIX_EPOINTS);
#else
  printf("warning: __get_number_of_allocated_endpoints: if compile with gcc -> always returns 0\n");
  return 0;
#endif
}


#endif /* UDP_TESTS_H_ */
