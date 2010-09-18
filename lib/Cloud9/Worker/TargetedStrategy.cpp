/*
 * TargetedStrategy.cpp
 *
 *  Created on: Sep 17, 2010
 *      Author: stefan
 */

#include "cloud9/worker/TargetedStrategy.h"

#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/TreeObjects.h"

#include "llvm/Function.h"

#include "klee/Internal/ADT/RNG.h"
#include "klee/Internal/Module/KModule.h"

using namespace klee;

namespace cloud9 {

namespace worker {

TargetedStrategy::interests_t TargetedStrategy::anything = interests_t();

TargetedStrategy::TargetedStrategy(WorkerTree *_workerTree) :
    workerTree(_workerTree), adoptionRate(10) {

}

bool TargetedStrategy::isInteresting(klee::ForkTag forkTag, interests_t &_interests) {
  if (_interests.empty())
    return true;

  if (_interests.count(forkTag.functionName) > 0)
    return true;

  return false;
}

bool TargetedStrategy::isInteresting(ExecutionJob *job, interests_t &_interests) {
  if (isInteresting(job->getForkTag(), _interests))
    return true;
  else
    return false;
}

void TargetedStrategy::adoptJobs() {
  unsigned int count = adoptionRate * uninterestingJobs.first.size() / 100;
  if (!count)
    count = 1;

  for (unsigned int i = 0; i < count; i++) {
    ExecutionJob *job = selectRandom(uninterestingJobs);

    removeJob(job, uninterestingJobs);
    insertJob(job, interestingJobs);
  }
}

ExecutionJob *TargetedStrategy::selectRandom(job_container_t &cont) {
  assert(cont.second.size() > 0);

  int index = klee::theRNG.getInt32() % cont.second.size();

  return cont.second[index];
}

void TargetedStrategy::insertJob(ExecutionJob *job,
    job_container_t &cont) {
  if (cont.first.count(job) == 0) {
    cont.second.push_back(job);
    cont.first[job] = cont.second.size() - 1;
  }
  assert(cont.first.size() == cont.second.size());
}

void TargetedStrategy::removeJob(ExecutionJob *job,
    job_container_t &cont) {
  if (cont.first.count(job) > 0) {
    cont.first[cont.second.back()] = cont.first[job];
    cont.second[cont.first[job]] = cont.second.back();
    cont.second.pop_back();

    cont.first.erase(job);
  }
  assert(cont.first.size() == cont.second.size());
}

ExecutionJob* TargetedStrategy::onNextJobSelection() {
  if (interestingJobs.first.size() == 0) {
    if (uninterestingJobs.first.size() == 0)
      return NULL;

    adoptJobs();
  }

  ExecutionJob *job = selectRandom(interestingJobs);
  return job;
}

void TargetedStrategy::onJobAdded(ExecutionJob *job) {
  if (isInteresting(job, localInterests)) {
    insertJob(job, interestingJobs);
  } else {
    insertJob(job, uninterestingJobs);
  }
}

void TargetedStrategy::onRemovingJob(ExecutionJob *job) {
  removeJob(job, interestingJobs);
  removeJob(job, uninterestingJobs);
}

void TargetedStrategy::updateInterests(interests_t &_interests) {
  localInterests = _interests;

  // Now we need to rehash states
  job_container_t newInteresting;
  job_container_t newUninteresting;

  for (unsigned i = 0; i < interestingJobs.second.size(); i++) {
    ExecutionJob *job = interestingJobs.second[i];

    if (isInteresting(job, localInterests))
      insertJob(job, newInteresting);
    else
      insertJob(job, newUninteresting);
  }

  for (unsigned i = 0; i < uninterestingJobs.second.size(); i++) {
    ExecutionJob *job = uninterestingJobs.second[i];

    if (isInteresting(job, localInterests))
      insertJob(job, newInteresting);
    else
      insertJob(job, newUninteresting);
  }

  interestingJobs = newInteresting;
  uninterestingJobs = newUninteresting;
}

unsigned int TargetedStrategy::selectForExport(job_container_t &container,
      interests_t &interests, std::vector<ExecutionJob*> &jobs,
      unsigned int maxCount) {
  unsigned int result = 0;

  for (unsigned int i = 0; i < container.second.size(); i++) {
    ExecutionJob *job = container.second[i];

    if (maxCount > 0 && isInteresting(job, interests)) {
      jobs.push_back(job);
      result++;
      maxCount--;
    }

    if (maxCount == 0)
      break;
  }

  return result;
}

unsigned int TargetedStrategy::selectForExport(interests_t &interests,
    std::vector<ExecutionJob*> &jobs, unsigned int maxCount) {
  // First, seek among the uninteresting ones
  unsigned int result = 0;

  result += selectForExport(uninterestingJobs, interests, jobs, maxCount-result);
  if (result == maxCount)
    return maxCount;

  // Second, seek among the interesting ones
  result += selectForExport(interestingJobs, interests, jobs, maxCount-result);
  if (result == maxCount)
    return maxCount;

  // Third, pick any uninteresting
  result += selectForExport(uninterestingJobs, anything, jobs, maxCount-result);
  if (result == maxCount)
    return maxCount;

  // Fourth, pick any interesting
  result += selectForExport(interestingJobs, anything, jobs, maxCount-result);

  return result;
}

}

}
