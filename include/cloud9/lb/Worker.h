/*
 * Worker.h
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#ifndef WORKER_H_
#define WORKER_H_

namespace cloud9 {

namespace lb {


class Worker {
private:
	int id;
public:
	Worker();
	virtual ~Worker();

	int getID() const { return id; }
};

}

}

#endif /* WORKER_H_ */
