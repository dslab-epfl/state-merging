/*
 * ExecutionPathSet.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef EXECUTIONPATHSET_H_
#define EXECUTIONPATHSET_H_

namespace cloud9 {

class ExecutionPath;

class ExecutionPathSet {
private:
	std::set<ExecutionPath>
public:
	ExecutionPathSet();
	virtual ~ExecutionPathSet();
};

}

#endif /* EXECUTIONPATHSET_H_ */
