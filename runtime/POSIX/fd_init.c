/*
 * fd_init.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "fd.h"

fd_entry_t __fdt[MAX_FDS];

void klee_init_fds(unsigned n_files, unsigned file_length,
                   int sym_stdout_flag, int do_all_writes_flag,
                   unsigned n_streams, unsigned stream_len,
                   unsigned n_dgrams, unsigned dgram_len,
                   unsigned max_failures) {
  assert(0 && "not implemented");
}
