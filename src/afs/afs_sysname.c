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


#include "afs/sysincludes.h"
#include "afsincludes.h"

/* Dictates behavior of @sys path expansion. */
int afs_atsys_type = AFS_ATSYS_INTERNAL;

struct afs_sysnames *afs_global_sysnames;
int afs_sysnamegen;
afs_kmutex_t afs_sysname_lock;

/*
 * To read or write the list of sysnames, hold afs_sysname_lock. (Be sure to
 * acquire afs_sysname_lock before GLOCK, if acquiring both.)
 *
 * Note that afs_sysname_lock is an afs_kmutex_t, not an afs_rwlock_t. This
 * allows us to use the lock outside of GLOCK.
 */

static void
sysnamelist_destroy(struct afs_sysnames *sysnames)
{
    int name_i;

    if (sysnames == NULL) {
	return;
    }

    for (name_i = 0; name_i < MAXNUMSYSNAMES; name_i++) {
	if (sysnames->namelist[name_i] != NULL) {
	    afs_osi_Free(sysnames->namelist[name_i], MAXSYSNAME);
	}
    }

    memset(sysnames, 0, sizeof(*sysnames));
}

static int
sysnamelist_init(struct afs_sysnames *sysnames)
{
    int name_i;
    int code;

    for (name_i = 0; name_i < MAXNUMSYSNAMES; name_i++) {
	sysnames->namelist[name_i] = afs_osi_Alloc(MAXSYSNAME);
	if (sysnames->namelist[name_i] == NULL) {
	    code = ENOMEM;
	    goto error;
	}
    }

    /* Initialize the new sysname list with 1 entry: SYS_NAME */
    if (strlcpy(sysnames->namelist[0], SYS_NAME, MAXSYSNAME) >= MAXSYSNAME) {
	code = E2BIG;
	goto error;
    }
    sysnames->namecount = 1;

    return 0;

 error:
    sysnamelist_destroy(sysnames);
    return code;
}

int
afs_sysname_init(void)
{
    int code;
    static int init_done;
    static struct afs_sysnames global_sysnames;

    if (init_done) {
	return 0;
    }

    MUTEX_INIT(&afs_sysname_lock, "afs_sysname_lock", MUTEX_DEFAULT, 0);

    code = sysnamelist_init(&global_sysnames);
    if (code != 0) {
	goto error;
    }

    afs_global_sysnames = &global_sysnames;
    afs_sysnamegen++;

    init_done = 1;
    return 0;

 error:
    afs_sysname_shutdown();
    return code;
}

void
afs_sysname_shutdown(void)
{
    sysnamelist_destroy(afs_global_sysnames);
    afs_global_sysnames = NULL;

    MUTEX_DESTROY(&afs_sysname_lock);
}
