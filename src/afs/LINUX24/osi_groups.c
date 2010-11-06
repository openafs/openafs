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


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */
#include "afs/nfsclient.h"
#ifdef AFS_LINUX22_ENV
#include "h/smp_lock.h"
#endif

#define NUMPAGGROUPS 2

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

int
setpag(cred_t **cr, afs_uint32 pagvalue, afs_uint32 *newpag,
       int change_parent)
{
    int code;

    AFS_STATCNT(setpag);

    code = __setpag(cr, pagvalue, newpag, change_parent);

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
extern asmlinkage int (*sys_setgroups32p) (int gidsetsize,
					   __kernel_gid32_t * grouplist);
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
extern asmlinkage long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);
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
#ifdef AFS_AMD64_LINUX20_ENV
extern asmlinkage long (*sys32_setgroupsp) (int gidsetsize, u16 * grouplist);
#endif /* AFS_AMD64_LINUX20_ENV */
#ifdef AFS_SPARC64_LINUX26_ENV
extern asmlinkage int (*sys32_setgroupsp) (int gidsetsize,
					   __kernel_gid32_t * grouplist);
#endif /* AFS_SPARC64_LINUX26_ENV */
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
#ifdef AFS_AMD64_LINUX20_ENV
extern asmlinkage long (*sys32_setgroups32p) (int gidsetsize, gid_t * grouplist);
#endif /* AFS_AMD64_LINUX20_ENV */
#ifdef AFS_SPARC64_LINUX26_ENV
extern asmlinkage int (*sys32_setgroups32p) (int gidsetsize,
					     __kernel_gid_t32 * grouplist);
#endif /* AFS_SPARC64_LINUX26_ENV */
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
