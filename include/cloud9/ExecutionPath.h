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

class ExecutionPathSet;

class ExecutionPath {
	friend class ExecutionPathSet;
private:
	std::vector<int> path;

	ExecutionPath *parent;
	int parentIndex;

	ExecutionPath();
public:
	virtual ~ExecutionPath();

	std::vector<int> getPath() const { return path; };
	ExecutionPath *getParent() { return parent; }
	int getParentIndex() { return parentIndex; }

	typedef std::vector<int>::iterator iterator;
};

}

#endif /* EXECUTIONPATH_H_ */
