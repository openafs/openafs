/******************************************************************************
 * skip_cas_adt.c
 *
 * Skip lists, allowing concurrent update by use of CAS primitives.
 *
 * Matt Benjamin <matt@linuxbox.com>
 *
 * Adapts MCAS skip list to use a pointer-key and typed comparison
 * function style (because often, your key isn't an integer).
 *
 * Also, set_for_each (and future additions) allow a set to be iterated.
 * Current set_for_each iteration is unordered.
 *
 * Caution, pointer values 0x0, 0x01, and 0x02 are reserved.  Fortunately,
 * no real pointer is likely to have one of these values.

Copyright (c) 2003, Keir Fraser All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
    * notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
    * copyright notice, this list of conditions and the following
    * disclaimer in the documentation and/or other materials provided
    * with the distribution.  Neither the name of the Keir Fraser
    * nor the names of its contributors may be used to endorse or
    * promote products derived from this software without specific
    * prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define __SET_IMPLEMENTATION__

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "portable_defns.h"
#include "ptst.h"
#include "set_queue_adt.h"

// todo:  get rid of me
typedef struct {
    unsigned long key;
    unsigned long secret_key;
} harness_ulong_t;



/*
 * SKIP LIST
 */

typedef struct node_st node_t;
typedef struct set_st osi_set_t;
typedef VOLATILE node_t *sh_node_pt;

struct node_st {
    int level;
//      int                xxx[1024]; /* XXXX testing gc */
#define LEVEL_MASK     0x0ff
#define READY_FOR_FREE 0x100
    setkey_t k;
    setval_t v;
    sh_node_pt next[1];
};

typedef int (*osi_set_cmp_func) (const void *lhs, const void *rhs);

struct set_st {
    CACHE_PAD(0);
    osi_set_cmp_func cmpf;
      CACHE_PAD(1);
    node_t head;
      CACHE_PAD(2);
};

static int gc_id[NUM_LEVELS];

/*
 * PRIVATE FUNCTIONS
 */

#define compare_keys(s, k1, k2) (s->cmpf((const void*) k1, (const void *) k2))

/*
 * Random level generator. Drop-off rate is 0.5 per level.
 * Returns value 1 <= level <= NUM_LEVELS.
 */
static int
get_level(ptst_t * ptst)
{
    unsigned long r = rand_next(ptst);
    int l = 1;
    r = (r >> 4) & ((1 << (NUM_LEVELS - 1)) - 1);
    while ((r & 1)) {
	l++;
	r >>= 1;
    }
    return (l);
}


/*
 * Allocate a new node, and initialise its @level field.
 * NB. Initialisation will eventually be pushed into garbage collector,
 * because of dependent read reordering.
 */
static node_t *
alloc_node(ptst_t * ptst)
{
    int l;
    node_t *n;
    l = get_level(ptst);
    n = gc_alloc(ptst, gc_id[l - 1]);
    n->level = l;
    return (n);
}


/* Free a node to the garbage collector. */
static void
free_node(ptst_t * ptst, sh_node_pt n)
{
    gc_free(ptst, (void *)n, gc_id[(n->level & LEVEL_MASK) - 1]);
}


/*
 * Search for first non-deleted node, N, with key >= @k at each level in @l.
 * RETURN VALUES:
 *  Array @pa: @pa[i] is non-deleted predecessor of N at level i
 *  Array @na: @na[i] is N itself, which should be pointed at by @pa[i]
 *  MAIN RETURN VALUE: same as @na[0].
 */
static sh_node_pt
strong_search_predecessors(osi_set_t * l, setkey_t k, sh_node_pt * pa,
			   sh_node_pt * na)
{
    sh_node_pt x, x_next, old_x_next, y, y_next;
    setkey_t y_k;
    int i;

  retry:
    RMB();

    x = &l->head;
    for (i = NUM_LEVELS - 1; i >= 0; i--) {
	/* We start our search at previous level's unmarked predecessor. */
	READ_FIELD(x_next, x->next[i]);
	/* If this pointer's marked, so is @pa[i+1]. May as well retry. */
	if (is_marked_ref(x_next))
	    goto retry;

	for (y = x_next;; y = y_next) {
	    /* Shift over a sequence of marked nodes. */
	    for (;;) {
		READ_FIELD(y_next, y->next[i]);
		if (!is_marked_ref(y_next))
		    break;
		y = get_unmarked_ref(y_next);
	    }

	    READ_FIELD(y_k, y->k);
	    if (compare_keys(l, y_k, k) >= 0)
		break;

	    /* Update estimate of predecessor at this level. */
	    x = y;
	    x_next = y_next;
	}

	/* Swing forward pointer over any marked nodes. */
	if (x_next != y) {
	    old_x_next = CASPO(&x->next[i], x_next, y);
	    if (old_x_next != x_next)
		goto retry;
	}

	if (pa)
	    pa[i] = x;
	if (na)
	    na[i] = y;
    }

    return (y);
}


/* This function does not remove marked nodes. Use it optimistically. */
static sh_node_pt
weak_search_predecessors(osi_set_t * l, setkey_t k, sh_node_pt * pa,
			 sh_node_pt * na)
{
    sh_node_pt x, x_next;
    setkey_t x_next_k;
    int i;

    x = &l->head;
    for (i = NUM_LEVELS - 1; i >= 0; i--) {
	for (;;) {
	    READ_FIELD(x_next, x->next[i]);
	    x_next = get_unmarked_ref(x_next);

	    READ_FIELD(x_next_k, x_next->k);
	    if (compare_keys(l, x_next_k, k) >= 0)
		break;

	    x = x_next;
	}

	if (pa)
	    pa[i] = x;
	if (na)
	    na[i] = x_next;
    }

    return (x_next);
}


/*
 * Mark @x deleted at every level in its list from @level down to level 1.
 * When all forward pointers are marked, node is effectively deleted.
 * Future searches will properly remove node by swinging predecessors'
 * forward pointers.
 */
static void
mark_deleted(sh_node_pt x, int level)
{
    sh_node_pt x_next;

    while (--level >= 0) {
	x_next = x->next[level];
	while (!is_marked_ref(x_next)) {
	    x_next = CASPO(&x->next[level], x_next, get_marked_ref(x_next));
	}
	WEAK_DEP_ORDER_WMB();	/* mark in order */
    }
}


static int
check_for_full_delete(sh_node_pt x)
{
    int level = x->level;
    return ((level & READY_FOR_FREE) ||
	    (CASIO(&x->level, level, level | READY_FOR_FREE) != level));
}


static void
do_full_delete(ptst_t * ptst, osi_set_t * l, sh_node_pt x, int level)
{
    setkey_t k = x->k;
#ifdef WEAK_MEM_ORDER
    sh_node_pt preds[NUM_LEVELS];
    int i = level;
  retry:
    (void)strong_search_predecessors(l, k, preds, NULL);
    /*
     * Above level 1, references to @x can disappear if a node is inserted
     * immediately before and we see an old value for its forward pointer. This
     * is a conservative way of checking for that situation.
     */
    if (i > 0)
	RMB();
    while (i > 0) {
	node_t *n = get_unmarked_ref(preds[i]->next[i]);
	while (compare_keys(l, n->k, k) < 0) {
	    n = get_unmarked_ref(n->next[i]);
	    RMB();		/* we don't want refs to @x to "disappear" */
	}
	if (n == x)
	    goto retry;
	i--;			/* don't need to check this level again, even if we retry. */
    }
#else
    (void)strong_search_predecessors(l, k, NULL, NULL);
#endif
    free_node(ptst, x);
}


/*
 * PUBLIC FUNCTIONS
 */

/*
 * Called once before any set operations, including set_alloc
 */
void
_init_osi_cas_skip_subsystem(void)
{
    int i;

    for (i = 0; i < NUM_LEVELS; i++) {
		gc_id[i] = gc_add_allocator(sizeof(node_t) + i * sizeof(node_t *),
			"cas_skip_level");
    }
}


osi_set_t *
osi_cas_skip_alloc(osi_set_cmp_func cmpf)
{
    osi_set_t *l;
    node_t *n;
    int i;

    n = malloc(sizeof(*n) + (NUM_LEVELS - 1) * sizeof(node_t *));
    memset(n, 0, sizeof(*n) + (NUM_LEVELS - 1) * sizeof(node_t *));
    n->k = (setkey_t *) SENTINEL_KEYMAX;

    /*
     * Set the forward pointers of final node to other than NULL,
     * otherwise READ_FIELD() will continually execute costly barriers.
     * Note use of 0xfe -- that doesn't look like a marked value!
     */
    memset(n->next, 0xfe, NUM_LEVELS * sizeof(node_t *));

    l = malloc(sizeof(*l) + (NUM_LEVELS - 1) * sizeof(node_t *));
    l->cmpf = cmpf;
    l->head.k = (setkey_t *) SENTINEL_KEYMIN;
    l->head.level = NUM_LEVELS;
    for (i = 0; i < NUM_LEVELS; i++) {
	l->head.next[i] = n;
    }

    return (l);
}


setval_t
osi_cas_skip_update(osi_set_t * l, setkey_t k, setval_t v, int overwrite)
{
    setval_t ov, new_ov;
    ptst_t *ptst;
    sh_node_pt preds[NUM_LEVELS], succs[NUM_LEVELS];
    sh_node_pt pred, succ, new = NULL, new_next, old_next;
    int i, level;

    ptst = critical_enter();

    succ = weak_search_predecessors(l, k, preds, succs);

  retry:
    ov = NULL;

    if (compare_keys(l, succ->k, k) == 0) {
	/* Already a @k node in the list: update its mapping. */
	new_ov = succ->v;
	do {
	    if ((ov = new_ov) == NULL) {
		/* Finish deleting the node, then retry. */
		READ_FIELD(level, succ->level);
		mark_deleted(succ, level & LEVEL_MASK);
		succ = strong_search_predecessors(l, k, preds, succs);
		goto retry;
	    }
	} while (overwrite && ((new_ov = CASPO(&succ->v, ov, v)) != ov));

	if (new != NULL)
	    free_node(ptst, new);
	goto out;
    }
#ifdef WEAK_MEM_ORDER
    /* Free node from previous attempt, if this is a retry. */
    if (new != NULL) {
	free_node(ptst, new);
	new = NULL;
    }
#endif

    /* Not in the list, so initialise a new node for insertion. */
    if (new == NULL) {
	new = alloc_node(ptst);
	new->k = k;
	new->v = v;
    }
    level = new->level;

    /* If successors don't change, this saves us some CAS operations. */
    for (i = 0; i < level; i++) {
	new->next[i] = succs[i];
    }

    /* We've committed when we've inserted at level 1. */
    WMB_NEAR_CAS();		/* make sure node fully initialised before inserting */
    old_next = CASPO(&preds[0]->next[0], succ, new);
    if (old_next != succ) {
	succ = strong_search_predecessors(l, k, preds, succs);
	goto retry;
    }

    /* Insert at each of the other levels in turn. */
    i = 1;
    while (i < level) {
	pred = preds[i];
	succ = succs[i];

	/* Someone *can* delete @new under our feet! */
	new_next = new->next[i];
	if (is_marked_ref(new_next))
	    goto success;

	/* Ensure forward pointer of new node is up to date. */
	if (new_next != succ) {
	    old_next = CASPO(&new->next[i], new_next, succ);
	    if (is_marked_ref(old_next))
		goto success;
	    assert(old_next == new_next);
	}

	/* Ensure we have unique key values at every level. */
	if (compare_keys(l, succ->k, k) == 0)
	    goto new_world_view;

	assert((compare_keys(l, pred->k, k) < 0) &&
	       (compare_keys(l, succ->k, k) > 0));

	/* Replumb predecessor's forward pointer. */
	old_next = CASPO(&pred->next[i], succ, new);
	if (old_next != succ) {
	  new_world_view:
	    RMB();		/* get up-to-date view of the world. */
	    (void)strong_search_predecessors(l, k, preds, succs);
	    continue;
	}

	/* Succeeded at this level. */
	i++;
    }

  success:
    /* Ensure node is visible at all levels before punting deletion. */
    WEAK_DEP_ORDER_WMB();
    if (check_for_full_delete(new)) {
	MB();			/* make sure we see all marks in @new. */
	do_full_delete(ptst, l, new, level - 1);
    }
  out:
    critical_exit(ptst);
    return (ov);
}

setval_t
osi_cas_skip_remove(osi_set_t * l, setkey_t k)
{
    setval_t v = NULL, new_v;
    ptst_t *ptst;
    sh_node_pt preds[NUM_LEVELS], x;
    int level, i;

    ptst = critical_enter();

    x = weak_search_predecessors(l, k, preds, NULL);
    if (compare_keys(l, x->k, k) > 0)
	goto out;

    READ_FIELD(level, x->level);
    level = level & LEVEL_MASK;

    /* Once we've marked the value field, the node is effectively deleted. */
    new_v = x->v;
    do {
	v = new_v;
	if (v == NULL)
	    goto out;
    } while ((new_v = CASPO(&x->v, v, NULL)) != v);

    /* Committed to @x: mark lower-level forward pointers. */
    WEAK_DEP_ORDER_WMB();	/* enforce above as linearisation point */
    mark_deleted(x, level);

    /*
     * We must swing predecessors' pointers, or we can end up with
     * an unbounded number of marked but not fully deleted nodes.
     * Doing this creates a bound equal to number of threads in the system.
     * Furthermore, we can't legitimately call 'free_node' until all shared
     * references are gone.
     */
    for (i = level - 1; i >= 0; i--) {
	if (CASPO(&preds[i]->next[i], x, get_unmarked_ref(x->next[i])) != x) {
	    if ((i != (level - 1)) || check_for_full_delete(x)) {
		MB();		/* make sure we see node at all levels. */
		do_full_delete(ptst, l, x, i);
	    }
	    goto out;
	}
    }

    free_node(ptst, x);

  out:
    critical_exit(ptst);
    return (v);
}


setval_t
osi_cas_skip_lookup(osi_set_t * l, setkey_t k)
{
    setval_t v = NULL;
    ptst_t *ptst;
    sh_node_pt x;

    ptst = critical_enter();

    x = weak_search_predecessors(l, k, NULL, NULL);
    if (compare_keys(l, x->k, k) == 0)
	READ_FIELD(v, x->v);

    critical_exit(ptst);
    return (v);
}


/* Hybrid Set/Queue Operations (Matt) */

/* Iterate over a sequential structure, calling callback_func
 * on each (undeleted) element visited. */

/* Each-element function passed to set_for_each */
typedef void (*osi_set_each_func) (osi_set_t * l, setval_t v, void *arg);
void
osi_cas_skip_for_each(osi_set_t * l, osi_set_each_func each_func, void *arg)
{
    sh_node_pt x, y, x_next, old_x_next;
    setkey_t x_next_k;
    ptst_t *ptst;
    int i;

    ptst = critical_enter();

    x = &l->head;
    for (i = NUM_LEVELS - 1; i >= 0; i--) {
	/* must revisit x at each level to see all
	 * nodes in the structure */
	y = x;

	for (;;) {
	    READ_FIELD(x_next, y->next[i]);
	    x_next = get_unmarked_ref(x_next);

	    READ_FIELD(x_next_k, x_next->k);
	    if (x_next_k == (setkey_t) SENTINEL_KEYMAX)
		break;

	    /* in our variation, a (stored) k is a v,
	     * ie, x_next_k is x_next_v */
	    each_func(l, x_next_k, arg);

	    y = x_next;
	}
    }

    critical_exit(ptst);
}
