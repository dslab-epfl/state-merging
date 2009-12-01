/*
 * SimpleLogger.h
 *
 *  Created on: Dec 1, 2009
 *      Author: stefan
 */

#ifndef SIMPLELOGGER_H_
#define SIMPLELOGGER_H_

#include "cloud9/Logger.h"

#include <iostream>

namespace cloud9 {

class SimpleLogger: public Logger {
public:
	SimpleLogger() { };
	virtual ~SimpleLogger() { };
};

}

#endif /* SIMPLELOGGER_H_ */
