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


namespace cloud9 {

namespace instrum {


using namespace std;
using namespace llvm;

class InstrumentationWriter;

enum Statistics {
	TotalProcInstructions = 0,
	TotalProcJobs = 1,
	TotalStatesExplored = 2,
	TotalPathsExplored = 3,

	TotalNewInstructions = 4,
	TotalNewStates = 5,
	TotalNewPaths = 6,

	TotalExportedJobs = 7,
	TotalImportedJobs = 8,
	TotalDroppedJobs = 9,

	CurrentQueueSize = 10,
	CurrentPathCount = 11,
	CurrentImportedPathCount = 12

};

enum Events {
	TestCase = 0,
	ErrorCase = 1,
	JobExecutionState = 2
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
};

extern InstrumentationManager &theInstrManager;

}

}

#endif /* INSTRUMENTATIONMANAGER_H_ */
