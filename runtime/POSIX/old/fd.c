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
