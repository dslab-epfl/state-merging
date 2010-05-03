/*
 * TreeNodeInfo.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef TREENODEINFO_H_
#define TREENODEINFO_H_

#include "cloud9/ExecutionTree.h"

#include <set>
#include <map>

namespace cloud9 {

namespace lb {

class Worker;

class TreeNodeInfo {
  friend class LoadBalancer;
public:
  class WorkerInfo {
  public:
    int jobCount;
    int revision;

    WorkerInfo() :
      jobCount(0), revision(0) {
    }
  };

private:
  std::map<int, WorkerInfo> workerData;
public:
  TreeNodeInfo() { }

  virtual ~TreeNodeInfo() { }

};

#define LB_LAYER_COUNT		1

#define LB_LAYER_DEFAULT	0

typedef ExecutionTree<TreeNodeInfo, LB_LAYER_COUNT, 2> LBTree;

}

}

#endif /* TREENODEINFO_H_ */
