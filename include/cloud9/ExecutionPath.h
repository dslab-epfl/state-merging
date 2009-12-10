/*
 * ExecutionPath.h
 *
 *  Created on: Dec 9, 2009
 *      Author: stefan
 */

#ifndef EXECUTIONPATH_H_
#define EXECUTIONPATH_H_

#include <vector>

namespace cloud9 {

class ExecutionPath {
public:
	ExecutionPath();
	virtual ~ExecutionPath();

	std::vector<int> path;

	typedef std::vector<int>::iterator iterator;
};

}

#endif /* EXECUTIONPATH_H_ */
