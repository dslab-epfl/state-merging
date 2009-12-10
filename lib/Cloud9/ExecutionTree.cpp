/*
 * ExecutionTree.cpp
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#include "cloud9/ExecutionTree.h"
#include "cloud9/ExecutionPath.h"

#include <algorithm>

namespace cloud9 {

template<class NodeInfo>
typename ExecutionTree<NodeInfo>::Node *ExecutionTree<NodeInfo>::getNode(ExecutionPath *p,
		ExecutionTree<NodeInfo>::Node *root) {

	ExecutionTree::Node *crtNode = root;

	for (ExecutionPath::iterator it = p->path.begin(); it != p->path.end(); it++) {
		ExecutionTree::Node *newNode = crtNode->getChild(*it);

		if (newNode != NULL) {
			crtNode = newNode;
			continue;
		}

		newNode = new ExecutionTree::Node(degree, crtNode);
		crtNode = newNode;
	}

	return crtNode;
}

template<class NodeInfo>
bool ExecutionTree<NodeInfo>::NodeCompare::operator() (const ExecutionTree::Node *a,
		const ExecutionTree::Node *b) {
	if (a->level > b->level)
		while (a->level > b->level)
			a = a->getParent();
	else
		while (a->level < b->level)
			b = b->getParent();

	if (a == b) // One of them is the ancestor of the other
		return false;

	while (a->level > 0) {
		ExecutionTree::Node *pa = a->parent;
		ExecutionTree::Node *pb = b->parent;

		if (pa == pb)
			return a->index < b->index;

		a = pa; b = pb;
	}

	return false;
}

template<class NodeInfo>
ExecutionTree<NodeInfo>::Node::Node(int deg, ExecutionTree<NodeInfo>::Node* p, int index) :
		degree(deg), children(deg, (ExecutionTree::Node*)NULL), parent(p) {
	assert(deg >= 2);

	if (p != NULL) {
		p->children[index] = this;
		p->count++;

		level = p->level + 1;
		this->index = index;
	} else {
		level = 0;
		index = 0;
	}
}

}
