/*
 * LBConnection.h
 *
 *  Created on: Jan 8, 2010
 *      Author: stefan
 */

#ifndef LBCONNECTION_H_
#define LBCONNECTION_H_

#include <boost/asio.hpp>

using namespace boost::asio::ip;

namespace cloud9 {

namespace worker {


class LBConnection {
private:
	tcp::resolver resolver;
	tcp::socket socket;

	bool resendStatNodes;	// Whether to re-send statistics structure, or just values
	int id; // The worker ID assigned by the load balancer
public:
	LBConnection(boost::asio::io_service &service);
	virtual ~LBConnection();

	void registerWorker();

	void sendUpdates();
};

}

}
#endif /* LBCONNECTION_H_ */
