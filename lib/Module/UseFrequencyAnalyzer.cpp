#include "Passes.h"

#include "llvm/Module.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Target/TargetData.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
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

static Constant* getI64Const(LLVMContext &Ctx, uint64_t value) {
  return ConstantInt::get(Type::getInt64Ty(Ctx), value);
}

UseFrequencyAnalyzerPass::UseFrequencyAnalyzerPass(TargetData *TD)
      : CallGraphSCCPass(ID), m_targetData(TD), m_kleeUseFreqFunc(0) {
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
  std::pair<Function*, bool> res2 = insertFunctionDecl(M, "_klee_use_freq_val",
                            FunctionType::get(Type::getVoidTy(Ctx), true));
  m_kleeUseFreqValFunc = res2.first;
  if (res2.second) {
    CG.getOrInsertFunction(m_kleeUseFreqValFunc);
    changed = true;
  }
  */

  return changed;
}

static bool isIgnored(const Value *hotValueDep) {
  if (const ConstantExpr *C = dyn_cast<ConstantExpr>(hotValueDep)) {
    if (C->getOpcode() == Instruction::GetElementPtr) {
      StringRef name = C->getOperand(0)->getName();
      if (name == "__pdata" || name == "__net" || name == "__fs"
          /* || name == "_stdio_streams"*/)
        return true;
    }
  } else {
    StringRef name = hotValueDep->getName();
    if (name == "__environ" || name == "__exit_slots" ||
        name == "__exit_count" || name == "__exit_cleanup" ||
        name == "__exit_function_table" ||
        name == "stdout" || name == "stderr" || name == "__ctype_b")
      return true;
  }
  return false;
}

enum HotValueKind { HVVal, HVPtr };
typedef std::pair<unsigned, const Value*> HotValue;

namespace llvm {
  template<> struct FoldingSetTrait<HotValue> {
    static inline void Profile(const HotValue hv, FoldingSetNodeID& ID) {
      ID.AddInteger(hv.first); ID.AddPointer(hv.second);
    }
  };
}

typedef DenseMap<HotValue, uint64_t> HotValueDeps;
typedef llvm::SmallPtrSet<const BasicBlock*, 256> VisitedBBs;

/// Traverse data flow dependencies of hotValue, gathering its dependencies
/// on values read from known memory location (only global variables for now).
/// Return a set of pointers to such memory locations.
static void gatherHotValueDeps(const Value* hotValueUse, HotValueDeps *deps,
                               const VisitedBBs &visitedBBs,
                               uint64_t numUsesMult = 1) {
  if (isa<Constant>(hotValueUse))
    return; // We are not interested in constants

  // Traverse the data flow for the value, limiting the depth
  DenseSet<const Value*> visitedOPs;
  SmallVector<std::pair<const Value*, unsigned>, 16>
      visitOPStack(1, std::make_pair(hotValueUse, 0));

  // Limited-depth data flow traversal
  while (!visitOPStack.empty()) {
    const Value *V = visitOPStack.back().first;
    unsigned depth = visitOPStack.back().second;
    visitOPStack.pop_back();

    if (!visitedOPs.insert(V).second)
      continue;

    if (const LoadInst *LI = dyn_cast<LoadInst>(V)) {
      const Value *Ptr = LI->getPointerOperand();
      if (isa<Constant>(Ptr)) {
        if (!isIgnored(Ptr))
          deps->insert(std::make_pair(HotValue(HVPtr, Ptr), numUsesMult));
      } else if (isa<AllocaInst>(Ptr) || isa<Argument>(Ptr)) {
        deps->insert(std::make_pair(HotValue(HVPtr, Ptr), numUsesMult));
      } /*else if (const CallInst *CI = dyn_cast<CallInst>(Ptr)) {
        if (CI->getCalledFunction()->getName() == "malloc")
          deps->insert(std::make_pair(HotValue(HVPtr, Ptr), numUsesMult));
      }*/
      continue;

    } else if (const PHINode *PHI = dyn_cast<PHINode>(V)) {
      deps->insert(std::make_pair(HotValue(HVVal, V), numUsesMult));
#warning Try uncommenting the following
      /*
      if (depth >= MAX_DATAFLOW_DEPTH)
        continue;
      for (unsigned i = 0, e = PHI->getNumIncomingValues(); i != e; ++i) {
        Value *arg = PHI->getIncomingValue(i);
        if (isa<Constant>(arg))
          continue;
        if (visitedBBs.count(PHI->getIncomingBlock(i)) != 0)
          continue; // Do not go through back edges
        visitOPStack.push_back(std::make_pair(arg, depth+1));
      }
      */
      continue;

    } else if (isa<Argument>(V)) {
      deps->insert(std::make_pair(HotValue(HVVal, V), numUsesMult));
      continue;

    } else if (const Instruction *U = dyn_cast<Instruction>(V)) {
      if (depth >= MAX_DATAFLOW_DEPTH)
        continue;

      // Explore operands of V
      for (User::const_op_iterator it = U->op_begin(),
           ie = U->op_end(); it != ie; ++it) {
        const Value *OP = *it;
        if (isa<Constant>(OP)) // No constants please
          continue;
        visitOPStack.push_back(std::make_pair(OP, depth+1));
      }
    }
  }
}

static void gatherCallSiteDeps(const CallSite CS, HotValueDeps *deps,
                               const VisitedBBs &visitedBBs,
                               const Function *kleeUseFreqFunc) {
  const Function *F = CS.getCalledFunction();
  if (!F) {
    const ConstantExpr *CE = dyn_cast<ConstantExpr>(CS.getCalledValue());
    if (!CE || CE->getOpcode() != Instruction::BitCast)
      return;
    F = dyn_cast<Function>(CE->getOperand(0));
    if (!F)
      return;
  }

  if (F->isDeclaration())
    return;

  foreach (const Instruction &I, F->getEntryBlock()) {
    const CallInst *CI = dyn_cast<CallInst>(&I);
    if (!CI || CI->getCalledFunction() != kleeUseFreqFunc)
      break;
    const MDNode *md = CI->getMetadata("uf");
    assert(md && md->getNumOperands() == 4);
    uint64_t k = cast<ConstantInt>(md->getOperand(0))->getZExtValue();
    Value* v = md->getOperand(1);
    uint64_t numUses = cast<ConstantInt>(md->getOperand(2))->getZExtValue();
    if (isa<Constant>(v)) {
      deps->insert(std::make_pair(HotValue(k, v), numUses));
    } else if (Argument* arg = dyn_cast<Argument>(v)) {
      Value *argV = CS.getArgument(arg->getArgNo());
      gatherHotValueDeps(argV, deps, visitedBBs, numUses);
      //deps->insert(std::make_pair(HotValue(k, argV), numUses));
    } else {
      assert(0);
    }
  }

}

/// Check if a given basic block already contains annotation for HotValue.
static bool isBlockAlreadyAnnotated(const BasicBlock *BB, HotValue hv,
                                    const Function *kleeUseFreqFunc) {
  for (BasicBlock::const_iterator it = BB->getFirstNonPHI(); ; ++it) {
    const CallInst *CI = dyn_cast<CallInst>(it);
    if (!CI || CI->getCalledFunction() != kleeUseFreqFunc)
      break;

    const MDNode *md = CI->getMetadata("uf");
    assert(md && md->getNumOperands() == 4);
    uint64_t k = cast<ConstantInt>(md->getOperand(0))->getZExtValue();

    if (hv.first == k && hv.second == md->getOperand(1))
      return true;
  }
  return false;
}

static void insertAnnotation(HotValue hv, uint64_t useCount,
                    uint64_t totalUseCount, CallGraphNode *funcCG,
                    CallGraphNode *callerCG, Instruction *insertBefore) {
  LLVMContext &Ctx = insertBefore->getContext();
  Value *args[4] = { getI64Const(Ctx, hv.first), const_cast<Value*>(hv.second),
      getI64Const(Ctx, useCount), getI64Const(Ctx, totalUseCount) };

  CallInst *CI = isa<Constant>(hv.second) ?
    CallInst::Create(funcCG->getFunction(), args, args+4, Twine(), insertBefore) :
    CallInst::Create(funcCG->getFunction(), Twine(), insertBefore);
  callerCG->addCalledFunction(CallSite(CI), funcCG);

  CI->setMetadata("uf", MDNode::get(Ctx, args, 4));
}

bool UseFrequencyAnalyzerPass::runOnFunction(CallGraphNode &CGNode) {
  Function &F = *CGNode.getFunction();
  BasicBlock *entryBB = &F.getEntryBlock();

  LoopInfo &loopInfo = getAnalysis<LoopInfo>(F);
  CallGraph &callGraph = getAnalysis<CallGraph>();
  CallGraphNode *kleeUseFreqCG = callGraph[m_kleeUseFreqFunc];

  LLVMContext &Ctx = F.getContext();

  //DominatorTree &DT = getAnalysis<DominatorTree>(F);

  // Use count after BB for all hot values
  typedef llvm::ImmutableMap<HotValue, uint64_t> UseCountInfo;
  UseCountInfo::Factory useCountInfoFactory;

  // For every BB in a function we store an upper estimate of how many times
  // each hot value is used after the first instruction in that BB
  typedef llvm::DenseMap<const BasicBlock*, UseCountInfo> UseCountMap;
  UseCountMap useCountMap;

  // Total use count after the first instruction in each BB
  typedef llvm::DenseMap<const BasicBlock*, uint64_t> TotalUseCountMap;
  TotalUseCountMap totalUseCountMap;

  // Visited blocks (to track backedges)
  VisitedBBs visitedBBs;

  // Post-order traversal
  for (po_ext_iterator<BasicBlock*, VisitedBBs> poIt =
                                    po_ext_begin(entryBB, visitedBBs),
             poE  = po_ext_end(entryBB, visitedBBs); poIt != poE; ++poIt) {
    BasicBlock *BB = *poIt;

    const Loop *bbLoop = loopInfo.getLoopFor(BB);
    const Loop *topLevelLoop = 0;
    uint64_t bbExecCount = 1;

    for (const Loop *L = bbLoop; L; topLevelLoop = L, L = L->getParentLoop()) {
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
        //totalUseCount = totalUseCountMap.lookup(*succIt);
      } else if (bbUseCountInfo.getRootWithoutRetain() !=
                 succUseCountInfo.getRootWithoutRetain()) {
        succDiff = true;
        // If any further successor has different info, lets combine it
        foreach (UseCountInfo::value_type &p, succUseCountInfo) {
          const uint64_t *count = bbUseCountInfo.lookup(p.first);
          if (!count || *count < p.second) {
            bbUseCountInfo = useCountInfoFactory.add(
                  bbUseCountInfo, p.first, p.second);
            //if (count)
            //  totalUseCount -= *count;
            //totalUseCount += p.second;
          }
        }
      }

      uint64_t succTotalUseCount = totalUseCountMap.lookup(*succIt);
      if (succTotalUseCount > totalUseCount)
        totalUseCount = succTotalUseCount;
    }

    // If use count differs on branch edges, we should add annotations
    if (succDiff) {
      for (succ_iterator succIt = succ_begin(BB),
                         succE  = succ_end(BB); succIt != succE; ++succIt) {
        BasicBlock *succBB = *succIt;

        UseCountMap::iterator bbUCIt = useCountMap.find(succBB);
        if (bbUCIt == useCountMap.end())
          continue; // This is a back edge, skip it for now

        UseCountInfo &succUseCountInfo = bbUCIt->second;
        uint64_t totalSuccUseCount = totalUseCountMap.lookup(succBB);

        foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
          if (isBlockAlreadyAnnotated(succBB, p.first, m_kleeUseFreqFunc))
            continue;

          const uint64_t *succUseCount = succUseCountInfo.lookup(p.first);
          if (!succUseCount || *succUseCount != p.second) {
            insertAnnotation(p.first, succUseCount ? *succUseCount : 0,
                             totalSuccUseCount, kleeUseFreqCG, &CGNode,
                             succBB->getFirstNonPHI());
            /*
            annotateBlock(p.first, succUseCount ? *succUseCount : 0,
                          totalSuccUseCount, succBB,
                          kleeUseFreqPtrCG, kleeUseFreqValCG,
                          &CGNode, m_targetData, &DT);
                          */
          }
        }
      }
    }

    // Go through BB instructions in reverse order, updating useCountInfo
    for (BasicBlock::InstListType::reverse_iterator
            rIt = BB->getInstList().rbegin(), rE = BB->getInstList().rend();
            rIt != rE; ++rIt) {
      Instruction *I = &*rIt;

      {
        Instruction *IA = rIt.base();
        if (isa<PHINode>(IA))
          IA = BB->getFirstNonPHI();
        /*
        BasicBlock::iterator insertAfter(I);
        if (isa<PHINode>(I))
          while (isa<PHINode>(llvm::next(insertAfter))) ++insertAfter;
          */

        HotValue hv(HVPtr, I);
        if (const uint64_t *useCount = bbUseCountInfo.lookup(hv)) {
          insertAnnotation(hv, *useCount,
                           totalUseCount, kleeUseFreqCG, &CGNode, IA);
          if (IA == &*rIt.base())
            ++rIt;
          /*
          annotateInst(hv, *useCount, totalUseCount, insertAfter,
                       kleeUseFreqPtrCG, kleeUseFreqValCG, &CGNode, m_targetData);
                       */
          bbUseCountInfo = useCountInfoFactory.remove(bbUseCountInfo, hv);
        }

        hv = HotValue(HVVal, I);
        if (const uint64_t *useCount = bbUseCountInfo.lookup(hv)) {
          insertAnnotation(hv, *useCount,
                           totalUseCount, kleeUseFreqCG, &CGNode, IA);
          if (IA == &*rIt.base())
            ++rIt;
          /*
          annotateInst(hv, *useCount, totalUseCount, insertAfter,
                       kleeUseFreqPtrCG, kleeUseFreqValCG, &CGNode, m_targetData);
                       */
          bbUseCountInfo = useCountInfoFactory.remove(bbUseCountInfo, hv);
        }
      }

      // NOTE: we don't do any path-sensitive analysis, moreover we don't
      // track values in registers. To avoid to much false negatives, we
      // intentionally do not track stores that may overwrite hot values

      HotValueDeps hotValueDeps;

      // Check whether a symbolic operand to the current instruction may
      // force KLEE to call the solver...
      if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
        if (BI->isConditional())
          gatherHotValueDeps(BI->getCondition(), &hotValueDeps, visitedBBs);
      } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        gatherHotValueDeps(LI->getPointerOperand(), &hotValueDeps, visitedBBs);
      } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        gatherHotValueDeps(SI->getPointerOperand(), &hotValueDeps, visitedBBs);
      } else if (IndirectBrInst *BI = dyn_cast<IndirectBrInst>(I)) {
        gatherHotValueDeps(BI->getAddress(), &hotValueDeps, visitedBBs);
      } else if (SwitchInst *SI = dyn_cast<SwitchInst>(I)) {
        gatherHotValueDeps(SI->getCondition(), &hotValueDeps, visitedBBs);
      } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
        gatherCallSiteDeps(CallSite(CI), &hotValueDeps,
                           visitedBBs, m_kleeUseFreqFunc);
      } else if (InvokeInst *CI = dyn_cast<InvokeInst>(I)) {
        gatherCallSiteDeps(CallSite(CI), &hotValueDeps,
                           visitedBBs, m_kleeUseFreqFunc);
      }

      SmallVector<Value*, 8> mdArgs;
      mdArgs.push_back(getI64Const(Ctx, bbExecCount));

      uint64_t totalUseCountAfter = totalUseCount;

      // Go through all the deps
      foreach (HotValueDeps::value_type &p, hotValueDeps) {
        HotValue hv = p.first;

        {
          Value *mdItem[3] = { getI64Const(Ctx, hv.first),
                               const_cast<Value*>(hv.second),
                               getI64Const(Ctx, p.second) };
          mdArgs.push_back(MDNode::get(Ctx, mdItem, 3));
        }

        uint64_t oldUseCount = 0;
        if (const uint64_t *oldUseCountPtr = bbUseCountInfo.lookup(hv))
          oldUseCount = *oldUseCountPtr;

        uint64_t numUses = p.second * bbExecCount;
        totalUseCount += numUses;

        bbUseCountInfo = useCountInfoFactory.add(
              bbUseCountInfo, hv, oldUseCount + numUses);

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
            if (isBlockAlreadyAnnotated(aBB, hv, m_kleeUseFreqFunc))
              continue;

            UseCountMap::iterator bbUCIt = useCountMap.find(aBB);
            if (bbUCIt == useCountMap.end())
              continue; // This is a back edge, skip it for now

            const uint64_t *oldSuccUseCount = bbUCIt->second.lookup(hv);
            uint64_t totalSuccUseCount = totalUseCountMap.lookup(aBB);

            insertAnnotation(hv, oldSuccUseCount ? *oldSuccUseCount : 0,
                             totalSuccUseCount, kleeUseFreqCG, &CGNode,
                             aBB->getFirstNonPHI());
            /*
            annotateBlock(hv, oldSuccUseCount ? *oldSuccUseCount : 0,
                  totalSuccUseCount, aBB, kleeUseFreqPtrCG, kleeUseFreqValCG,
                  &CGNode, m_targetData, &DT);
                  */

          }
        } else {
          // Annotate current instruction

          insertAnnotation(hv, oldUseCount, totalUseCountAfter,
                           kleeUseFreqCG, &CGNode, rIt.base());
          /*
          annotateInst(hv, oldUseCount, totalUseCountAfter, I,
                       kleeUseFreqPtrCG, kleeUseFreqValCG, &CGNode, m_targetData);
          */
          ++rIt;
        }
      }

      //I->setMetadata("uf_dbg", MDNode::get(Ctx, ArrayRef<Value*>(mdArgs)));
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
    insertAnnotation(p.first, p.second, totalUseCount,
                     kleeUseFreqCG, &CGNode, entryBB->getFirstNonPHI());
    /*
    annotateBlock(p.first, p.second, totalUseCount, entryBB,
                  kleeUseFreqPtrCG, kleeUseFreqValCG, &CGNode, m_targetData, &DT);
                  */
  }

  return true;
}

void UseFrequencyAnalyzerPass::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.addRequired<CallGraph>();
  Info.addRequired<DominatorTree>();
  Info.addRequired<LoopInfo>();
  Info.addPreserved<CallGraph>();
  Info.setPreservesCFG();
}

char UseFrequencyAnalyzerPass::ID = 0;

INITIALIZE_PASS_BEGIN(UseFrequencyAnalyzerPass, "use-frequency-analyzer",
                    "Analyze use frequency of program variables", false, false)
INITIALIZE_AG_DEPENDENCY(CallGraph)
INITIALIZE_PASS_DEPENDENCY(DominatorTree)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_END(UseFrequencyAnalyzerPass, "use-frequency-analyzer",
                    "Analyze use frequency of program variables", false, false)
