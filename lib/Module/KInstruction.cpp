//===-- KInstruction.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "llvm/Instruction.h"
#include <iostream>

using namespace llvm;
using namespace klee;

/***/

KInstruction::~KInstruction() {
  delete[] operands;
}

void KInstruction::dump() const {
  std::cerr << "Instruction:" << std::endl << "    ";
  inst->dump();
  std::cerr << "    at " << info->file << ":" << info->line
            << " (assembly line " << info->assemblyLine << ")\n";
}
