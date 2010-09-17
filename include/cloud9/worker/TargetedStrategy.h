/*
 * TargetedStrategy.h
 *
 *  Created on: Sep 17, 2010
 *      Author: stefan
 */

#ifndef TARGETEDSTRATEGY_H_
#define TARGETEDSTRATEGY_H_

#include "cloud9/worker/CoreStrategies.h"
#include "klee/ForkTag.h"

#include <set>
#include <vector>

namespace cloud9 {

namespace worker {

class TargetedStrategy: public BasicStrategy {
public:
  typedef std::set<std::string> interests_t;
private:
  typedef std::map<SymbolicState*, unsigned> state_set_t;
  typedef std::vector<SymbolicState*> state_vector_t;

  typedef std::pair<state_set_t, state_vector_t> state_container_t;

  WorkerTree *workerTree;

  state_container_t interestingStates;
  state_container_t uninterestingStates;

  interests_t localInterests;

  char adoptionRate;

  SymbolicState *selectRandom(state_container_t &container);
  void insertState(SymbolicState *state, state_container_t &container);
  void removeState(SymbolicState *state, state_container_t &container);

  bool isInteresting(klee::ForkTag forkTag, interests_t &interests);
  bool isInteresting(SymbolicState *state, interests_t &interests);
  void adoptStates();

  unsigned int selectForExport(state_container_t &container,
      interests_t &interests, std::vector<SymbolicState*> &states,
      unsigned int maxCount);
public:
  TargetedStrategy(WorkerTree *_workerTree);
  virtual ~TargetedStrategy() { }

  virtual ExecutionJob* onNextJobSelection();

  virtual void onStateActivated(SymbolicState *state);
  virtual void onStateUpdated(SymbolicState *state, WorkerTree::Node *oldNode);
  virtual void onStateDeactivated(SymbolicState *state);

  unsigned getInterestingCount() const { return interestingStates.first.size(); }
  unsigned getUninterestingCount() const { return uninterestingStates.first.size(); }

  void updateInterests(interests_t &interests);
  unsigned int selectForExport(interests_t &interests,
      std::vector<SymbolicState*> &states, unsigned int maxCount);

  static interests_t anything;
};

}

}

#endif /* TARGETEDSTRATEGY_H_ */
