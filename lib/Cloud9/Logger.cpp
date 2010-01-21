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

namespace cloud9 {

#ifdef CLOUD9_HAVE_ADVANCED_LOGGING
	Logger Logger::logger = Log4CXXLogger();
#else
	Logger Logger::logger = SimpleLogger();
#endif

Logger &Logger::getLogger() {
	return Logger::logger;
}

}
