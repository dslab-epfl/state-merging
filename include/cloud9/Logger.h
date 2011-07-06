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

#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#include "llvm/System/TimeValue.h"
#else
#include "llvm/Support/TimeValue.h"
#endif

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

#define CLOUD9_STACKTRACE cloud9::Logger::getStackTrace()



namespace cloud9 {

class Logger {
private:
	static Logger logger;

	std::string logPrefix;
	std::string errorPrefix;
	std::string infoPrefix;
	std::string debugPrefix;

	std::string timeStamp;

	llvm::sys::TimeValue startTime;
protected:
	Logger() :
		logPrefix(CLOUD9_DEFAULT_LOG_PREFIX),
		errorPrefix(CLOUD9_DEFAULT_ERROR_PREFIX),
		infoPrefix(CLOUD9_DEFAULT_INFO_PREFIX),
		debugPrefix(CLOUD9_DEFAULT_DEBUG_PREFIX),
		startTime(llvm::sys::TimeValue::now()){

	}

	virtual ~Logger() {

	}

	void updateTimeStamp();

public:
	static Logger &getLogger();

	const std::string &getLogPrefix() { return logPrefix; }
	void setLogPrefix(std::string prefix) { logPrefix = prefix; }

	std::ostream &getInfoStream() {
		updateTimeStamp();
		return getInfoStreamRaw() << timeStamp << logPrefix << infoPrefix;
	}

	std::ostream &getErrorStream() {
		updateTimeStamp();
		return getErrorStreamRaw() << timeStamp << logPrefix << errorPrefix;
	}

	std::ostream &getDebugStream() {
		updateTimeStamp();
		return getDebugStreamRaw() << timeStamp << logPrefix << debugPrefix;
	}

	static std::string getStackTrace();

	virtual std::ostream &getInfoStreamRaw() { return std::cerr; };
	virtual std::ostream &getErrorStreamRaw() { return std::cerr; };
	virtual std::ostream &getDebugStreamRaw() { return std::cerr; };
};

}

std::string operator*(const std::string input, unsigned int n);

#endif /* LOGGER_H_ */
