/*
 * ExecutionPath.h
 *
 *  Created on: Feb 12, 2010
 *      Author: stefan
 */

#ifndef EXECUTIONPATH_H_
#define EXECUTIONPATH_H_

#include "cloud9/Logger.h"

#include <vector>
#include <iostream>
#include <boost/shared_ptr.hpp>

namespace cloud9 {

namespace data {
class ExecutionPathSet;
}

class ExecutionPath;
class ExecutionPathSet;

typedef boost::shared_ptr<ExecutionPath> ExecutionPathPin;
typedef boost::shared_ptr<ExecutionPathSet> ExecutionPathSetPin;

class ExecutionPath {
	template<class>
	friend class ExecutionTree;
	friend class ExecutionPathSet;

	friend ExecutionPathSetPin parseExecutionPathSet(const cloud9::data::ExecutionPathSet &ps);

	friend void serializeExecutionPathSet(ExecutionPathSetPin &set,
			cloud9::data::ExecutionPathSet &result);
private:
	std::vector<int> path;

	ExecutionPath *parent;
	int parentIndex;

	ExecutionPath *getAbsolutePath();

	ExecutionPath() : parent(NULL), parentIndex(0) { };
public:
	virtual ~ExecutionPath() { };

	const std::vector<int> &getPath() const {
		assert(parent == NULL);
		return path;
	};

	typedef std::vector<int>::iterator path_iterator;
};


class ExecutionPathSet {
	template<class>
	friend class ExecutionTree;

	friend ExecutionPathSetPin parseExecutionPathSet(const cloud9::data::ExecutionPathSet &ps);

	friend void serializeExecutionPathSet(ExecutionPathSetPin &set,
			cloud9::data::ExecutionPathSet &result);
private:
	std::vector<ExecutionPath*> paths;

	ExecutionPathSet();

	typedef std::vector<ExecutionPath*>::iterator iterator;
public:
	virtual ~ExecutionPathSet();

	unsigned count() const {
		return paths.size();
	}

	ExecutionPathPin getPath(unsigned int index);

	static ExecutionPathSetPin getEmptySet() {
		return ExecutionPathSetPin(new ExecutionPathSet());
	}

	static ExecutionPathSetPin parse(std::istream &is);
};


}


#endif /* EXECUTIONPATH_H_ */
