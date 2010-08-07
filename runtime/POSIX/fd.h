/*
 * fd.h
 *
 *  Created on: Aug 6, 2010
 *      Author: stefan
 */

#ifndef FD_H_
#define FD_H_

#include "maxlimits.h"

#define FD_IS_CONCRETE      (1 << 0)    // The calls get forwarded to the underlying implementation
#define FD_IS_DUMMY         (1 << 1)    // The data read is symbolic, and the data written is discarded
#define FD_IS_FILE          (1 << 3)    // The fd points to a disk file
#define FD_IS_SOCKET        (1 << 4)    // The fd points to a socket
#define FD_IS_PIPE          (1 << 5)    // The fd points to a pipe
#define FD_CLOSE_ON_EXEC    (1 << 6)    // The fd should be closed at exec() time (ignored)

#define FD_IS_STREAM        (FD_IS_SOCKET | FD_IS_PIPE)
#define FD_CAN_SEEK         (FD_IS_FILE)

typedef struct {
  unsigned int refcount;
  int flags;
} file_base_t;

typedef struct {
  unsigned int attr;

  int concrete_fd;

  file_base_t *io_object;

  char allocated;
} fd_entry_t;

extern fd_entry_t __fdt[MAX_FDS];

void klee_init_fds(unsigned n_files, unsigned file_length,
                   int sym_stdout_flag, int do_all_writes_flag,
                   unsigned n_streams, unsigned stream_len,
                   unsigned n_dgrams, unsigned dgram_len,
                   unsigned max_failures);


#endif /* FD_H_ */
