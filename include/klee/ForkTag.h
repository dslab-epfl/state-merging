/*
 * ForkTag.h
 *
 *  Created on: Sep 4, 2010
 *      Author: stefan
 */

#ifndef FORKTAG_H_
#define FORKTAG_H_

#include "klee/Constants.h"

#include <string>

namespace klee {

struct ForkTag {
  ForkClass forkClass;

  // For fault injection
  bool fiVulnerable;

  // The location in the code where the fork was decided (it can be NULL)
  std::string functionName;

  ForkTag(ForkClass _fclass) :
    forkClass(_fclass), fiVulnerable(false) { }
};

}


#endif /* FORKTAG_H_ */
