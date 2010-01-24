/*
 * LocalFileWriter.cpp
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#include "cloud9/instrum/LocalFileWriter.h"

#include <iostream>
#include <iomanip>
#include <boost/io/ios_state.hpp>


namespace cloud9 {

namespace instrum {

static std::ostream &operator<<(std::ostream &s,
			const InstrumentationManager::TimeStamp &stamp) {
	boost::io::ios_all_saver saver(s);

	s << stamp.seconds() << "." << setw(9) << setfill('0') << stamp.nanoseconds();

	return s;
}

LocalFileWriter::LocalFileWriter(std::ostream &s, std::ostream &e) :
	statsStream(s), eventsStream(e) {

}

LocalFileWriter::~LocalFileWriter() {

}

void LocalFileWriter::writeStatistics(InstrumentationManager::TimeStamp &time,
		InstrumentationManager::StatisticsData &stats) {
	statsStream << time;

	bool first = true;

	for (InstrumentationManager::StatisticsData::iterator it = stats.begin();
			it != stats.end(); it++) {
		if (!first)
			statsStream << ' ';
		else
			first = false;

		statsStream << (*it).first << '=' << (*it).second;
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
