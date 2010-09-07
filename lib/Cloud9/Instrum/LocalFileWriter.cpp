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

LocalFileWriter::LocalFileWriter(std::ostream &s, std::ostream &e, std::ostream &c) :
	statsStream(s), eventsStream(e), coverageStream(c) {

}

LocalFileWriter::~LocalFileWriter() {

}

void LocalFileWriter::writeStatistics(InstrumentationManager::TimeStamp &time,
		InstrumentationManager::statistics_t &stats) {
	statsStream << time;

	for (InstrumentationManager::statistics_t::iterator it = stats.begin();
			it != stats.end(); it++) {

		statsStream << ' ' << (*it).first << '=' << (*it).second;
	}

	statsStream << endl;
}

void LocalFileWriter::writeEvents(InstrumentationManager::events_t &events) {
	for (InstrumentationManager::events_t::iterator it = events.begin();
			it != events.end(); it++) {
		eventsStream << (*it).first << ' ' << (*it).second.first << ' ' <<
				(*it).second.second << endl;
	}
}

void LocalFileWriter::writeCoverage(InstrumentationManager::TimeStamp &time,
        InstrumentationManager::coverage_t &coverage) {
  coverageStream << time;

  boost::io::ios_all_saver saver(coverageStream);
  coverageStream.precision(2);
  coverageStream << fixed;

  for (InstrumentationManager::coverage_t::iterator it = coverage.begin();
      it != coverage.end(); it++) {
    coverageStream << ' ' << it->first << '=';
    if (it->second.second) {
      coverageStream << it->second.first << '/' << it->second.second;

      double perc = ((double)it->second.first)*100/it->second.second;
      coverageStream << '(' << perc << ')';
    } else {
      coverageStream << it->second.first;
    }

    coverageStream << endl;
  }
}



}

}
