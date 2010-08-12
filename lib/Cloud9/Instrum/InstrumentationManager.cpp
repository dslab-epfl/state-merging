/*
 * InstrumentationManager.cpp
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#include "cloud9/instrum/InstrumentationManager.h"
#include "cloud9/instrum/InstrumentationWriter.h"
#include "llvm/Support/CommandLine.h"
#include "cloud9/Logger.h"

#include <iomanip>
#include <boost/io/ios_state.hpp>

using namespace llvm;

namespace {
cl::opt<int> InstrUpdateRate("c9-instr-update-rate",
		cl::desc("Rate (in seconds) of updating instrumentation info"),
		cl::init(2));
}

namespace cloud9 {

namespace instrum {

InstrumentationManager &theInstrManager = InstrumentationManager::getManager();

void InstrumentationManager::instrumThreadControl() {

	CLOUD9_INFO("Instrumentation started");

	for (;;) {
		boost::system::error_code code;
		timer.wait(code);

		if (terminated) {
			CLOUD9_INFO("Instrumentation interrupted. Stopping.");
			writeStatistics();
			writeEvents();
			break;
		}

		timer.expires_at(timer.expires_at() + boost::posix_time::seconds(InstrUpdateRate));

		writeStatistics();
		writeEvents();
	}
}


InstrumentationManager::InstrumentationManager() :
		referenceTime(now()), timer(service, boost::posix_time::seconds(InstrUpdateRate)), terminated(false) {

}

InstrumentationManager::~InstrumentationManager() {
	stop();

	for (WriterSet::iterator it = writers.begin(); it != writers.end(); it++) {
		InstrumentationWriter *writer = *it;

		delete writer;
	}
}

void InstrumentationManager::start() {
	instrumThread = boost::thread(&InstrumentationManager::instrumThreadControl, this);
}

void InstrumentationManager::stop() {
	if (instrumThread.joinable()) {
		terminated = true;
		timer.cancel();

		instrumThread.join();
	}
}

void InstrumentationManager::writeStatistics() {
	TimeStamp stamp = now() - referenceTime;

	for (WriterSet::iterator it = writers.begin(); it != writers.end(); it++) {
		InstrumentationWriter *writer = *it;

		writer->writeStatistics(stamp, stats);
	}
}

void InstrumentationManager::writeEvents() {
	boost::unique_lock<boost::mutex> lock(eventsMutex);
	EventsData eventsCopy = events;
	events.clear();
	lock.unlock();

	for (WriterSet::iterator it = writers.begin(); it != writers.end(); it++) {
		InstrumentationWriter *writer = *it;

		writer->writeEvents(eventsCopy);
	}
}

std::string InstrumentationManager::stampToString(TimeStamp stamp) {
	std::ostringstream ss;
	ss << stamp;
	ss.flush();

	return ss.str();
}

InstrumentationManager::TimeStamp InstrumentationManager::getRelativeTime(TimeStamp absoluteStamp) {
	return absoluteStamp - referenceTime;
}

std::ostream &operator<<(std::ostream &s,
			const InstrumentationManager::TimeStamp &stamp) {
	boost::io::ios_all_saver saver(s);

	s << stamp.seconds() << "." << setw(9) << setfill('0') << stamp.nanoseconds();

	return s;
}

}

}