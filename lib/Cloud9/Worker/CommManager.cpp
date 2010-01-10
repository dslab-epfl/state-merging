/*
 * CommManager.cpp
 *
 *  Created on: Jan 9, 2010
 *      Author: stefan
 */

#include "cloud9/worker/CommManager.h"

#include "cloud9/worker/PeerServer.h"
#include "cloud9/worker/PeerConnection.h"
#include "cloud9/worker/LBConnection.h"

#include <boost/asio.hpp>

namespace cloud9 {

namespace worker {

void CommManager::LBCommThread::operator()() {
	boost::asio::io_service service;

	LBConnection lbConnection();
}

void CommManager::PeerCommThread::operator()() {
	boost::asio::io_service service;

	service.run();
}

CommManager::CommManager() {
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
