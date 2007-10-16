/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* 
 *                      (5) Add functions to process supergroups:
 *                          ChangeIDEntry(), RemoveFromSGEntry(),
 *                          AddToSGEntry(), GetListSG2().
 *                      (6) Add code to existing functions to process
 *                          supergroups.
 *      2/1/98 jjm      Add mdw's changes for bit mapping for supergroups
 *
 *	09/26/02 kwc	Move depthsg definition here from ptserver.c since
 *			pt_util needs it defined also, and this file is
 *			common between the two programs.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/ptserver/ptutils.c,v 1.18.2.4 2007/08/11 23:50:02 jaltman Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <lock.h>
#include <ubik.h>
#include <rx/xdr.h>
#include <afs/com_err.h>
#include <afs/cellconfig.h>
#include "ptserver.h"
#include "pterror.h"
#include <stdlib.h>

/* Foreign cells are represented by the group system:authuser@cell*/
#define AUTHUSER_GROUP "system:authuser"

extern int restricted;
extern struct ubik_dbase *dbase;
extern struct afsconf_dir *prdir;
extern int pr_noAuth;
extern int IDCmp();

extern afs_int32 AddToEntry();
static char *whoami = "ptserver";

int prp_user_default = PRP_USER_DEFAULT;
int prp_group_default = PRP_GROUP_DEFAULT;

#if defined(SUPERGROUPS)

#include "map.h"

afs_int32 depthsg = 5;		/* Maximum iterations used during IsAMemberOF */
extern int IDCmp();
afs_int32 GetListSG2(struct ubik_trans *at, afs_int32 gid, prlist * alist,
		     afs_int32 * sizeP, afs_int32 depth);
afs_int32 allocNextId(struct ubik_trans *, struct prentry *);

struct map *sg_flagged;
struct map *sg_found;

#define NIL_MAP ((struct map *) 0)

/* pt_mywrite hooks into the logic that writes ubik records to disk
 *  at a very low level.  It invalidates mappings in sg_flagged/sg_found.
 *  By hoooking in at this level, we ensure that a ubik file reload
 *  invalidates our incore cache.
 *
 * We assert records, cheaders and everything else are 0 mod 64.
 *  So, we can always see the first 64 bytes of any record written.
 *  The stuff we're interested in (flags, id) are in the first 8 bytes.
 *  so we can always tell if we're writing a group record.
 */

int (*pt_save_dbase_write) ();

int
pt_mywrite(struct ubik_dbase *tdb, afs_int32 fno, char *bp, afs_int32 pos, afs_int32 count)
{
    afs_uint32 headersize = ntohl(cheader.headerSize);

    if (fno == 0 && pos + count > headersize) {
	afs_int32 p, l, c, o;
	char *cp;
	p = pos - headersize;
	cp = bp;
	l = count;
	if (p < 0) {
	    l += p;
	    p = 0;
	}
	while (l > 0) {
	    o = p % ENTRYSIZE;
	    c = ENTRYSIZE - o;
	    if (c > l)
		c = l;
#define xPT(p,x) ((((struct prentry *)(p))->flags & htonl(PRTYPE)) == htonl(x))
#if DEBUG_SG_MAP
	    if (o)
		fprintf(stderr, "Writing %d bytes of entry @ %#lx(+%d)\n", c,
			p - o, o);
	    else if (c == ENTRYSIZE)
		fprintf(stderr,
			"Writing %d bytes of entry @ %#lx (%d<%s>,%d)\n",
			c, p, ntohl(((struct prentry *)cp)->flags),
			xPT(cp,PRUSER) ? "user" : xPT(cp,PRFREE) ? "free" :
			xPT(cp,PRGRP) ? "group" : xPT(cp,PRCONT) ? "cont" :
			xPT(cp,PRCELL) ? "cell" : xPT(cp,PRFOREIGN) ? "foreign" :
			xPT(cp,PRINST) ? "sub/super instance" : "?",
			ntohl(((struct prentry *)cp)->id));
	    else if (c >= 8)
		fprintf(stderr,
			"Writing first %d bytes of entry @ %#lx (%d<%s>,%d)\n",
			c, p, ntohl(((struct prentry *)cp)->flags),
			xPT(cp,PRUSER) ? "user" : xPT(cp,PRFREE) ? "free" :
			xPT(cp,PRGRP) ? "group" : xPT(cp,PRCONT) ? "cont" :
			xPT(cp,PRCELL) ? "cell" : xPT(cp,PRFOREIGN) ? "foreign" :
			xPT(cp,PRINST) ? "sub/super instance" : "?",
			ntohl(((struct prentry *)cp)->id));
	    else
		fprintf(stderr, "Writing %d bytes of entry @ %#lx\n", c, p);
#endif
	    if (!o && c >= 8 && xPT(cp,PRGRP)) {
#if DEBUG_SG_MAP
		if (in_map(sg_found, -ntohl(((struct prentry *)cp)->id)))
		    fprintf(stderr, "Unfound: Removing group %d\n",
			    ntohl(((struct prentry *)cp)->id));
		if (in_map(sg_flagged, -ntohl(((struct prentry *)cp)->id)))
		    fprintf(stderr, "Unflag: Removing group %d\n",
			    ntohl(((struct prentry *)cp)->id));
#endif
		sg_found =
		    bic_map(sg_found,
			add_map(NIL_MAP, -ntohl(((struct prentry *)cp)->id)));
		sg_flagged =
		    bic_map(sg_flagged,
			add_map(NIL_MAP, -ntohl(((struct prentry *)cp)->id)));
	    }
	    cp += c;
	    p += c;
	    l -= c;
#undef xPT
	}
    }
    return (*pt_save_dbase_write) (tdb, fno, bp, pos, count);
}

/*
 * this function attaches pt_mywrite.  It's called once,
 *  just after ubik_ServerInit.
 */

void 
pt_hook_write()
{
    extern struct ubik_dbase *ubik_dbase;
    if (ubik_dbase->write != pt_mywrite) {
	pt_save_dbase_write = ubik_dbase->write;
	ubik_dbase->write = pt_mywrite;
    }
}

#endif /* SUPERGROUPS */

/* CorrectUserName - Check to make sure a user name is OK.  It must not include
 *   either a colon (or it would look like a group) or an atsign (or it would
 *   look like a foreign user).  The length is checked as well to make sure
 *   that the user name, an atsign, and the local cell name will fit in
 *   PR_MAXNAMELEN.  This is so this user can fit in another cells database as
 *   a foreign user with our cell name tacked on.  This is a predicate, so it
 *   return one if name is OK and zero if name is bogus. */

static int
CorrectUserName(char *name)
{
    extern int pr_realmNameLen;

    /* We accept foreign names, so we will deal with '@' later */
    if (strchr(name, ':') || strchr(name, '\n'))
	return 0;
    if (strlen(name) >= PR_MAXNAMELEN - pr_realmNameLen - 1)
	return 0;
    return 1;
}

/* CorrectGroupName - Like the above but handles more complicated cases caused
 * by including the ownership in the name.  The interface works by calculating
 * the correct name based on a given name and owner.  This allows easy use by
 * rename, which then compares the correct name with the requested new name. */

static afs_int32
CorrectGroupName(struct ubik_trans *ut, char aname[PR_MAXNAMELEN],	/* name for group */
		 afs_int32 cid,		/* caller id */
		 afs_int32 oid,		/* owner of group */
		 char cname[PR_MAXNAMELEN])	/* correct name for group */
{
    afs_int32 code;
    int admin;
    char *prefix;		/* ptr to group owner part */
    char *suffix;		/* ptr to group name part */
    char name[PR_MAXNAMELEN];	/* correct name for group */
    struct prentry tentry;

    if (strlen(aname) >= PR_MAXNAMELEN)
	return PRBADNAM;
    admin = pr_noAuth || IsAMemberOf(ut, cid, SYSADMINID);

    if (oid == 0)
	oid = cid;

    /* Determine the correct prefix for the name. */
    if (oid == SYSADMINID)
	prefix = "system";
    else {
	afs_int32 loc = FindByID(ut, oid);
	if (loc == 0) {
	    /* let admin create groups owned by non-existent ids (probably
	     * setting a group to own itself).  Check that they look like
	     * groups (with a colon) or otherwise are good user names. */
	    if (admin) {
		strcpy(cname, aname);
		goto done;
	    }
	    return PRNOENT;
	}
	code = pr_Read(ut, 0, loc, &tentry, sizeof(tentry));
	if (code)
	    return code;
	if (ntohl(tentry.flags) & PRGRP) {
	    if ((tentry.count == 0) && !admin)
		return PRGROUPEMPTY;
	    /* terminate prefix at colon if there is one */
	    if ((prefix = strchr(tentry.name, ':')))
		*prefix = 0;
	}
	prefix = tentry.name;
    }
    /* only sysadmin allow to use 'system:' prefix */
    if ((strcmp(prefix, "system") == 0) && !admin)
	return PRPERM;

    strcpy(name, aname);	/* in case aname & cname are same */
    suffix = strchr(name, ':');
    if (suffix == 0) {
	/* sysadmin can make groups w/o ':', but they must still look like
	 * legal user names. */
	if (!admin)
	    return PRBADNAM;
	strcpy(cname, name);
    } else {
	if (strlen(prefix) + strlen(suffix) >= PR_MAXNAMELEN)
	    return PRBADNAM;
	strcpy(cname, prefix);
	strcat(cname, suffix);
    }
  done:
    /* check for legal name with either group rules or user rules */
    if ((suffix = strchr(cname, ':'))) {
	/* check for confusing characters */
	if (strchr(cname, '\n') ||	/* restrict so recreate can work */
	    strchr(suffix + 1, ':'))	/* avoid multiple colons */
	    return PRBADNAM;
    } else {
	if (!CorrectUserName(cname))
	    return PRBADNAM;
    }
    return 0;
}

int
AccessOK(struct ubik_trans *ut, afs_int32 cid,		/* caller id */
	 struct prentry *tentry,	/* object being accessed */
	 int mem,			/* check membership in aid, if group */
	 int any)			/* if set return true */
{
    afs_int32 flags;
    afs_int32 oid;
    afs_int32 aid;

    if (pr_noAuth)
	return 1;
    if (cid == SYSADMINID)
	return 1;		/* special case fileserver */
    if (restricted && ((mem == PRP_ADD_MEM) || (mem == any == 0)))
	return 0;
    if (tentry) {
	flags = tentry->flags;
	oid = tentry->owner;
	aid = tentry->id;
    } else {
	flags = oid = aid = 0;
    }
    if (!(flags & PRACCESS)) {	/* provide default access */
	if (flags & PRGRP)
	    flags |= prp_group_default;
	else
	    flags |= prp_user_default;
    }

    if (flags & any)
	return 1;
    if (oid) {
	if ((cid == oid) || IsAMemberOf(ut, cid, oid))
	    return 1;
    }
    if (aid > 0) {		/* checking on a user */
	if (aid == cid)
	    return 1;
    } else if (aid < 0) {	/* checking on group */
	if ((flags & mem) && IsAMemberOf(ut, cid, aid))
	    return 1;
    }
    /* Allow members of SYSVIEWERID to get membership and status only */
    if (((mem == PRP_STATUS_MEM) || (mem == PRP_MEMBER_MEM)
	 || (any == PRP_OWNED_ANY)) && (IsAMemberOf(ut, cid, SYSVIEWERID)))
	return 1;
    if (IsAMemberOf(ut, cid, SYSADMINID))
	return 1;
    return 0;			/* no access */
}

afs_int32
CreateEntry(struct ubik_trans *at, char aname[PR_MAXNAMELEN], afs_int32 *aid, afs_int32 idflag, afs_int32 flag, afs_int32 oid, afs_int32 creator)
{
    /* get and init a new entry */
    afs_int32 code;
    afs_int32 newEntry;
    struct prentry tentry, tent;
    char *atsign;

    memset(&tentry, 0, sizeof(tentry));

    if ((oid == 0) || (oid == ANONYMOUSID))
	oid = creator;

    if (flag & PRGRP) {
	code = CorrectGroupName(at, aname, creator, oid, tentry.name);
	if (code)
	    return code;
	if (strcmp(aname, tentry.name) != 0)
	    return PRBADNAM;
    } else {			/* non-group must not have colon */
	if (!CorrectUserName(aname))
	    return PRBADNAM;
	strcpy(tentry.name, aname);
    }

    if (FindByName(at, aname, &tent))
	return PREXIST;

    newEntry = AllocBlock(at);
    if (!newEntry)
	return PRDBFAIL;
#ifdef PR_REMEMBER_TIMES
    tentry.createTime = time(0);
#endif

    if (flag & PRGRP) {
	tentry.flags = PRGRP;
	tentry.owner = oid;
    } else if (flag == 0) {
	tentry.flags = 0;
	tentry.owner = SYSADMINID;
    } else {
	return PRBADARG;
    }

    atsign = strchr(aname, '@');
    if (!atsign) {
	/* A normal user or group. Pick an id for it */
	if (idflag)
	    tentry.id = *aid;
	else {
	    code = AllocID(at, flag, &tentry.id);
	    if (code != PRSUCCESS)
		return code;
	}
    } else if (flag & PRGRP) {
	/* A foreign group. Its format must be AUTHUSER_GROUP@cellname
	 * Then pick an id for the group.
	 */
	int badFormat;

	*atsign = '\0';
	badFormat = strcmp(AUTHUSER_GROUP, aname);
	*atsign = '@';
	if (badFormat)
	    return PRBADNAM;

	if (idflag)
	    tentry.id = *aid;
	else {
	    code = AllocID(at, flag, &tentry.id);
	    if (code != PRSUCCESS)
		return code;
	}
    } else {
	/* A foreign user: <name>@<cell>. The foreign user is added to
	 * its representing group. It is 
	 */
	char *cellGroup;
	afs_int32 pos, n;
	struct prentry centry;

	/* To create the user <name>@<cell> the group AUTHUSER_GROUP@<cell>
	 * must exist.
	 */
	cellGroup =
	    (char *)malloc(strlen(AUTHUSER_GROUP) + strlen(atsign) + 1);
	strcpy(cellGroup, AUTHUSER_GROUP);
	strcat(cellGroup, atsign);
	pos = FindByName(at, cellGroup, &centry);
	free(cellGroup);
	if (!pos)
	    return PRBADNAM;
	code = pr_Read(at, 0, pos, &centry, sizeof(centry));
	if (code)
	    return code;

	/* cellid is the id of the group representing the cell */
	tentry.cellid = ntohl(centry.id);

	if (idflag) {
	    /* Check if id is good */
	    if (!inRange(&centry, *aid))
		return PRBADARG;
	    tentry.id = *aid;
	} else {
	    /* Allocate an ID special for this foreign user. It is based 
	     * on the representing group's id and nusers count.
	     */
	    tentry.id = allocNextId(at, &centry);
	    if (!tentry.id)
		return PRNOIDS;
	}

	/* The foreign user will be added to the representing foreign
	 * group. The group can hold up to 30 entries.
	 */
	if (!(ntohl(centry.flags) & PRQUOTA)) {
	    centry.flags = htonl(ntohl(centry.flags) | PRQUOTA);
	    centry.ngroups = htonl(30);
	}
	n = ntohl(centry.ngroups);
	if ((n <= 0) && !pr_noAuth)
	    return PRNOMORE;
	centry.ngroups = htonl(n - 1);

	/* write updated entry for group */
	code = pr_Write(at, 0, pos, &centry, sizeof(centry));

	/* Now add the new user entry to the database */
	tentry.creator = creator;
	*aid = tentry.id;
	code = pr_WriteEntry(at, 0, newEntry, &tentry);
	if (code)
	    return PRDBFAIL;
	code = AddToIDHash(at, *aid, newEntry);
	if (code != PRSUCCESS)
	    return code;
	code = AddToNameHash(at, aname, newEntry);
	if (code != PRSUCCESS)
	    return code;
	if (inc_header_word(at, foreigncount, 1))
	    return PRDBFAIL;

	/* Now add the entry to the authuser group for this cell.
	 * We will reread the entries for the user and the group
	 * instead of modifying them before writing them in the
	 * previous steps. Although not very efficient, much simpler
	 */
	pos = FindByID(at, tentry.cellid);
	if (!pos)
	    return PRBADNAM;
	code = pr_ReadEntry(at, 0, pos, &centry);
	if (code)
	    return code;
	code = AddToEntry(at, &centry, pos, *aid);
	if (code)
	    return code;
	/* and now the user entry */
	pos = FindByID(at, *aid);
	if (!pos)
	    return PRBADNAM;
	code = pr_ReadEntry(at, 0, pos, &tentry);
	if (code)
	    return code;
	code = AddToEntry(at, &tentry, pos, tentry.cellid);
	if (code)
	    return code;

	return PRSUCCESS;
    }

    /* Remember the largest group id or largest user id */
    if (flag & PRGRP) {
	/* group ids are negative */
	if (tentry.id < (afs_int32) ntohl(cheader.maxGroup)) {
	    code = set_header_word(at, maxGroup, htonl(tentry.id));
	    if (code)
		return PRDBFAIL;
	}
    } else {
	if (tentry.id > (afs_int32) ntohl(cheader.maxID)) {
	    code = set_header_word(at, maxID, htonl(tentry.id));
	    if (code)
		return PRDBFAIL;
	}
    }

    /* Charge the creator for this group */
    if (flag & PRGRP) {
	afs_int32 loc = FindByID(at, creator);
	struct prentry centry;
	int admin;

	if (loc) {		/* this should only fail during initialization */
	    code = pr_Read(at, 0, loc, &centry, sizeof(centry));
	    if (code)
		return code;

	    /* If quota is uninitialized, do it */
	    if (!(ntohl(centry.flags) & PRQUOTA)) {
		centry.flags = htonl(ntohl(centry.flags) | PRQUOTA);
		centry.ngroups = centry.nusers = htonl(20);
	    }

	    /* Admins don't get charged for creating a group.
	     * If in noAuth mode, you get changed for it but you 
	     * are still allowed to create as many groups as you want.
	     */
	    admin = ((creator == SYSADMINID)
		     || IsAMemberOf(at, creator, SYSADMINID));
	    if (!admin) {
		if (ntohl(centry.ngroups) <= 0) {
		    if (!pr_noAuth)
			return PRNOMORE;
		} else {
		    centry.ngroups = htonl(ntohl(centry.ngroups) - 1);
		}
	    }

	    code = pr_Write(at, 0, loc, &centry, sizeof(centry));
	    if (code)
		return code;
	}			/* if (loc) */
    } else {
	/* Initialize the quota for the user. Groups don't have their
	 * quota initialized.
	 */
	tentry.flags |= PRQUOTA;
	tentry.ngroups = tentry.nusers = 20;
    }

    tentry.creator = creator;
    *aid = tentry.id;
    code = pr_WriteEntry(at, 0, newEntry, &tentry);
    if (code)
	return PRDBFAIL;
    code = AddToIDHash(at, *aid, newEntry);
    if (code != PRSUCCESS)
	return code;
    code = AddToNameHash(at, aname, newEntry);
    if (code != PRSUCCESS)
	return code;
    if (tentry.flags & PRGRP) {
	code = AddToOwnerChain(at, tentry.id, oid);
	if (code)
	    return code;
    }
    if (tentry.flags & PRGRP) {
	if (inc_header_word(at, groupcount, 1))
	    return PRDBFAIL;
    } else if (tentry.flags & PRINST) {
	if (inc_header_word(at, instcount, 1))
	    return PRDBFAIL;
    } else {
	if (inc_header_word(at, usercount, 1))
	    return PRDBFAIL;
    }
    return PRSUCCESS;
}


/* RemoveFromEntry - remove aid from bid's entries list, freeing a continuation
 * entry if appropriate */

afs_int32
RemoveFromEntry(struct ubik_trans *at, afs_int32 aid, afs_int32 bid)
{
    afs_int32 code;
    struct prentry tentry;
    struct contentry centry;
    struct contentry hentry;
    afs_int32 temp;
    afs_int32 i, j;
    afs_int32 nptr;
    afs_int32 hloc;

    if (aid == bid)
	return PRINCONSISTENT;
    memset(&hentry, 0, sizeof(hentry));
    temp = FindByID(at, bid);
    if (temp == 0)
	return PRNOENT;
    code = pr_ReadEntry(at, 0, temp, &tentry);
    if (code != 0)
	return code;
#ifdef PR_REMEMBER_TIMES
    tentry.removeTime = time(0);
#endif
    for (i = 0; i < PRSIZE; i++) {
	if (tentry.entries[i] == aid) {
	    tentry.entries[i] = PRBADID;
	    tentry.count--;
	    code = pr_WriteEntry(at, 0, temp, &tentry);
	    if (code != 0)
		return code;
	    return PRSUCCESS;
	}
	if (tentry.entries[i] == 0)	/* found end of list */
	    return PRNOENT;
    }
    hloc = 0;
    nptr = tentry.next;
    while (nptr != 0) {
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return code;
	if ((centry.id != bid) || !(centry.flags & PRCONT))
	    return PRDBBAD;
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == aid) {
		centry.entries[i] = PRBADID;
		for (j = 0; j < COSIZE; j++)
		    if (centry.entries[j] != PRBADID
			&& centry.entries[j] != 0)
			break;
		if (j == COSIZE) {	/* can free this block */
		    if (hloc == 0) {
			tentry.next = centry.next;
		    } else {
			hentry.next = centry.next;
			code = pr_WriteCoEntry(at, 0, hloc, &hentry);
			if (code != 0)
			    return code;
		    }
		    code = FreeBlock(at, nptr);
		    if (code)
			return code;
		} else {	/* can't free it yet */
		    code = pr_WriteCoEntry(at, 0, nptr, &centry);
		    if (code != 0)
			return code;
		}
		tentry.count--;
		code = pr_WriteEntry(at, 0, temp, &tentry);
		if (code)
		    return PRDBFAIL;
		return 0;
	    }
	    if (centry.entries[i] == 0)
		return PRNOENT;
	}			/* for all coentry slots */
	hloc = nptr;
	nptr = centry.next;
	memcpy(&hentry, &centry, sizeof(centry));
    }				/* while there are coentries */
    return PRNOENT;
}

#if defined(SUPERGROUPS)
/* ChangeIDEntry - remove aid from bid's entries list, freeing a continuation
 * entry if appropriate */

afs_int32
ChangeIDEntry(register struct ubik_trans *at, register afs_int32 aid, afs_int32 newid, register afs_int32 bid)
{
    register afs_int32 code;
    struct prentry tentry;
    struct contentry centry;
    afs_int32 temp;
    afs_int32 i, j;
    afs_int32 nptr;

    if (aid == bid)
	return PRINCONSISTENT;
    temp = FindByID(at, bid);
    if (temp == 0) {
	return PRNOENT;
    }
    code = pr_ReadEntry(at, 0, temp, &tentry);
    if (code != 0)
	return code;
    for (i = 0; i < PRSIZE; i++) {
	if (tentry.entries[i] == aid) {
	    tentry.entries[i] = newid;
	    code = pr_WriteEntry(at, 0, temp, &tentry);
	    if (code != 0)
		return code;
	    return PRSUCCESS;
	}
	if (tentry.entries[i] == 0) {	/* found end of list */
	    return PRNOENT;
	}
    }

    nptr = tentry.next;
    while (nptr) {
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return code;
	if ((centry.id != bid) || !(centry.flags & PRCONT)) {
	    fprintf(stderr,
		    "ChangeIDEntry: bad database bid=%d centry.id=%d .flags=%d\n",
		    bid, centry.id, centry.flags);
	    return PRDBBAD;
	}
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == aid) {
		centry.entries[i] = newid;
		for (j = 0; j < COSIZE; j++)
		    if (centry.entries[j] != PRBADID
			&& centry.entries[j] != 0)
			break;
		code = pr_WriteCoEntry(at, 0, nptr, &centry);
		if (code != 0)
		    return code;
		return 0;
	    }
	    if (centry.entries[i] == 0) {
		return PRNOENT;
	    }
	}			/* for all coentry slots */
	nptr = centry.next;
    }				/* while there are coentries */
    return PRNOENT;
}

/*  #ifdef SUPERGROUPS */
/* RemoveFromSGEntry - remove aid from bid's supergroups list, freeing a
 * continuation entry if appropriate */

afs_int32
RemoveFromSGEntry(register struct ubik_trans *at, register afs_int32 aid, register afs_int32 bid)
{
    register afs_int32 code;
    struct prentry tentry;
    struct prentryg *tentryg;
    struct contentry centry;
    struct contentry hentry;
    afs_int32 temp;
    afs_int32 i, j;
    afs_int32 nptr;
    afs_int32 hloc;

    if (aid == bid)
	return PRINCONSISTENT;
    memset(&hentry, 0, sizeof(hentry));
    temp = FindByID(at, bid);
    if (temp == 0) {
	return PRNOENT;
    }
    code = pr_ReadEntry(at, 0, temp, &tentry);
    if (code != 0)
	return code;
#ifdef PR_REMEMBER_TIMES
    tentry.removeTime = time(NULL);
#endif
    tentryg = (struct prentryg *)&tentry;
    for (i = 0; i < SGSIZE; i++) {
	if (tentryg->supergroup[i] == aid) {
	    tentryg->supergroup[i] = PRBADID;
	    tentryg->countsg--;
	    code = pr_WriteEntry(at, 0, temp, &tentry);
	    if (code != 0)
		return code;
	    return PRSUCCESS;
	}
	if (tentryg->supergroup[i] == 0) {	/* found end of list */
	    return PRNOENT;
	}
    }
    hloc = 0;
    nptr = tentryg->nextsg;
    while (nptr) {
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return code;
	if ((centry.id != bid) || !(centry.flags & PRCONT)) {
	    fprintf(stderr,
		    "ChangeIDEntry: bad database bid=%d centry.id=%d .flags=%d\n",
		    bid, centry.id, centry.flags);
	    return PRDBBAD;
	}
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == aid) {
		centry.entries[i] = PRBADID;
		for (j = 0; j < COSIZE; j++)
		    if (centry.entries[j] != PRBADID
			&& centry.entries[j] != 0)
			break;
		if (j == COSIZE) {	/* can free this block */
		    if (hloc == 0) {
			tentryg->nextsg = centry.next;
		    } else {
			hentry.next = centry.next;
			code = pr_WriteCoEntry(at, 0, hloc, &hentry);
			if (code != 0)
			    return code;
		    }
		    code = FreeBlock(at, nptr);
		    if (code)
			return code;
		} else {	/* can't free it yet */
		    code = pr_WriteCoEntry(at, 0, nptr, &centry);
		    if (code != 0)
			return code;
		}
		tentryg->countsg--;
		code = pr_WriteEntry(at, 0, temp, &tentry);
		if (code)
		    return PRDBFAIL;
		return 0;
	    }
	    if (centry.entries[i] == 0) {
		return PRNOENT;
	    }
	}			/* for all coentry slots */
	hloc = nptr;
	nptr = centry.next;
	memcpy((char *)&centry, (char *)&hentry, sizeof(centry));
    }				/* while there are coentries */
    return PRNOENT;
}

#endif /* SUPERGROUPS */

/* DeleteEntry - delete the entry in tentry at loc, removing it from all
 * groups, putting groups owned by it on orphan chain, and freeing the space */

afs_int32
DeleteEntry(struct ubik_trans *at, struct prentry *tentry, afs_int32 loc)
{
    afs_int32 code;
    struct contentry centry;
    afs_int32 i;
    afs_int32 nptr;

    if (strchr(tentry->name, '@')) {
	if (tentry->flags & PRGRP) {
	    /* If there are still foreign user accounts from that cell
	     * don't delete the group */
	    if (tentry->count)
		return PRBADARG;
	} else {
	    /* adjust quota */

	    afs_int32 loc = FindByID(at, tentry->cellid);
	    struct prentry centry;
	    if (loc) {
		code = pr_Read(at, 0, loc, &centry, sizeof(centry));
		if (code)
		    return code;
		if (ntohl(centry.flags) & PRQUOTA) {
		    centry.ngroups = htonl(ntohl(centry.ngroups) + 1);
		}
		code = pr_Write(at, 0, loc, &centry, sizeof(centry));
		if (code)
		    return code;
	    }
	}
    }
    /* First remove the entire membership list */
    for (i = 0; i < PRSIZE; i++) {
	if (tentry->entries[i] == PRBADID)
	    continue;
	if (tentry->entries[i] == 0)
	    break;
#if defined(SUPERGROUPS)
	if ((tentry->flags & PRGRP) && tentry->entries[i] < 0)	/* Supergroup */
	    code = RemoveFromSGEntry(at, tentry->id, tentry->entries[i]);
	else
#endif
	    code = RemoveFromEntry(at, tentry->id, tentry->entries[i]);
	if (code)
	    return code;
    }
#if defined(SUPERGROUPS)
    {
	struct prentryg *tentryg = (struct prentryg *)tentry;

	/* Then remove the entire supergroup list */
	for (i = 0; i < SGSIZE; i++) {
	    if (tentryg->supergroup[i] == PRBADID)
		continue;
	    if (tentryg->supergroup[i] == 0)
		break;
	    code = RemoveFromEntry(at, tentry->id, tentryg->supergroup[i]);
	    if (code)
		return code;
	}
    }
#endif /* SUPERGROUPS */
    nptr = tentry->next;
    while (nptr != (afs_int32) NULL) {
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return PRDBFAIL;
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == PRBADID)
		continue;
	    if (centry.entries[i] == 0)
		break;
	    code = RemoveFromEntry(at, tentry->id, centry.entries[i]);
	    if (code)
		return code;
	}
	code = FreeBlock(at, nptr);	/* free continuation block */
	if (code)
	    return code;
	nptr = centry.next;
    }

    /* Remove us from other's owned chain.  Note that this will zero our owned
     * field (on disk) so this step must follow the above step in case we are
     * on our own owned list. */
    if (tentry->flags & PRGRP) {
	if (tentry->owner) {
	    code = RemoveFromOwnerChain(at, tentry->id, tentry->owner);
	    if (code)
		return code;
	} else {
	    code = RemoveFromOrphan(at, tentry->id);
	    if (code)
		return code;
	}
    }

    code = RemoveFromIDHash(at, tentry->id, &loc);
    if (code != PRSUCCESS)
	return code;
    code = RemoveFromNameHash(at, tentry->name, &loc);
    if (code != PRSUCCESS)
	return code;

    if (tentry->flags & PRGRP) {
	afs_int32 loc = FindByID(at, tentry->creator);
	struct prentry centry;
	int admin;

	if (loc) {
	    code = pr_Read(at, 0, loc, &centry, sizeof(centry));
	    if (code)
		return code;
	    admin = ((tentry->creator == SYSADMINID)
		     || IsAMemberOf(at, tentry->creator, SYSADMINID));
	    if (ntohl(centry.flags) & PRQUOTA) {
		if (!(admin && (ntohl(centry.ngroups) >= 20))) {
		    centry.ngroups = htonl(ntohl(centry.ngroups) + 1);
		}
	    }
	    code = pr_Write(at, 0, loc, &centry, sizeof(centry));
	    if (code)
		return code;
	}
    }

    if (tentry->flags & PRGRP) {
	if (inc_header_word(at, groupcount, -1))
	    return PRDBFAIL;
    } else if (tentry->flags & PRINST) {
	if (inc_header_word(at, instcount, -1))
	    return PRDBFAIL;
    } else {
	if (strchr(tentry->name, '@')) {
	    if (inc_header_word(at, foreigncount, -1))
		return PRDBFAIL;
	} else {
	    if (inc_header_word(at, usercount, -1))
		return PRDBFAIL;
	}
    }
    code = FreeBlock(at, loc);
    return code;
}

/* AddToEntry - add aid to entry's entries list, alloc'ing a continuation block
 * if needed.
 *
 * Note the entry is written out by this routine. */

afs_int32
AddToEntry(struct ubik_trans *tt, struct prentry *entry, afs_int32 loc, afs_int32 aid)
{
    afs_int32 code;
    afs_int32 i;
    struct contentry nentry;
    struct contentry aentry;
    afs_int32 nptr;
    afs_int32 last;		/* addr of last cont. block */
    afs_int32 first = 0;
    afs_int32 cloc = 0;
    afs_int32 slot = -1;

    if (entry->id == aid)
	return PRINCONSISTENT;
#ifdef PR_REMEMBER_TIMES
    entry->addTime = time(0);
#endif
    for (i = 0; i < PRSIZE; i++) {
	if (entry->entries[i] == aid)
	    return PRIDEXIST;
	if (entry->entries[i] == PRBADID) {	/* remember this spot */
	    first = 1;
	    slot = i;
	} else if (entry->entries[i] == 0) {	/* end of the line */
	    if (slot == -1) {
		first = 1;
		slot = i;
	    }
	    break;
	}
    }
    last = 0;
    nptr = entry->next;
    while (nptr != (afs_int32) NULL) {
	code = pr_ReadCoEntry(tt, 0, nptr, &nentry);
	if (code != 0)
	    return code;
	last = nptr;
	if (!(nentry.flags & PRCONT))
	    return PRDBFAIL;
	for (i = 0; i < COSIZE; i++) {
	    if (nentry.entries[i] == aid)
		return PRIDEXIST;
	    if (nentry.entries[i] == PRBADID) {
		if (slot == -1) {
		    slot = i;
		    cloc = nptr;
		}
	    } else if (nentry.entries[i] == 0) {
		if (slot == -1) {
		    slot = i;
		    cloc = nptr;
		}
		break;
	    }
	}
	nptr = nentry.next;
    }
    if (slot != -1) {		/* we found a place */
	entry->count++;
	if (first) {		/* place is in first block */
	    entry->entries[slot] = aid;
	    code = pr_WriteEntry(tt, 0, loc, entry);
	    if (code != 0)
		return code;
	    return PRSUCCESS;
	}
	code = pr_WriteEntry(tt, 0, loc, entry);
	if (code)
	    return code;
	code = pr_ReadCoEntry(tt, 0, cloc, &aentry);
	if (code != 0)
	    return code;
	aentry.entries[slot] = aid;
	code = pr_WriteCoEntry(tt, 0, cloc, &aentry);
	if (code != 0)
	    return code;
	return PRSUCCESS;
    }
    /* have to allocate a continuation block if we got here */
    nptr = AllocBlock(tt);
    if (last) {
	/* then we should tack new block after last block in cont. chain */
	nentry.next = nptr;
	code = pr_WriteCoEntry(tt, 0, last, &nentry);
	if (code != 0)
	    return code;
    } else {
	entry->next = nptr;
    }
    memset(&aentry, 0, sizeof(aentry));
    aentry.flags |= PRCONT;
    aentry.id = entry->id;
    aentry.next = 0;
    aentry.entries[0] = aid;
    code = pr_WriteCoEntry(tt, 0, nptr, &aentry);
    if (code != 0)
	return code;
    /* don't forget to update count, here! */
    entry->count++;
    code = pr_WriteEntry(tt, 0, loc, entry);
    return code;

}

#if defined(SUPERGROUPS)

/* AddToSGEntry - add aid to entry's supergroup list, alloc'ing a
 * continuation block if needed.
 *
 * Note the entry is written out by this routine. */

afs_int32
AddToSGEntry(struct ubik_trans *tt, struct prentry *entry, afs_int32 loc, afs_int32 aid)
{
    register afs_int32 code;
    afs_int32 i;
    struct contentry nentry;
    struct contentry aentry;
    struct prentryg *entryg;
    afs_int32 nptr;
    afs_int32 last;		/* addr of last cont. block */
    afs_int32 first = 0;
    afs_int32 cloc;
    afs_int32 slot = -1;

    if (entry->id == aid)
	return PRINCONSISTENT;
#ifdef PR_REMEMBER_TIMES
    entry->addTime = time(NULL);
#endif
    entryg = (struct prentryg *)entry;
    for (i = 0; i < SGSIZE; i++) {
	if (entryg->supergroup[i] == aid)
	    return PRIDEXIST;
	if (entryg->supergroup[i] == PRBADID) {	/* remember this spot */
	    first = 1;
	    slot = i;
	} else if (entryg->supergroup[i] == 0) {	/* end of the line */
	    if (slot == -1) {
		first = 1;
		slot = i;
	    }
	    break;
	}
    }
    last = 0;
    nptr = entryg->nextsg;
    while (nptr) {
	code = pr_ReadCoEntry(tt, 0, nptr, &nentry);
	if (code != 0)
	    return code;
	last = nptr;
	if (!(nentry.flags & PRCONT))
	    return PRDBFAIL;
	for (i = 0; i < COSIZE; i++) {
	    if (nentry.entries[i] == aid)
		return PRIDEXIST;
	    if (nentry.entries[i] == PRBADID) {
		if (slot == -1) {
		    slot = i;
		    cloc = nptr;
		}
	    } else if (nentry.entries[i] == 0) {
		if (slot == -1) {
		    slot = i;
		    cloc = nptr;
		}
		break;
	    }
	}
	nptr = nentry.next;
    }
    if (slot != -1) {		/* we found a place */
	entryg->countsg++;
	if (first) {		/* place is in first block */
	    entryg->supergroup[slot] = aid;
	    code = pr_WriteEntry(tt, 0, loc, entry);
	    if (code != 0)
		return code;
	    return PRSUCCESS;
	}
	code = pr_WriteEntry(tt, 0, loc, entry);
	if (code)
	    return code;
	code = pr_ReadCoEntry(tt, 0, cloc, &aentry);
	if (code != 0)
	    return code;
	aentry.entries[slot] = aid;
	code = pr_WriteCoEntry(tt, 0, cloc, &aentry);
	if (code != 0)
	    return code;
	return PRSUCCESS;
    }
    /* have to allocate a continuation block if we got here */
    nptr = AllocBlock(tt);
    if (last) {
	/* then we should tack new block after last block in cont. chain */
	nentry.next = nptr;
	code = pr_WriteCoEntry(tt, 0, last, &nentry);
	if (code != 0)
	    return code;
    } else {
	entryg->nextsg = nptr;
    }
    memset(&aentry, 0, sizeof(aentry));
    aentry.flags |= PRCONT;
    aentry.id = entry->id;
    aentry.next = 0;
    aentry.entries[0] = aid;
    code = pr_WriteCoEntry(tt, 0, nptr, &aentry);
    if (code != 0)
	return code;
    /* don't forget to update count, here! */
    entryg->countsg++;
    code = pr_WriteEntry(tt, 0, loc, entry);
    return code;

}
#endif /* SUPERGROUPS */

afs_int32
AddToPRList(prlist *alist, int *sizeP, afs_int32 id)
{
    char *tmp;
    int count;

    if (alist->prlist_len >= *sizeP) {
	count = alist->prlist_len + 100;
	if (alist->prlist_val) {
	    tmp =
		(char *)realloc(alist->prlist_val, count * sizeof(afs_int32));
	} else {
	    tmp = (char *)malloc(count * sizeof(afs_int32));
	}
	if (!tmp)
	    return (PRNOMEM);
	alist->prlist_val = (afs_int32 *) tmp;
	*sizeP = count;
    }
    alist->prlist_val[alist->prlist_len++] = id;
    return 0;
}

afs_int32
GetList(struct ubik_trans *at, struct prentry *tentry, prlist *alist, afs_int32 add)
{
    afs_int32 code;
    afs_int32 i;
    struct contentry centry;
    afs_int32 nptr;
    int size;
    int count = 0;

    size = 0;
    alist->prlist_val = 0;
    alist->prlist_len = 0;

    for (i = 0; i < PRSIZE; i++) {
	if (tentry->entries[i] == PRBADID)
	    continue;
	if (tentry->entries[i] == 0)
	    break;
	code = AddToPRList(alist, &size, tentry->entries[i]);
	if (code)
	    return code;
#if defined(SUPERGROUPS)
	if (!add)
	    continue;
	code = GetListSG2(at, tentry->entries[i], alist, &size, depthsg);
	if (code)
	    return code;
#endif
    }

    for (nptr = tentry->next; nptr != 0; nptr = centry.next) {
	/* look through cont entries */
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return code;
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == PRBADID)
		continue;
	    if (centry.entries[i] == 0)
		break;
	    code = AddToPRList(alist, &size, centry.entries[i]);
	    if (code)
		return code;
#if defined(SUPERGROUPS)
	    if (!add)
		continue;
	    code = GetListSG2(at, centry.entries[i], alist, &size, depthsg);
	    if (code)
		return code;
#endif
	}
	if (count++ > 50)
	    IOMGR_Poll(), count = 0;
    }

    if (add) {			/* this is for a CPS, so tack on appropriate stuff */
	if (tentry->id != ANONYMOUSID && tentry->id != ANYUSERID) {
	    if ((code = AddToPRList(alist, &size, ANYUSERID))
		|| (code = AddAuthGroup(tentry, alist, &size))
		|| (code = AddToPRList(alist, &size, tentry->id)))
		return code;
	} else {
	    if ((code = AddToPRList(alist, &size, ANYUSERID))
		|| (code = AddToPRList(alist, &size, tentry->id)))
		return code;
	}
    }
    if (alist->prlist_len > 100)
	IOMGR_Poll();
    qsort(alist->prlist_val, alist->prlist_len, sizeof(afs_int32), IDCmp);
    return PRSUCCESS;
}


afs_int32
GetList2(struct ubik_trans *at, struct prentry *tentry, struct prentry *tentry2, prlist *alist, afs_int32 add)
{
    afs_int32 code = 0;
    afs_int32 i;
    struct contentry centry;
    afs_int32 nptr;
    afs_int32 size;
    int count = 0;

    size = 0;
    alist->prlist_val = 0;
    alist->prlist_len = 0;
    for (i = 0; i < PRSIZE; i++) {
	if (tentry->entries[i] == PRBADID)
	    continue;
	if (tentry->entries[i] == 0)
	    break;
	code = AddToPRList(alist, &size, tentry->entries[i]);
	if (code)
	    return code;
#if defined(SUPERGROUPS)
	if (!add)
	    continue;
	code = GetListSG2(at, tentry->entries[i], alist, &size, depthsg);
	if (code)
	    return code;
#endif
    }

    nptr = tentry->next;
    while (nptr != (afs_uint32) NULL) {
	/* look through cont entries */
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return code;
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == PRBADID)
		continue;
	    if (centry.entries[i] == 0)
		break;
	    code = AddToPRList(alist, &size, centry.entries[i]);
	    if (code)
		return code;
#if defined(SUPERGROUPS)
	    if (!add)
		continue;
	    code = GetListSG2(at, centry.entries[i], alist, &size, depthsg);
	    if (code)
		return code;
#endif
	}
	nptr = centry.next;
	if (count++ > 50)
	    IOMGR_Poll(), count = 0;
    }

    for (i = 0; i < PRSIZE; i++) {
	if (tentry2->entries[i] == PRBADID)
	    continue;
	if (tentry2->entries[i] == 0)
	    break;
	code = AddToPRList(alist, &size, tentry2->entries[i]);
	if (code)
	    break;
    }

    if (!code) {
	nptr = tentry2->next;
	while (nptr != (afs_uint32) NULL) {
	    /* look through cont entries */
	    code = pr_ReadCoEntry(at, 0, nptr, &centry);
	    if (code != 0)
		break;
	    for (i = 0; i < COSIZE; i++) {
		if (centry.entries[i] == PRBADID)
		    continue;
		if (centry.entries[i] == 0)
		    break;
		code = AddToPRList(alist, &size, centry.entries[i]);
		if (code)
		    break;
	    }
	    nptr = centry.next;
	    if (count++ > 50)
		IOMGR_Poll(), count = 0;
	}
    }
    if (add) {			/* this is for a CPS, so tack on appropriate stuff */
	if (tentry->id != ANONYMOUSID && tentry->id != ANYUSERID) {
	    if ((code = AddToPRList(alist, &size, ANYUSERID))
		|| (code = AddToPRList(alist, &size, AUTHUSERID))
		|| (code = AddToPRList(alist, &size, tentry->id)))
		return code;
	} else {
	    if ((code = AddToPRList(alist, &size, ANYUSERID))
		|| (code = AddToPRList(alist, &size, tentry->id)))
		return code;
	}
    }
    if (alist->prlist_len > 100)
	IOMGR_Poll();
    qsort(alist->prlist_val, alist->prlist_len, sizeof(afs_int32), IDCmp);
    return PRSUCCESS;
}

#if defined(SUPERGROUPS)

afs_int32
GetListSG2(struct ubik_trans *at, afs_int32 gid, prlist *alist, afs_int32 *sizeP, afs_int32 depth)
{
    register afs_int32 code;
    struct prentry tentry;
    struct prentryg *tentryg = (struct prentryg *)&tentry;
    afs_int32 i;
    struct contentry centry;
    afs_int32 nptr;
    int count = 0;
    afs_int32 temp;
    int didsomething;
#if DEBUG_SG_MAP
    int predictfound, predictflagged;
#endif

#if DEBUG_SG_MAP
    predictfound = 0;
    if (!in_map(sg_flagged, -gid)) {
	predictflagged = 0;
	fprintf(stderr, "GetListSG2: I have not yet searched for gid=%d\n",
		gid);
    } else if (predictflagged = 1, in_map(sg_found, -gid)) {
	predictfound = 1;
	fprintf(stderr,
		"GetListSG2: I have already searched for gid=%d, and predict success.\n",
		gid);
    }
#endif

    if (in_map(sg_flagged, -gid) && !in_map(sg_found, -gid)) {
#if DEBUG_SG_MAP
	fprintf(stderr,
		"GetListSG2: I have already searched for gid=%d, and predict failure.\n",
		gid);
#else
	return 0;
#endif
    }

    if (depth < 1)
	return 0;
    temp = FindByID(at, gid);
    if (!temp) {
	code = PRNOENT;
	return code;
    }
    code = pr_ReadEntry(at, 0, temp, &tentry);
    if (code)
	return code;
#if DEBUG_SG_MAP
    fprintf(stderr, "GetListSG2: lookup for gid=%d [\n", gid);
#endif
    didsomething = 0;

    for (i = 0; i < SGSIZE; i++) {
	if (tentryg->supergroup[i] == PRBADID)
	    continue;
	if (tentryg->supergroup[i] == 0)
	    break;
	didsomething = 1;
#if DEBUG_SG_MAP
	fprintf(stderr, "via gid=%d, added %d\n", gid,
		e.tentryg.supergroup[i]);
#endif
	code = AddToPRList(alist, sizeP, tentryg->supergroup[i]);
	if (code)
	    return code;
	code =
	    GetListSG2(at, tentryg->supergroup[i], alist, sizeP, depth - 1);
	if (code)
	    return code;
    }

    nptr = tentryg->nextsg;
    while (nptr) {
	didsomething = 1;
	/* look through cont entries */
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return code;
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == PRBADID)
		continue;
	    if (centry.entries[i] == 0)
		break;
#if DEBUG_SG_MAP
	    fprintf(stderr, "via gid=%d, added %d\n", gid,
		    e.centry.entries[i]);
#endif
	    code = AddToPRList(alist, sizeP, centry.entries[i]);
	    if (code)
		return code;
	    code = GetListSG2(at, centry.entries[i], alist, sizeP, depth - 1);
	    if (code)
		return code;
	}
	nptr = centry.next;
	if (count++ > 50)
	    IOMGR_Poll(), count = 0;
    }
#if DEBUG_SG_MAP
    fprintf(stderr, "] for gid %d, done [flag=%s]\n", gid,
	    didsomething ? "TRUE" : "FALSE");
    if (predictflagged && didsomething != predictfound)
	fprintf(stderr, "**** for gid=%d, didsomething=%d predictfound=%d\n",
		didsomething, predictfound);
#endif
    if (didsomething)
	sg_found = add_map(sg_found, -gid);
    else
	sg_found = bic_map(sg_found, add_map(NIL_MAP, -gid));
    sg_flagged = add_map(sg_flagged, -gid);
    return 0;
}

afs_int32
GetSGList(struct ubik_trans *at, struct prentry *tentry, prlist *alist)
{
    register afs_int32 code;
    afs_int32 i;
    struct contentry centry;
    struct prentryg *tentryg;
    afs_int32 nptr;
    int size;
    int count = 0;

    size = 0;
    alist->prlist_val = 0;
    alist->prlist_len = 0;

    tentryg = (struct prentryg *)tentry;
    for (i = 0; i < SGSIZE; i++) {
	if (tentryg->supergroup[i] == PRBADID)
	    continue;
	if (tentryg->supergroup[i] == 0)
	    break;
	code = AddToPRList(alist, &size, tentryg->supergroup[i]);
	if (code)
	    return code;
    }

    nptr = tentryg->nextsg;
    while (nptr) {
	/* look through cont entries */
	code = pr_ReadCoEntry(at, 0, nptr, &centry);
	if (code != 0)
	    return code;
	for (i = 0; i < COSIZE; i++) {
	    if (centry.entries[i] == PRBADID)
		continue;
	    if (centry.entries[i] == 0)
		break;
	    code = AddToPRList(alist, &size, centry.entries[i]);
	    if (code)
		return code;
	}
	nptr = centry.next;
	if (count++ > 50)
	    IOMGR_Poll(), count = 0;
    }

    if (alist->prlist_len > 100)
	IOMGR_Poll();
    qsort((char *)alist->prlist_val, (int)alist->prlist_len,
	  sizeof(afs_int32), IDCmp);
    return PRSUCCESS;
}
#endif /* SUPERGROUPS */

afs_int32
GetOwnedChain(struct ubik_trans *ut, afs_int32 *next, prlist *alist)
{
    afs_int32 code;
    struct prentry tentry;
    int size;
    int count = 0;

    size = 0;
    alist->prlist_val = 0;
    alist->prlist_len = 0;

    for (; *next; *next = ntohl(tentry.nextOwned)) {
	code = pr_Read(ut, 0, *next, &tentry, sizeof(tentry));
	if (code)
	    return code;
	code = AddToPRList(alist, &size, ntohl(tentry.id));
	if (alist->prlist_len >= PR_MAXGROUPS) {
	    return PRTOOMANY;
	}
	if (code)
	    return code;
	if (count++ > 50)
	    IOMGR_Poll(), count = 0;
    }
    if (alist->prlist_len > 100)
	IOMGR_Poll();
    qsort(alist->prlist_val, alist->prlist_len, sizeof(afs_int32), IDCmp);
    return PRSUCCESS;
}

afs_int32
GetMax(struct ubik_trans *at, afs_int32 *uid, afs_int32 *gid)
{
    *uid = ntohl(cheader.maxID);
    *gid = ntohl(cheader.maxGroup);
    return PRSUCCESS;
}

afs_int32
SetMax(struct ubik_trans *at, afs_int32 id, afs_int32 flag)
{
    afs_int32 code;
    if (flag & PRGRP) {
	cheader.maxGroup = htonl(id);
	code =
	    pr_Write(at, 0, 16, (char *)&cheader.maxGroup,
		     sizeof(cheader.maxGroup));
	if (code != 0)
	    return code;
    } else {
	cheader.maxID = htonl(id);
	code =
	    pr_Write(at, 0, 20, (char *)&cheader.maxID,
		     sizeof(cheader.maxID));
	if (code != 0)
	    return code;
    }
    return PRSUCCESS;
}

afs_int32
read_DbHeader(struct ubik_trans *tt)
{
    afs_int32 code;

    if (!ubik_CacheUpdate(tt))
	return 0;

    code = pr_Read(tt, 0, 0, (char *)&cheader, sizeof(cheader));
    if (code != 0) {
	afs_com_err(whoami, code, "Couldn't read header");
    }
    return code;
}

int pr_noAuth;
afs_int32 initd = 0;

afs_int32
Initdb()
{
    afs_int32 code;
    struct ubik_trans *tt;
    afs_int32 len;

    /* init the database.  We'll try reading it, but if we're starting
     * from scratch, we'll have to do a write transaction. */

    pr_noAuth = afsconf_GetNoAuthFlag(prdir);

    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    if (!initd) {
	initd = 1;
    } else if (!ubik_CacheUpdate(tt)) {
	code = ubik_EndTrans(tt);
	return code;
    }

    len = sizeof(cheader);
    code = pr_Read(tt, 0, 0, (char *)&cheader, len);
    if (code != 0) {
	afs_com_err(whoami, code, "couldn't read header");
	ubik_AbortTrans(tt);
	return code;
    }
    if ((ntohl(cheader.version) == PRDBVERSION)
	&& ntohl(cheader.headerSize) == sizeof(cheader)
	&& ntohl(cheader.eofPtr) != (afs_uint32) NULL
	&& FindByID(tt, ANONYMOUSID) != 0) {
	/* database exists, so we don't have to build it */
	code = ubik_EndTrans(tt);
	if (code)
	    return code;
	return PRSUCCESS;
    }
    /* else we need to build a database */
    code = ubik_EndTrans(tt);
    if (code)
	return code;

    /* Only rebuild database if the db was deleted (the header is zero) and we
     * are running noAuth. */
    {
	char *bp = (char *)&cheader;
	int i;
	for (i = 0; i < sizeof(cheader); i++)
	    if (bp[i]) {
		code = PRDBBAD;
		afs_com_err(whoami, code,
			"Can't rebuild database because it is not empty");
		return code;
	    }
    }
    if (!pr_noAuth) {
	code = PRDBBAD;
	afs_com_err(whoami, code,
		"Can't rebuild database because not running NoAuth");
	return code;
    }

    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;

    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }

    /* before doing a rebuild, check again that the dbase looks bad, because
     * the previous check was only under a ReadAny transaction, and there could
     * actually have been a good database out there.  Now that we have a
     * real write transaction, make sure things are still bad.
     */
    if ((ntohl(cheader.version) == PRDBVERSION)
	&& ntohl(cheader.headerSize) == sizeof(cheader)
	&& ntohl(cheader.eofPtr) != (afs_uint32) NULL
	&& FindByID(tt, ANONYMOUSID) != 0) {
	/* database exists, so we don't have to build it */
	code = ubik_EndTrans(tt);
	if (code)
	    return code;
	return PRSUCCESS;
    }

    /* Initialize the database header */
    if ((code = set_header_word(tt, version, htonl(PRDBVERSION)))
	|| (code = set_header_word(tt, headerSize, htonl(sizeof(cheader))))
	|| (code = set_header_word(tt, eofPtr, cheader.headerSize))) {
	afs_com_err(whoami, code, "couldn't write header words");
	ubik_AbortTrans(tt);
	return code;
    }
#define InitialGroup(id,name) do {    \
    afs_int32 temp = (id);		      \
    afs_int32 flag = (id) < 0 ? PRGRP : 0; \
    code = CreateEntry		      \
	(tt, (name), &temp, /*idflag*/1, flag, SYSADMINID, SYSADMINID); \
    if (code) {			      \
	afs_com_err (whoami, code, "couldn't create %s with id %di.", 	\
		 (name), (id));	      \
	ubik_AbortTrans(tt);	      \
	return code;		      \
    }				      \
} while (0)

    InitialGroup(SYSADMINID, "system:administrators");
    InitialGroup(SYSBACKUPID, "system:backup");
    InitialGroup(ANYUSERID, "system:anyuser");
    InitialGroup(AUTHUSERID, "system:authuser");
    InitialGroup(SYSVIEWERID, "system:ptsviewers");
    InitialGroup(ANONYMOUSID, "anonymous");

    /* Well, we don't really want the max id set to anonymousid, so we'll set
     * it back to 0 */
    code = set_header_word(tt, maxID, 0);	/* correct in any byte order */
    if (code) {
	afs_com_err(whoami, code, "couldn't reset max id");
	ubik_AbortTrans(tt);
	return code;
    }

    code = ubik_EndTrans(tt);
    if (code)
	return code;
    return PRSUCCESS;
}

afs_int32
ChangeEntry(struct ubik_trans *at, afs_int32 aid, afs_int32 cid, char *name, afs_int32 oid, afs_int32 newid)
{
    afs_int32 code;
    afs_int32 i, nptr, pos;
#if defined(SUPERGROUPS)
    afs_int32 nextpos;
#endif
    struct contentry centry;
    struct prentry tentry, tent;
    afs_int32 loc;
    afs_int32 oldowner;
    char holder[PR_MAXNAMELEN];
    char temp[PR_MAXNAMELEN];
    char oldname[PR_MAXNAMELEN];
    char *atsign;

    memset(holder, 0, PR_MAXNAMELEN);
    memset(temp, 0, PR_MAXNAMELEN);
    loc = FindByID(at, aid);
    if (!loc)
	return PRNOENT;
    code = pr_ReadEntry(at, 0, loc, &tentry);
    if (code)
	return PRDBFAIL;
    if (restricted && !IsAMemberOf(at, cid, SYSADMINID)) 
	return PRPERM;
    if (tentry.owner != cid && !IsAMemberOf(at, cid, SYSADMINID)
	&& !IsAMemberOf(at, cid, tentry.owner) && !pr_noAuth)
	return PRPERM;
#ifdef PR_REMEMBER_TIMES
    tentry.changeTime = time(0);
#endif

    /* we're actually trying to change the id */
    if (newid && (newid != aid)) {
	if (!IsAMemberOf(at, cid, SYSADMINID) && !pr_noAuth)
	    return PRPERM;

	pos = FindByID(at, newid);
	if (pos)
	    return PRIDEXIST;	/* new id already in use! */
	if ((aid < 0 && newid > 0) || (aid > 0 && newid < 0))
	    return PRPERM;

	/* Should check that foreign users id to change to is good: inRange() */

	/* if new id is not in use, rehash things */
	code = RemoveFromIDHash(at, aid, &loc);
	if (code != PRSUCCESS)
	    return code;
	tentry.id = newid;
	code = pr_WriteEntry(at, 0, loc, &tentry);
	if (code)
	    return code;
	code = AddToIDHash(at, tentry.id, loc);
	if (code)
	    return code;

	/* get current data */
	code = pr_ReadEntry(at, 0, loc, &tentry);
	if (code)
	    return PRDBFAIL;

#if defined(SUPERGROUPS)
	if (tentry.id > (afs_int32) ntohl(cheader.maxID))
	    code = set_header_word(at, maxID, htonl(tentry.id));
	if (code)
	    return PRDBFAIL;

	/* need to fix up: membership
	 * (supergroups?)
	 * ownership
	 */

	for (i = 0; i < PRSIZE; i++) {
	    if (tentry.entries[i] == PRBADID)
		continue;
	    if (tentry.entries[i] == 0)
		break;
	    if ((tentry.flags & PRGRP) && tentry.entries[i] < 0) {	/* Supergroup */
		return 5;	/* not yet, in short. */
	    } else {
		code = ChangeIDEntry(at, aid, newid, tentry.entries[i]);
	    }
	    if (code)
		return code;
	}
	for (pos = ntohl(tentry.owned); pos; pos = nextpos) {
	    code = pr_ReadEntry(at, 0, pos, &tent);
	    if (code)
		break;
	    tent.owner = newid;
	    nextpos = tent.nextOwned;
	    code = pr_WriteEntry(at, 0, pos, &tent);
	    if (code)
		break;
	}
	pos = tentry.next;
	while (pos) {
#define centry  (*(struct contentry*)&tent)
	    code = pr_ReadCoEntry(at, 0, pos, &centry);
	    if ((centry.id != aid)
		|| !(centry.flags & PRCONT)) {
		fprintf(stderr,
			"ChangeEntry: bad database aid=%d centry.id=%d .flags=%d\n",
			aid, centry.id, centry.flags);
		return PRDBBAD;
	    }
	    centry.id = newid;
	    for (i = 0; i < COSIZE; i++) {
		if (centry.entries[i] == PRBADID)
		    continue;
		if (centry.entries[i] == 0)
		    break;
		if ((centry.flags & PRGRP) && centry.entries[i] < 0) {	/* Supergroup */
		    return 5;	/* not yet, in short. */
		} else {
		    code = ChangeIDEntry(at, aid, newid, centry.entries[i]);
		}
		if (code)
		    return code;
	    }
	    code = pr_WriteCoEntry(at, 0, pos, &centry);
	    pos = centry.next;
#undef centry
	}
	if (code)
	    return code;

#else /* SUPERGROUPS */


	/* Also change the references from the membership list */
	for (i = 0; i < PRSIZE; i++) {
	    if (tentry.entries[i] == PRBADID)
		continue;
	    if (tentry.entries[i] == 0)
		break;
	    pos = FindByID(at, tentry.entries[i]);
	    if (!pos)
		return (PRDBFAIL);
	    code = RemoveFromEntry(at, aid, tentry.entries[i]);
	    if (code)
		return code;
	    code = pr_ReadEntry(at, 0, pos, &tent);
	    if (code)
		return code;
	    code = AddToEntry(at, &tent, pos, newid);
	    if (code)
		return code;
	}
	/* Look through cont entries too. This needs to be broken into
	 * seperate transaction so that no one transaction becomes too 
	 * large to complete.
	 */
	for (nptr = tentry.next; nptr; nptr = centry.next) {
	    code = pr_ReadCoEntry(at, 0, nptr, &centry);
	    if (code)
		return code;
	    for (i = 0; i < COSIZE; i++) {
		if (centry.entries[i] == PRBADID)
		    continue;
		if (centry.entries[i] == 0)
		    break;
		pos = FindByID(at, centry.entries[i]);
		if (!pos)
		    return (PRDBFAIL);
		code = RemoveFromEntry(at, aid, centry.entries[i]);
		if (code)
		    return code;
		code = pr_ReadEntry(at, 0, pos, &tent);
		if (code)
		    return code;
		code = AddToEntry(at, &tent, pos, newid);
		if (code)
		    return code;
	    }
	}
#endif /* SUPERGROUPS */
    }

    atsign = strchr(tentry.name, '@');	/* check for foreign entry */

    /* Change the owner */
    if (oid && (oid != tentry.owner)) {
	/* only groups can have their owner's changed */
	if (!(tentry.flags & PRGRP))
	    return PRPERM;
	if (atsign != NULL)
	    return PRPERM;
	oldowner = tentry.owner;
	tentry.owner = oid;
	/* The entry must be written through first so Remove and Add routines
	 * can operate on disk data */
	code = pr_WriteEntry(at, 0, loc, (char *)&tentry);
	if (code)
	    return PRDBFAIL;

	/* switch owner chains */
	if (oldowner)		/* if it has an owner */
	    code = RemoveFromOwnerChain(at, tentry.id, oldowner);
	else			/* must be an orphan */
	    code = RemoveFromOrphan(at, tentry.id);
	if (code)
	    return code;
	code = AddToOwnerChain(at, tentry.id, tentry.owner);
	if (code)
	    return code;

	/* fix up the name */
	if (strlen(name) == 0)
	    name = tentry.name;
	/* get current data */
	code = pr_ReadEntry(at, 0, loc, &tentry);
	if (code)
	    return PRDBFAIL;
    }

    /* Change the name, if name is a ptr to tentry.name then this name change
     * is due to a chown, otherwise caller has specified a new name */
    if ((name == tentry.name) || (*name && (strcmp(tentry.name, name) != 0))) {
	strncpy(oldname, tentry.name, PR_MAXNAMELEN);
	if (tentry.flags & PRGRP) {
	    /* don't let foreign cell groups change name */
	    if (atsign != NULL)
		return PRPERM;
	    code = CorrectGroupName(at, name, cid, tentry.owner, tentry.name);
	    if (code)
		return code;

	    if (name == tentry.name) {	/* owner fixup */
		if (strcmp(oldname, tentry.name) == 0)
		    goto nameOK;
	    } else {		/* new name, caller must be correct */
		if (strcmp(name, tentry.name) != 0)
		    return PRBADNAM;
	    }
	} else
	    /* Allow a foreign name change only if the cellname part is
	     * the same */
	{
	    char *newatsign;

	    newatsign = strchr(name, '@');
	    if (newatsign != atsign) {	/* if they are the same no problem */
		/*if the pointers are not equal the strings better be */
		if ((atsign == NULL) || (newatsign == NULL)
		    || strcmp(atsign, newatsign))
		    return PRPERM;
	    }
	    if (!CorrectUserName(name))
		return PRBADNAM;
	}

	pos = FindByName(at, name, &tent);
	if (pos)
	    return PREXIST;
	code = RemoveFromNameHash(at, oldname, &loc);
	if (code != PRSUCCESS)
	    return code;
	strncpy(tentry.name, name, PR_MAXNAMELEN);
	code = pr_WriteEntry(at, 0, loc, (char *)&tentry);
	if (code)
	    return PRDBFAIL;
	code = AddToNameHash(at, tentry.name, loc);
	if (code != PRSUCCESS)
	    return code;
      nameOK:;
    }
    return PRSUCCESS;
}


afs_int32
allocNextId(struct ubik_trans * at, struct prentry * cellEntry)
{
    /* Id's for foreign cell entries are constructed as follows:
     * The 16 low order bits are the group id of the cell and the
     * top 16 bits identify the particular users in that cell */

    afs_int32 id;
    afs_int32 cellid = ((ntohl(cellEntry->id)) & 0x0000ffff);

    id = (ntohl(cellEntry->nusers) + 1);
    while (FindByID(at, ((id << 16) | cellid))) {
	id++;
	if (id > 0xffff)
	    return 0;
    }

    cellEntry->nusers = htonl(id);
    /* use the field nusers to keep 
     * the next available id in that
     * foreign cell's group. Note :
     * It would seem more appropriate
     * to use ngroup for that and nusers
     * to enforce the quota, however pts
     * does not have an option to change 
     * foreign users quota yet  */

    id = (id << 16) | cellid;
    return id;
}

int
inRange(struct prentry *cellEntry, afs_int32 aid)
{
    afs_uint32 id, cellid, groupid;


    /*
     * The only thing that we want to make sure here is that
     * the id is in the legal range of this group. If it is
     * a duplicate we don't care since it will get caught 
     * in a different check.
     */

    cellid = aid & 0x0000ffff;
    groupid = (ntohl(cellEntry->id)) & 0x0000ffff;
    if (cellid != groupid)
	return 0;		/* not in range */

    /* 
     * if we got here we're ok but we need to update the nusers
     * field in order to get the id correct the next time that 
     * we try to allocate it automatically
     */

    id = aid >> 16;
    if (id > ntohl(cellEntry->nusers))
	cellEntry->nusers = htonl(id);
    return 1;

}

int
AddAuthGroup(struct prentry *tentry, prlist *alist, afs_int32 *size)
{
    if (!(strchr(tentry->name, '@')))
	return (AddToPRList(alist, size, AUTHUSERID));
    else
	return PRSUCCESS;
}
