/*
 * CommManager.cpp
 *
 *  Created on: Jan 9, 2010
 *      Author: stefan
 */

#include "cloud9/worker/CommManager.h"
#include "cloud9/worker/WorkerCommon.h"

#include "cloud9/worker/PeerServer.h"
#include "cloud9/worker/LBConnection.h"
#include "cloud9/Logger.h"

#include <boost/asio.hpp>

namespace cloud9 {

namespace worker {

void CommManager::LBCommThread::operator()() {
	boost::asio::io_service service;
	boost::system::error_code error;

	CLOUD9_INFO("Connecting to the load balancer...");
	LBConnection lbConnection(service, jobManager);

	for (;;) {
		lbConnection.connect(error);

		if (error) {
			CLOUD9_ERROR("Could not connect to the load balancer: " <<
					error.message() << " Retrying in " << RetryConnectTime << " seconds");

			boost::asio::deadline_timer t(service, boost::posix_time::seconds(RetryConnectTime));
			t.wait();
			continue;
		}

		break;
	}

	CLOUD9_INFO("Connected to the load balancer");

	CLOUD9_INFO("Registering worker with the load balancer...");
	lbConnection.registerWorker();

	boost::asio::deadline_timer t(service, boost::posix_time::seconds(UpdateTime));

	for (;;) {
		t.wait();
		t.expires_at(t.expires_at() + boost::posix_time::seconds(UpdateTime));

		lbConnection.sendUpdates();
	}
}

void CommManager::PeerCommThread::operator()() {
	boost::asio::io_service service;

	PeerServer peerServer(service, jobManager);

	service.run();
}

CommManager::CommManager(JobManager *jm) :
		peerCommControl(jm), lbCommControl(jm) {
	// TODO Auto-generated constructor stub

}

CommManager::~CommManager() {
	// TODO Auto-generated destructor stub
}

void CommManager::setup() {
	peerCommThread = boost::thread(peerCommControl);
	lbCommThread = boost::thread(lbCommControl);
}

void CommManager::finalize() {
	peerCommThread.join();
	lbCommThread.join();
}

}

}
