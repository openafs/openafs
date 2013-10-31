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

/* Copy one credential structure to another, being careful about references */
static inline void
afs_copy_creds(cred_t *to_cred, const cred_t *from_cred) {
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
    /* Skip afs_from_kuid/afs_make_kuid round trip */
    to_cred->fsuid = from_cred->fsuid;
    to_cred->fsgid = from_cred->fsgid;
    to_cred->uid = from_cred->uid;
    to_cred->gid = from_cred->gid;
#else
    afs_set_cr_uid(to_cred, afs_cr_uid(from_cred));
    afs_set_cr_gid(to_cred, afs_cr_gid(from_cred));
    afs_set_cr_ruid(to_cred, afs_cr_ruid(from_cred));
    afs_set_cr_rgid(to_cred, afs_cr_rgid(from_cred));
#endif
    get_group_info(afs_cr_group_info(from_cred));
    afs_set_cr_group_info(to_cred, afs_cr_group_info(from_cred));
}

cred_t *
crget(void)
{
    cred_t *tmp;
    
    tmp = kmalloc(sizeof(cred_t), GFP_NOFS);
    if (!tmp)
        osi_Panic("crget: No more memory for creds!\n");

#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
    get_cred(tmp);
#else
    atomic_set(&tmp->cr_ref, 1);
#endif
    return tmp;
}

void
crfree(cred_t * cr)
{
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
    put_cred(cr);
#else
    if (atomic_dec_and_test(&cr->cr_ref)) {
        put_group_info(afs_cr_group_info(cr));
        kfree(cr);
    }
#endif
}


/* Return a duplicate of the cred. */
cred_t *
crdup(cred_t * cr)
{
    cred_t *tmp = crget();
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
    afs_copy_creds(tmp, cr);
#else
    afs_set_cr_uid(tmp, afs_cr_uid(cr));
    afs_set_cr_ruid(tmp, afs_cr_ruid(cr));
    afs_set_cr_gid(tmp, afs_cr_gid(cr));
    afs_set_cr_rgid(tmp, afs_cr_rgid(cr));

    get_group_info(afs_cr_group_info(cr));
    afs_set_cr_group_info(tmp, afs_cr_group_info(cr));
#endif
    return tmp;
}

cred_t *
crref(void)
{
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
    return (cred_t *)get_current_cred();
#else
    cred_t *cr = crget();

    afs_set_cr_uid(cr, current_fsuid());
    afs_set_cr_ruid(cr, current_uid());
    afs_set_cr_gid(cr, current_fsgid());
    afs_set_cr_rgid(cr, current_gid());

    task_lock(current);
    get_group_info(current_group_info());
    afs_set_cr_group_info(cr, current_group_info());
    task_unlock(current);

    return cr;
#endif
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
    /* Drop the reference to group_info - we'll overwrite it in afs_copy_creds */
    put_group_info(new_creds->group_info);
    afs_copy_creds(new_creds, cr);

    commit_creds(new_creds);
#else
    struct group_info *old_info;

    current->fsuid = afs_cr_uid(cr);
    current->uid = afs_cr_ruid(cr);
    current->fsgid = afs_cr_gid(cr);
    current->gid = afs_cr_rgid(cr);

    get_group_info(afs_cr_group_info(cr));
    task_lock(current);
    old_info = current->group_info;
    current->group_info = afs_cr_group_info(cr);
    task_unlock(current);
    put_group_info(old_info);
#endif
}
