//===-- StrategyStatistic.h  ----------------------------------------*- C++ -*-===//

#ifndef STRATEGYSTATISTIC_H_
#define STRATEGYSTATISTIC_H_

#include "cloud9/Common.h"

namespace cloud9 {

namespace lb {

typedef unsigned int strat_id_t;

class StrategyStatistic;

typedef std::map<strat_id_t, StrategyStatistic> strat_stat_map;

class StrategyStatistic {
public:
  uint32_t performance;
  uint32_t allocation;

  //bool toChange; // weather or not to change the strategy assignement on the worker
  //computed by the Load Balancer
  //unsigned nrJobsToMove; //number of jobs to assign to this strategy from other strategies
  //computed by the Load Balancer

  StrategyStatistic() :
    performance(0), allocation(0) {

  }

  static void initPortfolioStat(strat_stat_map &portfolioStat) {
    portfolioStat[RANDOM_PATH_STRATEGY] = StrategyStatistic();
    portfolioStat[WEIGHTED_RANDOM_STRATEGY] = StrategyStatistic();
    portfolioStat[RANDOM_STRATEGY] = StrategyStatistic();
  }
};

}

}

#endif /* StrategyStatistic.h*/

