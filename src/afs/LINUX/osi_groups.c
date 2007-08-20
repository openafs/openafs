/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * setgroups (syscall)
 * setpag
 *
 */
#include <afsconfig.h>
#include "afs/param.h"
#ifdef LINUX_KEYRING_SUPPORT
#include <linux/seq_file.h>
#endif

RCSID
    ("$Header: /cvs/openafs/src/afs/LINUX/osi_groups.c,v 1.25.2.8 2007/01/15 15:52:46 shadow Exp $");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */
#ifdef AFS_LINUX22_ENV
#include "h/smp_lock.h"
#endif

#ifdef AFS_LINUX26_ONEGROUP_ENV
#define NUMPAGGROUPS 1
#else
#define NUMPAGGROUPS 2
#endif

#if defined(AFS_LINUX26_ENV)
static int
afs_setgroups(cred_t **cr, struct group_info *group_info, int change_parent)
{
    struct group_info *old_info;

    AFS_STATCNT(afs_setgroups);

    old_info = (*cr)->cr_group_info;
    get_group_info(group_info);
    (*cr)->cr_group_info = group_info;
    put_group_info(old_info);

    crset(*cr);

    if (change_parent) {
	old_info = current->parent->group_info;
	get_group_info(group_info);
	current->parent->group_info = group_info;
	put_group_info(old_info);
    }

    return (0);
}
#else
static int
afs_setgroups(cred_t **cr, int ngroups, gid_t * gidset, int change_parent)
{
    int ngrps;
    int i;
    gid_t *gp;

    AFS_STATCNT(afs_setgroups);

    if (ngroups > NGROUPS)
	return EINVAL;

    gp = (*cr)->cr_groups;
    if (ngroups < NGROUPS)
	gp[ngroups] = (gid_t) NOGROUP;

    for (i = ngroups; i > 0; i--) {
	*gp++ = *gidset++;
    }

    (*cr)->cr_ngroups = ngroups;
    crset(*cr);
    return (0);
}
#endif

#if defined(AFS_LINUX26_ENV)
static struct group_info *
afs_getgroups(cred_t * cr)
{
    AFS_STATCNT(afs_getgroups);

    get_group_info(cr->cr_group_info);
    return cr->cr_group_info;
}
#else
/* Returns number of groups. And we trust groups to be large enough to
 * hold all the groups.
 */
static int
afs_getgroups(cred_t *cr, gid_t *groups)
{
    int i;
    int n;
    gid_t *gp;

    AFS_STATCNT(afs_getgroups);

    gp = cr->cr_groups;
    n = cr->cr_ngroups;

    for (i = 0; (i < n) && (*gp != (gid_t) NOGROUP); i++)
	*groups++ = *gp++;
    return i;
}
#endif

#if !defined(AFS_LINUX26_ENV)
/* Only propogate the PAG to the parent process. Unix's propogate to 
 * all processes sharing the cred.
 */
int
set_pag_in_parent(int pag, int g0, int g1)
{
    int i;
#ifdef STRUCT_TASK_STRUCT_HAS_PARENT
    gid_t *gp = current->parent->groups;
    int ngroups = current->parent->ngroups;
#else
    gid_t *gp = current->p_pptr->groups;
    int ngroups = current->p_pptr->ngroups;
#endif

    if ((ngroups < 2) || (afs_get_pag_from_groups(gp[0], gp[1]) == NOPAG)) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    return EINVAL;
	}
	for (i = ngroups - 1; i >= 0; i--) {
	    gp[i + 2] = gp[i];
	}
	ngroups += 2;
    }
    gp[0] = g0;
    gp[1] = g1;
    if (ngroups < NGROUPS)
	gp[ngroups] = NOGROUP;

#ifdef STRUCT_TASK_STRUCT_HAS_PARENT
    current->parent->ngroups = ngroups;
#else
    current->p_pptr->ngroups = ngroups;
#endif
    return 0;
}
#endif

#if defined(AFS_LINUX26_ENV)
int
__setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
         int change_parent)
{
    struct group_info *group_info;
    gid_t g0, g1;
    struct group_info *tmp;
    int i;
#ifdef AFS_LINUX26_ONEGROUP_ENV
    int j;
#endif
    int need_space = 0;

    group_info = afs_getgroups(*cr);
    if (group_info->ngroups < NUMPAGGROUPS
	||  afs_get_pag_from_groups(
#ifdef AFS_LINUX26_ONEGROUP_ENV
	    group_info
#else
	    GROUP_AT(group_info, 0) ,GROUP_AT(group_info, 1)
#endif
				    ) == NOPAG) 
	/* We will have to make sure group_info is big enough for pag */
	need_space = NUMPAGGROUPS;

    tmp = groups_alloc(group_info->ngroups + need_space);
    
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
#ifdef AFS_LINUX26_ONEGROUP_ENV
    for (i = 0, j = 0; i < group_info->ngroups; ++i) {
	int ths = GROUP_AT(group_info, i);
	int last = i > 0 ? GROUP_AT(group_info, i-1) : 0;
	if ((ths >> 24) == 'A')
	    continue;
	if (last <= *newpag && ths > *newpag) {
	   GROUP_AT(tmp, j) = *newpag;
	   j++;
	}
	GROUP_AT(tmp, j) = ths;
	j++;
    }
    if (j != i + need_space)
        GROUP_AT(tmp, j) = *newpag;
#else
    for (i = 0; i < group_info->ngroups; ++i)
      GROUP_AT(tmp, i + need_space) = GROUP_AT(group_info, i);
#endif
    put_group_info(group_info);
    group_info = tmp;

#ifndef AFS_LINUX26_ONEGROUP_ENV
    afs_get_groups_from_pag(*newpag, &g0, &g1);
    GROUP_AT(group_info, 0) = g0;
    GROUP_AT(group_info, 1) = g1;
#endif

    afs_setgroups(cr, group_info, change_parent);

    put_group_info(group_info);

    return 0;
}

#ifdef LINUX_KEYRING_SUPPORT
static struct key_type *__key_type_keyring;

static int
install_session_keyring(struct task_struct *task, struct key *keyring)
{
    struct key *old;
    char desc[20];
    unsigned long not_in_quota;
    int code = -EINVAL;

    if (!__key_type_keyring)
	return code;

    if (!keyring) {

	/* create an empty session keyring */
	not_in_quota = KEY_ALLOC_IN_QUOTA;
	sprintf(desc, "_ses.%u", task->tgid);

#ifdef KEY_ALLOC_NEEDS_STRUCT_TASK
	keyring = key_alloc(__key_type_keyring, desc,
			    task->uid, task->gid, task,
			    (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_ALL,
			    not_in_quota);
#else
	keyring = key_alloc(__key_type_keyring, desc,
			    task->uid, task->gid,
			    (KEY_POS_ALL & ~KEY_POS_SETATTR) | KEY_USR_ALL,
			    not_in_quota);
#endif
	if (IS_ERR(keyring)) {
	    code = PTR_ERR(keyring);
	    goto out;
	}
    }

    code = key_instantiate_and_link(keyring, NULL, 0, NULL, NULL);
    if (code < 0) {
	key_put(keyring);
	goto out;
    }

    /* install the keyring */
    spin_lock_irq(&task->sighand->siglock);
    old = task->signal->session_keyring;
    smp_wmb();
    task->signal->session_keyring = keyring;
    spin_unlock_irq(&task->sighand->siglock);

    if (old)
	    key_put(old);

out:
    return code;
}
#endif /* LINUX_KEYRING_SUPPORT */

#else
int
__setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
         int change_parent)
{
    gid_t *gidset;
    afs_int32 ngroups, code = 0;
    int j;

    gidset = (gid_t *) osi_Alloc(NGROUPS * sizeof(gidset[0]));
    ngroups = afs_getgroups(*cr, gidset);

    if (afs_get_pag_from_groups(gidset[0], gidset[1]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    osi_Free((char *)gidset, NGROUPS * sizeof(int));
	    return EINVAL;
	}
	for (j = ngroups - 1; j >= 0; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[0], &gidset[1]);
    code = afs_setgroups(cr, ngroups, gidset, change_parent);

    /* If change_parent is set, then we should set the pag in the parent as
     * well.
     */
    if (change_parent && !code) {
	code = set_pag_in_parent(*newpag, gidset[0], gidset[1]);
    }

    osi_Free((char *)gidset, NGROUPS * sizeof(int));
    return code;
}
#endif


int
setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
       int change_parent)
{
    int code;

    AFS_STATCNT(setpag);

    code = __setpag(cr, pagvalue, newpag, change_parent);

#ifdef LINUX_KEYRING_SUPPORT
    if (code == 0) {

	(void) install_session_keyring(current, NULL);

	if (current->signal->session_keyring) {
	    struct key *key;
	    key_perm_t perm;

	    perm = KEY_POS_VIEW | KEY_POS_SEARCH;
	    perm |= KEY_USR_VIEW | KEY_USR_SEARCH;

#ifdef KEY_ALLOC_NEEDS_STRUCT_TASK
	    key = key_alloc(&key_type_afs_pag, "_pag", 0, 0, current, perm, 1);
#else
	    key = key_alloc(&key_type_afs_pag, "_pag", 0, 0, perm, 1);
#endif

	    if (!IS_ERR(key)) {
		key_instantiate_and_link(key, (void *) newpag, sizeof(afs_uint32),
					 current->signal->session_keyring, NULL);
		key_put(key);
	    }
	}
    }
#endif /* LINUX_KEYRING_SUPPORT */

    return code;
}


/* Intercept the standard system call. */
extern asmlinkage long (*sys_setgroupsp) (int gidsetsize, gid_t * grouplist);
asmlinkage long
afs_xsetgroups(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();

    code = (*sys_setgroupsp) (gidsetsize, grouplist);
    if (code) {
	return code;
    }

    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

#if defined(AFS_LINUX24_ENV)
/* Intercept the standard uid32 system call. */
extern asmlinkage long (*sys_setgroups32p) (int gidsetsize, gid_t * grouplist);
asmlinkage long
afs_xsetgroups32(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();

    code = (*sys_setgroups32p) (gidsetsize, grouplist);

    if (code) {
	return code;
    }

    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}
#endif

#if defined(AFS_PPC64_LINUX20_ENV)
/* Intercept the uid16 system call as used by 32bit programs. */
extern long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);
asmlinkage long afs32_xsetgroups(int gidsetsize, gid_t *grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;
    
    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();
    
    code = (*sys32_setgroupsp)(gidsetsize, grouplist);
    if (code) {
	return code;
    }
    
    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();
    
    /* Linux syscall ABI returns errno as negative */
    return (-code);
}
#endif

#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_AMD64_LINUX20_ENV)
/* Intercept the uid16 system call as used by 32bit programs. */
extern long (*sys32_setgroupsp) (int gidsetsize, u16 * grouplist);
asmlinkage long
afs32_xsetgroups(int gidsetsize, u16 * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;
    
    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();
    
    code = (*sys32_setgroupsp) (gidsetsize, grouplist);
    if (code) {
	return code;
    }
    
    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();
    
    /* Linux syscall ABI returns errno as negative */
    return (-code);
}

#ifdef AFS_LINUX24_ENV
/* Intercept the uid32 system call as used by 32bit programs. */
extern long (*sys32_setgroups32p) (int gidsetsize, gid_t * grouplist);
asmlinkage long
afs32_xsetgroups32(int gidsetsize, gid_t * grouplist)
{
    long code;
    cred_t *cr = crref();
    afs_uint32 junk;
    int old_pag;

    lock_kernel();
    old_pag = PagInCred(cr);
    crfree(cr);
    unlock_kernel();

    code = (*sys32_setgroups32p) (gidsetsize, grouplist);
    if (code) {
	return code;
    }

    lock_kernel();
    cr = crref();
    if (old_pag != NOPAG && PagInCred(cr) == NOPAG) {
	/* re-install old pag if there's room. */
	code = __setpag(&cr, old_pag, &junk, 0);
    }
    crfree(cr);
    unlock_kernel();

    /* Linux syscall ABI returns errno as negative */
    return (-code);
}
#endif
#endif


#ifdef LINUX_KEYRING_SUPPORT
static void afs_pag_describe(const struct key *key, struct seq_file *m)
{
    seq_puts(m, key->description);

    seq_printf(m, ": %u", key->datalen);
}

static int afs_pag_instantiate(struct key *key, const void *data, size_t datalen)
{
    int code;
    afs_uint32 *userpag, pag = NOPAG;
    int g0, g1;

    if (key->uid != 0 || key->gid != 0)
	return -EPERM;

    code = -EINVAL;
    get_group_info(current->group_info);

    if (datalen != sizeof(afs_uint32) || !data)
	goto error;

    if (current->group_info->ngroups < NUMPAGGROUPS)
	goto error;

    /* ensure key being set matches current pag */
#ifdef AFS_LINUX26_ONEGROUP_ENV
    pag = afs_get_pag_from_groups(current->group_info);
#else
    g0 = GROUP_AT(current->group_info, 0);
    g1 = GROUP_AT(current->group_info, 1);

    pag = afs_get_pag_from_groups(g0, g1);
#endif
    if (pag == NOPAG)
	goto error;

    userpag = (afs_uint32 *) data;
    if (*userpag != pag)
	goto error;

    key->payload.value = (unsigned long) *userpag;
    key->datalen = sizeof(afs_uint32);
    code = 0;

error:
    put_group_info(current->group_info);
    return code;
}

static int afs_pag_match(const struct key *key, const void *description)
{
	return strcmp(key->description, description) == 0;
}

static void afs_pag_destroy(struct key *key)
{
    afs_uint32 pag = key->payload.value;
    struct unixuser *pu;

    pu = afs_FindUser(pag, -1, READ_LOCK);
    if (pu) {
	pu->ct.EndTimestamp = 0;
	pu->tokenTime = 0;
	afs_PutUser(pu, READ_LOCK);
    }
}

struct key_type key_type_afs_pag =
{
    .name        = "afs_pag",
    .describe    = afs_pag_describe,
    .instantiate = afs_pag_instantiate,
    .match       = afs_pag_match,
    .destroy     = afs_pag_destroy,
};

void osi_keyring_init(void)
{
    struct task_struct *p;

    p = find_task_by_pid(1);
    if (p && p->user->session_keyring)
	__key_type_keyring = p->user->session_keyring->type;

    register_key_type(&key_type_afs_pag);
}

void osi_keyring_shutdown(void)
{
    unregister_key_type(&key_type_afs_pag);
}

#else
void osi_keyring_init(void)
{
	return;
}

void osi_keyring_shutdown(void)
{
	return;
}
#endif
