/*
 * ExecutionTree.cpp
 *
 *  Created on: Jan 6, 2010
 *      Author: stefan
 */

#include "cloud9/ExecutionTree.h"
#include "cloud9/Protocols.h"

#include <string>

using namespace cloud9::data;

namespace cloud9 {

void ExecutionPath::parseExecutionPathSet(const ExecutionPathSet &ps,
		std::vector<ExecutionPath*> &result) {

	result.clear();

	for (int i = 0; i < ps.path_size(); i++) {
		const ExecutionPathSet_ExecutionPath &p = ps.path(i);

		ExecutionPath *path = new ExecutionPath();

		if (p.has_parent()) {
			path->parent = result[p.parent()];
			path->parentIndex = p.parent_pos();
		}

		const PathData &data = p.data();
		const unsigned char *pathBytes = (const unsigned char*)data.path().c_str();

		for (int j = 0; j < data.length(); j++) {
			path->path.push_back((pathBytes[j / 8] &
					(unsigned char)(1 << (j % 8))) != 0);

			result.push_back(path);
		}
	}
}

void ExecutionPath::serializeSet(const std::vector<ExecutionPath*> &set,
		cloud9::data::ExecutionPathSet &result) {


}

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
