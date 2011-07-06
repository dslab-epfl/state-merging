/*
 * ExecutionTrace.cpp
 *
 *  Created on: Mar 18, 2010
 *      Author: stefan
 */

#include "cloud9/worker/ExecutionTrace.h"

#include "cloud9/ExecutionTree.h"
#include "cloud9/worker/TreeNodeInfo.h"

#include "klee/ExecutionState.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/InstructionInfoTable.h"

#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"


#include <sstream>
#include <iostream>

using namespace std;

namespace cloud9 {

namespace worker {

ExecutionTrace::ExecutionTrace() {

}

ExecutionTrace::~ExecutionTrace() {
	for (iterator it = entries.begin(); it != entries.end(); it++) {
		delete *it;
	}
}

ConstraintLogEntry::ConstraintLogEntry(klee::ExecutionState *state) : DebugLogEntry(Constraint) {
	ostringstream oss(ostringstream::out);

	klee::c9::printStateConstraints(oss, *state) << std::endl;


	oss.flush();

	message = oss.str();
}

}

}
