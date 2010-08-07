/*
 * files.c
 *
 *  Created on: Aug 7, 2010
 *      Author: stefan
 */

#include "files.h"

#include <dirent.h>
#include <sys/types.h>


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

