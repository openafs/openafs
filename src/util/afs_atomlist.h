/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * An afs_atomlist is a memory allocation facility.
 *
 * You can request atoms of storage from the list, and return them to
 * the list when you are done with them.  All the atoms in a given atom
 * list are the same size.
 *
 * The reason to use an afs_atomlist instead of allocating and freeing
 * memory directly is to avoid memory fragmentation.  Storage for the
 * atoms is allocated in blocks of the given size, then handed out as
 * requested.
 *
 * When the atom list is destroyed, all the atoms allocated from it are
 * freed, regardless of whether they have been returned to the list.
 *
 * The caller is responsible for doing any required locking.
 */

#ifndef ATOMLIST_H
#define ATOMLIST_H

#ifndef KERNEL
#include <stddef.h>
#endif

typedef struct afs_atomlist afs_atomlist;

/*
 * afs_atomlist_create() creates a new afs_atomlist.
 *
 * atom_size -- the number of bytes of space that afs_atomlist_get() should
 *              return
 *
 * block_size -- the number of bytes that afs_atomlist_get() should allocate
 *               at a time
 *
 * allocate() -- allocates memory
 *
 * deallocate() -- frees memory acquired via allocate()
 *
 * afs_atomlist_create() returns a pointer to the new afs_atomlist, or 0
 * on error.
 */

afs_atomlist *afs_atomlist_create(size_t atom_size, size_t block_size,
				  void *(*allocate) (size_t n)
				  , void (*deallocate) (void *p, size_t n)
    );

/*
 * afs_atomlist_destroy() destroys the given afs_atomlist, freeing it
 * and all space that may have been allocated from it.
 */
void
  afs_atomlist_destroy(afs_atomlist * al);

/*
 * afs_atomlist_get() returns a pointer to an unused atom.
 */

void *afs_atomlist_get(afs_atomlist * al);

/*
 * afs_atomlist_put() returns the given atom to the free list in the
 * given afs_atomlist.
 *
 * It is an error to put back an atom that was not requested via
 * afs_atomlist_get().
 *
 * It is an error to put back an atom that is already on the free list.
 */

void
  afs_atomlist_put(afs_atomlist * al, void *data);

#endif /* ATOMLIST_H */
