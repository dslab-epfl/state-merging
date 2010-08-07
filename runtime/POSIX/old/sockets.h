#ifndef __EXE_SOCKETS__
#define __EXE_SOCKETS__

#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int __fd_socket(unsigned long *args);
int __fd_bind(unsigned long *args);
int __fd_connect(unsigned long *args);
int __fd_listen(unsigned long *args);
int __fd_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int __fd_shutdown(unsigned long *args);
int __fd_getsockname(unsigned long *args);
int __fd_getpeername(unsigned long *args);
ssize_t __fd_send(int fd, const void *buf, size_t len, int flags);
ssize_t __fd_recv(int fd, void *buf, size_t len, int flags);
ssize_t __fd_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t __fd_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
ssize_t __fd_sendmsg(int fd, struct msghdr *msg, int flags);
ssize_t __fd_recvmsg(int fd, struct msghdr *msg, int flags);

int klee_getaddrinfo(const char *node, const char *service,
		     const struct addrinfo *hints,
		     struct addrinfo **res);

#endif
