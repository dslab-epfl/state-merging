/*
 * ParallelSolver.cpp
 *
 *  Created on: Nov 24, 2010
 *      Author: stefan
 */

#include "klee/Solver.h"
#include "klee/SolverImpl.h"
#include "klee/Constraints.h"

#include <vector>
#include <deque>

#include <pthread.h>

#define QUERY_FINALIZAE ((Query*)(uintptr_t)(-1))

namespace klee {

class ParallelSolver: public SolverImpl {
  friend void *subQuerySolverThread(void *ps);
  friend void *subSolversManagerThread(void *ps);
private:
  bool optimizeDivides;

  pthread_mutex_t mutex;
  pthread_cond_t mainSolverStarted;
  pthread_cond_t mainSolverReady;
  pthread_cond_t subQueryReady;
  pthread_barrier_t doneBarrier;

  // Shared state (protected by the mutex)
  std::deque<Query*> queryQueue;
  const Query *mainQuery;
  bool subQueryStarted;

  // Various solver fields (they don't change from query to query)
  unsigned solverCount;
  pthread_t *subQuerySolvers;
  pthread_t subSolversManager;

  unsigned mainSolverTimeout;

  Solver *mainSolver;

  typedef std::vector<ref<Expr> > constraint_expr_t;

  void splitGeneratorAtPos(const Query &query,
      std::vector<constraint_expr_t> &newQueries, unsigned int pos,
      constraint_expr_t &crtQuery);

  void splitQuery(const Query &query, std::vector<constraint_expr_t> &newQueries);
public:
  ParallelSolver(unsigned solverCount, unsigned mainSolverTimeout, bool optimizeDivides, Solver *solver);
  virtual ~ParallelSolver();


  virtual bool computeTruth(const Query& query, bool &isValid);


  virtual bool computeValue(const Query& query, ref<Expr> &result);

  virtual bool computeInitialValues(const Query& query,
                                    const std::vector<const Array*>
                                      &objects,
                                    std::vector< std::vector<unsigned char> >
                                      &values,
                                    bool &hasSolution);
};

void *subQuerySolverThread(void *ps) {
  ParallelSolver *pSolver = static_cast<ParallelSolver*>(ps);

  // Create our local solver
  STPSolver *stpSolver = new STPSolver(false, pSolver->optimizeDivides);

  // Finalizing...
  delete stpSolver;

  return NULL;
}

void *subSolversManagerThread(void *ps) {
  ParallelSolver *pSolver = static_cast<ParallelSolver*>(ps);

  CLOUD9_DEBUG("Sub-Solver manager thread ready");

  for (;;) {
    pthread_mutex_lock(&pSolver->mutex);
    while (pSolver->mainQuery == NULL) {
      pthread_cond_wait(&pSolver->mainSolverStarted, &pSolver->mutex);
    }

    if (pSolver->mainQuery == QUERY_FINALIZAE) {
      pthread_mutex_unlock(&pSolver->mutex);
      CLOUD9_DEBUG("Sub-Solver manager thread done");
      break;
    }

    // Now wait a bit, before actually starting to solve the query
    struct timespec wTime;
    clock_gettime(CLOCK_REALTIME, &wTime);
    wTime.tv_nsec += (pSolver->mainSolverTimeout % 1000) * 1000000L;
    wTime.tv_sec += pSolver->mainSolverTimeout / 1000 + wTime.tv_nsec / 1000000000L;
    wTime.tv_nsec %= 1000000000L;


    pthread_cond_timedwait(&pSolver->mainSolverReady, &pSolver->mutex, &wTime);

    if (pSolver->mainQuery != NULL) {
      pSolver->subQueryStarted = true;
    }

    pthread_mutex_unlock(&pSolver->mutex);

    if (pSolver->subQueryStarted) {
      CLOUD9_DEBUG("Sub-query Manager: Waiting timed out... proceeding to sub-queries");
    }

    pthread_mutex_lock(&pSolver->mutex);
    pSolver->subQueryStarted = false;
    pthread_cond_signal(&pSolver->subQueryReady);
    pthread_mutex_unlock(&pSolver->mutex);
  }

  /*std::vector<ParallelSolver::constraint_expr_t> newQueries;
  pSolver->splitQuery(query, newQueries);

  CLOUD9_DEBUG("Generated " << newQueries.size() << " orthogonal queries");*/
  return NULL;
}

ParallelSolver::ParallelSolver(unsigned solverCount, unsigned mainSolverTimeout, bool optimizeDivides, Solver *solver) {
  assert(solverCount > 0 && "invalid number of sub-solvers");

  this->solverCount = solverCount;
  this->optimizeDivides = optimizeDivides;
  this->mainSolverTimeout = mainSolverTimeout;
  subQueryStarted = false;

  // Save the main solver
  mainSolver = solver;

  // Init the synchronization structures
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&mainSolverStarted, NULL);
  pthread_cond_init(&mainSolverReady, NULL);
  pthread_cond_init(&subQueryReady, NULL);

  // Now create the sub-threads
  int res;
  res = pthread_create(&subSolversManager, NULL,
      &subSolversManagerThread, (void*)this);
  assert(res == 0 && "cannot create solver thread");

  subQuerySolvers = new pthread_t[solverCount];
  /*for (int i = 0; i < solverCount; i++) {
    res = pthread_create(&subQuerySolvers[i], NULL, &subQuerySolverThread, (void*)this);
  }*/
}

ParallelSolver::~ParallelSolver() {
  pthread_mutex_lock(&mutex);
  assert(mainQuery == NULL && "a query is being solved in parallel");
  mainQuery = QUERY_FINALIZAE;
  pthread_cond_signal(&mainSolverStarted);
  pthread_mutex_unlock(&mutex);

  pthread_join(subSolversManager, NULL);
  delete mainSolver;

  // TODO: Join the rest of the threads...

  delete subQuerySolvers;
}

void ParallelSolver::splitGeneratorAtPos(const Query &query,
    std::vector<constraint_expr_t> & newQueries, unsigned int pos,
    constraint_expr_t &crtQuery) {
  if (pos == query.constraints.size()) {
    // We reached a solution, add it to the queries vector
    newQueries.push_back(constraint_expr_t(crtQuery));
    return;
  }

  ref<Expr> rootExpr = *(query.constraints.begin() + pos);

  std::deque<ref<Expr> > traversalQueue;
  traversalQueue.push_back(rootExpr);

  while (!traversalQueue.empty()) {
    ref<Expr> crtExpr = traversalQueue.front();
    traversalQueue.pop_front();

    if (crtExpr->getKind() != Expr::Or) {
      crtQuery[pos] = crtExpr;
      splitGeneratorAtPos(query, newQueries, pos+1, crtQuery);
      continue;
    }

    traversalQueue.push_back(cast<OrExpr>(crtExpr)->left);
    traversalQueue.push_back(cast<OrExpr>(crtExpr)->right);
  }
}

void ParallelSolver::splitQuery(const Query &query, std::vector<constraint_expr_t> &newQueries) {
  std::vector<ref<Expr> > crtQuery(query.constraints.size());

  splitGeneratorAtPos(query, newQueries, 0, crtQuery);
}

bool ParallelSolver::computeTruth(const Query& query, bool &isValid) {
  CLOUD9_DEBUG("OPTIMIZATION BUG: We should never reach here");

  return mainSolver->impl->computeTruth(query, isValid);
}

bool ParallelSolver::computeValue(const Query& query, ref<Expr> &result) {
  CLOUD9_DEBUG("OPTIMIZATION BUG: We should never reach here");

  return mainSolver->impl->computeValue(query, result);
}

bool ParallelSolver::computeInitialValues(const Query& query,
                                  const std::vector<const Array*>
                                    &objects,
                                  std::vector< std::vector<unsigned char> >
                                    &values,
                                  bool &hasSolution) {

  // Save the current query, before starting to solve
  pthread_mutex_lock(&mutex);
  mainQuery = &query;
  assert(!subQueryStarted && "another query is being solved in parallel");
  pthread_cond_signal(&mainSolverStarted);
  pthread_mutex_unlock(&mutex);

  bool result = mainSolver->impl->computeInitialValues(query, objects, values, hasSolution);

  // Now check to see whether the sub-query mechanism started
  pthread_mutex_lock(&mutex);

  if (subQueryStarted) {
    CLOUD9_DEBUG("Waiting for subqueries to finish...");

    while(subQueryStarted) {
      // We must wait for the subquery to finish
      pthread_cond_wait(&subQueryReady, &mutex);
    }
  } else {
    pthread_cond_signal(&mainSolverReady);
  }

  mainQuery = NULL;

  pthread_mutex_unlock(&mutex);

  return result;
}

Solver *createParallelSolver(unsigned solverCount, unsigned mainSolverTimeout, bool optimizeDivides, Solver *solver) {
  return new Solver(new ParallelSolver(solverCount, mainSolverTimeout, optimizeDivides, solver));
}

}
