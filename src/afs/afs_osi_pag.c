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
 * genpag
 * getpag
 * afs_setpag
 * AddPag
 * afs_InitReq
 * afs_get_pag_from_groups
 * afs_get_groups_from_pag
 * PagInCred
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"


/* Imported variables */
extern enum afs_shutdown_state afs_shuttingdown;

/* Exported variables */
afs_uint32 pag_epoch;
#if defined(UKERNEL)
afs_uint32 pagCounter = 1;
#else
afs_uint32 pagCounter = 0;
#endif /* UKERNEL */

#ifdef AFS_LINUX26_ONEGROUP_ENV
#define NUMPAGGROUPS 1
#else
#define NUMPAGGROUPS 2
#endif
/* Local variables */

/*
 * Pags are implemented as follows: the set of groups whose long
 * representation is '41XXXXXX' hex are used to represent the pags.
 * Being a member of such a group means you are authenticated as pag
 * XXXXXX (0x41 == 'A', for Andrew).  You are never authenticated as
 * multiple pags at once.
 *
 * The function afs_InitReq takes a credential field and formats the
 * corresponding venus request structure.  The uid field in the
 * vrequest structure is set to the *pag* you are authenticated as, or
 * the uid, if you aren't authenticated with a pag.
 *
 * The basic motivation behind pags is this: just because your unix
 * uid is N doesn't mean that you should have the same privileges as
 * anyone logged in on the machine as user N, since this would enable
 * the superuser on the machine to sneak in and make use of anyone's
 * authentication info, even that which is only accidentally left
 * behind when someone leaves a public workstation.
 *
 * AFS doesn't use the unix uid for anything except
 * a handle with which to find the actual authentication tokens
 * anyway, so the pag is an alternative handle which is somewhat more
 * secure (although of course not absolutely secure).
*/
#if !defined(UKERNEL)
afs_uint32
genpag(void)
{
    AFS_STATCNT(genpag);
#ifdef AFS_LINUX20_ENV
    /* Ensure unique PAG's (mod 200 days) when reloading the client. */
    return (('A' << 24) + ((pag_epoch + pagCounter++) & 0xffffff));
#else /* AFS_LINUX20_ENV */
    return (('A' << 24) + (pagCounter++ & 0xffffff));
#endif /* AFS_LINUX20_ENV */
}

afs_uint32
getpag(void)
{
    AFS_STATCNT(getpag);
#ifdef AFS_LINUX20_ENV
    /* Ensure unique PAG's (mod 200 days) when reloading the client. */
    return (('A' << 24) + ((pag_epoch + pagCounter) & 0xffffff));
#else
    return (('A' << 24) + (pagCounter & 0xffffff));
#endif
}

#else

/* Web enhancement: we don't need to restrict pags to 41XXXXXX since
 * we are not sharing the space with anyone.  So we use the full 32 bits. */

afs_uint32
genpag(void)
{
    AFS_STATCNT(genpag);
#ifdef AFS_LINUX20_ENV
    return (pag_epoch + pagCounter++);
#else
    return (pagCounter++);
#endif /* AFS_LINUX20_ENV */
}

afs_uint32
getpag(void)
{
    AFS_STATCNT(getpag);
#ifdef AFS_LINUX20_ENV
    /* Ensure unique PAG's (mod 200 days) when reloading the client. */
    return (pag_epoch + pagCounter);
#else
    return (pagCounter);
#endif
}
#endif /* UKERNEL */

/* used to require 10 seconds between each setpag to guarantee that
 * PAGs never wrap - which would be a security hole.  If we presume
 * that in ordinary operation, the average rate of PAG allocation
 * will not exceed one per second, the 24 bits provided will be
 * sufficient for ~200 days.  Unfortunately, if we merely assume that,
 * there will be an opportunity for attack.  So we must enforce it.
 * If we need to increase the average rate of PAG allocation, we
 * should increase the number of bits in a PAG, and/or reduce our
 * window in which we guarantee that the PAG counter won't wrap.
 * By permitting an average of one new PAG per second, new PAGs can
 * be allocated VERY frequently over a short period relative to total uptime.
 * Of course, there's only an issue here if one user stays logged (and re-
 * activates tokens repeatedly) for that entire period.
 */

static int afs_pag_sleepcnt = 0;
static int afs_pag_timewarn = 0;

static int
afs_pag_sleep(afs_ucred_t **acred)
{
    int rv = 0;

    if (!afs_suser(acred)) {
	if(osi_Time() - pag_epoch < pagCounter) {
	    rv = 1;
	}
	if (rv && (osi_Time() < pag_epoch)) {
	    if (!afs_pag_timewarn) {
		afs_pag_timewarn = 1;
		afs_warn("clock went backwards, not PAG throttling");
	    }
	    rv = 0;
	}
    }

    return rv;
}

static int
afs_pag_wait(afs_ucred_t **acred)
{
    if (afs_pag_sleep(acred)) {
	if (!afs_pag_sleepcnt) {
	    afs_warn("%s() PAG throttling triggered, pid %d... sleeping.  sleepcnt %d\n",
		     "afs_pag_wait", osi_getpid(), afs_pag_sleepcnt);
	}

	afs_pag_sleepcnt++;

	do {
	    /* XXX spins on EINTR */
	    afs_osi_Wait(1000, (struct afs_osi_WaitHandle *)0, 0);
	} while (afs_pag_sleep(acred));

	afs_pag_sleepcnt--;
    }

    return 0;
}

int
#if	defined(AFS_SUN5_ENV)
afs_setpag(afs_ucred_t **credpp)
#elif	defined(AFS_FBSD_ENV)
afs_setpag(struct thread *td, void *args)
#elif	defined(AFS_NBSD_ENV)
afs_setpag(afs_proc_t *p, const void *args, register_t *retval)
#elif  defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_setpag(afs_proc_t *p, void *args, int *retval)
#else
afs_setpag(void)
#endif
{

#if     defined(AFS_SUN5_ENV)
    afs_ucred_t **acred = *credpp;
#elif  defined(AFS_OBSD_ENV)
    afs_ucred_t **acred = &p->p_ucred;
#else
    afs_ucred_t **acred = NULL;
#endif

    int code = 0;

#if defined(AFS_SGI53_ENV) && defined(MP)
    /* This is our first chance to get the global lock. */
    AFS_GLOCK();
#endif /* defined(AFS_SGI53_ENV) && defined(MP) */

    AFS_STATCNT(afs_setpag);

    afs_pag_wait(acred);


#if	defined(AFS_SUN5_ENV)
    code = AddPag(genpag(), credpp);
#elif	defined(AFS_FBSD_ENV)
    code = AddPag(td, genpag(), &td->td_ucred);
#elif   defined(AFS_NBSD40_ENV)
    code = AddPag(p, genpag(), &p->l_proc->p_cred);
#elif	defined(AFS_XBSD_ENV)
    code = AddPag(p, genpag(), &p->p_rcred);
#elif	defined(AFS_AIX41_ENV)
    {
	struct ucred *credp;
	struct ucred *credp0;

	credp = crref();
	credp0 = credp;
	code = AddPag(genpag(), &credp);
	/* If AddPag() didn't make a new cred, then free our cred ref */
	if (credp == credp0) {
	    crfree(credp);
	}
    }
#elif	defined(AFS_HPUX110_ENV)
    {
	struct ucred *credp = p_cred(u.u_procp);
	code = AddPag(genpag(), &credp);
    }
#elif	defined(AFS_SGI_ENV)
    {
	cred_t *credp;
	credp = OSI_GET_CURRENT_CRED();
	code = AddPag(genpag(), &credp);
    }
#elif	defined(AFS_LINUX20_ENV)
    {
	afs_ucred_t *credp = crref();
	code = AddPag(genpag(), &credp);
	crfree(credp);
    }
#elif defined(AFS_DARWIN80_ENV)
    {
	afs_ucred_t *credp = kauth_cred_proc_ref(p);
	code = AddPag(p, genpag(), &credp);
	crfree(credp);
    }
#elif defined(AFS_DARWIN_ENV)
    {
	afs_ucred_t *credp = crdup(p->p_cred->pc_ucred);
	code = AddPag(p, genpag(), &credp);
	crfree(credp);
    }
#elif defined(UKERNEL)
    code = AddPag(genpag(), &(get_user_struct()->u_cred));
#else
    code = AddPag(genpag(), &u.u_cred);
#endif

    afs_Trace1(afs_iclSetp, CM_TRACE_SETPAG, ICL_TYPE_INT32, code);

#if defined(KERNEL_HAVE_UERROR)
    if (!getuerror())
	setuerror(code);
#endif

#if defined(AFS_SGI53_ENV) && defined(MP)
    AFS_GUNLOCK();
#endif /* defined(AFS_SGI53_ENV) && defined(MP) */

    return (code);
}

#if defined(UKERNEL)
/*
 * afs_setpag_val
 * This function is like setpag but sets the current thread's pag id to a
 * caller-provided value instead of calling genpag().  This implements a
 * form of token caching since the caller can recall a particular pag value
 * for the thread to restore tokens, rather than reauthenticating.
 */
int
#if	defined(AFS_SUN5_ENV)
afs_setpag_val(afs_ucred_t **credpp, int pagval)
#elif	defined(AFS_FBSD_ENV)
afs_setpag_val(struct thread *td, void *args, int pagval)
#elif  defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_setpag_val(afs_proc_t *p, void *args, int *retval, int pagval)
#else
afs_setpag_val(int pagval)
#endif
{

#if     defined(AFS_SUN5_ENV)
    afs_ucred_t **acred = *credp;
#elif  defined(AFS_OBSD_ENV)
    afs_ucred_t **acred = &p->p_ucred;
#else
    afs_ucred_t **acred = NULL;
#endif

    int code = 0;

#if defined(AFS_SGI53_ENV) && defined(MP)
    /* This is our first chance to get the global lock. */
    AFS_GLOCK();
#endif /* defined(AFS_SGI53_ENV) && defined(MP) */

    AFS_STATCNT(afs_setpag);

    afs_pag_wait(acred);

#if	defined(AFS_SUN5_ENV)
    code = AddPag(pagval, credpp);
#elif	defined(AFS_FBSD_ENV)
    code = AddPag(td, pagval, &td->td_ucred);
#elif	defined(AFS_XBSD_ENV)
    code = AddPag(p, pagval, &p->p_rcred);
#elif	defined(AFS_AIX41_ENV)
    {
	struct ucred *credp;
	struct ucred *credp0;

	credp = crref();
	credp0 = credp;
	code = AddPag(pagval, &credp);
	/* If AddPag() didn't make a new cred, then free our cred ref */
	if (credp == credp0) {
	    crfree(credp);
	}
    }
#elif	defined(AFS_HPUX110_ENV)
    {
	struct ucred *credp = p_cred(u.u_procp);
	code = AddPag(pagval, &credp);
    }
#elif	defined(AFS_SGI_ENV)
    {
	cred_t *credp;
	credp = OSI_GET_CURRENT_CRED();
	code = AddPag(pagval, &credp);
    }
#elif	defined(AFS_LINUX20_ENV)
    {
	afs_ucred_t *credp = crref();
	code = AddPag(pagval, &credp);
	crfree(credp);
    }
#elif defined(AFS_DARWIN_ENV)
    {
	struct ucred *credp = crdup(p->p_cred->pc_ucred);
	code = AddPag(p, pagval, &credp);
	crfree(credp);
    }
#elif defined(UKERNEL)
    code = AddPag(pagval, &(get_user_struct()->u_cred));
#else
    code = AddPag(pagval, &u.u_cred);
#endif

    afs_Trace1(afs_iclSetp, CM_TRACE_SETPAG, ICL_TYPE_INT32, code);
#if defined(KERNEL_HAVE_UERROR)
    if (!getuerror())
	setuerror(code);
#endif
#if defined(AFS_SGI53_ENV) && defined(MP)
    AFS_GUNLOCK();
#endif /* defined(AFS_SGI53_ENV) && defined(MP) */
    return (code);
}

#ifndef AFS_LINUX26_ONEGROUP_ENV
int
afs_getpag_val(void)
{
    int pagvalue;
#ifdef UKERNEL
    afs_ucred_t *credp = get_user_struct()->u_cred;
#else
    afs_ucred_t *credp = u.u_cred;
#endif
    gid_t gidset0, gidset1;
#ifdef AFS_SUN510_ENV
    const gid_t *gids;

    gids = crgetgroups(*credp);
    gidset0 = gids[0];
    gidset1 = gids[1];
#else
    gidset0 = credp->cr_groups[0];
    gidset1 = credp->cr_groups[1];
#endif
    pagvalue = afs_get_pag_from_groups(gidset0, gidset1);
    return pagvalue;
}
#endif
#endif /* UKERNEL */


/* Note - needs to be available on AIX, others can be static - rework this */
int
#if defined(AFS_FBSD_ENV)
AddPag(struct thread *p, afs_int32 aval, afs_ucred_t **credpp)
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
AddPag(afs_proc_t *p, afs_int32 aval, afs_ucred_t **credpp)
#else
AddPag(afs_int32 aval, afs_ucred_t **credpp)
#endif
{
    afs_int32 code;
    afs_uint32 newpag;

    AFS_STATCNT(AddPag);
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if ((code = setpag(p, credpp, aval, &newpag, 0)))
#else
    if ((code = setpag(credpp, aval, &newpag, 0)))
#endif
#if defined(KERNEL_HAVE_UERROR)
	return (setuerror(code), code);
#else
	return (code);
#endif
    return 0;
}


int
afs_InitReq(struct vrequest *av, afs_ucred_t *acred)
{
#if defined(AFS_LINUX26_ENV) && !defined(AFS_NONFSTRANS)
    int code;
#endif

    AFS_STATCNT(afs_InitReq);
    memset(av, 0, sizeof(*av));
    if (afs_shuttingdown == AFS_SHUTDOWN)
	return EIO;

#ifdef AFS_LINUX26_ENV
#if !defined(AFS_NONFSTRANS)
    if (osi_linux_nfs_initreq(av, acred, &code))
	return code;
#endif
#endif

    av->uid = PagInCred(acred);
    if (av->uid == NOPAG) {
	/* Afs doesn't use the unix uid for anuthing except a handle
	 * with which to find the actual authentication tokens so I
	 * think it's ok to use the real uid to make setuid
	 * programs (without setpag) to work properly.
	 */
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	if (acred == NOCRED)
	    av->uid = -2;	/* XXX nobody... ? */
	else
	    av->uid = afs_cr_uid(acred);	/* bsd creds don't have ruid */
#elif defined(AFS_SUN510_ENV)
        av->uid = crgetruid(acred);
#else
	av->uid = afs_cr_ruid(acred);	/* default when no pag is set */
#endif
    }
    return 0;
}

/*!
 * Allocate and setup a vrequest.
 *
 * \note The caller must free the allocated vrequest with
 *       afs_DestroyReq() if this function returns successfully (zero).
 *
 * \note The GLOCK must be held on platforms which require the GLOCK
 *       for osi_AllocSmallSpace() and osi_FreeSmallSpace().
 *
 * \param[out] avpp   address of the vrequest pointer
 * \param[in]  acred  user credentials to setup the vrequest
 *                    afs_osi_credp should be used for anonymous connections
 * \return     0 on success
 */
int
afs_CreateReq(struct vrequest **avpp, afs_ucred_t *acred)
{
    int code;
    struct vrequest *treq = NULL;

    if (afs_shuttingdown == AFS_SHUTDOWN) {
	return EIO;
    }
    if (!avpp || !acred) {
	return EINVAL;
    }
    treq = osi_AllocSmallSpace(sizeof(struct vrequest));
    if (!treq) {
	return ENOMEM;
    }
    code = afs_InitReq(treq, acred);
    if (code != 0) {
	osi_FreeSmallSpace(treq);
	return code;
    }
    *avpp = treq;
    return 0;
}

/*!
 * Deallocate a vrequest.
 *
 * \note The GLOCK must be held on platforms which require the GLOCK
 *       for osi_FreeSmallSpace().
 *
 * \param[in]  av  pointer to the vrequest to free; may be NULL
 */
void
afs_DestroyReq(struct vrequest *av)
{
    if (av) {
	osi_FreeSmallSpace(av);
    }
}

#ifndef AFS_LINUX26_ONEGROUP_ENV
afs_uint32
afs_get_pag_from_groups(gid_t g0a, gid_t g1a)
{
    afs_uint32 g0 = g0a;
    afs_uint32 g1 = g1a;
    afs_uint32 h, l, ret;

    AFS_STATCNT(afs_get_pag_from_groups);

    g0 -= 0x3f00;
    g1 -= 0x3f00;
    if (g0 < 0xc000 && g1 < 0xc000) {
	l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
	h = (g0 >> 14);
	h = (g1 >> 14) + h + h + h;
	ret = ((h << 28) | l);
# if defined(UKERNEL)
	return ret;
# else
	/* Additional testing */
	if (((ret >> 24) & 0xff) == 'A')
	    return ret;
# endif /* UKERNEL */
    }
    return NOPAG;
}

void
afs_get_groups_from_pag(afs_uint32 pag, gid_t * g0p, gid_t * g1p)
{
    unsigned short g0, g1;

    AFS_STATCNT(afs_get_groups_from_pag);
    *g0p = pag;
    *g1p = 0;
# if !defined(UKERNEL)
    pag &= 0x7fffffff;
# endif /* UKERNEL */
    g0 = 0x3fff & (pag >> 14);
    g1 = 0x3fff & pag;
    g0 |= ((pag >> 28) / 3) << 14;
    g1 |= ((pag >> 28) % 3) << 14;
    *g0p = g0 + 0x3f00;
    *g1p = g1 + 0x3f00;
}
#else
void afs_get_groups_from_pag(afs_uint32 pag, gid_t *g0p, gid_t *g1p)
{
    AFS_STATCNT(afs_get_groups_from_pag);
    *g0p = pag;
    *g1p = 0;
}
#endif

#if !defined(AFS_LINUX26_ENV) && !defined(AFS_DARWIN110_ENV)
static afs_int32
osi_get_group_pag(afs_ucred_t *cred)
{
    afs_int32 pag = NOPAG;
    gid_t g0, g1;
#if defined(AFS_SUN510_ENV)
    const gid_t *gids;
    int ngroups;
#endif

#if defined(AFS_SUN510_ENV)
    gids = crgetgroups(cred);
    ngroups = crgetngroups(cred);
#endif
#if defined(AFS_NBSD40_ENV)
    if (cred == NOCRED || cred == FSCRED)
      return NOPAG;
    if (osi_crngroups(cred) < 3)
      return NOPAG;
    g0 = osi_crgroupbyid(cred, 1);
    g1 = osi_crgroupbyid(cred, 2);
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if (cred == NOCRED || cred == FSCRED)
	return NOPAG;
    if (cred->cr_ngroups < 3)
	return NOPAG;
    /* gid is stored in cr_groups[0] */
    g0 = cred->cr_groups[1];
    g1 = cred->cr_groups[2];
#else
# if defined(AFS_AIX_ENV)
    if (cred->cr_ngrps < 2)
	return NOPAG;
# elif defined(AFS_LINUX26_ENV)
    if (afs_cr_group_info(cred)->ngroups < NUMPAGGROUPS)
	return NOPAG;
# elif defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_XBSD_ENV)
#  if defined(AFS_SUN510_ENV)
    if (ngroups < 2) {
#  else
    if (cred->cr_ngroups < 2) {
#  endif
	return NOPAG;
    }
# endif
# if defined(AFS_AIX51_ENV)
    g0 = cred->cr_groupset.gs_union.un_groups[0];
    g1 = cred->cr_groupset.gs_union.un_groups[1];
#elif defined(AFS_SUN510_ENV)
    g0 = gids[0];
    g1 = gids[1];
#else
    g0 = cred->cr_groups[0];
    g1 = cred->cr_groups[1];
#endif
#endif
    pag = (afs_int32) afs_get_pag_from_groups(g0, g1);
    return pag;
}
#endif


afs_int32
PagInCred(afs_ucred_t *cred)
{
    afs_int32 pag = NOPAG;

    AFS_STATCNT(PagInCred);
    if (cred == NULL || cred == afs_osi_credp) {
	return NOPAG;
    }
#ifndef AFS_DARWIN110_ENV
#if defined(AFS_LINUX26_ENV) && defined(LINUX_KEYRING_SUPPORT)
    /*
     * If linux keyrings are in use and we carry the session keyring in our credentials
     * structure, they should be the only criteria for determining
     * if we're in a PAG.  Groups are updated for legacy reasons only for now,
     * and should not be used to infer PAG membership
     * With keyrings but no kernel credentials, look at groups first and fall back
     * to looking at the keyrings.
     */
# if !defined(STRUCT_TASK_STRUCT_HAS_CRED)
    pag = osi_get_group_pag(cred);
# endif
    if (pag == NOPAG)
	pag = osi_get_keyring_pag(cred);
#elif defined(AFS_AIX51_ENV)
    if (kcred_getpag(cred, PAG_AFS, &pag) < 0 || pag == 0)
	pag = NOPAG;
#else
    pag = osi_get_group_pag(cred);
#endif
#endif
    return pag;
}
