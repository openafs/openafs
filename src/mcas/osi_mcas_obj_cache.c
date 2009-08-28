#include "osi_mcas_obj_cache.h"
#include <osi/osi_includes.h>
#include <osi/osi_types.h>
#include <afs/afsutil.h>

void
osi_mcas_obj_cache_create(osi_mcas_obj_cache_t * gc_id, size_t size,
	char *tag)
{
    ViceLog(7,
	    ("osi_mcas_obj_cache_create: size, adjsize %d\n", size,
	     size + sizeof(int *)));

    *(int *)gc_id = gc_add_allocator(size + sizeof(int *), tag);
}

void gc_trace(int alloc_id);
int gc_get_tag(int alloc_id);
int gc_get_blocksize(int alloc_id);

void *
osi_mcas_obj_cache_alloc(osi_mcas_obj_cache_t gc_id)
{
    ptst_t *ptst;
    void *obj;

    ptst = critical_enter();
    obj = (void *)gc_alloc(ptst, gc_id);
    critical_exit(ptst);

    ViceLog(11,
			("GC: osi_mcas_obj_cache_alloc: block of size %d "
#if OSI_DATAMODEL_IS(OSI_ILP32_ENV)
			 "0x%lx"
#else
			 "0x%llx"
#endif
			 " (%s)\n",
			 gc_get_blocksize(gc_id),
			 obj,
			 gc_get_tag(gc_id)));

    return (obj);
}

void
osi_mcas_obj_cache_free(osi_mcas_obj_cache_t gc_id, void *obj)
{
    ptst_t *ptst;

    ViceLog(11,
			("GC: osi_mcas_obj_cache_free: block of size %d "
#if OSI_DATAMODEL_IS(OSI_ILP32_ENV)
			 "0x%lx"
#else
			 "0x%llx"
#endif
			 " (%s)\n",
			 gc_get_blocksize(gc_id),
			 obj,
			 gc_get_tag(gc_id)));

    ptst = critical_enter();
    gc_free(ptst, (void *)obj, gc_id);
    critical_exit(ptst);
}

void
osi_mcas_obj_cache_destroy(osi_mcas_obj_cache_t gc_id)
{
    /* TODO:  implement, will need to implement gc_remove_allocator and related
     * modifications in gc.c */
    return;
}
