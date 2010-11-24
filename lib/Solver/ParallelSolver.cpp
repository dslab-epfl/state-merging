/*
 * ParallelSolver.cpp
 *
 *  Created on: Nov 24, 2010
 *      Author: stefan
 */

#include "klee/Solver.h"
#include "klee/SolverImpl.h"


namespace klee {

class ParallelSolver: public SolverImpl {
private:
  Solver* solver;
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

ParallelSolver::ParallelSolver(Solver *_solver) :
  solver(_solver) {

}

ParallelSolver::~ParallelSolver() {

}

bool ParallelSolver::computeTruth(const Query& query, bool &isValid) {
  return solver->impl->computeTruth(query, isValid);
}

bool ParallelSolver::computeValue(const Query& query, ref<Expr> &result) {
  return solver->impl->computeValue(query, result);
}

bool ParallelSolver::computeInitialValues(const Query& query,
                                  const std::vector<const Array*>
                                    &objects,
                                  std::vector< std::vector<unsigned char> >
                                    &values,
                                  bool &hasSolution) {
  return solver->impl->computeInitialValues(query, objects, values, hasSolution);
}

Solver *createParallelSolver(Solver *s) {
  return new Solver(new ParallelSolver(s));
}

}
