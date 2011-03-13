//===-- klee_init_env.c ---------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/klee.h"
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include "fd.h"
#include "multiprocess.h"
#include "misc.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

static void __emit_error(const char *msg) {
  klee_report_error(__FILE__, __LINE__, msg, "user.err");
}

/* Helper function that converts a string to an integer, and
   terminates the program with an error message is the string is not a
   proper number */   
static long int __str_to_int(char *s, const char *error_msg) {
  long int res;
  char *endptr;

  if (*s == '\0')
    __emit_error(error_msg);

  res = strtol(s, &endptr, 0);

  if (*endptr != '\0')
    __emit_error(error_msg);

  return res;
}

static int __isprint(const char c) {
  /* Assume ASCII */
  return ((32 <= c) & (c <= 126));
}

static int __streq(const char *a, const char *b) {
  while (*a == *b) {
    if (!*a)
      return 1;
    a++;
    b++;
  }
  return 0;
}

static char *__get_sym_str(int numChars, char *name) {
  int i;
  char *s = malloc(numChars+1);
  klee_mark_global(s);
  klee_make_symbolic(s, numChars+1, name);

  for (i=0; i<numChars; i++)
    klee_prefer_cex(s, __isprint(s[i]));
  
  s[numChars] = '\0';
  return s;
}

void klee_init_env(int* argcPtr, char*** argvPtr) {
  int argc = *argcPtr;
  char** argv = *argvPtr;

  unsigned sym_files = 0, sym_file_len = 0;
  char unsafe_flag = 0;

  int new_argc = 0;
  int new_argc_max = 0;
  char* new_argv[1024];

  int new_argv_rcount = 0;
  int new_argv_r[2048];

  int final_argc = 0;
  char** final_argv;

  unsigned max_len, min_argvs, max_argvs;
  char sym_arg_name[5] = "arg";
  char sym_narg_name[10] = "nargs";
  unsigned sym_arg_num = 0;
  int k=0, i;

  sym_arg_name[4] = '\0';
  sym_narg_name[6] = '\0';

#if 0
  // Recognize --help when it is the sole argument.
  if (argc == 2 && __streq(argv[1], "--help")) {
  __emit_error("klee_init_env\n\n\
usage: (klee_init_env) [options] [program arguments]\n\
  -sym-arg <N>              - Replace by a symbolic argument with length N\n\
  -sym-args <MIN> <MAX> <N> - Replace by at least MIN arguments and at most\n\
                              MAX arguments, each with maximum length N\n\
  -sym-files <NUM> <N>      - Make stdin and up to NUM symbolic files, each\n\
                              with maximum size N.\n\
  -sym-stdout               - Make stdout symbolic.\n\
  -max-fail <N>             - Allow up to <N> injected failures\n\
  -fd-fail                  - Shortcut for '-max-fail 1'\n\n");
  }
#endif

  while (k < argc) {
    if (__streq(argv[k], "--sym-arg") || __streq(argv[k], "-sym-arg")) {
      const char *msg = "--sym-arg expects an integer argument <max-len>";
      if (++k == argc)        
        __emit_error(msg);

      max_len = __str_to_int(argv[k++], msg);
      sym_arg_name[3] = '0' + sym_arg_num++;

      if (new_argc_max==1024)
        __emit_error("too many arguments for klee_init_env");

      new_argv[new_argc_max++] = __get_sym_str(max_len, sym_arg_name);
    }
    else if (__streq(argv[k], "--sym-args") || __streq(argv[k], "-sym-args")) {
      const char *msg = 
        "--sym-args expects three integer arguments <min-argvs> <max-argvs> <max-len>";

      if (k+3 >= argc)
        __emit_error(msg);
      
      k++;
      min_argvs = __str_to_int(argv[k++], msg);
      max_argvs = __str_to_int(argv[k++], msg);
      max_len = __str_to_int(argv[k++], msg);

      if (new_argc_max+max_argvs > 1024)
        __emit_error("too many arguments for klee_init_env");

      if (min_argvs < max_argvs) {
        new_argv_r[new_argv_rcount*2  ] = new_argc_max + min_argvs;
        new_argv_r[new_argv_rcount*2+1] = new_argc_max + max_argvs;
        ++new_argv_rcount;
      }

      for (i=0; (unsigned) i < max_argvs; ++i) {
        sym_arg_name[3] = '0' + sym_arg_num++;
        new_argv[new_argc_max++] = __get_sym_str(max_len, sym_arg_name);
      }

      //n_args = klee_range(min_argvs, max_argvs+1, "n_args");
      //klee_make_symbolic(&n_args, sizeof(n_args), "n_args");
      //klee_assume(n_args >= (int) min_argvs && n_args <= (int) max_argvs);
      //for (i=0; i < n_args; i++) {
      //  __add_arg(&new_argc, new_argv, tmp_argv[i], 1024);
      //}
    }
    else if (__streq(argv[k], "--sym-files") || __streq(argv[k], "-sym-files")) {
      const char* msg = "--sym-files expects two integer arguments <no-sym-files> <sym-file-len>";      

      if (k+2 >= argc)
        __emit_error(msg);
      
      k++;
      sym_files = __str_to_int(argv[k++], msg);
      sym_file_len = __str_to_int(argv[k++], msg);

    }
    else if (__streq(argv[k], "--unsafe") || __streq(argv[k], "-unsafe")) {
      unsafe_flag = 1;
      k++;
    }
    else {
      /* simply copy arguments */
      if (new_argc_max==1024)
        __emit_error("too many arguments for klee_init_env");
      new_argv[new_argc_max++] = argv[k++];
    }
  }

  klee_init_processes();
  klee_init_mmap();

  klee_init_fds(sym_files, sym_file_len, unsafe_flag);

  final_argv = (char**) malloc((new_argc_max+1) * sizeof(*final_argv));
  klee_mark_global(final_argv);

  for (i=0; i < new_argv_rcount; ++i) {
    sym_narg_name[5] = '0' + i;
    new_argv_r[i*2] = klee_range(new_argv_r[i*2], new_argv_r[i*2+1]+1, sym_narg_name);
  }

  for (i=0; i < new_argv_rcount; ++i) {
    // And now we fork...
    for (; new_argc < new_argv_r[i*2];)
      final_argv[final_argc++] = new_argv[new_argc++];
    new_argc = new_argv_r[i*2+1];
  }

  for (; new_argc < new_argc_max;)
    final_argv[final_argc++] = new_argv[new_argc++];

  final_argv[final_argc] = 0;

  *argcPtr = final_argc;
  *argvPtr = final_argv;

  klee_event(__KLEE_EVENT_BREAKPOINT, __KLEE_BREAK_TRACE);
}

