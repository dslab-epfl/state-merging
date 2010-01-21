/*
 * LBConnection.h
 *
 *  Created on: Jan 8, 2010
 *      Author: stefan
 */

#ifndef LBCONNECTION_H_
#define LBCONNECTION_H_

#include "cloud9/Protocols.h"

#include <boost/asio.hpp>

using namespace boost::asio::ip;
using namespace cloud9::data;

namespace cloud9 {

namespace worker {

class JobManager;

class LBConnection {
private:
	boost::asio::io_service &service;

	tcp::socket socket;

	JobManager *jobManager;

	int id; // The worker ID assigned by the load balancer

	void transferJobs(std::string &destAddr, int destPort,
			std::vector<ExecutionPath*> paths,
			std::vector<int> counts);

	void processResponse(LBResponseMessage &response);

	void updateLogPrefix();

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
