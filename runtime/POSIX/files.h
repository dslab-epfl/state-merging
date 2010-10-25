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
extern disk_file_t __stdin_file;

typedef struct {
  file_base_t __bdata;

  off_t offset;

  int concrete_fd;
  disk_file_t *storage;
} file_t;       // The open file structure

void __init_disk_file(disk_file_t *dfile, size_t maxsize, const char *symname,
    const struct stat *defstats);

disk_file_t *__get_sym_file(const char *pathname);

int _close_file(file_t *file);
ssize_t _read_file(file_t *file, void *buf, size_t count);
ssize_t _write_file(file_t *file, const void *buf, size_t count);
int _stat_file(file_t *file, struct stat *buf);
int _ioctl_file(file_t *file, unsigned long request, char *argp);

int _is_blocking_file(file_t *file, int event);

static inline int _file_is_concrete(file_t *file) {
  return file->concrete_fd >= 0;
}

int _open_concrete(int concrete_fd, int flags);
int _open_symbolic(disk_file_t *dfile, int flags);


#endif /* FILES_H_ */
