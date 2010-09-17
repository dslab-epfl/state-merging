/*
 * LBConnection.cpp
 *
 *  Created on: Jan 8, 2010
 *      Author: stefan
 */

#include "cloud9/worker/LBConnection.h"
#include "cloud9/worker/WorkerCommon.h"
#include "cloud9/worker/JobManager.h"
#include "cloud9/worker/StrategyPortfolio.h"
#include "cloud9/worker/TargetedStrategy.h"
#include "cloud9/Logger.h"
#include "cloud9/Protocols.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/System/Path.h"

#include <string>

using namespace llvm;
using namespace cloud9::data;

namespace cloud9 {

namespace worker {

LBConnection::LBConnection(boost::asio::io_service &s, JobManager *jm) :
  service(s), socket(s), jobManager(jm), id(0) {

}

void LBConnection::connect(boost::system::error_code &error) {
  connectSocket(service, socket, LBAddress, LBPort, error);

}

LBConnection::~LBConnection() {
  socket.close();
}

void LBConnection::updateLogPrefix() {
  char prefix[] = "Worker<   >: ";
  sprintf(prefix, "Worker<%03d>: ", id);

  cloud9::Logger::getLogger().setLogPrefix(prefix);
}

void LBConnection::registerWorker() {
  // Prepare the registration message
  WorkerReportMessage message;
  message.set_id(0); // Don't have an ID yet

  WorkerReportMessage_Registration *reg = message.mutable_registration();
  reg->set_address(LocalAddress);
  reg->set_port(LocalPort);
  reg->set_wants_updates(UseGlobalCoverage);

  llvm::sys::Path progPath(InputFile);

  reg->set_prog_name(progPath.getBasename());
  reg->set_stat_id_count(jobManager->getCoverageIDCount());
  reg->set_prog_crc(jobManager->getModuleCRC());

  // Send the message
  std::string msgString;
  bool result = message.SerializeToString(&msgString);
  assert(result);

  sendMessage(socket, msgString);

  std::string respString;
  // Wait for the response
  recvMessage(socket, respString);

  LBResponseMessage response;
  result = response.ParseFromString(respString);
  assert(result);

  id = response.id();

  updateLogPrefix();

  CLOUD9_INFO("Worker registered with the load balancer - ID: " << id);

  processResponse(response);
}

void LBConnection::sendJobStatistics(WorkerReportMessage &message) {
  WorkerReportMessage_NodeDataUpdate *dataUpdate =
      message.mutable_nodedataupdate();
  std::vector<int> data;
  int total = 0;
  ExecutionPathSetPin paths = ExecutionPathSet::getEmptySet();

  jobManager->getStatisticsData(data, paths, true);

  //CLOUD9_DEBUG("Sending " << data.size() << " update values and " <<
  //		paths.size() << " update nodes");

  for (std::vector<int>::iterator it = data.begin(); it != data.end(); it++) {
    dataUpdate->add_data(*it);
    total += *it;
  }

  CLOUD9_DEBUG("[" << total << "] jobs reported to the load balancer");

  TargetedStrategy *tStrategy = dynamic_cast<TargetedStrategy*>(jobManager->getStrategy());

  if (tStrategy) {
    CLOUD9_DEBUG("Interesting: [" << tStrategy->getInterestingCount() <<
        "] Uninteresting: [" << tStrategy->getUninterestingCount() << "]");
  }

  if (paths->count() > 0 || data.size() == 0) {
    assert(paths->count() == data.size());

    WorkerReportMessage_NodeSetUpdate *setUpdate =
        message.mutable_nodesetupdate();
    cloud9::data::ExecutionPathSet *pathSet = setUpdate->mutable_pathset();

    serializeExecutionPathSet(paths, *pathSet);
  }
}

void LBConnection::sendCoverageUpdates(WorkerReportMessage &message) {
  cov_update_t data;
  jobManager->getUpdatedLocalCoverage(data);

  if (data.size() > 0) {
    CLOUD9_DEBUG("Sending " << data.size() << " local coverage updates.");

    StatisticUpdate *update = message.add_localupdates();
    serializeStatisticUpdate(CLOUD9_STAT_NAME_LOCAL_COVERAGE, data, *update);
  }
}

void LBConnection::sendStrategyUpdates(WorkerReportMessage &message) {
  StrategyPortfolio *portfolio =
      dynamic_cast<StrategyPortfolio*> (jobManager->getStrategy());

  if (portfolio == NULL) {
    // The manager does not use a strategy portfolio, just drop these updates
    return;
  }

  WorkerReportMessage_StrategyPortfolioUpdate *update =
      message.mutable_strategyportfolioupdate();

  for (std::vector<unsigned int>::const_iterator it =
      portfolio->getStrategies().begin(); it
      != portfolio->getStrategies().end(); it++) {
    unsigned int strat = *it;
    StrategyPortfolioData *data = update->add_data();
    data->set_strategy(strat);
    data->set_allocation(portfolio->getStrategyAllocation(strat));
    data->set_performance(portfolio->getStrategyPerformance(strat));
  }
}

void LBConnection::sendUpdates() {
  // Prepare the updates message
  WorkerReportMessage message;
  message.set_id(id);

  sendJobStatistics(message);

  sendCoverageUpdates(message);

  std::string msgString;
  bool result = message.SerializeToString(&msgString);
  assert(result);
  sendMessage(socket, msgString);

  std::string respString;
  recvMessage(socket, respString);

  LBResponseMessage response;
  result = response.ParseFromString(respString);
  assert(result);

  processResponse(response);
}

void LBConnection::processResponse(LBResponseMessage &response) {
  assert(id == response.id());

  if (response.more_details()) {
    jobManager->setRefineStatistics();
  }

  if (response.has_jobtransfer()) {
    CLOUD9_DEBUG("Job transfer request");

    const LBResponseMessage_JobTransfer &transDetails = response.jobtransfer();

    std::string destAddress = transDetails.dest_address();
    int destPort = transDetails.dest_port();

    ExecutionPathSetPin paths;
    std::vector<int> counts;

    paths = parseExecutionPathSet(transDetails.path_set());

    counts.insert(counts.begin(), transDetails.count().begin(),
        transDetails.count().end());

    transferJobs(destAddress, destPort, paths, counts);
  }

  if (response.has_jobseed()) {
    const LBResponseMessage_JobSeed &seedDetails = response.jobseed();

    const cloud9::data::ExecutionPathSet &pathSet = seedDetails.path_set();
    ExecutionPathSetPin paths;

    paths = parseExecutionPathSet(pathSet);

    CLOUD9_DEBUG("Job seed request: " << paths->count() << " paths");

    if (seedDetails.strategies_size() > 0) {
      std::vector<unsigned int> strategies;

      for (int i = 0; i < seedDetails.strategies_size(); i++)
        strategies.push_back(seedDetails.strategies(i));

      jobManager->importJobs(paths, &strategies);
    } else {
      jobManager->importJobs(paths, NULL);
    }
  }

  if (UseGlobalCoverage) {
    for (int i = 0; i < response.globalupdates_size(); i++) {
      const StatisticUpdate &update = response.globalupdates(i);

      if (update.name() == CLOUD9_STAT_NAME_GLOBAL_COVERAGE) {
        cov_update_t data;

        parseStatisticUpdate(update, data);

        if (data.size() > 0) {
          CLOUD9_INFO("Receiving " << data.size() << " global coverage updates.");
          jobManager->setUpdatedGlobalCoverage(data);
        }
      }
    }
  }

  if (response.strategyportfolioresponse_size() > 0) {
    for (int i = 0; i < response.strategyportfolioresponse_size(); i++) {
      const StrategyPortfolioResponse &stratResp =
          response.strategyportfolioresponse(i);

      reInvestJobs(stratResp.newstrategy(), stratResp.oldstrategy(),
          stratResp.nrjobs());
    }
  }
}

void LBConnection::transferJobs(std::string &destAddr, int destPort,
    ExecutionPathSetPin paths, std::vector<int> counts) {

  std::vector<unsigned int> strategies;
  ExecutionPathSetPin jobPaths = jobManager->exportJobs(paths, counts,
      &strategies);

  tcp::socket peerSocket(service);
  boost::system::error_code error;

  connectSocket(service, peerSocket, destAddr, destPort, error);

  if (error) {
    CLOUD9_ERROR("Could not connect to the peer worker");
    return;
  }

  PeerTransferMessage message;
  cloud9::data::ExecutionPathSet *pSet = message.mutable_path_set();

  serializeExecutionPathSet(jobPaths, *pSet);

  for (unsigned int i = 0; i < strategies.size(); i++) {
    message.add_strategies(strategies[i]);
  }

  std::string msgString;
  message.SerializeToString(&msgString);

  sendMessage(peerSocket, msgString);

  peerSocket.close();
}

void LBConnection::reInvestJobs(unsigned int oldStrat, unsigned int newStrat,
    unsigned int maxCount) {
  StrategyPortfolio *portfolio =
      dynamic_cast<StrategyPortfolio*> (jobManager->getStrategy());

  if (portfolio == NULL) {
    CLOUD9_INFO("Cannot reinvest jobs as no strategy portfolio is used.");
    return;
  }

  portfolio->reInvestJobs(newStrat, oldStrat, maxCount);
}

}

}
