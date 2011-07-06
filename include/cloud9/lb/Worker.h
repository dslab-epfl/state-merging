/*
 * Worker.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef WORKER_H_
#define WORKER_H_

#include "cloud9/lb/TreeNodeInfo.h"

#include <vector>
#include <map>

namespace cloud9 {

namespace lb {

typedef unsigned int worker_id_t;
typedef unsigned int part_id_t;

typedef std::map<part_id_t, std::pair<unsigned, unsigned> > part_stat_t;

typedef std::pair<worker_id_t, unsigned> transfer_t;
typedef std::map<part_id_t, transfer_t> part_transfers_t;

class Worker {
  friend class LoadBalancer;
private:
  worker_id_t id;
  std::string address;
  int port;

  bool _wantsUpdates;
  bool _hasPartitions;

  std::vector<LBTree::Node*> nodes;
  unsigned nodesRevision;

  std::vector<char> globalCoverageUpdates;

  unsigned int totalJobs;

  part_stat_t statePartitions;
  std::set<part_id_t> activePartitions;

  unsigned int lastReportTime;

  bool transferReq;
  transfer_t globalTransfer;
  part_transfers_t partTransfers;

  Worker() : _wantsUpdates(false), _hasPartitions(false), nodesRevision(1),
      totalJobs(0), lastReportTime(0), transferReq(false) {

  }
public:
  virtual ~Worker() {
  }

  worker_id_t getID() const {
    return id;
  }

  unsigned getTotalJobs() const {
    return totalJobs;
  }

  const std::string &getAddress() const {
    return address;
  }
  int getPort() const {
    return port;
  }

  bool operator<(const Worker& w) {
    return id < w.id;
  }

  bool wantsUpdates() const {
    return _wantsUpdates;
  }

  bool hasPartitions() const {
    return _hasPartitions;
  }
};

}

}

#endif /* WORKER_H_ */
