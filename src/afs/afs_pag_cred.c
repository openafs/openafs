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

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "afs/unified_afs.h"
#include "rx/rx_globals.h"
#include "pagcb.h"


struct afspag_cell {
    struct afspag_cell *next;
    char *cellname;
    afs_int32 cellnum;
};

afs_rwlock_t afs_xpagcell;
afs_rwlock_t afs_xpagsys;
static int lastcell = 0;
static struct afspag_cell *cells = 0;
static struct afspag_cell *primary_cell = 0;


struct afspag_cell *afspag_GetCell(char *acell)
{
    struct afspag_cell *tcell;

    ObtainWriteLock(&afs_xpagcell, 820);

    for (tcell = cells; tcell; tcell = tcell->next) {
	if (!strcmp(acell, tcell->cellname))
	    break;
    }

    if (!tcell) {
	tcell = (struct afspag_cell *)afs_osi_Alloc(sizeof(struct afspag_cell));
	if (!tcell)
	    goto out;
	tcell->cellname = (char *)afs_osi_Alloc(strlen(acell) + 1);
	if (!tcell->cellname) {
	    afs_osi_Free(tcell, sizeof(struct afspag_cell));
	    tcell = 0;
	    goto out;
	}
	strcpy(tcell->cellname, acell);
	tcell->cellnum = ++lastcell;
	tcell->next = cells;
	cells = tcell;
	if (!primary_cell) primary_cell = tcell;
    }

out:
    ReleaseWriteLock(&afs_xpagcell);
    return tcell;
}


struct afspag_cell *afspag_GetPrimaryCell()
{
    struct afspag_cell *tcell;

    ObtainWriteLock(&afs_xpagcell, 821);
    tcell = primary_cell;
    ReleaseWriteLock(&afs_xpagcell);
    return tcell;
}


void afspag_SetPrimaryCell(char *acell)
{
    struct afspag_cell *tcell;

    tcell = afspag_GetCell(acell);
    ObtainWriteLock(&afs_xpagcell, 822);
    primary_cell = tcell;
    ReleaseWriteLock(&afs_xpagcell);
}


int afspag_PUnlog(char *ain, afs_int32 ainSize, struct AFS_UCRED **acred)
{
    register afs_int32 i;
    register struct unixuser *tu;
    afs_int32 pag, uid;

    AFS_STATCNT(PUnlog);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    pag = PagInCred(*acred);
    uid = (pag == NOPAG) ? (*acred)->cr_uid : pag;
    i = UHash(uid);
    ObtainWriteLock(&afs_xuser, 823);
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (tu->uid == uid) {
	    tu->vid = UNDEFVID;
	    tu->states &= ~UHasTokens;
	    /* security is not having to say you're sorry */
	    memset((char *)&tu->ct, 0, sizeof(struct ClearToken));
#ifdef UKERNEL
	    /* set the expire times to 0, causes
	     * afs_GCUserData to remove this entry
	     */
	    tu->ct.EndTimestamp = 0;
	    tu->tokenTime = 0;
#endif /* UKERNEL */
	}
    }
    ReleaseWriteLock(&afs_xuser);
    return 0;
}


int afspag_PSetTokens(char *ain, afs_int32 ainSize, struct AFS_UCRED **acred)
{
    afs_int32 i;
    register struct unixuser *tu;
    struct afspag_cell *tcell;
    struct ClearToken clear;
    char *stp;
    int stLen;
    afs_int32 flag, set_parent_pag = 0;
    afs_int32 pag, uid;

    AFS_STATCNT(PSetTokens);
    if (!afs_resourceinit_flag) {
	return EIO;
    }
    memcpy((char *)&i, ain, sizeof(afs_int32));
    ain += sizeof(afs_int32);
    stp = ain;			/* remember where the ticket is */
    if (i < 0 || i > MAXKTCTICKETLEN)
	return EINVAL;		/* malloc may fail */
    stLen = i;
    ain += i;			/* skip over ticket */
    memcpy((char *)&i, ain, sizeof(afs_int32));
    ain += sizeof(afs_int32);
    if (i != sizeof(struct ClearToken)) {
	return EINVAL;
    }
    memcpy((char *)&clear, ain, sizeof(struct ClearToken));
    if (clear.AuthHandle == -1)
	clear.AuthHandle = 999;	/* more rxvab compat stuff */
    ain += sizeof(struct ClearToken);
    if (ainSize != 2 * sizeof(afs_int32) + stLen + sizeof(struct ClearToken)) {
	/* still stuff left?  we've got primary flag and cell name.  Set these */
	memcpy((char *)&flag, ain, sizeof(afs_int32));	/* primary id flag */
	ain += sizeof(afs_int32);	/* skip id field */
	/* rest is cell name, look it up */
	/* some versions of gcc appear to need != 0 in order to get this right */
	if ((flag & 0x8000) != 0) {	/* XXX Use Constant XXX */
	    flag &= ~0x8000;
	    set_parent_pag = 1;
	}
	tcell = afspag_GetCell(ain);
    } else {
	/* default to primary cell, primary id */
	flag = 1;		/* primary id */
	tcell = afspag_GetPrimaryCell();
    }
    if (!tcell) return ESRCH;
    if (set_parent_pag) {
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#if defined(AFS_DARWIN_ENV)
	struct proc *p = current_proc();	/* XXX */
#else
	struct proc *p = curproc;	/* XXX */
#endif
#ifndef AFS_DARWIN80_ENV
	uprintf("Process %d (%s) tried to change pags in PSetTokens\n",
		p->p_pid, p->p_comm);
#endif
	setpag(p, acred, -1, &pag, 1);
#else
#ifdef	AFS_OSF_ENV
	setpag(u.u_procp, acred, -1, &pag, 1);	/* XXX u.u_procp is a no-op XXX */
#else
	setpag(acred, -1, &pag, 1);
#endif
#endif
    }
    pag = PagInCred(*acred);
    uid = (pag == NOPAG) ? (*acred)->cr_uid : pag;
    /* now we just set the tokens */
    tu = afs_GetUser(uid, tcell->cellnum, WRITE_LOCK);
    if (!tu->cellinfo)
	tu->cellinfo = (void *)tcell;
    tu->vid = clear.ViceId;
    if (tu->stp != NULL) {
	afs_osi_Free(tu->stp, tu->stLen);
    }
    tu->stp = (char *)afs_osi_Alloc(stLen);
    tu->stLen = stLen;
    memcpy(tu->stp, stp, stLen);
    tu->ct = clear;
#ifndef AFS_NOSTATS
    afs_stats_cmfullperf.authent.TicketUpdates++;
    afs_ComputePAGStats();
#endif /* AFS_NOSTATS */
    tu->states |= UHasTokens;
    tu->states &= ~UTokensBad;
    afs_SetPrimary(tu, flag);
    tu->tokenTime = osi_Time();
    afs_PutUser(tu, WRITE_LOCK);

    return 0;
}


int
SPAGCB_GetCreds(struct rx_call *a_call, afs_int32 a_uid,
                CredInfos *a_creds)
{
    struct unixuser *tu;
    CredInfo *tci;
    int bucket, count, i = 0, clen;
    char *cellname;

    RX_AFS_GLOCK();

    memset(a_creds, 0, sizeof(struct CredInfos));
    if ((rx_HostOf(rx_PeerOf(rx_ConnectionOf(a_call))) != afs_nfs_server_addr
	||  rx_PortOf(rx_PeerOf(rx_ConnectionOf(a_call))) != htons(7001))
#if 0 /* for debugging ONLY! */
	&&  rx_PortOf(rx_PeerOf(rx_ConnectionOf(a_call))) != htons(7901)
#endif
        ) {
	RX_AFS_GUNLOCK();
	return UAEPERM;
    }

    ObtainWriteLock(&afs_xuser, 823);

    /* count them first */
    bucket = UHash(a_uid);
    for (count = 0, tu = afs_users[bucket]; tu; tu = tu->next) {
	if (tu->uid == a_uid) count++;
    }

    if (!count) {
	ReleaseWriteLock(&afs_xuser);
	RX_AFS_GUNLOCK();
	return UAESRCH;
    }

    a_creds->CredInfos_val =
	(CredInfo *)afs_osi_Alloc(count * sizeof(CredInfo));
    if (!a_creds->CredInfos_val)
	goto out;
    a_creds->CredInfos_len = count;
    memset(a_creds->CredInfos_val, 0, count * sizeof(CredInfo));

    for (i = 0, tu = afs_users[bucket]; tu; tu = tu->next, i++) {
	if (tu->uid == a_uid && tu->cellinfo &&
	    (tu->states & UHasTokens) && !(tu->states & UTokensBad)) {

	    tci = &a_creds->CredInfos_val[i];
	    tci->vid               = tu->vid;
	    tci->ct.AuthHandle     = tu->ct.AuthHandle;
	    memcpy(tci->ct.HandShakeKey, tu->ct.HandShakeKey, 8);
	    tci->ct.ViceId         = tu->ct.ViceId;
	    tci->ct.BeginTimestamp = tu->ct.BeginTimestamp;
	    tci->ct.EndTimestamp   = tu->ct.EndTimestamp;

	    cellname = ((struct afspag_cell *)(tu->cellinfo))->cellname;
	    clen = strlen(cellname) + 1;
	    tci->cellname = afs_osi_Alloc(clen);
	    if (!tci->cellname)
		goto out;
	    memcpy(tci->cellname, cellname, clen);

	    tci->st.st_len = tu->stLen;
	    tci->st.st_val = afs_osi_Alloc(tu->stLen);
	    if (!tci->st.st_val) {
		afs_osi_Free(tci->cellname, clen);
		goto out;
	    }
	    memcpy(tci->st.st_val, tu->stp, tu->stLen);
	    if (tu->states & UPrimary)
		tci->states |= UPrimary;
	}
    }

    ReleaseWriteLock(&afs_xuser);
    RX_AFS_GUNLOCK();
    return 0;

out:
    if (a_creds->CredInfos_val) {
	while (i-- > 0) {
	    afs_osi_Free(a_creds->CredInfos_val[i].st.st_val,
			 a_creds->CredInfos_val[i].st.st_len);
	    afs_osi_Free(a_creds->CredInfos_val[i].cellname,
			 strlen(a_creds->CredInfos_val[i].cellname) + 1);
	}
	afs_osi_Free(a_creds->CredInfos_val, count * sizeof(CredInfo));
    }

    ReleaseWriteLock(&afs_xuser);
    RX_AFS_GUNLOCK();
    return UAENOMEM;
}


int afspag_PSetSysName(char *ain, afs_int32 ainSize, struct AFS_UCRED **acred)
{
    int setsysname, count, t;
    char *cp, *setp;

    setp = ain;
    memcpy((char *)&setsysname, ain, sizeof(afs_int32));
    ain += sizeof(afs_int32);
    if (!setsysname)
	return 0; /* nothing to do locally */

    /* Check my args */
    if (setsysname < 0 || setsysname > MAXNUMSYSNAMES)
	return EINVAL;
    if (!afs_osi_suser(*acred))
	return EACCES;
    for (cp = ain, count = 0; count < setsysname; count++) {
	/* won't go past end of ain since maxsysname*num < ain length */
	t = strlen(cp);
	if (t >= MAXSYSNAME || t <= 0)
	    return EINVAL;
	/* check for names that can shoot us in the foot */
	if (*cp == '.' && (cp[1] == 0 || (cp[1] == '.' && cp[2] == 0)))
	    return EINVAL;
	cp += t + 1;
    }

    ObtainWriteLock(&afs_xpagsys, 824);
    for (cp = ain, count = 0; count < setsysname; count++) {
	t = strlen(cp);
	memcpy(afs_sysnamelist[count], cp, t + 1);
	cp += t + 1;
    }
    afs_sysnamecount = setsysname;
    afs_sysnamegen++;
    ReleaseWriteLock(&afs_xpagsys);

    /* Change the arguments so we pass the allpags flag to the server */
    setsysname |= 0x8000;
    memcpy(setp, (char *)&setsysname, sizeof(afs_int32));
    return 0;
}


int
SPAGCB_GetSysName(struct rx_call *a_call, afs_int32 a_uid,
		  SysNameList *a_sysnames)
{
    int i;

    RX_AFS_GLOCK();

    ObtainReadLock(&afs_xpagsys);
    memset(a_sysnames, 0, sizeof(struct SysNameList));

    a_sysnames->SysNameList_len = afs_sysnamecount;
    a_sysnames->SysNameList_val =
	afs_osi_Alloc(afs_sysnamecount * sizeof(SysNameEnt));
    if (!a_sysnames->SysNameList_val)
	goto out;

    for (i = 0; i < afs_sysnamecount; i++) {
	a_sysnames->SysNameList_val[i].sysname =
	    afs_osi_Alloc(strlen(afs_sysnamelist[i]) + 1);
	if (!a_sysnames->SysNameList_val[i].sysname)
	    goto out;
	strcpy(a_sysnames->SysNameList_val[i].sysname, afs_sysnamelist[i]);
    }

    ReleaseReadLock(&afs_xpagsys);
    RX_AFS_GUNLOCK();
    return 0;

out:
    if (a_sysnames->SysNameList_val) {
	while (i-- > 0) {
	    afs_osi_Free(a_sysnames->SysNameList_val[i].sysname,
			 strlen(a_sysnames->SysNameList_val[i].sysname) + 1);
	}
	afs_osi_Free(a_sysnames->SysNameList_val,
		     afs_sysnamecount * sizeof(SysNameEnt));
    }

    ReleaseWriteLock(&afs_xpagsys);
    RX_AFS_GUNLOCK();
    return UAENOMEM;
}
