/*
 * underlying.h
 *
 *  Created on: Aug 10, 2010
 *      Author: stefan
 */

#ifndef UNDERLYING_H_
#define UNDERLYING_H_

#include "common.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <ctype.h>

DECLARE_MODEL(int, stat, const char *path, struct stat *buf);
DECLARE_MODEL(int, fstat, int fd, struct stat *buf);
DECLARE_MODEL(int, lstat, const char *path, struct stat *buf);

DECLARE_MODEL(int, close, int fd);

DECLARE_MODEL(int, fcntl, int fd, int cmd, ...);

DECLARE_MODEL(int, ioctl, int d, unsigned long request, ...);

DECLARE_MODEL(int, open, const char *pathname, int flags, ...);
DECLARE_MODEL(int, creat, const char *pathname, mode_t mode);

DECLARE_MODEL(char *, getcwd, char *buf, size_t size);

DECLARE_MODEL(off_t, lseek, int fd, off_t offset, int whence);

DECLARE_MODEL(int, chmod, const char *path, mode_t mode);
DECLARE_MODEL(int, fchmod, int fd, mode_t mode);

DECLARE_MODEL(ssize_t, readlink, const char *path, char *buf, size_t bufsiz);

DECLARE_MODEL(int, chown, const char *path, uid_t owner, gid_t group);
DECLARE_MODEL(int, fchown, int fd, uid_t owner, gid_t group);
DECLARE_MODEL(int, lchown, const char *path, uid_t owner, gid_t group);

DECLARE_MODEL(int, chdir, const char *path);
DECLARE_MODEL(int, fchdir, int fd);

DECLARE_MODEL(int, fsync, int fd);
DECLARE_MODEL(int, fdatasync, int fd);

DECLARE_MODEL(int, statfs, const char *path, struct statfs *buf);
DECLARE_MODEL(int, fstatfs, int fd, struct statfs *buf);

DECLARE_MODEL(int, truncate, const char *path, off_t length);
DECLARE_MODEL(int, ftruncate, int fd, off_t length);

DECLARE_MODEL(int, access, const char *pathname, int mode);

DECLARE_MODEL(ssize_t, read, int fd, void *buf, size_t count);
DECLARE_MODEL(ssize_t, write, int fd, const void *buf, size_t count);

DECLARE_MODEL(ssize_t, readv, int fd, const struct iovec *iov, int iovcnt);
DECLARE_MODEL(ssize_t, writev, int fd, const struct iovec *iov, int iovcnt);

DECLARE_MODEL(ssize_t, pread, int fd, void *buf, size_t count, off_t offset);
DECLARE_MODEL(ssize_t, pwrite, int fd, const void *buf, size_t count, off_t offset);

#ifdef HAVE_SYMBOLIC_CTYPE
DECLARE_MODEL(const int32_t **, __ctype_tolower_loc, void);
DECLARE_MODEL(const unsigned short **, __ctype_b_loc, void);
#endif

DECLARE_MODEL(int, select, int nfds, fd_set *readfds, fd_set *writefds,
    fd_set *exceptfds, struct timeval *timeout)

#endif /* UNDERLYING_H_ */