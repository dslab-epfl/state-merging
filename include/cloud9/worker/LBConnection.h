/*
 * LBConnection.h
 *
 *  Created on: Jan 8, 2010
 *      Author: stefan
 */

#ifndef LBCONNECTION_H_
#define LBCONNECTION_H_

#include "cloud9/Protocols.h"
#include "cloud9/worker/PartitioningStrategy.h"

#include <boost/asio.hpp>

using namespace boost::asio::ip;
using namespace cloud9::data;

namespace cloud9 {

namespace worker {

class JobManager;
class PartitioningStrategy;

class LBConnection {
private:
	boost::asio::io_service &service;

	tcp::socket socket;

	JobManager *jobManager;

	unsigned int id; // The worker ID assigned by the load balancer

	void transferJobs(std::string &destAddr, int destPort,
			ExecutionPathSetPin paths,
			std::vector<int> counts,
			part_select_t &partHints);

	PartitioningStrategy *getPartitioningStrategy();

	void processResponse(LBResponseMessage &response);

	void updateLogPrefix();

	void sendJobStatistics(WorkerReportMessage &message);
	void sendCoverageUpdates(WorkerReportMessage &message);
	void sendPartitionStatistics(WorkerReportMessage &message);

public:
	LBConnection(boost::asio::io_service &service, JobManager *jobManager);
	virtual ~LBConnection();

	void connect(boost::system::error_code &error);

	void registerWorker();

	void sendUpdates();
};

}

}
#endif /* LBCONNECTION_H_ */
