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


#ifdef KERNEL
#include "afs_atomlist.h"
#else /* KERNEL */
#include "afs_atomlist.h"
#endif /* KERNEL */

/*
 * The afs_atomlist abstract data type is for efficiently allocating
 * space for small structures.
 *
 * The atoms in an afs_atomlist are allocated in blocks.  The blocks
 * are chained together so that they can be freed when the afs_atomlist
 * is destroyed.  When a new block is allocated, its atoms are chained
 * together and added to the free list of atoms.
 *
 * If the requested atom size is smaller than the size of a pointer,
 * afs_atomlist_create() silently increases the atom size.  If
 * the atom size would result in incorrectly aligned pointers,
 * afs_atomlist_create() silently increases the atom size as necessary.
 *
 * A block of atoms is organized as follows.
 *
 * ---------------------------------------------------------------
 * | atom | atom | atom | ... | atom | nextblock | wasted space* |
 * ---------------------------------------------------------------
 * \____ atoms_per_block atoms ______/
 *
 * (*) A block may or may not contain wasted space at the end.  The
 * amount of wasted space depends on the size of a block, the size of an
 * atom, and the size of the pointer to the next block.  For instance,
 * if a block is 4096 bytes, an atom is 12 bytes, and a pointer is 4
 * bytes, there is no wasted space in a block.
 *
 * The pointer to the next block is stored AFTER all the atoms in the
 * block.  Here's why.
 *
 * If we put the pointer to the next block before the atoms,
 * followed immediately by the atoms, we would be assuming that the
 * atoms could be aligned on a pointer boundary.
 *
 * If we tried to solve the alignment problem by allocating an entire
 * atom for the pointer to the next block, we might waste space
 * gratuitously.  Say a block is 4096 bytes, an atom is 24 bytes, and a
 * pointer is 8 bytes.  In this case a block can hold 170 atoms, with 16
 * bytes left over.  This isn't enough space for another atom, but it is
 * enough space for the pointer to the next block.  There is no need to
 * use one of the atoms to store the pointer to the next block.
 *
 * So, we store the pointer to the next block after the end of the atoms
 * in the block.  In the worst case, the block size is an exact multiple
 * of the atom size, and we waste an entire atom to store the pointer to
 * the next block.  But we hope it is more typical that there is enough
 * extra space after the atoms to store the pointer to the next block.
 *
 * A more sophisticated scheme would keep the pointers to the atom
 * blocks in a separate list of blocks.  It would eliminate the
 * fragmentation of the atom blocks in the case where the block size
 * is a multiple of the atom size.  However, it is more complicated to
 * understand and to implement, so I chose not to do it at this time.
 * If fragmentation turns out to be a serious enough issue, we can
 * change the afs_atomlist implementation without affecting its users.
 */

struct afs_atomlist {
    size_t atom_size;
    size_t block_size;
    size_t atoms_per_block;
    void *(*allocate) (size_t n);
    void (*deallocate) (void *p, size_t n);
    void *atom_head;		/* pointer to head of atom free list */
    void *block_head;		/* pointer to block list */
};

afs_atomlist *
afs_atomlist_create(size_t atom_size, size_t block_size,
		    void *(*allocate) (size_t n)
		    , void (*deallocate) (void *p, size_t n)
    )
{
    afs_atomlist *al;
    size_t atoms_per_block;
    size_t extra_space;

    /*
     * Atoms must be at least as big as a pointer in order for
     * our implementation of the atom free list to work.
     */
    if (atom_size < sizeof(void *)) {
	atom_size = sizeof(void *);
    }

    /*
     * Atoms must be a multiple of the size of a pointer
     * so that the pointers in the atom free list will be
     * properly aligned.
     */
    if (atom_size % sizeof(void *) != (size_t) 0) {
	size_t pad = sizeof(void *) - (atom_size % sizeof(void *));
	atom_size += pad;
    }

    /*
     * Blocks are the unit of memory allocation.
     *
     * 1) Atoms are allocated out of blocks.
     *
     * 2) sizeof(void *) bytes in each block, aligned on a sizeof(void *)
     * boundary, are used to chain together the blocks so that they can
     * be freed later.  This reduces the space in each block for atoms.
     * It is intended that atoms should be small relative to the size of
     * a block, so this should not be a problem.
     *
     * At a minimum, a block must be big enough for one atom and
     * a pointer to the next block.
     */
    if (block_size < atom_size + sizeof(void *))
	return 0;

    atoms_per_block = block_size / atom_size;
    extra_space = block_size - (atoms_per_block * atom_size);
    if (extra_space < sizeof(void *)) {
	if (atoms_per_block < (size_t) 2) {
	    return 0;		/* INTERNAL ERROR! */
	}
	atoms_per_block--;
    }

    al = allocate(sizeof *al);
    if (!al)
	return 0;

    al->atom_size = atom_size;
    al->block_size = block_size;
    al->allocate = allocate;
    al->deallocate = deallocate;
    al->atom_head = 0;
    al->block_head = 0;
    al->atoms_per_block = atoms_per_block;

    return al;
}

void
afs_atomlist_destroy(afs_atomlist * al)
{
    void *cur;
    void *next;

    for (cur = al->block_head; cur; cur = next) {
	next = *(void **)((char *)cur + al->atoms_per_block * al->atom_size);
	al->deallocate(cur, al->block_size);
    }
    al->deallocate(al, sizeof *al);
}

void *
afs_atomlist_get(afs_atomlist * al)
{
    void *data;

    /* allocate a new block if necessary */
    if (!al->atom_head) {
	void *block;
	void *p;
	size_t i;

	block = al->allocate(al->block_size);
	if (!block) {
	    return 0;
	}

	/* add this block to the chain of allocated blocks */
	*(void **)((char *)block + al->atoms_per_block * al->atom_size) =
	    al->block_head;
	al->block_head = block;

	/* add this block's atoms to the atom free list */
	p = block;
	for (i = 0; i + 1 < al->atoms_per_block; i++) {
	    *(void **)p = (char *)p + al->atom_size;
	    p = (char *)p + al->atom_size;
	}
	*(void **)p = 0;
	al->atom_head = block;
    }

    if (!al->atom_head) {
	return 0;		/* INTERNAL ERROR */
    }

    data = al->atom_head;
    al->atom_head = *(void **)data;

    return data;
}

void
afs_atomlist_put(afs_atomlist * al, void *data)
{
    *(void **)data = al->atom_head;
    al->atom_head = data;
}
