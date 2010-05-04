/*
 * LBServer.h
 *
 *  Created on: Jan 7, 2010
 *      Author: stefan
 */

#ifndef LBSERVER_H_
#define LBSERVER_H_

#include "cloud9/lb/WorkerConnection.h"

#include <boost/asio.hpp>
#include <set>

using namespace boost::asio::ip;

namespace cloud9 {

namespace lb {

class LoadBalancer;

class LBServer {
private:
  tcp::acceptor acceptor;

  LoadBalancer *lb;

  void startAccept();

  void handleAccept(WorkerConnection::pointer conn,
      const boost::system::error_code &code);
public:
  LBServer(LoadBalancer *lb, boost::asio::io_service &io_service, int port);
  virtual ~LBServer();

  void run();
};

}

}

#endif /* LBSERVER_H_ */
