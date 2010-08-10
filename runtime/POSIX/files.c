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

#define CHECK_IS_FILE(fd) \
  do { \
    if (!STATIC_LIST_CHECK(__fdt, fd)) { \
    errno = EBADF; \
    return -1; \
    } \
    if (!(__fdt[fd].attr & FD_IS_FILE)) { \
      errno = ESPIPE; \
      return -1; \
    } \
  } while (0)


////////////////////////////////////////////////////////////////////////////////
// Internal Routines
////////////////////////////////////////////////////////////////////////////////

static int __isupper(const char c) {
  return (('A' <= c) & (c <= 'Z'));
}

static void *__concretize_ptr(const void *p) {
  /* XXX 32-bit assumption */
  char *pc = (char*) klee_get_valuel((long) p);
  klee_assume(pc == p);
  return pc;
}

static size_t __concretize_size(size_t s) {
  size_t sc = klee_get_valuel((long)s);
  klee_assume(sc == s);
  return sc;
}

static const char *__concretize_string(const char *s) {
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

/* Returns 1 if the process has the access rights specified by 'flags'
   to the file with stat 's'.  Returns 0 otherwise*/
static int _can_open(int flags, const struct stat *s) {
  int write_access, read_access;
  mode_t mode = s->st_mode;

  if (flags & (O_RDONLY | O_RDWR))
    read_access = 1;
  else
    read_access = 0;

  if (flags & (O_WRONLY | O_RDWR))
    write_access = 1;
  else
    write_access = 0;

  /* XXX: We don't worry about process uid and gid for now.
     We allow access if any user has access to the file. */
#if 0
  uid_t uid = s->st_uid;
  uid_t euid = geteuid();
  gid_t gid = s->st_gid;
  gid_t egid = getegid();
#endif

  if (read_access && !(mode & (S_IRUSR | S_IRGRP | S_IROTH)))
    return 0;

  if (write_access && !(mode & (S_IWUSR | S_IWGRP | S_IWOTH)))
    return 0;

  return 1;
}

int open(const char *pathname, int flags, ...) {
  mode_t mode = 0;

  if (flags & O_CREAT) {
    /* get mode */
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  // Obtain a new file descriptor
  int fd;
  STATIC_LIST_ALLOC(__fdt, fd);

  if (fd == MAX_FDS) {
    errno = ENFILE;
    return -1;
  }

  fd_entry_t *fde = &__fdt[fd];
  fde->attr |= FD_IS_FILE;

  // Obtain a symbolic file
  disk_file_t *dfile = __get_sym_file(pathname);

  if (!dfile) {
    // Try to open the file concretely
    fde->concrete_fd = CALL_UNDERLYING(open, __concretize_string(pathname),
        flags, mode);

    if (fde->concrete_fd == -1) {
      errno = klee_get_errno();
      STATIC_LIST_CLEAR(__fdt, fd);
      return -1;
    }

    fde->attr |= FD_IS_CONCRETE;

    return fd;
  }

  // Checking the flags
  if ((flags & O_CREAT) && (flags & O_EXCL)) {
    errno = EEXIST;
    return -1;
  }

  if ((flags & O_TRUNC) && (flags & O_RDONLY)) {
    /* The result of using O_TRUNC with O_RDONLY is undefined, so we
   return error */
    fprintf(stderr, "Undefined call to open(): O_TRUNC | O_RDONLY\n");
    errno = EACCES;
    return -1;
  }

  if ((flags & O_EXCL) && !(flags & O_CREAT)) {
    /* The result of using O_EXCL without O_CREAT is undefined, so
   we return error */
    fprintf(stderr, "Undefined call to open(): O_EXCL w/o O_RDONLY\n");
    errno = EACCES;
    return -1;
  }

  if (!_can_open(flags, dfile->stat)) {
    errno = EACCES;
    return -1;
  }

  // Now we can set up the open file structure...
  file_t *file = (file_t*)malloc(sizeof(file_t));
  file->__bdata.flags = flags;
  file->__bdata.refcount = 1;
  file->storage = dfile;
  file->offset = 0;

  if ((flags & (O_WRONLY | O_RDWR)) && (flags & O_TRUNC)) {
    file->storage->contents.size = 0;
  }

  if (flags & O_CLOEXEC) {
    fde->attr |= FD_CLOSE_ON_EXEC;
  }
}

int creat(const char *pathname, mode_t mode) {
  return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

////////////////////////////////////////////////////////////////////////////////

char *getcwd(char *buf, size_t size) {
  int r;

  if (!buf) {
    if (!size)
      size = 1024;
    buf = malloc(size);
  }

  buf = __concretize_ptr(buf);
  size = __concretize_size(size);
  /* XXX In terms of looking for bugs we really should do this check
     before concretization, at least once the routine has been fixed
     to properly work with symbolics. */
  klee_check_memory_access(buf, size);
  r = CALL_UNDERLYING(getcwd, buf, size);
  if (r == -1) {
    errno = klee_get_errno();
    return NULL;
  }

  return buf;
}

////////////////////////////////////////////////////////////////////////////////

static off_t _lseek(file_t *file, off_t offset, int whence) {
  off_t newOff;
  switch (whence) {
  case SEEK_SET:
    newOff = offset;
    break;
  case SEEK_CUR:
    newOff = file->offset + offset;
    break;
  case SEEK_END:
    newOff = file->storage.contents.size + offset;
    break;
  default:
    errno = EINVAL;
    return -1;
  }

  if (newOff < 0 || newOff > file->storage.contents.size) {
    errno = EINVAL;
    return -1;
  }

  file->offset = newOff;
  return file->offset;
}

off_t lseek(int fd, off_t offset, int whence) {
  CHECK_IS_FILE(fd);

  if (__fdt[fd].attr & FD_IS_CONCRETE) {
    int res = CALL_UNDERLYING(lseek, __fdt[fd].concrete_fd, offset, whence);
    if (res == -1) {
      errno = klee_get_errno();
    }
    return res;
  }

  file_t *file = (file_t*)__fdt[fd].io_object;

  return _lseek(file, offset, whence);
}

////////////////////////////////////////////////////////////////////////////////

static int _chmod(disk_file_t *dfile, mode_t mode) {
  if (geteuid() == dfile->stat->st_uid) {
    if (getgid() != dfile->stat->st_gid)
      mode &= ~ S_ISGID;
    dfile->stat->st_mode = ((dfile->stat->st_mode & ~07777) |
                         (mode & 07777));
    return 0;
  } else {
    errno = EPERM;
    return -1;
  }
}

int chmod(const char *path, mode_t mode) {
  disk_file_t *dfile = __get_sym_file(path);

  if (!dfile) {
    int res = CALL_UNDERLYING(chmod, __concretize_string(path), mode);
    if (res == -1)
      errno = klee_get_errno();
    return res;
  }

  return __chmod(dfile, mode);
}

int fchmod(int fd, mode_t mode) {
  CHECK_IS_FILE(fd);

  if (__fdt[fd].attr & FD_IS_CONCRETE) {
    int res = CALL_UNDERLYING(fchmod, __fdt[fd].concrete_fd, mode);
    if (res == -1)
      errno = klee_get_errno();
    return -1;
  }

  file_t *file = (file_t*)__fdt[fd].io_object;

  return _chmod(file->storage, mode);
}


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

#define _WRAP_FILE_SYSCALL_ERROR(call, ...) \
  do { \
    if (__get_sym_file(pathname)) { \
      klee_warning("symbolic path, " #call " unsupported (ENOENT)"); \
      errno = ENOENT; \
      return -1; \
    } \
    int ret = CALL_UNDERLYING(call, __concretize_string(pathname), ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

#define _WRAP_FILE_SYSCALL_IGNORE(call, ...) \
  do { \
    if (__get_sym_file(pathname)) { \
      klee_warning("symbolic path, " #call " does nothing"); \
      return 0; \
    } \
    int ret = CALL_UNDERLYING(call, __concretize_string(pathname), ##__VA_ARGS__); \
    if (ret == -1) \
      errno = klee_get_errno(); \
    return ret; \
  } while (0)

#define _WRAP_FILE_SYSCALL_BLOCK(call, ...) \
  do { \
    klee_warning(#call " blocked (EPERM)"); \
    errno = EPERM; \
    return -1; \
  } while (0)

int rmdir(const char *pathname) {
  _WRAP_FILE_SYSCALL_BLOCK(rmdir);
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsize) {
  _WRAP_FILE_SYSCALL_ERROR(readlink, buf, bufsize);
}

int unlink(const char *pathname) {
  _WRAP_FILE_SYSCALL_BLOCK(unlink);
}

int chroot(const char *pathname) {
  _WRAP_FILE_SYSCALL_BLOCK(chroot);
}

int chown(const char *pathname, uid_t owner, gid_t group) {
  _WRAP_FILE_SYSCALL_ERROR(chown, owner, group);
}

int lchown(const char *pathname, uid_t owner, gid_t group) {
  _WRAP_FILE_SYSCALL_ERROR(lchown, owner, group);
}

int chdir(const char *pathname) {
  _WRAP_FILE_SYSCALL_ERROR(chdir);
}

int fsync(int fd) {
  _WRAP_FD_SYSCALL_IGNORE(fsync);
}

int fdatasync(int fd) {
  _WRAP_FD_SYSCALL_IGNORE(fdatasync);
}

int fchdir(int fd) {
  _WRAP_FD_SYSCALL_ERROR(fchdir);
}

int fchown(int fd, uid_t owner, gid_t group) {
  _WRAP_FD_SYSCALL_ERROR(fchown, owner, group);
}

int fstatfs(int fd, struct statfs *buf) {
  _WRAP_FD_SYSCALL_ERROR(fstatfs, buf);
}

int statfs(const char *pathname, struct statfs *buf) {
  _WRAP_FILE_SYSCALL_ERROR(statfs, buf);
}

int ftruncate(int fd, off_t length) {
  _WRAP_FD_SYSCALL_ERROR(ftruncate, length);
}

int truncate(const char *pathname, off_t length) {
  _WRAP_FILE_SYSCALL_ERROR(truncate, length);
}

int access(const char *pathname, int mode) {
  _WRAP_FILE_SYSCALL_IGNORE(access, mode);
}
