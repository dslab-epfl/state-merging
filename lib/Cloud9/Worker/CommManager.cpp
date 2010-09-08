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

void CommManager::peerCommunicationControl() {
	PeerServer peerServer(peerCommService, jobManager);

	peerCommService.run();
}

void CommManager::lbCommunicationControl() {
	boost::system::error_code error;

	CLOUD9_INFO("Connecting to the load balancer...");
	LBConnection lbConnection(lbCommService, jobManager);

	while (!terminated) {
		lbConnection.connect(error);

		if (error) {
			CLOUD9_ERROR("Could not connect to the load balancer: " <<
					error.message() << " Retrying in " << RetryConnectTime << " seconds");

			boost::asio::deadline_timer t(lbCommService, boost::posix_time::seconds(RetryConnectTime));
			t.wait();
			continue;
		}

		break;
	}

	if (terminated)
		return;

	CLOUD9_INFO("Connected to the load balancer");

	//try {
		CLOUD9_INFO("Registering worker with the load balancer...");
		lbConnection.registerWorker();

		boost::asio::deadline_timer t(lbCommService, boost::posix_time::seconds(UpdateTime));

		while (!terminated) {
			t.wait();
			t.expires_at(t.expires_at() + boost::posix_time::seconds(UpdateTime));
			// XXX If the worker is blocked (e.g. debugging), take care not to
			// trigger the timer multiple times - use expires_from_now() to
			// check this out.

			lbConnection.sendUpdates();
		}

	//} catch (boost::system::system_error &) {
		// Silently ignore
	//}
}

CommManager::CommManager(JobManager *jm) : jobManager(jm), terminated(false) {

}

CommManager::~CommManager() {

}

void CommManager::setup() {
	peerCommThread = boost::thread(&CommManager::peerCommunicationControl, this);
	lbCommThread = boost::thread(&CommManager::lbCommunicationControl, this);
}

void CommManager::finalize() {
	terminated = true;

	if (peerCommThread.joinable()) {
		peerCommService.stop();
		peerCommThread.join();
	}

	if (lbCommThread.joinable()) {
		lbCommService.stop();
		lbCommThread.join();
	}
}

}

}
