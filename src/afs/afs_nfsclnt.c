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

RCSID
    ("$Header$");

#if !defined(AFS_NONFSTRANS) || defined(AFS_AIX_IAUTH_ENV)
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/nfsclient.h"
#include "rx/rx_globals.h"
#include "afs/pagcb.h"

void afs_nfsclient_hold(), afs_PutNfsClientPag(), afs_nfsclient_GC();
static void afs_nfsclient_getcreds();
int afs_nfsclient_sysname(), afs_nfsclient_stats(), afs_nfsclient_checkhost();
afs_int32 afs_nfsclient_gethost();
#ifdef AFS_AIX_IAUTH_ENV
int afs_allnfsreqs, afs_nfscalls;
#endif

/* routines exported to the "AFS exporter" layer */
struct exporterops nfs_exportops = {
    afs_nfsclient_reqhandler,
    afs_nfsclient_hold,
    afs_PutNfsClientPag,	/* Used to be afs_nfsclient_rele */
    afs_nfsclient_sysname,
    afs_nfsclient_GC,
    afs_nfsclient_stats,
    afs_nfsclient_checkhost,
    afs_nfsclient_gethost
};


struct nfsclientpag *afs_nfspags[NNFSCLIENTS];
afs_lock_t afs_xnfspag /*, afs_xnfsreq */ ;
extern struct afs_exporter *afs_nfsexporter;

/* Creates an nfsclientpag structure for the (uid, host) pair if one doesn't exist. RefCount is incremented and it's time stamped. */
static struct nfsclientpag *
afs_GetNfsClientPag(uid, host)
     register afs_int32 uid, host;
{
    register struct nfsclientpag *np;
    register afs_int32 i, now;

#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(afs_GetNfsClientPag);
    i = NHash(host);
    now = osi_Time();
    MObtainWriteLock(&afs_xnfspag, 314);
    for (np = afs_nfspags[i]; np; np = np->next) {
	if (np->uid == uid && np->host == host) {
	    np->refCount++;
	    np->lastcall = now;
	    MReleaseWriteLock(&afs_xnfspag);
	    return np;
	}
    }
    /* next try looking for NOPAG dude, if we didn't find an exact match */
    for (np = afs_nfspags[i]; np; np = np->next) {
	if (np->uid == NOPAG && np->host == host) {
	    np->refCount++;
	    np->lastcall = now;
	    MReleaseWriteLock(&afs_xnfspag);
	    return np;
	}
    }
    np = (struct nfsclientpag *)afs_osi_Alloc(sizeof(struct nfsclientpag));
    memset((char *)np, 0, sizeof(struct nfsclientpag));
    /* Copy the necessary afs_exporter fields */
    memcpy((char *)np, (char *)afs_nfsexporter, sizeof(struct afs_exporter));
    np->next = afs_nfspags[i];
    afs_nfspags[i] = np;
    np->uid = uid;
    np->host = host;
    np->refCount = 1;
    np->lastcall = now;
    MReleaseWriteLock(&afs_xnfspag);
    return np;
}


/* Decrement refCount; must always match a previous afs_FindNfsClientPag/afs_GetNfsClientPag call .
It's also called whenever a unixuser structure belonging to the remote user associated with the nfsclientpag structure, np, is garbage collected. */
void
afs_PutNfsClientPag(np)
     register struct nfsclientpag *np;
{
#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(afs_PutNfsClientPag);
    --np->refCount;
}


/* Return the nfsclientpag structure associated with the (uid, host) or {pag, host} pair, if pag is nonzero. RefCount is incremented and it's time stamped. */
static struct nfsclientpag *
afs_FindNfsClientPag(uid, host, pag)
     register afs_int32 uid, host, pag;
{
    register struct nfsclientpag *np;
    register afs_int32 i;

#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(afs_FindNfsClientPag);
    i = NHash(host);
    MObtainWriteLock(&afs_xnfspag, 315);
    for (np = afs_nfspags[i]; np; np = np->next) {
	if (np->host == host) {
	    if ((pag && pag == np->pag) || (!pag && (uid == np->uid))) {
		np->refCount++;
		np->lastcall = osi_Time();
		MReleaseWriteLock(&afs_xnfspag);
		return np;
	    }
	}
    }
    /* still not there, try looking for a wildcard dude */
    for (np = afs_nfspags[i]; np; np = np->next) {
	if (np->host == host) {
	    if (np->uid == NOPAG) {
		np->refCount++;
		np->lastcall = osi_Time();
		MReleaseWriteLock(&afs_xnfspag);
		return np;
	    }
	}
    }
    MReleaseWriteLock(&afs_xnfspag);
    return NULL;
}


/* routine to initialize the exporter, made global so we can call it
 * from pioctl calls.
 */
struct afs_exporter *afs_nfsexported = 0;
static afs_int32 init_nfsexporter = 0;

void
afs_nfsclient_init(void)
{
#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    if (!init_nfsexporter) {
	extern struct afs_exporter *exporter_add();

	init_nfsexporter = 1;
	LOCK_INIT(&afs_xnfspag, "afs_xnfspag");
	afs_nfsexported =
	    exporter_add(0, &nfs_exportops, EXP_EXPORTED, EXP_NFS, NULL);
    }
}


/* Main handler routine for the NFS exporter. It's called in the early
 * phases of any remote call (via the NFS server or pioctl).
 */
int
afs_nfsclient_reqhandler(struct afs_exporter *exporter,
			 struct AFS_UCRED **cred,
			 afs_int32 host, afs_int32 *pagparam,
			 struct afs_exporter **outexporter)
{
    register struct nfsclientpag *np, *tnp;
    extern struct unixuser *afs_FindUser(), *afs_GetUser();
    register struct unixuser *au = 0;
    afs_int32 uid, pag, code = 0;

    AFS_ASSERT_GLOCK();
    AFS_STATCNT(afs_nfsclient_reqhandler);
    if (!afs_nfsexporter)
	afs_nfsexporter = afs_nfsexported;

    afs_nfsexporter->exp_stats.calls++;
    if (!(afs_nfsexporter->exp_states & EXP_EXPORTED)) {
	/* No afs requests accepted as long as EXPORTED flag is turned 'off'. Set/Reset via a pioctl call (fs exportafs). Note that this is on top of the /etc/exports nfs requirement (i.e. /afs must be exported to all or whomever there too!)
	 */
	afs_nfsexporter->exp_stats.rejectedcalls++;
	return EINVAL;
    }
/*    ObtainWriteLock(&afs_xnfsreq); */
    pag = PagInCred(*cred);
#if defined(AFS_SUN510_ENV)
    uid = crgetuid(*cred);
#else
    uid = (*cred)->cr_uid;
#endif
    /* Do this early, so pag management knows */
#ifdef	AFS_OSF_ENV
    (*cred)->cr_ruid = NFSXLATOR_CRED;	/* Identify it as nfs xlator call */
#else
    (*cred)->cr_rgid = NFSXLATOR_CRED;	/* Identify it as nfs xlator call */
#endif
    if ((afs_nfsexporter->exp_states & EXP_CLIPAGS) && pag != NOPAG) {
	uid = pag;
    } else if (pag != NOPAG) {
	/* Do some minimal pag verification */
	if (pag > getpag()) {
	    pag = NOPAG;	/* treat it as not paged since couldn't be good  */
	} else {
	    if ((au = afs_FindUser(pag, -1, READ_LOCK))) {
		if (!au->exporter) {
		    pag = NOPAG;
		    afs_PutUser(au, READ_LOCK);
		    au = NULL;
		}
	    } else
		pag = NOPAG;	/*  No unixuser struct so pag not trusted  */
	}
    }
    np = afs_FindNfsClientPag(uid, host, 0);
    afs_Trace4(afs_iclSetp, CM_TRACE_NFSREQH, ICL_TYPE_INT32, pag,
	       ICL_TYPE_LONG, (*cred)->cr_uid, ICL_TYPE_INT32, host,
	       ICL_TYPE_POINTER, np);
    /* If remote-pags are enabled, we are no longer interested in what PAG
     * they claimed, and from here on we should behave as if they claimed
     * none at all, which is to say we use the (local) pag named in the
     * nfsclientpag structure (if any).  This is deferred until here so
     * that we can log the PAG they claimed.
     */
    if ((afs_nfsexporter->exp_states & EXP_CLIPAGS))
       	pag = NOPAG;
    if (!np) {
	/* Even if there is a "good" pag coming in we don't accept it if no nfsclientpag struct exists for the user since that would mean that the translator rebooted and therefore we ignore all older pag values */
#ifdef	AFS_OSF_ENV
	if (code = setpag(u.u_procp, cred, -1, &pag, 0)) {	/* XXX u.u_procp is a no-op XXX */
#else
	if ((code = setpag(cred, -1, &pag, 0))) {
#endif
	    if (au)
		afs_PutUser(au, READ_LOCK);
/*	    ReleaseWriteLock(&afs_xnfsreq);		*/
#if defined(KERNEL_HAVE_UERROR)
	    setuerror(code);
#endif
	    return (code);
	}
	np = afs_GetNfsClientPag(uid, host);
	np->pag = pag;
	np->client_uid = (*cred)->cr_uid;
    } else {
	if (pag == NOPAG) {
#ifdef	AFS_OSF_ENV
	    if (code = setpag(u.u_procp, cred, np->pag, &pag, 0)) {	/* XXX u.u_procp is a no-op XXX */
#else
	    if ((code = setpag(cred, np->pag, &pag, 0))) {
#endif
		afs_PutNfsClientPag(np);
/*		ReleaseWriteLock(&afs_xnfsreq);	*/
#if defined(KERNEL_HAVE_UERROR)
		setuerror(code);
#endif
		return (code);
	    }
	} else if (au->exporter
		   && ((struct afs_exporter *)np != au->exporter)) {
	    tnp = (struct nfsclientpag *)au->exporter;
	    if (tnp->uid && (tnp->uid != (afs_int32) - 2)) {	/* allow "root" initiators */
		/* Pag doesn't belong to caller; treat it as an unpaged call too */
#ifdef	AFS_OSF_ENV
		if (code = setpag(u.u_procp, cred, np->pag, &pag, 0)) {	/* XXX u.u_procp is a no-op XXX */
#else
		if ((code = setpag(cred, np->pag, &pag, 0))) {
#endif
		    afs_PutNfsClientPag(np);
		    afs_PutUser(au, READ_LOCK);
		    /*      ReleaseWriteLock(&afs_xnfsreq);     */
#if defined(KERNEL_HAVE_UERROR)
		    setuerror(code);
#endif
		    return (code);
		}
		afs_nfsexporter->exp_stats.invalidpag++;
	    }
	}
    }
    if (au)
	afs_PutUser(au, READ_LOCK);
    au = afs_GetUser(pag, -1, WRITE_LOCK);
    if (!(au->exporter)) {	/* Created new unixuser struct */
	np->refCount++;		/* so it won't disappear */
	au->exporter = (struct afs_exporter *)np;
	if ((afs_nfsexporter->exp_states & EXP_CALLBACK))
	    afs_nfsclient_getcreds(au);
    } else while (au->states & UNFSGetCreds) {
	afs_osi_Sleep((void *)au);
    }
    *pagparam = pag;
    *outexporter = (struct afs_exporter *)np;
    afs_PutUser(au, WRITE_LOCK);
/*    ReleaseWriteLock(&afs_xnfsreq);	*/
    return 0;
}

void
afs_nfsclient_getcreds(au)
    struct unixuser *au;
{
    struct nfsclientpag *np = (struct nfsclientpag *)(au->exporter);
    struct rx_securityClass *csec;
    struct rx_connection *tconn;
    SysNameList tsysnames;
    CredInfos tcreds;
    CredInfo *tcred;
    struct unixuser *tu;
    struct cell *tcell;
    int code, i, cellnum;

    au->states |= UNFSGetCreds;
    memset(&tcreds, 0, sizeof(tcreds));
    memset(&tsysnames, 0, sizeof(tsysnames));

    /* Get a connection */
    /* This sucks a little.  We should cache the connections or something.
     * But at this point I don't yet think it's worth the effort.
     */
    csec = rxnull_NewClientSecurityObject();
    AFS_GUNLOCK();
    tconn = rx_NewConnection(np->host, htons(7001), PAGCB_SERVICEID, csec, 0);
    AFS_GLOCK();

    /* Get the sysname, if needed */
    if (!np->sysnamecount) {
	AFS_GUNLOCK();
	code = PAGCB_GetSysName(tconn, np->uid, &tsysnames);
	AFS_GLOCK();
	if (code ||
	    tsysnames.SysNameList_len <= 0 ||
	    tsysnames.SysNameList_len > MAXNUMSYSNAMES)
	    goto done;
    
	for(i = 0; i < np->sysnamecount; i++)
	    afs_osi_Free(np->sysname[i], MAXSYSNAME);

	np->sysnamecount = tsysnames.SysNameList_len;
	for(i = 0; i < np->sysnamecount; i++)
	    np->sysname[i] = tsysnames.SysNameList_val[i].sysname;
        afs_osi_Free(tsysnames.SysNameList_val,
                     tsysnames.SysNameList_len * sizeof(SysNameEnt));
    }

    /* Get credentials */
    AFS_GUNLOCK();
    code = PAGCB_GetCreds(tconn, np->uid, &tcreds);
    AFS_GLOCK();
    if (code)
	goto done;

    /* Now, set the credentials they gave us... */
    for (i = 0; i < tcreds.CredInfos_len; i++) {
	tcred = &tcreds.CredInfos_val[i];

	/* Find the cell.  If it is unknown to us, punt this entry. */
	tcell = afs_GetCellByName(tcred->cellname, READ_LOCK);
	afs_osi_Free(tcred->cellname, strlen(tcred->cellname) + 1);
	if (!tcell) {
	    memset(tcred->ct.HandShakeKey, 0, 8);
	    memset(tcred->st.st_val, 0, tcred->st.st_len);
	    afs_osi_Free(tcred->st.st_val, tcred->st.st_len);
	    continue;
	}
	cellnum = tcell->cellNum;
	afs_PutCell(tcell, READ_LOCK);

	/* Find the appropriate unixuser.  This might be the same as
	 * the one we were passed (au), but that's OK.
	 */
	tu = afs_GetUser(np->pag, cellnum, WRITE_LOCK);
	if (!(tu->exporter)) {	/* Created new unixuser struct */
	    np->refCount++;		/* so it won't disappear */
	    tu->exporter = (struct afs_exporter *)np;
	}

	/* free any old secret token, and keep the new one */
	if (tu->stp != NULL) {
	    afs_osi_Free(tu->stp, tu->stLen);
	}
	tu->stp = tcred->st.st_val;
	tu->stLen = tcred->st.st_len;

	/* copy the clear token */
	memset(&tu->ct, 0, sizeof(tu->ct));
	memcpy(tu->ct.HandShakeKey, tcred->ct.HandShakeKey, 8);
	memset(tcred->ct.HandShakeKey, 0, 8);
	tu->ct.AuthHandle     = tcred->ct.AuthHandle;
	tu->ct.ViceId         = tcred->ct.ViceId;
	tu->ct.BeginTimestamp = tcred->ct.BeginTimestamp;
	tu->ct.EndTimestamp   = tcred->ct.EndTimestamp;

	/* Set everything else, reset connections, and move on. */
	tu->vid = tcred->vid;
	tu->states |= UHasTokens;
	tu->states &= ~UTokensBad;
	afs_SetPrimary(tu, !!(tcred->states & UPrimary));
	tu->tokenTime = osi_Time();
	afs_ResetUserConns(tu);
	afs_PutUser(tu, WRITE_LOCK);
    }
    afs_osi_Free(tcreds.CredInfos_val, tcreds.CredInfos_len * sizeof(CredInfo));

done:
    AFS_GUNLOCK();
    rx_DestroyConnection(tconn);
    AFS_GLOCK();
    au->states &= ~UNFSGetCreds;
    afs_osi_Wakeup((void *)au);
}


/* It's called whenever a new unixuser structure is created for the remote user associated with the nfsclientpag structure, np */
void
afs_nfsclient_hold(np)
     register struct nfsclientpag *np;
{
#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(afs_nfsclient_hold);
    np->refCount++;
}


/* check if this exporter corresponds to the specified host */
int
afs_nfsclient_checkhost(np, host)
    register struct nfsclientpag *np;
{
    if (np->type != EXP_NFS)
	return 0;
    return np->host == host;
}


/* get the host for this exporter, or 0 if there is an error */
afs_int32
afs_nfsclient_gethost(np)
    register struct nfsclientpag *np;
{
    if (np->type != EXP_NFS)
	return 0;
    return np->host;
}


/* if inname is non-null, a new system name value is set for the remote user (inname contains the new sysname). In all cases, outname returns the current sysname value for this remote user */
int 
afs_nfsclient_sysname(register struct nfsclientpag *np, char *inname, 
		      char ***outname, int *num, int allpags)
{
    register struct nfsclientpag *tnp;
    register afs_int32 i;
    char *cp;
    int count, t;
#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(afs_nfsclient_sysname);
    if (allpags > 0) {
	/* update every client, not just the one making the request */
	i = NHash(np->host);
	MObtainWriteLock(&afs_xnfspag, 315);
	for (tnp = afs_nfspags[i]; tnp; tnp = tnp->next) {
	    if (tnp != np && tnp->host == np->host)
		afs_nfsclient_sysname(tnp, inname, outname, num, -1);
	}
	MReleaseWriteLock(&afs_xnfspag);
    }
    if (inname) {
	    for(count=0; count < np->sysnamecount;++count) {
		afs_osi_Free(np->sysname[count], MAXSYSNAME);
		np->sysname[count] = NULL;
	    }
	for(count=0; count < *num;++count) {
	    np->sysname[count]= afs_osi_Alloc(MAXSYSNAME);
	}
	cp = inname;
	for(count=0; count < *num;++count) {
	    t = strlen(cp);
	    memcpy(np->sysname[count], cp, t+1); /* include null */
	    cp += t+1;
	}
	np->sysnamecount = *num;
    }
    if (allpags >= 0) {
	/* Don't touch our arguments when called recursively */
	*outname = np->sysname;
	*num = np->sysnamecount;
    }
    return 0;
}


/* Garbage collect routine for the nfs exporter. When pag is -1 then all entries are removed (used by the nfsclient_shutdown routine); else if it's non zero then only the entry with that pag is removed, else all "timedout" entries are removed. TimedOut entries are those who have no "unixuser" structures associated with them (i.e. unixusercnt == 0) and they haven't had any activity the last NFSCLIENTGC seconds */
void
afs_nfsclient_GC(exporter, pag)
     register struct afs_exporter *exporter;
     register afs_int32 pag;
{
    register struct nfsclientpag *np, **tnp, *nnp;
    register afs_int32 i, delflag;
	int count;

#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(afs_nfsclient_GC);
    MObtainWriteLock(&afs_xnfspag, 316);
    for (i = 0; i < NNFSCLIENTS; i++) {
	for (tnp = &afs_nfspags[i], np = *tnp; np; np = nnp) {
	    nnp = np->next;
	    delflag = 0;
	    if (np->refCount == 0 && np->lastcall < osi_Time() - NFSCLIENTGC)
		delflag = 1;
	    if ((pag == -1) || (!pag && delflag)
		|| (pag && (np->refCount == 0) && (np->pag == pag))) {
		*tnp = np->next;
		for(count=0; count < np->sysnamecount;++count) {
			afs_osi_Free(np->sysname[count], MAXSYSNAME);
		}
		afs_osi_Free(np, sizeof(struct nfsclientpag));
	    } else {
		tnp = &np->next;
	    }
	}
    }
    MReleaseWriteLock(&afs_xnfspag);
}


int
afs_nfsclient_stats(export)
     register struct afs_exporter *export;
{
    /* Nothing much to do here yet since most important stats are collected directly in the afs_exporter structure itself */
    AFS_STATCNT(afs_nfsclient_stats);
    return 0;
}

#ifdef AFS_AIX41_ENV
/* This is exposed so that vop_fid can test it, even if iauth is not
 *  installed.
 */
extern int afs_iauth_initd;
#endif

#ifdef AFS_AIX_IAUTH_ENV
char *afs_nfs_id = "AFSNFSTRANS";
/* afs_iauth_verify is the AFS authenticator for NFS.
 *
 * always returns 0.
 */
int
afs_iauth_verify(long id, fsid_t * fsidp, long host, int uid,
		 struct AFS_UCRED *credp, struct exportinfo *exp)
{
    int code;
    struct nfsclientpag *nfs_pag;
    afs_int32 dummypag;
    struct afs_exporter *outexporter = 0;


    /* Still needs basic test to see if exporter is on. And need to check the
     * whole no submounts bit.
     */

    if (id != (long)id)
	return 0;		/* not us. */

    /* Only care if it's AFS */
    if ((fsidp->val[0] != AFS_VFSMAGIC) || (fsidp->val[1] != AFS_VFSFSID)) {
	return 0;
    }

    AFS_GLOCK();
    code =
	afs_nfsclient_reqhandler((struct afs_exporter *)0, &credp, host,
				 &dummypag, &outexporter);
    if (!code && outexporter)
	EXP_RELE(outexporter);

    if (code) {
	/* ensure anonymous cred. */
	credp->cr_uid = credp->cr_ruid = (uid_t) - 2;	/* anonymous */
    }

    /* Mark this thread as an NFS translator thread. */
    credp->cr_rgid = NFSXLATOR_CRED;

    AFS_GUNLOCK();
    return 0;
}

/* afs_iauth_register - register the iauth verify routine. Returns 0 on success
 * and -1 on failure. Can fail because DFS has already registered.
 */
int
afs_iauth_register()
{
    if (nfs_iauth_register((unsigned long)afs_nfs_id, afs_iauth_verify))
	return -1;
    else {
	afs_iauth_initd = 1;
	return 0;
    }
}

/* afs_iauth_unregister - unregister the iauth verify routine. Called on shutdown. 
 */
void
afs_iauth_unregister()
{
    if (afs_iauth_initd)
	nfs_iauth_unregister((unsigned long)afs_nfs_id);
    afs_iauth_initd = 0;
}
#endif /* AFS_AIX_IAUTH_ENV */



void
shutdown_nfsclnt()
{
#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(afs_nfsclient_shutdown);
#ifdef AFS_AIX_IAUTH_ENV
    afs_iauth_register();
#endif
    afs_nfsclient_GC(afs_nfsexporter, -1);
    init_nfsexporter = 0;
}
#endif /* AFS_NONFSTRANS */
