/*
 * InstrumentationWriter.h
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#ifndef INSTRUMENTATIONWRITER_H_
#define INSTRUMENTATIONWRITER_H_

#include "cloud9/instrum/InstrumentationManager.h"

namespace cloud9 {

namespace instrum {

class InstrumentationWriter {
public:
	InstrumentationWriter() { };
	virtual ~InstrumentationWriter() { };

	virtual void writeStatistics(InstrumentationManager::TimeStamp &time,
			InstrumentationManager::statistics_t &stats) = 0;
	virtual void writeCoverage(InstrumentationManager::TimeStamp &time,
	    InstrumentationManager::coverage_t &coverage) = 0;
	virtual void writeEvents(InstrumentationManager::events_t &events) = 0;
};

}

}

#endif /* INSTRUMENTATIONWRITER_H_ */
