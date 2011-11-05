//===-- KInstruction.h ------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_KINSTRUCTION_H
#define KLEE_KINSTRUCTION_H

#include "klee/Config/config.h"
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/Support/DataTypes.h"
#else
#include "llvm/System/DataTypes.h"
#endif
#else
#include "llvm/Support/DataTypes.h"
#endif
#include <vector>

#include "llvm/ADT/DenseMap.h"
#include "klee/Internal/Module/QCE.h"

namespace llvm {
  class Instruction;
  class Value;
}

namespace klee {
  class Executor;
  struct InstructionInfo;
  class KModule;

  struct KQCEInfoItem {
    HotValue hotValue;
    double qce;
    int vnumber;
  };

  struct KQCEInfo {
    double total;
    std::vector<KQCEInfoItem> vars;
  };

  typedef llvm::DenseMap<HotValue, llvm::SmallVector<HotValue, 2> >
            HotValueArgMap;

  /// KInstruction - Intermediate instruction representation used
  /// during execution.
  struct KInstruction {
    llvm::Instruction *inst;    
    const InstructionInfo *info;

    /// Value numbers for each operand. -1 is an invalid value,
    /// otherwise negative numbers are indices (negated and offset by
    /// 2) into the module constant table and positive numbers are
    /// register indices.
    int *operands;
    /// Destination register index.
    unsigned dest;

    bool originallyCovered;
    bool isBBHead;

    KQCEInfo* qceInfo;

  public:
    virtual ~KInstruction();
    void dump() const;
    void print(llvm::raw_ostream &ostr) const;
  };

  struct KGEPInstruction : KInstruction {
    /// indices - The list of variable sized adjustments to add to the pointer
    /// operand to execute the instruction. The first element is the operand
    /// index into the GetElementPtr instruction, and the second element is the
    /// element size to multiple that index by.
    std::vector< std::pair<unsigned, uint64_t> > indices;

    /// offset - A constant offset to add to the pointer operand to execute the
    /// insturction.
    uint64_t offset;
  };

  struct KCallInstruction: KInstruction {
    bool vulnerable;    // Whether the result of this call is unchecked, and
                        // thus may lead to further errors
    HotValueArgMap hotValueArgMap;

    static bool classof(const KInstruction *) { return true; }
  };

  struct KUseFreqInstruction: KCallInstruction {
    bool isPointer;
    int valueIdx;
    uint64_t numUses;
    uint64_t totalNumUses;
  };
}

#endif

