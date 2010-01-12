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

#include "cloud9/Protocols.h"

namespace cloud9 {

class ExecutionPath {
	template<class>
	friend class ExecutionTree;
private:
	std::vector<int> path;

	ExecutionPath *parent;
	int parentIndex;

	ExecutionPath() : parent(NULL), parentIndex(0) { };
public:
	virtual ~ExecutionPath() { };

	std::vector<int> getPath() const { return path; };
	ExecutionPath *getParent() { return parent; }
	int getParentIndex() { return parentIndex; }

	ExecutionPath *getAbsolutePath();

	typedef std::vector<int>::iterator iterator;

	friend void parseExecutionPathSet(const cloud9::data::ExecutionPathSet &ps,
			std::vector<ExecutionPath*> &result);

	friend void serializeExecutionPathSet(const std::vector<ExecutionPath*> &set,
			cloud9::data::ExecutionPathSet &result);
};


template<class NodeInfo>
class TreeNode {
	template<class>
	friend class ExecutionTree;
private:
	std::vector<TreeNode*> children;
	TreeNode* parent;

	unsigned int level;
	unsigned int index;
	unsigned int count;

	unsigned int _label;

	NodeInfo _info;

	/*
	 * Creates a new node and connects it in position "index" in a parent
	 * node
	 */
	TreeNode(int deg, TreeNode* p, int index) :
		children(deg), parent(p), count(0), _label(0) {

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
	TreeNode* getLeft() const {
		return children.front();
	}

	TreeNode* getRight() const {
		return children.back();
	}

	TreeNode* getParent() const {
		return parent;
	}


	TreeNode* getChild(int index) const {
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

};

template<class NodeInfo>
class ExecutionTree {
public:
	typedef TreeNode<NodeInfo> Node;

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

		if (pos == 0)
			pos = p->path.size();

		for (ExecutionPath::iterator it = p->path.begin();
				it != p->path.end(); it++) {

			if (pos == 0)
				return crtNode;

			Node *newNode = crtNode->getChild(*it);

			if (newNode != NULL) {
				crtNode = newNode;
				continue;
			}

			newNode = new Node(degree, crtNode, *it);
			crtNode = newNode;

			pos--;
		}

		return crtNode;
	}
public:
	ExecutionTree(int deg): degree(deg){
		root = new Node(deg, NULL, 0);
	};

	virtual ~ExecutionTree() { };

	Node* getRoot() const { return root; }

	int getDegree() const { return degree; }

	Node *getNode(ExecutionPath *p) {
		return getNode(p, root, 0);
	}

	Node *getNode(ExecutionPath *p, int pos) {
		return getNode(p, root, pos);
	}

	Node *getNode(Node *root, int index) {
		Node *result = root->getChild(index);

		if (result != NULL)
			return result;

		result = new Node(degree, root, index);

		return result;
	}

	void removeNode(Node *node) {
		assert(node->count == 0);

		if (node->parent != NULL) {
			node->parent->children[node->index] = NULL;
			node->parent->count--;
		}

		delete node;
	}

	void removeSubTree(Node *root) {
		std::stack<Node*> nodes;

		nodes.push(root);

		while (!nodes.empty()) {
			Node *node = nodes.top();

			if (node->count == 0) {
				nodes.pop();
				removeNode(node);
			} else {
				for (int i = 0; i < degree; i++) {
					if (node->children[i] != NULL)
						nodes.push(node->children[i]);
				}
			}

		}
	}

	void removeSupportingBranch(Node *node, Node *root) {
		while (node->parent && node != root) {
			if (node->count > 0) // Stop when joining another branch, or hitting the job root
				break;

			Node *temp = node;
			node = node->parent;
			removeNode(temp);
		}
	}

	void buildPathSet(const std::vector<Node*> &seeds, std::vector<ExecutionPath*> &paths) {
		paths.clear();

		for (int i = 0; i < seeds.size(); i++) {
			Node* crtNode = seeds[i];

			ExecutionPath *path = new ExecutionPath();

			ExecutionPath *p = NULL;
			int pIndex = 0;

			while (crtNode != root) {
				if (crtNode->_label > 0) {
					// We hit an already built path
					p = paths[crtNode->_label - 1];
					pIndex = p->path.size() -
							(seeds[crtNode->_label - 1]->level - crtNode->level);
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

			paths.push_back(path);
		}

		// Clean up the labels
		for (int i = 0; i < seeds.size(); i++) {
			Node *crtNode = seeds[i];

			while (crtNode != root) {
				if (crtNode->_label == 0)
					break;
				else {
					crtNode->_label = 0;
					crtNode = crtNode->parent;
				}
			}
		}
	}

	void getNodes(const std::vector<ExecutionPath*> &paths,
			std::vector<Node*> &nodes) {

		nodes.clear();

		for (int i = 0; i < paths.size(); i++) {
			Node *crtNode = getNode(paths[i]);

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

#if 1 // XXX: debug
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
#endif

}

#endif /* EXECUTIONTREE_H_ */
