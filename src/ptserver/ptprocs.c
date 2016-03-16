/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *                      (3) function addToGroup
 *
 *                          1. Eliminate the code that tests for adding groups
 *                             to groups. This is an error in normal AFS.
 *                          2. If adding a group to a group call AddToSGEntry
 *                             to add the id of the group it's a member of.
 *
 *                      (4) function Delete
 *
 *                          1. Print a messsage if an error is returned from
 *                             FindByID() and PTDEBUG is defined.
 *                          2. If removing a group from a group call
 *                             RemoveFromSGEntry to remove the id of the
 *                             group it's a member of.
 *                          3. Remove supergroup continuation records.
 *
 *                      (5) function RemoveFromGroup
 *
 *                          1. Eliminate the code that tests for adding groups
 *                             to groups. This is an error in normal AFS.
 *                          2. If removing a group from a group call
 *                             RemoveFromSGEntry to remove the id of the
 *                             group it's a member of.
 *
 *                      (6) Add new functions PR_ListSuperGroups and
 *                          listSuperGroups.
 *
 *                      (7) function isAMemberOf
 *
 *                          1. Allow groups to be members of groups.
 *
 *                      Transarc does not currently use opcodes past 520, but
 *                      they *could* decide at any time to use more opcodes.
 *                      If they did, then one part of our local mods,
 *                      ListSupergroups, would break.  I've therefore
 *                      renumbered it to 530, and put logic in to enable the
 *                      old opcode to work (for now).
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/stds.h>
#include <ctype.h>
#include <stdio.h>
#include <lock.h>
#include <afs/afsutil.h>
#include <ubik.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/auth.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <string.h>
#include "ptserver.h"
#include "pterror.h"
#include "ptprototypes.h"
#include "afs/audit.h"

#ifdef AFS_ATHENA_STDENV
#include <krb.h>
#endif

extern int restricted;
extern struct ubik_dbase *dbase;
extern int pr_noAuth;
extern char *pr_realmName;
extern int prp_group_default;
extern int prp_user_default;

static afs_int32 iNewEntry(struct rx_call *call, char aname[], afs_int32 aid,
			   afs_int32 oid, afs_int32 *cid);
static afs_int32 newEntry(struct rx_call *call, char aname[], afs_int32 flag,
			  afs_int32 oid, afs_int32 *aid, afs_int32 *cid);
static afs_int32 whereIsIt(struct rx_call *call, afs_int32 aid, afs_int32 *apos,
			   afs_int32 *cid);
static afs_int32 dumpEntry(struct rx_call *call, afs_int32 apos,
			   struct prdebugentry *aentry, afs_int32 *cid);
static afs_int32 addToGroup(struct rx_call *call, afs_int32 aid, afs_int32 gid,
			    afs_int32 *cid);
static afs_int32 nameToID(struct rx_call *call, namelist *aname, idlist *aid);
static afs_int32 idToName(struct rx_call *call, idlist *aid, namelist *aname);
static afs_int32 Delete(struct rx_call *call, afs_int32 aid, afs_int32 *cid);
static afs_int32 UpdateEntry(struct rx_call *call, afs_int32 aid, char *name,
			     struct PrUpdateEntry *uentry, afs_int32 *cid);
static afs_int32 removeFromGroup(struct rx_call *call, afs_int32 aid,
				 afs_int32 gid, afs_int32 *cid);
static afs_int32 getCPS(struct rx_call *call, afs_int32 aid, prlist *alist,
			afs_int32 *over, afs_int32 *cid);
static afs_int32 getCPS2(struct rx_call *call, afs_int32 aid, afs_uint32 ahost,
			 prlist *alist, afs_int32 *over, afs_int32 *cid);
static afs_int32 getHostCPS(struct rx_call *call, afs_uint32 ahost,
			    prlist *alist, afs_int32 *over);
static afs_int32 listMax(struct rx_call *call, afs_int32 *uid, afs_int32 *gid);
static afs_int32 setMax(struct rx_call *call, afs_int32 aid, afs_int32 gflag,
			afs_int32 *cid);
static afs_int32 listEntry(struct rx_call *call, afs_int32 aid,
			   struct prcheckentry *aentry, afs_int32 *cid);
static afs_int32 listEntries(struct rx_call *call, afs_int32 flag,
			     afs_int32 startindex, prentries *bulkentries,
			     afs_int32 *nextstartindex, afs_int32 *cid);
static afs_int32 put_prentries(struct prentry *tentry, prentries *bulkentries);
static afs_int32 changeEntry(struct rx_call *call, afs_int32 aid, char *name,
			     afs_int32 oid, afs_int32 newid, afs_int32 *cid);
static afs_int32 setFieldsEntry(struct rx_call *call, afs_int32 id,
				afs_int32 mask, afs_int32 flags,
				afs_int32 ngroups, afs_int32 nusers,
				afs_int32 spare1, afs_int32 spare2,
				afs_int32 *cid);
static afs_int32 listElements(struct rx_call *call, afs_int32 aid,
			      prlist *alist, afs_int32 *over, afs_int32 *cid);
#if defined(SUPERGROUPS)
static afs_int32 listSuperGroups(struct rx_call *call, afs_int32 aid,
				 prlist *alist, afs_int32 *over,
				 afs_int32 *cid);
#endif
static afs_int32 listOwned(struct rx_call *call, afs_int32 aid, prlist *alist,
			   afs_int32 *lastP, afs_int32 *cid);
static afs_int32 isAMemberOf(struct rx_call *call, afs_int32 uid, afs_int32 gid,
			     afs_int32 *flag, afs_int32 *cid);
static afs_int32 addWildCards(struct ubik_trans *tt, prlist *alist,
			      afs_uint32 host);
static afs_int32 WhoIsThisWithName(struct rx_call *acall,
				   struct ubik_trans *at, afs_int32 *aid,
				   char *aname);

/* when we abort, the ubik cachedVersion will be reset, so we'll read in the
 * header on the next call.
 * Abort the transaction and return the code.
 */
#define ABORT_WITH(tt,code) return(ubik_AbortTrans(tt),code)

static int
CreateOK(struct ubik_trans *ut, afs_int32 cid, afs_int32 oid, afs_int32 flag,
	 int admin)
{
    if (restricted && !admin)
	return 0;

    if (flag & PRFOREIGN) {
	/* Foreign users are recognized by the '@' sign and
	 * not by the PRFOREIGN flag.
	 */
	return 0;
    } else if (flag & PRGRP) {
	/* Allow anonymous group creation only if owner specified
	 * and running noAuth.
	 */
	if (cid == ANONYMOUSID) {
	    if ((oid == 0) || !pr_noAuth)
		return 0;
	}
    } else {			/* creating a user */
	if (!admin && !pr_noAuth)
	    return 0;
    }
    return 1;			/* OK! */
}

afs_int32
WhoIsThis(struct rx_call *acall, struct ubik_trans *at, afs_int32 *aid)
{
    int foreign = 0;
    /* aid is set to the identity of the caller, if known, else ANONYMOUSID */
    /* returns -1 and sets aid to ANONYMOUSID on any failure */
    struct rx_connection *tconn;
    afs_int32 code;
    char tcell[MAXKTCREALMLEN];
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
    int ilen;
    char vname[256];

    *aid = ANONYMOUSID;
    tconn = rx_ConnectionOf(acall);
    code = rx_SecurityClassOf(tconn);
    if (code == 0)
	return 0;
    else if (code == 1) {	/* vab class */
	goto done;		/* no longer supported */
    } else if (code == 2) {	/* kad class */
	if ((code = rxkad_GetServerInfo(acall->conn, NULL, 0 /*was &exp */ ,
					name, inst, tcell, NULL)))
	    goto done;
#if 0
	/* This test is unnecessary, since rxkad_GetServerInfo already check.
	 * In addition, this is wrong since exp must be unsigned. */
	if (exp < FT_ApproxTime())
	    goto done;
#endif
	if (tcell[0])
	    foreign = afs_is_foreign_ticket_name(name,inst,tcell,pr_realmName);

	strncpy(vname, name, sizeof(vname));
	if ((ilen = strlen(inst))) {
	    if (strlen(vname) + 1 + ilen >= sizeof(vname))
		goto done;
	    strcat(vname, ".");
	    strcat(vname, inst);
	}
	if (foreign) {
	    if (strlen(vname) + strlen(tcell) + 1 >= sizeof(vname))
		goto done;
	    strcat(vname, "@");
	    strcat(vname, tcell);
	}
	if (strcmp(AUTH_SUPERUSER, vname) == 0)
	    *aid = SYSADMINID;	/* special case for the fileserver */
	else {
	    lcstring(vname, vname, sizeof(vname));
	    code = NameToID(at, vname, aid);
	}
    }
  done:
    if (code && !pr_noAuth)
	return -1;
    return 0;
}

afs_int32
SPR_INewEntry(struct rx_call *call, char aname[], afs_int32 aid, afs_int32 oid)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = iNewEntry(call, aname, aid, oid, &cid);
    osi_auditU(call, PTS_INewEntEvent, code, AUD_ID, aid, AUD_STR, aname,
	       AUD_ID, oid, AUD_END);
    ViceLog(5, ("PTS_INewEntry: code %d cid %d aid %d aname %s oid %d\n", code, cid, aid, aname, oid));
    return code;
}

static afs_int32
iNewEntry(struct rx_call *call, char aname[], afs_int32 aid, afs_int32 oid,
	  afs_int32 *cid)
{
    /* used primarily for conversion - not intended to be used as usual means
     * of entering people into the database. */
    struct ubik_trans *tt;
    afs_int32 code;
    afs_int32 gflag = 0;
    int admin;

    stolower(aname);
    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    admin = IsAMemberOf(tt, *cid, SYSADMINID);

    /* first verify the id is good */
    if (aid == 0)
	ABORT_WITH(tt, PRPERM);
    if (aid < 0) {
	gflag |= PRGRP;
	/* only sysadmin can reuse a group id */
	if (!admin && !pr_noAuth && (aid != ntohl(cheader.maxGroup) - 1))
	    ABORT_WITH(tt, PRPERM);
    }
    if (FindByID(tt, aid))
	ABORT_WITH(tt, PRIDEXIST);

    /* check a few other things */
    if (!CreateOK(tt, *cid, oid, gflag, admin))
	ABORT_WITH(tt, PRPERM);

    code = CreateEntry(tt, aname, &aid, 1, gflag, oid, *cid);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    /* finally, commit transaction */
    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}


afs_int32
SPR_NewEntry(struct rx_call *call, char aname[], afs_int32 flag, afs_int32 oid,
	     afs_int32 *aid)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = newEntry(call, aname, flag, oid, aid, &cid);
    osi_auditU(call, PTS_NewEntEvent, code, AUD_ID, *aid, AUD_STR, aname,
	       AUD_ID, oid, AUD_END);
    ViceLog(5, ("PTS_NewEntry: code %d cid %d aid %d aname %s oid %d\n", code, cid, *aid, aname, oid));
    return code;
}

static afs_int32
newEntry(struct rx_call *call, char aname[], afs_int32 flag, afs_int32 oid,
	 afs_int32 *aid, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    int admin;
    char cname[PR_MAXNAMELEN];
    stolower(aname);
    code = Initdb();
    if (code)
	return code;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    /* this is for cross-cell self registration. It is not added in the
     * SPR_INewEntry because we want self-registration to only do
     * automatic id assignment.
     */
    code = WhoIsThisWithName(call, tt, cid, cname);
    if (code && code != 2)
	ABORT_WITH(tt, PRPERM);
    admin = IsAMemberOf(tt, *cid, SYSADMINID);
    if (code == 2 /* foreign cell request */) {
	if (!restricted && (strcmp(aname, cname) == 0)) {
	    /* can't autoregister while providing an owner id */
	    if (oid != 0)
		ABORT_WITH(tt, PRPERM);

	    admin = 1;
	    oid = SYSADMINID;
	    *cid = SYSADMINID;
	}
    }
    if (!CreateOK(tt, *cid, oid, flag, admin))
	ABORT_WITH(tt, PRPERM);

    code = CreateEntry(tt, aname, aid, 0, flag, oid, *cid);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}



afs_int32
SPR_WhereIsIt(struct rx_call *call, afs_int32 aid, afs_int32 *apos)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = whereIsIt(call, aid, apos, &cid);
    osi_auditU(call, PTS_WheIsItEvent, code, AUD_ID, aid, AUD_LONG, *apos,
	       AUD_END);
    ViceLog(125, ("PTS_WhereIsIt: code %d cid %d aid %d apos %d\n", code, cid, aid, *apos));
    return code;
}

static afs_int32
whereIsIt(struct rx_call *call, afs_int32 aid, afs_int32 *apos, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 temp;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);

    temp = FindByID(tt, aid);
    if (!temp)
	ABORT_WITH(tt, PRNOENT);
    *apos = temp;
    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}


afs_int32
SPR_DumpEntry(struct rx_call *call, afs_int32 apos,
	      struct prdebugentry *aentry)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = dumpEntry(call, apos, aentry, &cid);
    osi_auditU(call, PTS_DmpEntEvent, code, AUD_LONG, apos, AUD_END);
    ViceLog(125, ("PTS_DumpEntry: code %d cid %d apos %d\n", code, cid, apos));
    return code;
}

static afs_int32
dumpEntry(struct rx_call *call, afs_int32 apos, struct prdebugentry *aentry,
	  afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    code = pr_ReadEntry(tt, 0, apos, (struct prentry *)aentry);
    if (code)
	ABORT_WITH(tt, code);

    if (!AccessOK(tt, *cid, 0, PRP_STATUS_MEM, 0))
	ABORT_WITH(tt, PRPERM);

    /* Since prdebugentry is in the form of a prentry not a coentry, we will
     * return the coentry slots in network order where the string is. */
#if 0
    if (aentry->flags & PRCONT) {	/* wrong type, get coentry instead */
	code = pr_ReadCoEntry(tt, 0, apos, aentry);
	if (code)
	    ABORT_WITH(tt, code);
    }
#endif
    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_AddToGroup(struct rx_call *call, afs_int32 aid, afs_int32 gid)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = addToGroup(call, aid, gid, &cid);
    osi_auditU(call, PTS_AdToGrpEvent, code, AUD_ID, gid, AUD_ID, aid,
	       AUD_END);
    ViceLog(5, ("PTS_AddToGroup: code %d cid %d gid %d aid %d\n", code, cid, gid, aid));
    return code;
}

static afs_int32
addToGroup(struct rx_call *call, afs_int32 aid, afs_int32 gid, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 tempu;
    afs_int32 tempg;
    struct prentry tentry;
    struct prentry uentry;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    if (gid == ANYUSERID || gid == AUTHUSERID)
	return PRPERM;
    if (aid == ANONYMOUSID)
	return PRPERM;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    tempu = FindByID(tt, aid);
    if (!tempu)
	ABORT_WITH(tt, PRNOENT);
    memset(&uentry, 0, sizeof(uentry));
    code = pr_ReadEntry(tt, 0, tempu, &uentry);
    if (code != 0)
	ABORT_WITH(tt, code);

#if !defined(SUPERGROUPS)
    /* we don't allow groups as members of groups at present */
    if (uentry.flags & PRGRP)
	ABORT_WITH(tt, PRNOTUSER);
#endif

    tempg = FindByID(tt, gid);
    if (!tempg)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, tempg, &tentry);
    if (code != 0)
	ABORT_WITH(tt, code);
    /* make sure that this is a group */
    if (!(tentry.flags & PRGRP))
	ABORT_WITH(tt, PRNOTGROUP);
    if (!AccessOK(tt, *cid, &tentry, PRP_ADD_MEM, PRP_ADD_ANY))
	ABORT_WITH(tt, PRPERM);

    code = AddToEntry(tt, &tentry, tempg, aid);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

#if defined(SUPERGROUPS)
    if (uentry.flags & PRGRP)
	code = AddToSGEntry(tt, &uentry, tempu, gid);	/* mod group to be in sg */
    else
#endif
	/* now, modify the user's entry as well */
	code = AddToEntry(tt, &uentry, tempu, gid);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);
    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_NameToID(struct rx_call *call, namelist *aname, idlist *aid)
{
    afs_int32 code;

    code = nameToID(call, aname, aid);
    osi_auditU(call, PTS_NmToIdEvent, code, AUD_END);
    ViceLog(125, ("PTS_NameToID: code %d\n", code));
    return code;
}

static afs_int32
nameToID(struct rx_call *call, namelist *aname, idlist *aid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 i;
    int size;
    int count = 0;

    /* Initialize return struct */
    aid->idlist_len = 0;
    aid->idlist_val = NULL;

    size = aname->namelist_len;
    if (size == 0)
	return 0;
    if (size < 0)
	return PRTOOMANY;

    aid->idlist_val = (afs_int32 *) malloc(size * sizeof(afs_int32));
    if (!aid->idlist_val)
	return PRNOMEM;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    for (i = 0; i < aname->namelist_len; i++) {
	char vname[256];
	char *nameinst, *cell;

	strncpy(vname, aname->namelist_val[i], sizeof(vname));
	vname[sizeof(vname)-1] ='\0';

	nameinst = vname;
	cell = strchr(vname, '@');
	if (cell) {
	    *cell = '\0';
	    cell++;
	}

	if (cell && afs_is_foreign_ticket_name(nameinst,NULL,cell,pr_realmName))
	    code = NameToID(tt, aname->namelist_val[i], &aid->idlist_val[i]);
	else
	    code = NameToID(tt, nameinst, &aid->idlist_val[i]);

	if (code != PRSUCCESS)
	    aid->idlist_val[i] = ANONYMOUSID;
        osi_audit(PTS_NmToIdEvent, code, AUD_STR,
		   aname->namelist_val[i], AUD_ID, aid->idlist_val[i],
		   AUD_END);
	ViceLog(125, ("PTS_NameToID: code %d aname %s aid %d\n", code,
		      aname->namelist_val[i], aid->idlist_val[i]));
	if (count++ > 50) {
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();
#endif
	    count = 0;
	}
    }
    aid->idlist_len = aname->namelist_len;

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

/*
 * SPR_IDToName
 * Given an array of ids, find the name for each of them.
 * The array of ids and names is unlimited.
 */
afs_int32
SPR_IDToName(struct rx_call *call, idlist *aid, namelist *aname)
{
    afs_int32 code;

    code = idToName(call, aid, aname);
    osi_auditU(call, PTS_IdToNmEvent, code, AUD_END);
    ViceLog(125, ("PTS_IDToName: code %d\n", code));
    return code;
}

static afs_int32
idToName(struct rx_call *call, idlist *aid, namelist *aname)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 i;
    int size;
    int count = 0;

    /* leave this first for rpc stub */
    size = aid->idlist_len;
    if (size == 0)
	return 0;
    if (size < 0 || size > INT_MAX / PR_MAXNAMELEN)
	return PRTOOMANY;
    aname->namelist_val = (prname *) malloc(size * PR_MAXNAMELEN);
    aname->namelist_len = 0;
    if (aname->namelist_val == 0)
	return PRNOMEM;
    if (aid->idlist_len == 0)
	return 0;
    if (size == 0)
	return PRTOOMANY;	/* rxgen will probably handle this */

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    for (i = 0; i < aid->idlist_len; i++) {
	code = IDToName(tt, aid->idlist_val[i], aname->namelist_val[i]);
	if (code != PRSUCCESS)
	    sprintf(aname->namelist_val[i], "%d", aid->idlist_val[i]);
        osi_audit(PTS_IdToNmEvent, code, AUD_ID, aid->idlist_val[i],
		  AUD_STR, aname->namelist_val[i], AUD_END);
	ViceLog(125, ("PTS_idToName: code %d aid %d aname %s\n", code,
		      aid->idlist_val[i], aname->namelist_val[i]));
	if (count++ > 50) {
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();
#endif
	    count = 0;
	}
    }
    aname->namelist_len = aid->idlist_len;

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_Delete(struct rx_call *call, afs_int32 aid)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = Delete(call, aid, &cid);
    osi_auditU(call, PTS_DelEvent, code, AUD_ID, aid, AUD_END);
    ViceLog(5, ("PTS_Delete: code %d cid %d aid %d\n", code, cid, aid));
    return code;
}

static afs_int32
Delete(struct rx_call *call, afs_int32 aid, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    struct prentry tentry;
    afs_int32 loc, nptr;
    int count;

    code = Initdb();
    if (code)
	return code;
    if (code != PRSUCCESS)
	return code;
    if (aid == SYSADMINID || aid == ANYUSERID || aid == AUTHUSERID
	|| aid == ANONYMOUSID)
	return PRPERM;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);

    /* Read in entry to be deleted */
    loc = FindByID(tt, aid);
    if (loc == 0)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, loc, &tentry);
    if (code)
	ABORT_WITH(tt, PRDBFAIL);

    /* Do some access checking */
    if (tentry.owner != *cid && !IsAMemberOf(tt, *cid, SYSADMINID)
	&& !IsAMemberOf(tt, *cid, tentry.owner) && !pr_noAuth)
	ABORT_WITH(tt, PRPERM);

    /* Delete each continuation block as a separate transaction so that no one
     * transaction become to large to complete. */
    nptr = tentry.next;
    while (nptr != 0) {
	struct contentry centry;
	int i;

	code = pr_ReadCoEntry(tt, 0, nptr, &centry);
	if (code != 0)
	    ABORT_WITH(tt, PRDBFAIL);
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == PRBADID)
		continue;
	    if (centry.entries[i] == 0)
		break;
#if defined(SUPERGROUPS)
	    if (aid < 0 && centry.entries[i] < 0)	/* Supergroup */
		code = RemoveFromSGEntry(tt, aid, centry.entries[i]);
	    else
#endif
		code = RemoveFromEntry(tt, aid, centry.entries[i]);
	    if (code)
		ABORT_WITH(tt, code);
	    tentry.count--;	/* maintain count */
#ifndef AFS_PTHREAD_ENV
	    if ((i & 3) == 0)
		IOMGR_Poll();
#endif
	}
	tentry.next = centry.next;	/* thread out this block */
	code = FreeBlock(tt, nptr);	/* free continuation block */
	if (code)
	    ABORT_WITH(tt, code);
	code = pr_WriteEntry(tt, 0, loc, &tentry);	/* update main entry */
	if (code)
	    ABORT_WITH(tt, code);

	/* end this trans and start a new one */
	code = ubik_EndTrans(tt);
	if (code)
	    return code;
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();		/* just to keep the connection alive */
#endif
	code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
	if (code)
	    return code;
	code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
	if (code)
	    ABORT_WITH(tt, code);

	/* re-read entry to get consistent uptodate info */
	loc = FindByID(tt, aid);
	if (loc == 0)
	    ABORT_WITH(tt, PRNOENT);
	code = pr_ReadEntry(tt, 0, loc, &tentry);
	if (code)
	    ABORT_WITH(tt, PRDBFAIL);

	nptr = tentry.next;
    }

#if defined(SUPERGROUPS)
    /* Delete each continuation block as a separate transaction
     * so that no one transaction become too large to complete. */
    {
	struct prentryg *tentryg = (struct prentryg *)&tentry;
	nptr = tentryg->nextsg;
	while (nptr != 0) {
	    struct contentry centry;
	    int i;

	    code = pr_ReadCoEntry(tt, 0, nptr, &centry);
	    if (code != 0)
		ABORT_WITH(tt, PRDBFAIL);
	    for (i = 0; i < COSIZE; i++) {
		if (centry.entries[i] == PRBADID)
		    continue;
		if (centry.entries[i] == 0)
		    break;
		code = RemoveFromEntry(tt, aid, centry.entries[i]);
		if (code)
		    ABORT_WITH(tt, code);
		tentryg->countsg--;	/* maintain count */
#ifndef AFS_PTHREAD_ENV
		if ((i & 3) == 0)
		    IOMGR_Poll();
#endif
	    }
	    tentryg->nextsg = centry.next;	/* thread out this block */
	    code = FreeBlock(tt, nptr);	/* free continuation block */
	    if (code)
		ABORT_WITH(tt, code);
	    code = pr_WriteEntry(tt, 0, loc, &tentry);	/* update main entry */
	    if (code)
		ABORT_WITH(tt, code);

	    /* end this trans and start a new one */
	    code = ubik_EndTrans(tt);
	    if (code)
		return code;
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();	/* just to keep the connection alive */
#endif

	    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
	    if (code)
		return code;
	    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
	    if (code)
		ABORT_WITH(tt, code);

	    /* re-read entry to get consistent uptodate info */
	    loc = FindByID(tt, aid);
	    if (loc == 0)
		ABORT_WITH(tt, PRNOENT);
	    code = pr_ReadEntry(tt, 0, loc, &tentry);
	    if (code)
		ABORT_WITH(tt, PRDBFAIL);

	    nptr = tentryg->nextsg;
	}
    }

#endif /* SUPERGROUPS */

    /* Then move the owned chain, except possibly ourself to the orphan list.
     * Because this list can be very long and so exceed the size of a ubik
     * transaction, we start a new transaction every 50 entries. */
    count = 0;
    nptr = tentry.owned;
    while (nptr != 0) {
	struct prentry nentry;

	code = pr_ReadEntry(tt, 0, nptr, &nentry);
	if (code)
	    ABORT_WITH(tt, PRDBFAIL);
	nptr = tentry.owned = nentry.nextOwned;	/* thread out */

	if (nentry.id != tentry.id) {	/* don't add us to orphan chain! */
	    code = AddToOrphan(tt, nentry.id);
	    if (code)
		ABORT_WITH(tt, code);
	    count++;
#ifndef AFS_PTHREAD_ENV
	    if ((count & 3) == 0)
		IOMGR_Poll();
#endif
	}
	if (count < 50)
	    continue;
	code = pr_WriteEntry(tt, 0, loc, &tentry);	/* update main entry */
	if (code)
	    ABORT_WITH(tt, code);

	/* end this trans and start a new one */
	code = ubik_EndTrans(tt);
	if (code)
	    return code;
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();		/* just to keep the connection alive */
#endif
	code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
	if (code)
	    return code;
	code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
	if (code)
	    ABORT_WITH(tt, code);

	/* re-read entry to get consistent uptodate info */
	loc = FindByID(tt, aid);
	if (loc == 0)
	    ABORT_WITH(tt, PRNOENT);
	code = pr_ReadEntry(tt, 0, loc, &tentry);
	if (code)
	    ABORT_WITH(tt, PRDBFAIL);

	nptr = tentry.owned;
    }

    /* now do what's left of the deletion stuff */
    code = DeleteEntry(tt, &tentry, loc);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_UpdateEntry(struct rx_call *call, afs_int32 aid, char *name,
	        struct PrUpdateEntry *uentry)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = UpdateEntry(call, aid, name, uentry, &cid);
    osi_auditU(call, PTS_UpdEntEvent, code, AUD_ID, aid, AUD_STR, name, AUD_END);
    ViceLog(5, ("PTS_UpdateEntry: code %d cid %d aid %d name %s\n", code, cid, aid, name));
    return code;
}

afs_int32
UpdateEntry(struct rx_call *call, afs_int32 aid, char *name,
	    struct PrUpdateEntry *uentry, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    struct prentry tentry;
    afs_int32 loc;
    int id = 0;

    code = Initdb();
    if (code)
	return code;
    if (code != PRSUCCESS)
	return code;
    if (aid) {
	id = aid;
	if (aid == SYSADMINID || aid == ANYUSERID || aid == AUTHUSERID
	    || aid == ANONYMOUSID)
	    return PRPERM;
    }
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    code = IsAMemberOf(tt, *cid, SYSADMINID);
    if (!code && !pr_noAuth)
	ABORT_WITH(tt, PRPERM);

    /* Read in entry to be deleted */
    if (id) {
	loc = FindByID(tt, aid);
    } else {
	loc = FindByName(tt, name, &tentry);
    }
    if (loc == 0)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, loc, &tentry);
    if (code)
	ABORT_WITH(tt, PRDBFAIL);

    if (uentry->Mask & PRUPDATE_NAMEHASH) {
	int tloc;
	code = RemoveFromNameHash(tt, tentry.name, &tloc);
	if (code != PRSUCCESS)
	    ABORT_WITH(tt, PRDBFAIL);
	code = AddToNameHash(tt, tentry.name, loc);
	if (code)
	    ABORT_WITH(tt, code);
    }

    if (uentry->Mask & PRUPDATE_IDHASH) {
	int tloc;
	if (!id)
	    id = tentry.id;
	code = RemoveFromIDHash(tt, id, &tloc);
	if (code != PRSUCCESS)
	    ABORT_WITH(tt, PRDBFAIL);
	code = AddToIDHash(tt, id, loc);
	if (code)
	    ABORT_WITH(tt, code);
    }

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_RemoveFromGroup(struct rx_call *call, afs_int32 aid, afs_int32 gid)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = removeFromGroup(call, aid, gid, &cid);
    osi_auditU(call, PTS_RmFmGrpEvent, code, AUD_ID, gid, AUD_ID, aid,
	       AUD_END);
    ViceLog(5, ("PTS_RemoveFromGroup: code %d cid %d gid %d aid %d\n", code, cid, gid, aid));
    return code;
}

static afs_int32
removeFromGroup(struct rx_call *call, afs_int32 aid, afs_int32 gid,
		afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 tempu;
    afs_int32 tempg;
    struct prentry uentry;
    struct prentry gentry;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    tempu = FindByID(tt, aid);
    if (!tempu)
	ABORT_WITH(tt, PRNOENT);
    tempg = FindByID(tt, gid);
    if (!tempg)
	ABORT_WITH(tt, PRNOENT);
    memset(&uentry, 0, sizeof(uentry));
    memset(&gentry, 0, sizeof(gentry));
    code = pr_ReadEntry(tt, 0, tempu, &uentry);
    if (code != 0)
	ABORT_WITH(tt, code);
    code = pr_ReadEntry(tt, 0, tempg, &gentry);
    if (code != 0)
	ABORT_WITH(tt, code);
    if (!(gentry.flags & PRGRP))
	ABORT_WITH(tt, PRNOTGROUP);
#if !defined(SUPERGROUPS)
    if (uentry.flags & PRGRP)
	ABORT_WITH(tt, PRNOTUSER);
#endif
    if (!AccessOK(tt, *cid, &gentry, PRP_REMOVE_MEM, 0))
	ABORT_WITH(tt, PRPERM);
    code = RemoveFromEntry(tt, aid, gid);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);
#if defined(SUPERGROUPS)
    if (!(uentry.flags & PRGRP))
#endif
	code = RemoveFromEntry(tt, gid, aid);
#if defined(SUPERGROUPS)
    else
	code = RemoveFromSGEntry(tt, gid, aid);
#endif
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}


afs_int32
SPR_GetCPS(struct rx_call *call, afs_int32 aid, prlist *alist, afs_int32 *over)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = getCPS(call, aid, alist, over, &cid);
    osi_auditU(call, PTS_GetCPSEvent, code, AUD_ID, aid, AUD_END);
    ViceLog(125, ("PTS_GetCPS: code %d cid %d aid %d\n", code, cid, aid));
    return code;
}

static afs_int32
getCPS(struct rx_call *call, afs_int32 aid, prlist *alist, afs_int32 *over,
       afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 temp;
    struct prentry tentry;

    *over = 0;
    alist->prlist_len = 0;
    alist->prlist_val = NULL;
    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    temp = FindByID(tt, aid);
    if (!temp)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, temp, &tentry);
    if (code)
	ABORT_WITH(tt, code);

    /* afs does authenticate now */
    code = WhoIsThis(call, tt, cid);
    if (code || !AccessOK(tt, *cid, &tentry, PRP_MEMBER_MEM, PRP_MEMBER_ANY))
	ABORT_WITH(tt, PRPERM);

    code = GetList(tt, &tentry, alist, 1);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    return code;
}


int
inCPS(prlist CPS, afs_int32 id)
{
    int i;

    for (i = (CPS.prlist_len - 1); i >= 0; i--) {
	if (CPS.prlist_val[i] == id)
	    return (1);
    }
    return (0);
}


afs_int32
SPR_GetCPS2(struct rx_call *call, afs_int32 aid, afs_int32 ahost,
	    prlist *alist, afs_int32 *over)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = getCPS2(call, aid, ahost, alist, over, &cid);
    osi_auditU(call, PTS_GetCPS2Event, code, AUD_ID, aid, AUD_HOST, htonl(ahost),
	       AUD_END);
    ViceLog(125, ("PTS_GetCPS2: code %d cid %d aid %d ahost %d\n", code, cid, aid, ahost));
    return code;
}

static afs_int32
getCPS2(struct rx_call *call, afs_int32 aid, afs_uint32 ahost, prlist *alist,
	afs_int32 *over, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 temp;
    struct prentry tentry;
    struct prentry host_tentry;
    afs_int32 hostid;
    int host_list = 0;
    struct in_addr iaddr;
    char hoststr[16];

    *over = 0;
    iaddr.s_addr = ntohl(ahost);
    alist->prlist_len = 0;
    alist->prlist_val = NULL;
    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    if (aid != PRBADID) {
	temp = FindByID(tt, aid);
	if (!temp)
	    ABORT_WITH(tt, PRNOENT);
	code = pr_ReadEntry(tt, 0, temp, &tentry);
	if (code)
	    ABORT_WITH(tt, code);

	/* afs does authenticate now */
	code = WhoIsThis(call, tt, cid);
	if (code
	    || !AccessOK(tt, *cid, &tentry, PRP_MEMBER_MEM, PRP_MEMBER_ANY))
	    ABORT_WITH(tt, PRPERM);
    }
    code = NameToID(tt, afs_inet_ntoa_r(iaddr.s_addr, hoststr), &hostid);
    if (code == PRSUCCESS && hostid != 0) {
	temp = FindByID(tt, hostid);
	if (temp) {
	    code = pr_ReadEntry(tt, 0, temp, &host_tentry);
	    if (code == PRSUCCESS)
		host_list = 1;
	    else
		fprintf(stderr, "pr_ReadEntry returned %d\n", code);
	} else
	    fprintf(stderr, "FindByID Failed -- Not found\n");
    }
    if (host_list)
	code = GetList2(tt, &tentry, &host_tentry, alist, 1);
    else
	code = GetList(tt, &tentry, alist, 1);
    if (!code)
	code = addWildCards(tt, alist, ntohl(ahost));
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    return code;
}


afs_int32
SPR_GetHostCPS(struct rx_call *call, afs_int32 ahost, prlist *alist,
	       afs_int32 *over)
{
    afs_int32 code;

    code = getHostCPS(call, ahost, alist, over);
    osi_auditU(call, PTS_GetHCPSEvent, code, AUD_HOST, htonl(ahost), AUD_END);
    ViceLog(125, ("PTS_GetHostCPS: code %d ahost %d\n", code, ahost));
    return code;
}

afs_int32
getHostCPS(struct rx_call *call, afs_uint32 ahost, prlist *alist,
	   afs_int32 *over)
{
    afs_int32 code, temp;
    struct ubik_trans *tt;
    struct prentry host_tentry;
    afs_int32 hostid;
    struct in_addr iaddr;
    char hoststr[16];

    *over = 0;
    iaddr.s_addr = ntohl(ahost);
    alist->prlist_len = 0;
    alist->prlist_val = NULL;
    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = NameToID(tt, afs_inet_ntoa_r(iaddr.s_addr, hoststr), &hostid);
    if (code == PRSUCCESS && hostid != 0) {
	temp = FindByID(tt, hostid);
	if (temp) {
	    code = pr_ReadEntry(tt, 0, temp, &host_tentry);
	    if (code == PRSUCCESS) {
		code = GetList(tt, &host_tentry, alist, 0);
		if (code)
		    goto bad;
	    } else
		fprintf(stderr, "pr_ReadEntry returned %d\n", code);
	} else
	    fprintf(stderr, "FindByID Failed -- Not found\n");
    }
    code = addWildCards(tt, alist, ntohl(ahost));
  bad:
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    return code;
}


afs_int32
SPR_ListMax(struct rx_call *call, afs_int32 *uid, afs_int32 *gid)
{
    afs_int32 code;

    code = listMax(call, uid, gid);
    osi_auditU(call, PTS_LstMaxEvent, code, AUD_END);
    ViceLog(125, ("PTS_ListMax: code %d\n", code));
    return code;
}

afs_int32
listMax(struct rx_call *call, afs_int32 *uid, afs_int32 *gid)
{
    afs_int32 code;
    struct ubik_trans *tt;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = GetMax(tt, uid, gid);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_SetMax(struct rx_call *call, afs_int32 aid, afs_int32 gflag)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = setMax(call, aid, gflag, &cid);
    osi_auditU(call, PTS_SetMaxEvent, code, AUD_ID, aid, AUD_LONG, gflag,
	       AUD_END);
    ViceLog(125, ("PTS_SetMax: code %d cid %d aid %d gflag %d\n", code, cid, aid, gflag));
    return code;
}

static afs_int32
setMax(struct rx_call *call, afs_int32 aid, afs_int32 gflag, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    if (!AccessOK(tt, *cid, 0, 0, 0))
	ABORT_WITH(tt, PRPERM);
    if (((gflag & PRGRP) && (aid > 0)) || (!(gflag & PRGRP) && (aid < 0)))
	ABORT_WITH(tt, PRBADARG);

    code = SetMax(tt, aid, gflag);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_ListEntry(struct rx_call *call, afs_int32 aid, struct prcheckentry *aentry)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = listEntry(call, aid, aentry, &cid);
    osi_auditU(call, PTS_LstEntEvent, code, AUD_ID, aid, AUD_END);
    ViceLog(125, ("PTS_ListEntry: code %d cid %d aid %d\n", code, cid, aid));
    return code;
}

static afs_int32
listEntry(struct rx_call *call, afs_int32 aid, struct prcheckentry *aentry,
	  afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 temp;
    struct prentry tentry;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    temp = FindByID(tt, aid);
    if (!temp)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, temp, &tentry);
    if (code != 0)
	ABORT_WITH(tt, code);
    if (!AccessOK(tt, *cid, &tentry, PRP_STATUS_MEM, PRP_STATUS_ANY))
	ABORT_WITH(tt, PRPERM);

    aentry->flags = tentry.flags >> PRIVATE_SHIFT;
    if (aentry->flags == 0) {
	if (tentry.flags & PRGRP)
	    aentry->flags = prp_group_default >> PRIVATE_SHIFT;
	else
	    aentry->flags = prp_user_default >> PRIVATE_SHIFT;
    }
    aentry->owner = tentry.owner;
    aentry->id = tentry.id;
    strncpy(aentry->name, tentry.name, PR_MAXNAMELEN);
    aentry->creator = tentry.creator;
    aentry->ngroups = tentry.ngroups;
    aentry->nusers = tentry.nusers;
    aentry->count = tentry.count;
    memset(aentry->reserved, 0, sizeof(aentry->reserved));
    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
SPR_ListEntries(struct rx_call *call, afs_int32 flag, afs_int32 startindex,
		prentries *bulkentries, afs_int32 *nextstartindex)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = listEntries(call, flag, startindex, bulkentries, nextstartindex, &cid);
    osi_auditU(call, PTS_LstEntsEvent, code, AUD_LONG, flag, AUD_END);
    ViceLog(125, ("PTS_ListEntries: code %d cid %d flag %d\n", code, cid, flag));
    return code;
}

static afs_int32
listEntries(struct rx_call *call, afs_int32 flag, afs_int32 startindex,
	    prentries *bulkentries, afs_int32 *nextstartindex, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 i, eof, pos, maxentries, f;
    struct prentry tentry;
    afs_int32 pollcount = 0;

    *nextstartindex = -1;
    bulkentries->prentries_val = 0;
    bulkentries->prentries_len = 0;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    /* Make sure we are an authenticated caller and that we are on the
     * SYSADMIN list.
     */
    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    code = IsAMemberOf(tt, *cid, SYSADMINID);
    if (!code && !pr_noAuth)
	ABORT_WITH(tt, PRPERM);

    eof = ntohl(cheader.eofPtr) - sizeof(cheader);
    maxentries = eof / sizeof(struct prentry);
    for (i = startindex; i < maxentries; i++) {
	pos = i * sizeof(struct prentry) + sizeof(cheader);
	code = pr_ReadEntry(tt, 0, pos, &tentry);
	if (code)
	    goto done;

	if (++pollcount > 50) {
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();
#endif
	    pollcount = 0;
	}

	f = (tentry.flags & PRTYPE);
	if (((flag & PRUSERS) && (f == 0)) ||	/* User  entry */
	    ((flag & PRGROUPS) && (f & PRGRP))) {	/* Group entry */
	    code = put_prentries(&tentry, bulkentries);
	    if (code == -1)
		break;		/* Filled return array */
	    if (code)
		goto done;
	}
    }
    code = 0;
    if (i < maxentries)
	*nextstartindex = i;

  done:
    if (code) {
	if (bulkentries->prentries_val)
	    free(bulkentries->prentries_val);
	bulkentries->prentries_val = 0;
	bulkentries->prentries_len = 0;
	ABORT_WITH(tt, code);
    } else {
	code = ubik_EndTrans(tt);
    }
    if (code)
	return code;
    return PRSUCCESS;
}

#define PR_MAXENTRIES 500
static afs_int32
put_prentries(struct prentry *tentry, prentries *bulkentries)
{
    struct prlistentries *entry;

    if (bulkentries->prentries_val == 0) {
	bulkentries->prentries_len = 0;
	bulkentries->prentries_val =
	    (struct prlistentries *)malloc(PR_MAXENTRIES *
					   sizeof(struct prentry));
	if (!bulkentries->prentries_val) {
	    return (PRNOMEM);
	}
    }

    if (bulkentries->prentries_len >= PR_MAXENTRIES) {
	return (-1);
    }

    entry = (struct prlistentries *)bulkentries->prentries_val;
    entry += bulkentries->prentries_len;

    entry->flags = tentry->flags >> PRIVATE_SHIFT;
    if (entry->flags == 0) {
	entry->flags =
	    ((tentry->
	      flags & PRGRP) ? prp_group_default : prp_user_default) >>
	    PRIVATE_SHIFT;
    }
    entry->owner = tentry->owner;
    entry->id = tentry->id;
    entry->creator = tentry->creator;
    entry->ngroups = tentry->ngroups;
    entry->nusers = tentry->nusers;
    entry->count = tentry->count;
    strncpy(entry->name, tentry->name, PR_MAXNAMELEN);
    memset(entry->reserved, 0, sizeof(entry->reserved));
    bulkentries->prentries_len++;
    return 0;
}

afs_int32
SPR_ChangeEntry(struct rx_call *call, afs_int32 aid, char *name, afs_int32 oid,
		afs_int32 newid)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = changeEntry(call, aid, name, oid, newid, &cid);
    osi_auditU(call, PTS_ChgEntEvent, code, AUD_ID, aid, AUD_STR, name,
	       AUD_LONG, oid, AUD_LONG, newid, AUD_END);
    ViceLog(5, ("PTS_ChangeEntry: code %d cid %d aid %d name %s oid %d newid %d\n", code, cid, aid, name, oid, newid));
    return code;
}

static afs_int32
changeEntry(struct rx_call *call, afs_int32 aid, char *name, afs_int32 oid,
	    afs_int32 newid, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 pos;

    if (!name)
	return PRPERM;
    stolower(name);

    code = Initdb();
    if (code)
	return code;
    if (aid == ANYUSERID || aid == AUTHUSERID || aid == ANONYMOUSID
	|| aid == SYSADMINID)
	return PRPERM;
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    pos = FindByID(tt, aid);
    if (!pos)
	ABORT_WITH(tt, PRNOENT);
    /* protection check in changeentry */
    code = ChangeEntry(tt, aid, *cid, name, oid, newid);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    return code;
}

afs_int32
SPR_SetFieldsEntry(struct rx_call *call,
		   afs_int32 id,
		   afs_int32 mask, /* specify which fields to update */
		   afs_int32 flags, afs_int32 ngroups, afs_int32 nusers,
		   afs_int32 spare1, afs_int32 spare2)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code =
	setFieldsEntry(call, id, mask, flags, ngroups, nusers, spare1,
		       spare2, &cid);
    osi_auditU(call, PTS_SetFldEntEvent, code, AUD_ID, id, AUD_END);
    ViceLog(5, ("PTS_SetFieldsEntry: code %d cid %d id %d\n", code, cid, id));
    return code;
}

static afs_int32
setFieldsEntry(struct rx_call *call,
	       afs_int32 id,
	       afs_int32 mask, /* specify which fields to update */
	       afs_int32 flags, afs_int32 ngroups, afs_int32 nusers,
	       afs_int32 spare1, afs_int32 spare2, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 pos;
    struct prentry tentry;
    afs_int32 tflags;

    if (mask == 0)
	return 0;		/* no-op */
    code = Initdb();
    if (code)
	return code;
    if (id == ANYUSERID || id == AUTHUSERID || id == ANONYMOUSID)
	return PRPERM;
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);
    pos = FindByID(tt, id);
    if (!pos)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, pos, &tentry);
    if (code)
	ABORT_WITH(tt, code);
    tflags = tentry.flags;

    if (mask & (PR_SF_NGROUPS | PR_SF_NUSERS)) {
	if (!AccessOK(tt, *cid, 0, 0, 0))
	    ABORT_WITH(tt, PRPERM);
	if ((tflags & PRQUOTA) == 0) {	/* default if only setting one */
	    tentry.ngroups = tentry.nusers = 20;
	}
    } else {
	if (!AccessOK(tt, *cid, &tentry, 0, 0))
	    ABORT_WITH(tt, PRPERM);
    }

    if (mask & 0xffff) {	/* if setting flag bits */
	afs_int32 flagsMask = mask & 0xffff;
	tflags &= ~(flagsMask << PRIVATE_SHIFT);
	tflags |= (flags & flagsMask) << PRIVATE_SHIFT;
	tflags |= PRACCESS;
    }

    if (mask & PR_SF_NGROUPS) {	/* setting group limit */
	if (ngroups < 0)
	    ABORT_WITH(tt, PRBADARG);
	tentry.ngroups = ngroups;
	tflags |= PRQUOTA;
    }

    if (mask & PR_SF_NUSERS) {	/* setting foreign user limit */
	if (nusers < 0)
	    ABORT_WITH(tt, PRBADARG);
	tentry.nusers = nusers;
	tflags |= PRQUOTA;
    }
    tentry.flags = tflags;

    code = pr_WriteEntry(tt, 0, pos, &tentry);
    if (code)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    return code;
}

afs_int32
SPR_ListElements(struct rx_call *call, afs_int32 aid, prlist *alist,
		 afs_int32 *over)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = listElements(call, aid, alist, over, &cid);
    osi_auditU(call, PTS_LstEleEvent, code, AUD_ID, aid, AUD_END);
    ViceLog(125, ("PTS_ListElements: code %d cid %d aid %d\n", code, cid, aid));
    return code;
}

static afs_int32
listElements(struct rx_call *call, afs_int32 aid, prlist *alist,
	     afs_int32 *over, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 temp;
    struct prentry tentry;

    *over = 0;
    alist->prlist_len = 0;
    alist->prlist_val = NULL;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);

    temp = FindByID(tt, aid);
    if (!temp)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, temp, &tentry);
    if (code)
	ABORT_WITH(tt, code);
    if (!AccessOK(tt, *cid, &tentry, PRP_MEMBER_MEM, PRP_MEMBER_ANY))
	ABORT_WITH(tt, PRPERM);

    code = GetList(tt, &tentry, alist, 0);
    if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);
    return code;
}


afs_int32
SPR_ListSuperGroups(struct rx_call *call, afs_int32 aid, prlist *alist,
		    afs_int32 *over)
{
#if defined(SUPERGROUPS)
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = listSuperGroups(call, aid, alist, over, &cid);
    osi_auditU(call, PTS_LstSGrps, code, AUD_ID, aid, AUD_END);
    ViceLog(125, ("PTS_ListSuperGroups: code %d cid %d aid %d\n", code, cid, aid));
    return code;
#else
    return RXGEN_OPCODE;
#endif
}

#if defined(SUPERGROUPS)
static afs_int32
listSuperGroups(struct rx_call *call, afs_int32 aid, prlist *alist,
		afs_int32 *over, afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 temp;
    struct prentry tentry;

    alist->prlist_len = 0;
    alist->prlist_val = (afs_int32 *) 0;

    code = Initdb();
    if (code != PRSUCCESS)
	goto done;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	goto done;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);

    temp = FindByID(tt, aid);
    if (!temp)
	ABORT_WITH(tt, PRNOENT);
    code = pr_ReadEntry(tt, 0, temp, &tentry);
    if (code)
	ABORT_WITH(tt, code);
    if (!AccessOK(tt, *cid, &tentry, PRP_MEMBER_MEM, PRP_MEMBER_ANY))
	ABORT_WITH(tt, PRPERM);

    code = GetSGList(tt, &tentry, alist);
    *over = 0;
    if (code == PRTOOMANY)
	*over = 1;
    else if (code != PRSUCCESS)
	ABORT_WITH(tt, code);

    code = ubik_EndTrans(tt);

  done:
    return code;
}

#endif /* SUPERGROUPS */

/*
 * SPR_ListOwned
 * List the entries owned by this id.  If the id is zero,
 * return the orphans list. This will return up to PR_MAXGROUPS
 * at a time with the lastP available to get the rest. The
 * maximum value is enforced in GetOwnedChain().
 */
afs_int32
SPR_ListOwned(struct rx_call *call, afs_int32 aid, prlist *alist,
	      afs_int32 *lastP)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = listOwned(call, aid, alist, lastP, &cid);
    osi_auditU(call, PTS_LstOwnEvent, code, AUD_ID, aid, AUD_END);
    ViceLog(125, ("PTS_ListOwned: code %d cid %d aid %d\n", code, cid, aid));
    return code;
}

afs_int32
listOwned(struct rx_call *call, afs_int32 aid, prlist *alist, afs_int32 *lastP,
	  afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;
    struct prentry tentry;
    afs_int32 head = 0;
    afs_int32 start;

    alist->prlist_len = 0;
    alist->prlist_val = NULL;

    if (!lastP)
	return PRBADARG;
    start = *lastP;
    *lastP = 0;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    code = WhoIsThis(call, tt, cid);
    if (code)
	ABORT_WITH(tt, PRPERM);

    if (start) {
	code = pr_ReadEntry(tt, 0, start, &tentry);
	if (!code && (tentry.owner == aid))
	    head = start;	/* pick up where we left off */
    }

    if (!head) {
	if (aid) {
	    afs_int32 loc = FindByID(tt, aid);
	    if (loc == 0)
		ABORT_WITH(tt, PRNOENT);
	    code = pr_ReadEntry(tt, 0, loc, &tentry);
	    if (code)
		ABORT_WITH(tt, code);

	    if (!AccessOK(tt, *cid, &tentry, -1, PRP_OWNED_ANY))
		ABORT_WITH(tt, PRPERM);
	    head = tentry.owned;
	} else {
	    if (!AccessOK(tt, *cid, 0, 0, 0))
		ABORT_WITH(tt, PRPERM);
	    head = ntohl(cheader.orphan);
	}
    }

    code = GetOwnedChain(tt, &head, alist);
    if (code) {
	if (code == PRTOOMANY)
	    *lastP = head;
	else
	    ABORT_WITH(tt, code);
    }

    code = ubik_EndTrans(tt);
    return code;
}

afs_int32
SPR_IsAMemberOf(struct rx_call *call, afs_int32 uid, afs_int32 gid,
		afs_int32 *flag)
{
    afs_int32 code;
    afs_int32 cid = ANONYMOUSID;

    code = isAMemberOf(call, uid, gid, flag, &cid);
    osi_auditU(call, PTS_IsMemOfEvent, code, AUD_LONG, uid, AUD_LONG, gid,
	       AUD_END);
    ViceLog(125, ("PTS_IsAMemberOf: code %d cid %d uid %d gid %d\n", code, cid, uid, gid));
    return code;
}

static afs_int32
isAMemberOf(struct rx_call *call, afs_int32 uid, afs_int32 gid, afs_int32 *flag,
	    afs_int32 *cid)
{
    afs_int32 code;
    struct ubik_trans *tt;

    code = Initdb();
    if (code != PRSUCCESS)
	return code;
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code)
	ABORT_WITH(tt, code);
    code = read_DbHeader(tt);
    if (code)
	ABORT_WITH(tt, code);

    {
	afs_int32 uloc = FindByID(tt, uid);
	afs_int32 gloc = FindByID(tt, gid);
	struct prentry uentry, gentry;

	if (!uloc || !gloc)
	    ABORT_WITH(tt, PRNOENT);
	code = WhoIsThis(call, tt, cid);
	if (code)
	    ABORT_WITH(tt, PRPERM);
	code = pr_ReadEntry(tt, 0, uloc, &uentry);
	if (code)
	    ABORT_WITH(tt, code);
	code = pr_ReadEntry(tt, 0, gloc, &gentry);
	if (code)
	    ABORT_WITH(tt, code);
#if !defined(SUPERGROUPS)
	if ((uentry.flags & PRGRP) || !(gentry.flags & PRGRP))
	    ABORT_WITH(tt, PRBADARG);
#else
	if (!(gentry.flags & PRGRP))
	    ABORT_WITH(tt, PRBADARG);
#endif
	if (!AccessOK(tt, *cid, &uentry, 0, PRP_MEMBER_ANY)
	    && !AccessOK(tt, *cid, &gentry, PRP_MEMBER_MEM, PRP_MEMBER_ANY))
	    ABORT_WITH(tt, PRPERM);
    }

    *flag = IsAMemberOf(tt, uid, gid);
    code = ubik_EndTrans(tt);
    return code;
}

static afs_int32
addWildCards(struct ubik_trans *tt, prlist *alist, afs_uint32 host)
{
    afs_int32 temp;
    struct prentry tentry;
    prlist wlist;
    unsigned wild = htonl(0xffffff00);
    struct in_addr iaddr;
    afs_int32 hostid;
    int size = 0, i, code;
    int added = 0;
    char hoststr[16];

    while ((host = (host & wild))) {
	wild = htonl(ntohl(wild) << 8);
	iaddr.s_addr = host;
	code = NameToID(tt, afs_inet_ntoa_r(iaddr.s_addr, hoststr), &hostid);
	if (code == PRSUCCESS && hostid != 0) {
	    temp = FindByID(tt, hostid);
	    if (temp) {
		code = pr_ReadEntry(tt, 0, temp, &tentry);
		if (code != PRSUCCESS)
		    continue;
	    } else
		continue;
	} else
	    continue;
	wlist.prlist_len = 0;
	wlist.prlist_val = NULL;

	code = GetList(tt, &tentry, &wlist, 0);
	if (code)
	    return code;
	added += wlist.prlist_len;
	for (i = 0; i < wlist.prlist_len; i++) {
	    if (!inCPS(*alist, wlist.prlist_val[i]))
		if ((code = AddToPRList(alist, &size, wlist.prlist_val[i]))) {
		    free(wlist.prlist_val);
		    return (code);
		}
	}
	if (wlist.prlist_val)
	    free(wlist.prlist_val);
    }
    if (added)
	qsort(alist->prlist_val, alist->prlist_len, sizeof(afs_int32), IDCmp);
    return 0;
}

static afs_int32
WhoIsThisWithName(struct rx_call *acall, struct ubik_trans *at, afs_int32 *aid,
		  char *aname)
{
    /* aid is set to the identity of the caller, if known, else ANONYMOUSID */
    /* returns -1 and sets aid to ANONYMOUSID on any failure */
    struct rx_connection *tconn;
    afs_int32 code;
    char tcell[MAXKTCREALMLEN];
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
    int ilen;
    char vname[256];

    *aid = ANONYMOUSID;
    tconn = rx_ConnectionOf(acall);
    code = rx_SecurityClassOf(tconn);
    if (code == 0)
	return 0;
    else if (code == 1) {	/* vab class */
	goto done;		/* no longer supported */
    } else if (code == 2) {	/* kad class */

	int clen;

	if ((code = rxkad_GetServerInfo(acall->conn, NULL, 0 /*was &exp */ ,
					name, inst, tcell, NULL)))
	    goto done;


	strncpy(vname, name, sizeof(vname));
	if ((ilen = strlen(inst))) {
	    if (strlen(vname) + 1 + ilen >= sizeof(vname))
		goto done;
	    strcat(vname, ".");
	    strcat(vname, inst);
	}
	if ((clen = strlen(tcell))) {
	    int foreign = afs_is_foreign_ticket_name(name,inst,tcell,pr_realmName);

	    if (foreign) {
		if (strlen(vname) + 1 + clen >= sizeof(vname))
		    goto done;
		strcat(vname, "@");
		strcat(vname, tcell);
		lcstring(vname, vname, sizeof(vname));
		code = NameToID(at, vname, aid);
		strcpy(aname, vname);
		return 2;
	    }
	}

	if (strcmp(AUTH_SUPERUSER, vname) == 0)
	    *aid = SYSADMINID;	/* special case for the fileserver */
	else {
	    lcstring(vname, vname, sizeof(vname));
	    code = NameToID(at, vname, aid);
	}
    }
  done:
    if (code && !pr_noAuth)
	return -1;
    return 0;
}
