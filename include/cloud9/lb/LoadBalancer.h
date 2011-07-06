/*
 * LoadBalancer.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef LOADBALANCER_H_
#define LOADBALANCER_H_

#include "cloud9/lb/TreeNodeInfo.h"
#include "cloud9/lb/Worker.h"

#include <set>
#include <map>
#include <boost/asio.hpp>

namespace cloud9 {

class ExecutionPath;

namespace lb {

// TODO: Refactor this into a more generic class
class LoadBalancer {
private:
  LBTree *tree;

  boost::asio::deadline_timer timer;
  unsigned worryTimer;
  unsigned balanceTimer;

  std::string programName;
  unsigned statIDCount;
  unsigned programCRC;

  worker_id_t nextWorkerID;

  unsigned rounds;
  bool done;

  std::map<worker_id_t, Worker*> workers;
  std::set<worker_id_t> reports;

  // TODO: Restructure this to a more intuitive representation with
  // global coverage + coverage deltas
  std::vector<uint64_t> globalCoverageData;
  std::vector<char> globalCoverageUpdates;

  bool analyzeBalance(std::map<worker_id_t, unsigned> &load,
      std::map<worker_id_t, transfer_t> &xfers, unsigned balanceThreshold,
      unsigned minTransfer);
  void analyzeAggregateBalance();
  bool analyzePartitionBalance();

  void displayStatistics();

  void periodicCheck(const boost::system::error_code& error);

public:
  LoadBalancer(boost::asio::io_service &service);
  virtual ~LoadBalancer();

  worker_id_t registerWorker(const std::string &address, int port,
      bool wantsUpdates, bool hasPartitions);
  void deregisterWorker(worker_id_t id);

  void registerProgramParams(const std::string &programName, unsigned crc,
      unsigned statIDCount);
  void checkProgramParams(const std::string &programName, unsigned crc,
      unsigned statIDCount);

  void analyze(worker_id_t id);

  LBTree *getTree() const {
    return tree;
  }

  void updateWorkerStatNodes(worker_id_t id,
      std::vector<LBTree::Node*> &newNodes);
  void updateWorkerStats(worker_id_t id, std::vector<int> &stats);

  void updateCoverageData(worker_id_t id, const cov_update_t &data);

  void updatePartitioningData(worker_id_t id, const part_stat_t &stats);

  Worker* getWorker(worker_id_t id) {
    std::map<worker_id_t, Worker*>::iterator it = workers.find(id);
    if (it == workers.end())
      return NULL;
    else
      return (*it).second;
  }

  int getWorkerCount() {
    return workers.size();
  }

  bool isDone() {
    return done;
  }

  void getAndResetCoverageUpdates(worker_id_t id, cov_update_t &data);

  bool requestAndResetTransfer(worker_id_t id, transfer_t &globalTrans,
      part_transfers_t &partTrans);
};

}

}

#endif /* LOADBALANCER_H_ */
