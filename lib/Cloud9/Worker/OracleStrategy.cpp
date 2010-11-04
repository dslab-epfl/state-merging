/*
 * OracleStrategy.cpp
 *
 *  Created on: May 7, 2010
 *      Author: stefan
 */

#include "cloud9/worker/OracleStrategy.h"
#include "cloud9/worker/TreeObjects.h"

#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/InstructionInfoTable.h"

#include "llvm/Instruction.h"

namespace cloud9 {

namespace worker {

OracleStrategy::OracleStrategy(WorkerTree *_tree, std::vector<unsigned int> &_goalPath) :
  tree(_tree), goalPath(_goalPath) {

}

OracleStrategy::~OracleStrategy() {
  // TODO Auto-generated destructor stub
}

bool OracleStrategy::checkProgress(SymbolicState *state) {
  unsigned int i = 0;
  bool result = true; // Assume by default that everything is OK

  while (state->_instrPos < goalPath.size() && i < state->_instrProgress.size()) {
    unsigned int expected = goalPath[state->_instrPos];
    unsigned int obtained = state->_instrProgress[i]->inst->getOpcode();

    if (expected != obtained) {
      CLOUD9_DEBUG("Oracle instruction mismatch. Expected " <<
          expected << " and got " <<
          obtained << " instead. Instruction: " << state->_instrProgress[i]->info->assemblyLine);

      result = false;
      break;
    }
    state->_instrPos++;
    i++;
  }

  state->_instrProgress.erase(state->_instrProgress.begin(),
      state->_instrProgress.begin() + i);

  return result;
}

ExecutionJob* OracleStrategy::onNextJobSelection() {
  if (validStates.empty())
    return NULL;

  SymbolicState *state = *(validStates.begin());

  return selectJob(tree, state);
}

void OracleStrategy::onStateActivated(SymbolicState *state) {
  if (checkProgress(state)) {
    CLOUD9_DEBUG("Valid state added to the oracle, at instr position " << state->_instrPos);
    validStates.insert(state);
  }
}

void OracleStrategy::onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode) {
  if (!checkProgress(state)) {
    CLOUD9_DEBUG("State became invalid on the oracle, at instr position " << state->_instrPos);
    // Filter out the states that went off the road
    validStates.erase(state);
  }
}

void OracleStrategy::onStateDeactivated(SymbolicState *state) {
  validStates.erase(state);
}

}

}
