/*
 * KleeHandler.h
 *
 *  Created on: Dec 11, 2009
 *      Author: stefan
 */

#ifndef KLEEHANDLER_H_
#define KLEEHANDLER_H_

#include "klee/Interpreter.h"
#include "klee/Internal/ADT/TreeStream.h"

namespace klee {

class KleeHandler : public InterpreterHandler {
private:
  Interpreter *m_interpreter;
  TreeStreamWriter *m_pathWriter, *m_symPathWriter;
  std::ostream *m_infoFile;

  char m_outputDirectory[1024];
  unsigned m_testIndex;  // number of tests written so far
  unsigned m_pathsExplored; // number of paths explored so far

  // used for writing .ktest files
  int m_argc;
  char **m_argv;

  void initCloud9Instrumentation();

public:
  KleeHandler(int argc, char **argv);
  ~KleeHandler();

  std::ostream &getInfoStream() const { return *m_infoFile; }
  unsigned getNumTestCases() { return m_testIndex; }
  unsigned getNumPathsExplored() { return m_pathsExplored; }
  void incPathsExplored() { m_pathsExplored++; }

  void setInterpreter(Interpreter *i);

  void processTestCase(const ExecutionState  &state,
                       const char *errorMessage,
                       const char *errorSuffix);

  std::string getOutputFilename(const std::string &filename);
  std::ostream *openOutputFile(const std::string &filename);
  std::string getTestFilename(const std::string &suffix, unsigned id);

  std::ostream *openTestFile(const std::string &suffix, unsigned id);
  std::ostream *openTestFile(const std::string &suffix) {
    return openTestFile(suffix, m_testIndex+1);
  }

  // load a .out file
  static void loadOutFile(std::string name,
                          std::vector<unsigned char> &buffer);

  // load a .path file
  static void loadPathFile(std::string name,
                           std::vector<bool> &buffer);

  static void getOutFiles(std::string path,
			  std::vector<std::string> &results);
};

}

#endif /* KLEEHANDLER_H_ */
