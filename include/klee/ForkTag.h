/*
 * ForkTag.h
 *
 *  Created on: Sep 4, 2010
 *      Author: stefan
 */

#ifndef FORKTAG_H_
#define FORKTAG_H_

namespace klee {

class KFunction;

enum ForkClass {
  KLEE_FORK_DEFAULT = 0,
  KLEE_FORK_FAULTINJ = 1,
  KLEE_FORK_SCHEDULE = 2,
  KLEE_FORK_INTERNAL = 3,
  KLEE_FORK_MULTI = 4
};

struct ForkTag {
  ForkClass forkClass;

  // For fault injection
  bool fiVulnerable;

  // The location in the code where the fork was decided (it can be NULL)
  KFunction *location;

  ForkTag(ForkClass _fclass) :
    forkClass(_fclass), fiVulnerable(true), location(0) { }
};

}


#endif /* FORKTAG_H_ */
