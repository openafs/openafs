/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_alloc.c - Linux memory allocation routines.
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "h/mm.h"
#include "h/slab.h"

#include "afs_atomlist.h"
#include "afs_lhash.h"

#define MAX_KMALLOC_SIZE PAGE_SIZE	/* Max we should alloc with kmalloc */
#define MAX_BUCKET_LEN 30	/* max. no. of entries per buckets we expect to see */
#define STAT_INTERVAL 8192	/* we collect stats once every STAT_INTERVAL allocs */

/* types of alloc */
#define KM_TYPE 1		/* kmalloc */
#define VM_TYPE 2		/* vmalloc */

struct osi_linux_mem {
    void *chunk;
};

/* These assume 32-bit pointers */
#define MEMTYPE(A) (((unsigned long)A) & 0x3)
#define MEMADDR(A) (void *)((unsigned long)(A) & (~0x3))

/* globals */
afs_atomlist *al_mem_pool;	/* pool of osi_linux_mem structures */
afs_lhash *lh_mem_htab;		/* mem hash table */
unsigned int allocator_init = 0;	/* has the allocator been initialized? */
unsigned int afs_linux_cur_allocs = 0;
unsigned int afs_linux_total_allocs = 0;
unsigned int afs_linux_hash_verify_count = 0;	/* used by hash_verify */
struct afs_lhash_stat afs_linux_lsb;	/* hash table statistics */
unsigned int afs_linux_hash_bucket_dist[MAX_BUCKET_LEN];	/* bucket population distribution in our hash table */

#if defined(AFS_LINUX24_ENV)
#include "h/vmalloc.h"
#else
/* externs : can we do this in a better way. Including vmalloc.h causes other
 * problems.*/
extern void vfree(void *addr);
extern void *vmalloc(unsigned long size);
#endif

/* Allocator support functions (static) */

static int
hash_equal(const void *a, const void *b)
{
    return (MEMADDR(((struct osi_linux_mem *)a)->chunk) ==
	    MEMADDR(((struct osi_linux_mem *)b)->chunk));

}

/* linux_alloc : Allocates memory from the linux kernel. It uses 
 *               kmalloc if possible. Otherwise, we use vmalloc. 
 * Input:
 *  asize - size of memory required in bytes
 * Return Values:
 *  returns NULL if we failed to allocate memory.
 *  or pointer to memory if we succeeded.
 */
static void *
linux_alloc(unsigned int asize, int drop_glock)
{
    void *new = NULL;
    int max_retry = 10;
    int haveGlock = ISAFS_GLOCK();

    /*  if we can use kmalloc use it to allocate the required memory. */
    while (!new && max_retry) {
	if (asize <= MAX_KMALLOC_SIZE) {
	    new = (void *)(unsigned long)kmalloc(asize,
#ifdef GFP_NOFS
						 GFP_NOFS
#else
						 GFP_KERNEL
#endif
		);
	    if (new)		/* piggy back alloc type */
		new = (void *)(KM_TYPE | (unsigned long)new);
	} else {
	    new = (void *)vmalloc(asize);
	    if (new)		/* piggy back alloc type */
		new = (void *)(VM_TYPE | (unsigned long)new);
	}

	if (!new) {
#ifdef set_current_state
	    set_current_state(TASK_INTERRUPTIBLE);
#else
	    current->state = TASK_INTERRUPTIBLE;
#endif
	    if (drop_glock && haveGlock)
		AFS_GUNLOCK();
	    schedule_timeout(HZ);
	    if (drop_glock && haveGlock)
		AFS_GLOCK();
#ifdef set_current_state
	    set_current_state(TASK_RUNNING);
#else
	    current->state = TASK_RUNNING;
#endif
	    --max_retry;
	}
    }
    if (new)
	memset(MEMADDR(new), 0, asize);

    return new;
}

static void
linux_free(void *p)
{

    /* mask out the type information from the pointer and
     *  use the appropriate free routine to free the chunk.
     */
    switch (MEMTYPE(p)) {
    case KM_TYPE:
	kfree(MEMADDR(p));
	break;
    case VM_TYPE:
	vfree(MEMADDR(p));
	break;
    default:
	printf("afs_osi_Free: Asked to free unknown type %d at 0x%lx\n",
	       (int)MEMTYPE(p), (unsigned long)MEMADDR(p));
	break;
    }

}

/* hash_chunk() receives a pointer to a chunk and hashes it to produce a
 *            key that the hashtable can use. The key is obtained by 
 *            right shifting out the 2 LSBs and then multiplying the
 *            result by a constant no. and dividing it with a large prime.
 */
#define HASH_CONST   32786
#define HASH_PRIME   79367
static unsigned
hash_chunk(void *p)
{
    unsigned int key;

    key = (unsigned int)(long)p >> 2;
    key = (key * HASH_CONST) % HASH_PRIME;

    return key;
}

/* hash_free() : Invoked by osi_linux_free_afs_memory(), thru 
 *          afs_lhash_iter(), this function is called by the lhash
 *          module for every entry in the hash table. hash_free
 *          frees the memory associated with the entry as well
 *          as returning the osi_linux_mem struct to its pool.
 */
static void
hash_free(size_t index, unsigned key, void *data)
{
    linux_free(((struct osi_linux_mem *)data)->chunk);
    afs_atomlist_put(al_mem_pool, data);
}

/* hash_verify() is invoked by osi_linux_verify_alloced_memory() thru
 *   afs_lhash_iter() and is called by the lhash module for every element
 *   in the hash table. 
 *  hash_verify() verifies (within limits) that the memory passed to it is
 *  valid.
 */
static void
hash_verify(size_t index, unsigned key, void *data)
{
    struct osi_linux_mem *lmp = (struct osi_linux_mem *)data;
    int memtype;

    memtype = MEMTYPE(lmp->chunk);
    if (memtype != KM_TYPE && memtype != VM_TYPE) {
	printf
	    ("osi_linux_verify_alloced_memory: unknown type %d at 0x%lx, index=%lu\n",
	     (int)memtype, (unsigned long)lmp->chunk, (unsigned long)index);
    }
    afs_linux_hash_verify_count++;
}


/* local_free() : wrapper for vfree(), to deal with incompatible protoypes */
static void
local_free(void *p, size_t n)
{
    vfree(p);
}

/* linux_alloc_init(): Initializes the kernel memory allocator. As part
 *    of this process, it also initializes a pool of osi_linux_mem
 *    structures as well as the hash table itself.
 *  Return values:
 *    0 - failure
 *    1 - success
 */
static int
linux_alloc_init()
{
    /* initiate our pool of osi_linux_mem structs */
    al_mem_pool =
	afs_atomlist_create(sizeof(struct osi_linux_mem), sizeof(long) * 1024,
			    (void *)vmalloc, local_free);
    if (!al_mem_pool) {
	printf("afs_osi_Alloc: Error in initialization(atomlist_create)\n");
	return 0;
    }

    /* initialize the hash table to hold references to alloc'ed chunks */
    lh_mem_htab = afs_lhash_create(hash_equal, (void *)vmalloc, local_free);
    if (!lh_mem_htab) {
	printf("afs_osi_Alloc: Error in initialization(lhash_create)\n");
	return 0;
    }

    return 1;

}

/* hash_bucket_stat() : Counts the no. of elements in each bucket and
 *   stores results in our bucket stats vector.
 */
static unsigned int cur_bucket, cur_bucket_len;
static void
hash_bucket_stat(size_t index, unsigned key, void *data)
{
    if (index == cur_bucket) {
	/* while still on the same bucket, inc len & return */
	cur_bucket_len++;
	return;
    } else {			/* if we're on the next bucket, store the distribution */
	if (cur_bucket_len < MAX_BUCKET_LEN)
	    afs_linux_hash_bucket_dist[cur_bucket_len]++;
	else
	    printf
		("afs_get_hash_stats: Warning! exceeded max bucket len %d\n",
		 cur_bucket_len);
	cur_bucket = index;
	cur_bucket_len = 1;
    }
}

/* get_hash_stats() : get hash table statistics */
static void
get_hash_stats()
{
    int i;

    afs_lhash_stat(lh_mem_htab, &afs_linux_lsb);

    /* clear out the bucket stat vector */
    for (i = 0; i < MAX_BUCKET_LEN; i++, afs_linux_hash_bucket_dist[i] = 0);
    cur_bucket = cur_bucket_len = 00;

    /* populate the bucket stat vector */
    afs_lhash_iter(lh_mem_htab, hash_bucket_stat);
}

/************** Linux memory allocator interface functions **********/

#if defined(AFS_LINUX24_ENV)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
DEFINE_MUTEX(afs_linux_alloc_sem);
#else
DECLARE_MUTEX(afs_linux_alloc_sem);
#endif
#else
struct semaphore afs_linux_alloc_sem = MUTEX;
#endif

void *
osi_linux_alloc(unsigned int asize, int drop_glock)
{
    void *new = NULL;
    struct osi_linux_mem *lmem;

    new = linux_alloc(asize, drop_glock);	/* get a chunk of memory of size asize */

    if (!new) {
	printf("afs_osi_Alloc: Can't vmalloc %d bytes.\n", asize);
	return new;
    }

    down(&afs_linux_alloc_sem);

    /* allocator hasn't been initialized yet */
    if (allocator_init == 0) {
	if (linux_alloc_init() == 0) {
	    goto error;
	}
	allocator_init = 1;	/* initialization complete */
    }

    /* get an atom to store the pointer to the chunk */
    lmem = (struct osi_linux_mem *)afs_atomlist_get(al_mem_pool);
    if (!lmem) {
	printf("afs_osi_Alloc: atomlist_get() failed.");
	goto free_error;
    }
    /* store the chunk reference */
    lmem->chunk = new;

    /* hash in the chunk */
    if (afs_lhash_enter(lh_mem_htab, hash_chunk(new), lmem) != 0) {
	printf("afs_osi_Alloc: lhash_enter failed\n");
	goto free_error;
    }
    afs_linux_cur_allocs++;	/* no. of current allocations */
    afs_linux_total_allocs++;	/* total no. of allocations done so far */
    if ((afs_linux_cur_allocs % STAT_INTERVAL) == 0) {
	get_hash_stats();
    }
  error:
    up(&afs_linux_alloc_sem);
    return MEMADDR(new);

  free_error:
    if (new) {
	linux_free(new);
    }
    new = NULL;
    goto error;


}

/* osi_linux_free() - free chunk of memory passed to us.
 */
void
osi_linux_free(void *addr)
{
    struct osi_linux_mem lmem, *lmp;

    down(&afs_linux_alloc_sem);

    lmem.chunk = addr;
    /* remove this chunk from our hash table */
    if ((lmp =
	 (struct osi_linux_mem *)afs_lhash_remove(lh_mem_htab,
						  hash_chunk(addr), &lmem))) {
	linux_free(lmp->chunk);	/* this contains the piggybacked type info */
	afs_atomlist_put(al_mem_pool, lmp);	/* return osi_linux_mem struct to pool */
	afs_linux_cur_allocs--;
    } else {
	printf("osi_linux_free: failed to remove chunk from hashtable\n");
    }

    up(&afs_linux_alloc_sem);
}

/* osi_linux_free_afs_memory() - free all chunks of memory allocated.
 */
void
osi_linux_free_afs_memory(void)
{
    down(&afs_linux_alloc_sem);

    if (allocator_init) {
	/* iterate through all elements in the hash table and free both 
	 * the chunk and the atom associated with it.
	 */
	afs_lhash_iter(lh_mem_htab, hash_free);

	/*  free the atomlist. */
	afs_atomlist_destroy(al_mem_pool);

	/* free the hashlist. */
	afs_lhash_destroy(lh_mem_htab);

	/* change the state so that the allocator is now uninitialized. */
	allocator_init = 0;
    }
    up(&afs_linux_alloc_sem);
}

/* osi_linux_verify_alloced_memory(): verify all chunks of alloced memory in
 *          our hash table.
 */
void
osi_linux_verify_alloced_memory()
{
    down(&afs_linux_alloc_sem);

    /* count of times hash_verify was called. reset it to 0 before iteration */
    afs_linux_hash_verify_count = 0;

    /* iterate thru elements in the hash table */
    afs_lhash_iter(lh_mem_htab, hash_verify);

    if (afs_linux_hash_verify_count != afs_linux_cur_allocs) {
	/* hmm, some pieces of memory are missing. */
	printf
	    ("osi_linux_verify_alloced_memory: %d chunks of memory are not accounted for during verify!\n",
	     afs_linux_hash_verify_count - afs_linux_cur_allocs);
    }

    up(&afs_linux_alloc_sem);
    return;
}
