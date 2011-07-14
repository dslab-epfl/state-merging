#include "Passes.h"

#include "llvm/Module.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ImmutableMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Support/DataFlow.h"
#include "llvm/Support/CFG.h"

#include <iostream>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#define MAX_DATAFLOW_DEPTH 8
#define DEFAULT_LOOP_TRIP_COUNT 10

namespace llvm {
  void initializeUseFrequencyAnalyzerPassPass(PassRegistry&);
}

using namespace llvm;
using namespace klee;

UseFrequencyAnalyzerPass::UseFrequencyAnalyzerPass(TargetData *TD)
      : CallGraphSCCPass(ID),
      /*: FunctionPass(ID),*/ m_targetData(TD),
        m_kleeUseFreqFunc(0) {
  initializeUseFrequencyAnalyzerPassPass(*PassRegistry::getPassRegistry());
}

bool UseFrequencyAnalyzerPass::runOnSCC(CallGraphSCC &SCC) {
  bool changed = false;

  assert(m_targetData);

  for (CallGraphSCC::iterator it = SCC.begin(), ie = SCC.end(); it != ie; ++it) {
    CallGraphNode *CGNode = *it;
    Function *F = CGNode->getFunction();
    if (F && !F->isDeclaration()) {
      bool localChanged = runOnFunction(*CGNode);
      changed |= localChanged;
    }
  }

  return changed;
}

static std::pair<Function*, bool> insertFunctionDecl(Module &M,
                              StringRef name, const FunctionType *FTy) {
  Function *F = M.getFunction(name);
  if (F) {
    assert(F->getFunctionType() == FTy && "Wrong prototype for klee intrinsic");
    return std::make_pair(F, false);
  } else {
    F = Function::Create(FTy, Function::ExternalLinkage, name, &M);
    return std::make_pair(F, true);
  }
}

/// Ensure that the module has a definition for _klee_use_freq function
bool UseFrequencyAnalyzerPass::doInitialization(llvm::CallGraph &CG) {
//bool UseFrequencyAnalyzerPass::doInitialization(llvm::Module &M) {
  Module &M = CG.getModule();
  LLVMContext &Ctx = M.getContext();

  assert(m_targetData);

  bool changed = false;
  std::pair<Function*, bool> res1 = insertFunctionDecl(M, "_klee_use_freq",
                            FunctionType::get(Type::getVoidTy(Ctx), true));
  m_kleeUseFreqFunc = res1.first;
  if (res1.second) {
    CG.getOrInsertFunction(m_kleeUseFreqFunc);
    changed = true;
  }

  /*
  std::pair<Function*, bool> res2 = insertFunctionDecl(M, "_klee_use_freq_total",
                            FunctionType::get(Type::getVoidTy(Ctx), true));
  m_kleeUseFreqTotalFunc = res2.first;
  changed |= res2.second;
  */

  return changed;
}

typedef DenseMap<Value*, uint64_t> HotValueDeps;

static bool isIgnored(Value *hotValueDep) {
  if (ConstantExpr *C = dyn_cast<ConstantExpr>(hotValueDep)) {
    if (C->getOpcode() == Instruction::GetElementPtr) {
      StringRef name = C->getOperand(0)->getName();
      if (name == "__pdata" || name == "__net" /* || name == "_stdio_streams"*/)
        return true;
    }
  } else {
    StringRef name = hotValueDep->getName();
    if (name == "__environ" || name == "__exit_slots" ||
        name == "__exit_count" || name == "__exit_cleanup" ||
        name == "__exit_function_table")
      return true;
  }
  return false;
}

/// Traverse data flow dependencies of hotValue, gathering its dependencies
/// on values read from known memory location (only global variables for now).
/// Return a set of pointers to such memory locations.
static void gatherHotValueDeps(Value* hotValue, HotValueDeps *deps) {
  if (isa<Constant>(hotValue))
    return; // We are not interested in constants

  // Traverse the data flow for the value, limiting the depth
  DenseSet<Value*> visitedOPs;
  SmallVector<std::pair<Value*, unsigned>, 16>
      visitOPStack(1, std::make_pair(hotValue, 0));

  // Limited-depth data flow traversal
  while (!visitOPStack.empty()) {
    Value *V = visitOPStack.back().first;
    unsigned depth = visitOPStack.back().second;
    visitOPStack.pop_back();

    if (!visitedOPs.insert(V).second)
      continue;

    if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
      Value *Ptr = LI->getPointerOperand();
      if (isa<Constant>(Ptr) /*|| isa<Argument>(Ptr)*/) {
        if (!isIgnored(Ptr))
          deps->insert(std::make_pair(Ptr, 1));
        continue;
      }
    }

    User *U = dyn_cast<User>(V);
    if (!U || depth >= MAX_DATAFLOW_DEPTH ||
        isa<LoadInst>(V) || isa<Constant>(V) || isa<PHINode>(V)) {
      // Don't go through memory loads, don't consider operands to constants
      // Also, don't go through PHI nodes for now.
      continue;
    }

    // Explore operands of V
    for (User::op_iterator it = U->op_begin(), ie = U->op_end(); it != ie; ++it)
      if (!isa<Constant>(*it)) // No constants please
        visitOPStack.push_back(std::make_pair<Value*, unsigned>(*it, depth+1));
  }
}

static void gatherCallSiteDeps(CallSite CS, HotValueDeps *deps,
                               Function *kleeUseFreqFunc) {
  Function *F = CS.getCalledFunction();
  if (!F) {
    ConstantExpr *CE = dyn_cast<ConstantExpr>(CS.getCalledValue());
    if (!CE || CE->getOpcode() != Instruction::BitCast)
      return;
    F = dyn_cast<Function>(CE->getOperand(0));
    if (!F)
      return;
  }

  if (F->isDeclaration())
    return;

  foreach (Instruction &I, F->getEntryBlock()) {
    CallInst *CI = dyn_cast<CallInst>(&I);
    if (!CI || CI->getCalledFunction() != kleeUseFreqFunc)
      continue;
    if (isa<Constant>(CI->getArgOperand(0))) {
        deps->insert(std::make_pair(CI->getArgOperand(0),
                    cast<ConstantInt>(CI->getArgOperand(2))->getZExtValue()));
    }
  }

}

/// Estimate execution count of an instruction inside loop
static uint64_t estimateExecCountInLoop(Loop *ILoop) {
  uint64_t useCount = 1;

  // If the value is used in a loop, multiply useCount by number of iterations
  for (Loop *L = ILoop; L; L = L->getParentLoop()) {
    if (unsigned tripCount = L->getSmallConstantTripCount())
      useCount *= tripCount;
    else
      useCount *= DEFAULT_LOOP_TRIP_COUNT; // XXX: made-up heuristic
  }

  return useCount;
}

/// Check if a given basic block already contains annotation for ptr. Otherwise,
/// if the block contains any other annotations, fill in the totalUseCountAnn
/// value with the total use count arg of the last annotation (XXX: ugly).
static bool isBlockAlreadyAnnotated(BasicBlock *BB, Value *ptr,
                                    Function *kleeUseFreqFunc) {
  for (BasicBlock::iterator it = BB->getFirstNonPHI(); ; ++it) {
    CallInst *CI = dyn_cast<CallInst>(it);
    if (!CI || CI->getCalledFunction() != kleeUseFreqFunc)
      break; // All block annotations are always at the begining of the BB

    if (CI->getArgOperand(0) == ptr)
      return true;
  }
  return false;
}

static bool isBlockAlreadyAnnotated(BasicBlock *BB, Value *ptr,
                                    Function *kleeUseFreqFunc,
                                    uint64_t *totalUseCountAnn) {
  for (BasicBlock::iterator it = BB->getFirstNonPHI(); ; ++it) {
    CallInst *CI = dyn_cast<CallInst>(it);
    if (!CI || CI->getCalledFunction() != kleeUseFreqFunc)
      break; // All block annotations are always at the begining of the BB

    Value *totalCount = CI->getArgOperand(3);
    assert(isa<ConstantInt>(totalCount));
    *totalUseCountAnn = cast<ConstantInt>(totalCount)->getZExtValue();

    if (CI->getArgOperand(0) == ptr)
      return true;
  }
  return false;
}

static Constant* getInt64Const(LLVMContext &Ctx, uint64_t value) {
  return ConstantInt::get(Type::getInt64Ty(Ctx), value);
}

/// Add use count annotation telling how many times the value pointer by ptr
/// will be used after execution useInst. If useInst is in a loop (pointed by
/// useInstLoop), annotation is added just after exiting from the loop the
/// top-level loop in which useInstLoop is contained, otherwise annotation
/// is added just after useInst.
static std::vector<CallInst*> addUseCountAnnotation(
                                    Instruction *useInst, Loop *useInstLoop,
                                    Value *ptr, uint64_t useCountAfterInst,
                                    uint64_t totalUseCountAfterInst,
                                    Function *kleeUseFreqFunc, TargetData *TD) {
  assert(isa<Constant>(ptr) || isa<Argument>(ptr)); // just in case...

  assert(isa<PointerType>(ptr->getType()));
  const Type* ptrValTy = cast<PointerType>(ptr->getType())->getElementType();

  std::vector<CallInst*> result;

  LLVMContext &Ctx = kleeUseFreqFunc->getContext();
  Value *annotationArgs[4];
  annotationArgs[0] = ptr;
  annotationArgs[1] = getInt64Const(Ctx, TD->getTypeSizeInBits(ptrValTy));
  annotationArgs[2] = getInt64Const(Ctx, useCountAfterInst);
  annotationArgs[3] = getInt64Const(Ctx, totalUseCountAfterInst);

  // Find top level loop
  Loop *topLevelLoop = 0;
  for (; useInstLoop; useInstLoop = useInstLoop->getParentLoop())
    topLevelLoop = useInstLoop;

  if (topLevelLoop) {
    // If we are in a loop, the value may still be used on next iteration,
    // so the annotations should be inserted on loop exits

    SmallVector<BasicBlock*, 8> exitBlocks;
    topLevelLoop->getExitBlocks(exitBlocks);
    for (unsigned i = 0, ie = exitBlocks.size(); i != ie; ++i) {
      // The exitBlocks can contain duplicates, we should filter them away
      // Usually the number of exitBlocks is so small that using a set
      // is an overkill - O(n^2) with small constant factor will do better
      unsigned j = 0;
      for (unsigned j = 0; j < i; ++j) {
        if (exitBlocks[j] == exitBlocks[i])
          break;
      }
      if (j != i)
        continue;

      BasicBlock *exitBB = exitBlocks[i];

      // The exitBB may already contain annotations inserted due to another
      // hot instructions with the same value in the same loop. If so, don't
      // bother inserting more annotations (as we go backwards, the first
      // inserted annotation will be the correct one).
      uint64_t tcAfter = ~0ULL;
      if (!isBlockAlreadyAnnotated(exitBB, ptr, kleeUseFreqFunc, &tcAfter)) {
        if (tcAfter == ~0ULL)
          tcAfter = totalUseCountAfterInst;
        annotationArgs[3] = getInt64Const(Ctx, tcAfter);
        result.push_back(CallInst::Create(kleeUseFreqFunc,
                              annotationArgs, annotationArgs+4, "",
                              exitBB->getFirstNonPHI()));
      }
    }
  } else if (isa<TerminatorInst>(useInst)) {
    // The is the last instruction in a BB - we should annotate successord
    BasicBlock *BB = useInst->getParent();
    for (succ_iterator it = succ_begin(BB), ie = succ_end(BB); it != ie; ++it) {
      BasicBlock *succBB = *it;
      uint64_t tcAfter = ~0ULL;
      if (!isBlockAlreadyAnnotated(succBB, ptr, kleeUseFreqFunc, &tcAfter)) {
        if (tcAfter == ~0ULL)
          tcAfter = totalUseCountAfterInst;
        annotationArgs[3] = getInt64Const(Ctx, tcAfter);
        result.push_back(CallInst::Create(kleeUseFreqFunc,
                              annotationArgs, annotationArgs+4, "",
                              succBB->getFirstNonPHI()));
      }
    }
  } else {
    // If not in loop and not a terminator, insert annotation just after
    // the current instruction
    result.push_back(CallInst::Create(kleeUseFreqFunc,
                        annotationArgs, annotationArgs+4, "",
                        ++BasicBlock::iterator(useInst)));
  }

  return result;
}

static void annotateBlock(Value *ptr, uint64_t useCount, uint64_t totalUseCount,
                          BasicBlock *BB, CallGraphNode *kleeUseFreqCG,
                          CallGraphNode *callerCG, TargetData *TD) {
  LLVMContext &Ctx = ptr->getContext();
  const Type* ptrValTy = cast<PointerType>(ptr->getType())->getElementType();
  Value *args[4] = { ptr, getInt64Const(Ctx, TD->getTypeSizeInBits(ptrValTy)),
      getInt64Const(Ctx, useCount), getInt64Const(Ctx, totalUseCount) };
  CallInst *CI = CallInst::Create(kleeUseFreqCG->getFunction(), args, args+4,
                                  "", BB->getFirstNonPHI());
  callerCG->addCalledFunction(CallSite(CI), kleeUseFreqCG);
}

static void annotateInst(Value *ptr, uint64_t useCount, uint64_t totalUseCount,
                         Instruction *inst, CallGraphNode *kleeUseFreqCG,
                         CallGraphNode *callerCG, TargetData *TD) {
  LLVMContext &Ctx = ptr->getContext();
  const Type* ptrValTy = cast<PointerType>(ptr->getType())->getElementType();
  Value *args[4] = { ptr, getInt64Const(Ctx, TD->getTypeSizeInBits(ptrValTy)),
      getInt64Const(Ctx, useCount), getInt64Const(Ctx, totalUseCount) };
  CallInst *CI = CallInst::Create(kleeUseFreqCG->getFunction(), args, args+4,
                                  "", ++BasicBlock::iterator(inst));
  callerCG->addCalledFunction(CallSite(CI), kleeUseFreqCG);
}

bool UseFrequencyAnalyzerPass::runOnFunction(llvm::CallGraphNode &CGNode) {
  Function &F = *CGNode.getFunction();
  BasicBlock *entryBB = &F.getEntryBlock();

  LoopInfo &loopInfo = getAnalysis<LoopInfo>(F);
  CallGraph &callGraph = getAnalysis<CallGraph>();
  CallGraphNode *kleeUseFreqCG = callGraph[m_kleeUseFreqFunc];

  // Use count after BB for all hot values
  typedef llvm::ImmutableMap<Value*, uint64_t> UseCountInfo;
  UseCountInfo::Factory useCountInfoFactory;

  // For every BB in a function we store an upper estimate of how many times
  // each hot value is used after the first instruction in that BB
  typedef llvm::DenseMap<BasicBlock*, UseCountInfo> UseCountMap;
  UseCountMap useCountMap;

  // Total use count after the first instruction in each BB
  typedef llvm::DenseMap<BasicBlock*, uint64_t> TotalUseCountMap;
  TotalUseCountMap totalUseCountMap;

  // Post-order traversal
  for (po_iterator<BasicBlock*> poIt = po_begin(entryBB),
                                poE  = po_end(entryBB); poIt != poE; ++poIt) {
    BasicBlock *BB = *poIt;

    Loop *bbLoop = loopInfo.getLoopFor(BB);
    Loop *topLevelLoop = 0;
    uint64_t bbExecCount = 1;

    for (Loop *L = bbLoop; L; topLevelLoop = L, L = L->getParentLoop()) {
      if (unsigned tripCount = L->getSmallConstantTripCount())
        bbExecCount *= tripCount;
      else
        bbExecCount *= DEFAULT_LOOP_TRIP_COUNT; // XXX: made-up heuristic
    }

    UseCountInfo &bbUseCountInfo = useCountMap.insert(
          std::make_pair(BB, useCountInfoFactory.getEmptyMap())).first->second;
    uint64_t &totalUseCount =
        totalUseCountMap.insert(std::make_pair(BB, 0)).first->second;

    bool succDiff = false;

    // Initially set useCountInfo for the current BB to be a maximum of
    // useCountInfo for all BB's successors
    for (succ_iterator succIt = succ_begin(BB),
                       succE  = succ_end(BB); succIt != succE; ++succIt) {

      UseCountMap::iterator bbUCIt = useCountMap.find(*succIt);
      if (bbUCIt == useCountMap.end())
        continue; // This is a back edge, skip it for now

      UseCountInfo &succUseCountInfo = bbUCIt->second;
      if (bbUseCountInfo.isEmpty()) {
        // Copy initial info from the first successor
        bbUseCountInfo = succUseCountInfo;
        totalUseCount = totalUseCountMap.lookup(*succIt);
      } else if (bbUseCountInfo.getRootWithoutRetain() !=
                 succUseCountInfo.getRootWithoutRetain()) {
        succDiff = true;
        // If any further successor has different info, lets combine it
        foreach (UseCountInfo::value_type &p, succUseCountInfo) {
          const uint64_t *count = bbUseCountInfo.lookup(p.first);
          if (!count || *count < p.second) {
            bbUseCountInfo = useCountInfoFactory.add(
                  bbUseCountInfo, p.first, p.second);
            if (count)
              totalUseCount -= *count;
            totalUseCount += p.second;
          }
        }
      }
    }

    // If use count differs on branch edges, we should add annotations
    if (succDiff) {
      for (succ_iterator succIt = succ_begin(BB),
                         succE  = succ_end(BB); succIt != succE; ++succIt) {
        BasicBlock *succBB = *succIt;
        UseCountInfo &succUseCountInfo = useCountMap.find(succBB)->second;
        uint64_t totalSuccUseCount = totalUseCountMap.lookup(succBB);

        foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
          if (isBlockAlreadyAnnotated(succBB, p.first, m_kleeUseFreqFunc))
            continue;

          const uint64_t *succUseCount = succUseCountInfo.lookup(p.first);
          if (!succUseCount || *succUseCount != p.second) {
            annotateBlock(p.first, succUseCount ? *succUseCount : 0,
                          totalSuccUseCount, succBB, kleeUseFreqCG,
                          &CGNode, m_targetData);
          }
        }
      }
    }

    // Go through BB instructions in reverse order, updating useCountInfo
    for (BasicBlock::InstListType::reverse_iterator
            rIt = BB->getInstList().rbegin(), rE = BB->getInstList().rend();
            rIt != rE; ++rIt) {
      Instruction *I = &*rIt;

      // NOTE: we don't do any path-sensitive analysis, moreover we don't
      // track values in registers. To avoid to much false negatives, we
      // intentionally do not track stores that may overwrite hot values

      typedef DenseMap<Value*, uint64_t> HotValueDeps;
      HotValueDeps hotValueDeps;

      // Check whether a symbolic operand to the current instruction may
      // force KLEE to call the solver...
      if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
        if (BI->isConditional())
          gatherHotValueDeps(BI->getCondition(), &hotValueDeps);
      } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        gatherHotValueDeps(LI->getPointerOperand(), &hotValueDeps);
      } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        gatherHotValueDeps(SI->getPointerOperand(), &hotValueDeps);
      } else if (IndirectBrInst *BI = dyn_cast<IndirectBrInst>(I)) {
        gatherHotValueDeps(BI->getAddress(), &hotValueDeps);
      } else if (SwitchInst *SI = dyn_cast<SwitchInst>(I)) {
        gatherHotValueDeps(SI->getCondition(), &hotValueDeps);
      } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
        gatherCallSiteDeps(CallSite(CI), &hotValueDeps, m_kleeUseFreqFunc);
      } else if (InvokeInst *CI = dyn_cast<InvokeInst>(I)) {
        gatherCallSiteDeps(CallSite(CI), &hotValueDeps, m_kleeUseFreqFunc);
      }

      uint64_t totalUseCountAfter = totalUseCount;

      // Go through all the deps
      foreach (HotValueDeps::value_type &p, hotValueDeps) {
        Value *ptr = p.first;

        uint64_t oldUseCount = 0;
        if (const uint64_t *oldUseCountPtr = bbUseCountInfo.lookup(ptr))
          oldUseCount = *oldUseCountPtr;

        uint64_t numUses = p.second * bbExecCount;
        totalUseCount += numUses;

        bbUseCountInfo = useCountInfoFactory.add(
              bbUseCountInfo, ptr, oldUseCount + numUses);

        if (bbLoop || I == BB->getTerminator()) {
          SmallPtrSet<BasicBlock*, 8> blocksToAnnotate;
          if (bbLoop) {
            SmallVector<BasicBlock*, 8> exitBlocks;
            topLevelLoop->getExitBlocks(exitBlocks);
            foreach (BasicBlock* exitBB, exitBlocks)
              blocksToAnnotate.insert(exitBB);
          } else {
            for (succ_iterator it = succ_begin(BB),
                               ie = succ_end(BB); it != ie; ++it)
              blocksToAnnotate.insert(*it);
          }

          foreach (BasicBlock *aBB, blocksToAnnotate) {
            if (isBlockAlreadyAnnotated(aBB, ptr, m_kleeUseFreqFunc))
              continue;

            UseCountMap::iterator bbUCIt = useCountMap.find(aBB);
            if (bbUCIt == useCountMap.end())
              continue; // This is a back edge, skip it for now

            const uint64_t *oldSuccUseCount = bbUCIt->second.lookup(ptr);
            uint64_t totalSuccUseCount = totalUseCountMap.lookup(aBB);

            annotateBlock(ptr, oldSuccUseCount ? *oldSuccUseCount : 0,
                  totalSuccUseCount, aBB, kleeUseFreqCG, &CGNode, m_targetData);

            /*
            const Type* ptrValTy =
                cast<PointerType>(ptr->getType())->getElementType();
            Value *args[4] = { ptr,
                getInt64Const(Ctx, m_targetData->getTypeSizeInBits(ptrValTy)),
                getInt64Const(Ctx, oldSuccUseCount ? *oldSuccUseCount : 0),
                getInt64Const(Ctx, totalSuccUseCount) };
            CallInst *CI =
                CallInst::Create(m_kleeUseFreqFunc, args, args+4, "",
                                 aBB->getFirstNonPHI());
            CGNode.addCalledFunction(CallSite(CI), kleeUseFreqCG);
            */
          }
        } else {
          // Annotate current instruction
          annotateInst(ptr, oldUseCount, totalUseCountAfter, I,
                       kleeUseFreqCG, &CGNode, m_targetData);
          /*
          const Type* ptrValTy =
              cast<PointerType>(ptr->getType())->getElementType();
          Value *args[4] = { ptr,
              getInt64Const(Ctx, m_targetData->getTypeSizeInBits(ptrValTy)),
              getInt64Const(Ctx, oldUseCount),
              getInt64Const(Ctx, totalUseCountAfter) };
          CallInst *CI =
              CallInst::Create(m_kleeUseFreqFunc, args, args+4, "", rIt.base());
          CGNode.addCalledFunction(CallSite(CI), kleeUseFreqCG);
          */
          ++rIt;
        }
      }
    }

#if 0
    // Dump it
    std::cerr << "UseCountInfo for BB: " << BB->getParent()->getNameStr()
              << ":" << BB->getNameStr() << std::endl;
    foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
      p.first->dump();
      std::cerr << " = " << p.second << std::endl;
    }
    std::cerr << "  total = " << totalUseCount << std::endl;
#endif

  }

  // Now output the annotation for the function entry block
  UseCountInfo &useCountInfo = useCountMap.find(entryBB)->second;
  uint64_t totalUseCount = totalUseCountMap.lookup(entryBB);
  foreach (UseCountInfo::value_type &p, useCountInfo) {
    annotateBlock(p.first, p.second, totalUseCount, entryBB,
                  kleeUseFreqCG, &CGNode, m_targetData);
  }

  return true;
}

/// Annotate function with a frequency of use information
bool UseFrequencyAnalyzerPass::runOnFunction1(llvm::CallGraphNode &CGNode) {
  Function &F = *CGNode.getFunction();
  LoopInfo &loopInfo = getAnalysis<LoopInfo>(F);
  CallGraph &callGraph = getAnalysis<CallGraph>();
  CallGraphNode *kleeUseFreqCG = callGraph[m_kleeUseFreqFunc];

  bool changed = false;

  LLVMContext &Ctx = F.getContext();
  BasicBlock *entryBB = &F.getEntryBlock();

  typedef DenseMap<Value*, uint64_t> HotValuesMap;

  // Data structures for a post-order traversal of the CFG
  DenseSet<BasicBlock*> visitedBBs;
  std::vector<std::pair<BasicBlock*, succ_iterator> > visitBBStack(1,
        std::make_pair(entryBB, succ_begin(entryBB)));

  // Keep track of maximum number of uses of hot values for each level of the
  // visitedBBStack (hotValuesStack.size() always equals to visitBBStack.size())
  std::vector<HotValuesMap> hotValuesStack;
  hotValuesStack.reserve(256); // Reserve some space, as copying is expensive
  hotValuesStack.push_back(HotValuesMap());

  // Do the traversal
  while (!visitBBStack.empty()) {

    // While there are still children to visit
    while (visitBBStack.back().second != succ_end(visitBBStack.back().first)) {
      BasicBlock *BB = *visitBBStack.back().second++;
      if (visitedBBs.insert(BB).second) {
        // If the block is not visited, go down to its children
        visitBBStack.push_back(std::make_pair(BB, succ_begin(BB)));
        hotValuesStack.push_back(HotValuesMap());
      }
    }

    BasicBlock *BB = visitBBStack.back().first;
    HotValuesMap &hotValuesMap = hotValuesStack.back();

    Loop *BBLoop = loopInfo.getLoopFor(BB);
    uint64_t BBExecCount = estimateExecCountInLoop(BBLoop);

    uint64_t totalUseCount = 0;
    foreach (HotValuesMap::value_type &p, hotValuesMap)
      totalUseCount += p.second;

    // Go backwards through the BB
    for (BasicBlock::InstListType::reverse_iterator
            rIt = BB->getInstList().rbegin(), rE = BB->getInstList().rend();
            rIt != rE; ++rIt) {
      Instruction *I = &*rIt;

      // NOTE: we don't do any path-sensitive analysis, moreover we don't
      // track values in registers. To avoid to much false negatives, we
      // intentionally do not track stores that may overwrite hot values

      Value* hotValue = 0;

      typedef DenseMap<Value*, uint64_t> HotValueDeps;
      HotValueDeps hotValueDeps;

      // Check whether a symbolic operand to the current instruction may
      // force KLEE to call the solver...
      if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
        if (BI->isConditional())
          hotValue = BI->getCondition();
      } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        hotValue = LI->getPointerOperand();
      } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        hotValue = SI->getPointerOperand();
      } else if (IndirectBrInst *BI = dyn_cast<IndirectBrInst>(I)) {
        hotValue = BI->getAddress();
      } else if (SwitchInst *SI = dyn_cast<SwitchInst>(I)) {
        hotValue = SI->getCondition();
      } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
        gatherCallSiteDeps(CallSite(CI), &hotValueDeps, m_kleeUseFreqFunc);
      } else if (InvokeInst *CI = dyn_cast<InvokeInst>(I)) {
        gatherCallSiteDeps(CallSite(CI), &hotValueDeps, m_kleeUseFreqFunc);
      }

      if (BB->getName() == "bb8.i.i655") {
        std::cerr << std::endl;
      }

      if (BB->getName() == "bb5") {
        std::cerr << std::endl;
      }

      // Traverse the data flow for the value (with limited depth)
      if (hotValue && !isa<Constant>(hotValue))
        gatherHotValueDeps(hotValue, &hotValueDeps);

      // Insert klee annotation specifying how may times the value
      // is used after this instruction.
      foreach (HotValueDeps::value_type &p, hotValueDeps) {
        Value *ptr = p.first;
        unsigned useCount = p.second;

        HotValuesMap::iterator hvIt =
            hotValuesMap.insert(std::make_pair(ptr, 0u)).first;
        std::vector<CallInst*> annotations =
            addUseCountAnnotation(I, BBLoop, ptr, hvIt->second, totalUseCount,
                                  m_kleeUseFreqFunc, m_targetData);
        foreach (CallInst *CI, annotations) {
          CGNode.addCalledFunction(CallSite(CI), kleeUseFreqCG);
          changed = true;
        }
        // Update iterator. Remember that reverse_iterator internally stores
        // a pointer to the next element (in usual order) after the one
        // that its operator*() returns.
        if (annotations.size() == 1 && annotations[0] == &*rIt)
          ++rIt;

        // Compute new use count
        hvIt->second += BBExecCount*useCount;
        totalUseCount += BBExecCount*useCount;
      }
    }

    if (hotValuesStack.size() > 1) {
      // Merge hotValuesMap from this block to its predecesor
      HotValuesMap &predHotValuesMap = hotValuesStack[hotValuesStack.size()-2];

      foreach (HotValuesMap::value_type &p, hotValuesMap) {
        HotValuesMap::iterator hvIt =
            predHotValuesMap.insert(std::make_pair(p.first, 0)).first;
        if (hvIt->second < p.second)
          hvIt->second = p.second;
      }

      hotValuesStack.pop_back();
    }

    visitBBStack.pop_back();
  }

  uint64_t totalUseCount = 0;
  foreach (HotValuesMap::value_type &p, hotValuesStack.back())
    totalUseCount += p.second;

  // Insert initial annotations
  foreach (HotValuesMap::value_type &p, hotValuesStack.back()) {
    Value *ptr = p.first;

    assert(isa<PointerType>(ptr->getType()));
    const Type* ptrValTy = cast<PointerType>(ptr->getType())->getElementType();

    Value *annotationArgs[4];
    annotationArgs[0] = ptr;
    annotationArgs[1] =
        getInt64Const(Ctx, m_targetData->getTypeSizeInBits(ptrValTy));
    annotationArgs[2] = getInt64Const(Ctx, p.second);
    annotationArgs[3] = getInt64Const(Ctx, totalUseCount);
    CallInst *CI = CallInst::Create(m_kleeUseFreqFunc,
                                    annotationArgs, annotationArgs+4,
                                    "", entryBB->getFirstNonPHI());
    CGNode.addCalledFunction(CallSite(CI), kleeUseFreqCG);
    changed = true;
  }

  return changed;
}

void UseFrequencyAnalyzerPass::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.addRequired<CallGraph>();
  Info.addRequired<LoopInfo>();
  Info.addPreserved<CallGraph>();
  Info.setPreservesCFG();
}

char UseFrequencyAnalyzerPass::ID = 0;

INITIALIZE_PASS_BEGIN(UseFrequencyAnalyzerPass, "use-frequency-analyzer",
                    "Analyze use frequency of program variables", false, false)
INITIALIZE_AG_DEPENDENCY(CallGraph)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_END(UseFrequencyAnalyzerPass, "use-frequency-analyzer",
                    "Analyze use frequency of program variables", false, false)
