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


namespace cloud9 {

namespace instrum {


using namespace std;
using namespace llvm;

class InstrumentationManager {
public:
	enum Statistics {
		TotalInstructions,
		TotalJobs
	};

	enum Events {
		TestCase,
		ErrorCase,
		JobExecutionState
	};
private:
	typedef sys::TimeValue TimeStamp;

	TimeStamp referenceTime;

	TimeStamp now() {
		sys::TimeValue now(0,0), user(0,0), sys(0,0);
		sys::Process::GetTimeUsage(now, user, sys);

		return now;
	}

	map<Statistics, int> stats;
	map<Events, vector<pair<TimeStamp, string> > > events;
public:
	InstrumentationManager() {
		referenceTime = now();
	}

	virtual ~InstrumentationManager() {

	}

	void recordEvent(Events id, string value) {
		events[id].push_back(make_pair(now(), value));
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
