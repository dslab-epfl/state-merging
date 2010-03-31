/*
 * CommManager.h
 *
 *  Created on: Jan 9, 2010
 *      Author: stefan
 */

#ifndef COMMMANAGER_H_
#define COMMMANAGER_H_

#include <boost/thread.hpp>
#include <boost/asio.hpp>

namespace cloud9 {

namespace worker {

class JobManager;

class CommManager {
private:
	JobManager *jobManager;

	boost::thread peerCommThread;
	boost::thread lbCommThread;

	boost::asio::io_service peerCommService;
	boost::asio::io_service lbCommService;

	bool terminated;

	void peerCommunicationControl();
	void lbCommunicationControl();

public:
	CommManager(JobManager *jobManager);
	virtual ~CommManager();

	void setup();

	void finalize();
};

}

}

#endif /* COMMMANAGER_H_ */
