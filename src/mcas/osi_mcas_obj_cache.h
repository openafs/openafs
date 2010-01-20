
#ifndef __OSI_MCAS_OBJ_CACHE_H
#define __OSI_MCAS_OBJ_CACHE_H

#include "../mcas/portable_defns.h"
#include "../mcas/ptst.h"
#include "../mcas/gc.h"

typedef int osi_mcas_obj_cache_t;

/* Create a new MCAS GC pool, and return its identifier, which
 * follows future calls */
void osi_mcas_obj_cache_create(osi_mcas_obj_cache_t * gc_id, size_t size,
	char *tag);	/* alignment? */

/* Allocate an object from the pool identified by
 * gc_id */
void *osi_mcas_obj_cache_alloc(osi_mcas_obj_cache_t gc_id);

/* Release object obj to its GC pool, identified by
 * gc_id */
void osi_mcas_obj_cache_free(osi_mcas_obj_cache_t gc_id, void *obj);

/* Terminate an MCAS GC pool */
void osi_mcas_obj_cache_destroy(osi_mcas_obj_cache_t gc_id);

#endif /* __OSI_MCAS_OBJ_CACHE_H */
