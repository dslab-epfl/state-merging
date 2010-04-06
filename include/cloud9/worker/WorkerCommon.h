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

//#define KLEE_LIBRARY_PATH 	KLEE_DIR "/" RUNTIME_CONFIGURATION "/lib"

enum LibcType {
	NoLibc, UcLibc
};

enum JobSelectionType {
	RandomSel,
	RandomPathSel,
	CoverageOptimizedSel
};

enum JobSizingType {
	UnlimitedSize,
	FixedSize
};

enum JobExplorationType {
	RandomPathExpl
};

extern std::string InputFile;
extern LibcType Libc;
extern bool WithPOSIXRuntime;

extern JobSelectionType JobSelection;
extern JobSizingType JobSizing;
extern JobExplorationType JobExploration;

extern bool UseGlobalCoverage;

extern int MaxJobSize;
extern int MaxJobDepth;
extern int MaxJobOperations;

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
