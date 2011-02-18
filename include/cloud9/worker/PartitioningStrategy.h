/*
 * PartitioningStrategy.h
 *
 *  Created on: Feb 18, 2011
 *      Author: stefan
 */

#ifndef PARTITIONINGSTRATEGY_H_
#define PARTITIONINGSTRATEGY_H_

#include "cloud9/worker/CoreStrategies.h"

#include <vector>
#include <set>

namespace cloud9 {

namespace worker {

class StatePartition {
private:
  JobSelectionStrategy *strategy;
public:
  StatePartition() { }
};

class PartitioningStrategy: public BasicStrategy {
public:
  PartitioningStrategy();
  virtual ~PartitioningStrategy();
};

}

}

#endif /* PARTITIONINGSTRATEGY_H_ */
