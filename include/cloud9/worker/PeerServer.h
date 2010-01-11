/*
 * PeerServer.h
 *
 *  Created on: Jan 9, 2010
 *      Author: stefan
 */

#ifndef PEERSERVER_H_
#define PEERSERVER_H_

#include "cloud9/Protocols.h"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>

using namespace boost::asio::ip;

namespace cloud9 {

namespace worker {

class JobManager;

class PeerConnection: public boost::enable_shared_from_this<PeerConnection> {
private:
	tcp::socket socket;
	JobManager *jobManager;
	AsyncMessageReader<boost::function<void ()> > msgReader;

	void handleMessageReceived();

	PeerConnection(boost::asio::io_service& service, JobManager *jobManager);
public:
	typedef boost::shared_ptr<PeerConnection> pointer;

	static pointer create(boost::asio::io_service& service, JobManager *jobManager) {
		return pointer(new PeerConnection(service, jobManager));
	}

	void start();

	tcp::socket &getSocket() { return socket; }
};

class PeerServer {

private:
	tcp::acceptor acceptor;

	JobManager *jobManager;

	void startAccept();

	void handleAccept(PeerConnection::pointer socket, const boost::system::error_code &error);
public:
	PeerServer(boost::asio::io_service &service, JobManager *jobManager);
	virtual ~PeerServer();
};

}

}

#endif /* PEERSERVER_H_ */
