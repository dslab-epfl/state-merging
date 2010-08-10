//===-- fd_32.c -----------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define _LARGEFILE64_SOURCE
#include "fd.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <dirent.h>

/***/

static void __stat64_to_stat(struct stat64 *a, struct stat *b) {
  b->st_dev = a->st_dev;
  b->st_ino = a->st_ino;
  b->st_mode = a->st_mode;
  b->st_nlink = a->st_nlink;
  b->st_uid = a->st_uid;
  b->st_gid = a->st_gid;
  b->st_rdev = a->st_rdev;
  b->st_size = a->st_size;
  b->st_atime = a->st_atime;
  b->st_mtime = a->st_mtime;
  b->st_ctime = a->st_ctime;
  b->st_blksize = a->st_blksize;
  b->st_blocks = a->st_blocks;
}

/***/

int __xstat(int vers, const char *path, struct stat *buf) {
  struct stat64 tmp;
  int res = __fd_stat(path, &tmp);
  __stat64_to_stat(&tmp, buf);
  return res;
}

int stat(const char *path, struct stat *buf) {
  struct stat64 tmp;
  int res = __fd_stat(path, &tmp);
  __stat64_to_stat(&tmp, buf);
  return res;
}

int __lxstat(int vers, const char *path, struct stat *buf) {
  struct stat64 tmp;
  int res = __fd_lstat(path, &tmp);
  __stat64_to_stat(&tmp, buf);
  return res;
}

int lstat(const char *path, struct stat *buf) {
  struct stat64 tmp;
  int res = __fd_lstat(path, &tmp);
  __stat64_to_stat(&tmp, buf);
  return res;
}

int __fxstat(int vers, int fd, struct stat *buf) {
  struct stat64 tmp;
  int res = __fd_fstat(fd, &tmp);
  __stat64_to_stat(&tmp, buf);
  return res;
}

int fstat(int fd, struct stat *buf) {
  struct stat64 tmp;
  int res = __fd_fstat(fd, &tmp);
  __stat64_to_stat(&tmp, buf);
  return res;
}


/* Forward to 64 versions (uclibc expects versions w/o asm specifier) */

int open64(const char *pathname, int flags, ...) __attribute__((weak));
int open64(const char *pathname, int flags, ...) {
  mode_t mode = 0;

  if (flags & O_CREAT) {
    /* get mode */
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  return __fd_open(pathname, flags, mode);
}

int stat64(const char *path, struct stat64 *buf) __attribute__((weak));
int stat64(const char *path, struct stat64 *buf) {
  return __fd_stat(path, buf);
}

int lstat64(const char *path, struct stat64 *buf) __attribute__((weak));
int lstat64(const char *path, struct stat64 *buf) {
  return __fd_lstat(path, buf);
}

int fstat64(int fd, struct stat64 *buf) __attribute__((weak));
int fstat64(int fd, struct stat64 *buf) {
  return __fd_fstat(fd, buf);
}
