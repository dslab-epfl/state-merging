/*
 * common.c
 *
 *  Created on: Aug 11, 2010
 *      Author: stefan
 */

#include "common.h"

#include <sys/types.h>

#include <klee/klee.h>

void *__concretize_ptr(const void *p) {
  char *pc = (char*) klee_get_valuel((long) p);
  klee_assume(pc == p);
  return pc;
}

size_t __concretize_size(size_t s) {
  size_t sc = klee_get_valuel((long)s);
  klee_assume(sc == s);
  return sc;
}

const char *__concretize_string(const char *s) {
  char *sc = __concretize_ptr(s);
  unsigned i;

  for (i=0; ; ++i) {
    char c = *sc;
    if (!(i&(i-1))) {
      if (!c) {
        *sc++ = 0;
        break;
      } else if (c=='/') {
        *sc++ = '/';
      }
    } else {
      char cc = (char) klee_get_valuel((long)c);
      klee_assume(cc == c);
      *sc++ = cc;
      if (!cc) break;
    }
  }

  return s;
}

int __inject_fault(const char *fname, int errno, ...) {
  return 0;
}
