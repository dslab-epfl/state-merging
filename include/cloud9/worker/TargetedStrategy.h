/*
 * Cloud9 Parallel Symbolic Execution Engine
 *
 * Copyright (c) 2011, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * All contributors are listed in CLOUD9-AUTHORS file.
 *
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
  typedef std::map<ExecutionJob*, unsigned> job_set_t;
  typedef std::vector<ExecutionJob*> job_vector_t;

  typedef std::pair<job_set_t, job_vector_t> job_container_t;

  WorkerTree *workerTree;

  job_container_t workingSet;
  job_container_t interestingJobs;
  job_container_t uninterestingJobs;

  interests_t localInterests;

  char adoptionRate;

  unsigned int explosionLimitSize;
  unsigned int workingSetSize;

  ExecutionJob *selectRandom(job_container_t &container);

  void insertInterestingJob(ExecutionJob *job, job_container_t &wset, job_container_t &others);
  void removeInterestingJob(ExecutionJob *job, job_container_t &wset, job_container_t &others);

  void insertJob(ExecutionJob *job, job_container_t &container);
  void removeJob(ExecutionJob *job, job_container_t &container);

  bool isInteresting(klee::ForkTag forkTag, interests_t &interests);
  bool isInteresting(ExecutionJob *job, interests_t &interests);
  void adoptJobs();

  unsigned int selectForExport(job_container_t &container,
      interests_t &interests, std::vector<ExecutionJob*> &jobs,
      unsigned int maxCount);
public:
  TargetedStrategy(WorkerTree *_workerTree, JobManager *_jobManager);
  virtual ~TargetedStrategy() { }

  virtual ExecutionJob* onNextJobSelection();

  virtual void onJobAdded(ExecutionJob *job);
  virtual void onRemovingJob(ExecutionJob *job);

  unsigned getInterestingCount() const { return workingSet.first.size() + interestingJobs.first.size(); }
  unsigned getUninterestingCount() const { return uninterestingJobs.first.size(); }

  void updateInterests(interests_t &interests);
  unsigned int selectForExport(interests_t &interests,
      std::vector<ExecutionJob*> &jobs, unsigned int maxCount);

  static interests_t anything;
};

}

}

#endif /* TARGETEDSTRATEGY_H_ */
