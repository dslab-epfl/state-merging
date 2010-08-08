/*
 * files.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "files.h"

#include "common.h"

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <klee/klee.h>


////////////////////////////////////////////////////////////////////////////////
// Internal Routines
////////////////////////////////////////////////////////////////////////////////

static int __isupper(const char c) {
  return (('A' <= c) & (c <= 'Z'));
}

/* Returns pointer to the symbolic file structure if the pathname is symbolic */
disk_file_t *__get_sym_file(const char *pathname) {
  char c = pathname[0];

  if (c == 0 || pathname[1] != 0)
    return NULL;

  unsigned int i;
  for (i = 0; i < MAX_FILES; i++) {
    if (!ARRAY_CHECK(__fs.files, i))
      continue;
    if (c == 'A' + (char)i) {
      disk_file_t *df = __fs.files[i];
      return df;
    }
  }

  return NULL;
}

static void _init_stats(disk_file_t *dfile, const struct stat *defstats) {
  struct stat *stat = dfile->stat;

  /* Important since we copy this out through getdents, and readdir
     will otherwise skip this entry. For same reason need to make sure
     it fits in low bits. */
  klee_assume((stat->st_ino & 0x7FFFFFFF) != 0);

  /* uclibc opendir uses this as its buffer size, try to keep
     reasonable. */
  klee_assume((stat->st_blksize & ~0xFFFF) == 0);

  klee_prefer_cex(stat, !(stat->st_mode & ~(S_IFMT | 0777)));
  klee_prefer_cex(stat, stat->st_dev == defstats->st_dev);
  klee_prefer_cex(stat, stat->st_rdev == defstats->st_rdev);
  klee_prefer_cex(stat, (stat->st_mode&0700) == 0600);
  klee_prefer_cex(stat, (stat->st_mode&0070) == 0020);
  klee_prefer_cex(stat, (stat->st_mode&0007) == 0002);
  klee_prefer_cex(stat, (stat->st_mode&S_IFMT) == S_IFREG);
  klee_prefer_cex(stat, stat->st_nlink == 1);
  klee_prefer_cex(stat, stat->st_uid == defstats->st_uid);
  klee_prefer_cex(stat, stat->st_gid == defstats->st_gid);
  klee_prefer_cex(stat, stat->st_blksize == 4096);
  klee_prefer_cex(stat, stat->st_atime == defstats->st_atime);
  klee_prefer_cex(stat, stat->st_mtime == defstats->st_mtime);
  klee_prefer_cex(stat, stat->st_ctime == defstats->st_ctime);

  stat->st_size = dfile->contents->max_size;
  stat->st_blocks = 8;
}

void __init_disk_file(disk_file_t *dfile, size_t maxsize, const char *symname,
    const struct stat *defstats) {
  char namebuf[64];

  // Initializing the file name...
  dfile->name = (char*)malloc(MAX_PATH_LEN);

  strcpy(namebuf, symname); strcat(namebuf, "-name");
  klee_make_symbolic(dfile->name, MAX_PATH_LEN, namebuf);
  klee_make_shared(dfile->name, MAX_PATH_LEN);

  unsigned int i;
  for (i = 0; i < MAX_PATH_LEN; i++) {
    klee_prefer_cex(dfile->name, __isupper(dfile->name[i]));
  }

  // Initializing the buffer contents...
  _block_init(&dfile->contents, maxsize);

  strcpy(namebuf, symname); strcat(namebuf, "-data");
  klee_make_symbolic(dfile->contents.contents, maxsize, namebuf);
  klee_make_shared(dfile->contents.contents, maxsize);

  // Initializing the statistics...
  dfile->stat = (struct stat*)malloc(sizeof(struct stat));

  strcpy(namebuf, symname); strcat(namebuf, "-stat");
  klee_make_symbolic(dfile->stat, sizeof(struct stat), namebuf);
  klee_make_shared(dfile->stat, sizeof(struct stat));

  _init_stats(dfile, defstats);
}

////////////////////////////////////////////////////////////////////////////////
// The POSIX API
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Directory management
////////////////////////////////////////////////////////////////////////////////

DIR *opendir(const char *name) {
  assert(0 && "not implemented");
  return NULL;
}

DIR *fdopendir(int fd) {
  assert(0 && "not implemented");
  return NULL;
}

int closedir(DIR *dirp) {
  assert(0 && "not implemented");
  return -1;
}

struct dirent *readdir(DIR *dirp) {
  assert(0 && "not implemented");
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Forwarded / unsupported calls
////////////////////////////////////////////////////////////////////////////////

