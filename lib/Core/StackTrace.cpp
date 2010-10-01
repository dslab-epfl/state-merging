/*
 * StackTrace.cpp
 *
 *  Created on: Oct 1, 2010
 *      Author: stefan
 */

#include "klee/StackTrace.h"

#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"

#include "llvm/Function.h"

#include <iomanip>

using namespace llvm;

namespace klee {

void StackTrace::dump(std::ostream &out) {
  unsigned idx = 0;

  for (stack_t::iterator it = contents.begin(); it != contents.end(); it++) {
    Function *f = it->first.first->function;
    const InstructionInfo &ii = *it->first.second->info;

    out << "\t#" << idx++
        << " " << std::setw(8) << std::setfill('0') << ii.assemblyLine
        << " in " << f->getNameStr() << " (";

    unsigned index = 0;
    for (Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end();
         ai != ae; ++ai) {
      if (ai!=f->arg_begin()) out << ", ";

      out << ai->getNameStr();
      // XXX should go through function
      ref<Expr> value = it->second[index++];
      if (isa<ConstantExpr>(value))
        out << "=" << value;
    }
    out << ")";
    if (ii.file != "")
      out << " at " << ii.file << ":" << ii.line;
    out << "\n";
  }
}

}
