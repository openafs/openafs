/* 
 * $Id: linked_list.c,v 1.1.2.2 2005/07/15 16:11:24 rra Exp $
 * 
 * This file contains general linked list routines.
 * 
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#if !defined(lint) && !defined(SABER)
static char *rcsid_list_c = "$Id: linked_list.c,v 1.1.2.2 2005/07/15 16:11:24 rra Exp $";
#endif /* lint || SABER */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linked_list.h"

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

void ll_init(linked_list *list)
  /* 
   * Requires:
   *   List must point to a linked list structure.  It is not acceptable
   *   to pass a null pointer to this routine.
   * Modifies:
   *   list
   * Effects:
   *   Initializes the list to be one with no elements.  If list is
   *   NULL, prints an error message and causes the program to crash.
   */
{
    if (list == NULL) {
	fprintf(stderr, "Error: calling ll_init with null pointer.\n");
	abort();
    }

    /* This sets everything to zero, which is what we want. */
#ifdef WINDOWS
	memset(list, 0, sizeof(linked_list));
#else
    bzero((char *)list, sizeof(linked_list));
#endif /* WINDOWS */
}

ll_node *ll_add_node(linked_list *list, ll_end which_end)
  /*
   * Modifies:
   *   list
   * Effects: 
   *   Adds a node to one end of the list (as specified by which_end)
   *   and returns a pointer to the node added.  which_end is of type
   *   ll_end and should be either ll_head or ll_tail as specified in 
   *   list.h.  If there is not enough memory to allocate a node, 
   *   the program returns NULL.
   */
{
    ll_node *node = NULL;
    
    if ((node = (ll_node *)calloc(1, sizeof(ll_node))) != NULL) {
	if (list->nelements == 0) {
	    list->first = node;
	    list->last = node;
	    list->nelements = 1;
	}
	else {
	    switch (which_end) {
	      case ll_head:
		list->first->prev = node;
		node->next = list->first;
		list->first = node;
		break;
	      case ll_tail:
		list->last->next = node;
		node->prev = list->last;
		list->last = node;
		break;
	      default:
		fprintf(stderr, "%s%s",
			"ll_add_node got a which_end parameter that ",
			"it can't handle.\n");
		abort();
	    }
	    list->nelements++;
	}
    }
	
    return(node);
}


int ll_delete_node(linked_list *list, ll_node *node)
  /* 
   * Modifies: 
   *   list
   * Effects:
   *   If node is in list, deletes node and returns LL_SUCCESS.  
   *   Otherwise, returns LL_FAILURE.  If node contains other data,
   *   it is the responsibility of the caller to free it.  Also, since
   *   this routine frees node, after the routine is called, "node"
   *   won't point to valid data.
   */
{
    int status = LL_SUCCESS;
    ll_node *cur_node = NULL;
    int found = FALSE;

    if (list->nelements == 0)
	status = LL_FAILURE;
    else {
	for (cur_node = list->first; (cur_node != NULL) && !found;
	     cur_node = cur_node->next) {
	    if (cur_node == node) {

		if (cur_node->prev)
		    cur_node->prev->next = cur_node->next;
		else
		    list->first = cur_node->next;

		if (cur_node->next)
		    cur_node->next->prev = cur_node->prev;
		else
		    list->last = cur_node->prev;

		free(cur_node);
		list->nelements--;
		found = TRUE;
	    }
	}
    }

    if (!found)
	status = LL_FAILURE;

    return(status);
}


/* ll_add_data is a macro defined in linked_list.h */

/* This routine maintains a list of strings preventing duplication. */
int ll_string(linked_list *list, ll_s_action action, char *string)
{
    int status = LL_SUCCESS;
    ll_node *cur_node;

    switch(action) {
      case ll_s_check:
	/* Scan the list until we find the string in question */
	for (cur_node = list->first; cur_node && (status == FALSE); 
	     cur_node = cur_node->next)
	    status = (strcmp(string, cur_node->data) == 0);
	break;
      case ll_s_add:
	/* Add a string to the list. */
	if (!ll_string(list, ll_s_check, string)) {
	    if (cur_node = ll_add_node(list, ll_tail)) {
		char *new_string;
		if (new_string = (char *)calloc(strlen(string) + 1, 
						sizeof(char))) {
		    strcpy(new_string, string);
		    ll_add_data(cur_node, new_string);
		}
		else 
		    status = LL_FAILURE;
	    }
	    else
		status = LL_FAILURE;
	}
	break;
      default:
	/* This should never happen */
	status = LL_FAILURE;
	break;
    }

    return(status);
}
