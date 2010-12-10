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

enum EventClass {
  TestCase = 0,
  ErrorCase = 1,
  JobExecutionState = 2,
  TimeOut = 3,
  InstructionBatch = 4,
  ReplayBatch = 5,
  SMTSolve = 6,
  SATSolve = 7,
  ConstraintSolve = 8
};

enum EventAttribute {
  Default = 0,
  WallTime = 1,
  ThreadTime = 2,
  StateDepth = 3,
  StateMultiplicity = 4
};

class IOServices;

class InstrumentationManager {
public:
	typedef sys::TimeValue TimeStamp;

	typedef vector<int> statistics_t;

	typedef map<int, string> event_attributes_t;
	typedef pair<TimeStamp, int> event_id_t;

	typedef map<int, event_attributes_t> pending_events_t;
	typedef vector<pair<event_id_t, event_attributes_t> > events_t;

	typedef map<string, pair<unsigned, unsigned> > coverage_t;
private:
	typedef set<InstrumentationWriter*> writer_set_t;

	TimeStamp referenceTime;

	TimeStamp now() {
		return TimeStamp::now();
	}

	statistics_t stats;

	pending_events_t pendingEvents;
	events_t events;

	writer_set_t writers;

	coverage_t coverage;
	bool covUpdated;

	IOServices *ioServices;

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

	void recordEventAttributeStr(EventClass id, EventAttribute attr, string value);

	template <class T>
	void recordEventAttribute(EventClass id, EventAttribute attr, T value) {
	  stringstream ss;
	  ss << value;
	  ss.flush();

	  recordEventAttributeStr(id, attr, ss.str());
	}

	void recordEventTiming(EventClass id, const Timer &t) {
	  recordEventAttribute(id, WallTime, t.getRealTime());
	  recordEventAttribute(id, ThreadTime, t.getThreadTime());
	}

	void recordEvent(EventClass id, bool reset = true);

	void recordEvent(EventClass id, Timer &t) {
	  recordEventTiming(id, t);
	  recordEvent(id);
	}

	void recordEvent(EventClass id, string value) {
	  recordEventAttributeStr(id, Default, value);
	  recordEvent(id);
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
