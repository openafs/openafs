/* Left-leaning red/black trees */

#ifndef OPENAFS_OPR_RBTREE_H
#define OPENAFS_OPR_RBTREE_H 1

struct opr_rbtree_node {
    struct opr_rbtree_node *left;
    struct opr_rbtree_node *right;
    struct opr_rbtree_node *parent;
    int red;
};

struct opr_rbtree {
    struct opr_rbtree_node *root;
};

extern void opr_rbtree_init(struct opr_rbtree *head);
extern struct opr_rbtree_node *opr_rbtree_first(struct opr_rbtree *head);
extern struct opr_rbtree_node *opr_rbtree_last(struct opr_rbtree *head);
extern struct opr_rbtree_node *opr_rbtree_next(struct opr_rbtree_node *node);
extern struct opr_rbtree_node *opr_rbtree_prev(struct opr_rbtree_node *node);
extern void opr_rbtree_insert(struct opr_rbtree *head,
			      struct opr_rbtree_node *parent,
			      struct opr_rbtree_node **childptr,
			      struct opr_rbtree_node *node);
extern void opr_rbtree_remove(struct opr_rbtree *head,
			      struct opr_rbtree_node *node);
extern void opr_rbtree_replace(struct opr_rbtree *head,
			       struct opr_rbtree_node *old,
			       struct opr_rbtree_node *replacement);

#endif
