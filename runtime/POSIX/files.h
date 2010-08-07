/*
 * files.h
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#ifndef FILES_H_
#define FILES_H_

#include "buffers.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  char *name;
  block_buffer_t *contents;

  struct stat *stat;
} disk_file_t;

typedef struct {
  file_base_t __bdata;

  off_t offset;
  disk_file_t *storage;
} file_t;


#endif /* FILES_H_ */
