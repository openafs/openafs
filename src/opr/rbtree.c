/*
 * Copyright (c) 2008 - 2010 Jason Evans <jasone@FreeBSD.org>
 * Copyright (c) 2011 Your File System Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Left-leaning rbtree implementation. Originally derived from the FreeBSD
 * CPP macro definitions by Jason Evans, but extensively modified to
 *     *) Be function, rather than macro based
 *     *) Use parent pointers, rather than calling an embeded comparison fn
 *     *) Use 'NULL' to represent empty nodes, rather than a magic pointer
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef KERNEL
# include "afs/sysincludes.h"
# include "afsincludes.h"
#else
# include <roken.h>
#endif
#include <afs/opr.h>

#include "rbtree.h"

struct {
    struct opr_rbtree_node *left;
    struct opr_rbtree_node *right;
    struct opr_rbtree_node *parent;
    int red;
} opr_rbtree_node;

struct {
    struct opr_rbtree_node *root;
} opr_rbtree;

/* Update the parent pointers for a node which is being replaced.
 *
 * If the original node had no parent, then it was the root of the tree,
 * and the replacement is too.
 * Otherwise, the original node must have been either the left or right
 * child of its parent, so update the left (or right) pointer to point
 * to the replacement as appropriate.
 */

static_inline void
update_parent_ptr(struct opr_rbtree *head, struct opr_rbtree_node *old,
		  struct opr_rbtree_node *replacement)
{
    if (old->parent) {
	if (old->parent->left == old)
	    old->parent->left = replacement;
	else
	    old->parent->right = replacement;
    } else
	head->root = replacement;
}

void
opr_rbtree_init(struct opr_rbtree *head)
{
    head->root = NULL;
}

struct opr_rbtree_node *
opr_rbtree_first(struct opr_rbtree *head)
{
    struct opr_rbtree_node *node;

    node = head->root;
    if (node == NULL)
	return node;

    while (node->left != NULL)
	node = node->left;

    return node;
}

struct opr_rbtree_node *
opr_rbtree_last(struct opr_rbtree *head)
{
    struct opr_rbtree_node *node;

    node = head->root;

    if (node == NULL)
	return node;

    while (node->right != NULL)
	node = node->right;

    return node;
}


struct opr_rbtree_node *
opr_rbtree_next(struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *parent;

    /* Where there is a right child, the next node is to the right, then
     * left as far as we can go */
    if (node->right != NULL) {
	node = node->right;
	while (node->left != NULL)
	     node = node->left;

	return node;
    }

    /* If there is no right hand child, then the next node is above us.
     * Whenever our ancestor is a right-hand child, the next node is
     * further up. When it is a left-hand child, it is our next node
     */
    while ((parent = node->parent) != NULL && node == parent->right)
	node = parent;

    return parent;
}

struct opr_rbtree_node *
opr_rbtree_prev(struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *parent;

    if (node->left != NULL) {
	node = node->left;
	while (node->right != NULL)
	    node = node->right;

	return node;
    }

    /* Same ancestor logic as for 'next', but in reverse */
    while ((parent = node->parent) != NULL && node == parent->left)
	node = parent;

    return parent;
}

static_inline void
initnode(struct opr_rbtree_node *node)
{
    node->left = node->right = node->parent = NULL;
    node->red = 1;
}

static_inline void
rotateright(struct opr_rbtree *head, struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *left = node->left;

    node->left = left->right;
    if (left->right)
	left->right->parent = node;

    left->right = node;
    left->parent = node->parent;

    update_parent_ptr(head, node, left);

    node->parent = left;
}

static_inline void
rotateleft(struct opr_rbtree *head, struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *right = node->right;

    node->right = right->left;
    if (right->left)
	right->left->parent = node;

    right->left = node;
    right->parent = node->parent;

    update_parent_ptr(head, node, right);

    node->parent = right;
}

static_inline void
swapnode(struct opr_rbtree_node **a, struct opr_rbtree_node **b)
{
    struct opr_rbtree_node *tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

static void
insert_recolour(struct opr_rbtree *head, struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *parent, *gramps;

    while ((parent = node->parent) && parent->red) {
	gramps = parent->parent;

	if (parent == gramps->left) {
	    struct opr_rbtree_node *uncle = gramps->right;

	    if (uncle && uncle->red) {
		uncle->red = 0;
		parent->red = 0;
		gramps->red = 1;
		node = gramps;
		continue;
	    }

	    if (parent->right == node) {
		rotateleft(head, parent);
		swapnode(&parent, &node);
	    }

	    parent->red = 0;
	    gramps->red = 1;
	    rotateright(head, gramps);
	} else {
	    struct opr_rbtree_node *uncle = gramps->left;

	    if (uncle && uncle->red) {
		uncle->red = 0;
		parent->red = 0;
		gramps->red = 1;
		node = gramps;
		continue;
	    }

	    if (parent->left == node) {
		rotateright(head, parent);
		swapnode(&parent, &node);
	    }

	    parent->red = 0;
	    gramps->red = 1;
	    rotateleft(head, gramps);
	}
    }

    head->root->red = 0;
}

void
opr_rbtree_insert(struct opr_rbtree *head,
		  struct opr_rbtree_node *parent,
		  struct opr_rbtree_node **childptr,
		  struct opr_rbtree_node *node)
{
    /* Link node 'node' into the tree at position 'parent', using either the
     * left or right pointers */

    node->parent = parent;
    node->left = node->right = NULL;
    node->red = 1;
    *childptr = node;

    /* Rebalance the tree for the newly inserted node */
    insert_recolour(head, node);
}

static void
remove_recolour(struct opr_rbtree *head, struct opr_rbtree_node *parent,
		struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *other;

    while ((node == NULL || !node->red) && node != head->root) {
	if (parent->left == node) {
	    other = parent->right;
	    if (other->red) {
		other->red = 0;
		parent->red = 1;
		rotateleft(head, parent);
		other = parent->right;
	    }
	    if ((other->left == NULL || !other->left->red) &&
	        (other->right == NULL || !other->right->red)) {
		other->red = 1;
		node = parent;
		parent = node->parent;
	    } else {
		if (other->right == NULL || !other->right->red) {
		    other->left->red = 0;
		    other->red = 1;
		    rotateright(head, other);
		    other = parent->right;
		}
		other->red = parent->red;
		parent->red = 0;
		other->right->red = 0;
		rotateleft(head, parent);
		node = head->root;
		break;
	    }
	} else {
	    other = parent->left;
	    if (other->red) {
		other->red = 0;
		parent->red = 1;
		rotateright(head, parent);
		other = parent->left;
	    }
	    if ((other->left == NULL || !other->left->red) &&
		(other->right == NULL || !other->right->red)) {
		other->red = 1;
		node = parent;
		parent = node->parent;
	    } else {
		if (other->left == NULL || !other->left->red) {
		    other->right->red = 0;
		    other->red = 1;
		    rotateleft(head, other);
		    other = parent->left;
		}
		other->red = parent->red;
		parent->red = 0;
		other->left->red = 0;
		rotateright(head, parent);
		node = head->root;
		break;
	    }
	}
    }
    if (node)
	node->red = 0;
}

void
opr_rbtree_remove(struct opr_rbtree *head, struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *child, *parent;
    int red;


    if (node->left == NULL && node->right == NULL) {
	/* A node with no non-leaf children */
	update_parent_ptr(head, node, NULL);

	if (!node->red)
	    remove_recolour(head, node->parent, NULL);

	return;
    }

    if (node->left != NULL && node->right != NULL) {
	/* A node with two children.
	 *
         * Move the next node in the tree (which will be a leaf node)
	 * onto our tree current position, then rebalance as required
	 */
	struct opr_rbtree_node *old, *left;

	old = node;

	/* Set node to the next node in the tree from the current
	 * position, where the next node is the left-most leaf node
	 * in our right child */
	node = node->right;
	while ((left = node->left) != NULL)
	    node = left;

	/* Move 'node' into the position occupied by 'old', which is being
	 * removed */

	update_parent_ptr(head, old, node);

	child = node->right;
	parent = node->parent;
	red = node->red;

	/* As we're logically just copying the value, must preserve the
	 * old node's colour */
	node->red = old->red;

	/* ... and the old node's linkage */
	if (parent == old)
	    parent = node;
	else {
	    if (child)
		child->parent = parent;
	    parent->left = child;

	    node->right = old->right;
	    old->right->parent = node;
	}

	node->parent = old->parent;
	node->left = old->left;
	old->left->parent = node;

	/* If the node being removed was black, then we must recolour the
	 * tree to maintain balance */
	if (!red)
	    remove_recolour(head, parent, child);

	return;
    }

    /* Only remaining option - node with a single child */

    if (node->left == NULL)
        child = node->right;
    else {
	opr_Assert(node->right == NULL);
	child = node->left;
    }

    child->parent = node->parent;

    update_parent_ptr(head, node, child);

    if (!node->red)
	remove_recolour(head, node->parent, child);
}

void
opr_rbtree_replace(struct opr_rbtree *head,
		   struct opr_rbtree_node *old,
		   struct opr_rbtree_node *replacement)
{
    /* Update our parent's pointer to us */
    update_parent_ptr(head, old, replacement);

    /* And our children's pointers to us */
    if (old->left)
	old->left->parent = replacement;
    if (old->right)
	old->right->parent = replacement;

    /* Copy over parent, left, right and colour */
    *replacement = *old;
}
