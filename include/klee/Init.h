/*
 * Init.h
 *
 *  Created on: Sep 2, 2010
 *      Author: stefan
 */

#ifndef INIT_H_
#define INIT_H_

namespace llvm {
class Module;
}

namespace klee {
void externalsAndGlobalsCheck(const llvm::Module *m);
llvm::Module* loadByteCode();
llvm::Module* prepareModule(llvm::Module *module);
void readProgramArguments(int &pArgc, char **&pArgv, char **&pEnvp, char **envp);
}


#endif /* INIT_H_ */
