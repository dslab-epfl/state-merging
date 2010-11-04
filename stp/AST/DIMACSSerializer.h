/*
 * DIMACSSerializer.h
 *
 *  Created on: May 18, 2010
 *      Author: stefan
 */

#ifndef DIMACSSERIALIZER_H_
#define DIMACSSERIALIZER_H_

#include <vector>
#include <ostream>

namespace cloud9 {

class DIMACSSerializer {
public:
  typedef int Var;

  typedef std::pair<Var, bool> Lit;
  typedef std::vector<Lit> Clause;

private:
  std::vector<Clause> clauses;
  unsigned int varCount;
  std::string description;

public:
  void addClause(Clause &clause);
  void setVarCount(unsigned int count) { varCount = count; }
  void setDescription(std::string desc) { description = desc; }

  void serialize(std::ostream &os);
  void serialize(std::string fileName);

  DIMACSSerializer() : description("SAT Expression") { }

  DIMACSSerializer(std::string desc) : description(desc) { }

  virtual ~DIMACSSerializer() { }
};

}

#endif /* DIMACSSERIALIZER_H_ */
