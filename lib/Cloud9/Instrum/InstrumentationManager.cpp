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

#include <boost/asio.hpp>

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
	boost::asio::io_service service;
	CLOUD9_INFO("Instrumentation started");

	for (;;) {
		boost::asio::deadline_timer t(service, boost::posix_time::seconds(InstrUpdateRate));
		t.wait();

		writeStatistics();
		writeEvents();
	}
}


InstrumentationManager::InstrumentationManager() :
		referenceTime(now()) {

}

InstrumentationManager::~InstrumentationManager() {
	instrumThread.join();
}

void InstrumentationManager::start() {
	instrumThread = boost::thread(&InstrumentationManager::instrumThreadControl, this);
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

}

}
