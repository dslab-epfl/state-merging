/*
 * SymbolicEngine.h
 *
 *  Created on: Dec 23, 2009
 *      Author: stefan
 */

#ifndef SYMBOLICENGINE_H_
#define SYMBOLICENGINE_H_

namespace klee {
class ExecutionState;
}

namespace llvm {
class Function;
}

namespace cloud9 {

namespace worker {


class SymbolicEngine {
public:
	SymbolicEngine() {};
	virtual ~SymbolicEngine() {};

	virtual klee::ExecutionState *initRootState(llvm::Function *f, int argc,
			char **argv, char **envp) = 0;
};

}

}

#endif /* SYMBOLICENGINE_H_ */
