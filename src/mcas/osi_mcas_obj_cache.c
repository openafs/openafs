#include "osi_mcas_obj_cache.h"
#include <afs/afsutil.h>

void
osi_mcas_obj_cache_create(osi_mcas_obj_cache_t * gc_id, size_t size)
{
    ViceLog(7,
	    ("osi_mcas_obj_cache_create: size, adjsize %d\n", size,
	     size + sizeof(int *)));

    *(int *)gc_id = gc_add_allocator(size + sizeof(int *));
}

void gc_trace(int alloc_id);
int gc_get_blocksize(int alloc_id);

void *
osi_mcas_obj_cache_alloc(osi_mcas_obj_cache_t gc_id)
{
    ptst_t *ptst;
    void *obj;

#if MCAS_ALLOC_DISABLED
#warning XXXXX mcas allocator cache is DISABLED for debugging!!
    obj = malloc(gc_get_blocksize(gc_id));
#else
    ptst = critical_enter();
    obj = (void *)gc_alloc(ptst, gc_id);
    critical_exit(ptst);
#endif
    return (obj);
}

void
osi_mcas_obj_cache_free(osi_mcas_obj_cache_t gc_id, void *obj)
{
    ptst_t *ptst;

#if MCAS_ALLOC_DISABLED
#warning XXXXX mcas allocator cache is DISABLED for debugging!!
#else
    ptst = critical_enter();
    gc_free(ptst, (void *)obj, gc_id);
    critical_exit(ptst);
#endif
}

void
osi_mcas_obj_cache_destroy(osi_mcas_obj_cache_t gc_id)
{
    /* TODO:  implement, will need to implement gc_remove_allocator and related
     * modifications in gc.c */
    return;
}
