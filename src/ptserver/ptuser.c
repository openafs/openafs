/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#if defined(UKERNEL)
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afs_usrops.h"
#include "../afs/afsincludes.h"
#include "../afs/stds.h"
#include "../rx/rx.h"
#include "../rx/xdr.h"
#include "../rx/rxkad.h"
#include "../afs/auth.h"
#include "../afs/cellconfig.h"
#include "../afs/afsutil.h"

#include "../afs/ptclient.h"
#include "../afs/pterror.h"

#else /* defined(UKERNEL) */
#include <afs/param.h>
#include <afs/stds.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV 
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <rx/rxkad.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include "ptclient.h"
#include "pterror.h"
#endif /* defined(UKERNEL) */


struct ubik_client *pruclient = 0;
static afs_int32 lastLevel;			/* security level pruclient, if any */

static char *whoami = "libprot";

afs_int32 pr_Initialize (secLevel, confDir, cell)
  IN afs_int32 secLevel;
  IN char *confDir;
  IN char *cell;
{
    afs_int32 code;
    struct rx_connection *serverconns[MAXSERVERS];
    struct rx_securityClass *sc[3];
    static struct afsconf_dir *tdir = 0; /* only do this once */
    static char tconfDir[100];
    struct ktc_token ttoken;
    afs_int32 scIndex;
    static struct afsconf_cell info;
    afs_int32 i;
    char cellstr[64];

    initialize_pt_error_table();
    initialize_rxk_error_table();
    initialize_acfg_error_table();
    initialize_ktc_error_table();
/*
    initialize_lwp_error_table();
    initialize_rx_error_table();
*/

    if (strcmp(confDir, tconfDir)) {
	/*
	 * Different conf dir; force re-evaluation.
	 */
	tdir = (struct afsconf_dir *)0;
	pruclient = (struct ubik_client *)0;
    }
    if (tdir == 0) {
	strncpy(tconfDir, confDir, sizeof(tconfDir));
#if defined(UKERNEL)
	tdir = afs_cdir;
	if (!cell) {
	    cell = afs_LclCellName;
	}
#else /* defined(UKERNEL) */  
	tdir = afsconf_Open(confDir);
	if (!tdir) {
	    fprintf(stderr,
		    "libprot: Could not open configuration directory: %s.\n",
		    confDir);
	    return -1;
	}

	if (!cell) {
	   code = afsconf_GetLocalCell(tdir, cellstr, sizeof(cellstr));
	   if (code) {
	      fprintf(stderr,
		      "vos: can't get local cell name - check %s/%s\n",
		      confDir, AFSDIR_THISCELL_FILE);
	      exit(1);
	   }
	   cell = cellstr;
	}
#endif /* defined(UKERNEL) */  

	code = afsconf_GetCellInfo(tdir,cell,"afsprot",&info);
	if (code) {
	    fprintf(stderr, "libprot: Could not locate cell %s in %s/%s\n",
		    cell, confDir, AFSDIR_CELLSERVDB_FILE);
	    return code;
	}
    }

    /* If we already have a client and it is at the security level we
     * want, don't get a new one. Unless the security level is 2 in
     * which case we will get one (and re-read the key file).
     */
    if (pruclient && (lastLevel == secLevel) && (secLevel != 2))
       return 0;
    
    code = rx_Init(0);
    if (code) {
	fprintf(stderr,"libprot:  Could not initialize rx.\n");
	return code;
    }

    scIndex = secLevel;
    sc[0] = 0;
    sc[1] = 0;
    sc[2] = 0;
    /* Most callers use secLevel==1, however, the fileserver uses secLevel==2
     * to force use of the KeyFile.  secLevel == 0 implies -noauth was
     * specified. */
    if ((secLevel == 2) && (afsconf_GetLatestKey (tdir, 0,0) == 0)) {
	/* If secLevel is two assume we're on a file server and use
         * ClientAuthSecure if possible. */
	code = afsconf_ClientAuthSecure (tdir, &sc[2], &scIndex);
	if (code) scIndex = 0;		/* use noauth */
	if (scIndex != 2)
	    /* if there was a problem, an unauthenticated conn is returned */
	    sc[scIndex] = sc[2];
    }
    else if (secLevel > 0) {
	struct ktc_principal sname;
        strcpy(sname.cell,info.name);
	sname.instance[0] = 0;
	strcpy(sname.name, "afs");
	code = ktc_GetToken(&sname,&ttoken, sizeof(ttoken), (char *)0);
	if (code) scIndex = 0;
	else {
	    if (ttoken.kvno >= 0 && ttoken.kvno <= 255)
		/* this is a kerberos ticket, set scIndex accordingly */
		scIndex = 2;
	    else {
		fprintf (stderr,
			 "libprot: funny kvno (%d) in ticket, proceeding\n",
			 ttoken.kvno);
		scIndex = 2;
	    }
	    sc[2] = (struct rx_securityClass *) rxkad_NewClientSecurityObject
		(rxkad_clear, &ttoken.sessionKey, ttoken.kvno,
		 ttoken.ticketLen, ttoken.ticket);
	}
    }
    if (scIndex == 1) return PRBADARG;
    if ((scIndex == 0) && (sc[0] == 0))
	sc[0] = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
    if ((scIndex == 0) && (secLevel != 0))
	com_err (whoami, code,
		 "Could not get afs tokens, running unauthenticated.");

    bzero (serverconns, sizeof(serverconns)); /* terminate list!!! */
    for (i = 0;i<info.numServers;i++) 
	serverconns[i] = rx_NewConnection
	    (info.hostAddr[i].sin_addr.s_addr, info.hostAddr[i].sin_port,
	     PRSRV, sc[scIndex], scIndex);

    code = ubik_ClientInit(serverconns, &pruclient);
    if (code) {
	com_err (whoami, code, "ubik client init failed.");
	return code;
    }
    lastLevel = scIndex;

    code = rxs_Release (sc[scIndex]);
    return code;
}


pr_End()
{
   int code = 0;

    if (pruclient) {
	code = ubik_ClientDestroy (pruclient);
	pruclient = 0;
    }
   return code;
}



pr_CreateUser(name,id)
char name[PR_MAXNAMELEN];
afs_int32 *id;
{
    register afs_int32 code;

    stolower(name);
    if (*id) {
	code = ubik_Call(PR_INewEntry,pruclient,0,name,*id,0);
	return code;
    }
    else {
	code = ubik_Call(PR_NewEntry, pruclient, 0, name,0,0,id);
	return code;
    }
    
}

pr_CreateGroup(name,owner, id)
char name[PR_MAXNAMELEN];
char owner[PR_MAXNAMELEN];
afs_int32 *id;
{
    register afs_int32 code;
    afs_int32 oid = 0;
    afs_int32 flags = 0;
    
    stolower(name);
    if (owner) {
	code = pr_SNameToId(owner,&oid);
	if (code) return code;
	if (oid == ANONYMOUSID) return PRNOENT;
    }
    flags |= PRGRP;
    if (*id) {
	code = ubik_Call(PR_INewEntry,pruclient,0,name,*id,oid);
	return code;
    }
    else {
	code = ubik_Call(PR_NewEntry,pruclient, 0, name,flags,oid,id);
	return code;
    }
}

pr_Delete(name)
char *name;
{
    register afs_int32 code;
    afs_int32 id;
    
    stolower(name);
    code = pr_SNameToId(name,&id);
    if (code) return code;
    if (id == ANONYMOUSID) return PRNOENT;
    code = ubik_Call(PR_Delete,pruclient,0,id);
    return code;
}

pr_DeleteByID(id)
afs_int32 id;
{
    register afs_int32 code;

    code = ubik_Call(PR_Delete,pruclient,0,id);
    return code;
}

pr_AddToGroup(user,group)
char *user;
char *group;
{
    register afs_int32 code;
    namelist lnames;
    idlist lids;

    lnames.namelist_len = 2;
    lnames.namelist_val = (prname *)malloc(2*PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0],user,PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[1],group,PR_MAXNAMELEN);
    lids.idlist_val = 0;
    lids.idlist_len = 0;
    code = pr_NameToId(&lnames,&lids);
    if (code) goto done;
    /* if here, still could be missing an entry */
    if (lids.idlist_val[0] == ANONYMOUSID || lids.idlist_val[1] == ANONYMOUSID) {
	code = PRNOENT;
	goto done;
    }
    code = ubik_Call(PR_AddToGroup, pruclient, 0, lids.idlist_val[0], lids.idlist_val[1]);
  done:
    if (lnames.namelist_val) free(lnames.namelist_val);
    if (lids.idlist_val) free(lids.idlist_val);
    return code;
}

pr_RemoveUserFromGroup(user,group)
char *user;
char *group;
{
    register afs_int32 code;
    namelist lnames;
    idlist lids;

    lnames.namelist_len = 2;
    lnames.namelist_val = (prname *)malloc(2*PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0],user,PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[1],group,PR_MAXNAMELEN);
    lids.idlist_val = 0;
    lids.idlist_len = 0;
    code = pr_NameToId(&lnames,&lids);
    if (code) goto done;

    if (lids.idlist_val[0] == ANONYMOUSID || lids.idlist_val[1] == ANONYMOUSID) {
        code = PRNOENT;
	goto done;
    }
    code = ubik_Call(PR_RemoveFromGroup, pruclient, 0, lids.idlist_val[0], lids.idlist_val[1]);
done:
    if (lnames.namelist_val) free(lnames.namelist_val);
    if (lids.idlist_val) free(lids.idlist_val);
    return code;

}

pr_NameToId(names, ids)
namelist *names;
idlist *ids;
{
    register afs_int32 code;
    register afs_int32 i;

    for (i=0;i<names->namelist_len;i++)
	stolower(names->namelist_val[i]);
    code = ubik_Call(PR_NameToID,pruclient,0,names,ids);
    return code;
}

pr_SNameToId(name,id)
char name[PR_MAXNAMELEN];
afs_int32 *id;
{
    namelist lnames;
    idlist lids;
    register afs_int32 code;

    lids.idlist_len = 0;
    lids.idlist_val = 0;
    lnames.namelist_len = 1;
    lnames.namelist_val = (prname *)malloc(PR_MAXNAMELEN);
    stolower(name);
    strncpy(lnames.namelist_val[0],name,PR_MAXNAMELEN);
    code = ubik_Call(PR_NameToID,pruclient,0,&lnames,&lids);
    if (lids.idlist_val) {
	*id = *lids.idlist_val;
	free(lids.idlist_val);
    }
    if (lnames.namelist_val) free(lnames.namelist_val);
    return code;
}
    


pr_IdToName(ids,names)
idlist *ids;
namelist *names;
{
    register afs_int32 code;

    code = ubik_Call(PR_IDToName,pruclient,0,ids,names);
    return code;
}

pr_SIdToName(id,name)
afs_int32 id;
char name[PR_MAXNAMELEN];
{
    namelist lnames;
    idlist lids;
    register afs_int32 code;

    lids.idlist_len = 1;
    lids.idlist_val = (afs_int32 *)malloc(sizeof(afs_int32));
    *lids.idlist_val = id;
    lnames.namelist_len = 0;
    lnames.namelist_val = 0;
    code = ubik_Call(PR_IDToName,pruclient,0,&lids,&lnames);
    if (lnames.namelist_val) {
	strncpy(name,lnames.namelist_val[0],PR_MAXNAMELEN);
	free(lnames.namelist_val);
    }
    if (lids.idlist_val) free(lids.idlist_val);
    return code;
}
    
	

pr_GetCPS(id, CPS)
afs_int32 id;
prlist *CPS;
{
    register afs_int32 code;
    afs_int32 over;

    over = 0;
    code = ubik_Call(PR_GetCPS,pruclient,0,id,CPS,&over);
    if (code != PRSUCCESS) return code;
    if (over) {
	/* do something about this, probably make a new call */
	/* don't forget there's a hard limit in the interface */
	fprintf (stderr, "membership list for id %d exceeds display limit\n", id);
    }
    return 0;
}


pr_GetCPS2(id, host, CPS)
    afs_int32 id;
    afs_int32 host;
    prlist *CPS;
{
    register afs_int32 code;
    afs_int32 over;

    over = 0;
    code = ubik_Call(PR_GetCPS2,pruclient,0,id,host,CPS,&over);
    if (code != PRSUCCESS) return code;
    if (over) {
	/* do something about this, probably make a new call */
	/* don't forget there's a hard limit in the interface */
	fprintf (stderr, "membership list for id %d exceeds display limit\n", id);
    }
    return 0;
}

pr_GetHostCPS(host, CPS)
    afs_int32 host;
    prlist *CPS;
{
    register afs_int32 code;
    afs_int32 over;

    over = 0;
    code = ubik_Call(PR_GetHostCPS,pruclient,0,host,CPS,&over);
    if (code != PRSUCCESS) return code;
    if (over) {
	/* do something about this, probably make a new call */
	/* don't forget there's a hard limit in the interface */
	fprintf (stderr, "membership list for host id %d exceeds display limit\n", host);
    }
    return 0;
}


pr_ListMembers(group,lnames)
char *group;
namelist *lnames;
{
    register afs_int32 code;
    afs_int32 gid;

    code = pr_SNameToId(group,&gid);
    if (code) return code;
    if (gid == ANONYMOUSID) return PRNOENT;
    code = pr_IDListMembers(gid, lnames);
    return code;
}

pr_ListOwned (oid,lnames,moreP)
  afs_int32 oid;
  namelist *lnames;
  afs_int32 *moreP;
{
    register afs_int32 code;
    prlist alist;
    idlist *lids;

    alist.prlist_len = 0;
    alist.prlist_val = 0;
    code = ubik_Call(PR_ListOwned,pruclient,0,oid,&alist,moreP);
    if (code) return code;
    if (*moreP == 1) {
      /* Remain backwards compatible when moreP was a T/F bit */
      fprintf (stderr, "membership list for id %d exceeds display limit\n",
	       oid);
      *moreP = 0;
    }
    lids = (idlist *)&alist;
    code = pr_IdToName(lids,lnames);
    if (code) return code;
    if (alist.prlist_val) free(alist.prlist_val);
    return PRSUCCESS;
}

pr_IDListMembers(gid,lnames)
afs_int32 gid;
namelist *lnames;
{
    register afs_int32 code;
    prlist alist;
    idlist *lids;
    afs_int32 over;

    alist.prlist_len = 0;
    alist.prlist_val = 0;
    code = ubik_Call(PR_ListElements,pruclient,0,gid,&alist,&over);
    if (code) return code;
    if (over) {
       fprintf (stderr, "membership list for id %d exceeds display limit\n", gid);
    }
    lids = (idlist *)&alist;
    code = pr_IdToName(lids,lnames);
    if (code) return code;
    if (alist.prlist_val) free(alist.prlist_val);
    return PRSUCCESS;
}


pr_ListEntry(id, aentry)
  afs_int32 id;
  struct prcheckentry *aentry;
{
    register afs_int32 code;

    code = ubik_Call (PR_ListEntry, pruclient, 0, id, aentry);
    return code;
}

afs_int32 pr_ListEntries(flag, startindex, nentries, entries, nextstartindex)
  afs_int32                startindex;
  afs_int32                *nentries;
  struct prlistentries **entries;
  afs_int32                *nextstartindex;
{
  afs_int32     code;
  prentries bulkentries;

  *nentries = 0;
  *entries  = (struct prlistentries *)0;
  *nextstartindex = -1;
  bulkentries.prentries_val = 0;
  bulkentries.prentries_len = 0;

  code = ubik_Call(PR_ListEntries, pruclient, 0,
		   flag, startindex, &bulkentries, nextstartindex);
  *nentries = bulkentries.prentries_len;
  *entries  = bulkentries.prentries_val;
  return code;
}

pr_CheckEntryByName(name,id,owner,creator)
char *name;
afs_int32 *id;
char *owner;
char *creator;
{
    /* struct prcheckentry returns other things, which aren't useful to show at this time. */
    register afs_int32 code;
    struct prcheckentry aentry;

    code = pr_SNameToId(name,id);
    if (code) return code;
    if (*id == ANONYMOUSID) return PRNOENT;
    code = ubik_Call(PR_ListEntry,pruclient,0,*id,&aentry);
    if (code) return code;
    /* this should be done in one RPC, but I'm lazy. */
    code = pr_SIdToName(aentry.owner,owner);
    if (code) return code;
    code = pr_SIdToName(aentry.creator,creator);
    if (code) return code;
    return PRSUCCESS;
}

pr_CheckEntryById(name,id,owner,creator)
char *name;
afs_int32 id;
char *owner;
char *creator;
{
    /* struct prcheckentry returns other things, which aren't useful to show at this time. */
    register afs_int32 code;
    struct prcheckentry aentry;

    code = pr_SIdToName(id,name);
    if (code) return code;
    if (id == ANONYMOUSID) return PRNOENT;
    code = ubik_Call(PR_ListEntry,pruclient,0,id,&aentry);
    if (code) return code;
    /* this should be done in one RPC, but I'm lazy. */
    code = pr_SIdToName(aentry.owner,owner);
    if (code) return code;
    code = pr_SIdToName(aentry.creator,creator);
    if (code) return code;
    return PRSUCCESS;
}

pr_ChangeEntry(oldname,newname,newid,newowner)
char *oldname;
char *newname;
afs_int32 *newid;
char *newowner;
{
    register afs_int32 code;
    afs_int32 id;
    afs_int32 oid =0;

    code = pr_SNameToId(oldname,&id);
    if (code) return code;
    if (id == ANONYMOUSID) return PRNOENT;
    if (newowner && *newowner) {
	code = pr_SNameToId(newowner,&oid);
	if (code) return code;
	if (oid == ANONYMOUSID) return PRNOENT;
    }
    code = ubik_Call(PR_ChangeEntry,pruclient, 0,id,newname,oid,newid);
    return code;
}

pr_IsAMemberOf(uname,gname,flag)
char *uname;
char *gname;
afs_int32 *flag;
{
    register afs_int32 code;
    namelist lnames;
    idlist lids;

    stolower(uname);
    stolower(gname);
    lnames.namelist_len = 2;
    lnames.namelist_val = (prname *)malloc(2*PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0],uname,PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[1],gname,PR_MAXNAMELEN);
    lids.idlist_val= 0;
    lids.idlist_len = 0;
    code = pr_NameToId(&lnames,&lids);
    if (code) {
	if (lnames.namelist_val) free(lnames.namelist_val);
	if (lids.idlist_val) free(lids.idlist_val);
	return code;
    }
    code = ubik_Call(PR_IsAMemberOf,pruclient,0,lids.idlist_val[0],lids.idlist_val[1],flag);
    if (lnames.namelist_val) free(lnames.namelist_val);
    if (lids.idlist_val) free(lids.idlist_val);
    return code;
}


pr_ListMaxUserId(mid)
afs_int32 *mid;
{
    register afs_int32 code;
    afs_int32 gid;
    code = ubik_Call(PR_ListMax,pruclient,0,mid,&gid);
    return code;
}

pr_SetMaxUserId(mid)
afs_int32 mid;
{
    register afs_int32 code;
    afs_int32 flag = 0;
    code = ubik_Call(PR_SetMax,pruclient,0,mid,flag);
    return code;
}

pr_ListMaxGroupId(mid)
afs_int32 *mid;
{
    register afs_int32 code;
    afs_int32 id;
    code = ubik_Call(PR_ListMax,pruclient,0,&id,mid);
    return code;
}

pr_SetMaxGroupId(mid)
afs_int32 mid;
{
    register afs_int32 code;
    afs_int32 flag = 0;

    flag |= PRGRP;
    code = ubik_Call(PR_SetMax,pruclient,0,mid,flag);
    return code;
}

afs_int32 pr_SetFieldsEntry (id, mask, flags, ngroups, nusers)
  afs_int32 id;
  afs_int32 mask;
  afs_int32 flags, ngroups, nusers;
{
    register afs_int32 code;

    code = ubik_Call(PR_SetFieldsEntry,pruclient,0,id,mask, flags, ngroups, nusers, 0,0);
    return code;
}


stolower(s)
char *s;
{
    while (*s) {
	if (isupper(*s)) *s = tolower(*s);
	s++;
    }
}

