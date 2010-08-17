/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include "afs_atomlist.h"
#include "afs_lhash.h"
#ifndef KERNEL
/* for now, only turn on assertions in user-space code */
#include <assert.h>
#define CHECK_INVARIANTS
#endif /* !KERNEL */

#ifndef NULL
#define NULL 0
#endif

/* max hash table load factor */
enum { LOAD_FACTOR = 5 };

struct bucket {
    struct bucket *next;
    void *data;
    unsigned key;
};

struct afs_lhash {
    int (*equal) (const void *a, const void *b);

    void *(*allocate) (size_t n);

    void (*deallocate) (void *p, size_t n);

    size_t p;			/* index of next bucket to be split */
    size_t maxp;		/* upper bound on p during this expansion */

    size_t ndata;		/* count of data records in hash */

    size_t ltable;		/* logical table size */

    size_t ntable;		/* physical table size */
    struct bucket **table;

    afs_atomlist *bucket_list;	/* hash bucket allocator */

    size_t search_calls;	/* cumulative afs_lhash_search() call count */
    size_t search_tests;	/* cumulative afs_lhash_search() comparison count */
    size_t remove_calls;	/* cumulative afs_lhash_remove() call count */
    size_t remove_tests;	/* cumulative afs_lhash_remove() comparison count */
};

/*
 * make sure the given hash table can accomodate the given index
 * value; expand the bucket table if necessary
 *
 * returns 0 on success, <0 on failure
 */

static int
afs_lhash_accomodate(afs_lhash * lh, size_t max_index)
{
    /*
     * The usual approach to growing ntable to accomodate max_index
     * would be to double ntable enough times.  This would give us
     * an exponential backoff in how many times we need to grow
     * ntable.  However, there is a space tradeoff.
     *
     * Since afs_lhash may be used in an environment where memory is
     * constrained, we choose instead to grow ntable in fixed
     * increments.  This may have a larger total cost, but it is
     * amortized in smaller increments.
     *
     * If even this cost is too great, we can consider adopting the
     * two-level array approach mentioned in the paper.  This could
     * keep the sizes of the allocations more consistent, and also
     * reduce the amount of data copying.  However, we won't do that
     * until we see real results that show that the benefit of the
     * additional complexity is worth it.
     */
    enum { ntable_inc = 1024 / sizeof *lh->table };

    size_t new_ntable;
    struct bucket **new_table;
    size_t i;

    /* if the given index fits, we're done */

    if (max_index < lh->ntable)
	return 0;

    /* otherwise, determine new_ntable and allocate new_table */

    if (lh->ntable == (size_t) 0) {
	new_ntable = ntable_inc;
    } else {
	new_ntable = lh->ntable;
    }

    for (; !(max_index < new_ntable); new_ntable += ntable_inc)
	continue;

    new_table = lh->allocate(new_ntable * sizeof *lh->table);
    if (!new_table) {
	return -1;
    }

    /* initialize new_table from the old table */

    for (i = 0; i < lh->ntable; i++) {
	new_table[i] = lh->table[i];
    }
    for (i = lh->ntable; i < new_ntable; i++) {
	new_table[i] = 0;
    }

    /*
     * free the old table, if any, and switch to the new table
     *
     * (when called from afs_lhash_create(), the table is empty)
     */

    if (lh->table && lh->ntable) {
	lh->deallocate(lh->table, lh->ntable * sizeof *lh->table);
    }
    lh->ntable = new_ntable;
    lh->table = new_table;

    return 0;
}

/*
 * given a hash table and a key value, returns the index corresponding
 * to the key value
 */

static size_t
afs_lhash_address(const afs_lhash * lh, unsigned key)
{
    enum { prime = 1048583 };
    size_t h;
    size_t address;

    h = key % prime;		/* 0 <= h < prime */

    address = h % lh->maxp;
    if (address < lh->p) {
	address = h % (2 * lh->maxp);
    }

    return address;
}

/*
 * if possible, expand the logical size of the given hash table
 */
static void
afs_lhash_expand(afs_lhash * lh)
{
    size_t old_address;		/* index of bucket to split */
    size_t new_address;		/* index of new bucket */

    struct bucket *current_b;	/* for scanning down old chain */
    struct bucket *previous;

    struct bucket *last_of_new;	/* last element in new chain */

    /* save address to split */

    old_address = lh->p;

    /* determine new address, grow table if necessary */

    new_address = lh->maxp + lh->p;

    if (afs_lhash_accomodate(lh, new_address) < 0) {
	return;
    }

    /* adjust the state variables */

    /* this makes afs_lhash_address() work with respect
     * to the expanded table */

    lh->p++;
    if (lh->p == lh->maxp) {
	lh->maxp *= 2;
	lh->p = 0;
    }

    lh->ltable++;

#ifdef CHECK_INVARIANTS
    assert(lh->ltable - 1 == new_address);
    assert(lh->ltable <= lh->ntable);
#endif /* CHECK_INVARIANTS */

    /* relocate records to the new bucket */

    current_b = lh->table[old_address];
    previous = 0;
    last_of_new = 0;
    lh->table[new_address] = 0;

    while (current_b) {
	size_t addr;
	addr = afs_lhash_address(lh, current_b->key);
	if (addr == new_address) {
	    /* attach it to the end of the new chain */
	    if (last_of_new) {
		last_of_new->next = current_b;
	    } else {
		lh->table[new_address] = current_b;
	    }
	    if (previous) {
		previous->next = current_b->next;
	    } else {
		lh->table[old_address] = current_b->next;
	    }
	    last_of_new = current_b;
	    current_b = current_b->next;
	    last_of_new->next = 0;
	} else {
#ifdef CHECK_INVARIANTS
	    assert(addr == old_address);
#endif /* CHECK_INVARIANTS */
	    /* leave it on the old chain */
	    previous = current_b;
	    current_b = current_b->next;
	}
    }
}

afs_lhash *
afs_lhash_create(int (*equal) (const void *a, const void *b),
		 /* returns true if the elements pointed to by
		  * a and b are the same, false otherwise */
		 void *(*allocate) (size_t n), void (*deallocate) (void *p,
								   size_t n)
    )
{
    afs_lhash *lh;

    lh = allocate(sizeof *lh);
    if (!lh)
	return (afs_lhash *) 0;

    lh->equal = equal;
    lh->allocate = allocate;
    lh->deallocate = deallocate;

    lh->p = 0;
    lh->maxp = 16;

    lh->ltable = lh->maxp;

    lh->ndata = 0;

    lh->ntable = 0;
    lh->table = NULL;

    if (afs_lhash_accomodate(lh, lh->ltable - 1) < 0) {
	lh->deallocate(lh, sizeof *lh);
	return (afs_lhash *) 0;
    }
#ifdef CHECK_INVARIANTS
    assert(lh->ltable <= lh->ntable);
#endif /* CHECK_INVARIANTS */

    /* maybe the chunk size should be passed in for hashes, so we
     * can pass it down here */

    lh->bucket_list =
	afs_atomlist_create(sizeof(struct bucket), sizeof(long) * 1024,
			    allocate, deallocate);
#ifdef CHECK_INVARIANTS
    assert(lh->bucket_list);
#endif /* CHECK_INVARIANTS */

    lh->search_calls = 0;
    lh->search_tests = 0;
    lh->remove_calls = 0;
    lh->remove_tests = 0;

    return lh;
}

void
afs_lhash_destroy(afs_lhash * lh)
{
#ifdef CHECK_INVARIANTS
    assert(lh->ltable <= lh->ntable);
#endif /* CHECK_INVARIANTS */

    /*
     * first, free the buckets
     *
     * afs_atomlist_destroy() implicitly frees all the memory allocated
     * from the given afs_atomlist, so there is no need to go through
     * the hash table to explicitly free each bucket.
     */

    afs_atomlist_destroy(lh->bucket_list);

    /* then, free the table and the afs_lhash */

    lh->deallocate(lh->table, lh->ntable * sizeof *lh->table);
    lh->deallocate(lh, sizeof *lh);
}

void
afs_lhash_iter(afs_lhash * lh,
	       void (*f) (size_t index, unsigned key, void *data)
    )
{
    size_t i;

#ifdef CHECK_INVARIANTS
    assert(lh->ltable <= lh->ntable);
#endif /* CHECK_INVARIANTS */

    for (i = 0; i < lh->ltable; i++) {
	struct bucket *current_b;

	for (current_b = lh->table[i]; current_b; current_b = current_b->next) {
	    f(i, current_b->key, current_b->data);
	}
    }
}

void *
afs_lhash_search(afs_lhash * lh, unsigned key, const void *data)
{
    size_t k;
    struct bucket *previous;
    struct bucket *current_b;

    lh->search_calls++;

    k = afs_lhash_address(lh, key);
    for (previous = 0, current_b = lh->table[k]; current_b;
	 previous = current_b, current_b = current_b->next) {
	lh->search_tests++;
	if (lh->equal(data, current_b->data)) {

	    /*
	     * Since we found what we were looking for, move
	     * the bucket to the head of the chain.  The
	     * theory is that unused hash table entries will
	     * be left at the end of the chain, where they
	     * will not cause search times to increase.
	     *
	     * This optimization is both easy to understand
	     * and cheap to execute, so we go ahead and do
	     * it.
	     *
	     */

	    if (previous) {
		previous->next = current_b->next;
		current_b->next = lh->table[k];
		lh->table[k] = current_b;
	    }

	    return current_b->data;
	}
    }

    return 0;
}

void *
afs_lhash_rosearch(const afs_lhash * lh, unsigned key, const void *data)
{
    size_t k;
    struct bucket *current_b;

    k = afs_lhash_address(lh, key);
    for (current_b = lh->table[k]; current_b; current_b = current_b->next) {
	if (lh->equal(data, current_b->data)) {
	    return current_b->data;
	}
    }

    return 0;
}

void *
afs_lhash_remove(afs_lhash * lh, unsigned key, const void *data)
{
    size_t k;
    struct bucket **pprev;
    struct bucket *cur;

    lh->remove_calls++;

    k = afs_lhash_address(lh, key);
    for (pprev = 0, cur = lh->table[k]; cur;
	 pprev = &cur->next, cur = cur->next) {
	lh->remove_tests++;
	if (lh->equal(data, cur->data)) {
	    void *data = cur->data;

	    if (pprev) {
		*pprev = cur->next;
	    } else {
		lh->table[k] = cur->next;
	    }

	    afs_atomlist_put(lh->bucket_list, cur);

	    lh->ndata--;

	    return data;
	}
    }

    return 0;
}

int
afs_lhash_enter(afs_lhash * lh, unsigned key, void *data)
{
    size_t k;
    struct bucket *buck;

    buck = afs_atomlist_get(lh->bucket_list);
    if (!buck) {
	return -1;
    }

    buck->key = key;
    buck->data = data;

    k = afs_lhash_address(lh, key);

    buck->next = lh->table[k];
    lh->table[k] = buck;

    lh->ndata++;

    /*
     * The load factor is the number of data items in the hash
     * table divided by the logical table length.  We expand the
     * table when the load factor exceeds a predetermined limit
     * (LOAD_FACTOR).  To avoid floating point, we multiply both
     * sides of the inequality by the logical table length...
     */
    if (lh->ndata > LOAD_FACTOR * lh->ltable) {
	afs_lhash_expand(lh);
#if 0
	printf("lh->p = %d; lh->maxp = %d\n", lh->p, lh->maxp);
#endif
    }
#ifdef CHECK_INVARIANTS
    assert(lh->ndata <= LOAD_FACTOR * lh->ltable);
#endif /* CHECK_INVARIANTS */

    return 0;
}

int
afs_lhash_stat(afs_lhash * lh, struct afs_lhash_stat *sb)
{
    size_t k;

    int min_chain_length = -1;
    int max_chain_length = -1;
    size_t buckets = lh->ltable;
    size_t records = 0;

    if (!sb) {
	return -1;
    }
#ifdef CHECK_INVARIANTS
    assert(lh->ltable <= lh->ntable);
#endif /* CHECK_INVARIANTS */

    for (k = 0; k < lh->ltable; k++) {
	struct bucket *buck;
	int chain_length = 0;

	for (buck = lh->table[k]; buck; buck = buck->next) {
	    chain_length++;
	    records++;
	}

	if (min_chain_length == -1) {
	    min_chain_length = chain_length;
	}

	if (max_chain_length == -1) {
	    max_chain_length = chain_length;
	}

	if (chain_length < min_chain_length) {
	    min_chain_length = chain_length;
	}

	if (max_chain_length < chain_length) {
	    max_chain_length = chain_length;
	}
    }

    sb->min_chain_length = min_chain_length;
    sb->max_chain_length = max_chain_length;
    sb->buckets = buckets;
    sb->records = records;

#ifdef CHECK_INVARIANTS
    assert(lh->ndata == records);
#endif /* CHECK_INVARIANTS */

    sb->search_calls = lh->search_calls;
    sb->search_tests = lh->search_tests;
    sb->remove_calls = lh->remove_calls;
    sb->remove_tests = lh->remove_tests;

    return 0;
}
