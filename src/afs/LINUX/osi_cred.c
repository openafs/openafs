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

    put_group_info(cr_group_info(cr));

    kfree(cr);
}


/* Return a duplicate of the cred. */
cred_t *
crdup(cred_t * cr)
{
    cred_t *tmp = crget();

    set_cr_uid(tmp, cr_uid(cr));
    set_cr_ruid(tmp, cr_ruid(cr));
    set_cr_gid(tmp, cr_gid(cr));
    set_cr_rgid(tmp, cr_rgid(cr));

    get_group_info(cr_group_info(cr));
    set_cr_group_info(tmp, cr_group_info(cr));

    return tmp;
}

cred_t *
crref(void)
{
    cred_t *cr = crget();

    set_cr_uid(cr, current_fsuid());
    set_cr_ruid(cr, current_uid());
    set_cr_gid(cr, current_fsgid());
    set_cr_rgid(cr, current_gid());

    task_lock(current);
    get_group_info(current_group_info());
    set_cr_group_info(cr, current_group_info());
    task_unlock(current);

    return cr;
}

/* Set the cred info into the current task */
void
crset(cred_t * cr)
{
    struct group_info *old_info;
#if defined(STRUCT_TASK_HAS_CRED)
    struct cred *new_creds;

    /* If our current task doesn't have identical real and effective
     * credentials, commit_cred won't let us change them, so we just
     * bail here.
     */
    if (current->cred != current->real_cred)
        return;
    new_creds = prepare_creds();
    new_creds->fsuid = cr_uid(cr);
    new_creds->uid = cr_ruid(cr);
    new_creds->fsgid = cr_gid(cr);
    new_creds->gid = cr_rgid(cr);
#else
    current->fsuid = cr_uid(cr);
    current->uid = cr_ruid(cr);
    current->fsgid = cr_gid(cr);
    current->gid = cr_rgid(cr);
#endif

    /* using set_current_groups() will sort the groups */
    get_group_info(cr_group_info(cr));

    task_lock(current);
#if defined(STRUCT_TASK_HAS_CRED)
    old_info = current->cred->group_info;
    new_creds->group_info = cr_group_info(cr);
    commit_creds(new_creds);
#else
    old_info = current->group_info;
    current->group_info = cr_group_info(cr);
#endif
    task_unlock(current);

    put_group_info(old_info);
}
