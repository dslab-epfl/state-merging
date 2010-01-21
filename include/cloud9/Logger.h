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

#define CLOUD9_DEFAULT_LOG_PREFIX			"Cloud9:\t"
#define CLOUD9_DEFAULT_ERROR_PREFIX			"ERROR:\t"
#define CLOUD9_DEFAULT_INFO_PREFIX		"Info:\t"
#define CLOUD9_DEFAULT_DEBUG_PREFIX			"Debug:\t"

#define CLOUD9_ERROR(msg)	cloud9::Logger::getLogger().getInfoStream() << \
	msg << std::endl

#define CLOUD9_INFO(msg)	cloud9::Logger::getLogger().getInfoStream() << \
	msg << std::endl

#define CLOUD9_DEBUG(msg)	cloud9::Logger::getLogger().getDebugStream() << \
	msg << std::endl

#define CLOUD9_EXIT(msg) do { CLOUD9_ERROR(msg); exit(1); } while(0)



namespace cloud9 {

class Logger {
private:
	static Logger logger;

	std::string logPrefix;
	std::string errorPrefix;
	std::string infoPrefix;
	std::string debugPrefix;
protected:
	Logger() :
		logPrefix(CLOUD9_DEFAULT_LOG_PREFIX),
		errorPrefix(CLOUD9_DEFAULT_ERROR_PREFIX),
		infoPrefix(CLOUD9_DEFAULT_INFO_PREFIX),
		debugPrefix(CLOUD9_DEFAULT_DEBUG_PREFIX) {

	}

	virtual ~Logger() {

	}

public:
	static Logger &getLogger();

	const std::string &getLogPrefix() { return logPrefix; }
	void setLogPrefix(std::string prefix) { logPrefix = prefix; }

	std::ostream &getInfoStream() {
		return getInfoStreamRaw() << logPrefix << infoPrefix;
	}

	std::ostream &getErrorStream() {
		return getErrorStreamRaw() << logPrefix << errorPrefix;
	}

	std::ostream &getDebugStream() {
		return getDebugStreamRaw() << logPrefix << debugPrefix;
	}

	virtual std::ostream &getInfoStreamRaw() { return std::cerr; };
	virtual std::ostream &getErrorStreamRaw() { return std::cerr; };
	virtual std::ostream &getDebugStreamRaw() { return std::cerr; };
};

}

#endif /* LOGGER_H_ */
