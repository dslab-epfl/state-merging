/*
 * ExecutionPath.h
 *
 *  Created on: Feb 12, 2010
 *      Author: stefan
 */

#ifndef EXECUTIONPATH_H_
#define EXECUTIONPATH_H_

#include "cloud9/Logger.h"
#include "cloud9/Protocols.h"

#include <vector>
#include <boost/shared_ptr.hpp>

namespace cloud9 {

class ExecutionPath {
	template<class>
	friend class ExecutionTree;

	friend typename ExecutionPathSet::Pin parseExecutionPathSet(cloud9::data::ExecutionPathSet &ps);

	friend void serializeExecutionPathSet(ExecutionPathSet::Pin &set,
			cloud9::data::ExecutionPathSet &result);
public:
	typedef boost::shared_ptr<ExecutionPath> Pin;
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

	friend typename ExecutionPathSet::Pin parseExecutionPathSet(cloud9::data::ExecutionPathSet &ps);

	friend void serializeExecutionPathSet(ExecutionPathSet::Pin &set,
			cloud9::data::ExecutionPathSet &result);
public:
	typedef boost::shared_ptr<ExecutionPathSet> Pin;
private:
	std::vector<ExecutionPath*> paths;

	ExecutionPathSet();

	typedef std::vector<ExecutionPath*>::iterator iterator;
public:
	virtual ~ExecutionPathSet();

	int count() const {
		return paths.size();
	}

	ExecutionPath::Pin getPath(int index);
};

}


#endif /* EXECUTIONPATH_H_ */
