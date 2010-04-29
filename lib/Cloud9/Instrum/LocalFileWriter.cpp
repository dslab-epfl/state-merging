/*
 * LocalFileWriter.cpp
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#include "cloud9/instrum/LocalFileWriter.h"

#include <iostream>


namespace cloud9 {

namespace instrum {

LocalFileWriter::LocalFileWriter(std::ostream &s, std::ostream &e) :
	statsStream(s), eventsStream(e) {

}

LocalFileWriter::~LocalFileWriter() {

}

void LocalFileWriter::writeStatistics(InstrumentationManager::TimeStamp &time,
		InstrumentationManager::StatisticsData &stats) {
	statsStream << time;

	for (InstrumentationManager::StatisticsData::iterator it = stats.begin();
			it != stats.end(); it++) {

		statsStream << ' ' << (*it).first << '=' << (*it).second;
	}

	statsStream << endl;
}

void LocalFileWriter::writeEvents(InstrumentationManager::EventsData &events) {
	for (InstrumentationManager::EventsData::iterator it = events.begin();
			it != events.end(); it++) {
		eventsStream << (*it).first << ' ' << (*it).second.first << ' ' <<
				(*it).second.second << endl;
	}
}



}

}
