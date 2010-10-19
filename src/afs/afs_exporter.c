/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics gathering code */

struct afs_exporter *root_exported = 0;	/* Head of "exporters" link list */
afs_lock_t afs_xexp;


/* Add a new "afs exporter" entry to the table of exporters. The default initial values of the entry are passed in as parameters. */
static afs_int32 init_xexported = 0;

struct afs_exporter *
exporter_add(afs_int32 size, struct exporterops *ops, afs_int32 state,
	     afs_int32 type, char *data)
{
    struct afs_exporter *ex, *op;
    afs_int32 length;

    AFS_STATCNT(exporter_add);
    if (!init_xexported) {
	init_xexported = 1;
	LOCK_INIT(&afs_xexp, "afs_xexp");
    }
    length = (size ? size : sizeof(struct afs_exporter));
    ex = afs_osi_Alloc(length);
    osi_Assert(ex != NULL);
    memset(ex, 0, length);
    ObtainWriteLock(&afs_xexp, 308);
    for (op = root_exported; op; op = op->exp_next) {
	if (!op->exp_next)
	    break;
    }
    if (op)
	op->exp_next = ex;
    else
	root_exported = ex;
    ReleaseWriteLock(&afs_xexp);
    ex->exp_next = 0;
    ex->exp_op = ops;
    ex->exp_states = state;
    ex->exp_data = data;
    ex->exp_type = type;
    return ex;
}


/* Returns the "afs exporter" structure of type, "type". NULL is returned if not found */
struct afs_exporter *
exporter_find(int type)
{
    struct afs_exporter *op;

    AFS_STATCNT(exporter_add);
    ObtainReadLock(&afs_xexp);
    for (op = root_exported; op; op = op->exp_next) {
	if (op->exp_type == type) {
	    ReleaseReadLock(&afs_xexp);
	    return op;
	}
    }
    ReleaseReadLock(&afs_xexp);
    return (struct afs_exporter *)0;
}


void
shutdown_exporter(void)
{
    struct afs_exporter *ex, *op;

    for (op = root_exported; op; op = ex) {
	ex = op->exp_next;
	afs_osi_Free(op, sizeof(struct afs_exporter));
    }
    init_xexported = 0;
}
