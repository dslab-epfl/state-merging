/*
 * KleeCommon.h
 *
 *  Created on: Apr 6, 2010
 *      Author: stefan
 */

#ifndef KLEECOMMON_H_
#define KLEECOMMON_H_

#include "klee/Config/config.h"

#include <string>

#define KLEE_ROOT_VAR	"KLEE_ROOT"
#define KLEE_UCLIBC_ROOT_VAR "KLEE_UCLIBC_ROOT"

std::string getKleePath();
std::string getKleeLibraryPath();
std::string getUclibcPath();


#endif /* KLEECOMMON_H_ */
