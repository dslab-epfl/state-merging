/*
 * ExecutionTree.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef EXECUTIONTREE_H_
#define EXECUTIONTREE_H_

#include <cassert>
#include <vector>

#include "cloud9/ExecutionPath.h"

namespace cloud9 {

template<class NodeInfo>
class ExecutionTree {
public:
	class Node {
	private:
		unsigned int degree;
		std::vector<Node*> children;
		Node* parent;

		unsigned int level;
		unsigned int index;
		unsigned int count;

		/*
		 * Creates a new node and connects it in position "index" in a parent
		 * node
		 */
		Node(int deg, Node* p, int index);
	public:
		Node* getLeft() { return getChild(0); };

		Node* getRight() { return getChild(degree-1); };
		Node* getParent() { return parent; };

		Node* getChild(int index) { return children[index]; }

		int getLevel() { return level; }
		int getIndex() { return index; }
		int getCount() { return count; }

		NodeInfo info;
	};

	struct NodeCompare {
		bool operator() (const Node *a, const Node *b);
	};

private:
	unsigned int degree;
	Node* root;
public:
	ExecutionTree(int deg): degree(deg){
		root = new Node(deg, NULL);
	};

	virtual ~ExecutionTree() { };

	Node* getRoot() { return root; }

	Node* getNode(ExecutionPath *path) {
		return getNode(path, root);
	}

	Node* getNode(ExecutionPath *path, Node* root);

};

}

#endif /* EXECUTIONTREE_H_ */
