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

class PartitioningStrategy;

class StatePartition {
  friend class PartitioningStrategy;
private:
  StateSelectionStrategy *strategy;
  std::set<SymbolicState*> states;
public:
  StatePartition() { }
};

class PartitioningStrategy: public StateSelectionStrategy {
private:
  typedef unsigned int part_id_t;
  std::set<SymbolicState*> states;
public:
  PartitioningStrategy();
  virtual ~PartitioningStrategy();
};

}

}

#endif /* PARTITIONINGSTRATEGY_H_ */
