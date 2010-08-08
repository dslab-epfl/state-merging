/*
 * fd_init.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "fd.h"
#include "files.h"

#include <stdlib.h>

// File descriptor table static initialization,
// for pre-klee_init_fds FD operations
fd_entry_t __fdt[MAX_FDS] = {
    { FD_IS_CONCRETE | FD_IS_PIPE, 0, NULL, 1},
    { FD_IS_CONCRETE | FD_IS_PIPE, 1, NULL, 1},
    { FD_IS_CONCRETE | FD_IS_PIPE, 2, NULL, 1}
};

// Symbolic file system
filesystem_t __fs;

void klee_init_fds(unsigned n_files, unsigned file_length,
                   int sym_stdout_flag) {
  assert(0 && "not implemented");
}
