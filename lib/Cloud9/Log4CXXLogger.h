/*
 * Log4CXXLogger.h
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#ifndef LOG4CXXLOGGER_H_
#define LOG4CXXLOGGER_H_

#include "cloud9/Logger.h"

#include <iostream>

namespace cloud9 {

class Log4CXXLogger: public Logger {
public:
	Log4CXXLogger();
	virtual ~Log4CXXLogger();


};

}

#endif /* LOG4CXXLOGGER_H_ */
