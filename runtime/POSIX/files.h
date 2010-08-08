/*
 * files.h
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#ifndef FILES_H_
#define FILES_H_

#include "buffers.h"
#include "fd.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
  char *name;
  block_buffer_t contents;

  struct stat *stat;
} disk_file_t;  // The "disk" storage of the file

typedef struct {
  disk_file_t *files[MAX_FILES];
} filesystem_t;

extern filesystem_t __fs;

typedef struct {
  file_base_t __bdata;

  off_t offset;
  disk_file_t *storage;
} file_t;       // The open file structure

void __init_disk_file(disk_file_t *dfile, size_t maxsize, const char *symname,
    const struct stat *defstats);

disk_file_t *__get_sym_file(const char *pathname);


#endif /* FILES_H_ */
