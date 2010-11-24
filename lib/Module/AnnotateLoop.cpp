//===-- AnnotateLoop.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Passes.h"
#include "klee/Config/config.h"

#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/SmallSet.h"

#if !(LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/LLVMContext.h"
#endif

using namespace llvm;
using namespace klee;

char AnnotateLoopPass::ID = 1337;

bool AnnotateLoopPass::runOnLoop(Loop *L, LPPassManager &LPM) {
  BasicBlock *H = L->getHeader();
  Module *M = H->getParent()->getParent();

  Value *_klee_loop_iter = M->getFunction("_klee_loop_iter");
  Value *_klee_loop_exit = M->getFunction("_klee_loop_exit");

  std::vector<Value*> loopArgs(1,
      ConstantInt::get(Type::getInt32Ty(getGlobalContext()), ++lastLoopID));

  assert(_klee_loop_iter && _klee_loop_exit &&
         "Loop tracking functions not found!");

  // Annotate loop header: it runs every iteration
  CallInst::Create(_klee_loop_iter, loopArgs.begin(), loopArgs.end(), "",
                   H->getFirstNonPHI());

  // Find and annotate all loop exits
  SmallVector<BasicBlock*, 8> ExitBBs;
  L->getExitBlocks(ExitBBs);

  SmallSet<BasicBlock*, 8> UExitBBs;
  UExitBBs.insert(ExitBBs.begin(), ExitBBs.end());
  for (SmallSet<BasicBlock*, 8>::iterator EI = UExitBBs.begin(),
                                          EE = UExitBBs.end(); EI != EE; ++EI) {
    CallInst::Create(_klee_loop_exit, loopArgs.begin(), loopArgs.end(), "",
                     (*EI)->getFirstNonPHI());
  }

  return true;
}
