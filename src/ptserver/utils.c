/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/opr.h>
#include <afs/afs_lock.h>
#include <ubik.h>

#include "ptserver.h"
#include "pterror.h"
#include "ptprototypes.h"

#if defined(SUPERGROUPS)
extern afs_int32 depthsg;
afs_int32 IsAMemberOfSG(struct pt_ctx *ctx, afs_int32 aid, afs_int32 gid,
			afs_int32 depth);
#endif

static afs_int32
IDHash(afs_int32 x)
{
    /* returns hash bucket for x */
    return ((abs(x)) % HASHSIZE);
}

static afs_int32
NameHash(char *aname)
{
    /* returns hash bucket for aname */
    unsigned int hash = 0;
    size_t i;
/* stolen directly from the HashString function in the vol package */
    for (i = strlen(aname), aname += i - 1; i--; aname--)
	hash = (hash * 31) + (*(unsigned char *)aname - 31);
    return (hash % HASHSIZE);
}


afs_int32
pr_Write(struct pt_ctx *ctx, afs_int32 afd, afs_int32 pos, void *buff, afs_int32 len)
{
    /* package up seek and write into one procedure for ease of use */
    struct ubik_trans *tt = ctx->trans;
    afs_int32 code;
    if ((pos < sizeof(ctx->cheader[0])) && (buff != (char *)ctx->cheader + pos)) {
	fprintf(stderr,
		"ptserver: dbwrite: Illegal attempt to write a location 0\n");
	return PRDBFAIL;
    }
    code = ubik_Seek(tt, afd, pos);
    if (code)
	return code;
    code = ubik_Write(tt, buff, len);
    return code;
}

afs_int32
pr_Read(struct pt_ctx *ctx, afs_int32 afd, afs_int32 pos, void *buff, afs_int32 len)
{
    /* same thing for read */
    struct ubik_trans *tt = ctx->trans;
    afs_int32 code;
    code = ubik_Seek(tt, afd, pos);
    if (code)
	return code;
    code = ubik_Read(tt, buff, len);
    return code;
}

int
pr_WriteEntry(struct pt_ctx *ctx, afs_int32 afd, afs_int32 pos, struct prentry *tentry)
{
    afs_int32 code;
    afs_int32 i;
    struct prentry nentry;

    if (ntohl(1) != 1) {	/* Need to swap bytes. */
	memset(&nentry, 0, sizeof(nentry));	/* make sure reseved fields are zero */
	nentry.flags = htonl(tentry->flags);
	nentry.id = htonl(tentry->id);
	nentry.cellid = htonl(tentry->cellid);
	nentry.next = htonl(tentry->next);
	nentry.nextID = htonl(tentry->nextID);
	nentry.nextName = htonl(tentry->nextName);
	nentry.owner = htonl(tentry->owner);
	nentry.creator = htonl(tentry->creator);
	nentry.ngroups = htonl(tentry->ngroups);
	nentry.nusers = htonl(tentry->nusers);
	nentry.count = htonl(tentry->count);
	nentry.instance = htonl(tentry->instance);
	nentry.owned = htonl(tentry->owned);
	nentry.nextOwned = htonl(tentry->nextOwned);
	nentry.parent = htonl(tentry->parent);
	nentry.sibling = htonl(tentry->sibling);
	nentry.child = htonl(tentry->child);
	strncpy(nentry.name, tentry->name, PR_MAXNAMELEN);
	nentry.createTime = htonl(tentry->createTime);
	nentry.addTime = htonl(tentry->addTime);
	nentry.removeTime = htonl(tentry->removeTime);
	nentry.changeTime = htonl(tentry->changeTime);
	for (i = 0; i < PRSIZE; i++)
	    nentry.entries[i] = htonl(tentry->entries[i]);
	tentry = &nentry;
    }
    code = pr_Write(ctx, afd, pos, (char *)tentry, sizeof(struct prentry));
    return (code);
}

int
pr_ReadEntry(struct pt_ctx *ctx, afs_int32 afd, afs_int32 pos, struct prentry *tentry)
{
    struct ubik_trans *tt = ctx->trans;
    afs_int32 code;
    afs_int32 i;
    struct prentry nentry;
    code = ubik_Seek(tt, afd, pos);
    if (code)
	return (code);
    if (ntohl(1) == 1) {	/* no swapping needed */
	code = ubik_Read(tt, (char *)tentry, sizeof(struct prentry));
	return (code);
    }
    code = ubik_Read(tt, (char *)&nentry, sizeof(struct prentry));
    if (code)
	return (code);
    memset(tentry, 0, sizeof(*tentry));	/* make sure reseved fields are zero */
    tentry->flags = ntohl(nentry.flags);
    tentry->id = ntohl(nentry.id);
    tentry->cellid = ntohl(nentry.cellid);
    tentry->next = ntohl(nentry.next);
    tentry->nextID = ntohl(nentry.nextID);
    tentry->nextName = ntohl(nentry.nextName);
    tentry->owner = ntohl(nentry.owner);
    tentry->creator = ntohl(nentry.creator);
    tentry->ngroups = ntohl(nentry.ngroups);
    tentry->nusers = ntohl(nentry.nusers);
    tentry->count = ntohl(nentry.count);
    tentry->instance = ntohl(nentry.instance);
    tentry->owned = ntohl(nentry.owned);
    tentry->nextOwned = ntohl(nentry.nextOwned);
    tentry->parent = ntohl(nentry.parent);
    tentry->sibling = ntohl(nentry.sibling);
    tentry->child = ntohl(nentry.child);
    strncpy(tentry->name, nentry.name, PR_MAXNAMELEN);
    tentry->createTime = ntohl(nentry.createTime);
    tentry->addTime = ntohl(nentry.addTime);
    tentry->removeTime = ntohl(nentry.removeTime);
    tentry->changeTime = ntohl(nentry.changeTime);
    for (i = 0; i < PRSIZE; i++)
	tentry->entries[i] = ntohl(nentry.entries[i]);
    return (code);
}

int
pr_WriteCoEntry(struct pt_ctx *ctx, afs_int32 afd, afs_int32 pos, struct contentry *tentry)
{
    afs_int32 code;
    afs_int32 i;
    struct contentry nentry;

    if (ntohl(1) != 1) {	/* No need to swap */
	memset(&nentry, 0, sizeof(nentry));	/* make reseved fields zero */
	nentry.flags = htonl(tentry->flags);
	nentry.id = htonl(tentry->id);
	nentry.cellid = htonl(tentry->cellid);
	nentry.next = htonl(tentry->next);
	for (i = 0; i < COSIZE; i++)
	    nentry.entries[i] = htonl(tentry->entries[i]);
	tentry = &nentry;
    }
    code = pr_Write(ctx, afd, pos, (char *)tentry, sizeof(struct contentry));
    return (code);
}

int
pr_ReadCoEntry(struct pt_ctx *ctx, afs_int32 afd, afs_int32 pos, struct contentry *tentry)
{
    struct ubik_trans *tt = ctx->trans;
    afs_int32 code;
    afs_int32 i;
    struct contentry nentry;
    code = ubik_Seek(tt, afd, pos);
    if (code)
	return (code);
    if (ntohl(1) == 1) {	/* No swapping needed. */
	code = ubik_Read(tt, (char *)tentry, sizeof(struct contentry));
	return (code);
    }
    code = ubik_Read(tt, (char *)&nentry, sizeof(struct contentry));
    if (code)
	return (code);
    memset(tentry, 0, sizeof(*tentry));	/* make reseved fields zero */
    tentry->flags = ntohl(nentry.flags);
    tentry->id = ntohl(nentry.id);
    tentry->cellid = ntohl(nentry.cellid);
    tentry->next = ntohl(nentry.next);
    for (i = 0; i < COSIZE; i++)
	tentry->entries[i] = ntohl(nentry.entries[i]);
    return (code);
}

/* AllocBloc - allocate a free block of storage for entry, returning address of
 * new entry */

afs_int32
AllocBlock(struct pt_ctx *ctx)
{
    afs_int32 code;
    afs_int32 temp;
    struct prentry tentry;

    if (ctx->cheader->freePtr) {
	/* allocate this dude */
	temp = ntohl(ctx->cheader->freePtr);
	code = pr_ReadEntry(ctx, 0, temp, &tentry);
	if (code)
	    return 0;
	ctx->cheader->freePtr = htonl(tentry.next);
	code =
	    pr_Write(ctx, 0, 8, (char *)&ctx->cheader->freePtr,
		     sizeof(ctx->cheader->freePtr));
	if (code != 0)
	    return 0;
	return temp;
    } else {
	/* hosed, nothing on free list, grow file */
	temp = ntohl(ctx->cheader->eofPtr);	/* remember this guy */
	ctx->cheader->eofPtr = htonl(temp + ENTRYSIZE);
	code =
	    pr_Write(ctx, 0, 12, (char *)&ctx->cheader->eofPtr,
		     sizeof(ctx->cheader->eofPtr));
	if (code != 0)
	    return 0;
	return temp;
    }
}

afs_int32
FreeBlock(struct pt_ctx *ctx, afs_int32 pos)
{
    /* add a block of storage to the free list */
    afs_int32 code;
    struct prentry tentry;

    memset(&tentry, 0, sizeof(tentry));
    tentry.next = ntohl(ctx->cheader->freePtr);
    tentry.flags |= PRFREE;
    ctx->cheader->freePtr = htonl(pos);
    code =
	pr_Write(ctx, 0, 8, (char *)&ctx->cheader->freePtr, sizeof(ctx->cheader->freePtr));
    if (code != 0)
	return code;
    code = pr_WriteEntry(ctx, 0, pos, &tentry);
    if (code != 0)
	return code;
    return PRSUCCESS;
}

afs_int32
FindByID(struct pt_ctx *ctx, afs_int32 aid)
{
    /* returns address of entry if found, 0 otherwise */
    afs_int32 code;
    afs_int32 i;
    struct prentry tentry;
    afs_int32 entry;

    if ((aid == PRBADID) || (aid == 0))
	return 0;
    i = IDHash(aid);
    entry = ntohl(ctx->cheader->idHash[i]);
    if (entry == 0)
	return entry;
    memset(&tentry, 0, sizeof(tentry));
    code = pr_ReadEntry(ctx, 0, entry, &tentry);
    if (code != 0)
	return 0;
    if (aid == tentry.id)
	return entry;
    opr_Assert(entry != tentry.nextID);
    entry = tentry.nextID;
    while (entry != 0) {
	memset(&tentry, 0, sizeof(tentry));
	code = pr_ReadEntry(ctx, 0, entry, &tentry);
	if (code != 0)
	    return 0;
	if (aid == tentry.id)
	    return entry;
	opr_Assert(entry != tentry.nextID);
	entry = tentry.nextID;
    }
    return 0;
}

afs_int32
FindByName(struct pt_ctx *ctx, char aname[PR_MAXNAMELEN], struct prentry *tentryp)
{
    /* ditto */
    afs_int32 code;
    afs_int32 i;
    afs_int32 entry;

    i = NameHash(aname);
    entry = ntohl(ctx->cheader->nameHash[i]);
    if (entry == 0)
	return entry;
    memset(tentryp, 0, sizeof(struct prentry));
    code = pr_ReadEntry(ctx, 0, entry, tentryp);
    if (code != 0)
	return 0;
    if ((strncmp(aname, tentryp->name, PR_MAXNAMELEN)) == 0)
	return entry;
    opr_Assert(entry != tentryp->nextName);
    entry = tentryp->nextName;
    while (entry != 0) {
	memset(tentryp, 0, sizeof(struct prentry));
	code = pr_ReadEntry(ctx, 0, entry, tentryp);
	if (code != 0)
	    return 0;
	if ((strncmp(aname, tentryp->name, PR_MAXNAMELEN)) == 0)
	    return entry;
	opr_Assert(entry != tentryp->nextName);
	entry = tentryp->nextName;
    }
    return 0;
}

afs_int32
AllocID(struct pt_ctx *ctx, afs_int32 flag, afs_int32 *aid)
{
    /* allocs an id from the proper area of address space, based on flag */
    afs_int32 code = 1;
    afs_int32 i = 0;
    int maxcount = 50;	/* to prevent infinite loops */

    if (flag & PRGRP) {
	*aid = ntohl(ctx->cheader->maxGroup);
	/* Check for PRBADID to avoid wrap-around. */
	while (code && i < maxcount && *aid != PRBADID) {
	    --(*aid);
	    code = FindByID(ctx, *aid);
	    i++;
	}
	if (code)
	    return PRNOIDS;
	ctx->cheader->maxGroup = htonl(*aid);
	code =
	    pr_Write(ctx, 0, 16, (char *)&ctx->cheader->maxGroup,
		     sizeof(ctx->cheader->maxGroup));
	if (code)
	    return PRDBFAIL;
	return PRSUCCESS;
    } else if (flag & PRFOREIGN) {
	*aid = ntohl(ctx->cheader->maxForeign);
	while (code && i < maxcount) {
	    ++(*aid);
	    code = FindByID(ctx, *aid);
	    i++;
	}
	if (code)
	    return PRNOIDS;
	ctx->cheader->maxForeign = htonl(*aid);
	code =
	    pr_Write(ctx, 0, 24, (char *)&ctx->cheader->maxForeign,
		     sizeof(ctx->cheader->maxForeign));
	if (code)
	    return PRDBFAIL;
	return PRSUCCESS;
    } else {
	*aid = ntohl(ctx->cheader->maxID);
	while (code && i < maxcount && *aid != 0x7fffffff) {
	    ++(*aid);
	    code = FindByID(ctx, *aid);
	    i++;
	}
	if (code)
	    return PRNOIDS;
	ctx->cheader->maxID = htonl(*aid);
	code =
	    pr_Write(ctx, 0, 20, (char *)&ctx->cheader->maxID,
		     sizeof(ctx->cheader->maxID));
	if (code)
	    return PRDBFAIL;
	return PRSUCCESS;
    }
}

int
IDCmp(const void *a, const void *b)
{
    /* used to sort CPS's so that comparison with acl's is easier */
    if (*(afs_int32 *)a > *(afs_int32 *)b) {
	return 1;
    } else if (*(afs_int32 *)a == *(afs_int32 *)b) {
	return 0;
    } else /* (*a < *b) */ {
	return -1;
    }
}

afs_int32
RemoveFromIDHash(struct pt_ctx *ctx, afs_int32 aid, afs_int32 *loc)		/* ??? in case ID hashed twice ??? */
{
    /* remove entry designated by aid from id hash table */
    afs_int32 code;
    afs_int32 current, trail, i;
    struct prentry tentry;
    struct prentry bentry;

    if ((aid == PRBADID) || (aid == 0))
	return PRINCONSISTENT;
    i = IDHash(aid);
    current = ntohl(ctx->cheader->idHash[i]);
    memset(&tentry, 0, sizeof(tentry));
    memset(&bentry, 0, sizeof(bentry));
    trail = 0;
    if (current == 0)
	return PRSUCCESS;	/* already gone */
    code = pr_ReadEntry(ctx, 0, current, &tentry);
    if (code)
	return PRDBFAIL;
    while (aid != tentry.id) {
	opr_Assert(trail != current);
	trail = current;
	current = tentry.nextID;
	if (current == 0)
	    break;
	code = pr_ReadEntry(ctx, 0, current, &tentry);
	if (code)
	    return PRDBFAIL;
    }
    if (current == 0)
	return PRSUCCESS;	/* we didn't find him, so he's already gone */
    if (trail == 0) {
	/* it's the first entry! */
	ctx->cheader->idHash[i] = htonl(tentry.nextID);
	code =
	    pr_Write(ctx, 0, 72 + HASHSIZE * 4 + i * 4,
		     (char *)&ctx->cheader->idHash[i], sizeof(ctx->cheader->idHash[i]));
	if (code)
	    return PRDBFAIL;
    } else {
	code = pr_ReadEntry(ctx, 0, trail, &bentry);
	if (code)
	    return PRDBFAIL;
	bentry.nextID = tentry.nextID;
	code = pr_WriteEntry(ctx, 0, trail, &bentry);
	if (code)
	    return PRDBFAIL;
    }
    *loc = current;
    return PRSUCCESS;
}

afs_int32
AddToIDHash(struct pt_ctx *ctx, afs_int32 aid, afs_int32 loc)
{
    /* add entry at loc designated by aid to id hash table */
    afs_int32 code;
    afs_int32 i;
    struct prentry tentry;

    if ((aid == PRBADID) || (aid == 0))
	return PRINCONSISTENT;
    i = IDHash(aid);
    memset(&tentry, 0, sizeof(tentry));
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code)
	return PRDBFAIL;
    tentry.nextID = ntohl(ctx->cheader->idHash[i]);
    ctx->cheader->idHash[i] = htonl(loc);
    code = pr_WriteEntry(ctx, 0, loc, &tentry);
    if (code)
	return PRDBFAIL;
    code =
	pr_Write(ctx, 0, 72 + HASHSIZE * 4 + i * 4, (char *)&ctx->cheader->idHash[i],
		 sizeof(ctx->cheader->idHash[i]));
    if (code)
	return PRDBFAIL;
    return PRSUCCESS;
}

afs_int32
RemoveFromNameHash(struct pt_ctx *ctx, char *aname, afs_int32 *loc)
{
    /* remove from name hash */
    afs_int32 code;
    afs_int32 current, trail, i;
    struct prentry tentry;
    struct prentry bentry;

    i = NameHash(aname);
    current = ntohl(ctx->cheader->nameHash[i]);
    memset(&tentry, 0, sizeof(tentry));
    memset(&bentry, 0, sizeof(bentry));
    trail = 0;
    if (current == 0)
	return PRSUCCESS;	/* already gone */
    code = pr_ReadEntry(ctx, 0, current, &tentry);
    if (code)
	return PRDBFAIL;
    while (strcmp(aname, tentry.name)) {
	opr_Assert(trail != current);
	trail = current;
	current = tentry.nextName;
	if (current == 0)
	    break;
	code = pr_ReadEntry(ctx, 0, current, &tentry);
	if (code)
	    return PRDBFAIL;
    }
    if (current == 0)
	return PRSUCCESS;	/* we didn't find him, already gone */
    if (trail == 0) {
	/* it's the first entry! */
	ctx->cheader->nameHash[i] = htonl(tentry.nextName);
	code =
	    pr_Write(ctx, 0, 72 + i * 4, (char *)&ctx->cheader->nameHash[i],
		     sizeof(ctx->cheader->nameHash[i]));
	if (code)
	    return PRDBFAIL;
    } else {
	code = pr_ReadEntry(ctx, 0, trail, &bentry);
	if (code)
	    return PRDBFAIL;
	bentry.nextName = tentry.nextName;
	code = pr_WriteEntry(ctx, 0, trail, &bentry);
	if (code)
	    return PRDBFAIL;
    }
    *loc = current;
    return PRSUCCESS;
}

afs_int32
AddToNameHash(struct pt_ctx *ctx, char *aname, afs_int32 loc)
{
    /* add to name hash */
    afs_int32 code;
    afs_int32 i;
    struct prentry tentry;

    i = NameHash(aname);
    memset(&tentry, 0, sizeof(tentry));
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code)
	return PRDBFAIL;
    tentry.nextName = ntohl(ctx->cheader->nameHash[i]);
    ctx->cheader->nameHash[i] = htonl(loc);
    code = pr_WriteEntry(ctx, 0, loc, &tentry);
    if (code)
	return PRDBFAIL;
    code =
	pr_Write(ctx, 0, 72 + i * 4, (char *)&ctx->cheader->nameHash[i],
		 sizeof(ctx->cheader->nameHash[i]));
    if (code)
	return PRDBFAIL;
    return PRSUCCESS;
}

afs_int32
AddToOwnerChain(struct pt_ctx *ctx, afs_int32 gid, afs_int32 oid)
{
    /* add entry designated by gid to owner chain of entry designated by oid */
    afs_int32 code;
    afs_int32 loc;
    struct prentry tentry;
    struct prentry gentry;
    afs_int32 gloc;

    loc = FindByID(ctx, oid);
    if (!loc)
	return PRNOENT;
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code != 0)
	return PRDBFAIL;
    if (oid == gid) {		/* added it to its own chain */
	tentry.nextOwned = tentry.owned;
	tentry.owned = loc;
    } else {
	gloc = FindByID(ctx, gid);
	code = pr_ReadEntry(ctx, 0, gloc, &gentry);
	if (code != 0)
	    return PRDBFAIL;
	gentry.nextOwned = tentry.owned;
	tentry.owned = gloc;
	code = pr_WriteEntry(ctx, 0, gloc, &gentry);
	if (code != 0)
	    return PRDBFAIL;
    }
    code = pr_WriteEntry(ctx, 0, loc, &tentry);
    if (code != 0)
	return PRDBFAIL;
    return PRSUCCESS;
}

/* RemoveFromOwnerChain - remove gid from owner chain for oid */

afs_int32
RemoveFromOwnerChain(struct pt_ctx *ctx, afs_int32 gid, afs_int32 oid)
{
    afs_int32 code;
    afs_int32 nptr;
    struct prentry thisEntry;
    struct prentry thatEntry;
    struct prentry *te;		/* pointer to current (this) entry */
    struct prentry *le;		/* pointer to previous (last) entry */
    afs_int32 loc, lastLoc;

    loc = FindByID(ctx, oid);
    if (!loc)
	return PRNOENT;
    code = pr_ReadEntry(ctx, 0, loc, &thisEntry);
    if (code != 0)
	return PRDBFAIL;
    le = &thisEntry;
    lastLoc = 0;
    nptr = thisEntry.owned;
    while (nptr != 0) {
	if (nptr == lastLoc)
	    te = le;
	else {
	    if (&thisEntry == le)
		te = &thatEntry;
	    else
		te = &thisEntry;
	    code = pr_ReadEntry(ctx, 0, nptr, te);
	    if (code != 0)
		return PRDBFAIL;
	}
	if (te->id == gid) {
	    /* found it */
	    if (lastLoc == 0) {	/* modifying first of chain */
		le->owned = te->nextOwned;
		lastLoc = loc;	/* so we write to correct location */
	    } else
		le->nextOwned = te->nextOwned;
	    te->nextOwned = 0;
	    if (te != le) {
		code = pr_WriteEntry(ctx, 0, nptr, te);
		if (code != 0)
		    return PRDBFAIL;
	    }
	    code = pr_WriteEntry(ctx, 0, lastLoc, le);
	    if (code != 0)
		return PRDBFAIL;
	    return PRSUCCESS;
	}
	lastLoc = nptr;
	le = te;
	nptr = te->nextOwned;
    }
    return PRSUCCESS;		/* already removed? */
}

/* AddToOrphan - add gid to orphan list, as it's owner has died */

afs_int32
AddToOrphan(struct pt_ctx *ctx, afs_int32 gid)
{
    afs_int32 code;
    afs_int32 loc;
    struct prentry tentry;

    loc = FindByID(ctx, gid);
    if (!loc)
	return PRNOENT;
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code != 0)
	return PRDBFAIL;
    tentry.nextOwned = ntohl(ctx->cheader->orphan);
    code = set_header_word(ctx, orphan, htonl(loc));
    if (code != 0)
	return PRDBFAIL;
    tentry.owner = 0;		/* so there's no confusion later */
    code = pr_WriteEntry(ctx, 0, loc, &tentry);
    if (code != 0)
	return PRDBFAIL;
    return PRSUCCESS;
}

afs_int32
RemoveFromOrphan(struct pt_ctx *ctx, afs_int32 gid)
{
    /* remove gid from the orphan list */
    afs_int32 code;
    afs_int32 loc;
    afs_int32 nptr;
    struct prentry tentry;
    struct prentry bentry;

    loc = FindByID(ctx, gid);
    if (!loc)
	return PRNOENT;
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code != 0)
	return PRDBFAIL;
    if (ctx->cheader->orphan == htonl(loc)) {
	ctx->cheader->orphan = htonl(tentry.nextOwned);
	tentry.nextOwned = 0;
	code =
	    pr_Write(ctx, 0, 32, (char *)&ctx->cheader->orphan,
		     sizeof(ctx->cheader->orphan));
	if (code != 0)
	    return PRDBFAIL;
	code = pr_WriteEntry(ctx, 0, loc, &tentry);
	if (code != 0)
	    return PRDBFAIL;
	return PRSUCCESS;
    }
    nptr = ntohl(ctx->cheader->orphan);
    memset(&bentry, 0, sizeof(bentry));
    loc = 0;
    while (nptr != 0) {
	code = pr_ReadEntry(ctx, 0, nptr, &tentry);
	if (code != 0)
	    return PRDBFAIL;
	if (gid == tentry.id) {
	    /* found it */
	    bentry.nextOwned = tentry.nextOwned;
	    tentry.nextOwned = 0;
	    code = pr_WriteEntry(ctx, 0, loc, &bentry);
	    if (code != 0)
		return PRDBFAIL;
	    code = pr_WriteEntry(ctx, 0, nptr, &tentry);
	    if (code != 0)
		return PRDBFAIL;
	    return PRSUCCESS;
	}
	loc = nptr;
	nptr = tentry.nextOwned;
	memcpy(&bentry, &tentry, sizeof(tentry));
    }
    return PRSUCCESS;
}

afs_int32
IsOwnerOf(struct pt_ctx *ctx, afs_int32 aid, afs_int32 gid)
{
    /* returns 1 if aid is the owner of gid, 0 otherwise */
    afs_int32 code;
    struct prentry tentry;
    afs_int32 loc;

    loc = FindByID(ctx, gid);
    if (!loc)
	return 0;
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code != 0)
	return 0;
    if (tentry.owner == aid)
	return 1;
    return 0;
}

afs_int32
OwnerOf(struct pt_ctx *ctx, afs_int32 gid)
{
    /* returns the owner of gid */
    afs_int32 code;
    afs_int32 loc;
    struct prentry tentry;

    loc = FindByID(ctx, gid);
    if (!loc)
	return 0;
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code != 0)
	return 0;
    return tentry.owner;
}


afs_int32
IsAMemberOf(struct pt_ctx *ctx, afs_int32 aid, afs_int32 gid)
{
    /* returns true if aid is a member of gid */
#if !defined(SUPERGROUPS)
    struct prentry tentry;
    struct contentry centry;
    afs_int32 code;
    afs_int32 i;
    afs_int32 loc;
#endif

    /* special case anyuser and authuser */
    if (gid == ANYUSERID)
	return 1;
    if (gid == AUTHUSERID && aid != ANONYMOUSID)
	return 1;
    /* check -localauth case */
    if (gid == SYSADMINID && aid == SYSADMINID)
        return 1;
    if ((gid == 0) || (aid == 0))
	return 0;
#if defined(SUPERGROUPS)
    return IsAMemberOfSG(ctx, aid, gid, depthsg);
#else
    loc = FindByID(ctx, gid);
    if (!loc)
	return 0;
    memset(&tentry, 0, sizeof(tentry));
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code)
	return 0;
    if (!(tentry.flags & PRGRP))
	return 0;
    for (i = 0; i < PRSIZE; i++) {
	if (tentry.entries[i] == 0)
	    return 0;
	if (tentry.entries[i] == aid)
	    return 1;
    }
    if (tentry.next) {
	loc = tentry.next;
	while (loc) {
	    memset(&centry, 0, sizeof(centry));
	    code = pr_ReadCoEntry(ctx, 0, loc, &centry);
	    if (code)
		return 0;
	    for (i = 0; i < COSIZE; i++) {
		if (centry.entries[i] == aid)
		    return 1;
		if (centry.entries[i] == 0)
		    return 0;
	    }
	    loc = centry.next;
	}
    }
    return 0;			/* actually, should never get here */
#endif
}


#if defined(SUPERGROUPS)
afs_int32
IsAMemberOfSG(struct pt_ctx *ctx, afs_int32 aid, afs_int32 gid, afs_int32 depth)
{
    /* returns true if aid is a member of gid */
    struct prentry tentry;
    struct contentry centry;
    afs_int32 code;
    afs_int32 i;
    afs_int32 loc;

    if (depth < 1)
	return 0;
    loc = FindByID(ctx, gid);
    if (!loc)
	return 0;
    memset(&tentry, 0, sizeof(tentry));
    code = pr_ReadEntry(ctx, 0, loc, &tentry);
    if (code)
	return 0;
    if (!(tentry.flags & PRGRP))
	return 0;
    for (i = 0; i < PRSIZE; i++) {
	gid = tentry.entries[i];
	if (gid == 0)
	    return 0;
	if (gid == aid)
	    return 1;
	if (gid == ANYUSERID)
	    return 1;
	if (gid == AUTHUSERID && aid != ANONYMOUSID)
	    return 1;
	if (gid < 0) {
#ifndef AFS_PTHREAD_ENV
	    IOMGR_Poll();
#endif
	    if (IsAMemberOfSG(ctx, aid, gid, depth - 1))
		return 1;
	}
    }
    if (tentry.next) {
	loc = tentry.next;
	while (loc) {
	    memset(&centry, 0, sizeof(centry));
	    code = pr_ReadCoEntry(ctx, 0, loc, &centry);
	    if (code)
		return 0;
	    for (i = 0; i < COSIZE; i++) {
		gid = centry.entries[i];
		if (gid == 0)
		    return 0;
		if (gid == aid)
		    return 1;
		if (gid == ANYUSERID)
		    return 1;
		if (gid == AUTHUSERID && aid != ANONYMOUSID)
		    return 1;
		if (gid < 0) {
#ifndef AFS_PTHREAD_ENV
		    IOMGR_Poll();
#endif
		    if (IsAMemberOfSG(ctx, aid, gid, depth - 1))
			return 1;
		}
	    }
	    loc = centry.next;
	}
    }
    return 0;			/* actually, should never get here */
}
#endif /* SUPERGROUPS */
