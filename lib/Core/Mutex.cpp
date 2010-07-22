#include "klee/Mutex.h"

using namespace klee;
Mutex::Mutex(ref<Expr> mutex):
  address(mutex)
{
  thread = 0;
  taken = false;
}

