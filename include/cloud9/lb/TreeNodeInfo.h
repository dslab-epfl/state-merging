/*
 * TreeNodeInfo.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef TREENODEINFO_H_
#define TREENODEINFO_H_

#include "cloud9/ExecutionTree.h"

namespace cloud9 {

namespace lb {

class TreeNodeInfo {
public:
	TreeNodeInfo() {};
	virtual ~TreeNodeInfo() {};
};


typedef ExecutionTree<TreeNodeInfo> LBTree;

}

}


#endif /* TREENODEINFO_H_ */
