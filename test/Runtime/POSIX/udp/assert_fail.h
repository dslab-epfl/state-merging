/*
 * assert_fail.h
 *
 *  Created on: Nov 13, 2010
 *      Author: art_haali
 */


#ifndef ASSERT_FAIL_H_
#define ASSERT_FAIL_H_

#ifdef _IS_LLVM
#include <klee/klee.h>
#endif
#include <errno.h>  //errno
#include <string.h> //strerror
#include <stdlib.h> //exit
#ifndef _IS_LLVM
#include <stdio.h> //printf
#endif


#ifdef _IS_LLVM
static void __print(const char* message)
{
  klee_debug(message);
}
#else
static void __print(const char* message)
{
  printf("%s", message);
}
#endif


void assert_fail(const char* message)
{
  __print("\n--------------- TEST FAILED ---------------\n");
  __print(message);
  __print("\nerrno: ");
  const char* errno_as_string = strerror(errno);
  __print(errno_as_string);
  __print("\n\n");
  exit(-1);
}

//TODO: ah: remove copy-paste: call my_assert
#define assert_return(func) if((func) == -1) assert_fail(__STRING(func_error_returned\x3A\t##func));

#define my_assert(expr) if((expr) == 0) assert_fail(__STRING(expr_is_false\x3A\t##expr));

#endif /* ASSERT_FAIL_H_ */
