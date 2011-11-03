#include "klee/Internal/Module/QCE.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Instruction.h"
#include "llvm/BasicBlock.h"
#include "llvm/Function.h"

using namespace llvm;
using namespace klee;

void HotValue::print(llvm::raw_ostream &ostr) const {
  ostr << (isPtr() ? "*  " : "   ");
  if (Instruction *I = dyn_cast<Instruction>(getValue())) {
    Instruction *tmp = I->clone();
    if (I->hasName())
      tmp->setName(I->getName());
    tmp->setMetadata("qce", NULL);
    tmp->print(ostr);
    delete tmp;
    ostr << " (at " << I->getParent()->getParent()->getName() << ")";
  } else if (Argument *A = dyn_cast<Argument>(getValue())) {
    A->print(ostr);
    ostr << " (at " << A->getParent()->getName() << ")";
  } else {
    getValue()->print(ostr);
  }
}

void HotValue::dump() const {
  print(dbgs()); dbgs() << "\n";
}

