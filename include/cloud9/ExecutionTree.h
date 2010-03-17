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
#include <stack>
#include <set>
#include <algorithm>
#include <iostream>
#include <string>
#include <boost/intrusive_ptr.hpp>

#include "cloud9/Protocols.h"
#include "cloud9/Logger.h"
#include "cloud9/ExecutionPath.h"

namespace cloud9 {

template<class NodeInfo>
class TreeNode {
	template<class>
	friend class ExecutionTree;

	template<class NI>
	friend void intrusive_ptr_add_ref(TreeNode<NI> * p);

	template<class NI>
	friend void intrusive_ptr_release(TreeNode<NI> * p);

public:
	typedef boost::intrusive_ptr<TreeNode<NodeInfo> > Pin;
	typedef TreeNode<NodeInfo> *Ptr;
private:
	std::vector<TreeNode*> children;
	TreeNode* parent;

	unsigned int level;
	unsigned int index;
	unsigned int count;

	unsigned int _label;

	/*
	 * Basically, the difference between count and _refCount is that count keeps
	 * track of the internal references (pointers from other nodes), while
	 * _refCount keeps track of external, persistent references to the node.
	 *
	 * The reason of doing this is to control the destruction of nodes in cascade
	 * and avoid performance issues when the tree grows very large.
	 */
	unsigned int _refCount;

	NodeInfo _info;

	/*
	 * Creates a new node and connects it in position "index" in a parent
	 * node
	 */
	TreeNode(int deg, TreeNode* p, int index) :
		children(deg), parent(p), count(0), _label(0), _refCount(0) {

		if (p != NULL) {
			p->children[index] = this;
			p->count++;

			level = p->level + 1;
			this->index = index;
		} else {
			level = 0;
			this->index = 0;
		}
	}
public:

	Ptr getLeft() const {
		return children.front();
	}

	Ptr getRight() const {
		return children.back();
	}

	Ptr getParent() const {
		return parent;
	}

	Ptr getSibling() const {
		if (parent == NULL)
			return NULL;

		return parent->getChild(1 - index);
	}


	Ptr getChild(int index) const {
		return children[index];
	}

	int getLevel() const {
		return level;
	}
	int getIndex() const {
		return index;
	}
	int getCount() const {
		return count;
	}

	NodeInfo& operator*() {
		return _info;
	}

	Pin pin() {
		return Pin(this);
	}

};

template<class NodeInfo>
class ExecutionTree {
	template<class NI>
	friend void intrusive_ptr_release(TreeNode<NI> * p);
public:
	typedef TreeNode<NodeInfo> Node;
	typedef typename TreeNode<NodeInfo>::Pin NodePin;
	typedef typename TreeNode<NodeInfo>::Ptr NodePtr;

	struct NodeBreadthCompare {
		bool operator()(const Node *a, const Node *b) {
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

				a = pa;
				b = pb;
			}

			return false;
		}
	};

	struct NodeDepthCompare {
		bool operator()(const Node *a, const Node *b) {
			return a->level < b->level;
		}
	};
private:
	unsigned int degree;
	Node* root;

	Node *getNode(ExecutionPath *p, Node* root, int pos) {
		Node *crtNode = root;

		if (p->parent != NULL) {
			crtNode = getNode(p->parent, root, p->parentIndex);
		}

		for (ExecutionPath::path_iterator it = p->path.begin();
				it != p->path.end(); it++) {

			if (pos == 0)
				return crtNode;

			Node *newNode = getNode(crtNode, *it);

			crtNode = newNode;
			pos--;
		}

		return crtNode;
	}

	static void removeSupportingBranch(Node *node, Node *root) {
		// Checking for node->parent ensures that we will never delete the
		// root node
		while (node->parent && node != root) {
			if (node->count > 0 || node->_refCount > 0) // Stop when joining another branch, or hitting the job root
				break;

			Node *temp = node;
			node = node->parent;
			removeNode(temp);
		}
	}

	static void removeNode(Node *node) {
		assert(node->count == 0);
		assert(node->_refCount == 0);

		if (node->parent != NULL) {
			node->parent->children[node->index] = NULL;
			node->parent->count--;
		}

		delete node;
	}

public:
	ExecutionTree(int deg): degree(deg){
		root = new Node(deg, NULL, 0);
	};

	virtual ~ExecutionTree() { };

	Node* getRoot() const { return root; }

	int getDegree() const { return degree; }

	Node *getNode(ExecutionPathPin p) {
		return getNode(p.get(), root, p->path.size());
	}

	Node *getNode(Node *root, int index) {
		Node *result = root->getChild(index);

		if (result != NULL)
			return result;

		result = new Node(degree, root, index);

		return result;
	}

	template<typename NodeIterator>
	ExecutionPathSetPin buildPathSet(NodeIterator begin, NodeIterator end) {
		ExecutionPathSet *set = new ExecutionPathSet();

		std::vector<Node*> processed; // XXX: Require a random access iterator

		int i = 0;
		for (NodeIterator it = begin; it != end; it++) {
			Node* crtNode = *it;

			ExecutionPath *path = new ExecutionPath();

			ExecutionPath *p = NULL;
			int pIndex = 0;

			while (crtNode != root) {
				if (crtNode->_label > 0) {
					// We hit an already built path
					p = set->paths[crtNode->_label - 1];
					pIndex = p->path.size() -
							(processed[crtNode->_label - 1]->level - crtNode->level);
					break;
				} else {
					path->path.push_back(crtNode->index);
					crtNode->_label = i + 1;

					crtNode = crtNode->parent;
				}
			}

			std::reverse(path->path.begin(), path->path.end());
			path->parent = p;
			path->parentIndex = pIndex;

			set->paths.push_back(path);
			processed.push_back(*it);
			i++;
		}

		// Clean up the labels
		for (NodeIterator it = begin; it != end; it++) {
			Node *crtNode = *it;

			while (crtNode != root) {
				if (crtNode->_label == 0)
					break;
				else {
					crtNode->_label = 0;
					crtNode = crtNode->parent;
				}
			}
		}

		return ExecutionPathSetPin(set);
	}

	template<typename NodeCollection>
	void getNodes(ExecutionPathSetPin pathSet, NodeCollection &nodes) {
		nodes.clear();

		for (ExecutionPathSet::iterator it = pathSet->paths.begin();
				it != pathSet->paths.end(); it++) {
			Node *crtNode = getNode(*it, root, (*it)->path.size());
			nodes.push_back(crtNode);
		}
	}

};


template<class NI>
void getASCIINode(const TreeNode<NI> &node, std::string &result) {
	result.push_back('<');

	const TreeNode<NI> *crtNode = &node;

	while (crtNode->getParent() != NULL) {
		result.push_back(crtNode->getIndex() ? '1' : '0');

		crtNode = crtNode->getParent();
	}

	result.push_back('>');

	std::reverse(result.begin() + 1, result.end() - 1);
}

template<class NI>
std::ostream& operator<<(std::ostream &os,
		const TreeNode<NI> &node) {

	std::string str;
	getASCIINode(node, str);
	os << str;

	return os;
}

template<class NI>
void intrusive_ptr_add_ref(TreeNode<NI> * p) {
	assert(p);

	p->_refCount++;
}

template<class NI>
void intrusive_ptr_release(TreeNode<NI> * p) {
	assert(p);
	assert(p->_refCount > 0);

	p->_refCount--;

	if (p->_refCount == 0) {
		ExecutionTree<NI>::removeSupportingBranch(p, NULL);
	}
}

#if 1 // XXX: debug
#include <boost/lexical_cast.hpp>

template<typename NodeIterator>
std::string getASCIINodeSet(NodeIterator begin, NodeIterator end) {
	std::string result;
	bool first = true;

	result.push_back('[');

	for (NodeIterator it = begin; it != end; it++) {
		if (!first)
			result.append(", ");
		else
			first = false;

		std::string nodeStr;
		getASCIINode(**it, nodeStr);

		result.append(nodeStr);
	}

	result.push_back(']');

	return result;
}

template<typename DataIterator>
std::string getASCIIDataSet(DataIterator begin, DataIterator end) {
	std::string result;

	bool first = true;
	result.push_back('[');

	for (DataIterator it = begin; it != end; it++) {
		if (!first)
			result.append(", ");
		else
			first = false;

		result.append(boost::lexical_cast<std::string>(*it));
	}

	result.push_back(']');

	return result;
}
#endif

}

#endif /* EXECUTIONTREE_H_ */
