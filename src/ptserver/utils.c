/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef lint
#endif
#include <afs/param.h>
#include <sys/types.h>
#include <lock.h>
#include <ubik.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#endif
#include "ptserver.h"
#include "pterror.h"

afs_int32 IDHash(x)
afs_int32 x;
{
    /* returns hash bucket for x */
    return ((abs(x)) % HASHSIZE);
}

afs_int32 NameHash(aname)
register unsigned char *aname;
{
    /* returns hash bucket for aname */
    register unsigned int hash=0;
    register int i;
/* stolen directly from the HashString function in the vol package */
    for (i=strlen(aname),aname += i-1;i--;aname--)
	hash = (hash*31) + (*aname-31);
    return(hash % HASHSIZE);
}


afs_int32 pr_Write(tt,afd,pos,buff,len)
struct ubik_trans *tt;
afs_int32 afd;
afs_int32 pos;
char *buff;
afs_int32 len;
{
    /* package up seek and write into one procedure for ease of use */
    afs_int32 code;
    if ((pos < sizeof(cheader)) && (buff != (char *)&cheader + pos)) {
	fprintf (stderr, "ptserver: dbwrite: Illegal attempt to write a location 0\n");
	return PRDBFAIL;
    }
    code = ubik_Seek(tt,afd,pos);
    if (code) return code;
    code = ubik_Write(tt,buff,len);
    return code;
}

afs_int32 pr_Read(tt,afd,pos,buff,len)
struct ubik_trans *tt;
afs_int32 afd;
afs_int32 pos;
char *buff;
afs_int32 len;
{
    /* same thing for read */
    afs_int32 code;
    code = ubik_Seek(tt,afd,pos);
    if (code) return code;
    code = ubik_Read(tt,buff,len);
    return code;
}

pr_WriteEntry(tt, afd, pos, tentry)
struct ubik_trans *tt;
afs_int32 afd;
afs_int32 pos;
struct prentry *tentry;
{
    afs_int32 code;
    register afs_int32 i;
    struct prentry nentry;

    if (ntohl(1) != 1) {	/* Need to swap bytes. */
      bzero (&nentry, sizeof(nentry));	/* make sure reseved fields are zero */
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
#ifdef PR_REMEMBER_TIMES
      nentry.createTime = htonl(tentry->createTime);
      nentry.addTime = htonl(tentry->addTime);
      nentry.removeTime = htonl(tentry->removeTime);
      nentry.changeTime = htonl(tentry->changeTime);
#endif
      for (i = 0; i < PRSIZE; i++)
	nentry.entries[i] = htonl(tentry->entries[i]);
      tentry = &nentry;
    }
    code = pr_Write (tt, afd, pos, (char *)tentry, sizeof(struct prentry));
    return(code);
}
  
pr_ReadEntry(tt, afd, pos, tentry)
struct ubik_trans *tt;
afs_int32 afd;
afs_int32 pos;
struct prentry *tentry;
{
    afs_int32 code;
    register afs_int32 i;
    struct prentry nentry;
    code = ubik_Seek(tt, afd, pos);
    if (code) return (code);
    if (ntohl(1) == 1) {	/* no swapping needed */
      code = ubik_Read(tt, (char *) tentry, sizeof(struct prentry));
      return(code);
    }
    code = ubik_Read(tt, (char *) &nentry, sizeof(struct prentry));
    if (code) return (code);
    bzero (tentry, sizeof(*tentry));	/* make sure reseved fields are zero */
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
#ifdef PR_REMEMBER_TIMES
    tentry->createTime = ntohl(nentry.createTime);
    tentry->addTime = ntohl(nentry.addTime);
    tentry->removeTime = ntohl(nentry.removeTime);
    tentry->changeTime = ntohl(nentry.changeTime);
#endif
    for (i = 0; i < PRSIZE; i++)
      tentry->entries[i] = ntohl(nentry.entries[i]);
    return(code);
}

pr_WriteCoEntry(tt, afd, pos, tentry)
  struct ubik_trans *tt;
  afs_int32 afd;
  afs_int32 pos;
  struct contentry *tentry;
{
    afs_int32 code;
    register afs_int32 i;
    struct contentry nentry;

    if (ntohl(1) != 1) {	/* No need to swap */
	bzero (&nentry, sizeof(nentry)); /* make reseved fields zero */
	nentry.flags = htonl(tentry->flags);
	nentry.id = htonl(tentry->id);
	nentry.cellid = htonl(tentry->cellid);
	nentry.next = htonl(tentry->next);
	for (i = 0; i < COSIZE; i++)
	    nentry.entries[i] = htonl(tentry->entries[i]);
	tentry = &nentry;
    }
    code = pr_Write (tt, afd, pos, (char *)tentry, sizeof(struct contentry));
    return(code);
}

pr_ReadCoEntry(tt, afd, pos, tentry)
struct ubik_trans *tt;
afs_int32 afd;
afs_int32 pos;
struct contentry *tentry;
{
    afs_int32 code;
    register afs_int32 i;
    struct contentry nentry;
    code = ubik_Seek(tt, afd, pos);
    if (code) return (code);
    if (ntohl(1) == 1) {	/* No swapping needed. */
      code = ubik_Read(tt, (char *) tentry, sizeof(struct contentry));
      return(code);
    }
    code = ubik_Read(tt, (char *) &nentry, sizeof(struct contentry));
    if (code) return (code);
    bzero (tentry, sizeof(*tentry)); /* make reseved fields zero */
    tentry->flags = ntohl(nentry.flags);
    tentry->id = ntohl(nentry.id);
    tentry->cellid = ntohl(nentry.cellid);
    tentry->next = ntohl(nentry.next);
    for (i = 0; i < COSIZE; i++)
      tentry->entries[i] = ntohl(nentry.entries[i]);
    return(code);
}

/* AllocBloc - allocate a free block of storage for entry, returning address of
 * new entry */

afs_int32 AllocBlock(at)
  register struct ubik_trans *at;
{
    register afs_int32 code;
    afs_int32 temp;
    struct prentry tentry;

    if (cheader.freePtr) {
	/* allocate this dude */
	temp = ntohl(cheader.freePtr);      
	code = pr_ReadEntry(at, 0, temp, &tentry);
	if (code) return 0;
	cheader.freePtr = htonl(tentry.next);
	code = pr_Write(at, 0, 8, (char *)&cheader.freePtr, sizeof(cheader.freePtr));
	if (code != 0) return 0;
	return temp;
    }
    else {
	/* hosed, nothing on free list, grow file */
	temp = ntohl(cheader.eofPtr);	/* remember this guy */
	cheader.eofPtr = htonl(temp + ENTRYSIZE);
	code = pr_Write(at, 0, 12,(char *) &cheader.eofPtr, sizeof(cheader.eofPtr));
	if (code != 0) return 0;
	return temp;
    }
}

afs_int32 FreeBlock(at, pos)
register struct ubik_trans *at;
afs_int32 pos;
{
    /* add a block of storage to the free list */
    register afs_int32 code;
    struct prentry tentry;

    bzero(&tentry,sizeof(tentry));
    tentry.next = ntohl(cheader.freePtr);
    tentry.flags |= PRFREE;
    cheader.freePtr = htonl(pos);
    code = pr_Write(at,0,8, (char *) &cheader.freePtr,sizeof(cheader.freePtr));
    if (code != 0) return code;
    code = pr_WriteEntry(at,0,pos,&tentry);
    if (code != 0) return code;
    return PRSUCCESS;
}

afs_int32 FindByID(at,aid)
register struct ubik_trans *at;
afs_int32 aid;
{
    /* returns address of entry if found, 0 otherwise */
    register afs_int32 code;
    afs_int32 i;
    struct prentry tentry;
    afs_int32 entry;

    if ((aid == PRBADID) || (aid == 0)) return 0;
    i = IDHash(aid);
    entry = ntohl(cheader.idHash[i]);
    if (entry == 0) return entry;
    bzero(&tentry,sizeof(tentry));
    code = pr_ReadEntry(at, 0, entry, &tentry);
    if (code != 0) return 0;
    if (aid == tentry.id) return entry;
    entry = tentry.nextID;
    while (entry != NULL) {
	bzero(&tentry,sizeof(tentry));
	code = pr_ReadEntry(at,0,entry,&tentry);
	if (code != 0) return 0;
	if (aid == tentry.id) return entry;
	entry = tentry.nextID;
    }
    return 0;
}
 


afs_int32 FindByName(at,aname, tentryp)    
register struct ubik_trans *at;
char aname[PR_MAXNAMELEN];
struct prentry *tentryp;
{
    /* ditto */
    register afs_int32 code;
    afs_int32 i;
    afs_int32 entry;

    i = NameHash(aname);
    entry = ntohl(cheader.nameHash[i]);
    if (entry == 0) return entry;
    bzero(tentryp,sizeof(struct prentry));
    code = pr_ReadEntry(at, 0, entry, tentryp);
    if (code != 0) return 0;
    if ((strncmp(aname,tentryp->name,PR_MAXNAMELEN)) == 0) return entry;
    entry = tentryp->nextName;
    while (entry != NULL) {
	bzero(tentryp, sizeof(struct prentry));
	code = pr_ReadEntry(at,0,entry, tentryp);
	if (code != 0) return 0;
	if ((strncmp(aname,tentryp->name,PR_MAXNAMELEN)) == 0) return entry;
	entry = tentryp->nextName;
    }
    return 0;
}

afs_int32 AllocID(at,flag,aid)
register struct ubik_trans *at;
afs_int32 flag;
afs_int32 *aid;
{
    /* allocs an id from the proper area of address space, based on flag */
    register afs_int32 code = 1;
    register afs_int32 i = 0;
    register maxcount = 50;  /* to prevent infinite loops */
    
    if (flag & PRGRP) {
	*aid = ntohl(cheader.maxGroup);
	while (code && i<maxcount) {
	    --(*aid);
	    code = FindByID(at,*aid);
	    i++;
	}
	if (code) return PRNOIDS;
	cheader.maxGroup = htonl(*aid);
	code = pr_Write(at,0,16,(char *)&cheader.maxGroup,sizeof(cheader.maxGroup));
	if (code) return PRDBFAIL;
	return PRSUCCESS;
    }
    else if (flag & PRFOREIGN) {
	*aid = ntohl(cheader.maxForeign);
	while (code && i<maxcount) {
	    ++(*aid);
	    code = FindByID(at,*aid);
	    i++;
	}
	if (code) return PRNOIDS;
	cheader.maxForeign = htonl(*aid);
	code = pr_Write(at,0,24,(char *)&cheader.maxForeign,sizeof(cheader.maxForeign));
	if (code) return PRDBFAIL;
	return PRSUCCESS;
    }
    else {
	*aid = ntohl(cheader.maxID);
	while (code && i<maxcount) {
	    ++(*aid);
	    code = FindByID(at,*aid);
	    i++;
	}
	if (code) return PRNOIDS;
	cheader.maxID = htonl(*aid);
	code = pr_Write(at,0,20,(char *)&cheader.maxID,sizeof(cheader.maxID));
	if (code) return PRDBFAIL;
	return PRSUCCESS;
    }
}

afs_int32 IDToName(at, aid, aname)
  register struct ubik_trans *at;
  afs_int32 aid;
  char aname[PR_MAXNAMELEN];
{
    afs_int32 temp;
    struct prentry tentry;
    register afs_int32 code;

    temp = FindByID(at,aid);
    if (temp == 0) return PRNOENT;
    code = pr_Read (at, 0, temp, (char *)&tentry, sizeof(tentry));
    if (code) return code;
    strncpy (aname, tentry.name, PR_MAXNAMELEN);
    return PRSUCCESS;
}

afs_int32 NameToID(at, aname, aid)
register struct ubik_trans *at;
char aname[PR_MAXNAMELEN];
afs_int32 *aid;
{
    register afs_int32 code;
    afs_int32 temp;
    struct prentry tentry;

    temp = FindByName(at,aname, &tentry);
    if (!temp) return PRNOENT;
    *aid = tentry.id;
    return PRSUCCESS;
}

afs_int32 IDCmp(a,b)
afs_int32 *a;
afs_int32 *b;
{
    /* used to sort CPS's so that comparison with acl's is easier */
    if (*a > *b) return 1;
    if (*a == *b) return 0;
    if (*a < *b) return -1;
}

afs_int32 RemoveFromIDHash(tt,aid,loc)
struct ubik_trans *tt;
afs_int32 aid;
afs_int32 *loc;				/* ??? in case ID hashed twice ??? */
{
    /* remove entry designated by aid from id hash table */
    register afs_int32 code;
    afs_int32 current, trail, i;
    struct prentry tentry;
    struct prentry bentry;

    if ((aid == PRBADID) || (aid == 0)) return PRINCONSISTENT;
    i = IDHash(aid);
    current = ntohl(cheader.idHash[i]);
    bzero(&tentry,sizeof(tentry));
    bzero(&bentry,sizeof(bentry));
    trail = 0;
    if (current == NULL) return PRSUCCESS; /* already gone */
    code = pr_ReadEntry(tt,0,current,&tentry);
    if (code) return PRDBFAIL;
    while (aid != tentry.id) {
	trail = current;
	current = tentry.nextID;
	if (current == NULL) break;
	code = pr_ReadEntry(tt,0,current,&tentry);
	if (code) return PRDBFAIL;
    }
    if (current == NULL) return PRSUCCESS;  /* we didn't find him, so he's already gone */
    if (trail == NULL) {
	/* it's the first entry! */
	cheader.idHash[i] = htonl(tentry.nextID);
	code = pr_Write(tt,0,72+HASHSIZE*4+i*4,(char *)&cheader.idHash[i],sizeof(cheader.idHash[i]));
	if (code) return PRDBFAIL;
    }
    else {
	code = pr_ReadEntry(tt,0,trail, &bentry);
	if (code) return PRDBFAIL;
	bentry.nextID = tentry.nextID;
	code = pr_WriteEntry(tt,0,trail,&bentry);
    }
    *loc = current;
    return PRSUCCESS;
}

afs_int32 AddToIDHash(tt, aid, loc)
struct ubik_trans *tt;
afs_int32 aid;
afs_int32 loc;				/* ??? */
{
    /* add entry at loc designated by aid to id hash table */
    register afs_int32 code;
    afs_int32 i;
    struct prentry tentry;

    if ((aid == PRBADID) || (aid == 0)) return PRINCONSISTENT;
    i = IDHash(aid);
    bzero(&tentry,sizeof(tentry));
    code = pr_ReadEntry(tt,0,loc,&tentry);
    if (code) return PRDBFAIL;
    tentry.nextID = ntohl(cheader.idHash[i]);
    cheader.idHash[i] = htonl(loc);
    code = pr_WriteEntry(tt,0,loc,&tentry);
    if (code) return PRDBFAIL;
    code = pr_Write(tt,0,72+HASHSIZE*4+i*4,(char *)&cheader.idHash[i],sizeof(cheader.idHash[i]));
    if (code) return PRDBFAIL;
    return PRSUCCESS;
}

afs_int32 RemoveFromNameHash(tt,aname,loc)
struct ubik_trans *tt;
char *aname;
afs_int32 *loc;
{
    /* remove from name hash */
    register afs_int32 code;
    afs_int32 current, trail, i;
    struct prentry tentry;
    struct prentry bentry;

    i = NameHash(aname);
    current = ntohl(cheader.nameHash[i]);
    bzero(&tentry,sizeof(tentry));
    bzero(&bentry,sizeof(bentry));
    trail = 0;
    if (current == NULL) return PRSUCCESS;  /* already gone */
    code = pr_ReadEntry(tt,0,current,&tentry);
    if (code) return PRDBFAIL;
    while (strcmp(aname,tentry.name)) {
	trail = current;
	current = tentry.nextName;
	if (current == NULL) break;
	code = pr_ReadEntry(tt,0,current,&tentry);
	if (code) return PRDBFAIL;
    }
    if (current == NULL) return PRSUCCESS;  /* we didn't find him, already gone */
    if (trail == NULL) {
	/* it's the first entry! */
	cheader.nameHash[i] = htonl(tentry.nextName);
	code = pr_Write(tt,0,72+i*4,(char *)&cheader.nameHash[i],sizeof(cheader.nameHash[i]));
	if (code) return PRDBFAIL;
    }
    else {
	code = pr_ReadEntry(tt,0,trail, &bentry);
	if (code) return PRDBFAIL;
	bentry.nextName = tentry.nextName;
	code = pr_WriteEntry(tt,0,trail,&bentry);
    }
    *loc = current;
    return PRSUCCESS;
}

afs_int32 AddToNameHash(tt, aname, loc)
struct ubik_trans *tt;
char *aname;
afs_int32 loc;
{
    /* add to name hash */
    register afs_int32 code;
    afs_int32 i;
    struct prentry tentry;

    i = NameHash(aname);
    bzero(&tentry,sizeof(tentry));
    code = pr_ReadEntry(tt,0,loc,&tentry);
    if (code) return PRDBFAIL;
    tentry.nextName = ntohl(cheader.nameHash[i]);
    cheader.nameHash[i] = htonl(loc);
    code = pr_WriteEntry(tt,0,loc,&tentry);
    if (code) return PRDBFAIL;
    code = pr_Write(tt,0,72+i*4,(char *)&cheader.nameHash[i],sizeof(cheader.nameHash[i]));
    if (code) return PRDBFAIL;
    return PRSUCCESS;
}

afs_int32 AddToOwnerChain(at,gid,oid)
  struct ubik_trans *at;
  afs_int32 gid;
  afs_int32 oid;
{
    /* add entry designated by gid to owner chain of entry designated by oid */
    register afs_int32 code;
    afs_int32 loc;
    struct prentry tentry;
    struct prentry gentry;
    afs_int32 gloc;

    loc = FindByID(at,oid);
    if (!loc) return PRNOENT;
    code = pr_ReadEntry(at,0,loc,&tentry);
    if (code != 0) return PRDBFAIL;
    if (oid == gid) {			/* added it to its own chain */
	tentry.nextOwned = tentry.owned;
	tentry.owned = loc;
    } else {
	gloc = FindByID(at,gid);
	code = pr_ReadEntry(at,0,gloc,&gentry);
	if (code != 0) return PRDBFAIL;
	gentry.nextOwned = tentry.owned;
	tentry.owned = gloc;
	code = pr_WriteEntry(at,0,gloc,&gentry);
	if (code != 0) return PRDBFAIL;
    }
    code = pr_WriteEntry(at,0,loc,&tentry);
    if (code != 0) return PRDBFAIL;
    return PRSUCCESS;
}

/* RemoveFromOwnerChain - remove gid from owner chain for oid */

afs_int32 RemoveFromOwnerChain(at,gid,oid)
  struct ubik_trans *at;
  afs_int32 gid;
  afs_int32 oid;
{
    register afs_int32 code;
    afs_int32 nptr;
    struct prentry thisEntry;
    struct prentry thatEntry;
    struct prentry *te;			/* pointer to current (this) entry */
    struct prentry *le;			/* pointer to previous (last) entry */
    afs_int32 loc, lastLoc;

    loc = FindByID(at,oid);
    if (!loc) return PRNOENT;
    code = pr_ReadEntry (at, 0, loc, &thisEntry);
    if (code != 0) return PRDBFAIL;
    le =  &thisEntry;
    lastLoc = 0;
    nptr = thisEntry.owned;
    while (nptr != NULL) {
	if (nptr == lastLoc) te = le;
	else {
	    if (&thisEntry == le) te = &thatEntry;
	    else te = &thisEntry;
	    code = pr_ReadEntry (at, 0, nptr, te);
	    if (code != 0) return PRDBFAIL;
	}
	if (te->id == gid) {
	    /* found it */
	    if (lastLoc == 0) {		/* modifying first of chain */
		le->owned = te->nextOwned;
		lastLoc = loc;		/* so we write to correct location */
	    }
	    else le->nextOwned = te->nextOwned;
	    te->nextOwned = 0;
	    if (te != le) {
		code = pr_WriteEntry (at, 0, nptr, te);
		if (code != 0) return PRDBFAIL;
	    }
	    code = pr_WriteEntry (at, 0, lastLoc, le);
	    if (code != 0) return PRDBFAIL;
	    return PRSUCCESS;
	}
	lastLoc = nptr;
	le = te;
	nptr = te->nextOwned;
    }
    return PRSUCCESS;			/* already removed? */
}

/* AddToOrphan - add gid to orphan list, as it's owner has died */

afs_int32 AddToOrphan(at,gid)
  struct ubik_trans *at;
  afs_int32 gid;
{
    register afs_int32 code;
    afs_int32 loc;
    struct prentry tentry;

    loc = FindByID(at,gid);
    if (!loc) return PRNOENT;
    code = pr_ReadEntry(at,0,loc,&tentry);
    if (code != 0) return PRDBFAIL;
    tentry.nextOwned = ntohl(cheader.orphan);
    code = set_header_word (at, orphan, htonl(loc));
    if (code != 0) return PRDBFAIL;
    tentry.owner = 0;			/* so there's no confusion later */
    code = pr_WriteEntry(at,0,loc,&tentry);
    if (code != 0) return PRDBFAIL;
    return PRSUCCESS;
}

afs_int32 RemoveFromOrphan(at,gid)
struct ubik_trans *at;
afs_int32 gid;
{
    /* remove gid from the orphan list */
    register afs_int32 code;
    afs_int32 loc;
    afs_int32 nptr;
    struct prentry tentry;
    struct prentry bentry;

    loc = FindByID(at,gid);
    if (!loc) return PRNOENT;
    code = pr_ReadEntry(at,0,loc,&tentry);
    if (code != 0) return PRDBFAIL;
    if (cheader.orphan == htonl(loc)) {
	cheader.orphan = htonl(tentry.nextOwned);
	tentry.nextOwned = 0;
	code = pr_Write(at,0,32,(char *)&cheader.orphan,sizeof(cheader.orphan));
	if (code != 0) return PRDBFAIL;
	code = pr_WriteEntry(at,0,loc,&tentry);
	if (code != 0) return PRDBFAIL;
	return PRSUCCESS;
    }
    nptr = ntohl(cheader.orphan);
    bzero(&bentry,sizeof(bentry));
    loc = 0;
    while (nptr != NULL) {
	code = pr_ReadEntry(at,0,nptr,&tentry);
	if (code != 0) return PRDBFAIL;
	if (gid == tentry.id) {
	    /* found it */
	    bentry.nextOwned = tentry.nextOwned;
	    tentry.nextOwned = 0;
	    code = pr_WriteEntry(at,0,loc,&bentry);
	    if (code != 0) return PRDBFAIL;
	    code = pr_WriteEntry(at,0,nptr,&tentry);
	    if (code != 0) return PRDBFAIL;
	    return PRSUCCESS;
	}
	loc = nptr;
	nptr = tentry.nextOwned;
	bcopy(&tentry,&bentry, sizeof(tentry));
    }
    return PRSUCCESS;
}

afs_int32 IsOwnerOf(at,aid,gid)
struct ubik_trans *at;
afs_int32 aid;
afs_int32 gid;
{
    /* returns 1 if aid is the owner of gid, 0 otherwise */
    register afs_int32 code;
    struct prentry tentry;
    afs_int32 loc;

    loc = FindByID(at,gid);
    if (!loc) return 0;
    code = pr_ReadEntry(at,0,loc,&tentry);
    if (code != 0) return 0;
    if (tentry.owner == aid) return 1;
    return 0;
}

afs_int32 OwnerOf(at,gid)
struct ubik_trans *at;
afs_int32 gid;
{
    /* returns the owner of gid */
    register afs_int32 code;
    afs_int32 loc;
    struct prentry tentry;

    loc = FindByID(at,gid);
    if (!loc) return 0;
    code = pr_ReadEntry(at,0,loc,&tentry);
    if (code != 0) return 0;
    return tentry.owner;
}
    

afs_int32 IsAMemberOf(at,aid,gid)
struct ubik_trans *at;
afs_int32 aid;
afs_int32 gid;
{
    /* returns true if aid is a member of gid */
    struct prentry tentry;
    struct contentry centry;
    register afs_int32 code;
    afs_int32 i;
    afs_int32 loc;

    /* special case anyuser and authuser */
    if (gid == ANYUSERID) return 1;
    if (gid == AUTHUSERID && aid != ANONYMOUSID) return 1;
    if ((gid == 0) || (aid == 0)) return 0;
    loc = FindByID(at,gid);
    if (!loc) return 0;
    bzero(&tentry,sizeof(tentry));
    code = pr_ReadEntry(at, 0, loc,&tentry);
    if (code) return 0;
    if (!(tentry.flags & PRGRP)) return 0;
    for (i= 0;i<PRSIZE;i++) {
	if (tentry.entries[i] == 0) return 0;
	if (tentry.entries[i] == aid) return 1;
    }
    if (tentry.next) {
	loc = tentry.next;
	while (loc) {
	    bzero(&centry,sizeof(centry));
	    code = pr_ReadCoEntry(at,0,loc,&centry);
	    if (code) return 0;
	    for (i=0;i<COSIZE;i++) {
		if (centry.entries[i] == aid) return 1;
		if (centry.entries[i] == 0) return 0;
	    }
	    loc = centry.next;
	}
    }
    return 0;  /* actually, should never get here */
}
