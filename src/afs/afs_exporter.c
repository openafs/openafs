/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* statistics gathering code */

struct afs_exporter	*root_exported=0;   /* Head of "exporters" link list */
afs_lock_t		afs_xexp;


/* Add a new "afs exporter" entry to the table of exporters. The default initial values of the entry are passed in as parameters. */
static afs_int32 init_xexported = 0;
struct afs_exporter *exporter_add(size, ops, state, type, data)
afs_int32 size, state, type;
struct exporterops *ops;
char *data;
{
    struct afs_exporter *ex, *op;
    afs_int32 length;

    AFS_STATCNT(exporter_add);
    if (!init_xexported) {
	init_xexported = 1;
	LOCK_INIT(&afs_xexp, "afs_xexp");
    }
    length = (size ? size : sizeof(struct afs_exporter));
    ex = (struct afs_exporter *) afs_osi_Alloc(length);
    bzero((char *)ex, length);
    MObtainWriteLock(&afs_xexp,308);
    for (op = root_exported; op; op = op->exp_next) {
	if (!op->exp_next)
	    break;
    }
    if (op) 
	op->exp_next = ex;
    else
	root_exported = ex;
    MReleaseWriteLock(&afs_xexp);
    ex->exp_next = 0;
    ex->exp_op = ops;
    ex->exp_states = state;
    ex->exp_data = data;
    ex->exp_type = type;
    return ex;
}


/* Returns the "afs exporter" structure of type, "type". NULL is returned if not found */
struct afs_exporter *exporter_find(type)
int type;
{
    struct afs_exporter *op;

    AFS_STATCNT(exporter_add);
    MObtainReadLock(&afs_xexp);
    for (op = root_exported; op; op = op->exp_next) {
	if (op->exp_type == type) {
	    MReleaseReadLock(&afs_xexp);
	    return op;
	}
    }
    MReleaseReadLock(&afs_xexp);
    return (struct afs_exporter *)0;
}


shutdown_exporter() 
{
    struct afs_exporter *ex, *op;

    for (op = root_exported; op; op = ex) {
	ex = op->exp_next;
	afs_osi_Free(op, sizeof(struct afs_exporter));
    }
    init_xexported = 0;
}
