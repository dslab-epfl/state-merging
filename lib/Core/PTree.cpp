//===-- PTree.cpp ---------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PTree.h"

#include <klee/Expr.h>
#include <klee/util/ExprPPrinter.h>
#include "klee/ExecutionState.h"

#include <vector>
#include <iostream>

using namespace klee;

  /* *** */

PTree::PTree(const data_type &_root) : root(new Node(0,_root)) {
}

PTree::~PTree() {}

std::pair<PTreeNode*, PTreeNode*>
PTree::split(Node *n, 
             const data_type &leftData, 
             const data_type &rightData,
	     const ref<Expr>& condition,
             ForkTag forkTag) {

  assert(n && !n->left && !n->right);
  assert(n->state == PTreeNode::RUNNING);
  n->state = PTreeNode::SPLITTED;
  n->condition = condition;
  n->left = new Node(n, leftData);
  n->right = new Node(n, rightData);
  n->forkTag = forkTag;

  return std::make_pair(n->left, n->right);
}

void PTree::merge(Node *target, Node *other) {
  assert(target);
  assert(other && !other->left && !other->right);
  assert(other->state == PTreeNode::RUNNING);

  other->left = target;
  other->state = PTreeNode::MERGED;
}

void PTree::terminate(Node *n) {
  if(n->state != PTreeNode::MERGED)
    n->state = PTreeNode::TERMINATED;
#if 0
  assert(!n->left && !n->right);
  do {
    Node *p = n->parent;
    delete n;
    if (p) {
      if (n == p->left) {
        p->left = 0;
      } else {
        assert(n == p->right);
        p->right = 0;
      }
    }
    n = p;
  } while (n && !n->left && !n->right);
#endif
}

void PTree::dump(std::ostream &os) {
  ExprPPrinter *pp = ExprPPrinter::create(os);
  pp->setNewline("\\l");
  os << "digraph G {\n";
  os << "\tsize=\"10,7.5\";\n";
  os << "\tratio=fill;\n";
  os << "\trotate=90;\n";
  os << "\tcenter = \"true\";\n";
  os << "\tnode [style=\"filled\",width=.1,height=.1,fontname=\"Terminus\"]\n";
  os << "\tedge [arrowsize=.3]\n";
  std::vector<PTree::Node*> stack;
  stack.push_back(root);
  while (!stack.empty()) {
    PTree::Node *n = stack.back();
    stack.pop_back();

    if (true || n->condition.isNull()) {
      os << "\tn" << n << " [label=\"\"";
    } else {
      os << "\tn" << n << " [label=\"";
      pp->print(n->condition);
      os << "\",shape=diamond";
    }
    if (n->state == PTreeNode::RUNNING)
      os << ",fillcolor=green";
    else if(n->state == PTreeNode::TERMINATED)
      os << ",fillcolor=red";
    os << "];\n";
    if (n->left) {
      os << "\tn" << n << " -> n" << n->left << ";\n";
      if(n->state != PTreeNode::MERGED)
        stack.push_back(n->left);
    }
    if (n->right) {
      os << "\tn" << n << " -> n" << n->right << ";\n";
      if(n->state != PTreeNode::MERGED)
        stack.push_back(n->right);
    }
  }
  os << "}\n";
  delete pp;
}

PTreeNode::PTreeNode(PTreeNode *_parent, 
                     ExecutionState *_data) 
  : parent(_parent),
    left(0),
    right(0),
    data(_data),
    forkTag(KLEE_FORK_DEFAULT),
    condition(0),
    state(RUNNING) {
}

PTreeNode::~PTreeNode() {
}

