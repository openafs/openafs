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

    kfree(cr);
}


/* Return a duplicate of the cred. */
cred_t *
crdup(cred_t * cr)
{
    cred_t *tmp = crget();

    afs_set_cr_uid(tmp, afs_cr_uid(cr));
    afs_set_cr_ruid(tmp, afs_cr_ruid(cr));
    afs_set_cr_gid(tmp, afs_cr_gid(cr));
    afs_set_cr_rgid(tmp, afs_cr_rgid(cr));

    memcpy(tmp->cr_groups, cr->cr_groups, NGROUPS * sizeof(gid_t));
    tmp->cr_ngroups = cr->cr_ngroups;

    return tmp;
}

cred_t *
crref(void)
{
    cred_t *cr = crget();

    afs_set_cr_uid(cr, current_fsuid());
    afs_set_cr_ruid(cr, current_uid());
    afs_set_cr_gid(cr, current_fsgid());
    afs_set_cr_rgid(cr, current_gid());

    memcpy(cr->cr_groups, current->groups, NGROUPS * sizeof(gid_t));
    cr->cr_ngroups = current->ngroups;

    return cr;
}


/* Set the cred info into the current task */
void
crset(cred_t * cr)
{
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
    struct cred *new_creds;

    /* If our current task doesn't have identical real and effective
     * credentials, commit_cred won't let us change them, so we just
     * bail here.
     */
    if (current->cred != current->real_cred)
        return;
    new_creds = prepare_creds();
    new_creds->fsuid = afs_cr_uid(cr);
    new_creds->uid = afs_cr_ruid(cr);
    new_creds->fsgid = afs_cr_gid(cr);
    new_creds->gid = afs_cr_rgid(cr);
#else
    current->fsuid = afs_cr_uid(cr);
    current->uid = afs_cr_ruid(cr);
    current->fsgid = afs_cr_gid(cr);
    current->gid = afs_cr_rgid(cr);
#endif
    memcpy(current->groups, cr->cr_groups, NGROUPS * sizeof(gid_t));
    current->ngroups = cr->cr_ngroups;
}
