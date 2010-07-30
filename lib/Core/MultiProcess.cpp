/*
 * MultiProcess.cpp
 *
 *  Created on: Jul 22, 2010
 *      Author: stefan
 */

#include "klee/MultiProcess.h"

namespace klee {

process_id_t Process::pidCounter = 2;

Process::Process() : ppid(0) {
  pid = pidCounter++;
}

}
