/*
 * fd_init.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "fd.h"
#include "files.h"
#include "sockets.h"

#include "models.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <klee/klee.h>

// File descriptor table static initialization,
// for pre-klee_init_fds FD operations
fd_entry_t __fdt[MAX_FDS] = {
    { FD_IS_CONCRETE | FD_IS_PIPE, 0, NULL, 1},
    { FD_IS_CONCRETE | FD_IS_PIPE, 1, NULL, 1},
    { FD_IS_CONCRETE | FD_IS_PIPE, 2, NULL, 1}
};

// Symbolic file system
filesystem_t    __fs;

// Symbolic network
network_t       __net;
unix_t          __unix_net;

static void _init_fdt(void) {
  // Duplicate the STD{IN,OUT,ERR} descriptors, since they belong to
  // the symbolic execution engine and we don't want to manipulate them
  // directly.
  int fd;
  for (fd = 0; fd < 3; fd++) {
    __fdt[fd].concrete_fd = CALL_UNDERLYING(fcntl, __fdt[fd].concrete_fd,
        F_DUPFD, 0L);
    assert(__fdt[fd].concrete_fd != -1);
  }
}

static void _init_filesystem(unsigned n_files, unsigned file_length) {
  char fname[] = "FILE??";
  unsigned int fname_len = strlen(fname);

  struct stat s;
  int res = CALL_UNDERLYING(stat, ".", &s);
  errno = klee_get_errno();

  assert(res == 0 && "Could not get default stat values");

  klee_make_shared(&__fs, sizeof(filesystem_t));
  memset(&__fs, 0, sizeof(filesystem_t));

  // Create n symbolic files
  unsigned int i;
  for (i = 0; i < n_files; i++) {
    __fs.files[i] = (disk_file_t*)malloc(sizeof(disk_file_t));
    klee_make_shared(__fs.files[i], sizeof(disk_file_t));

    disk_file_t *dfile = __fs.files[i];

    fname[fname_len-1] = '0' + (i % 10);
    fname[fname_len-2] = '0' + (i / 10);

    __init_disk_file(dfile, file_length, fname, &s);
  }
}

static void _init_network(void) {
  // Initialize the INET domain
  klee_make_shared(&__net, sizeof(__net));

  __net.net_addr.s_addr = htonl(DEFAULT_NETWORK_ADDR);
  __net.next_port = htons(DEFAULT_UNUSED_PORT);
  STATIC_LIST_INIT(__net.end_points);

  // Initialize the UNIX domain
  klee_make_shared(&__unix_net, sizeof(__unix_net));
  STATIC_LIST_INIT(__unix_net.end_points);
}

void klee_init_fds(unsigned n_files, unsigned file_length,
                   int sym_stdout_flag) {
  _init_fdt();
  _init_filesystem(n_files, file_length);
  _init_network();
}
