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
		friend class ExecutionTree;

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
		Node(int deg, Node* p, int index) {
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
	public:
		Node* getLeft() const { return getChild(0); };

		Node* getRight() const { return getChild(degree-1); };
		Node* getParent() const { return parent; };

		Node* getChild(int index) const { return children[index]; }

		int getLevel() const { return level; }
		int getIndex() const { return index; }
		int getCount() const { return count; }

		NodeInfo info;
	};

	struct NodeCompare {
		bool operator() (const Node *a, const Node *b) {
			if (a->level > b->level)
				while (a->level > b->level)
					a = a->getParent();
			else
				while (a->level < b->level)
					b = b->getParent();

			if (a == b) // One of them is the ancestor of the other
				return false;

			while (a->level > 0) {
				Node *pa = a->parent;
				Node *pb = b->parent;

				if (pa == pb)
					return a->index < b->index;

				a = pa; b = pb;
			}

			return false;
		}
	};

private:
	unsigned int degree;
	Node* root;
public:
	ExecutionTree(int deg = 2): degree(deg){
		root = new Node(deg, NULL, 0);
	};

	virtual ~ExecutionTree() { };

	Node* getRoot() { return root; }

	Node* getNode(ExecutionPath *path) {
		return getNode(path, root);
	}

	Node* getNode(ExecutionPath *p, Node* root) {
		Node *crtNode = root;

		for (ExecutionPath::iterator it = p->path.begin(); it != p->path.end(); it++) {
			Node *newNode = crtNode->getChild(*it);

			if (newNode != NULL) {
				crtNode = newNode;
				continue;
			}

			newNode = new Node(degree, crtNode);
			crtNode = newNode;
		}

		return crtNode;
	}

};

}

#endif /* EXECUTIONTREE_H_ */
