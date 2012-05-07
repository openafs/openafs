#include <afsconfig.h>
#include <afs/param.h>

#include <stdlib.h>
#include <stdio.h>

#include <tests/tap/basic.h>

#include <afs/opr.h>
#include <opr/rbtree.h>

struct intnode {
    struct opr_rbtree_node node;
    int value;
};

int _checkTree(struct opr_rbtree_node *node)
{
    struct opr_rbtree_node *left, *right;
    int lheight, rheight;

    if (node == NULL)
	return 1;

    left = node->left;
    right = node->right;

    /* Leaf nodes are always black */
    if (node->red && ((left && left->red) || (right &&right->red))) {
	printf("Consecutive red nodes\n");
	return 0;
    }

    /* XXX - Check ordering in nodes */

    lheight = _checkTree(left);
    rheight = _checkTree(right);

    if (lheight !=0 && rheight !=0 && lheight != rheight) {
	printf("Left and right branches have differing number of black nodes\n");
	return 0;
    }

    if (lheight !=0 && rheight !=0)
	return node->red ? lheight : lheight+1;
    else
	return 0;
}

int
checkTree(struct opr_rbtree *head) {
    return _checkTree(head->root);
}

int
insertNode(struct opr_rbtree *head, int value) {

    struct intnode *inode;
    struct opr_rbtree_node *parent, **childptr;

    inode = malloc(sizeof(struct intnode));
    inode->value = value;

    childptr = &head->root;
    parent = NULL;

    while (*childptr) {
	struct intnode *tnode;
	parent = *childptr;
	tnode = opr_containerof(parent, struct intnode, node);

	if (value < tnode->value)
	    childptr = &(*childptr)->left;
	else if (value > tnode->value)
	    childptr = &(*childptr)->right;
	else
	    return -1;
    }
    opr_rbtree_insert(head, parent, childptr, &inode->node);
    return 0;
}

int
countNodes(struct opr_rbtree *head) {
    struct opr_rbtree_node *node;
    int count;

    node = opr_rbtree_first(head);
    if (node == NULL)
	return 0;

    count = 1;
    while ((node = opr_rbtree_next(node)))
	count++;

    return count;
}

/* Now, insert 1000 nodes into the tree, making sure with each insertion that
 * the tree is still valid, and has the correct number of nodes
 */
int
createTree(struct opr_rbtree *head)
{
    int counter;

    for (counter = 1000; counter>0; counter--) {
	while (insertNode(head, random())!=0);
	if (!checkTree(head)) {
	   printf("Tree check failed at %d\n", 1001 - counter);
	   return 0;
	}
	if (countNodes(head) != 1001 - counter ) {
	   printf("Tree has fewer nodes than inserted : %d", 1001 - counter);
	   return 0;
        }
    }
    return 1;
}

/* Randomly remove nodes from the tree, this is really, really inefficient, but
 * hey
 */
int
destroyTree(struct opr_rbtree *head)
{
    int counter;

    for (counter = 1000; counter>0; counter--) {
	struct opr_rbtree_node *node;
	int remove, i;

	remove = random() % counter;
	node = opr_rbtree_first(head);
	for (i=0; i<remove; i++)
	    node = opr_rbtree_next(node);

	opr_rbtree_remove(head, node);
	if (countNodes(head) != counter-1) {
	    printf("Tree has lost nodes after %d deletions", 1001 - counter);
	    return 0;
	}

	if (!checkTree(head)) {
	    printf("Tree check failed at %d removals\n", 1001 - counter);
	    return 0;
	}
    }
    return 1;
}

int
main(void)
{
    struct opr_rbtree head;

    plan(3);

    opr_rbtree_init(&head);
    ok(1, "Initialising the tree works");

    ok(createTree(&head), "Creating tree with 1000 nodes works");
    ok(destroyTree(&head), "Tree retains consistency as nodes deleted");

    return 0;
}
