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

namespace cloud9 {

class ExecutionPath {
	template<class>
	friend class ExecutionTree;
private:
	std::vector<int> path;

	ExecutionPath *parent;
	int parentIndex;

	ExecutionPath();
public:
	virtual ~ExecutionPath();

	std::vector<int> getPath() const { return path; };
	ExecutionPath *getParent() { return parent; }
	int getParentIndex() { return parentIndex; }

	ExecutionPath *getAbsolutePath();

	typedef std::vector<int>::iterator iterator;
};

template<class NodeInfo>
class ExecutionTree {
public:
	class Node {
		friend class ExecutionTree;

	private:
		std::vector<Node*> children;
		Node* parent;

		unsigned int level;
		unsigned int index;
		unsigned int count;

		unsigned int _label;

		NodeInfo _info;

		/*
		 * Creates a new node and connects it in position "index" in a parent
		 * node
		 */
		Node(int deg, Node* p, int index) :
			children(deg), parent(p) {

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
		Node* getLeft() const { return children[0]; };

		Node* getRight() const { return children.front(); };
		Node* getParent() const { return children.back(); };

		Node* getChild(int index) const { return children[index]; }

		int getLevel() const { return level; }
		int getIndex() const { return index; }
		int getCount() const { return count; }

		NodeInfo& operator*() {
			return _info;
		}
	};

	struct NodeBreadthCompare {
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

	struct NodeDepthCompare {
		bool operator() (const Node *a, const Node *b) {
			return a->level < b->level;
		}
	};

private:
	unsigned int degree;
	Node* root;
public:
	ExecutionTree(int deg): degree(deg){
		root = new Node(deg, NULL, 0);
	};

	virtual ~ExecutionTree() { };

	Node* getRoot() const { return root; }

	int getDegree() const { return degree; }

	Node* getNode(ExecutionPath *path) {
		return getNode(path, root);
	}

	Node* getNode(ExecutionPath *p, Node* root) {
		Node *crtNode = root;

		for (ExecutionPath::iterator it = p->path.begin();
				it != p->path.end(); it++) {

			Node *newNode = crtNode->getChild(*it);

			if (newNode != NULL) {
				crtNode = newNode;
				continue;
			}

			newNode = new Node(degree, crtNode, *it);
			crtNode = newNode;
		}

		return crtNode;
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

	void buildPathSet(std::vector<Node*> &seeds, std::vector<ExecutionPath*> &paths) {
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

};

}

#endif /* EXECUTIONTREE_H_ */
