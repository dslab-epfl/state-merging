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

#include <iostream>

#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH

#warning XXX: try making the following much larger
#define MAX_DATAFLOW_DEPTH 24

#define DEFAULT_LOOP_TRIP_COUNT 10

#define BWIDTH 256
#define USE_QC_MAX

namespace llvm {
  void initializeUseFrequencyAnalyzerPassPass(PassRegistry&);
}

using namespace llvm;
using namespace klee;

namespace {
  cl::opt<double>
  QcAddThreshold("qc-add-threshold", cl::init(0.00001));

  cl::opt<uint64_t>
  QcAddAbsThreshold("qc-add-abs-threshold", cl::init(0));

  cl::opt<bool>
  QcAnnotateAll("qc-annotate-all", cl::init(false));
}

UseFrequencyAnalyzerPass::UseFrequencyAnalyzerPass(TargetData *TD)
      : CallGraphSCCPass(ID), m_targetData(TD),
        m_kleeUseFreqFunc(0), m_kleeTotalUseFreqFunc(0) {
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
  {
    std::pair<Function*, bool> res =
        insertFunctionDecl(M, "_klee_use_freq_dbg",
                           FunctionType::get(Type::getVoidTy(Ctx), true));
    m_kleeUseFreqFunc = res.first;
    if (res.second) {
      CG.getOrInsertFunction(m_kleeUseFreqFunc);
      changed = true;
    }
  }

  {
    std::pair<Function*, bool> res =
        insertFunctionDecl(M, "_klee_total_use_freq_dbg",
                           FunctionType::get(Type::getVoidTy(Ctx), true));
    m_kleeTotalUseFreqFunc = res.first;
    if (res.second) {
      CG.getOrInsertFunction(m_kleeTotalUseFreqFunc);
      changed = true;
    }
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

enum HotValueKind { HVVal, HVPtr };
typedef PointerIntPair<const llvm::Value*, 1, HotValueKind> HotValueBaseTy;

class HotValue: public HotValueBaseTy {
public:
  HotValue(HotValueKind K, const llvm::Value *V): PointerIntPair(V, K) {}

  const llvm::Value *getValue() const { return getPointer(); }
  HotValueKind getKind() const { return getInt(); }

  bool isVal() const { return getInt() == HVVal; }
  bool isPtr() const { return getInt() == HVPtr; }

  // An easy way to support DenseMap
  HotValue(const HotValueBaseTy& P): PointerIntPair(P) {}
  operator HotValueBaseTy () {
    return HotValueBaseTy::getFromOpaqueValue(getOpaqueValue());
  }

};

namespace llvm {
  template<> struct FoldingSetTrait<HotValue> {
    static inline void Profile(const HotValue hv, FoldingSetNodeID& ID) {
      ID.AddPointer(hv.getOpaqueValue());
    }
  };
  template<> struct DenseMapInfo<HotValue>:
    public DenseMapInfo<PointerIntPair<const llvm::Value*, 1, HotValueKind> > {
  };
}

typedef DenseMap<HotValue, APInt> HotValueDeps;
typedef llvm::SmallPtrSet<const BasicBlock*, 256> VisitedBBs;

/// Traverse data flow dependencies of hotValue, gathering its dependencies
/// on values read from known memory location (only global variables for now).
/// Return a set of pointers to such memory locations.
static void gatherHotValueDeps(const Value* hotValueUse, HotValueDeps *deps,
                               const VisitedBBs &visitedBBs,
                               APInt numUsesMult = APInt(BWIDTH, 1)) {
  if (isa<Constant>(hotValueUse) || isa<MDNode>(hotValueUse))
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

    } else if (const PHINode *PHI = dyn_cast<PHINode>(V)) {
      deps->insert(std::make_pair(HotValue(HVVal, V), numUsesMult));
#warning Try commenting/uncommenting the following
      if (depth >= MAX_DATAFLOW_DEPTH)
        continue;
      for (unsigned i = 0, e = PHI->getNumIncomingValues(); i != e; ++i) {
        Value *arg = PHI->getIncomingValue(i);
        if (isa<Constant>(arg) || isa<MDNode>(arg))
          continue;
        //if (visitedBBs.count(PHI->getIncomingBlock(i)) != 0)
        //  continue; // Do not go through back edges
        visitOPStack.push_back(std::make_pair(arg, depth+1));
      }
      continue;

    } else if (const Instruction *U = dyn_cast<Instruction>(V)) {
      if (depth >= MAX_DATAFLOW_DEPTH)
        continue;

      // We do not put intermidiate values into the hot value set, however,
      // we track all their dependencies instead.

      // Explore operands of V
      for (User::const_op_iterator it = U->op_begin(),
           ie = U->op_end(); it != ie; ++it) {
        const Value *OP = *it;
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

  const MDNode *MD = F->getEntryBlock().begin()->getMetadata("qc");
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

#if 0
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
#endif
}

#if 0
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

static MDNode* insertAnnotation(HotValue hv, uint64_t useCount,
                    uint64_t totalUseCount, CallGraphNode *funcCG,
                    CallGraphNode *callerCG, Instruction *insertBefore) {
  LLVMContext &Ctx = insertBefore->getContext();
  Value *args[4] = { getI64Const(Ctx, hv.first), const_cast<Value*>(hv.second),
      getI64Const(Ctx, useCount), getI64Const(Ctx, totalUseCount) };

  CallInst *CI = isa<Constant>(hv.second) ?
    CallInst::Create(funcCG->getFunction(), args, args+4, Twine(), insertBefore) :
    CallInst::Create(funcCG->getFunction(), Twine(), insertBefore);
  callerCG->addCalledFunction(CallSite(CI), funcCG);

  MDNode* mdNode = MDNode::get(Ctx, args, 4);
  CI->setMetadata("uf", mdNode);

  return mdNode;
}
#endif

typedef llvm::ImmutableMap<HotValue, APInt> UseCountInfo;

static bool _annotationComparator(Value *l, Value *r) {
  return cast<ConstantInt>(cast<MDNode>(l)->getOperand(2))->getValue().ugt(
            cast<ConstantInt>(cast<MDNode>(r)->getOperand(2))->getValue());
}

static void addAnnotationQC(Instruction *I, APInt totalUseCount,
                            const UseCountInfo& useCountInfo,
                            const char* mdName = "qc") {
  LLVMContext &Ctx = I->getContext();
  assert(I->getMetadata("qc") == NULL);
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

#if 1
/* Compute query count information for a function described by CGNode,
   annotate the function accordingly. */
bool UseFrequencyAnalyzerPass::runOnFunction(CallGraphNode &CGNode) {
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
    APInt bbExecCount(BWIDTH, 1);

    const Loop *topLevelLoop = 0;
    const Loop *bbLoop = loopInfo.getLoopFor(BB);
    for (const Loop *L = bbLoop; L; topLevelLoop = L, L = L->getParentLoop()) {
      unsigned tripCount = L->getSmallConstantTripCount();
      if (!tripCount)
        tripCount = DEFAULT_LOOP_TRIP_COUNT;
      bbExecCount *= APInt(BWIDTH, tripCount);
    }

    // Initially set useCountInfo for the current BB to be a maximum of
    // useCountInfo for all BB's successors
    UseCountInfo &bbUseCountInfo = useCountMap.insert(
          std::make_pair(BB, useCountInfoFactory.getEmptyMap())).first->second;
    APInt &bbTotalUseCount = totalUseCountMap.insert(
          std::make_pair(BB, APInt(BWIDTH, 0))).first->second;

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
                p.second + (count ? *count : APInt(BWIDTH, 0)));
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

      // NOTE: we don't do any path-sensitive analysis, moreover we don't
      // track values in registers. To avoid to much false negatives, we
      // intentionally do not track stores that may overwrite hot values

      HotValueDeps hotValueDeps;
      APInt instTotalUseCount(BWIDTH, 0);

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
              p.second * bbExecCount + (count ? *count : APInt(BWIDTH, 0)));
      }
      bbTotalUseCount += instTotalUseCount * bbExecCount;

#if 0
      // Annotate instruction if it modifies use count and is not in a loop
      if (instTotalUseCount && !bbLoop && I != BB->begin()) {
#error: this is totally wrong. What is important is UC after the instruction!
        addAnnotation(I, bbTotalUseCount, bbUseCountInfo);
      }
#endif
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
                                                     p.first, APInt(BWIDTH, 0));
      }
    }

#if 0
    // Annotate the block if it is not in a loop, or is a header of a loop
    if (!topLevelLoop || topLevelLoop->getHeader() == BB) {
      // Compute V^{h,add} set
      double addThreshold = bbTotalUseCount.roundToDouble() * QcAddThreshold;
      UseCountInfo Vh = useCountInfoFactory.getEmptyMap();
      foreach (UseCountInfo::value_type &p, bbUseCountInfo) {
        if (p.second.roundToDouble() > addThreshold)
          Vh = useCountInfoFactory.add(Vh, p.first, p.second);
      }

      if (!Vh.isEmpty())
        addAnnotationQC(BB->begin(), bbTotalUseCount, Vh, "vh");
    }
#endif

#if 0
    if (BB == &F.getEntryBlock() ||
        (QcAnnotateAll && (!topLevelLoop || topLevelLoop->getHeader() == BB))) {
      addAnnotationQC(BB->begin(), bbTotalUseCount, bbUseCountInfo);
    }
#endif
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
                                    p.first, count ? *count : APInt(BWIDTH, 0));
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

#else
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
  for (po_iterator<BasicBlock*> poIt = po_begin(entryBB),
                                poE  = po_end(entryBB); poIt != poE; ++poIt) {
    BasicBlock *BB = *poIt;
    visitedBBs.insert(BB);

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

        if (loopInfo.getLoopFor(succBB))
#warning What if we decrepemnt use count here or in the header ?
          continue; // Do not annotate blocks inside loops

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

    llvm::SmallPtrSet<const Instruction*, 32> visitedBBInsts;

    // Go through BB instructions in reverse order, updating useCountInfo
    for (BasicBlock::InstListType::reverse_iterator
            rIt = BB->getInstList().rbegin(), rE = BB->getInstList().rend();
            rIt != rE; ++rIt) {
      Instruction *I = &*rIt;
      visitedBBInsts.insert(I);

      {
        Instruction *IA = rIt.base();
        if (isa<PHINode>(IA))
          IA = BB->getFirstNonPHI();

        // If I allocates a pointer that is a hotvalue
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

        // If I defines the hotvalue
        hv = HotValue(HVVal, I);
        if (const uint64_t *useCount = bbUseCountInfo.lookup(hv)) {
          MDNode *mdNode = insertAnnotation(hv, *useCount,
                           totalUseCount, kleeUseFreqCG, &CGNode, IA);
          if (IA == &*rIt.base())
            ++rIt;

          // XXX: special case to avoid delay between the assignment of a local
          // variable and an invokation of klee_use_freq
          I->setMetadata("ul", mdNode);

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

        if (const Instruction *hvI = dyn_cast<Instruction>(hv.second)) {
          if (visitedBBs.count(hvI->getParent()) > 0) {
            if (hvI->getParent() != BB || visitedBBInsts.count(hvI) > 0) {
              // If hvI was already visited...
              // This means we are in a loop. We still need to annotate it.

              assert(I != hvI);

              Instruction *IA = const_cast<Instruction*>(hvI);
              if (isa<PHINode>(IA))
                IA = IA->getParent()->getFirstNonPHI();

              MDNode *mdNode =
                  insertAnnotation(hv, oldUseCount + numUses, totalUseCountAfter,
                                   kleeUseFreqCG, &CGNode, IA);
              if (IA == &*rIt.base())
                ++rIt;

              if (hv.first == HVVal)
                const_cast<Instruction*>(hvI)->setMetadata("ul", mdNode);
            }
          }
        }

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
    if (isa<Instruction>(p.first.second))
      continue; // XXX: this is wrong - solve by proper handling of PHI deps
    insertAnnotation(p.first, p.second, totalUseCount,
                     kleeUseFreqCG, &CGNode, entryBB->getFirstNonPHI());
    /*
    annotateBlock(p.first, p.second, totalUseCount, entryBB,
                  kleeUseFreqPtrCG, kleeUseFreqValCG, &CGNode, m_targetData, &DT);
                  */
  }

  return true;
}
#endif

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
