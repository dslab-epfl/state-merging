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


public:
	LocalFileWriter(std::ostream &statsStream, std::ostream &eventsStream);
	virtual ~LocalFileWriter();

	void writeStatistics(InstrumentationManager::TimeStamp &time,
			InstrumentationManager::StatisticsData &stats);

	void writeEvents(InstrumentationManager::EventsData &events);
};

}

}

#endif /* LOCALFILEWRITER_H_ */
