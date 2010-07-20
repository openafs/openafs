/*
 * Copyright (c) 1995 - 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* this file can go away when roken appears in this branch */
#include <afsconfig.h>
#include "afscp.h"
#include "afscp_internal.h"

#ifdef ROKEN_LIB_FUNCTION
#error this file can go away when roken appears in this branch
#endif

#ifdef _WIN32
#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL     __cdecl
#else
#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL
#endif

#ifndef HAVE_STRNLEN
ROKEN_LIB_FUNCTION size_t ROKEN_LIB_CALL
strnlen(const char *s, size_t len)
{
    size_t i;

    for(i = 0; i < len && s[i]; i++)
	;
    return i;
}
#endif

#ifndef HAVE_TSEARCH
/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 *
 * $NetBSD: tsearch.c,v 1.3 1999/09/16 11:45:37 lukem Exp $
 * $NetBSD: twalk.c,v 1.1 1999/02/22 10:33:16 christos Exp $
 * $NetBSD: tdelete.c,v 1.2 1999/09/16 11:45:37 lukem Exp $
 * $NetBSD: tfind.c,v 1.2 1999/09/16 11:45:37 lukem Exp $
 */

#include <stdlib.h>
#include <sys/types.h>
#include "afscp_search.h"

typedef struct node {
  char         *key;
  struct node  *llink, *rlink;
} node_t;

#ifndef __DECONST
#define __DECONST(type, var)    ((type)(uintptr_t)(const void *)(var))
#endif

/*
 * find or insert datum into search tree
 *
 * Parameters:
 *	vkey:	key to be located
 *	vrootp:	address of tree root
 */

ROKEN_LIB_FUNCTION void *
rk_tsearch(const void *vkey, void **vrootp,
	int (*compar)(const void *, const void *))
{
	node_t *q;
	node_t **rootp = (node_t **)vrootp;

	if (rootp == NULL)
		return NULL;

	while (*rootp != NULL) {	/* Knuth's T1: */
		int r;

		if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
			return *rootp;		/* we found it! */

		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: follow left branch */
		    &(*rootp)->rlink;		/* T4: follow right branch */
	}

	q = malloc(sizeof(node_t));		/* T5: key not found */
	if (q != 0) {				/* make new node */
		*rootp = q;			/* link new node to old */
		/* LINTED const castaway ok */
		q->key = __DECONST(void *, vkey); /* initialize new node */
		q->llink = q->rlink = NULL;
	}
	return q;
}

/*
 * Walk the nodes of a tree
 *
 * Parameters:
 *	root:	Root of the tree to be walked
 */
static void
trecurse(const node_t *root, void (*action)(const void *, VISIT, int),
	 int level)
{

	if (root->llink == NULL && root->rlink == NULL)
		(*action)(root, leaf, level);
	else {
		(*action)(root, preorder, level);
		if (root->llink != NULL)
			trecurse(root->llink, action, level + 1);
		(*action)(root, postorder, level);
		if (root->rlink != NULL)
			trecurse(root->rlink, action, level + 1);
		(*action)(root, endorder, level);
	}
}

/*
 * Walk the nodes of a tree
 *
 * Parameters:
 *	vroot:	Root of the tree to be walked
 */
ROKEN_LIB_FUNCTION void
rk_twalk(const void *vroot,
      void (*action)(const void *, VISIT, int))
{
	if (vroot != NULL && action != NULL)
		trecurse(vroot, action, 0);
}

/*
 * delete node with given key
 *
 * vkey:   key to be deleted
 * vrootp: address of the root of the tree
 * compar: function to carry out node comparisons
 */
ROKEN_LIB_FUNCTION void *
rk_tdelete(const void * __restrict vkey, void ** __restrict vrootp,
	int (*compar)(const void *, const void *))
{
	node_t **rootp = (node_t **)vrootp;
	node_t *p, *q, *r;
	int cmp;

	if (rootp == NULL || (p = *rootp) == NULL)
		return NULL;

	while ((cmp = (*compar)(vkey, (*rootp)->key)) != 0) {
		p = *rootp;
		rootp = (cmp < 0) ?
		    &(*rootp)->llink :		/* follow llink branch */
		    &(*rootp)->rlink;		/* follow rlink branch */
		if (*rootp == NULL)
			return NULL;		/* key not found */
	}
	r = (*rootp)->rlink;			/* D1: */
	if ((q = (*rootp)->llink) == NULL)	/* Left NULL? */
		q = r;
	else if (r != NULL) {			/* Right link is NULL? */
		if (r->llink == NULL) {		/* D2: Find successor */
			r->llink = q;
			q = r;
		} else {			/* D3: Find NULL link */
			for (q = r->llink; q->llink != NULL; q = r->llink)
				r = q;
			r->llink = q->rlink;
			q->llink = (*rootp)->llink;
			q->rlink = (*rootp)->rlink;
		}
	}
	free(*rootp);				/* D4: Free node */
	*rootp = q;				/* link parent to new node */
	return p;
}

/*
 * find a node, or return 0
 *
 * Parameters:
 *	vkey:	key to be found
 *	vrootp:	address of the tree root
 */
ROKEN_LIB_FUNCTION void *
rk_tfind(const void *vkey, void * const *vrootp,
      int (*compar)(const void *, const void *))
{
	node_t **rootp = (node_t **)vrootp;

	if (rootp == NULL)
		return NULL;

	while (*rootp != NULL) {		/* T1: */
		int r;

		if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
			return *rootp;		/* key found */
		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: follow left branch */
		    &(*rootp)->rlink;		/* T4: follow right branch */
	}
	return NULL;
}
#endif
