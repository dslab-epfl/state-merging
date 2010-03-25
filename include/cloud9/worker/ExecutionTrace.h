/*
 * ExecutionTrace.h
 *
 *  Created on: Mar 18, 2010
 *      Author: stefan
 */

#ifndef EXECUTIONTRACE_H_
#define EXECUTIONTRACE_H_


#include <vector>
#include <string>
#include <iostream>

namespace klee {
class ExecutionState;
class KInstruction;
}

namespace cloud9 {

namespace worker {

class ExecutionTraceEntry {
public:
	ExecutionTraceEntry() {}
	virtual ~ExecutionTraceEntry() {}
};

class InstructionTraceEntry: public ExecutionTraceEntry {
private:
	klee::KInstruction *ki;
public:
	InstructionTraceEntry(klee::KInstruction *_ki) : ki(_ki) {

	}

	virtual ~InstructionTraceEntry() {}

	klee::KInstruction *getInstruction() const { return ki; }
};

class ControlFlowEntry: public ExecutionTraceEntry {
private:
	bool branch;
	bool call;
	bool fnReturn;
public:
	ControlFlowEntry(bool _branch, bool _call, bool _return) :
		branch(_branch), call(_call), fnReturn(_return) {

	}

	virtual ~ControlFlowEntry() { }

	bool isBranch() const { return branch; }
	bool isCall() const { return call; }
	bool isReturn() const { return fnReturn; }
};

class DebugLogEntry: public ExecutionTraceEntry {
protected:
	std::string message;

	DebugLogEntry() { }
public:
	DebugLogEntry(const std::string &msg) : message(msg) {

	}

	virtual ~DebugLogEntry() { }

	const std::string &getMessage() const { return message; }
};

class ConstraintLogEntry: public DebugLogEntry {
public:
	ConstraintLogEntry(klee::ExecutionState *state);

	virtual ~ConstraintLogEntry() { }
};

class ExecutionTrace {
public:
	typedef std::vector<ExecutionTraceEntry*>::iterator iterator;
	typedef std::vector<ExecutionTraceEntry*>::const_iterator const_iterator;

private:
	std::vector<ExecutionTraceEntry*> entries;
public:
	ExecutionTrace();
	virtual ~ExecutionTrace();

	const std::vector<ExecutionTraceEntry*> &getEntries() const { return entries; }

	void appendEntry(ExecutionTraceEntry *entry) {
		entries.push_back(entry);
	}
};

}

}

#endif /* EXECUTIONTRACE_H_ */
