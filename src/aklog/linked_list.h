/*
 * $Id$
 *
 * This is the header file for a general list linked package.
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#ifndef __LINKED_LIST_H__
#define __LINKED_LIST_H__

#define LL_SUCCESS 0
#define LL_FAILURE -1

typedef struct _ll_node {
    struct _ll_node *prev;
    struct _ll_node *next;
    char *data;
} ll_node;

typedef struct {
    ll_node *first;
    ll_node *last;
    int nelements;
} linked_list;

typedef enum {ll_head, ll_tail} ll_end;
typedef enum {ll_s_add, ll_s_check} ll_s_action;


/*
 * ll_add_data just assigns the data field of node to be d.
 * If this were c++, this would be an inline function and d
 * would be a void *, but we'll take what we can get...
 */
#define ll_add_data(n,d) (((n)->data)=(char*)(d))

#ifdef __STDC__

void ll_init(linked_list *list);
ll_node *ll_add_node(linked_list *list, ll_end which_end);
int ll_delete_node(linked_list *list, ll_node *node);
int ll_string(linked_list *, ll_s_action, char *);

#else /* __STDC__ */

void ll_init();
ll_node *ll_add_node();
int ll_delete_node();
int ll_string();

#endif /* __STDC__ */

#endif /* __LINKED_LIST_H__ */
