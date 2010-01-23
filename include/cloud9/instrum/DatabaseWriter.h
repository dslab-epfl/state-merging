/*
 * DatabaseWriter.h
 *
 *  Created on: Jan 23, 2010
 *      Author: stefan
 */

#ifndef DATABASEWRITER_H_
#define DATABASEWRITER_H_

#include "cloud9/instrum/InstrumentationWriter.h"

namespace cloud9 {

namespace instrum {

class DatabaseWriter: public InstrumentationWriter {
public:
	DatabaseWriter();
	virtual ~DatabaseWriter();
};

}

}

#endif /* DATABASEWRITER_H_ */
