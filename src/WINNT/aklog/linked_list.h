/* $Id: linked_list.h,v 1.1 2004/04/13 03:05:31 jaltman Exp $ */

/* Copyright 1990, 1991, 1999 by the Massachusetts Institute of
 * Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#ifndef AKLOG__LINKED_LIST_H
#define AKLOG__LINKED_LIST_H

#define LL_SUCCESS 0
#define LL_FAILURE -1

typedef struct _ll_node {
    struct _ll_node *prev;
    struct _ll_node *next;
    void *data;
} ll_node;

typedef struct {
    ll_node *first;
    ll_node *last;
    int nelements;
} linked_list;

typedef enum {ll_head, ll_tail} ll_end;


/* ll_add_data just assigns the data field of node to be d. */
#define ll_add_data(n,d) (((n)->data)=(d))

void ll_init(linked_list *list);
ll_node *ll_add_node(linked_list *list, ll_end which_end);
int ll_delete_node(linked_list *list, ll_node *node);
int ll_string_check(linked_list *, char *);
int ll_add_string(linked_list *, char *);

#endif /* AKLOG__LINKED_LIST_H */
