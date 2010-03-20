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
#include "klee/util/ExprPPrinter.h"

#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"


#include <sstream>
#include <iostream>
#include <iomanip>
#include <boost/io/ios_state.hpp>

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

ConstraintLogEntry::ConstraintLogEntry(klee::ExecutionState *state) {
	ostringstream oss(ostringstream::out);

	klee::ExprPPrinter::printConstraints(oss, state->constraints);

	oss.flush();

	constraints = oss.str();
}

void serializeExecutionTrace(std::ostream &os, klee::ExecutionState *state) {
	WorkerTree::Node *node = state->getWorkerNode().get();

	std::vector<int> path;
	WorkerTree::Node *crtNode = node;

	while (crtNode->getParent() != NULL) {
		path.push_back(crtNode->getIndex());
		crtNode = crtNode->getParent();
	}

	std::reverse(path.begin(), path.end());


	const llvm::BasicBlock *crtBasicBlock = NULL;
	const llvm::Function *crtFunction = NULL;

	llvm::raw_os_ostream raw_os(os);


	for (int i = 0; i <= path.size(); i++) {
		const ExecutionTrace &trace = (**crtNode).getTrace();
		// Output each instruction in the node
		for (ExecutionTrace::const_iterator it = trace.getEntries().begin();
				it != trace.getEntries().end(); it++) {
			if (InstructionTraceEntry *instEntry = dynamic_cast<InstructionTraceEntry*>(*it)) {
				klee::KInstruction *ki = instEntry->getInstruction();
				bool newBB = false;
				bool newFn = false;

				if (ki->inst->getParent() != crtBasicBlock) {
					crtBasicBlock = ki->inst->getParent();
					newBB = true;
				}

				if (crtBasicBlock != NULL && crtBasicBlock->getParent() != crtFunction) {
					crtFunction = crtBasicBlock->getParent();
					newFn = true;
				}

				if (newFn) {
					os << std::endl;
					os << "   Function '" << ((crtFunction != NULL) ? crtFunction->getNameStr() : "") << "':" << std::endl;
				}

				if (newBB) {
					os << "----------- " << ((crtBasicBlock != NULL) ? crtBasicBlock->getNameStr() : "") << " ----" << std::endl;
				}

				boost::io::ios_all_saver saver(os);
				os << std::setw(9) << ki->info->assemblyLine << ": ";
				saver.restore();
				ki->inst->print(raw_os, NULL);
				os << std::endl;
			} else if (ConstraintLogEntry *logEntry = dynamic_cast<ConstraintLogEntry*>(*it)) {
				os << logEntry->getConstraints() << std::endl;
			}
		}

		if (i < path.size()) {
			crtNode = crtNode->getChild(path[i]);
		}
	}
}

}

}
