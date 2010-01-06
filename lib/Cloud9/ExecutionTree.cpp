/*
 * ExecutionTree.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/ExecutionTree.h"

namespace cloud9 {

ExecutionPath *ExecutionPath::getAbsolutePath() {
	ExecutionPath *absPath = new ExecutionPath();
	absPath->parent = NULL;
	absPath->parentIndex = 0;

	ExecutionPath *crtPath = this;
	int crtIndex = crtPath->path.size();

	while (crtPath != NULL) {
		absPath->path.insert(absPath->path.begin(), crtPath->path.begin(),
				crtPath->path.begin() + crtIndex);

		crtIndex = crtPath->parentIndex;
		crtPath = crtPath->parent;
	}

	return absPath;
}

}
