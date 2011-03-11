/*
 * Common.h
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#ifndef COMMON_WORKER_H_
#define COMMON_WORKER_H_

#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include <string>

#include "cloud9/Common.h"

enum LibcType {
	NoLibc, UcLibc
};

extern std::string InputFile;
extern LibcType Libc;
extern bool WithPOSIXRuntime;

extern bool UseGlobalCoverage;

extern std::string LBAddress;
extern int LBPort;

extern std::string LocalAddress;
extern int LocalPort;

extern int RetryConnectTime;
extern int UpdateTime;

namespace klee {
	class RNG;
	extern RNG theRNG;
}


#endif /* COMMON_H_ */
