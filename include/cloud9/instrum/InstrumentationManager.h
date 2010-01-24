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
#include <map>
#include <utility>
#include <cassert>

#include <boost/thread.hpp>


namespace cloud9 {

namespace instrum {


using namespace std;
using namespace llvm;

class InstrumentationWriter;

class InstrumentationManager {
public:
	enum Statistics {
		TotalInstructions = 1,
		TotalJobs = 2
	};

	enum Events {
		TestCase = 1,
		ErrorCase = 2,
		JobExecutionState = 3
	};

	typedef sys::TimeValue TimeStamp;
	typedef map<Statistics, int> StatisticsData;
	typedef vector<pair<TimeStamp, pair<Events, string> > > EventsData;
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
public:
	InstrumentationManager();
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
};

}

}

#endif /* INSTRUMENTATIONMANAGER_H_ */
