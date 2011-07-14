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

static void __add_arg(int *argc, char **argv, char *arg, int argcMax) {
  if (*argc==argcMax) {
    __emit_error("too many arguments for klee_init_env");
  } else {
    argv[*argc] = arg;
    (*argc)++;
  }
}

void klee_init_env(int argc, char **argv) {
  unsigned sym_files = 0, sym_file_len = 0;
  char unsafe_flag = 0;
  int k=0;

  while (k < argc) {
    if (__streq(argv[k], "--sym-files") || __streq(argv[k], "-sym-files")) {
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
      k++;
    }
  }

  klee_init_processes();
  klee_init_fds(sym_files, sym_file_len, unsafe_flag);
  klee_init_mmap();
}

void klee_process_args(int* argcPtr, char*** argvPtr) {
  int argc = *argcPtr;
  char** argv = *argvPtr;

  int new_argc = 0, max_argc = 0, n_args;
  char* new_argv[1024];
  char* tmp_argv[1024];
  unsigned max_len, min_argvs, max_argvs;
  char** final_argv;
  char sym_arg_name[5] = "arg";
  unsigned sym_arg_num = 0;
  int k=0, i;

  sym_arg_name[4] = '\0';

  while (k < argc) {
    if (__streq(argv[k], "--sym-arg") || __streq(argv[k], "-sym-arg")) {
      const char *msg = "--sym-arg expects an integer argument <max-len>";
      if (++k == argc)        
	__emit_error(msg);
		
      max_len = __str_to_int(argv[k++], msg);
      sym_arg_name[3] = '0' + sym_arg_num++;
      __add_arg(&new_argc, new_argv, 
                __get_sym_str(max_len, sym_arg_name),
                1024);
      ++max_argc;
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

      for (i=0; (unsigned) i < max_argvs; ++i) {
        sym_arg_name[3] = '0' + sym_arg_num++;
        tmp_argv[i] = __get_sym_str(max_len, sym_arg_name);
      }

      //n_args = klee_range(min_argvs, max_argvs+1, "n_args");
      klee_make_symbolic(&n_args, sizeof(n_args), "n_args");
      klee_assume(n_args >= (int) min_argvs && n_args <= (int) max_argvs);
      for (i=0; i < n_args; i++) {
        __add_arg(&new_argc, new_argv, tmp_argv[i], 1024);
      }
      max_argc += max_argvs;
    }
    else if (__streq(argv[k], "--sym-files") || __streq(argv[k], "-sym-files")) {
      k++;
    }
    else if (__streq(argv[k], "--unsafe") || __streq(argv[k], "-unsafe")) {
      k++;
    }
    else {
      /* simply copy arguments */
      __add_arg(&new_argc, new_argv, argv[k++], 1024);
      ++max_argc;
    }
  }

  final_argv = (char**) malloc((max_argc+1) * sizeof(*final_argv));
  klee_mark_global(final_argv);
  memcpy(final_argv, new_argv, new_argc * sizeof(*final_argv));
  final_argv[new_argc] = 0;

  *argcPtr = new_argc;
  *argvPtr = final_argv;

  klee_event(__KLEE_EVENT_BREAKPOINT, __KLEE_BREAK_TRACE);
}

