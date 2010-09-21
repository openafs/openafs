/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * An afs_lhash is a linear hash table.  It is intended for use where
 * the number of elements in the hash table is not known in advance.
 * It grows as required in order to keep the average hash chain length
 * within certain bounds, which keeps average lookup times small.
 * Growth is efficient and does not require rehashing of all the
 * elements in the table at once.
 *
 * The caller is responsible for doing any required locking.
 *
 * For more information on the algorithm, see
 *
 *	Dynamic Hash Tables
 *	Per-Åke Larson (Per-Ake Larson)
 *	Communications of the ACM
 *	Vol. 31, No. 4 (April 1988), Pages 446-457
 */

#ifndef AFS_LHASH_H
#define AFS_LHASH_H

#if !defined(KERNEL) || defined(UKERNEL)
#include <stddef.h>
#endif

/*
 * The user is responsible for generating the key values corresponding
 * to the data in the hash table.  The key values should be distributed
 * over some interval [0,M], where M is sufficiently large, say
 * M > 2**20 (1048576).
 */

typedef struct afs_lhash afs_lhash;

struct afs_lhash_stat {
    size_t min_chain_length;
    size_t max_chain_length;
    size_t buckets;
    size_t records;

    size_t search_calls;	/* cumulative afs_lhash_search() call count */
    size_t search_tests;	/* cumulative afs_lhash_search() comparison count */
    size_t remove_calls;	/* cumulative afs_lhash_remove() call count */
    size_t remove_tests;	/* cumulative afs_lhash_remove() comparison count */
};

/*
 * afs_lhash_create() allocates a new hash table.
 *
 * equal() -- compares table elements for equality
 *
 * allocate() -- allocates memory
 *
 * deallocate() -- frees memory acquired via allocate()
 *
 * afs_lhash_create() returns a pointer to the new hash table, or 0 on
 * error.
 */

afs_lhash *afs_lhash_create(int (*equal) (const void *a, const void *b)
			    /* returns true if the elements pointed to by
			     * a and b are the same, false otherwise */
			    , void *(*allocate) (size_t n)
			    , void (*deallocate) (void *p, size_t n)
    );

/*
 * afs_lhash_destroy() destroys the given hash table.
 *
 * Note that the caller is responsible for freeing the table elements if
 * required.
 */

void
  afs_lhash_destroy(afs_lhash * lh);

/*
 * afs_lhash_iter() calls the given function for each element of the
 * given hash table.
 *
 * The function called with the key and data pointer for each element of
 * the hash table.  It is also given the hash table index value, in case
 * the function wants to compute how many elements are in each bucket.
 */

void

  afs_lhash_iter(afs_lhash * lh,
		 void (*f) (size_t index, unsigned key, void *data)
    );

/*
 * afs_lhash_search() searches the given hash table for the given key
 * and element.
 *
 * The element may be incomplete; it is compared to elements in the hash
 * table using the hash table's equal() function.
 *
 * If the element is found, it is moved to the front of its hash chain
 * list to try to make future lookups faster.
 *
 * afs_lhash_search() returns a pointer to the desired data element if
 * found, 0 otherwise.
 */

void *afs_lhash_search(afs_lhash * lh, unsigned key, const void *data);

/*
 * afs_lhash_rosearch() searches the given hash table for the given key
 * and element.
 *
 * The element may be incomplete; it is compared to elements in the hash
 * table using the hash table's equal() function.
 *
 * The hash table is not modified.
 *
 * afs_lhash_rosearch() returns a pointer to the desired data element if
 * found, 0 otherwise.
 */

void *afs_lhash_rosearch(const afs_lhash * lh, unsigned key,
			 const void *data);

/*
 * afs_lhash_remove() removes an item matching the given key and data
 * from the hash table.
 *
 * afs_lhash_remove() returns a pointer to the item removed on success,
 * 0 otherwise.
 */

void *afs_lhash_remove(afs_lhash * lh, unsigned key, const void *data);

/*
 * afs_lhash_enter() enters the given data element into the given hash
 * table using the given key.
 *
 * It is not an error to enter the same [key, data] twice, so the
 * caller should check for duplicates if that is important.
 *
 * afs_lhash_enter() returns 0 on success, nonzero otherwise.
 */

int
  afs_lhash_enter(afs_lhash * lh, unsigned key, void *data);

/*
 * afs_lhash_stat() writes certain statistics about the given hash table
 * into the given afs_lhash_stat structure.
 *
 * afs_lhash_stat() returns 0 on success, nonzero otherwise.
 */

int
  afs_lhash_stat(afs_lhash * lh, struct afs_lhash_stat *sb);

#endif /* AFS_LHASH_H */
