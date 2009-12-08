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

template<class NodeInfo, class ArcInfo>
class ExecutionTree {
public:
	class Node;

	class Arc {
	private:
		Node *parent;
		Node *child;

		ArcInfo info;

		Arc(Node *p, Node *c) : parent(p), child(c) {
			assert(p != NULL);
			assert(c != NULL);
		};
	public:

		Node *getParent() { return parent; }
		Node *getChild() { return child; }

		ArcInfo getInfo() { return info; };
		void setInfo(ArcInfo info) { this->info = info; };
	};

	class Node {
	private:
		unsigned int degree;
		std::vector<Arc*> children;
		Node* parent;

		NodeInfo info;

		Node(int deg, Node* p) : degree(deg), children(deg, (Node*)NULL){
			assert(deg >= 2);
		}
	public:
		Node* getLeft() { return getChild(0); };

		Node* getRight() { return getChild(degree-1); };
		Node* getParent() { return parent; };

		Arc* getArc(int index) { return children[index]; }

		Node* getChild(int index) {
			Arc *arc = getArc(index);
			if (arc)
				return arc->getChild();
			else
				return NULL;
		}

		NodeInfo getInfo() { return info; }
		void setInfo(ArcInfo info) { this->info = info; }
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

};

#endif /* EXECUTIONTREE_H_ */
