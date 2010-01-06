/*
 * Common.h
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#ifndef COMMON_H_
#define COMMON_H_

#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "klee/Config/config.h"
#include <string>

/* Cloud9 features and capabilities */

#define CLOUD9_HAVE_WATCHDOG			// Implement a fork-based watchdog mechanism
//#define CLOUD9_HAVE_ADVANCED_LOGGING	// Use log4cxx for logging purposes

#define KLEE_LIBRARY_PATH 	KLEE_DIR "/" RUNTIME_CONFIGURATION "/lib"

enum LibcType {
	NoLibc, UcLibc
};

enum JobSelectionType {
	RandomSel
};

enum JobSizingType {
	UnlimitedSize
};

enum JobExplorationType {
	RandomExpl
};

extern std::string InputFile;
extern LibcType Libc;
extern bool WithPOSIXRuntime;

extern JobSelectionType JobSelection;
extern JobSizingType JobSizing;
extern JobExplorationType JobExploration;

namespace klee {
	class RNG;
	extern RNG theRNG;
}


#endif /* COMMON_H_ */
