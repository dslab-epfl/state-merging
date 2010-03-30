/*
 * Logger.cpp
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#include "cloud9/Common.h"
#include "cloud9/Logger.h"

#ifdef CLOUD9_HAVE_ADVANCED_LOGGING
#include "Log4CXXLogger.h"
#endif
#include "SimpleLogger.h"

#include "llvm/ADT/STLExtras.h"

#include <dlfcn.h>
#include <cxxabi.h>
#include <execinfo.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

using namespace llvm;

namespace cloud9 {

#ifdef CLOUD9_HAVE_ADVANCED_LOGGING
	Logger Logger::logger = Log4CXXLogger();
#else
	Logger Logger::logger = SimpleLogger();
#endif

static std::string GetStackTrace(int startingFrom = 0, int maxDepth = 4) {
	static void* StackTrace[256];
	static char lineBuffer[256];
	std::string result;

	// Use backtrace() to output a backtrace on Linux systems with glibc.
	int depth = backtrace(StackTrace, static_cast<int> (array_lengthof(
			StackTrace)));

	if (depth > startingFrom + maxDepth)
		depth = startingFrom + maxDepth;

	for (int i = startingFrom; i < depth; ++i) {
		if (i > startingFrom)
			result.append(" ");
		Dl_info dlinfo;
		dladdr(StackTrace[i], &dlinfo);
		//const char* name = strrchr(dlinfo.dli_fname, '/');

		snprintf(lineBuffer, 256, "%-3d", i);

		result.append(lineBuffer);

		if (dlinfo.dli_sname != NULL) {
			int res;
			char* d = abi::__cxa_demangle(dlinfo.dli_sname, NULL, NULL, &res);

			snprintf(lineBuffer, 256, " %s + %ld",
					(d == NULL) ? dlinfo.dli_sname : d, (char*) StackTrace[i]
							- (char*) dlinfo.dli_saddr);
			result.append(lineBuffer);

			free(d);
		}
	}

	return result;
}

Logger &Logger::getLogger() {
	return Logger::logger;
}

std::string Logger::getStackTrace() {
	return cloud9::GetStackTrace(2, 8);
}



}
