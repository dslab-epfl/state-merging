/*
 * ExecutionTree.h
 *
 *  Created on: Dec 8, 2009
 *      Author: stefan
 */

#ifndef EXECUTIONTREE_H_
#define EXECUTIONTREE_H_

#include <cassert>
#include <cstring>
#include <vector>
#include <stack>
#include <set>
#include <algorithm>
#include <iostream>
#include <string>

#include "cloud9/Protocols.h"
#include "cloud9/Logger.h"
#include "cloud9/ExecutionPath.h"

namespace cloud9 {

template<class, int, int>
class TreeNode;


/* An adaptation of boost::intrusive_ptr */
template<class NodeType>
class NodePin {
private:
	typedef NodePin this_type;

public:
	NodePin(int layer) :
		p_(0), layer_(layer) {
	}

	NodePin(NodeType * p, int layer) :
		p_(p), layer_(layer) {
		assert(p_ != 0);
		node_pin_add_ref(p_, layer_);
	}

	NodePin(NodePin const & rhs) :
		p_(rhs.p_), layer_(rhs.layer_) {
		assert(p_ != 0);
		node_pin_add_ref(p_, layer_);
	}

	~NodePin() {
		if (p_ != 0)
			node_pin_release(p_, layer_);
	}

	NodePin & operator=(NodePin const & rhs) {
		this_type(rhs).swap(*this);
		return *this;
	}

	NodeType * get() const {
		return p_;
	}

	int layer() const {
		return layer_;
	}

	void reset() {
		this_type(layer_).swap(*this);
	}

	NodeType & operator*() const {
		assert( p_ != 0 );
		return *p_;
	}

	NodeType * operator->() const {
		assert( p_ != 0 );
		return p_;
	}

	bool operator!() const {
		return p_ == 0;
	}

	typedef NodeType * NodePin::*unspecified_bool_type;

	operator unspecified_bool_type() const {
		return p_ == 0 ? 0 : &this_type::p_;
	}

	void swap(NodePin & rhs) {
		assert(layer_ == rhs.layer_);

		NodeType * tmp = p_;
		p_ = rhs.p_;
		rhs.p_ = tmp;
	}

private:

	NodeType * p_;
	int layer_;
};

template<class T, class U> inline bool operator==(NodePin<T> const & a, NodePin<U> const & b)
{
    return a.get() == b.get();
}

template<class T, class U> inline bool operator!=(NodePin<T> const & a, NodePin<U> const & b)
{
    return a.get() != b.get();
}

template<class T, class U> inline bool operator==(NodePin<T> const & a, U * b)
{
    return a.get() == b;
}

template<class T, class U> inline bool operator!=(NodePin<T> const & a, U * b)
{
    return a.get() != b;
}

template<class T, class U> inline bool operator==(T * a, NodePin<U> const & b)
{
    return a == b.get();
}

template<class T, class U> inline bool operator!=(T * a, NodePin<U> const & b)
{
    return a != b.get();
}

template<class T> inline bool operator<(NodePin<T> const & a, NodePin<T> const & b)
{
    return std::less<T *>()(a.get(), b.get());
}


template<class NodeInfo, int Layers, int Degree>
class TreeNode {
	template<class, int, int>
	friend class ExecutionTree;

	template<class NI, int L, int D>
	friend void node_pin_add_ref(TreeNode<NI, L, D> * p, int layer);

	template<class NI, int L, int D>
	friend void node_pin_release(TreeNode<NI, L, D> * p, int layer);

public:
	typedef NodePin<TreeNode<NodeInfo, Layers, Degree> > Pin;

	typedef TreeNode<NodeInfo, Layers, Degree> *ptr;
private:
	ptr childrenNodes[Degree];		// Pointers to the children of the node
	ptr parent;				// Pointer to the parent of the node

	unsigned int level;				// Node level in the tree
	unsigned int index;				// The index of the child in the parent children vector

	unsigned int count[Layers];		// The number of children per each layer
	unsigned int totalCount;		// The total number of children (used for internal ref-counting)

	bool children[Degree][Layers];
	bool exists[Layers];	// Whether the current node exists on a specific layer

	unsigned int _label;	// Internal

	/*
	 * Basically, the difference between count and _refCount is that count keeps
	 * track of the internal references (pointers from other nodes), while
	 * _refCount keeps track of external, persistent references to the node.
	 *
	 * The reason of doing this is to control the destruction of nodes in cascade
	 * and avoid performance issues when the tree grows very large.
	 */
	unsigned int _refCount[Layers];
	unsigned int _totalRefCount;

	NodeInfo _info;

	/*
	 * Creates a new node and connects it in position "index" in a parent
	 * node
	 */
	TreeNode(TreeNode* p, int index) :
		parent(p), totalCount(0), _label(0), _totalRefCount(0) {

		memset(childrenNodes, 0, Degree*sizeof(TreeNode*));
		memset(children, 0, Degree*Layers*sizeof(bool));
		memset(count, 0, Layers*sizeof(unsigned int));
		memset(_refCount, 0, Layers*sizeof(unsigned int));
		memset(exists, 0, Layers*sizeof(bool));

		if (p != NULL) {
			p->childrenNodes[index] = this;

			level = p->level + 1;
			this->index = index;
		} else {
			level = 0;
			this->index = 0;
		}
	}

	void incCount(int layer) { count[layer]++; totalCount++; }
	void decCount(int layer) { count[layer]--; totalCount--; }

	void _incRefCount(int layer) { _refCount[layer]++; _totalRefCount++; }
	void _decRefCount(int layer) { _refCount[layer]--; _totalRefCount--; }

	void makeNode(int layer) {
		assert(!exists[layer]);

		if (parent != NULL) {
			assert(!parent->children[index][layer]);

			parent->children[index][layer] = true;
			parent->incCount(layer);
		}

		exists[layer] = true;
	}

	void clearNode(int layer) {
		assert(exists[layer]);

		exists[layer] = false;

		if (parent != NULL) {
			assert(parent->children[index][layer]);

			parent->children[index][layer] = false;
			parent->decCount(layer);
		}
	}
public:

	ptr getParent() const { return parent; }


	ptr getChild(int layer, int index) const {
		assert(exists[layer]);

		if (children[index][layer])
			return childrenNodes[index];
		else
			return NULL;
	}

	ptr getLeft(int layer) const {
		return getChild(layer, 0);
	}

	ptr getRight(int layer) const {
		return getChild(layer, Degree-1);
	}

	int getLevel() const { return level; }
	int getIndex() const { return index; }
	int getCount(int layer) const { return count[layer]; }
	int getTotalCount() const { return totalCount; }

	bool layerExists(int layer) const { return exists[layer]; }
	bool isLeaf(int layer) const { return count[layer] == 0; }


	NodeInfo& operator*() {
		return _info;
	}

	const NodeInfo& operator*() const {
		return _info;
	}

	Pin pin(int layer) {
		assert(exists[layer]);
		return Pin(this, layer);
	}

};

template<class NodeInfo, int Layers, int Degree>
class ExecutionTree {
	template<class NI, int L, int D>
	friend void node_pin_release(TreeNode<NI, L, D> * p, int layer);

#define BEGIN_LAYERED_DFS_SCAN(layer, root, node) \
		std::stack<Node*> __dfs_nodes; \
		__dfs_nodes.push((root)); \
		while (!__dfs_nodes.empty()) { \
			Node *node = __dfs_nodes.top(); \
			__dfs_nodes.pop();

#define END_LAYERED_DFS_SCAN(layer, root, node) \
			for (int __i = 0; __i < Degree; __i++) { \
				Node *__child = node->getChild((layer), __i); \
				if (__child) \
					__dfs_nodes.push(__child); \
			} \
		}

#define BEGIN_DFS_SCAN(root, node) \
        std::stack<Node*> __dfs_nodes; \
        __dfs_nodes.push((root)); \
        while (!__dfs_nodes.empty()) { \
          Node *node = __dfs_nodes.top(); \
          __dfs_nodes.pop();

#define END_DFS_SCAN(root, node) \
          for (int __i = 0; __i < Degree; __i++) { \
            Node *__child = node->childrenNodes[__i]; \
            if (__child) \
              __dfs_nodes.push(__child); \
          } \
        }

public:
	typedef TreeNode<NodeInfo, Layers, Degree> Node;
	typedef typename TreeNode<NodeInfo, Layers, Degree>::Pin NodePin;

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
	Node* root;

	Node *getNode(int layer, ExecutionPath *p, Node* root, int pos) {
		Node *crtNode = root;
		assert(root->exists[layer]);

		if (p->parent != NULL) {
			crtNode = getNode(layer, p->parent, root, p->parentIndex);
		}

		for (ExecutionPath::path_iterator it = p->path.begin();
				it != p->path.end(); it++) {

			if (pos == 0)
				return crtNode;

			Node *newNode = getNode(layer, crtNode, *it);

			crtNode = newNode;
			pos--;
		}

		return crtNode;
	}

	static void removeSupportingBranch(int layer, Node *node, Node *root) {
		// Checking for node->parent ensures that we will never delete the
		// root node
		while (node->parent && node != root) {
			if (node->count[layer] > 0 || node->_refCount[layer] > 0) // Stop when joining another branch, or hitting the job root
				break;

			Node *temp = node;
			node = node->parent;
			removeNode(layer, temp);
		}
	}

	static void removeNode(int layer, Node *node) {
		assert(node->count[layer] == 0);
		assert(node->_refCount[layer] == 0);

		node->clearNode(layer);

		if (node->totalCount == 0 && node->_totalRefCount == 0) {
			assert(node->parent);
			node->parent->childrenNodes[node->index] = NULL;
			delete node; // Clean it for good, nobody references it anymore
		}
	}

	template<typename NodeIterator>
	void cleanupLabels(NodeIterator begin, NodeIterator end) {

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
	}

public:
	ExecutionTree() {
		root = new Node(NULL, 0);

		// Create a root node in each layer
		for (int layer = 0; layer < Layers; layer++)
			root->makeNode(layer);
	}

	virtual ~ExecutionTree() { }

	Node* getRoot() const {
		return root;
	}

	Node *getNode(int layer, ExecutionPathPin p) {
		return getNode(layer, p.get(), root, p->path.size());
	}

	Node *getNode(int layer, Node *root, int index) {
		assert(root->exists[layer]);

		Node *result = root->childrenNodes[index];

		if (result == NULL)
			result = new Node(root, index);

		if (!root->children[index][layer])
			result->makeNode(layer);

		return result;
	}

	Node *getNode(int layer, Node *node) {
		Node *crtNode = node;
		while (!crtNode->exists[layer]) {
			crtNode->makeNode(layer);
			crtNode = crtNode->parent;
		}

		return node;
	}

	// WARNING: Expensive operation, use it with caution
	void collapseNode(Node *node) {
	  int index = -1;

	  // Check that the operation is valid
	  assert(node->_totalRefCount == 0);

	  for (int i = 0; i < Degree; i++) {
	    if (node->childrenNodes[i] != NULL) {
	      assert(index < 0);
	      index = i;
	    }
	  }
	  assert(index >= 0);

	  for (int layer = 0; layer < Layers; layer++) {
	    assert((!node->parent) || (!node->exists[layer]) || node->children[index][layer]);
	  }

	  // Perform the pointer manipulation
	  Node *parent = node->parent;
	  Node *child = node->childrenNodes[index];

	  child->parent = parent;
	  child->index = node->index;

	  BEGIN_DFS_SCAN(child, descendant)
	  descendant->level = descendant->level - 1;
	  END_DFS_SCAN(child, descendant)

	  if (parent != NULL)
	    parent->childrenNodes[node->index] = child;
	  else {
	    root = child;

	    for (int layer = 0; layer < Layers; layer++) {
	      if (!root->exists[layer])
            root->makeNode(layer);
	    }
	  }

	  delete node;
	}

	unsigned int countLeaves(int layer, Node *root) {
		assert(root->exists[layer]);
		unsigned int result = 0;

		BEGIN_LAYERED_DFS_SCAN(layer, root, node)

		if (node->isLeaf(layer))
			result++;

		END_LAYERED_DFS_SCAN(layer, root, node)

		return result;
	}

	template<class Predicate>
	unsigned int countLeaves(int layer, Node *root, Predicate pred) {
		assert(root->exists[layer]);
		unsigned int result = 0;

		BEGIN_LAYERED_DFS_SCAN(layer, root, node)

		if (node->isLeaf(layer) && pred(node))
			result++;

		END_LAYERED_DFS_SCAN(layer, root, node)

		return result;
	}

	template<typename NodeCollection>
	void getLeaves(int layer, Node *root, NodeCollection &nodes) {
		assert(root->exists[layer]);

		BEGIN_LAYERED_DFS_SCAN(layer, root, node)

		if (node->isLeaf(layer))
			nodes.push_back(node);

		END_LAYERED_DFS_SCAN(layer, root, node)
	}

	template<typename NodeCollection, typename Predicate>
	void getLeaves(int layer, Node *root, Predicate pred, unsigned int maxCount, NodeCollection &nodes) {
		assert(root->exists[layer]);
		bool unlimited = (maxCount == 0);

		BEGIN_LAYERED_DFS_SCAN(layer, root, node)

		if (node->isLeaf(layer) && pred(node)) {
			if (unlimited || maxCount > 0)
				nodes.push_back(node);

			if (!unlimited) {
				if (maxCount > 0)
					maxCount--;
				else
					break;
			}
		}

		END_LAYERED_DFS_SCAN(layer, root, node)
	}

	template<class Generator>
    Node* selectRandomLeaf(int layer, Node *root, Generator &gen,
        int layerMask = 0) {

      assert(root->exists[layer]);
      Node *crtNode = root;
      int crtCount;

      while (1) {
        Node *candidate = NULL;
        if (!layerMask)
          crtCount = crtNode->getCount(layer);
        else {
          crtCount = 0;
          for (int i = 0; i < Degree; i++) {
            Node *child = crtNode->getChild(layer, i);
            if (child && child->exists[layerMask]) {
              candidate = child;
              crtCount++;
            }
          }
        }

        if (!crtCount)
          break;
        if (layerMask && crtCount == 1) {
          crtNode = candidate;
          continue;
        }

        int index = gen.getInt32() % crtCount;
        for (int i = 0; i < Degree; i++) {
          Node *child = crtNode->getChild(layer, i);
          if (child && (!layerMask || child->exists[layerMask])) {
            if (index == 0) {
              crtNode = child;
              break;
            } else {
              index--;
            }
          }
        }
      }

      return crtNode;

    }

	template<class Generator, typename NodeIterator>
	Node *selectRandomLeaf(int layer, Node *root, Generator &gen,
	    NodeIterator begin, NodeIterator end) {

	  assert(root->exists[layer]);

	  for (NodeIterator it = begin; it != end; it++) {
	    Node *crtNode = *it;
	    assert(crtNode->exists[layer]);

	    while (crtNode != root) {
	      if (crtNode->_label == 1)
	        break;

	      crtNode->_label = 1;
	      crtNode = crtNode->parent;
	    }
	  }

	  Node *crtNode = root;

	  while(1) {
	    int crtCount = 0;
	    Node *candidate = NULL;
	    for (int i = 0; i < Degree; i++) {
	      Node *child = crtNode->getChild(layer, i);
	      if (child && child->_label == 1) {
	        candidate = child;
	        crtCount++;
	      }
	    }

	    if (!crtCount)
	      break;
	    if (crtCount == 1) {
	      crtNode = candidate;
	      continue;
	    }

	    int index = gen.getInt32() % crtCount;
	    for (int i = 0; i < Degree; i++) {
	      Node *child = crtNode->getChild(layer, i);
	      if (child && child->_label == 1) {
	        if (!index) {
	          crtNode = child;
	          break;
	        } else {
	          index--;
	        }
	      }
	    }
	  }

	  cleanupLabels(begin, end);

	  return crtNode;
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

		cleanupLabels(begin, end);

		return ExecutionPathSetPin(set);
	}

	void buildPath(int layer, Node *node, Node *pathRoot, std::vector<int> &path) {
	  assert(node->layerExists(layer));
	  path.clear();

      while (node != pathRoot) {
        path.push_back(node->getIndex());
        node = node->getParent();
      }

      std::reverse(path.begin(), path.end());
    }

	template<typename NodeCollection>
	void getNodes(int layer, ExecutionPathSetPin pathSet, NodeCollection &nodes) {
		nodes.clear();

		for (ExecutionPathSet::iterator it = pathSet->paths.begin();
				it != pathSet->paths.end(); it++) {
			Node *crtNode = getNode(layer, *it, root, (*it)->path.size());
			nodes.push_back(crtNode);
		}
	}

#undef BEGIN_LAYERED_DFS_SCAN
#undef END_LAYERED_DFS_SCAN

#undef BEGIN_DFS_SCAN
#undef END_DFS_SCAN

}; // End of ExecutionTree class


template<class NI, int L, int D>
void getASCIINode(const TreeNode<NI, L, D> &node, std::string &result) {
	result.push_back('<');

	const TreeNode<NI, L, D> *crtNode = &node;

	while (crtNode->getParent() != NULL) {
		result.push_back(crtNode->getIndex() ? '1' : '0');

		crtNode = crtNode->getParent();
	}

	result.push_back('>');

	std::reverse(result.begin() + 1, result.end() - 1);
}

template<class NI, int L, int D>
std::ostream& operator<<(std::ostream &os,
		const TreeNode<NI, L, D> &node) {

	std::string str;
	getASCIINode(node, str);
	os << str;

	return os;
}

template<class NodeType>
std::ostream& operator<<(std::ostream &os, const NodePin<NodeType> &pin) {
	os << *(pin.get());
	return os;
}

template<class NI, int L, int D>
void node_pin_add_ref(TreeNode<NI, L, D> *p, int layer) {
	assert(p);

	p->_incRefCount(layer);

	//if (layer == 0) CLOUD9_DEBUG("New inc ref count " << p->_refCount[0] << " for node " << *p);
}

template<class NI, int L, int D>
void node_pin_release(TreeNode<NI, L, D> *p, int layer) {
	assert(p);
	assert(p->_refCount[layer] > 0);

	p->_decRefCount(layer);

	//if (layer == 0) CLOUD9_DEBUG("New dec ref count " << p->_refCount[0] << " for node " << *p);

	if (p->_refCount[layer] == 0) {
		ExecutionTree<NI, L, D>::removeSupportingBranch(layer, p, NULL);
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
