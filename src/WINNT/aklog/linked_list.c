/* Copyright 1999, 1991, 1999 by the Massachusetts Institute of Technology.
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

/* This file contains general linked list routines. */

static const char rcsid[] = "$Id: linked_list.c,v 1.1 2004/04/13 03:05:31 jaltman Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linked_list.h"

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
void ll_init(linked_list *list)
{
  if (list == NULL)
    {
      fprintf(stderr, "Error: calling ll_init with null pointer.\n");
      abort();
    }

  list->first = list->last = NULL;
  list->nelements = 0;
}

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
ll_node *ll_add_node(linked_list *list, ll_end which_end)
{
  ll_node *node = NULL;

  node = malloc(sizeof(ll_node));
  if (node)
    {
      node->data = NULL;
      if (list->nelements == 0)
	{
	  list->first = node;
	  list->last = node;
	  list->nelements = 1;
	  node->prev = node->next = NULL;
	}
      else
	{
	  switch (which_end)
	    {
	    case ll_head:
	      list->first->prev = node;
	      node->next = list->first;
	      list->first = node;
	      node->prev = NULL;
	      break;
	    case ll_tail:
	      list->last->next = node;
	      node->prev = list->last;
	      list->last = node;
	      node->next = NULL;
	      break;
	    default:
	      fprintf(stderr, "ll_add_node got a which_end parameter that "
		      "it can't handle.\n");
	      abort();
	    }
	  list->nelements++;
	}
    }

  return node;
}


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
int ll_delete_node(linked_list *list, ll_node *node)
{
  ll_node *cur_node;

  for (cur_node = list->first; cur_node; cur_node = cur_node->next)
    {
      if (cur_node == node)
	{
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
	  return LL_SUCCESS;
	}
    }

  return LL_FAILURE;
}

int ll_string_check(linked_list *list, char *string)
{
  ll_node *cur_node;

  /* Scan the list until we find the string in question */
  for (cur_node = list->first; cur_node; cur_node = cur_node->next)
    {
      if (strcmp(string, cur_node->data) == 0)
	return 1;
    }
  return 0;
}

/* This routine maintains a list of strings preventing duplication. */
int ll_add_string(linked_list *list, char *string)
{
  ll_node *node;
  char *new_string;

  if (!ll_string_check(list, string))
    {
      node = ll_add_node(list, ll_tail);
      if (node)
	{
	  new_string = strdup(string);
	  if (new_string)
	    ll_add_data(node, new_string);
	  else
	    return LL_FAILURE;
	}
      else
	return LL_FAILURE;
    }
  return LL_SUCCESS;
}
