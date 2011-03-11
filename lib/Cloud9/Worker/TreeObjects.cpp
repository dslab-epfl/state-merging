/*
 * TreeObjects.cpp
 *
 *  Created on: Apr 25, 2010
 *      Author: stefan
 */

#include "cloud9/worker/TreeObjects.h"

namespace cloud9 {

namespace worker {

std::ostream &operator<< (std::ostream &os, const SymbolicState &state) {
	os << *state;
	return os;
}

}
}

