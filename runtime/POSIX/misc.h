/*
 * misc.h
 *
 *  Created on: Sep 20, 2010
 *      Author: stefan
 */

#ifndef MISC_H_
#define MISC_H_

#include "common.h"

#include <stddef.h>

typedef struct {
  void *addr;
  size_t length;

  int prot;
  int flags;

  char allocated;
} mmap_block_t;

extern mmap_block_t __mmaps[MAX_MMAPS];

void klee_init_mmap(void);


#endif /* MISC_H_ */
