//===-- fd.c --------------------------------------------------------------===//
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
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <termios.h>
#include <sys/select.h>
#include <ctype.h>

#include <klee/klee.h>

/* #define DEBUG */

void klee_warning(const char*);
void klee_warning_once(const char*);
int klee_get_errno(void);
void klee_change_name(void*, char*);

static void *__concretize_ptr(const void *p);
static size_t __concretize_size(size_t s);
static const char *__concretize_string(const char *s);

/* Returns pointer to the file entry for a valid fd */
exe_file_t* __get_file(int fd) {
  if (fd>=0 && fd<MAX_FDS) {
    exe_file_t *f = &__exe_env.fds[fd];
    if (f->flags & eOpen)
      return f;
  }

  return 0;
}

int access(const char *pathname, int mode) {
  exe_disk_file_t *dfile = __get_sym_file(pathname);
  
  if (dfile) {
    /* XXX we should check against stat values but we also need to
       enforce in open and friends then. */
    return 0;
  } else {
    int r = syscall(__NR_access, __concretize_string(pathname), mode);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  } 
}

mode_t umask(mode_t mask) {  
  mode_t r = __exe_env.umask;
  __exe_env.umask = mask & 0777;
  return r;
}


/* Returns 1 if the process has the access rights specified by 'flags'
   to the file with stat 's'.  Returns 0 otherwise*/
static int has_permission(int flags, struct stat64 *s) {
  int write_access, read_access;
  mode_t mode = s->st_mode;
  
  if (flags & O_RDONLY || flags & O_RDWR)
    read_access = 1;
  else read_access = 0;

  if (flags & O_WRONLY || flags & O_RDWR)
    write_access = 1;
  else write_access = 0;

  /* XXX: We don't worry about process uid and gid for now. 
     We allow access if any user has access to the file. */
#if 0
  uid_t uid = s->st_uid;
  uid_t euid = geteuid();
  gid_t gid = s->st_gid;
  gid_t egid = getegid();
#endif  

  if (read_access && ((mode & S_IRUSR) | (mode & S_IRGRP) | (mode & S_IROTH)))
    return 0;

  if (write_access && !((mode & S_IWUSR) | (mode & S_IWGRP) | (mode & S_IWOTH)))
    return 0;

  return 1;
}


int __fd_open(const char *pathname, int flags, mode_t mode)  {
  exe_disk_file_t *df;
  exe_file_t *f;

  int fd = __get_new_fd(&f);
  if (fd < 0) 
    return fd;

  df = __get_sym_file(pathname);
  if (df) {    
    /* XXX Should check access against mode / stat / possible deletion. */
    f->dfile = df;
    
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

    if (!has_permission(flags, df->stat)) {
	errno = EACCES;
	return -1;
    }
    else
      f->dfile->stat->st_mode = ((f->dfile->stat->st_mode & ~0777) |
				 (mode & ~__exe_env.umask));
  } else {    
    int os_fd = syscall(__NR_open, __concretize_string(pathname), flags, mode);
    if (os_fd == -1) {
      errno = klee_get_errno();
      return -1;
    }
    f->fd = os_fd;
  }
  
  f->flags = eOpen;
  if ((flags & O_ACCMODE) == O_RDONLY) {
    f->flags |= eReadable;
  } else if ((flags & O_ACCMODE) == O_WRONLY) {
    f->flags |= eWriteable;
  } else { /* XXX What actually happens here if != O_RDWR. */
    f->flags |= eReadable | eWriteable;
  }
  
  return fd;
}

int close(int fd) {
  static int n_calls = 0;
  exe_file_t *f;
  int r = 0;
  
  n_calls++;  

  f = __get_file(fd);
  if (!f) {
    errno = EBADF;
    return -1;
  } 

  if (__exe_fs.max_failures && *__exe_fs.close_fail == n_calls) {
    __exe_fs.max_failures--;
    errno = EIO;
    return -1;
  }

#if 0
  if (!f->dfile) {
    /* if a concrete fd */
    r = syscall(__NR_close, f->fd);
  }
  else r = 0;
#endif

  if ((f->flags & eSocket) && (f->flags & eDgramSocket)) {
    assert(f->dfile);   /* We assume a symbolic socket */
    assert(f->foreign); /* and a non-NULL foreign address */
    free(f->foreign);
  }
  memset(f, 0, sizeof *f);
  
  return r;
}


ssize_t __fd_scatter_read(exe_file_t *f, const struct iovec *iov, int iovcnt)
{
  ssize_t total = 0;

  assert(f->off >= 0);
  if (f->dfile->size < f->off)
    return 0;

  for (; iovcnt > 0; iov++, iovcnt--) {
    size_t this_len = iov->iov_len;
    if (f->off + this_len > f->dfile->size)
      this_len = f->dfile->size - f->off;
    memcpy(iov->iov_base, f->dfile->contents + f->off, this_len);
    total += this_len;
    f->off += this_len;
  }

  return total;
}


ssize_t read(int fd, void *buf, size_t count) {
  static int n_calls = 0;
  exe_file_t *f;

  n_calls++;

  if (count == 0) 
    return 0;

  if (buf == NULL) {
    errno = EFAULT;
    return -1;
  }
  
  f = __get_file(fd);

  if (!f) {
    errno = EBADF;
    return -1;
  }  

  if (__exe_fs.max_failures && *__exe_fs.read_fail == n_calls) {
    __exe_fs.max_failures--;
    errno = EIO;
    return -1;
  }
  
  if (!f->dfile) {
    /* concrete file */
    int r;
    buf = __concretize_ptr(buf);
    count = __concretize_size(count);
    /* XXX In terms of looking for bugs we really should do this check
       before concretization, at least once the routine has been fixed
       to properly work with symbolics. */
    klee_check_memory_access(buf, count);
    if (f->fd == 0)
      r = syscall(__NR_read, f->fd, buf, count);
    else
      r = syscall(__NR_pread64, f->fd, buf, count, (off64_t) f->off);

    if (r == -1) {
      errno = klee_get_errno();
      return -1;
    }
    
    if (f->fd != 0)
      f->off += r;
    return r;
  }
  else {
    /* symbolic file */
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len  = count;
    return __fd_scatter_read(f, &iov, 1);
  }
}


ssize_t __fd_gather_write(exe_file_t *f, const struct iovec *iov, int iovcnt)
{
  ssize_t total = 0;

  for (; iovcnt > 0; iov++, iovcnt--) {
    size_t this_len = iov->iov_len;
    if (f->dfile->src) {
      /* writes to a symbolic socket always succeed, ignored */
      klee_check_memory_access(iov->iov_base, iov->iov_len);
      goto skip;
    }

    if (f->off + this_len > f->dfile->size) {
      if (__exe_env.save_all_writes)
        assert(0);
      else {
        if (f->off < f->dfile->size)
          this_len = f->dfile->size - f->off;
        else
          this_len = 0;
      }
    }

    if (this_len)
      memcpy(f->dfile->contents + f->off, iov->iov_base, this_len);
    
    if (this_len != iov->iov_len)
      klee_warning_once("write() ignores bytes.");

    if (f->dfile == __exe_fs.sym_stdout)
      __exe_fs.stdout_writes += this_len;

  skip:
    total += iov->iov_len;
    f->off += iov->iov_len;
  }

  return total;
}


ssize_t write(int fd, const void *buf, size_t count) {
  static int n_calls = 0;
  exe_file_t *f;

  n_calls++;

  f = __get_file(fd);

  if (!f) {
    errno = EBADF;
    return -1;
  }

  if (__exe_fs.max_failures && *__exe_fs.write_fail == n_calls) {
    __exe_fs.max_failures--;
    errno = EIO;
    return -1;
  }

  if (!f->dfile) {
    int r;

    buf = __concretize_ptr(buf);
    count = __concretize_size(count);
    /* XXX In terms of looking for bugs we really should do this check
       before concretization, at least once the routine has been fixed
       to properly work with symbolics. */
    klee_check_memory_access(buf, count);
    if (f->fd == 1 || f->fd == 2)
      r = syscall(__NR_write, f->fd, buf, count);
    else r = syscall(__NR_pwrite64, f->fd, buf, count, (off64_t) f->off);
    
    if (r == -1) {
      errno = klee_get_errno();
      return -1;
    }
    
    assert(r >= 0);
    if (f->fd != 1 && f->fd != 2)
      f->off += r;

    return r;
  }
  else {
    /* symbolic file */    
    struct iovec iov;
    iov.iov_base = (void *) buf;  /* const_cast */
    iov.iov_len  = count;
    return __fd_gather_write(f, &iov, 1);
  }
}


off64_t __fd_lseek(int fd, off64_t offset, int whence) {
  off64_t new_off;
  exe_file_t *f = __get_file(fd);

  if (!f) {
    errno = EBADF;
    return -1;
  }

  if (!f->dfile) {
    /* We could always do SEEK_SET then whence, but this causes
       troubles with directories since we play nasty tricks with the
       offset, and the OS doesn't want us to randomly seek
       directories. We could detect if it is a directory and correct
       the offset, but really directories should only be SEEK_SET, so
       this solves the problem. */
    if (whence == SEEK_SET) {
      new_off = syscall(__NR_lseek, f->fd, (int) offset, SEEK_SET);
    } else {
      new_off = syscall(__NR_lseek, f->fd, (int) f->off, SEEK_SET);

      /* If we can't seek to start off, just return same error.
         Probably ESPIPE. */
      if (new_off != -1) {
        assert(new_off == f->off);
        new_off = syscall(__NR_lseek, f->fd, (int) offset, whence);
      }
    }

    if (new_off == -1) {
      errno = klee_get_errno();
      return -1;
    }

    f->off = new_off;
    return new_off;
  }
  
  switch (whence) {
  case SEEK_SET: new_off = offset; break;
  case SEEK_CUR: new_off = f->off + offset; break;
  case SEEK_END: new_off = f->dfile->size + offset; break;
  default: {
    errno = EINVAL;
    return (off64_t) -1;
  }
  }

  if (new_off < 0) {
    errno = EINVAL;
    return (off64_t) -1;
  }
    
  f->off = new_off;
  return f->off;
}

int __fd_stat(const char *path, struct stat64 *buf) {  
  exe_disk_file_t *dfile = __get_sym_file(path);
  if (dfile) {
    memcpy(buf, dfile->stat, sizeof(*dfile->stat));
    return 0;
  } 

  {
#if __WORDSIZE == 64
    int r = syscall(__NR_stat, __concretize_string(path), buf);
#else
    int r = syscall(__NR_stat64, __concretize_string(path), buf);
#endif
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

int __fd_lstat(const char *path, struct stat64 *buf) {
  exe_disk_file_t *dfile = __get_sym_file(path);
  if (dfile) {
    memcpy(buf, dfile->stat, sizeof(*dfile->stat));
    return 0;
  } 

  {    
#if __WORDSIZE == 64
    int r = syscall(__NR_lstat, __concretize_string(path), buf);
#else
    int r = syscall(__NR_lstat64, __concretize_string(path), buf);
#endif
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

int chdir(const char *path) {
  exe_disk_file_t *dfile = __get_sym_file(path);

  if (dfile) {
    /* XXX incorrect */
    klee_warning("symbolic file, ignoring (ENOENT)");
    errno = ENOENT;
    return -1;
  }

  {
    int r = syscall(__NR_chdir, __concretize_string(path));
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

/* Sets mode and or errno and return appropriate result. */
static int __df_chmod(exe_disk_file_t *df, mode_t mode) {
  if (geteuid() == df->stat->st_uid) {
    if (getgid() != df->stat->st_gid)
      mode &= ~ S_ISGID;
    df->stat->st_mode = ((df->stat->st_mode & ~07777) | 
                         (mode & 07777));
    return 0;
  } else {
    errno = EPERM;
    return -1;
  }
}

int chmod(const char *path, mode_t mode) {
  static int n_calls = 0;

  exe_disk_file_t *dfile = __get_sym_file(path);

  n_calls++;
  if (__exe_fs.max_failures && *__exe_fs.chmod_fail == n_calls) {
    __exe_fs.max_failures--;
    errno = EIO;
    return -1;
  }

  if (dfile) {
    return __df_chmod(dfile, mode);
  } else {
    int r = syscall(__NR_chmod, __concretize_string(path), mode);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

int fchmod(int fd, mode_t mode) {
  static int n_calls = 0;

  exe_file_t *f = __get_file(fd);
  
  if (!f) {
    errno = EBADF;
    return -1;
  }

  n_calls++;
  if (__exe_fs.max_failures && *__exe_fs.fchmod_fail == n_calls) {
    __exe_fs.max_failures--;
    errno = EIO;
    return -1;
  }

  if (f->dfile) {
    return __df_chmod(f->dfile, mode);
  } else {
    int r = syscall(__NR_fchmod, f->fd, mode);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }  
}

static int __df_chown(exe_disk_file_t *df, uid_t owner, gid_t group) {
  klee_warning("symbolic file, ignoring (EPERM)");
  errno = EPERM;
  return -1;  
}

int chown(const char *path, uid_t owner, gid_t group) {
  exe_disk_file_t *df = __get_sym_file(path);

  if (df) {
    return __df_chown(df, owner, group);
  } else {
    int r = syscall(__NR_chown, __concretize_string(path), owner, group);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

int lchown(const char *path, uid_t owner, gid_t group) {
  /* XXX Ignores 'l' part */
  exe_disk_file_t *df = __get_sym_file(path);

  if (df) {
    return __df_chown(df, owner, group);
  } else {
    int r = syscall(__NR_chown, __concretize_string(path), owner, group);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

int __fd_fstat(int fd, struct stat64 *buf) {
  exe_file_t *f = __get_file(fd);

  if (!f) {
    errno = EBADF;
    return -1;
  }
  
  if (!f->dfile) {
#if __WORDSIZE == 64
    int r = syscall(__NR_fstat, f->fd, buf);
#else
    int r = syscall(__NR_fstat64, f->fd, buf);
#endif
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
  
  memcpy(buf, f->dfile->stat, sizeof(*f->dfile->stat));
  return 0;
}

int __fd_ftruncate(int fd, off64_t length) {
  static int n_calls = 0;
  exe_file_t *f = __get_file(fd);

  n_calls++;

  if (!f) {
    errno = EBADF;
    return -1;
  }

  if (__exe_fs.max_failures && *__exe_fs.ftruncate_fail == n_calls) {
    __exe_fs.max_failures--;
    errno = EIO;
    return -1;
  }
  
  if (f->dfile) {
    klee_warning("symbolic file, ignoring (EIO)");
    errno = EIO;
    return -1;
  } else {
#if __WORDSIZE == 64
    int r = syscall(__NR_ftruncate, f->fd, length);
#else
    int r = syscall(__NR_ftruncate64, f->fd, length);
#endif
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }  
}



int __fd_statfs(const char *path, struct statfs *buf) {
  exe_disk_file_t *dfile = __get_sym_file(path);
  if (dfile) {
    /* XXX incorrect */
    klee_warning("symbolic file, ignoring (ENOENT)");
    errno = ENOENT;
    return -1;
  }

  {
    int r = syscall(__NR_statfs, __concretize_string(path), buf);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

int fstatfs(int fd, struct statfs *buf) {
  exe_file_t *f = __get_file(fd);

  if (!f) {
    errno = EBADF;
    return -1;
  }
  
  if (f->dfile) {
    klee_warning("symbolic file, ignoring (EBADF)");
    errno = EBADF;
    return -1;
  } else {
    int r = syscall(__NR_fstatfs, f->fd, buf);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

int rmdir(const char *pathname) {
  exe_disk_file_t *dfile = __get_sym_file(pathname);
  if (dfile) {
    /* XXX check access */ 
    if (S_ISDIR(dfile->stat->st_mode)) {
      dfile->stat->st_ino = 0;
      return 0;
    } else {
      errno = ENOTDIR;
      return -1;
    }
  }

  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

int unlink(const char *pathname) {
  exe_disk_file_t *dfile = __get_sym_file(pathname);
  if (dfile) {
    /* XXX check access */ 
    if (S_ISREG(dfile->stat->st_mode)) {
      dfile->stat->st_ino = 0;
      return 0;
    } else if (S_ISDIR(dfile->stat->st_mode)) {
      errno = EISDIR;
      return -1;
    } else {
      errno = EPERM;
      return -1;
    }
  }

  klee_warning("ignoring (EPERM)");
  errno = EPERM;
  return -1;
}

ssize_t readlink(const char *path, char *buf, size_t bufsize) {
  exe_disk_file_t *dfile = __get_sym_file(path);
  if (dfile) {
    /* XXX We need to get the sym file name really, but since we don't
       handle paths anyway... */
    if (S_ISLNK(dfile->stat->st_mode)) {
      buf[0] = path[0];
      if (bufsize>1) buf[1] = '.';
      if (bufsize>2) buf[2] = 'l';
      if (bufsize>3) buf[3] = 'n';
      if (bufsize>4) buf[4] = 'k';
      return (bufsize>5) ? 5 : bufsize;
    } else {
      errno = EINVAL;
      return -1;
    }
  } else {
    int r = syscall(__NR_readlink, path, buf, bufsize);
    if (r == -1)
      errno = klee_get_errno();
    return r;
  }
}

// Copy-paste this into the program that uses the regular FD_* operations
#undef FD_SET
#undef FD_CLR
#undef FD_ISSET
#undef FD_ZERO
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	memset((char *)(p), '\0', sizeof(*(p)))

int select(int nfds, fd_set *read, fd_set *write,
           fd_set *except, struct timeval *timeout) {
  fd_set in_read, in_write, in_except, os_read, os_write, os_except;
  int i, count = 0, os_nfds = 0;

  if (read) {
    in_read = *read;
    FD_ZERO(read);
  } else {
    FD_ZERO(&in_read);
  }

  if (write) {
    in_write = *write;
    FD_ZERO(write);
  } else {
    FD_ZERO(&in_write);
  }
   
  if (except) {
    in_except = *except;
    FD_ZERO(except);
  } else {
    FD_ZERO(&in_except);
  }

  FD_ZERO(&os_read);
  FD_ZERO(&os_write);
  FD_ZERO(&os_except);

  /* Check for symbolic stuff */
  for (i=0; i<nfds; i++) {    
    if (FD_ISSET(i, &in_read) || FD_ISSET(i, &in_write) || FD_ISSET(i, &in_except)) {
      exe_file_t *f = __get_file(i);
      if (!f) {
        errno = EBADF;
        return -1;
      } else if (f->dfile) {
        /* Operations on this fd will never block... */
        unsigned flags = 0;
        if (FD_ISSET(i, &in_read)) {
          if (!(f->flags & eSocket))
            flags |= 01;
          else if (f->flags & eDgramSocket)
            flags |= (__exe_fs.n_sym_dgrams_used  < __exe_fs.n_sym_dgrams)  ? 01 : 0; /* more dgrams available */
          else if (f->flags & eListening)
            flags |= (__exe_fs.n_sym_streams_used < __exe_fs.n_sym_streams) ? 01 : 0; /* more streams available */
          else
            flags |= 01;

          if (flags & 01) FD_SET(i, read);
        }
        if (FD_ISSET(i, &in_write)) { flags |= 02; FD_SET(i, write); }
	//XXX: zamf: this is strange, why should set except if there was no error, 
	//I don't understand how this could have worked
        //if (FD_ISSET(i, &in_except)) { flags |= 04; FD_SET(i, except); }
        if (flags) ++count;
      } else {
        if (FD_ISSET(i, &in_read)) FD_SET(f->fd, &os_read);
        if (FD_ISSET(i, &in_write)) FD_SET(f->fd, &os_write);
        if (FD_ISSET(i, &in_except)) FD_SET(f->fd, &os_except);
        if (f->fd >= os_nfds) os_nfds = f->fd + 1;
      }
    }
  }

  if (os_nfds > 0) {
    /* Never allow blocking select. This is broken but what else can
       we do. */
    struct timeval tv = { 0, 0 };    
    int r = syscall(__NR_select, os_nfds, 
                    &os_read, &os_write, &os_except, &tv);
    
    if (r == -1) {
      /* If no symbolic results, return error. Otherwise we will
         silently ignore the OS error. */
      if (!count) {
        errno = klee_get_errno();
        return -1;
      }
    } else {
      count += r;

      /* Translate resulting sets back */
      for (i=0; i<nfds; i++) {
        exe_file_t *f = __get_file(i);
        if (f && !f->dfile) {
          if (read && FD_ISSET(f->fd, &os_read)) FD_SET(i, read);
          if (write && FD_ISSET(f->fd, &os_write)) FD_SET(i, write);
          if (except && FD_ISSET(f->fd, &os_except)) FD_SET(i, except);
        }
      }
    }
  }

  return count;
}

/*** Library functions ***/

char *getcwd(char *buf, size_t size) {
  static int n_calls = 0;
  int r;

  n_calls++;

  if (__exe_fs.max_failures && *__exe_fs.getcwd_fail == n_calls) {
    __exe_fs.max_failures--;
    errno = ERANGE;
    return NULL;
  }

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
  r = syscall(__NR_getcwd, buf, size);
  if (r == -1) {
    errno = klee_get_errno();
    return NULL;
  }
    
  return buf;
}

/*** Helper functions ***/

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



/* Trivial model:
   if path is "/" (basically no change) accept, otherwise reject
*/
int chroot(const char *path) {
  if (path[0] == '\0') {
    errno = ENOENT;
    return -1;
  }
    
  if (path[0] == '/' && path[1] == '\0') {
    return 0;
  }
  
  klee_warning("ignoring (ENOENT)");
  errno = ENOENT;
  return -1;
}
