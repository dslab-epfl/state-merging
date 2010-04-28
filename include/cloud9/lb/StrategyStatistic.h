//===-- StrategyStatistic.h  ----------------------------------------*- C++ -*-===//

#ifndef STRATEGYSTATISTIC_H_
#define STRATEGYSTATISTIC_H_


class StrategyStatistic {
 public:
  uint32_t performance; //received from the worker
  uint32_t allocation; //received from the worker
  bool toChange; // weather or not to change the strategy assignement on the worker
                 //computed by the Load Balancer
  unsigned nrJobsToMove; //number of jobs to assign to this strategy from other strategies
                         //computed by the Load Balancer
};


#endif /* StrategyStatistic.h*/


