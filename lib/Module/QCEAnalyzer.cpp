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
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/Support/DataFlow.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/CommandLine.h"

#include "klee/Internal/Module/QCE.h"

#include <iostream>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#warning XXX: try making the following much larger
#define MAX_DATAFLOW_DEPTH 0

#define DEFAULT_LOOP_TRIP_COUNT 10
#define USE_QC_MAX

namespace llvm {
  void initializeQCEAnalyzerPassPass(PassRegistry&);
}

using namespace llvm;
using namespace klee;

typedef DenseMap<HotValue, APInt> HotValueDeps;
typedef llvm::SmallPtrSet<const BasicBlock*, 256> VisitedBBs;

typedef llvm::ImmutableMap<HotValue, APInt> UseCountInfo;

QCEAnalyzerPass::QCEAnalyzerPass(TargetData *TD)
      : CallGraphSCCPass(ID) {
  initializeQCEAnalyzerPassPass(*PassRegistry::getPassRegistry());
}

bool QCEAnalyzerPass::runOnSCC(CallGraphSCC &SCC) {
  bool changed = false;

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

static bool isIgnored(const Value *hotValueDep) {
  if (const ConstantExpr *C = dyn_cast<ConstantExpr>(hotValueDep)) {
    if (C->getOpcode() == Instruction::BitCast) {
      hotValueDep = C->getOperand(0);
    }
  }

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
        name == "stdout" || name == "stderr" || name == "__ctype_b" ||
        name == "__ctype_toupper" || name == "__ctype_tolower" ||
        name == "__rtld_fini" || name == "__app_fini")
      return true;
  }
  return false;
}

/// Traverse data flow dependencies of hotValue, gathering its dependencies
/// on values read from known memory location (only global variables for now).
/// Return a set of pointers to such memory locations.
static void gatherHotValueDeps(Value* hotValueUse, HotValueDeps *deps,
                               const VisitedBBs &visitedBBs,
                               APInt numUsesMult = APInt(QCE_BWIDTH, 1)) {
  if (isa<Constant>(hotValueUse) || isa<MDNode>(hotValueUse))
    return; // We are not interested in constants

  // Traverse the data flow for the value, limiting the depth
  DenseSet<Value*> visitedOPs;
  SmallVector<std::pair<Value*, unsigned>, 16>
      visitOPStack(1, std::make_pair(hotValueUse, 0));

  // Limited-depth data flow traversal
  while (!visitOPStack.empty()) {
    Value *V = visitOPStack.back().first;
    unsigned depth = visitOPStack.back().second;
    visitOPStack.pop_back();

    if (!visitedOPs.insert(V).second)
      continue;

    if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
      Value *Ptr = LI->getPointerOperand();
      if (isa<Constant>(Ptr)) {
        if (!isIgnored(Ptr))
          deps->insert(std::make_pair(HotValue(HVPtr, Ptr), numUsesMult));
      } else if (isa<AllocaInst>(Ptr) || isa<Argument>(Ptr)) {
        deps->insert(std::make_pair(HotValue(HVPtr, Ptr), numUsesMult));
      }
#warning XXX: why is this commented out ?
      /*else if (const CallInst *CI = dyn_cast<CallInst>(Ptr)) {
        if (CI->getCalledFunction()->getName() == "malloc")
          deps->insert(std::make_pair(HotValue(HVPtr, Ptr), numUsesMult));
      }*/
      continue;

    } else if (isa<Argument>(V)) {
      deps->insert(std::make_pair(HotValue(HVVal, V), numUsesMult));
      continue;

    } else if (PHINode *PHI = dyn_cast<PHINode>(V)) {
      deps->insert(std::make_pair(HotValue(HVVal, V), numUsesMult));
#warning Try commenting/uncommenting the following
#if MAX_DATAFLOW_DEPTH > 0
      if (depth >= MAX_DATAFLOW_DEPTH)
        continue;
#endif
      for (unsigned i = 0, e = PHI->getNumIncomingValues(); i != e; ++i) {
        Value *arg = PHI->getIncomingValue(i);
        if (isa<Constant>(arg) || isa<MDNode>(arg))
          continue;
        //if (visitedBBs.count(PHI->getIncomingBlock(i)) != 0)
        //  continue; // Do not go through back edges
        visitOPStack.push_back(std::make_pair(arg, depth+1));
      }
      continue;

    } else if (Instruction *U = dyn_cast<Instruction>(V)) {
#if MAX_DATAFLOW_DEPTH > 0
      if (depth >= MAX_DATAFLOW_DEPTH)
        continue;
#endif

      // We do not put intermidiate values into the hot value set, however,
      // we track all their dependencies instead.

      // Explore operands of V
      for (User::op_iterator it = U->op_begin(),
           ie = U->op_end(); it != ie; ++it) {
        Value *OP = *it;
        if (isa<Constant>(OP) || isa<MDNode>(OP)) // No constants please
          continue;
        visitOPStack.push_back(std::make_pair(OP, depth+1));
      }
    }
  }
}

static void gatherCallSiteDeps(const CallSite CS,
                               APInt *totalUses, HotValueDeps *deps,
                               const VisitedBBs &visitedBBs) {
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

  const MDNode *MD = F->getEntryBlock().begin()->getMetadata("qce");
  if (!MD)
    return; // XXX

  assert(MD && MD->getNumOperands() > 0);

  *totalUses = cast<ConstantInt>(MD->getOperand(0))->getValue();

  for (unsigned i = 1; i < MD->getNumOperands(); ++i) {
    const MDNode *HMD = cast<const MDNode>(MD->getOperand(i));
    HotValue HV = HotValue(
          HotValueKind(cast<ConstantInt>(HMD->getOperand(0))->getZExtValue()),
          HMD->getOperand(1));
    APInt numUses = cast<ConstantInt>(HMD->getOperand(2))->getValue();

    if (HV.isVal()) {
      if (const Argument* A = dyn_cast<Argument>(HV.getValue())) {
        Value *V = CS.getArgument(A->getArgNo());
        gatherHotValueDeps(V, deps, visitedBBs, numUses);
      } else {
#warning XXX
        //assert(0);
      }
    } else {
      if (isa<Constant>(HV.getValue())) {
        deps->insert(std::make_pair(HV, numUses));
      } else {
#warning XXX
        //assert(0);
      }
    }
  }
}

static bool _annotationComparator(Value *l, Value *r) {
  return cast<ConstantInt>(cast<MDNode>(l)->getOperand(2))->getValue().ugt(
            cast<ConstantInt>(cast<MDNode>(r)->getOperand(2))->getValue());
}

static void addAnnotationQC(Instruction *I, APInt totalUseCount,
                            const UseCountInfo& useCountInfo,
                            const char* mdName = "qce") {
  LLVMContext &Ctx = I->getContext();
  assert(I->getMetadata("qce") == NULL);
  llvm::SmallVector<Value*, 32> Args;
  Args.push_back(ConstantInt::get(Ctx, totalUseCount));

  foreach (UseCountInfo::value_type &p, useCountInfo) {
    Value *NArgs[3] = { ConstantInt::get(Ctx, APInt(1, p.first.getKind())),
                        const_cast<Value*>(p.first.getValue()),
                        ConstantInt::get(Ctx, p.second) };
    Args.push_back(MDNode::get(Ctx, NArgs, 3));
  }

  std::sort(Args.begin()+1, Args.end(), _annotationComparator);

  I->setMetadata(mdName, MDNode::get(Ctx, Args));
}

/* Compute query count information for a function described by CGNode,
   annotate the function accordingly. */
bool QCEAnalyzerPass::runOnFunction(CallGraphNode &CGNode) {
  Function &F = *CGNode.getFunction();
  BasicBlock *entryBB = &F.getEntryBlock();

  // Split entryBB to have an empty entry block (so that it would contain
  // only function-level annotations)
#warning Does this work correctly with LoopInfo and DominatorTree ?
  entryBB->splitBasicBlock(entryBB->begin());

  LoopInfo &loopInfo = getAnalysis<LoopInfo>(F);
  DominatorTree &DT = getAnalysis<DominatorTree>(F);

  // Use count after BB for all hot values
  UseCountInfo::Factory useCountInfoFactory;

  // For every BB in a function we store an upper estimate of how many times
  // each hot value is used after the first instruction in that BB.
  // Each basic block that is not in a loop is annotated according to this map.
  // For BBs where particular use count becomes zero, we add zero-valued
  // item to this map to put as an annotation later on. For BBs in loops,
  // we add such zero-valued item at the loop exits.
  typedef llvm::DenseMap<const BasicBlock*, UseCountInfo> UseCountMap;
  UseCountMap useCountMap;

  // Total use count after the first instruction in each BB
  typedef llvm::DenseMap<const BasicBlock*, APInt> TotalUseCountMap;
  TotalUseCountMap totalUseCountMap;

  // Visited blocks (to track backedges)
  VisitedBBs visitedBBs;

  // Post-order traversal
  for (po_iterator<BasicBlock*> poIt = po_begin(entryBB),
                                poE  = po_end(entryBB); poIt != poE; ++poIt) {
    BasicBlock *BB = *poIt;
    visitedBBs.insert(BB);

    // Compute estimated number of executions if BB is a part of a loop
    APInt bbExecCount(QCE_BWIDTH, 1);

    const Loop *topLevelLoop = 0;
    const Loop *bbLoop = loopInfo.getLoopFor(BB);
    for (const Loop *L = bbLoop; L; topLevelLoop = L, L = L->getParentLoop()) {
      unsigned tripCount = L->getSmallConstantTripCount();
      if (!tripCount)
        tripCount = DEFAULT_LOOP_TRIP_COUNT;
      bbExecCount *= APInt(QCE_BWIDTH, tripCount);
    }

    // Initially set useCountInfo for the current BB to be a maximum of
    // useCountInfo for all BB's successors
    UseCountInfo &bbUseCountInfo = useCountMap.insert(
          std::make_pair(BB, useCountInfoFactory.getEmptyMap())).first->second;
    APInt &bbTotalUseCount = totalUseCountMap.insert(
          std::make_pair(BB, APInt(QCE_BWIDTH, 0))).first->second;

    for (succ_iterator succIt = succ_begin(BB),
                       succE  = succ_end(BB); succIt != succE; ++succIt) {

      UseCountMap::iterator succUseCountIt = useCountMap.find(*succIt);
      if (succUseCountIt == useCountMap.end())
        continue; // This is a back edge, skip it for now

      UseCountInfo &succUseCountInfo = succUseCountIt->second;
      if (bbUseCountInfo.isEmpty()) {
        // Copy initial info from the first successor
        bbUseCountInfo = succUseCountInfo;

        // Do not propagete zero-valued items
        foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
          if (!p.second)
            bbUseCountInfo = useCountInfoFactory.remove(bbUseCountInfo,
                                                        p.first);
        }
      } else {
        foreach (UseCountInfo::value_type &p, succUseCountInfo) {
          const APInt *count = bbUseCountInfo.lookup(p.first);
#ifdef USE_QC_MAX
          if ((!count || count->ult(p.second)) && !!p.second)
            bbUseCountInfo = useCountInfoFactory.add(
                  bbUseCountInfo, p.first, p.second);
#else
          bbUseCountInfo = useCountInfoFactory.add(
                bbUseCountInfo, p.first,
                p.second + (count ? *count : APInt(QCE_BWIDTH, 0)));
#endif
        }
      }

      const APInt succTotalUseCount = totalUseCountMap.lookup(*succIt);
#ifdef USE_QC_MAX
      if (bbTotalUseCount.ult(succTotalUseCount))
        bbTotalUseCount = succTotalUseCount;
#else
      bbTotalUseCount += succTotalUseCount;
#endif
    }

    if (!bbLoop) {
      // Filter out local vars that are defined strinctly after BB
      foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
        if (const Instruction *I = dyn_cast<Instruction>(p.first.getValue())) {
          if (I->getParent() != BB && DT.dominates(BB, I->getParent()))
            bbUseCountInfo = useCountInfoFactory.remove(bbUseCountInfo, p.first);
        }
      }
    }

    // Go through BB instructions in reverse order, updating useCountInfo
    for (BasicBlock::InstListType::reverse_iterator
            rIt = BB->getInstList().rbegin(), rE = BB->getInstList().rend();
            rIt != rE; ++rIt) {
      Instruction *I = &*rIt;

      assert(!(isa<AllocaInst>(I) && bbLoop));
      assert(!isa<InvokeInst>(I) && "Invoke instruction is not supported!");

      // XXX: hack: annotate call instructions with the state after the call
      if (isa<CallInst>(I) && !bbLoop) {
        addAnnotationQC(llvm::next(BasicBlock::iterator(I)),
                        bbTotalUseCount, bbUseCountInfo);
      }

      // NOTE: we don't do any path-sensitive analysis, moreover we don't
      // track values in registers. To avoid to much false negatives, we
      // intentionally do not track stores that may overwrite hot values

      HotValueDeps hotValueDeps;
      APInt instTotalUseCount(QCE_BWIDTH, 0);

      // Check whether a symbolic operand to the current instruction may
      // force KLEE to call the solver...
      if (CallSite CS = CallSite(I)) {
        gatherCallSiteDeps(CS, &instTotalUseCount, &hotValueDeps, visitedBBs);
      } else {
        Value *V = 0;

        if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
          if (BI->isConditional()) {
            V = BI->getCondition();
          }
        } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
          V = LI->getPointerOperand();
        } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
          V = SI->getPointerOperand();
        } else if (IndirectBrInst *BI = dyn_cast<IndirectBrInst>(I)) {
          V = BI->getAddress();
        } else if (SwitchInst *SI = dyn_cast<SwitchInst>(I)) {
          V = SI->getCondition();
        }

        if (V && !isa<Constant>(V)) {
          gatherHotValueDeps(V, &hotValueDeps, visitedBBs);
          instTotalUseCount = 1;
        }
      }

      foreach (HotValueDeps::value_type &p, hotValueDeps) {
        const APInt *count = bbUseCountInfo.lookup(p.first);
        bbUseCountInfo = useCountInfoFactory.add(
              bbUseCountInfo, p.first,
              p.second * bbExecCount + (count ? *count : APInt(QCE_BWIDTH, 0)));
      }
      bbTotalUseCount += instTotalUseCount * bbExecCount;
    } // end of instructions traversal

    // Add zero-valued items for variables defined in this block
    SmallVector<BasicBlock*, 16> succBBs;
    if (!topLevelLoop) {
      // Quadratic algorithm, but successors are usually only a few
      for (succ_iterator it = succ_begin(BB), ie = succ_end(BB); it!=ie; ++it) {
        if (std::find(succBBs.begin(), succBBs.end(), *it) == succBBs.end())
          succBBs.push_back(*it);
      }
    } else if (BB == topLevelLoop->getHeader()) {
      topLevelLoop->getUniqueExitBlocks(succBBs);
    }

    foreach (BasicBlock *succBB, succBBs) {
      UseCountMap::iterator succUseCountIt = useCountMap.find(succBB);
      if (succUseCountIt == useCountMap.end())
        continue; // backedge?
      UseCountInfo &succUseCountInfo = succUseCountIt->second;
      foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
        const APInt *succCount = succUseCountInfo.lookup(p.first);
        if (!succCount)
          succUseCountInfo = useCountInfoFactory.add(succUseCountInfo,
                                                     p.first, APInt(QCE_BWIDTH, 0));
      }
    }
  } // end of BB traversal

  // Go through blocks again and add annotations where appropriate
  for (Function::iterator fIt = F.begin(), fE = F.end(); fIt != fE; ++fIt) {
    BasicBlock *BB = fIt;

    UseCountMap::iterator bbUseCountIt = useCountMap.find(BB);
    if (bbUseCountIt == useCountMap.end())
      continue; // Unreachable block ?

    const UseCountInfo &bbUseCountInfo = bbUseCountIt->second;
    APInt bbTotalUseCount = totalUseCountMap.find(BB)->second;

    // Do not annotate blocks in loops, except the top-level headers
    Loop *L = loopInfo.getLoopFor(BB);
    if (L) {
      while (L->getParentLoop()) L = L->getParentLoop(); // Get top-level loop
      if (L->getHeader() != BB)
        continue;
    }

    addAnnotationQC(BB->begin(), bbTotalUseCount, bbUseCountInfo);
  }

#if 0
  // Go through blocks again and add annotations where appropriate
  for (po_iterator<BasicBlock*> poIt = po_begin(entryBB),
                                poE  = po_end(entryBB); poIt != poE; ++poIt) {
    BasicBlock *BB = *poIt;

    // Do not annotate blocks in loops, except the headers
    Loop *L = loopInfo.getLoopFor(BB);
    if (L) {
      while (L->getParentLoop()) L = L->getParentLoop();
      if (L->getHeader() != BB)
        continue;
    }

    // Gather a set of predecessors. If a predecessor is in a loop, we replace
    // it with the header of corresponding top-level loop, as this is the last
    // block that contained annotations


    APInt bbTotalUseCount = totalUseCountMap.find(BB)->second;
    const UseCountInfo &bbUseCountInfo = useCountMap.find(BB)->second;

    // Annotate all blocks that have no predecessors
    if (pred_begin(BB) == pred_end(BB)) {
      addAnnotationQC(BB->begin(), bbTotalUseCount, bbUseCountInfo);
      continue;
    }

    // Check whether the block should be annotated
    bool diffTotalUseCount = false;
    UseCountInfo diffUseCount = useCountInfoFactory.getEmptyMap();

    for (pred_iterator predIt = pred_begin(BB), predE = pred_end(BB);
                       predIt != predE; ++predIt) {
      BasicBlock *predBB = *predIt;
      UseCountMap::iterator predUseCountIt = useCountMap.find(predBB);
      if (predUseCountIt == useCountMap.end())
        continue; // XXX?
      const UseCountInfo &predUseCountInfo = predUseCountIt->second;

      // Check whether predUseCount has items that differ,
      // or are not in bbUseCount at all
      foreach (UseCountInfo::value_type &p, predUseCountInfo) {
        const APInt *count = bbUseCountInfo.lookup(p.first);
        if (!count || *count != p.second)
          diffUseCount = useCountInfoFactory.add(diffUseCount,
                                    p.first, count ? *count : APInt(QCE_BWIDTH, 0));
      }

      // Check whether bbUseCount has items that are not in predUseCount
      foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
        const APInt *predCount = predUseCountInfo.lookup(p.first);
        if (!predCount)
          diffUseCount = useCountInfoFactory.add(diffUseCount,
                                                 p.first, p.second);
      }

      // Check whether totalUseCount differs
      if (!diffTotalUseCount &&
          totalUseCountMap.find(BB)->second != bbTotalUseCount)
        diffTotalUseCount = true;
    }

    // Annotate if necessary
    if (diffTotalUseCount || !diffUseCount.isEmpty()) {
      //addAnnotationQC(BB, bbTotalUseCount, diffUseCount);
      addAnnotationQC(BB->begin(), bbTotalUseCount, bbUseCountInfo);
    }
  }
#endif

  return true;
}

void QCEAnalyzerPass::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.addRequired<CallGraph>();
  Info.addRequired<DominatorTree>();
  Info.addRequired<LoopInfo>();
  Info.addPreserved<CallGraph>();
  Info.setPreservesCFG();
}

char QCEAnalyzerPass::ID = 0;

INITIALIZE_PASS_BEGIN(QCEAnalyzerPass, "qce-analyzer",
                    "QCE analysis", false, false)
INITIALIZE_AG_DEPENDENCY(CallGraph)
INITIALIZE_PASS_DEPENDENCY(DominatorTree)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_END(QCEAnalyzerPass, "qce-analyzer",
                    "QCE analysis", false, false)
