/*
 * underlying.h
 *
 *  Created on: Aug 10, 2010
 *      Author: stefan
 */

#ifndef UNDERLYING_H_
#define UNDERLYING_H_

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <fcntl.h>

#ifdef __FORCE_USE_UNDERLYING
#define DECLARE_UNDERLYING(type, name, ...) \
    type __klee_original_ ## name(__VA_ARGS__); \
    __attribute__((used)) static const void* __usage_original_ ## name = (void*)&__klee_original_ ## name;
#else
#define DECLARE_UNDERLYING(type, name, ...) \
    type __klee_original_ ## name(__VA_ARGS__);
#endif

DECLARE_UNDERLYING(int, stat, const char *path, struct stat *buf);
DECLARE_UNDERLYING(int, fstat, int fd, struct stat *buf);
DECLARE_UNDERLYING(int, lstat, const char *path, struct stat *buf);

DECLARE_UNDERLYING(int, close, int fd);

DECLARE_UNDERLYING(int, fcntl, int fd, int cmd, ...);

DECLARE_UNDERLYING(int, ioctl, int d, int request, ...);

DECLARE_UNDERLYING(int, open, const char *pathname, int flags, ...);
DECLARE_UNDERLYING(int, creat, const char *pathname, mode_t mode);

DECLARE_UNDERLYING(char *, getcwd, char *buf, size_t size);

DECLARE_UNDERLYING(off_t, lseek, int fd, off_t offset, int whence);

DECLARE_UNDERLYING(int, chmod, const char *path, mode_t mode);
DECLARE_UNDERLYING(int, fchmod, int fd, mode_t mode);

DECLARE_UNDERLYING(ssize_t, readlink, const char *path, char *buf, size_t bufsiz);

DECLARE_UNDERLYING(int, chown, const char *path, uid_t owner, gid_t group);
DECLARE_UNDERLYING(int, fchown, int fd, uid_t owner, gid_t group);
DECLARE_UNDERLYING(int, lchown, const char *path, uid_t owner, gid_t group);

DECLARE_UNDERLYING(int, chdir, const char *path);
DECLARE_UNDERLYING(int, fchdir, int fd);

DECLARE_UNDERLYING(int, fsync, int fd);
DECLARE_UNDERLYING(int, fdatasync, int fd);

DECLARE_UNDERLYING(int, statfs, const char *path, struct statfs *buf);
DECLARE_UNDERLYING(int, fstatfs, int fd, struct statfs *buf);

DECLARE_UNDERLYING(int, truncate, const char *path, off_t length);
DECLARE_UNDERLYING(int, ftruncate, int fd, off_t length);

DECLARE_UNDERLYING(int, access, const char *pathname, int mode);

DECLARE_UNDERLYING(ssize_t, read, int fd, void *buf, size_t count);
DECLARE_UNDERLYING(ssize_t, write, int fd, const void *buf, size_t count);


#endif /* UNDERLYING_H_ */
