/*
 * InstrumentationManager.h
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#ifndef INSTRUMENTATIONMANAGER_H_
#define INSTRUMENTATIONMANAGER_H_

#include "llvm/System/Process.h"

#include "cloud9/instrum/Timing.h"

#include <set>
#include <vector>
#include <utility>
#include <cassert>
#include <sstream>
#include <map>


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
	CurrentStateCount = 19,

	MAX_STATISTICS = 21
};

enum Events {
	TestCase = 0,
	ErrorCase = 1,
	JobExecutionState = 2,
	TimeOut = 3,
	InstructionBatch = 4,
	ReplayBatch = 5,
	SMTSolve = 6,
	SATSolve = 7,

	MAX_EVENTS = 8
};

class IOServices;

class InstrumentationManager {
public:
	typedef sys::TimeValue TimeStamp;
	typedef vector<int> statistics_t;
	typedef vector<pair<TimeStamp, pair<int, string> > > events_t;

	typedef map<string, std::pair<unsigned, unsigned> > coverage_t;
private:
	typedef set<InstrumentationWriter*> writer_set_t;

	TimeStamp referenceTime;

	TimeStamp now() {
		return TimeStamp::now();
	}

	statistics_t stats;
	events_t events;

	writer_set_t writers;

	coverage_t coverage;
	bool covUpdated;

	IOServices *ioServices;

	bool terminated;

	void instrumThreadControl();

	void writeStatistics();
	void writeEvents();
	void writeCoverage();

	InstrumentationManager();

public:
	virtual ~InstrumentationManager();

	void registerWriter(InstrumentationWriter *writer) {
		assert(writer != NULL);

		writers.insert(writer);
	}

	void start();
	void stop();

	void recordEvent(Events id, string value);
	void recordEvent(Events id, Timer &timer) {
	  ostringstream os;
	  os << timer;
	  os.flush();

	  recordEvent(id, os.str());
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

	void updateCoverage(string tag, std::pair<unsigned, unsigned> value);

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
