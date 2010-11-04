/*
 * DIMACSSerializer.cpp
 *
 *  Created on: May 18, 2010
 *      Author: stefan
 */

#include "DIMACSSerializer.h"

#include <cassert>
#include <fstream>

namespace cloud9 {

void DIMACSSerializer::addClause(Clause &clause) {
  for (Clause::iterator it = clause.begin(); it != clause.end(); it++) {
    assert(it->first > 0);
  }

  clauses.push_back(clause);
}

void DIMACSSerializer::serialize(std::ostream &os) {
  os << "c " << description << std::endl;
  os << "p cnf " << varCount << " " << clauses.size() << std::endl;

  for (std::vector<Clause>::iterator it = clauses.begin(); it != clauses.end(); it++) {
    for (Clause::iterator cit = it->begin(); cit != it->end(); cit++) {
      if (cit->second)
        os << -cit->first << ' ';
      else
        os << cit->first << ' ';
    }
    os << "0" << std::endl;
  }
}

void DIMACSSerializer::serialize(std::string fileName) {
  std::ofstream fs(fileName);

  assert(!fs.fail());

  serialize(fs);
}

}
