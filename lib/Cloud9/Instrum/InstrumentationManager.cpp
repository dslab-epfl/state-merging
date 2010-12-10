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
#include <boost/thread.hpp>
#include <boost/asio.hpp>

using namespace llvm;

namespace {
cl::opt<int> InstrUpdateRate("c9-instr-update-rate",
		cl::desc("Rate (in seconds) of updating instrumentation info"),
		cl::init(2));
}

namespace cloud9 {

namespace instrum {

class IOServices {
public:
  boost::timed_mutex terminateMutex;
  boost::mutex eventsMutex;
  boost::mutex coverageMutex;
  boost::thread instrumThread;
public:
  IOServices() { }
};

InstrumentationManager &theInstrManager = InstrumentationManager::getManager();

void InstrumentationManager::instrumThreadControl() {

	CLOUD9_INFO("Instrumentation started");

	for (;;) {
		boost::system::error_code code;
        bool terminated = ioServices->terminateMutex.timed_lock(boost::posix_time::seconds(InstrUpdateRate));

        if (terminated) {
			CLOUD9_INFO("Instrumentation interrupted. Stopping.");
			writeStatistics();
			writeEvents();
            writeCoverage();
            ioServices->terminateMutex.unlock();
			break;
		}

		writeStatistics();
		writeEvents();
		writeCoverage();
	}
}


InstrumentationManager::InstrumentationManager() :
        referenceTime(now()), stats(MAX_STATISTICS), covUpdated(false) {

  ioServices = new IOServices();
}

InstrumentationManager::~InstrumentationManager() {
	stop();

	for (writer_set_t::iterator it = writers.begin(); it != writers.end(); it++) {
		InstrumentationWriter *writer = *it;

		delete writer;
	}

	delete ioServices;
}

void InstrumentationManager::start() {
    ioServices->terminateMutex.lock();
	ioServices->instrumThread = boost::thread(&InstrumentationManager::instrumThreadControl, this);
}

void InstrumentationManager::stop() {
	if (ioServices->instrumThread.joinable()) {
        ioServices->terminateMutex.unlock();

		ioServices->instrumThread.join();
	}
}

void InstrumentationManager::writeStatistics() {
	TimeStamp stamp = now() - referenceTime;

	for (writer_set_t::iterator it = writers.begin(); it != writers.end(); it++) {
		InstrumentationWriter *writer = *it;

		writer->writeStatistics(stamp, stats);
	}
}

void InstrumentationManager::writeEvents() {
	boost::unique_lock<boost::mutex> lock(ioServices->eventsMutex);
	events_t eventsCopy = events;
	events.clear();
	lock.unlock();

	for (writer_set_t::iterator it = writers.begin(); it != writers.end(); it++) {
		InstrumentationWriter *writer = *it;

		writer->writeEvents(eventsCopy);
	}
}

void InstrumentationManager::writeCoverage() {
  boost::unique_lock<boost::mutex> lock(ioServices->coverageMutex);
  if (!covUpdated) {
    lock.unlock();
    return;
  }

  coverage_t coverageCopy = coverage;
  covUpdated = false;
  lock.unlock();

  TimeStamp stamp = now() - referenceTime;

  for (writer_set_t::iterator it = writers.begin(); it != writers.end(); it++) {
    InstrumentationWriter *writer = *it;

    writer->writeCoverage(stamp, coverageCopy);
  }
}

void InstrumentationManager::recordEventAttributeStr(EventClass id, EventAttribute attr, string value) {
  boost::lock_guard<boost::mutex> lock(ioServices->eventsMutex);
  pendingEvents[id][attr] = value;
}

void InstrumentationManager::recordEvent(EventClass id, bool reset) {
  boost::lock_guard<boost::mutex> lock(ioServices->eventsMutex);
  events.push_back(make_pair(make_pair(now() - referenceTime, id), pendingEvents[id]));

  if (reset)
    pendingEvents.erase(id);
}

void InstrumentationManager::updateCoverage(string tag, std::pair<unsigned, unsigned> value) {
  boost::lock_guard<boost::mutex> lock(ioServices->coverageMutex);

  coverage[tag] = value;
  covUpdated = true;
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
