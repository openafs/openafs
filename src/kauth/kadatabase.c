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

RCSID
    ("$Header$");

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <ubik.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afsutil.h>
#include "kauth.h"
#include "kautils.h"
#include "kaserver.h"

extern Date cheaderReadTime;	/* time cheader last read in */

#define set_header_word(tt,field,value) kawrite ((tt), ((char *)&(cheader.field) - (char *)&cheader), ((cheader.field = (value)), (char *)&(cheader.field)), sizeof(afs_int32))

#define inc_header_word(tt,field) kawrite ((tt), ((char *)&(cheader.field) - (char *)&cheader), ((cheader.field = (htonl(ntohl(cheader.field)+1))), (char *)&(cheader.field)), sizeof(afs_int32))

static int index_OK();

afs_int32
NameHash(aname, ainstance)
     register char *aname;
     register char *ainstance;
{
    register unsigned int hash;
    register int i;

    /* stolen directly from the HashString function in the vol package */
    hash = 0;
    for (i = strlen(aname), aname += i - 1; i--; aname--)
	hash = (hash * 31) + (*((unsigned char *)aname) - 31);
    for (i = strlen(ainstance), ainstance += i - 1; i--; ainstance--)
	hash = (hash * 31) + (*((unsigned char *)ainstance) - 31);
    return (hash % HASHSIZE);
}

/* package up seek and write into one procedure for ease of use */

afs_int32
kawrite(tt, pos, buff, len)
     struct ubik_trans *tt;
     afs_int32 pos;
     char *buff;
     afs_int32 len;
{
    afs_int32 code;

    code = ubik_Seek(tt, 0, pos);
    if (code)
	return code;
    code = ubik_Write(tt, buff, len);
    return code;
}

/* same thing for read */

afs_int32
karead(tt, pos, buff, len)
     struct ubik_trans *tt;
     afs_int32 pos;
     char *buff;
     afs_int32 len;
{
    afs_int32 code;

    code = ubik_Seek(tt, 0, pos);
    if (code)
	return code;
    code = ubik_Read(tt, buff, len);
    return code;
}

static struct Lock cheader_lock;
static struct Lock keycache_lock;

static int maxCachedKeys;

static struct cachedKey {
    Date used;
    int superseded;		/* NEVERDATE => this is current key */
    afs_int32 kvno;
    struct ktc_encryptionKey key;
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
} *keyCache;
static afs_int32 keyCacheVersion = 0;

static afs_int32 maxKeyLifetime;
static int dbfixup = 0;

init_kadatabase(initFlags)
     int initFlags;		/* same as init_kaprocs (see which) */
{
    Lock_Init(&cheader_lock);
    Lock_Init(&keycache_lock);

    maxCachedKeys = 10;
    keyCache =
	(struct cachedKey *)malloc(maxCachedKeys * sizeof(struct cachedKey));
    keyCacheVersion = 0;
    if (initFlags & 4) {
	maxKeyLifetime = 90;
    } else {
	maxKeyLifetime = MAXKTCTICKETLIFETIME;
    }
    if (initFlags & 8)
	dbfixup++;
}

/* check that the database has been initialized.  Be careful to fail in a safe
   manner, to avoid bogusly reinitializing the db.  */

afs_int32
CheckInit(at, db_init)
     struct ubik_trans *at;
     int (*db_init) ();		/* procedure to call if rebuilding DB */
{
    register afs_int32 code;
    afs_int32 iversion;
    afs_int32 tversion;

    /* Don't read header if not necessary */
    if (!ubik_CacheUpdate(at))
	return 0;

    ObtainWriteLock(&cheader_lock);
    if ((code = karead(at, 0, (char *)&iversion, sizeof(iversion)))
	|| (code =
	    karead(at, sizeof(cheader) - sizeof(afs_int32), (char *)&tversion,
		   sizeof(afs_int32)))) {
	if (code == UEOF)
	    printf("No data base\n");
	else
	    printf("I/O Error\n");
    } else {
	iversion = ntohl(iversion);	/* convert to host order */
	tversion = ntohl(tversion);
	if ((iversion == KADBVERSION) && (tversion == KADBVERSION)) {
	    code = karead(at, 0, (char *)&cheader, sizeof(cheader));
	    if (code) {
		printf("SetupHeader failed\n");
		code = KAIO;
	    } else {
		cheaderReadTime = time(0);
	    }
	} else {
	    printf("DB version should be %d; Initial = %d; Terminal = %d\n",
		   KADBVERSION, iversion, tversion);
	    code = KAIO;
	}
    }
    ReleaseWriteLock(&cheader_lock);
    if (code == 0)
	return 0;

    /* if here, we have no version number or the wrong version number in the
     * file */
    if ((code == UEOF) || ((iversion == 0) && (tversion == 0)))
	code = KAEMPTY;
    else
	code = KAIO;

    if ((db_init == 0) || (code == KAIO))
	return code;

    printf("Error discovered in header, rebuilding.\n");

    /* try to write a good header */
    memset(&cheader, 0, sizeof(cheader));
    cheader.version = htonl(KADBVERSION);
    cheader.checkVersion = htonl(KADBVERSION);
    cheader.headerSize = htonl(sizeof(cheader));
    cheader.freePtr = 0;
    cheader.eofPtr = htonl(sizeof(cheader));
    cheader.kvnoPtr = 0;
    cheader.specialKeysVersion = htonl(time(0));	/* anything non-zero will do */
    cheader.stats.cpws = cheader.stats.allocs = cheader.stats.frees = 0;
    cheader.admin_accounts = 0;
    cheader.hashsize = htonl(HASHSIZE);
    code = kawrite(at, 0, (char *)&cheader, sizeof(cheader));
    if (code)
	return KAIO;		/* return the error code */

    return db_init(at);		/* initialize the db */
}

/* Allocate a free block of storage for entry, returning address of a new
   zeroed entry.  If zero is returned, a Ubik I/O error can be assumed.  */

afs_int32
AllocBlock(at, tentry)
     register struct ubik_trans *at;
     struct kaentry *tentry;
{
    register afs_int32 code;
    afs_int32 temp;

    if (cheader.freePtr) {
	/* allocate this dude */
	temp = ntohl(cheader.freePtr);
	code = karead(at, temp, (char *)tentry, sizeof(kaentry));
	if (code)
	    return 0;		/* can't read block */
	code = set_header_word(at, freePtr, tentry->next);
    } else {
	/* hosed, nothing on free list, grow file */
	temp = ntohl(cheader.eofPtr);	/* remember this guy */
	code = set_header_word(at, eofPtr, htonl(temp + sizeof(kaentry)));
    }
    if (code)
	return 0;

    code = inc_header_word(at, stats.allocs);
    if (code)
	return 0;
    memset(tentry, 0, sizeof(kaentry));	/* zero new entry */
    return temp;
}

/* Free a block given its index.  It must already have been unthreaded.
   Returns zero for success or an error code on failure. */

afs_int32
FreeBlock(at, index)
     struct ubik_trans *at;
     afs_int32 index;
{
    struct kaentry tentry;
    int code;

    /* check index just to be on the safe side */
    if (!index_OK(index))
	return KABADINDEX;

    memset(&tentry, 0, sizeof(kaentry));
    tentry.next = cheader.freePtr;
    tentry.flags = htonl(KAFFREE);
    code = set_header_word(at, freePtr, htonl(index));
    if (code)
	return KAIO;
    code = kawrite(at, index, (char *)&tentry, sizeof(kaentry));
    if (code)
	return KAIO;

    code = inc_header_word(at, stats.frees);
    if (code)
	return KAIO;
    return 0;
}

/* Look for a block by name and instance.  If found read the block's contents
   into the area pointed to by tentry and return the block's index.  If not
   found offset is set to zero.  If an error is encountered a non-zero code is
   returned. */

afs_int32
FindBlock(at, aname, ainstance, toP, tentry)
     struct ubik_trans *at;
     char *aname;
     char *ainstance;
     afs_int32 *toP;
     struct kaentry *tentry;
{
    register afs_int32 i, code;
    register afs_int32 to;

    *toP = 0;
    i = NameHash(aname, ainstance);
    for (to = ntohl(cheader.nameHash[i]); to != NULLO;
	 to = ntohl(tentry->next)) {
	code = karead(at, to, (char *)tentry, sizeof(kaentry));
	if (code)
	    return code;
	/* see if the name matches */
	if (!strcmp(aname, tentry->userID.name)
	    && (ainstance == (char *)0
		|| !strcmp(ainstance, tentry->userID.instance))) {
	    *toP = to;		/* found it */
	    return 0;
	}
    }
    *toP = 0;			/* no such entry */
    return 0;
}

/* Add a block to the hash table given a pointer to the block and its index.
   The block is threaded onto the hash table and written to disk.  The routine
   returns zero if there were no errors. */

afs_int32
ThreadBlock(at, index, tentry)
     struct ubik_trans *at;
     afs_int32 index;
     struct kaentry *tentry;
{
    int code;
    int hi;			/* hash index */

    if (!index_OK(index))
	return KABADINDEX;
    hi = NameHash(tentry->userID.name, tentry->userID.instance);
    tentry->next = cheader.nameHash[hi];
    code = set_header_word(at, nameHash[hi], htonl(index));
    if (code)
	return KAIO;
    code = kawrite(at, index, (char *)tentry, sizeof(kaentry));
    if (code)
	return KAIO;
    return 0;
}

/* Remove a block from the hash table.  If success return 0, else return an
   error code. */

afs_int32
UnthreadBlock(at, aentry)
     struct ubik_trans *at;
     struct kaentry *aentry;
{
    register afs_int32 i, code;
    register afs_int32 to;
    afs_int32 lo;
    struct kaentry tentry;

    i = NameHash(aentry->userID.name, aentry->userID.instance);
    lo = 0;
    for (to = ntohl(cheader.nameHash[i]); to != NULLO;
	 to = ntohl(tentry.next)) {
	code = karead(at, to, (char *)&tentry, sizeof(kaentry));
	if (code)
	    return KAIO;
	/* see if the name matches */
	if (!strcmp(aentry->userID.name, tentry.userID.name)
	    && !strcmp(aentry->userID.instance, tentry.userID.instance)) {
	    /* found it */
	    if (lo) {		/* unthread from last block */
		code =
		    kawrite(at, lo, (char *)&tentry.next, sizeof(afs_int32));
		if (code)
		    return KAIO;
	    } else {		/* unthread from hash table */
		code = set_header_word(at, nameHash[i], tentry.next);
		if (code)
		    return KAIO;
	    }
	    aentry->next = 0;	/* just to be sure */
	    return 0;
	}
	lo = DOFFSET(to, &tentry, &tentry.next);
    }
    return KANOENT;
}

/* Given an index to the last block (or zero the first time) read the contents
   of the next block and return its index.  The last argument is a pointer to
   an estimate of the number of remaining blocks to read out.  The remaining
   count is an estimate because it may include free blocks that are not
   returned.  If there are no more blocks remaining is zero and the returned
   index is zero.  A non-zero index indicates that tentry has been filled with
   valid data.  If an error is encountered the returned index is zero and the
   remaining count is negative.  */

afs_int32
NextBlock(at, index, tentry, remaining)
     struct ubik_trans *at;
     afs_int32 index;
     struct kaentry *tentry;
     afs_int32 *remaining;
{
    int code;
    afs_int32 last;

    if (index == 0)		/* get first one */
	index = sizeof(cheader);
    else {
	if (!index_OK(index)) {
	    *remaining = -1;	/* error */
	    return 0;
	}
	index += sizeof(kaentry);
    }
    /* now search for the first entry that isn't free */
    for (last = ntohl(cheader.eofPtr); index < last; index += sizeof(kaentry)) {
	code = karead(at, index, (char *)tentry, sizeof(kaentry));
	if (code) {
	    *remaining = -1;
	    return 0;
	}
	if (!(ntohl(tentry->flags) & (KAFFREE | KAFOLDKEYS))) {
	    /* estimate remaining number of entries, not including this one */
	    *remaining = (last - index) / sizeof(kaentry) - 1;
	    return index;
	}
    }
    *remaining = 0;		/* no more entries */
    return 0;
}

/* These are a collections of routines that deal with externally known keys.
   They maintain a database of key version numbers and the corresponding key
   and pointer to the user entry. */

afs_int32
ka_NewKey(tt, tentryaddr, tentry, key)
     struct ubik_trans *tt;
     afs_int32 tentryaddr;
     struct kaentry *tentry;
     struct ktc_encryptionKey *key;
{
    struct kaOldKeys okeys;	/* old keys block */
    afs_int32 okeysaddr, nextaddr;	/* offset of old keys block */
    afs_int32 prevptr, nextprevptr;
    int code, i;
    Date now = time(0);
    afs_int32 newkeyver;	/* new key version number */
    afs_int32 newtotalkeyentries = 0, oldtotalkeyentries = 0, keyentries;
    int foundcurrentkey = 0, addednewkey = 0, modified;

    es_Report("Newkey for %s.%s\n", tentry->userID.name,
	      tentry->userID.instance);

    newkeyver = ntohl(tentry->key_version) + 1;
    if ((newkeyver < 1) || (newkeyver >= MAXKAKVNO))
	newkeyver = 1;

    /* An entry may have more than one oldkeys blocks. The entry
     * points to the most current, but all the oldkeys blocks for an
     * entry are not linked together. All oldkeys blocks for all
     * entries are linked together off of the header. So we follow
     * this link.
     */
    for (prevptr = 0, okeysaddr = ntohl(cheader.kvnoPtr); okeysaddr;
	 prevptr = nextprevptr, okeysaddr = nextaddr) {
	/* foreacholdkeysblock */
	/* Read the oldKeys block */
	code = karead(tt, okeysaddr, (char *)&okeys, sizeof(okeys));
	if (code)
	    return code;

	nextaddr = ntohl(okeys.next);
	nextprevptr = DOFFSET(okeysaddr, &okeys, &okeys.next);

	/* We only want oldkey blocks that belong to this entry */
	if (ntohl(okeys.entry) != tentryaddr)
	    continue;

	modified = 0;		/* This oldkeys block has not been modified */
	keyentries = 0;		/* Number of valid key entries in the block */
	for (i = 0; i < NOLDKEYS; i++) {
	    /* foreachkey */
	    /* Keep count of number of entries found */
	    if (okeys.keys[i].superseded != 0) {
		oldtotalkeyentries++;
	    }

	    /* If we find the entry that is not superseded, then supersede it */
	    if (ntohl(okeys.keys[i].superseded) == NEVERDATE) {
		okeys.keys[i].superseded = htonl(now);
		modified = 1;
#ifdef AUTH_DBM_LOG
		if (foundcurrentkey) {
		    ViceLog(0,
			    ("Warning: Entry %s.%s contains more than one valid key: fixing\n",
			     tentry->userID.name, tentry->userID.instance));
		}
#endif
		foundcurrentkey = 1;
	    }

	    /* If we find an oldkey of the same version or
	     * an old key that has expired, then delete it.
	     */
	    if ((ntohl(okeys.keys[i].version) == newkeyver)
		|| ((now - ntohl(okeys.keys[i].superseded) > maxKeyLifetime))) {
		okeys.keys[i].superseded = 0;
		okeys.keys[i].version = htonl(-1);
		memset(&okeys.keys[i].key, 0,
		       sizeof(struct ktc_encryptionKey));
		modified = 1;

		es_Report("Dropped oldkey %d seconds old with kvno %d\n",
			  now - ntohl(okeys.keys[i].superseded),
			  ntohl(okeys.keys[i].version));
	    }

	    /* Add our key here if its free */
	    if (!addednewkey && (okeys.keys[i].superseded == 0)) {
		okeys.keys[i].version = htonl(newkeyver);
		okeys.keys[i].superseded = htonl(NEVERDATE);
		memcpy(&okeys.keys[i].key, key,
		       sizeof(struct ktc_encryptionKey));
		modified = 1;
		addednewkey = okeysaddr;
	    }

	    /* Keep count of number of entries found */
	    if (okeys.keys[i].superseded != 0) {
		keyentries++;
		newtotalkeyentries++;
	    }
	}			/* foreachkey */

	/* If we modified the block, write it out */
	if (modified && keyentries) {
	    code = kawrite(tt, okeysaddr, (char *)&okeys, sizeof(okeys));
	    if (code)
		return code;
	}

	/* If there are no more entries in this oldkeys block, delete it */
	if (keyentries == 0) {
	    if (!prevptr) {
		code = set_header_word(tt, kvnoPtr, okeys.next);
	    } else {
		code =
		    kawrite(tt, prevptr, (char *)&okeys.next,
			    sizeof(afs_int32));
	    }
	    if (code)
		return code;
	    code = FreeBlock(tt, okeysaddr);
	    if (code)
		return code;

	    nextprevptr = prevptr;	/* won't bump prevptr */
	}
    }				/* foreacholdkeysblock */

    /* If we could not add the key, create a new oldkeys block */
    if (!addednewkey) {
	/* Allocate and fill in an oldkeys block */
	addednewkey = AllocBlock(tt, (struct kaentry *)&okeys);
	if (!addednewkey)
	    return KACREATEFAIL;
	okeys.flags = htonl(KAFOLDKEYS);
	okeys.entry = htonl(tentryaddr);
	okeys.keys[0].version = htonl(newkeyver);
	okeys.keys[0].superseded = htonl(NEVERDATE);
	memcpy(&okeys.keys[0].key, key, sizeof(struct ktc_encryptionKey));
	newtotalkeyentries++;

	/* Thread onto the header's chain of oldkeys */
	okeys.next = cheader.kvnoPtr;
	code = set_header_word(tt, kvnoPtr, htonl(addednewkey));
	if (code)
	    return code;

	/* Write the oldkeys block out */
	code = kawrite(tt, addednewkey, (char *)&okeys, sizeof(okeys));
	if (code)
	    return code;

	es_Report("New oldkey block allocated at %d\n", addednewkey);
    }
#ifdef AUTH_DBM_LOG
    if (oldtotalkeyentries != ntohl(tentry->misc.asServer.nOldKeys)) {
	ViceLog(0,
		("Warning: Entry %s.%s reports %d oldkeys, found %d: fixing\n",
		 tentry->userID.name, tentry->userID.instance,
		 ntohl(tentry->misc.asServer.nOldKeys), oldtotalkeyentries));
    }
#endif

    /* Update the tentry. We rely on caller to write it out */
    tentry->misc.asServer.oldKeys = htonl(addednewkey);
    tentry->misc.asServer.nOldKeys = htonl(newtotalkeyentries);
    tentry->key_version = htonl(newkeyver);
    memcpy(&tentry->key, key, sizeof(tentry->key));

    /* invalidate key caches everywhere */
    code = inc_header_word(tt, specialKeysVersion);
    if (code)
	return code;

    es_Report("New kvno is %d, now are %d oldkeys\n", newkeyver,
	      newtotalkeyentries);
    return 0;
}

afs_int32
ka_DelKey(tt, tentryaddr, tentry)
     struct ubik_trans *tt;
     afs_int32 tentryaddr;
     struct kaentry *tentry;
{
    int code;
    struct kaOldKeys okeys;	/* old keys block */
    afs_int32 okeysaddr, nextaddr;	/* offset of old keys block */
    afs_int32 prevptr = 0;
    Date now = time(0);

    es_Report("DelKey for %s.%s\n", tentry->userID.name,
	      tentry->userID.instance);

    /* An entry may have more than one oldkeys blocks. The entry
     * points to the most current, but all the oldkeys blocks for an
     * entry are not linked together. All oldkeys blocks for all
     * entries are linked together off of the header. So we follow
     * this link.
     */
    for (okeysaddr = ntohl(cheader.kvnoPtr); okeysaddr; okeysaddr = nextaddr) {
	/* foreacholdkeysblock */
	/* Read the oldKeys block */
	code = karead(tt, okeysaddr, (char *)&okeys, sizeof(okeys));
	if (code)
	    return code;
	nextaddr = ntohl(okeys.next);

	/* We only want oldkey blocks that belong to this entry */
	if (ntohl(okeys.entry) != tentryaddr) {
	    prevptr = DOFFSET(okeysaddr, &okeys, &okeys.next);
	    continue;
	}

	/* Delete the oldkeys block */
	if (prevptr) {
	    code =
		kawrite(tt, prevptr, (char *)&okeys.next, sizeof(afs_int32));
	} else {
	    code = set_header_word(tt, kvnoPtr, okeys.next);
	}
	if (code)
	    return code;
	code = FreeBlock(tt, okeysaddr);
	if (code)
	    return code;
    }				/* foreacholdkeysblock */

    /* Update the tentry. We rely on caller to write it out */
    tentry->misc.asServer.oldKeys = 0;
    tentry->misc.asServer.nOldKeys = 0;

    /* invalidate key caches everywhere */
    code = inc_header_word(tt, specialKeysVersion);
    if (code)
	return code;

    return 0;
}

void
ka_debugKeyCache(info)
     struct ka_debugInfo *info;
{
    int i;

    memcpy(&info->cheader_lock, &cheader_lock, sizeof(info->cheader_lock));
    memcpy(&info->keycache_lock, &keycache_lock, sizeof(info->keycache_lock));

    info->kcVersion = keyCacheVersion;
    info->kcSize = maxCachedKeys;
    info->kcUsed = 0;
    for (i = 0; i < maxCachedKeys; i++) {
	if (keyCache[i].used) {
	    if (info->kcUsed < KADEBUGKCINFOSIZE) {
		int j = info->kcUsed;
		char principal[sizeof(keyCache[0].name) +
			       sizeof(keyCache[0].inst)];

		info->kcInfo[j].used = keyCache[i].superseded;
		info->kcInfo[j].kvno = keyCache[i].kvno;
		info->kcInfo[j].primary =
		    (keyCache[i].superseded == NEVERDATE);
		info->kcInfo[j].keycksum = 0;
#if DEBUG_KEY_CACHE
		{
		    int k;
		    for (k = 0; k < sizeof(struct ktc_encryptionKey); k++)
			info->kcInfo[j].keycksum +=
			    ((char *)&keyCache[i].key)[k];
		}
#endif
		strcpy(principal, keyCache[i].name);
		strcat(principal, ".");
		strcat(principal, keyCache[i].inst);
		strncpy(info->kcInfo[j].principal, principal,
			sizeof(info->kcInfo[0].principal));
	    }
	    info->kcUsed++;
	}
    }
}

/* Add a key to the key cache, expanding it if necessary. */

void
ka_Encache(name, inst, kvno, key, superseded)
     char *name;
     char *inst;
     afs_int32 kvno;
     struct ktc_encryptionKey *key;
     Date superseded;
{
    int i;

    ObtainWriteLock(&keycache_lock);
    if (keyCacheVersion != ntohl(cheader.specialKeysVersion)) {
	for (i = 0; i < maxCachedKeys; i++)
	    keyCache[i].used = 0;
    }

    for (i = 0; i < maxCachedKeys; i++)
	if (keyCache[i].used == 0) {
	  encache:
	    keyCache[i].kvno = kvno;
	    strncpy(keyCache[i].name, name, sizeof(keyCache[i].name));
	    strncpy(keyCache[i].inst, inst, sizeof(keyCache[i].inst));
	    keyCacheVersion = ntohl(cheader.specialKeysVersion);
	    memcpy(&keyCache[i].key, key, sizeof(*key));
	    keyCache[i].superseded = superseded;
	    keyCache[i].used = time(0);

	    ReleaseWriteLock(&keycache_lock);
	    return;
	}
    /* i == maxCachedKeys */
    keyCache =
	(struct cachedKey *)realloc(keyCache,
				    (maxCachedKeys *=
				     2) * sizeof(struct cachedKey));
    if (keyCache == 0) {
	es_Report("Can't realloc keyCache! out of memory?");
	exit(123);
    }

    {
	int j = i;		/* initialize new storage */
	while (j < maxCachedKeys)
	    keyCache[j++].used = 0;
    }
    goto encache;
}

/* Look up the key given a principal and a kvno.  This is called by GetTicket
   to get the decryption key for the authenticating ticket.  It is also called
   by the rxkad security module to decrypt admin tickets.  The rxkad call is
   with tt==0, since Rx can't call Ubik. */

afs_int32
ka_LookupKvno(tt, name, inst, kvno, key)
     struct ubik_trans *tt;
     char *name;
     char *inst;
     afs_int32 kvno;
     struct ktc_encryptionKey *key;
{
    int i;
    int code = 0;
    afs_int32 to;
    struct kaentry tentry;
    afs_int32 ko;
    struct kaOldKeys okeys;

    ObtainReadLock(&keycache_lock);
    if (keyCacheVersion != ntohl(cheader.specialKeysVersion))
	code = KAKEYCACHEINVALID;
    else {
	for (i = 0; i < maxCachedKeys; i++) {
	    if (keyCache[i].used) {	/* zero used date means invalid */
		if ((keyCache[i].kvno == kvno)
		    && (strcmp(keyCache[i].name, name) == 0)
		    && (strcmp(keyCache[i].inst, inst) == 0)) {
		    memcpy(key, &keyCache[i].key, sizeof(*key));
		    keyCache[i].used = time(0);
		    ReleaseReadLock(&keycache_lock);
		    return 0;
		}
	    }
	}
	code = KAUNKNOWNKEY;
    }
    ReleaseReadLock(&keycache_lock);
    if (!tt)
	return code;

    /* we missed in the cache so need to look in the Ubik database */
    code = FindBlock(tt, name, inst, &to, &tentry);
    if (code)
	return code;
    if (to == 0)
	return KANOENT;

    /* first check the current key */
    if (tentry.key_version == htonl(kvno)) {
	memcpy(key, &tentry.key, sizeof(*key));
	ka_Encache(name, inst, kvno, key, NEVERDATE);
	return 0;
    }
    for (ko = ntohl(cheader.kvnoPtr); ko; ko = ntohl(okeys.next)) {
	code = karead(tt, ko, (char *)&okeys, sizeof(okeys));
	if (code)
	    return KAIO;
	if (ntohl(okeys.entry) == to)
	    for (i = 0; i < NOLDKEYS; i++)
		if (okeys.keys[i].superseded
		    && (ntohl(okeys.keys[i].version) == kvno)) {
		    memcpy(key, &okeys.keys[i].key, sizeof(*key));
		    ka_Encache(name, inst, kvno, key,
			       ntohl(okeys.keys[i].superseded));
		    return 0;
		}
    }
    return KAUNKNOWNKEY;
}

/* Look up the primary key and key version for a principal. */

afs_int32
ka_LookupKey(tt, name, inst, kvno, key)
     struct ubik_trans *tt;
     char *name;
     char *inst;
     afs_int32 *kvno;		/* returned */
     struct ktc_encryptionKey *key;	/* copied out */
{
    int i;
    afs_int32 to;
    struct kaentry tentry;
    afs_int32 code = 0;

    ObtainReadLock(&keycache_lock);
    if (keyCacheVersion != ntohl(cheader.specialKeysVersion))
	code = KAKEYCACHEINVALID;
    else {
	for (i = 0; i < maxCachedKeys; i++) {
	    if (keyCache[i].used) {	/* zero used date means invalid */
		if ((keyCache[i].superseded == NEVERDATE)
		    && (strcmp(keyCache[i].name, name) == 0)
		    && (strcmp(keyCache[i].inst, inst) == 0)) {
		    memcpy(key, &keyCache[i].key, sizeof(*key));
		    *kvno = keyCache[i].kvno;
		    keyCache[i].used = time(0);
		    ReleaseReadLock(&keycache_lock);
		    return 0;
		}
	    }
	}
	code = KAUNKNOWNKEY;
    }
    ReleaseReadLock(&keycache_lock);
    if (!tt)
	return code;

    /* we missed in the cache so need to look in the Ubik database */
    code = FindBlock(tt, name, inst, &to, &tentry);
    if (code)
	return code;
    if (to == 0)
	return KANOENT;
    memcpy(key, &tentry.key, sizeof(*key));
    *kvno = ntohl(tentry.key_version);
    ka_Encache(name, inst, *kvno, key, NEVERDATE);
    return 0;
}

/* This is, hopefully a temporary mechanism to fill the cache will all keys
   since filling cache misses during rxkad challenge responses will deadlock if
   Ubik needs to use Rx. */

afs_int32
ka_FillKeyCache(tt)
     struct ubik_trans *tt;
{
    int nfound;
    afs_int32 ko;
    int code;
    int i;
    struct ktc_encryptionKey k;
    struct kaOldKeys okeys;
    struct kaentry tentry;

    /* this is a little marginal, but... */
    if (keyCacheVersion == ntohl(cheader.specialKeysVersion))
	return 0;

    nfound = 0;
    for (ko = ntohl(cheader.kvnoPtr); ko; ko = ntohl(okeys.next)) {
	code = karead(tt, ko, (char *)&okeys, sizeof(okeys));
	if (code)
	    return KAIO;
	/* get name & instance */
	code =
	    karead(tt, ntohl(okeys.entry), (char *)&tentry, sizeof(tentry));
	if (code)
	    return KAIO;

	/* get all the old keys in this block */
	for (i = 0; i < NOLDKEYS; i++)
	    if (okeys.keys[i].superseded) {
		code =
		    ka_LookupKvno(tt, tentry.userID.name,
				  tentry.userID.instance,
				  ntohl(okeys.keys[i].version), &k);
		if (code)
		    return code;
	    }
    }
    if (++nfound > maxCachedKeys)
	return KADATABASEINCONSISTENT;
    return 0;
}

afs_int32
update_admin_count(tt, delta)
     struct ubik_trans *tt;
     int delta;
{
    afs_int32 to;
    afs_int32 code;

    cheader.admin_accounts = htonl(ntohl(cheader.admin_accounts) + delta);
    to = DOFFSET(0, &cheader, &cheader.admin_accounts);
    code =
	kawrite(tt, to, (char *)&cheader.admin_accounts, sizeof(afs_int32));
    if (code)
	return KAIO;
    return 0;
}

static int
index_OK(index)
     afs_int32 index;
{
    if ((index < sizeof(cheader)) || (index >= ntohl(cheader.eofPtr))
	|| ((index - sizeof(cheader)) % sizeof(kaentry) != 0))
	return 0;
    return 1;
}

#define LEGALCHARS ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"

int
name_instance_legal(name, instance)
     char *name;
     char *instance;
{
    int code;

/* No string checks apply anymore.  The international people want to use full 8
   bit ascii without problems. */
#if 1
    code = (strlen(name) < MAXKTCNAMELEN)
	&& (strlen(instance) < MAXKTCNAMELEN);
#else
    map = LEGALCHARS;		/* permitted chars, instance allows <period> */
    code = (strlen(name) > 0) && string_legal(instance, map)
	&& string_legal(name, map + 1);
#endif
    if (!code)
	dynamic_statistics.string_checks++;
    return code;
}

static int
string_legal(str, map)
     char *str;
     char *map;
{
    int slen;

    slen = strlen(str);
    if (slen >= MAXKTCNAMELEN)
	return 0;		/* with trailing null must fit in data base */
    return (slen == strspn(str, map));	/* strspn returns length(str) if all chars in map */
}
