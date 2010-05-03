/*
 * InstrumentationManager.h
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#ifndef INSTRUMENTATIONMANAGER_H_
#define INSTRUMENTATIONMANAGER_H_

#include "llvm/System/Process.h"

#include <set>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cassert>

#include <boost/thread.hpp>
#include <boost/asio.hpp>


namespace cloud9 {

namespace instrum {


using namespace std;
using namespace llvm;

class InstrumentationWriter;

enum Statistics {
	TotalProcInstructions = 0,

	TotalProcJobs = 1,
	TotalReplayedJobs = 14,
	TotalExportedJobs = 8,
	TotalImportedJobs = 9,
	TotalDroppedJobs = 10,

	TotalForkedStates = 15,
	TotalFinishedStates = 16,

	TotalTreePaths = 17,

	TotalReplayInstructions = 20,

	CurrentJobCount = 11,
	CurrentActiveStateCount = 18,
	CurrentInactiveStateCount = 19
};

enum Events {
	TestCase = 0,
	ErrorCase = 1,
	JobExecutionState = 2,
	TimeOut = 3
};

class InstrumentationManager {
public:
	typedef sys::TimeValue TimeStamp;
	typedef unordered_map<int,int> StatisticsData;
	typedef vector<pair<TimeStamp, pair<int, string> > > EventsData;

private:
	typedef set<InstrumentationWriter*> WriterSet;

	TimeStamp referenceTime;

	TimeStamp now() {
		return TimeStamp::now();
	}

	StatisticsData stats;
	EventsData events;

	WriterSet writers;

	boost::mutex eventsMutex;
	boost::thread instrumThread;

	boost::asio::io_service service;
	boost::asio::deadline_timer timer;

	bool terminated;

	void instrumThreadControl();

	void writeStatistics();
	void writeEvents();

	InstrumentationManager();

public:
	virtual ~InstrumentationManager();

	void registerWriter(InstrumentationWriter *writer) {
		assert(writer != NULL);

		writers.insert(writer);
	}

	void start();
	void stop();

	void recordEvent(Events id, string value) {
		boost::lock_guard<boost::mutex> lock(eventsMutex);
		events.push_back(make_pair(now() - referenceTime, make_pair(id, value)));
	}

	void setStatistic(Statistics id, int value) {
		stats[id] = value;
	}

	void topStatistic(Statistics id, int value) {
		if (value > stats[id])
			stats[id] = value;
	}

	void incStatistic(Statistics id, int value = 1) {
		stats[id] += value;
	}

	void decStatistic(Statistics id, int value = 1) {
		stats[id] -= value;
	}

	static InstrumentationManager &getManager() {
		static InstrumentationManager manager;

		return manager;
	}

	std::string stampToString(TimeStamp stamp);
	TimeStamp getRelativeTime(TimeStamp absoluteStamp);
};

extern InstrumentationManager &theInstrManager;

std::ostream &operator<<(std::ostream &s,
			const InstrumentationManager::TimeStamp &stamp);

}

}

#endif /* INSTRUMENTATIONMANAGER_H_ */
