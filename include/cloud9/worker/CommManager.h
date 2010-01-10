/*
 * CommManager.h
 *
 *  Created on: Jan 9, 2010
 *      Author: stefan
 */

#ifndef COMMMANAGER_H_
#define COMMMANAGER_H_

#include <boost/thread.hpp>

namespace cloud9 {

namespace worker {

class CommManager {
private:
	struct LBCommThread {
		void operator()();
	};

	struct PeerCommThread {
		void operator()();
	};

	PeerCommThread peerCommControl;
	LBCommThread lbCommControl;

	boost::thread peerCommThread;
	boost::thread lbCommThread;

public:
	CommManager();
	virtual ~CommManager();

	void setup();

	void finalize();
};

}

}

#endif /* COMMMANAGER_H_ */
