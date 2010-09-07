/*
 * LocalFileWriter.h
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#ifndef LOCALFILEWRITER_H_
#define LOCALFILEWRITER_H_

#include "cloud9/instrum/InstrumentationWriter.h"

#include <iostream>
#include <string>

namespace cloud9 {

namespace instrum {

class LocalFileWriter: public InstrumentationWriter {
private:
	std::ostream &statsStream;
	std::ostream &eventsStream;
	std::ostream &coverageStream;

public:
	LocalFileWriter(std::ostream &statsStream, std::ostream &eventsStream,
	    std::ostream &coverageStream);
	virtual ~LocalFileWriter();

	void writeStatistics(InstrumentationManager::TimeStamp &time,
			InstrumentationManager::statistics_t &stats);

	void writeEvents(InstrumentationManager::events_t &events);

    virtual void writeCoverage(InstrumentationManager::TimeStamp &time,
        InstrumentationManager::coverage_t &coverage);
};

}

}

#endif /* LOCALFILEWRITER_H_ */
