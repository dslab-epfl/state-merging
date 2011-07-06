/*
 * ExecutionTrace.h
 *
 *  Created on: Mar 18, 2010
 *      Author: stefan
 */

#ifndef EXECUTIONTRACE_H_
#define EXECUTIONTRACE_H_

#include "klee/StackTrace.h"

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
  enum Kind{ Exec, Instr, Break, Control, Debug, Constraint, Event };
	ExecutionTraceEntry() : kind(Exec) {}
  Kind getKind() const{ return kind; }
	virtual ~ExecutionTraceEntry() {}
protected:
  Kind kind;
  ExecutionTraceEntry(Kind k) : kind(k) {}
};

class InstructionTraceEntry: public ExecutionTraceEntry {
private:
	klee::KInstruction *ki;
public:
	InstructionTraceEntry(klee::KInstruction *_ki) : ExecutionTraceEntry(Instr), ki(_ki) {

}

	virtual ~InstructionTraceEntry() {}

	klee::KInstruction *getInstruction() const { return ki; }

  static bool classof(const ExecutionTraceEntry* entry){ return entry->getKind()==Instr; }
};

class BreakpointEntry: public ExecutionTraceEntry {
private:
  unsigned int id;
public:
  BreakpointEntry(unsigned int _id) : ExecutionTraceEntry(Break), id(_id) {

  }

  virtual ~BreakpointEntry() { }

  unsigned int getID() const { return id; }

  static bool classof(const ExecutionTraceEntry* entry){  return entry->getKind()==Break;  }
};

class ControlFlowEntry: public ExecutionTraceEntry {
private:
	bool branch;
	bool call;
	bool fnReturn;
public:
	ControlFlowEntry(bool _branch, bool _call, bool _return) : ExecutionTraceEntry(Control),
		branch(_branch), call(_call), fnReturn(_return) {

	}

	virtual ~ControlFlowEntry() { }

	bool isBranch() const { return branch; }
	bool isCall() const { return call; }
	bool isReturn() const { return fnReturn; }

  static bool classof(const ExecutionTraceEntry* entry){  return entry->getKind()==Control;  }
};

class DebugLogEntry: public ExecutionTraceEntry {
protected:
	std::string message;

	DebugLogEntry() { }
  DebugLogEntry(Kind k) : ExecutionTraceEntry(k) {}
public:
	DebugLogEntry(const std::string &msg) : ExecutionTraceEntry(Debug), message(msg) {

	}

	virtual ~DebugLogEntry() { }

	const std::string &getMessage() const { return message; }

  static bool classof(const ExecutionTraceEntry* entry){  return entry->getKind()==Debug;  }
};

class ConstraintLogEntry: public DebugLogEntry {
public:
	ConstraintLogEntry(klee::ExecutionState *state);

	virtual ~ConstraintLogEntry() { }

  static bool classof(const ExecutionTraceEntry* entry){  return entry->getKind()==Constraint;  }
};

class EventEntry: public ExecutionTraceEntry {
private:
  klee::StackTrace stackTrace;
  unsigned int type;
  long int value;
public:
  EventEntry(klee::StackTrace _stackTrace, unsigned int _type, long int _value) : ExecutionTraceEntry(Event),
    stackTrace(_stackTrace), type(_type), value(_value) {

  }

  const klee::StackTrace &getStackTrace() const { return stackTrace; }
  unsigned int getType() const { return type; }
  long int getValue() const { return value; }

  static bool classof(const ExecutionTraceEntry* entry){  return entry->getKind()==Event;  }
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
