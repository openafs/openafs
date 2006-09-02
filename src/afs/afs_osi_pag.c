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

RCSID
    ("$Header: /cvs/openafs/src/afs/afs_osi_pag.c,v 1.21.2.7 2006/08/17 13:56:29 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"


/* Imported variables */
extern int afs_shuttingdown;

/* Exported variables */
afs_uint32 pag_epoch;
#if defined(UKERNEL) && defined(AFS_WEB_ENHANCEMENTS)
afs_uint32 pagCounter = 1;
#else
afs_uint32 pagCounter = 0;
#endif /* UKERNEL && AFS_WEB_ENHANCEMENTS */

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
#if !defined(UKERNEL) || !defined(AFS_WEB_ENHANCEMENTS)
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
#endif /* UKERNEL && AFS_WEB_ENHANCEMENTS */

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

static int
afs_pag_sleep(struct AFS_UCRED **acred)
{
    int rv = 0;

    if (!afs_suser(acred)) {
	if(osi_Time() - pag_epoch < pagCounter) {
	    rv = 1;
	}
    }

    return rv;
}

static int
afs_pag_wait(struct AFS_UCRED **acred)
{
    if (afs_pag_sleep(acred)) {
	if (!afs_pag_sleepcnt) {
	    printf("%s() PAG throttling triggered, pid %d... sleeping.  sleepcnt %d\n",
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
afs_setpag(struct AFS_UCRED **credpp)
#elif  defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_setpag(struct proc *p, void *args, int *retval)
#else
afs_setpag(void)
#endif
{

#if     defined(AFS_SUN5_ENV)
    struct AFS_UCRED **acred = *credpp;
#elif  defined(AFS_OBSD_ENV)
    struct AFS_UCRED **acred = &p->p_ucred;
#else
    struct AFS_UCRED **acred = NULL;
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
#elif	defined(AFS_OSF_ENV) || defined(AFS_XBSD_ENV)
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
	struct AFS_UCRED *credp = crref();
	code = AddPag(genpag(), &credp);
	crfree(credp);
    }
#elif defined(AFS_DARWIN80_ENV)
    {
	struct ucred *credp = kauth_cred_proc_ref(p);
	code = AddPag(p, genpag(), &credp);
	kauth_cred_rele(credp);
    }
#elif defined(AFS_DARWIN_ENV)
    {
	struct ucred *credp = crdup(p->p_cred->pc_ucred);
	code = AddPag(p, genpag(), &credp);
	crfree(credp);
    }
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

#if defined(UKERNEL) && defined(AFS_WEB_ENHANCEMENTS)
/*
 * afs_setpag_val
 * This function is like setpag but sets the current thread's pag id to a
 * caller-provided value instead of calling genpag().  This implements a
 * form of token caching since the caller can recall a particular pag value
 * for the thread to restore tokens, rather than reauthenticating.
 */
int
#if	defined(AFS_SUN5_ENV)
afs_setpag_val(struct AFS_UCRED **credpp, int pagval)
#elif  defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_setpag_val(struct proc *p, void *args, int *retval, int pagval)
#else
afs_setpag_val(int pagval)
#endif
{

#if     defined(AFS_SUN5_ENV)
    struct AFS_UCRED **acred = *credp;
#elif  defined(AFS_OBSD_ENV)
    struct AFS_UCRED **acred = &p->p_ucred;
#else
    struct AFS_UCRED **acred = NULL;
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
#elif	defined(AFS_OSF_ENV) || defined(AFS_XBSD_ENV)
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
	struct AFS_UCRED *credp = crref();
	code = AddPag(pagval, &credp);
	crfree(credp);
    }
#elif defined(AFS_DARWIN_ENV)
    {
	struct ucred *credp = crdup(p->p_cred->pc_ucred);
	code = AddPag(p, pagval, &credp);
	crfree(credp);
    }
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

int
afs_getpag_val()
{
    int pagvalue;
    struct AFS_UCRED *credp = u.u_cred;
    int gidset0, gidset1;

    gidset0 = credp->cr_groups[0];
    gidset1 = credp->cr_groups[1];
    pagvalue = afs_get_pag_from_groups(gidset0, gidset1);
    return pagvalue;
}
#endif /* UKERNEL && AFS_WEB_ENHANCEMENTS */


/* Note - needs to be available on AIX, others can be static - rework this */
#if defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
int
AddPag(struct proc *p, afs_int32 aval, struct AFS_UCRED **credpp)
#else
int
AddPag(afs_int32 aval, struct AFS_UCRED **credpp)
#endif
{
    afs_int32 newpag, code;

    AFS_STATCNT(AddPag);
#if defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
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
afs_InitReq(register struct vrequest *av, struct AFS_UCRED *acred)
{
    AFS_STATCNT(afs_InitReq);
    if (afs_shuttingdown)
	return EIO;
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
	    av->uid = acred->cr_uid;	/* bsd creds don't have ruid */
#else
	av->uid = acred->cr_ruid;	/* default when no pag is set */
#endif
    }
    av->initd = 0;
    return 0;
}



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
#if defined(UKERNEL) && defined(AFS_WEB_ENHANCEMENTS)
	return ret;
#else
	/* Additional testing */
	if (((ret >> 24) & 0xff) == 'A')
	    return ret;
#endif /* UKERNEL && AFS_WEB_ENHANCEMENTS */
    }
    return NOPAG;
}


void
afs_get_groups_from_pag(afs_uint32 pag, gid_t * g0p, gid_t * g1p)
{
    unsigned short g0, g1;


    AFS_STATCNT(afs_get_groups_from_pag);
#if !defined(UKERNEL) || !defined(AFS_WEB_ENHANCEMENTS)
    pag &= 0x7fffffff;
#endif /* UKERNEL && AFS_WEB_ENHANCEMENTS */
    g0 = 0x3fff & (pag >> 14);
    g1 = 0x3fff & pag;
    g0 |= ((pag >> 28) / 3) << 14;
    g1 |= ((pag >> 28) % 3) << 14;
    *g0p = g0 + 0x3f00;
    *g1p = g1 + 0x3f00;
}


afs_int32
PagInCred(const struct AFS_UCRED *cred)
{
    afs_int32 pag;
    gid_t g0, g1;

    AFS_STATCNT(PagInCred);
    if (cred == NULL || cred == afs_osi_credp) {
	return NOPAG;
    }
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if (cred == NOCRED || cred == FSCRED) {
	return NOPAG;
    }
    if (cred->cr_ngroups < 3)
	return NOPAG;
    /* gid is stored in cr_groups[0] */
    g0 = cred->cr_groups[1];
    g1 = cred->cr_groups[2];
#else
#if defined(AFS_AIX51_ENV)
    if (kcred_getpag(cred, PAG_AFS, &pag) < 0 || pag == 0)
	pag = NOPAG;
    return pag;
#elif defined(AFS_AIX_ENV)
    if (cred->cr_ngrps < 2) {
	return NOPAG;
    }
#elif defined(AFS_LINUX26_ENV)
    if (cred->cr_group_info->ngroups < 2) {
	pag = NOPAG;
	goto out;
    }
#elif defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_DUX40_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_XBSD_ENV)
    if (cred->cr_ngroups < 2) {
	pag = NOPAG;
	goto out;
    }
#endif
#if defined(AFS_AIX51_ENV)
    g0 = cred->cr_groupset.gs_union.un_groups[0];
    g1 = cred->cr_groupset.gs_union.un_groups[1];
#elif defined(AFS_LINUX26_ENV)
    g0 = GROUP_AT(cred->cr_group_info, 0);
    g1 = GROUP_AT(cred->cr_group_info, 1);
#else
    g0 = cred->cr_groups[0];
    g1 = cred->cr_groups[1];
#endif
#endif
    pag = (afs_int32) afs_get_pag_from_groups(g0, g1);
out:
#if defined(AFS_LINUX26_ENV) && defined(LINUX_KEYRING_SUPPORT)
    if (pag == NOPAG) {
	struct key *key;
	afs_uint32 pag, newpag;

	key = request_key(&key_type_afs_pag, "_pag", NULL);
	if (!IS_ERR(key)) {
	    if (key_validate(key) == 0 && key->uid == 0) {	/* also verify in the session keyring? */

		pag = (afs_uint32) key->payload.value;
		if (((pag >> 24) & 0xff) == 'A')
		    __setpag(&cred, pag, &newpag, 0);
	    }
	    key_put(key);
	} 
    }
#endif
    return pag;
}
