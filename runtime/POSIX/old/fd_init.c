//===-- fd_init.c ---------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#include "fd.h"
#include <klee/klee.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>


exe_file_system_t __exe_fs;

/* NOTE: It is important that these are statically initialized
   correctly, since things that run before main may hit them given the
   current way things are linked. */

/* XXX Technically these flags are initialized w.o.r. to the
   environment we are actually running in. We could patch them in
   klee_init_fds, but we still have the problem that uclibc calls
   prior to main will get the wrong data. Not such a big deal since we
   mostly care about sym case anyway. */


exe_sym_env_t __exe_env = {
  {{ 0, eOpen | eReadable,  0, NULL, {NULL, 0}, NULL, 0},
   { 1, eOpen | eWriteable, 0, NULL, {NULL, 0}, NULL, 0},
   { 2, eOpen | eWriteable, 0, NULL, {NULL, 0}, NULL, 0}},
  022,
  0,
  0
};

static unsigned __sym_uint32(const char *name) {
  unsigned x;
  klee_make_symbolic(&x, sizeof x, name);
  return x;
}

/* n_files: number of symbolic input files, excluding stdin
   file_length: size in bytes of each symbolic file, including stdin
   sym_stdout_flag: 1 if stdout should be symbolic, 0 otherwise
   save_all_writes_flag: 1 if all writes are executed as expected, 0 if 
                         writes past the initial file size are discarded 
			 (file offset is always incremented)
   max_failures: maximum number of system call failures */
void klee_init_fds(unsigned n_files, unsigned file_length, 
		   int sym_stdout_flag, int save_all_writes_flag,
		   unsigned n_streams, unsigned stream_len,
           unsigned n_dgrams, unsigned dgram_len,
		   unsigned max_failures) {
  unsigned k;
  char fname[] = "FILE?";
  char sname[] = "STREAM?";
  char dname[] = "DGRAM?";
  struct stat64 s;

  stat64(".", &s);

  klee_make_shared(&__exe_fs, sizeof(__exe_fs));

  __exe_fs.n_sym_files = n_files;
  __exe_fs.sym_files = malloc(sizeof(*__exe_fs.sym_files) * n_files);
  __exe_fs.n_sym_files_used = 0;
  klee_make_shared(__exe_fs.sym_files, sizeof(*__exe_fs.sym_files) * n_files);

  for (k=0; k < n_files; k++) {
    fname[strlen(fname)-1] = '1' + k;
    __create_new_dfile(&__exe_fs.sym_files[k], file_length, fname, &s, 0);
  }
  
  /* setting symbolic stdin */
  if (file_length) {
    __exe_fs.sym_stdin = malloc(sizeof(*__exe_fs.sym_stdin));
    klee_make_shared(__exe_fs.sym_stdin, sizeof(*__exe_fs.sym_stdin));

    __create_new_dfile(__exe_fs.sym_stdin, file_length, "STDIN", &s, 0);
    __exe_env.fds[0].dfile = __exe_fs.sym_stdin;
  }
  else __exe_fs.sym_stdin = NULL;

  __exe_fs.max_failures = max_failures;
  if (__exe_fs.max_failures) {
    __exe_fs.read_fail = malloc(sizeof(*__exe_fs.read_fail));
    __exe_fs.write_fail = malloc(sizeof(*__exe_fs.write_fail));
    __exe_fs.close_fail = malloc(sizeof(*__exe_fs.close_fail));
    __exe_fs.ftruncate_fail = malloc(sizeof(*__exe_fs.ftruncate_fail));
    __exe_fs.getcwd_fail = malloc(sizeof(*__exe_fs.getcwd_fail));

    klee_make_symbolic(__exe_fs.read_fail, sizeof(*__exe_fs.read_fail), "read_fail");
    klee_make_symbolic(__exe_fs.write_fail, sizeof(*__exe_fs.write_fail), "write_fail");
    klee_make_symbolic(__exe_fs.close_fail, sizeof(*__exe_fs.close_fail), "close_fail");
    klee_make_symbolic(__exe_fs.ftruncate_fail, sizeof(*__exe_fs.ftruncate_fail), "ftruncate_fail");
    klee_make_symbolic(__exe_fs.getcwd_fail, sizeof(*__exe_fs.getcwd_fail), "getcwd_fail");

    klee_make_shared(__exe_fs.read_fail, sizeof(*__exe_fs.read_fail));
    klee_make_shared(__exe_fs.write_fail, sizeof(*__exe_fs.write_fail));
    klee_make_shared(__exe_fs.close_fail, sizeof(*__exe_fs.close_fail));
    klee_make_shared(__exe_fs.ftruncate_fail, sizeof(*__exe_fs.ftruncate_fail));
    klee_make_shared(__exe_fs.getcwd_fail, sizeof(*__exe_fs.getcwd_fail));
  }

  /* setting symbolic stdout */
  if (sym_stdout_flag) {
    __exe_fs.sym_stdout = malloc(sizeof(*__exe_fs.sym_stdout));
    klee_make_shared(__exe_fs.sym_stdout, sizeof(*__exe_fs.sym_stdout));

    __create_new_dfile(__exe_fs.sym_stdout, 1024, "STDOUT", &s, 0);
    __exe_env.fds[1].dfile = __exe_fs.sym_stdout;
    __exe_fs.stdout_writes = 0;
  }
  else __exe_fs.sym_stdout = NULL;
  
  __exe_env.save_all_writes = save_all_writes_flag;
  __exe_env.version = __sym_uint32("model_version");
  klee_assume(__exe_env.version == 1);

  /** Streams **/
  __exe_fs.n_sym_streams = n_streams;
  __exe_fs.sym_streams = malloc(sizeof(*__exe_fs.sym_streams) * n_streams);
  for (k=0; k < n_streams; k++) {
    sname[strlen(sname)-1] = '1' + k;
    __create_new_dfile(&__exe_fs.sym_streams[k], stream_len, sname, &s, 1);
  }
  __exe_fs.n_sym_streams_used = 0;


  /** Datagrams **/
  __exe_fs.n_sym_dgrams = n_dgrams;
  __exe_fs.sym_dgrams = malloc(sizeof(*__exe_fs.sym_dgrams) * n_dgrams);
  for (k=0; k < n_dgrams; k++) {
    dname[strlen(dname)-1] = '1' + k;
    __create_new_dfile(&__exe_fs.sym_dgrams[k], dgram_len, dname, &s, 1);
  }
  __exe_fs.n_sym_dgrams_used = 0;
}
