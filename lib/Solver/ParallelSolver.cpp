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

namespace klee {

class ParallelSolver: public SolverImpl {
private:
  Solver* solver;

  typedef std::vector<ref<Expr> > constraint_expr_t;

  void splitGeneratorAtPos(const Query &query,
      std::vector<constraint_expr_t> &newQueries, unsigned int pos,
      constraint_expr_t &crtQuery);

  void splitQuery(const Query &query, std::vector<constraint_expr_t> &newQueries);
public:
  ParallelSolver(Solver *_solver);
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

struct SolverWork {
  pthread_mutex_t mutex;
  bool cancelFlag;
};

static void *subQuerySolver(void *work) {
  SolverWork *solverWork = static_cast<SolverWork*>(work);
}

static void *mainQuerySolver(void *work) {
  SolverWork *solverWork = static_cast<SolverWork*>(work);
}

ParallelSolver::ParallelSolver(Solver *_solver) :
  solver(_solver) {

}

ParallelSolver::~ParallelSolver() {

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
  std::vector<constraint_expr_t> newQueries;
  splitQuery(query, newQueries);

  CLOUD9_DEBUG("Generated " << newQueries.size() << " orthogonal queries");

  return solver->impl->computeTruth(query, isValid);
}

bool ParallelSolver::computeValue(const Query& query, ref<Expr> &result) {
  std::vector<constraint_expr_t> newQueries;
  splitQuery(query, newQueries);

  CLOUD9_DEBUG("Generated " << newQueries.size() << " orthogonal queries");

  return solver->impl->computeValue(query, result);
}

bool ParallelSolver::computeInitialValues(const Query& query,
                                  const std::vector<const Array*>
                                    &objects,
                                  std::vector< std::vector<unsigned char> >
                                    &values,
                                  bool &hasSolution) {
  std::vector<constraint_expr_t> newQueries;
  splitQuery(query, newQueries);

  CLOUD9_DEBUG("Generated " << newQueries.size() << " orthogonal queries");

  return solver->impl->computeInitialValues(query, objects, values, hasSolution);
}

Solver *createParallelSolver(Solver *s) {
  return new Solver(new ParallelSolver(s));
}

}
