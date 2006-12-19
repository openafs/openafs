/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_cred.c - Linux cred handling routines.
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"

cred_t *
crget(void)
{
    cred_t *tmp;

#if !defined(GFP_NOFS)
#define GFP_NOFS GFP_KERNEL
#endif
    tmp = kmalloc(sizeof(cred_t), GFP_NOFS);
    if (!tmp)
	    osi_Panic("crget: No more memory for creds!\n");

    tmp->cr_ref = 1;
    return tmp;
}

void
crfree(cred_t * cr)
{
    if (cr->cr_ref > 1) {
	cr->cr_ref--;
	return;
    }

#if defined(AFS_LINUX26_ENV)
    put_group_info(cr->cr_group_info);
#endif

    kfree(cr);
}


/* Return a duplicate of the cred. */
cred_t *
crdup(cred_t * cr)
{
    cred_t *tmp = crget();

    tmp->cr_uid = cr->cr_uid;
    tmp->cr_ruid = cr->cr_ruid;
    tmp->cr_gid = cr->cr_gid;
    tmp->cr_rgid = cr->cr_rgid;

#if defined(AFS_LINUX26_ENV)
    get_group_info(cr->cr_group_info);
    tmp->cr_group_info = cr->cr_group_info;
#else
    memcpy(tmp->cr_groups, cr->cr_groups, NGROUPS * sizeof(gid_t));
    tmp->cr_ngroups = cr->cr_ngroups;
#endif

    return tmp;
}

cred_t *
crref(void)
{
    cred_t *cr = crget();

    cr->cr_uid = current->fsuid;
    cr->cr_ruid = current->uid;
    cr->cr_gid = current->fsgid;
    cr->cr_rgid = current->gid;

#if defined(AFS_LINUX26_ENV)
    task_lock(current);
    get_group_info(current->group_info);
    cr->cr_group_info = current->group_info;
    task_unlock(current);
#else
    memcpy(cr->cr_groups, current->groups, NGROUPS * sizeof(gid_t));
    cr->cr_ngroups = current->ngroups;
#endif
    return cr;
}


/* Set the cred info into the current task */
void
crset(cred_t * cr)
{
    current->fsuid = cr->cr_uid;
    current->uid = cr->cr_ruid;
    current->fsgid = cr->cr_gid;
    current->gid = cr->cr_rgid;
#if defined(AFS_LINUX26_ENV)
{
    struct group_info *old_info;

    /* using set_current_groups() will sort the groups */
    get_group_info(cr->cr_group_info);

    task_lock(current);
    old_info = current->group_info;
    current->group_info = cr->cr_group_info;
    task_unlock(current);

    put_group_info(old_info);
}
#else
    memcpy(current->groups, cr->cr_groups, NGROUPS * sizeof(gid_t));
    current->ngroups = cr->cr_ngroups;
#endif
}
