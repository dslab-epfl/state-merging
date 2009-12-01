/*
 * Logger.h
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#include <string>
#include <ostream>
#include <iostream>

#define CLOUD9_LOG_PREFIX			"Cloud9:\t"
#define CLOUD9_ERROR_PREFIX			"ERROR:\t"
#define CLOUD9_WARNING_PREFIX		"Warning:\t"
#define CLOUD9_DEBUG_PREFIX			"Debug:\t"

#define CLOUD9_ERROR(msg)	cloud9::Logger::getLogger().getErrorStream() << \
	CLOUD9_LOG_PREFIX << CLOUD9_ERROR_PREFIX << msg << std::endl

#define CLOUD9_WARNING(msg)	cloud9::Logger::getLogger().getWarningStream() << \
	CLOUD9_LOG_PREFIX << CLOUD9_WARNING_PREFIX << msg << std::endl

#define CLOUD9_DEBUG(msg)	cloud9::Logger::getLogger().getDebugStream() << \
	CLOUD9_LOG_PREFIX << CLOUD9_DEBUG_PREFIX << msg << std::endl

#define CLOUD9_EXIT(msg) do { CLOUD9_ERROR(msg); exit(1); } while(0)



namespace cloud9 {

class Logger {
private:
	static Logger logger;

protected:
	Logger();

	virtual ~Logger();

public:
	static Logger &getLogger();

	virtual std::ostream &getWarningStream() { return std::cerr; };
	virtual std::ostream &getErrorStream() { return std::cerr; };
	virtual std::ostream &getDebugStream() { return std::cerr; };
};

}

#endif /* LOGGER_H_ */
