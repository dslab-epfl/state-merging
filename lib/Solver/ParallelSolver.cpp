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

#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>

#define QUERY_FINALIZAE ((Query*)(uintptr_t)(-1))

#define _CHECKED(smt, expected) \
	do { \
		int res = smt; \
		assert(res == expected); \
	} while (0)

#define _CHECKED2(smt, expected1, expected2) \
	do { \
		int res = smt; \
		assert(res == expected1 || res == expected2); \
	} while (0)

namespace klee {

class ParallelSolver: public SolverImpl {
  friend void *subQuerySolverThread(void *ps);
  friend void *subSolversManagerThread(void *ps);
private:
  bool optimizeDivides;

  pthread_mutex_t mutex;

  pthread_cond_t mainSolverStarted;
  pthread_cond_t mainSolverReady;
  pthread_cond_t subQueryManagerReady;

  pthread_barrier_t queueReadyBarrier;
  pthread_barrier_t queueDoneBarrier;

  // Shared state (protected by the mutex)
  std::deque<Query*> subQueryQueue;
  const Query *mainQuery;

  const std::vector<const Array*> *solObjects;
  std::vector< std::vector<unsigned char> > *solValues;
  bool *hasSolution;

  bool subQueryStarted;

  // Various solver fields (they don't change from query to query)
  unsigned solverCount;
  pthread_t *subQuerySolvers;
  pthread_t subSolversManager;

  unsigned mainSolverTimeout;

  STPSolver *mainSolver;
  char *mainBuffer;

  typedef std::vector<ref<Expr> > constraint_expr_t;

  void splitGeneratorAtPos(const Query &query,
      std::vector<constraint_expr_t> &newQueries, unsigned int pos,
      constraint_expr_t &crtQuery);

  void splitQuery(const Query &query, std::vector<constraint_expr_t> &newQueries);

  void solveQueryInParallel(const Query *query);
  void solveSubQuerySerially(char *solveBuffer, const Query *query);
public:
  ParallelSolver(unsigned solverCount, unsigned mainSolverTimeout, bool optimizeDivides, STPSolver *solver);
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

  // Create our local solver buffer
  char *solverBuffer = pSolver->mainSolver->getSharedMemSegment();

  CLOUD9_DEBUG("Sub-query solver thread ready");

  for(;;) {
    _CHECKED2(pthread_barrier_wait(&pSolver->queueReadyBarrier), 0, PTHREAD_BARRIER_SERIAL_THREAD);

    bool done = false;

    for (;;) {
      _CHECKED(pthread_mutex_lock(&pSolver->mutex), 0);
      if (pSolver->subQueryQueue.empty()) {
        _CHECKED(pthread_mutex_unlock(&pSolver->mutex), 0);
        break;
      }

      Query *q = pSolver->subQueryQueue.front();
      if (q == QUERY_FINALIZAE) {
        _CHECKED(pthread_mutex_unlock(&pSolver->mutex), 0);
        CLOUD9_DEBUG("Sub-query solver done");
        done = true;
        break;
      }

      pSolver->subQueryQueue.pop_front();
      _CHECKED(pthread_mutex_unlock(&pSolver->mutex), 0);

      pSolver->solveSubQuerySerially(solverBuffer, q);

      delete q;
    }

    if (done)
    	break;

    _CHECKED2(pthread_barrier_wait(&pSolver->queueDoneBarrier), 0, PTHREAD_BARRIER_SERIAL_THREAD);
  }

  // Finalizing...
  pSolver->mainSolver->releaseSharedMemSegment(solverBuffer);

  return NULL;
}

void *subSolversManagerThread(void *ps) {
  ParallelSolver *pSolver = static_cast<ParallelSolver*>(ps);

  CLOUD9_DEBUG("Sub-Solver manager thread ready");

  for (;;) {
    _CHECKED(pthread_mutex_lock(&pSolver->mutex), 0);
    while (pSolver->mainQuery == NULL) {
      _CHECKED(pthread_cond_wait(&pSolver->mainSolverStarted, &pSolver->mutex), 0);
    }

    if (pSolver->mainQuery == QUERY_FINALIZAE) {
      _CHECKED(pthread_mutex_unlock(&pSolver->mutex), 0);
      pSolver->solveQueryInParallel(pSolver->mainQuery);
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

    _CHECKED(pthread_mutex_unlock(&pSolver->mutex), 0);

    if (pSolver->subQueryStarted) {
      CLOUD9_DEBUG("Sub-query Manager: Waiting timed out... proceeding to sub-queries");

      assert(pSolver->mainQuery != NULL);
      pSolver->solveQueryInParallel(pSolver->mainQuery);
    }

    _CHECKED(pthread_mutex_lock(&pSolver->mutex), 0);
    pSolver->subQueryStarted = false;
    _CHECKED(pthread_cond_signal(&pSolver->subQueryManagerReady), 0);
    _CHECKED(pthread_mutex_unlock(&pSolver->mutex), 0);
  }

  return NULL;
}

void ParallelSolver::solveSubQuerySerially(char *solveBuffer, const Query *query) {
  std::vector< std::vector<unsigned char> > values;
  bool solution;

  mainSolver->getInitialValues(*query, *solObjects, values, solution, solveBuffer);

  CLOUD9_DEBUG("Sub-query solved!");
}

void ParallelSolver::solveQueryInParallel(const Query *query) {
  if (query == QUERY_FINALIZAE) {
    _CHECKED(pthread_mutex_lock(&mutex), 0);
    subQueryQueue.push_back(QUERY_FINALIZAE);
    _CHECKED(pthread_mutex_unlock(&mutex), 0);

    _CHECKED2(pthread_barrier_wait(&queueReadyBarrier), 0, PTHREAD_BARRIER_SERIAL_THREAD);

    for (unsigned i = 0; i < solverCount; i++) {
      _CHECKED(pthread_join(subQuerySolvers[i], NULL), 0);
    }

    return;
  }

  std::vector<constraint_expr_t> newQueries;
  std::vector<ConstraintManager*> newManagers;
  splitQuery(*query, newQueries);

  CLOUD9_DEBUG("Generated " << newQueries.size() << " orthogonal queries");

  if (newQueries.size() == 1) {
    // Really don't have anything to do here. Pass...
    return;
  }

  // Now populate the queries vector
  _CHECKED(pthread_mutex_lock(&mutex), 0);

  assert(subQueryQueue.empty() && "queries leaked from previous invocation");

  for (unsigned i = 0; i < newQueries.size(); i++) {
    ConstraintManager *m = new ConstraintManager(newQueries[i]);
    newManagers.push_back(m);
    subQueryQueue.push_back(new Query(*m, mainQuery->expr));
  }
  _CHECKED(pthread_mutex_unlock(&mutex), 0);

  _CHECKED2(pthread_barrier_wait(&queueReadyBarrier), 0, PTHREAD_BARRIER_SERIAL_THREAD); // Synchronize with the group

  _CHECKED2(pthread_barrier_wait(&queueDoneBarrier), 0, PTHREAD_BARRIER_SERIAL_THREAD); // Synchronize again with the group

  _CHECKED(pthread_mutex_lock(&mutex), 0);
  while (subQueryQueue.size() > 0) {
    delete subQueryQueue.front();
    subQueryQueue.pop_front();
  }
  _CHECKED(pthread_mutex_unlock(&mutex), 0);

  for (unsigned i = 0; i < newManagers.size(); i++) {
    delete newManagers[i];
  }
  newManagers.clear();
}

ParallelSolver::ParallelSolver(unsigned solverCount, unsigned mainSolverTimeout, bool optimizeDivides, STPSolver *solver) {
  assert(solverCount > 0 && "invalid number of sub-solvers");

  this->solverCount = solverCount;
  this->optimizeDivides = optimizeDivides;
  this->mainSolverTimeout = mainSolverTimeout;
  subQueryStarted = false;

  // Save the main solver
  mainSolver = solver;
  mainBuffer = solver->getSharedMemSegment();
  mainQuery = NULL;

  // Init the synchronization structures
  _CHECKED(pthread_mutex_init(&mutex, NULL), 0);

  _CHECKED(pthread_cond_init(&mainSolverStarted, NULL), 0);
  _CHECKED(pthread_cond_init(&mainSolverReady, NULL), 0);
  _CHECKED(pthread_cond_init(&subQueryManagerReady, NULL), 0);

  _CHECKED(pthread_barrier_init(&queueReadyBarrier, NULL, solverCount + 1), 0);
  _CHECKED(pthread_barrier_init(&queueDoneBarrier, NULL, solverCount + 1), 0); // "+1" accounts for the sub-solvers manager

  // Now create the sub-threads
  int res;
  res = pthread_create(&subSolversManager, NULL,
      &subSolversManagerThread, (void*)this);
  assert(res == 0 && "cannot create solver thread");

  subQuerySolvers = new pthread_t[solverCount];
  for (unsigned i = 0; i < solverCount; i++) {
    res = pthread_create(&subQuerySolvers[i], NULL, &subQuerySolverThread, (void*)this);
    assert(res == 0 && "cannot create solver thread");
  }
}

ParallelSolver::~ParallelSolver() {
  _CHECKED(pthread_mutex_lock(&mutex), 0);
  assert(mainQuery == NULL && "a query is being solved in parallel");
  mainQuery = QUERY_FINALIZAE;
  _CHECKED(pthread_cond_signal(&mainSolverStarted), 0);
  _CHECKED(pthread_mutex_unlock(&mutex), 0);

  _CHECKED(pthread_join(subSolversManager, NULL), 0);

  delete subQuerySolvers;

  mainSolver->releaseSharedMemSegment(mainBuffer);
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
  _CHECKED(pthread_mutex_lock(&mutex), 0);
  mainQuery = &query;
  solObjects = &objects;
  assert(!subQueryStarted && "another query is being solved in parallel");
  _CHECKED(pthread_cond_signal(&mainSolverStarted), 0);
  _CHECKED(pthread_mutex_unlock(&mutex), 0);

  bool result = mainSolver->getInitialValues(query, objects, values,
		  hasSolution, mainBuffer);

  // Now check to see whether the sub-query mechanism started
  _CHECKED(pthread_mutex_lock(&mutex), 0);

  if (subQueryStarted) {
    CLOUD9_DEBUG("Waiting for subqueries to finish...");

    while(subQueryStarted) {
      // We must wait for the subquery to finish
      _CHECKED(pthread_cond_wait(&subQueryManagerReady, &mutex), 0);
    }
  } else {
    _CHECKED(pthread_cond_signal(&mainSolverReady), 0);
  }

  mainQuery = NULL;

  _CHECKED(pthread_mutex_unlock(&mutex), 0);

  return result;
}

Solver *createParallelSolver(unsigned solverCount, unsigned mainSolverTimeout, bool optimizeDivides, STPSolver *solver) {
  return new Solver(new ParallelSolver(solverCount, mainSolverTimeout, optimizeDivides, solver));
}

}
