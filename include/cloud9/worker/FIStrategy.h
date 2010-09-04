/*
 * FIStrategy.h
 *
 *  Created on: Sep 4, 2010
 *      Author: stefan
 */

#ifndef FISTRATEGY_H_
#define FISTRATEGY_H_

#include "cloud9/worker/CoreStrategies.h"

namespace cloud9 {

namespace worker {

class FIStrategy: public BasicStrategy {
private:
  std::map<SymbolicState*, unsigned> fiCounters;    // Fault injection counters
public:
  FIStrategy();
  virtual ~FIStrategy();
};

}

}

#endif /* FISTRATEGY_H_ */
