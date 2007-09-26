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

/*
 *                      (3) Define a structure, idused, instead of an
 *                          array of long integers, idmap, to count group
 *                          memberships. These structures are on a linked
 *                          list, with each structure containing IDCOUNT
 *                          slots for id's.
 *                      (4) Add new functions to processs the structure
 *                          described above:
 *                             zeromap(), idcount(), inccount().
 *                      (5) Add code, primarily in WalkNextChain():
 *                           1. Test id's, allowing groups within groups.
 *                           2. Count the membership list for supergroups,
 *                              and follow the continuation chain for
 *                              supergroups.
 *                      (6) Add fprintf statements for various error
 *                          conditions.
 */

#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#include <io.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <errno.h>
#include <fcntl.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <ubik.h>
#include <afs/cmd.h>
#include <afs/com_err.h>

#include "ptint.h"
#include "pterror.h"
#include "ptserver.h"

struct prheader cheader;
int fd;
const char *pr_dbaseName;
char *whoami = "db_verify";
#define UBIK_HEADERSIZE 64

afs_int32
printheader(struct prheader *h)
{
    printf("Version           = %d\n", ntohl(h->version));
    printf("Header Size       = %d\n", ntohl(h->headerSize));
    printf("Free Ptr          = 0x%x\n", ntohl(h->freePtr));
    printf("EOF  Ptr          = 0x%x\n", ntohl(h->eofPtr));
    printf("Max Group     ID  = %d\n", ntohl(h->maxGroup));
    printf("Max User      ID  = %d\n", ntohl(h->maxID));
    printf("Max Foreign   ID  = %d\n", ntohl(h->maxForeign));
/* printf("Max Sub/Super ID  = %d\n", ntohl(h->maxInst)); */
    printf("Orphaned groups   = %d\n", ntohl(h->orphan));
    printf("User      Count   = %d\n", ntohl(h->usercount));
    printf("Group     Count   = %d\n", ntohl(h->groupcount));
/* printf("Foreign   Count   = %d\n", ntohl(h->foreigncount)); NYI */
/* printf("Sub/super Count   = %d\n", ntohl(h->instcount));    NYI */
    printf("Name Hash         = %d buckets\n", HASHSIZE);
    printf("ID   Hash         = %d buckets\n", HASHSIZE);
    return 0;
}

static afs_int32
pr_Read(afs_int32 pos, char *buff, afs_int32 len)
{
    afs_int32 code;

    code = lseek(fd, UBIK_HEADERSIZE + pos, 0);
    if (code == -1)
	return errno;

    code = read(fd, buff, len);
    if (code != len)
	return -1;
    if (code == -1)
	return errno;

    return 0;
}

/* InitDB ()
 *   Initializes the a transaction on the database and reads the header into
 * the static variable cheader.  If successful it returns a read-locked
 * transaction.  If ubik reports that cached database info should be up to date
 * the cheader structure is not re-read from the ubik.
 */

afs_int32
ReadHeader()
{
    afs_int32 code;

    code = pr_Read(0, (char *)&cheader, sizeof(cheader));
    if (code) {
	afs_com_err(whoami, code, "couldn't read header");
	return code;
    }
    /* Check and see if database exists and is approximately OK. */
    if (ntohl(cheader.headerSize) != sizeof(cheader)
	|| ntohl(cheader.eofPtr) == 0) {
	if (code)
	    return code;
	afs_com_err(whoami, PRDBBAD, "header is bad");
	return PRDBBAD;
    }
    return 0;
}

static afs_int32
IDHash(afs_int32 x)
{
    /* returns hash bucket for x */
    return ((abs(x)) % HASHSIZE);
}

static afs_int32
NameHash(register unsigned char *aname)
{
    /* returns hash bucket for aname */
    register unsigned int hash = 0;
    register int i;
/* stolen directly from the HashString function in the vol package */
    for (i = strlen(aname), aname += i - 1; i--; aname--)
	hash = (hash * 31) + (*aname - 31);
    return (hash % HASHSIZE);
}

#define MAP_NAMEHASH 1
#define MAP_IDHASH 2
#define MAP_HASHES (MAP_NAMEHASH | MAP_IDHASH)
#define MAP_CONT 4
#define MAP_FREE 8
#define MAP_OWNED 0x10
#define MAP_RECREATE 0x20

struct misc_data {
    int nEntries;		/* number of database entries */
    int anon;			/* found anonymous Id */
    afs_int32 maxId;		/* user */
    afs_int32 minId;		/* group */
    afs_int32 maxForId;		/* foreign user id */
#if defined(SUPERGROUPS)
#define IDCOUNT 512
    struct idused {
	int idstart;
	afs_int32 idcount[IDCOUNT];
	struct idused *idnext;
    } *idmap;
#else
    int idRange;		/* number of ids in map */
    afs_int32 *idmap;		/* map of all id's: midId is origin */
#endif				/* SUPERGROUPS */
    int nusers;			/* counts of each type */
    int ngroups;
    int nforeigns;
    int ninsts;
    int ncells;
    int maxOwnerLength;		/* longest owner chain */
    int maxContLength;		/* longest chain of cont. blks */
    int orphanLength;		/* length of orphan list */
    int freeLength;		/* length of free list */
    int verbose;
    int listuheader;
    int listpheader;
    int listentries;
    FILE *recreate;		/* stream for recreate instructions */
};

#if defined(SUPERGROUPS)
void zeromap(struct idused *idmap);
void inccount(struct idused **idmapp, int id);
int idcount(struct idused **idmapp, int id);
#endif

int
readUbikHeader(struct misc_data *misc)
{
    int offset, r;
    struct ubik_hdr uheader;

    offset = lseek(fd, 0, 0);
    if (offset != 0) {
	printf("error: lseek to 0 failed: %d %d\n", offset, errno);
	return (-1);
    }

    /* now read the info */
    r = read(fd, &uheader, sizeof(uheader));
    if (r != sizeof(uheader)) {
	printf("error: read of %d bytes failed: %d %d\n", sizeof(uheader), r,
	       errno);
	return (-1);
    }

    uheader.magic = ntohl(uheader.magic);
    uheader.size = ntohl(uheader.size);
    uheader.version.epoch = ntohl(uheader.version.epoch);
    uheader.version.counter = ntohl(uheader.version.counter);

    if (misc->listuheader) {
	printf("Ubik Header\n");
	printf("   Magic           = 0x%x\n", uheader.magic);
	printf("   Size            = %u\n", uheader.size);
	printf("   Version.epoch   = %u\n", uheader.version.epoch);
	printf("   Version.counter = %u\n", uheader.version.counter);
    }

    if (uheader.size != UBIK_HEADERSIZE)
	printf("Ubik header size is %u (should be %u)\n", uheader.size,
	       UBIK_HEADERSIZE);
    if (uheader.magic != UBIK_MAGIC)
	printf("Ubik header magic is 0x%x (should be 0x%x)\n", uheader.magic,
	       UBIK_MAGIC);

    return (0);
}

afs_int32
ConvertDiskAddress(afs_uint32 ea, int *eiP)
{
    int i;

    *eiP = -1;

    if (ea < sizeof(cheader))
	return PRDBADDR;
    if (ea >= ntohl(cheader.eofPtr))
	return PRDBADDR;
    ea -= sizeof(cheader);
    i = ea / sizeof(struct prentry);
    if (i * sizeof(struct prentry) != ea)
	return PRDBADDR;
/*    if ((i < 0) || (i >= misc->nEntries)) return PRDBADDR; */
    *eiP = i;
    return 0;
}

int
PrintEntryError(struct misc_data *misc, afs_int32 ea, struct prentry *e, int indent)
{

    pr_PrintEntry(stderr, /*net order */ 0, ea, e, indent);
    return 0;
}

afs_int32
WalkHashTable(afs_int32 hashtable[],	/* hash table to walk */
	      int hashType,		/* hash function to use */
	      char map[],		/* one byte per db entry */
	      struct misc_data *misc)	/* stuff to keep track of */
{
    afs_int32 code;
    int hi;			/* index in hash table */
    afs_int32 ea;		/* entry's db addr */
    int ei;			/* entry's index */
    char bit;			/* bits to check for in map */
    struct prentry e;
    afs_int32 next_ea;
    afs_int32 id;
    afs_int32 flags;
    afs_int32 hash;

    bit = hashType;

    for (hi = 0; hi < HASHSIZE; hi++) {
	ea = 0;
	next_ea = ntohl(hashtable[hi]);
	while (next_ea) {
	    code = ConvertDiskAddress(next_ea, &ei);
	    if (code) {
		fprintf(stderr, "Bad chain address %d\n", next_ea);
		if (ea) {
		    fprintf(stderr, "Last entry in chain:\n");
		    if (PrintEntryError(misc, ea, &e, 2))
			return PRDBBAD;
		}
		fprintf(stderr, "Skipping remainder of hash bucket %d\n", hi);
		break;
	    }
	    ea = next_ea;
	    code = pr_Read(ea, (char *)&e, sizeof(e));
	    if (code)
		return code;

	    id = ntohl(e.id);

	    if (((e.flags & htonl((PRGRP | PRINST))) == 0)
		&& (strchr(e.name, '@'))) {
		/* Foreign user */
		if (id > misc->maxForId)
		    misc->maxForId = id;
	    } else {
		if (id == ANONYMOUSID)
		    misc->anon++;
		else if (id > misc->maxId)
		    misc->maxId = id;
		if (id < misc->minId)
		    misc->minId = id;
	    }

	    switch (hashType) {
	    case MAP_NAMEHASH:
		next_ea = ntohl(e.nextName);
		hash = NameHash(e.name);
		break;
	    case MAP_IDHASH:
		next_ea = ntohl(e.nextID);
		hash = IDHash(id);
		break;
	    default:
		fprintf(stderr, "unknown hash table type %d\n", hashType);
		return PRBADARG;
	    }

	    if (map[ei] & bit) {
		fprintf(stderr,
			"Entry found twice in hash table: bucket %d\n", hi);
		if (hi != hash)
		    fprintf(stderr, "also in wrong bucket: should be in %d\n",
			    hash);
		if (PrintEntryError(misc, ea, &e, 2))
		    return PRDBBAD;
		break;
	    }
	    map[ei] |= bit;

	    flags = ntohl(e.flags);
	    switch (flags & PRTYPE) {
	    case PRFREE:
		fprintf(stderr, "ENTRY IS FREE");
		goto abort;
	    case PRCONT:
		fprintf(stderr, "ENTRY IS CONTINUATION");
		goto abort;
	    case PRGRP:
	    case PRUSER:
		break;
	    case PRCELL:
	    case PRFOREIGN:
	    case PRINST:
		fprintf(stderr, "ENTRY IS unexpected type (flags=0x%x)\n",
			flags);
		break;
	    default:
		fprintf(stderr, "ENTRY IS OF unknown type (flags=0x%x)\n",
			flags);
		goto abort;
	    }

	    if (hash != hi) {
		fprintf(stderr, "entry hashed in bucket %d should be %d\n",
			hi, hash);
	      abort:
		if (PrintEntryError(misc, ea, &e, 2))
		    return PRDBBAD;
		continue;
	    }
	}
    }
    return 0;
}

afs_int32
WalkNextChain(char map[],		/* one byte per db entry */
	      struct misc_data *misc,	/* stuff to keep track of */
	      afs_int32 ea, struct prentry *e)
{
    afs_int32 head;
    int bit;
    afs_int32 code;
    struct prentry c;		/* continuation entry */
    afs_int32 na;		/* next thread */
    int ni;
    afs_int32 eid;
    int count;			/* number of members */
    int i;
    int noErrors = 1;
    int length;			/* length of chain */
#if defined(SUPERGROUPS)
    int sgcount;		/* number of sgentrys */
    afs_int32 sghead;
#define g (((struct prentryg *)e))
#endif

    if (e) {
	head = ntohl(e->next);
	eid = ntohl(e->id);
	bit = MAP_CONT;
	count = 0;		/* set to >9999 if list ends early */
#if defined(SUPERGROUPS)
	sgcount = 0;
	sghead = ntohl(g->next);
#endif
	for (i = 0; i < PRSIZE; i++) {
	    afs_int32 id = ntohl(e->entries[i]);
	    if (id == PRBADID)
		continue;
	    else if (id) {
		int eid_s, id_s;
		count++;
		/* in case the ids are large, convert to pure sign. */
		if (id > 0)
		    id_s = 1;
		else
		    id_s = -1;
		if (eid > 0)
		    eid_s = 1;
		else
		    eid_s = -1;
#if defined(SUPERGROUPS)
		if (id_s > 0 && eid_s > 0) {
		    fprintf(stderr,
			    "User can't be member of user in membership list\n");
		    if (PrintEntryError(misc, ea, e, 2))
			return PRDBBAD;
		    noErrors = 0;
		}
#else
		if (id_s * eid_s > 0) {	/* sign should be different */
		    fprintf(stderr,
			    "Bad user/group dicotomy in membership list\n");
		    if (PrintEntryError(misc, ea, e, 2))
			return PRDBBAD;
		    noErrors = 0;
		}
#endif /* SUPERGROUPS */
		/* count each user as a group, and each group a user is in */
#if defined(SUPERGROUPS)
		if (!(id < 0 && eid < 0) && (id != ANONYMOUSID))
		    inccount(&misc->idmap, id);
#else
		if ((id >= misc->minId) && (id <= misc->maxId)
		    && (id != ANONYMOUSID))
		    misc->idmap[id - misc->minId]++;
#endif /* SUPERGROUPS */
	    } else if (head)
		count = 9999;
	    else
		break;
	}
#if defined(SUPERGROUPS)
	sghead = ntohl(g->nextsg);
	if ((e->flags & htonl(PRGRP))) {
	    for (i = 0; i < SGSIZE; ++i) {
		afs_int32 id = ntohl(g->supergroup[i]);
		if (id == PRBADID)
		    continue;
		else if (id) {
		    if (id > 0) {
			fprintf(stderr,
				"User can't be member of supergroup list\n");
			if (PrintEntryError(misc, ea, e, 2))
			    return PRDBBAD;
			noErrors = 0;
		    }
		    sgcount++;
		    inccount(&misc->idmap, id);
		}
	    }
	}
#endif /* SUPERGROUPS */
    } else {
	head = ntohl(cheader.freePtr);
#if defined(SUPERGROUPS)
	sghead = 0;
#endif
	bit = MAP_FREE;
    }

#if defined(SUPERGROUPS)
    length = 0;
    for (na = sghead; na; na = ntohl(c.next)) {
	code = ConvertDiskAddress(na, &ni);
	if (code) {
	    fprintf(stderr, "Bad SGcontinuation ptr %d", na);
	    if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (na != sghead) {
		fprintf(stderr, "last block: \n");
		if (PrintEntryError(misc, na, &c, 4))
		    return PRDBBAD;
	    }
	    return 0;
	}
	code = pr_Read(na, (char *)&c, sizeof(c));
	if (code)
	    return code;
	length++;

	if (map[ni]) {
	    fprintf(stderr, "Continuation entry reused\n");
	    if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (PrintEntryError(misc, na, &c, 4))
		return PRDBBAD;
	    noErrors = 0;
	    break;
	}
	map[ni] |= bit;
	if ((ntohl(c.id) != eid)) {
	    fprintf(stderr, "Continuation id mismatch\n");
	    if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (PrintEntryError(misc, na, &c, 4))
		return PRDBBAD;
	    noErrors = 0;
	    continue;
	}

	/* update membership count */
	for (i = 0; i < COSIZE; i++) {
	    afs_int32 id = ntohl(c.entries[i]);
	    if (id == PRBADID)
		continue;
	    else if (id) {
		int eid_s, id_s;
		sgcount++;
		/* in case the ids are large, convert to pure sign. */
		if (id > 0)
		    id_s = 1;
		else
		    id_s = -1;
		if (eid > 0)
		    eid_s = 1;
		else
		    eid_s = -1;
		if (id_s > 0) {
		    fprintf(stderr,
			    "User can't be member of supergroup list\n");
		    if (PrintEntryError(misc, ea, e, 2))
			return PRDBBAD;
		    if (PrintEntryError(misc, na, &c, 4))
			return PRDBBAD;
		    noErrors = 0;
		}
		/* count each user as a group, and each group a user is in */
		if ((id != ANONYMOUSID))
		    inccount(&misc->idmap, id);
	    } else if (c.next)
		count = 9999;
	    else
		break;
	}
    }
    if (length > misc->maxContLength)
	misc->maxContLength = length;
#endif /* SUPERGROUPS */
    length = 0;
    for (na = head; na; na = ntohl(c.next)) {
	code = ConvertDiskAddress(na, &ni);
	if (code) {
	    fprintf(stderr, "Bad continuation ptr %d", na);
	    if (e == 0)
		fprintf(stderr, "walking free list");
	    else if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (na != head) {
		fprintf(stderr, "last block: \n");
		if (PrintEntryError(misc, na, &c, 4))
		    return PRDBBAD;
	    }
	    return 0;
	}
	code = pr_Read(na, (char *)&c, sizeof(c));
	if (code)
	    return code;
	length++;

	if (map[ni]) {
	    fprintf(stderr, "Continuation entry reused\n");
	    if (e == 0)
		fprintf(stderr, "walking free list");
	    else if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (PrintEntryError(misc, na, &c, 4))
		return PRDBBAD;
	    noErrors = 0;
	    break;
	}
	map[ni] |= bit;
	if (e && (ntohl(c.id) != eid)) {
	    fprintf(stderr, "Continuation id mismatch\n");
	    if (e == 0)
		fprintf(stderr, "walking free list");
	    else if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (PrintEntryError(misc, na, &c, 4))
		return PRDBBAD;
	    noErrors = 0;
	    continue;
	}

	/* update membership count */
	if (e)
	    for (i = 0; i < COSIZE; i++) {
		afs_int32 id = ntohl(c.entries[i]);
		if (id == PRBADID)
		    continue;
		else if (id) {
		    int eid_s, id_s;
		    count++;
		    /* in case the ids are large, convert to pure sign. */
		    if (id > 0)
			id_s = 1;
		    else
			id_s = -1;
		    if (eid > 0)
			eid_s = 1;
		    else
			eid_s = -1;
#if defined(SUPERGROUPS)
		    if (id_s > 0 && eid_s > 0) {
			fprintf(stderr,
				"User can't be member of user in membership list\n");
			if (PrintEntryError(misc, ea, e, 2))
			    return PRDBBAD;
			if (PrintEntryError(misc, na, &c, 4))
			    return PRDBBAD;
			noErrors = 0;
		    }
#else
		    if (id_s * eid_s > 0) {	/* sign should be different */
			fprintf(stderr,
				"Bad user/group dicotomy in membership list\n");
			if (PrintEntryError(misc, ea, e, 2))
			    return PRDBBAD;
			if (PrintEntryError(misc, na, &c, 4))
			    return PRDBBAD;
			noErrors = 0;
		    }
#endif /* SUPERGROUPS */
		    /* count each user as a group, and each group a user is in */
#if defined(SUPERGROUPS)
		    if (!(id < 0 && eid < 0) && (id != ANONYMOUSID))
			inccount(&misc->idmap, id);
#else
		    if ((id >= misc->minId) && (id <= misc->maxId)
			&& (id != ANONYMOUSID))
			misc->idmap[id - misc->minId]++;
#endif /* SUPERGROUPS */
		} else if (c.next)
		    count = 9999;
		else
		    break;
	    }
    }
    if (e && noErrors && (count != ntohl(e->count))) {
#if defined(SUPERGROUPS)
	if (count >= 9999)
	    fprintf(stderr, "Membership list ends early\n");
#else
	if (count > 9999)
	    fprintf(stderr, "Membership list ends early\n");
#endif /* SUPERGROUPS */
	fprintf(stderr, "Count was %d should be %d\n", count,
		ntohl(e->count));
	if (PrintEntryError(misc, ea, e, 2))
	    return PRDBBAD;
#if defined(SUPERGROUPS)
	noErrors = 0;
    }
    if (e && (e->flags & htonl(PRGRP)) && (sgcount != ntohl(g->countsg))) {
	fprintf(stderr, "SGCount was %d should be %d\n", sgcount,
		ntohl(g->countsg));
	if (PrintEntryError(misc, ea, e, 2))
	    return PRDBBAD;
#endif
    }

    if (e) {
	if (length > misc->maxContLength)
	    misc->maxContLength = length;
    } else
	misc->freeLength = length;

    return 0;
#if defined(SUPERGROUPS)
#undef g
#endif
}

afs_int32
WalkOwnedChain(char map[],		/* one byte per db entry */
	       struct misc_data *misc,	/* stuff to keep track of */
	       afs_int32 ea, struct prentry *e)
{
    afs_int32 head;
    afs_int32 code;
    struct prentry c;		/* continuation entry */
    afs_int32 na;		/* next thread */
    int ni;
    afs_int32 eid;
    int length;			/* length of chain */

    if (e) {
	head = ntohl(e->owned);
	eid = ntohl(e->id);
    } else
	head = ntohl(cheader.orphan);

    length = 0;
    for (na = head; na; na = ntohl(c.nextOwned)) {
	code = ConvertDiskAddress(na, &ni);
	if (code) {
	    fprintf(stderr, "Bad owned list ptr %d", na);
	    if (e == 0)
		fprintf(stderr, "walking orphan list");
	    else if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (na != head) {
		fprintf(stderr, "last block: \n");
		if (PrintEntryError(misc, na, &c, 4))
		    return PRDBBAD;
	    }
	    return 0;
	}
	code = pr_Read(na, (char *)&c, sizeof(c));
	if (code)
	    return code;
	length++;

	if (map[ni] & MAP_OWNED) {
	    fprintf(stderr, "Entry on multiple owner chains\n");
	    if (e == 0)
		fprintf(stderr, "walking orphan list");
	    else if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (PrintEntryError(misc, na, &c, 4))
		return PRDBBAD;
	    break;
	}
	map[ni] |= MAP_OWNED;
	if ((map[ni] & MAP_HASHES) != MAP_HASHES) {
	    fprintf(stderr, "Owned entry not hashed properly\n");
	  abort:
	    if (e == 0)
		fprintf(stderr, "walking orphan list");
	    else if (PrintEntryError(misc, ea, e, 2))
		return PRDBBAD;
	    if (PrintEntryError(misc, na, &c, 4))
		return PRDBBAD;
	    continue;
	}
	if (e) {
	    if (ntohl(c.owner) != eid) {
		fprintf(stderr, "Owner id mismatch\n");
		goto abort;
	    }
	} else /* orphan */ if (c.owner) {
	    fprintf(stderr, "Orphan group owner not zero\n");
	    goto abort;
	}
    }

    if (e) {
	if (length > misc->maxOwnerLength)
	    misc->maxOwnerLength = length;
    } else
	misc->orphanLength = length;

    return 0;
}

afs_int32
WalkChains(char map[],		/* one byte per db entry */
	   struct misc_data *misc)	/* stuff to keep track of */
{
    afs_int32 code;
    int ei;
    afs_int32 ea;		/* entry's db addr */
    struct prentry e;
    afs_int32 id;
    int type;

    /* check all entries found in hash table walks */
    for (ei = 0; ei < misc->nEntries; ei++)
	if (map[ei] & MAP_HASHES) {
	    ea = ei * sizeof(struct prentry) + sizeof(cheader);
	    code = pr_Read(ea, (char *)&e, sizeof(e));
	    if (code)
		return code;

	    if ((map[ei] & MAP_HASHES) != MAP_HASHES) {
		fprintf(stderr, "entry not in both hashtables\n");
		if ((map[ei] & MAP_NAMEHASH) != MAP_NAMEHASH)
		    fprintf(stderr, "--> entry not in Name hashtable\n");
		if ((map[ei] & MAP_IDHASH) != MAP_IDHASH)
		    fprintf(stderr, "--> entry not in ID hashtable\n");

	      abort:
		if (PrintEntryError(misc, ea, &e, 2))
		    return PRDBBAD;
		continue;
	    }

	    id = ntohl(e.id);

	    type = ntohl(e.flags) & PRTYPE;
	    switch (type) {
	    case PRGRP:
		if (id >= 0) {
		    fprintf(stderr, "Group id not negative\n");
		    goto abort;
		}
		/* special case sysadmin: it owns itself */
		if (id == SYSADMINID) {
		    if (ntohl(e.owner) != SYSADMINID) {
			fprintf(stderr,
				"System:administrators doesn't own itself\n");
			goto abort;
		    }
		}
		code = WalkOwnedChain(map, misc, ea, &e);
		if (code)
		    return code;
		code = WalkNextChain(map, misc, ea, &e);
		if (code)
		    return code;
		misc->ngroups++;
		break;
	    case PRUSER:
		if (id <= 0) {
#if defined(SUPERGROUPS)
		    fprintf(stderr, "User id not positive\n");
#else
		    fprintf(stderr, "User id negative\n");
#endif
		    goto abort;
		}

		/* Users are owned by sysadmin, but sysadmin doesn't have an owner
		 * chain.  Check this then set the owned bit. */
		if (ntohl(e.owner) != SYSADMINID) {
		    fprintf(stderr,
			    "User not owned by system:administrators\n");
		    goto abort;
		}
		if (e.nextOwned) {
		    fprintf(stderr, "User has owned pointer\n");
		    goto abort;
		}
		map[ei] |= MAP_OWNED;

		code = WalkOwnedChain(map, misc, ea, &e);
		if (code)
		    return code;
		code = WalkNextChain(map, misc, ea, &e);
		if (code)
		    return code;
		if (strchr(e.name, '@') == 0) {
		    misc->nusers++;	/* Not a foreign user */
		} else {
		    misc->nforeigns++;	/* A foreign user */
		}
		break;
	    case PRFREE:
	    case PRCONT:
	    case PRCELL:
		misc->ncells++;
		break;
	    case PRFOREIGN:
		fprintf(stderr,
			"ENTRY IS unexpected type [PRFOREIGN] (flags=0x%x)\n",
			ntohl(e.flags));
		break;
	    case PRINST:
		misc->ninsts++;
		break;
	    default:
		fprintf(stderr, "entry with unexpected type");
		goto abort;
	    }
	}

    return 0;
}

afs_int32
GC(char map[], struct misc_data *misc)
{
    afs_int32 code;
    int ei;
    afs_int32 ea;
    struct prentry e;
    char m;

    for (ei = 0; ei < misc->nEntries; ei++) {
	ea = ei * sizeof(struct prentry) + sizeof(cheader);
	code = pr_Read(ea, (char *)&e, sizeof(e));
	if (code)
	    return code;
	m = map[ei];
	if (m == 0) {
	    fprintf(stderr, "Unreferenced entry:");
	    if (PrintEntryError(misc, ea, &e, 2))
		return PRDBBAD;
	}
	/* all users and groups should be owned, and their membership counts
	 * should be okay */
	else if ((m & MAP_HASHES) == MAP_HASHES) {
	    afs_int32 id;
	    int refCount;
	    if (!(m & MAP_OWNED)) {
		fprintf(stderr, "Entry not on any owner chain:\n");
		if (PrintEntryError(misc, ea, &e, 2))
		    return PRDBBAD;
	    }
	    id = ntohl(e.id);
#if defined(SUPERGROUPS)
	    if ((id != ANONYMOUSID)
		&& ((refCount = idcount(&misc->idmap, id)) != ntohl(e.count))) 
#else
	    if ((id >= misc->minId) && (id <= misc->maxId)
		&& (id != ANONYMOUSID)
		&& ((refCount = misc->idmap[id - misc->minId]) !=
		    ntohl(e.count))) 
#endif /* SUPERGROUPS */
	      {
		afs_int32 na;
		fprintf(stderr,
			"Entry membership count is inconsistent: %d entries refer to this one\n",
			refCount);
		if (PrintEntryError(misc, ea, &e, 2))
		    return PRDBBAD;

		/* get continuation blocks too */
		for (na = ntohl(e.next); na; na = ntohl(e.next)) {
		    int ni;
		    code = ConvertDiskAddress(na, &ni);
		    if (code)
			return code;
		    code = pr_Read(na, (char *)&e, sizeof(e));
		    if (code)
			return code;
		    if (PrintEntryError(misc, na, &e, 4))
			return PRDBBAD;
		}
	    }
	}
    }
    return 0;
}

char *
QuoteName(char *s)
{
    char *qs;
    if (strpbrk(s, " \t")) {
	qs = (char *)malloc(strlen(s) + 3);
	strcpy(qs, "\"");
	strcat(qs, s);
	strcat(qs, "\"");
    } else
	qs = s;
    return qs;
}

afs_int32
DumpRecreate(char map[], struct misc_data *misc)
{
    afs_int32 code;
    int ei;
    afs_int32 ea;
    struct prentry e;
    afs_int32 id;
    afs_int32 flags;
    afs_int32 owner;
    char *name;
    int builtinUsers = 0;
    int createLow = 0;		/* users uncreate from here */
#if defined(SUPERGROUPS)
    struct idused *idmap;	/* map of all id's */
#else
    afs_int32 *idmap;		/* map of all id's */
#endif
    int found;
    FILE *rc;

    rc = misc->recreate;
    idmap = misc->idmap;
#if defined(SUPERGROUPS)
    zeromap(idmap);
#else
    memset(idmap, 0, misc->idRange * sizeof(misc->idmap[0]));
#endif
    do {
	found = 0;
	for (ei = createLow; ei < misc->nEntries; ei++) {
	    if ((map[ei] & MAP_HASHES) && (map[ei] & MAP_RECREATE) == 0) {
		afs_int32 mask;
		afs_int32 access;
		int gq, uq;

		ea = ei * sizeof(struct prentry) + sizeof(cheader);
		code = pr_Read(ea, (char *)&e, sizeof(e));
		if (code)
		    return code;

		if (misc->listentries)
		    pr_PrintEntry(stdout, 0 /*not in host order */ , ea, &e,
				  0);

		id = ntohl(e.id);
		flags = ntohl(e.flags);
		owner = ntohl(e.owner);
		name = QuoteName(e.name);

		if (!strcmp(e.name, "system:administrators")
		    || !strcmp(e.name, "system:anyuser")
		    || !strcmp(e.name, "system:authuser")
		    || !strcmp(e.name, "system:backup")
		    || !strcmp(e.name, "anonymous")) {
		    builtinUsers++;
		    goto user_done;
		}

		/* check for duplicate id.  This may still lead to duplicate
		 * names. */
#if defined(SUPERGROUPS)
		if (idcount(&idmap, id)) 
#else
		if (idmap[id - misc->minId]) 
#endif
		  {
		    fprintf(stderr, "Skipping entry with duplicate id %di\n",
			    id);
		    goto user_done;
		}

		/* If owner doesn't exist skip for now, unless we're our own
		 * owner.  If so, a special case allows a group to own itself
		 * if caller is sysadmin.  This leaves only owner cycles to
		 * deal with. */

		if ((owner < misc->minId) || (owner > misc->maxId)) {
		    if (owner == ANONYMOUSID)
			fprintf(stderr,
				"Warning: id %di is owned by ANONYMOUS; using sysadmin instead\n",
				id);
		    else
			fprintf(stderr,
				"Bogus owner (%d) of id %di; using sysadmin instead\n",
				owner, id);
		    owner = SYSADMINID;
		}
		if (id == owner) {
		    fprintf(stderr, "Warning: group %s is self owning\n",
			    name);
		} else if (owner == 0) {
		    fprintf(stderr,
			    "Warning: orphan group %s will become self owning.\n",
			    name);
		    owner = id;
		}
#if defined(SUPERGROUPS)
		else if (!idcount(&idmap, owner))
		    goto user_skip;
#else
		else if (idmap[owner - misc->minId] == 0)
		    goto user_skip;
#endif

		if (rc)
		    fprintf(rc, "cr %s %d %d\n", name, id, owner);

		gq = uq = access = mask = 0;
		if (flags & PRACCESS) {
		    access = (flags >> PRIVATE_SHIFT);
		    mask |= PR_SF_ALLBITS;
		}
		if (flags & PRQUOTA) {
		    gq = ntohl(e.ngroups);
		    uq = ntohl(e.nusers);
		    mask |= PR_SF_NGROUPS | PR_SF_NUSERS;
		}
		if (mask && rc) {
		    fprintf(rc, "sf %d %x %x %d %d\n", id, mask, access, gq,
			    uq);
		}
	      user_done:
		map[ei] |= MAP_RECREATE;
#if defined(SUPERGROUPS)
		if (id != ANONYMOUSID)
		    inccount(&idmap, id);
#else
		if (id != ANONYMOUSID)
		    idmap[id - misc->minId]++;
#endif
		found++;
	    }
	    /* bump low water mark if possible */
	    if (ei == createLow)
		createLow++;
	  user_skip:;
	}
	misc->verbose = 0;
    } while (found);

    /* Now create the entries with circular owner dependencies and make them
     * own themselves.  This is the only way to create them with the correct
     * names. */
    for (ei = 0; ei < misc->nEntries; ei++)
	if (((map[ei] & MAP_HASHES) == MAP_HASHES)
	    && (map[ei] & MAP_RECREATE) == 0) {
	    ea = ei * sizeof(struct prentry) + sizeof(cheader);
	    code = pr_Read(ea, (char *)&e, sizeof(e));
	    if (code)
		return code;

	    id = ntohl(e.id);
	    name = QuoteName(e.name);
	    fprintf(stderr, "Warning: group %s in self owning cycle\n", name);
	    if (rc)
		fprintf(rc, "cr %s %d %d\n", name, id, id);
#if defined(SUPERGROUPS)
	    inccount(&idmap, id);
#else
	    idmap[id - misc->minId]++;
#endif
	}
    for (ei = 0; ei < misc->nEntries; ei++)
	if (((map[ei] & MAP_HASHES) == MAP_HASHES)
	    && (map[ei] & MAP_RECREATE) == 0) {
	    ea = ei * sizeof(struct prentry) + sizeof(cheader);
	    code = pr_Read(ea, (char *)&e, sizeof(e));
	    if (code)
		return code;

	    owner = ntohl(e.owner);
#if defined(SUPERGROUPS)
	    if (!idcount(&idmap, owner)) 
#else
	    if (idmap[owner - misc->minId] == 0) 
#endif
	      {
		fprintf(stderr,
			"Skipping chown of '%s' to non-existant owner %di\n",
			e.name, owner);
	    } else if (rc)
		fprintf(rc, "ce %d \"\" %d 0\n", ntohl(e.id), e.owner);
	}

    if (rc == 0)
	return 0;

    /* Reconstruct membership information based on the groups' user lists. */
    for (ei = 0; ei < misc->nEntries; ei++) {
	if ((map[ei] & MAP_HASHES) == MAP_HASHES) {
	    ea = ei * sizeof(struct prentry) + sizeof(cheader);
	    code = pr_Read(ea, (char *)&e, sizeof(e));
	    if (code)
		return code;

	    id = ntohl(e.id);
	    flags = ntohl(e.flags);

	    if ((id < 0) && (flags & PRGRP)) {
		int count = 0;
		afs_int32 na;
#if defined(SUPERGROUPS)
		afs_int32 ng;
#endif
		int i;
		for (i = 0; i < PRSIZE; i++) {
		    afs_int32 uid = ntohl(e.entries[i]);
		    if (uid == 0)
			break;
		    if (uid == PRBADID)
			continue;
#if !defined(SUPERGROUPS)
		    if (uid > 0) {
#endif
			fprintf(rc, "au %d %d\n", uid, id);
			count++;
#if !defined(SUPERGROUPS)
		    } else
			fprintf(stderr, "Skipping %di in group %di\n", uid,
				id);
#endif
		}
#if defined(SUPERGROUPS)
#define g	(*((struct prentryg *)&e))
		ng = ntohl(g.nextsg);
		for (i = 0; i < SGSIZE; i++) {
		    afs_int32 uid = ntohl(g.supergroup[i]);
		    if (uid == 0)
			break;
		    if (uid == PRBADID)
			continue;
		    fprintf(rc, "au %d %d\n", uid, id);
		    count++;
		}
		while (ng) {
		    struct prentry c;
		    code = pr_Read(ng, (char *)&c, sizeof(c));
		    if (code)
			return code;

		    if ((id == ntohl(c.id)) && (c.flags & htonl(PRCONT))) {
			for (i = 0; i < COSIZE; i++) {
			    afs_int32 uid = ntohl(c.entries[i]);
			    if (uid == 0)
				break;
			    if (uid == PRBADID)
				continue;
			    fprintf(rc, "au %d %d\n", uid, id);
			    count++;
			}
		    } else {
			fprintf(stderr, "Skipping continuation block at %d\n",
				ng);
			break;
		    }
		    ng = ntohl(c.next);
		}
#undef g
#endif /* SUPERGROUPS */
		na = ntohl(e.next);
		while (na) {
		    struct prentry c;
		    code = pr_Read(na, (char *)&c, sizeof(c));
		    if (code)
			return code;

		    if ((id == ntohl(c.id)) && (c.flags & htonl(PRCONT))) {
			for (i = 0; i < COSIZE; i++) {
			    afs_int32 uid = ntohl(c.entries[i]);
			    if (uid == 0)
				break;
			    if (uid == PRBADID)
				continue;
#if !defined(SUPERGROUPS)
			    if (uid > 0) {
#endif
				fprintf(rc, "au %d %d\n", uid, id);
				count++;
#if !defined(SUPERGROUPS)
			    } else
				fprintf(stderr, "Skipping %di in group %di\n",
					uid, id);
#endif
			}
		    } else {
			fprintf(stderr, "Skipping continuation block at %d\n",
				na);
			break;
		    }
		    na = ntohl(c.next);
		}
		if (count != ntohl(e.count))
		    fprintf(stderr,
			    "Group membership count problem found %d should be %d\n",
			    count, ntohl(e.count));
	    } else if ((id < 0) || (flags & PRGRP)) {
		fprintf(stderr, "Skipping group %di\n", id);
	    }
	}
    }
    return 0;
}

afs_int32
CheckPrDatabase(struct misc_data *misc)	/* info & statistics */
{
    afs_int32 code;
    afs_int32 eof;
    int n;
    char *map;			/* map of each entry in db */

    eof = ntohl(cheader.eofPtr);
    eof -= sizeof(cheader);
    n = eof / sizeof(struct prentry);
    if ((eof < 0) || (n * sizeof(struct prentry) != eof)) {
	code = PRDBBAD;
	afs_com_err(whoami, code, "eof ptr no good: eof=%d, sizeof(prentry)=%d",
		eof, sizeof(struct prentry));
      abort:
	return code;
    }
    if (misc->verbose)
	printf("Database has %d entries\n", n);
    map = (char *)malloc(n);
    memset(map, 0, n);
    misc->nEntries = n;

    if (misc->verbose) {
	printf("\nChecking name hash table\n");
	fflush(stdout);
    }
    code = WalkHashTable(cheader.nameHash, MAP_NAMEHASH, map, misc);
    if (code) {
	afs_com_err(whoami, code, "walking name hash");
	goto abort;
    }
    if (misc->verbose) {
	printf("\nChecking id hash table\n");
	fflush(stdout);
    }
    code = WalkHashTable(cheader.idHash, MAP_IDHASH, map, misc);
    if (code) {
	afs_com_err(whoami, code, "walking id hash");
	goto abort;
    }

    /* hash walk calculates min and max id */
#if defined(SUPERGROUPS)
    misc->idmap = 0;
#else
    n = ((misc->maxId > misc->maxForId) ? misc->maxId : misc->maxForId);
    misc->idRange = n - misc->minId + 1;
    misc->idmap = (afs_int32 *) malloc(misc->idRange * sizeof(afs_int32));
    if (!misc->idmap) {
	afs_com_err(whoami, 0, "Unable to malloc space for max ids of %d",
		misc->idRange);
	code = -1;
	goto abort;
    }
    memset(misc->idmap, 0, misc->idRange * sizeof(misc->idmap[0]));
#endif /* SUPERGROUPS */

    if (misc->verbose) {
	printf("\nChecking entry chains\n");
	fflush(stdout);
    }
    code = WalkChains(map, misc);
    if (code) {
	afs_com_err(whoami, code, "walking chains");
	goto abort;
    }
    if (misc->verbose) {
	printf("\nChecking free list\n");
	fflush(stdout);
    }
    code = WalkNextChain(map, misc, 0, 0);
    if (code) {
	afs_com_err(whoami, code, "walking free list");
	goto abort;
    }
    if (misc->verbose) {
	printf("\nChecking orphans list\n");
	fflush(stdout);
    }
    code = WalkOwnedChain(map, misc, 0, 0);
    if (code) {
	afs_com_err(whoami, code, "walking orphan list");
	goto abort;
    }

    if (misc->verbose) {
	printf("\nChecking for unreferenced entries\n");
	fflush(stdout);
    }
    code = GC(map, misc);
    if (code) {
	afs_com_err(whoami, code, "looking for unreferenced entries");
	goto abort;
    }

    DumpRecreate(map, misc);	/* check for owner cycles */
    if (misc->recreate)
	fclose(misc->recreate);

    if (misc->anon != 2)	/* once for each hash table */
	fprintf(stderr, "Problems with ANON=%d\n", misc->anon);
    if (misc->ncells || misc->ninsts)
	fprintf(stderr, "Unexpected entry type\n");
    if (misc->nusers != ntohl(cheader.usercount)) {
	fprintf(stderr,
		"User count inconsistent: should be %d, header claims: %d\n",
		misc->nusers, ntohl(cheader.usercount));
    }
    if (misc->ngroups != ntohl(cheader.groupcount)) {
	fprintf(stderr,
		"Group count inconsistent: should be %d, header claims: %d\n",
		misc->ngroups, ntohl(cheader.groupcount));
    }
    if (misc->maxId > ntohl(cheader.maxID))
	fprintf(stderr,
		"Database's max user Id (%d) is smaller than largest user's Id (%d).\n",
		ntohl(cheader.maxID), misc->maxId);
    if (misc->minId < ntohl(cheader.maxGroup))
	fprintf(stderr,
		"Database's max group Id (%d) is smaller than largest group's Id (%d).\n",
		ntohl(cheader.maxGroup), misc->minId);

    if (misc->verbose) {
	printf("\nMaxId = %d, MinId = %d, MaxForeignId = %d\n", misc->maxId,
	       misc->minId, misc->maxForId);
	printf
	    ("Free list is %d entries in length, %d groups on orphan list\n",
	     misc->freeLength, misc->orphanLength);
	printf
	    ("The longest owner list is %d, the longest continuation block chain is %d\n",
	     misc->maxOwnerLength, misc->maxContLength);
	printf("%d users ; %d foreign users ; and %d groups\n", misc->nusers,
	       misc->nforeigns, misc->ngroups);
    }

    free(map);
    return code;
}
    
#include "AFS_component_version_number.c"

int
WorkerBee(struct cmd_syndesc *as, char *arock)
{
    afs_int32 code;
    char *recreateFile;
    struct misc_data misc;	/* info & statistics */

    initialize_PT_error_table();
    initialize_U_error_table();
    initialize_rx_error_table();

    pr_dbaseName = AFSDIR_SERVER_PRDB_FILEPATH;
    memset(&misc, 0, sizeof(misc));

    pr_dbaseName = as->parms[0].items->data;	/* -database */
    misc.listuheader = (as->parms[1].items ? 1 : 0);	/* -uheader  */
    misc.listpheader = (as->parms[2].items ? 1 : 0);	/* -pheader  */
    misc.listentries = (as->parms[3].items ? 1 : 0);	/* -entries  */
    misc.verbose = (as->parms[4].items ? 1 : 0);	/* -verbose  */
    recreateFile = (as->parms[5].items ? as->parms[5].items->data : NULL);	/* -rebuild  */

    fd = open(pr_dbaseName, O_RDONLY, 0);
    if (fd == -1) {
	afs_com_err(whoami, errno, "Open failed on db %s", pr_dbaseName);
	exit(2);
    }

    /* Read the ubik header */
    if (misc.listuheader) {
	readUbikHeader(&misc);
    }

    code = ReadHeader();
    if (code)
	return code;
    if (misc.listpheader)
	printheader(&cheader);

    if (recreateFile) {
	misc.recreate = fopen(recreateFile, "w");
	if (misc.recreate == 0) {
	    afs_com_err(whoami, errno,
		    "can't create file for recreation instructions: %s",
		    recreateFile);
	    exit(4);
	}
    }
    code = CheckPrDatabase(&misc);
    if (code) {
	afs_com_err(whoami, code, "Checking prserver database");
	exit(3);
    }
    exit(0);
}

int
main(int argc, char *argv[])
{
    struct cmd_syndesc *ts;

    setlinebuf(stdout);

    ts = cmd_CreateSyntax(NULL, WorkerBee, NULL, "PRDB check");
    cmd_AddParm(ts, "-database", CMD_SINGLE, CMD_REQUIRED, "ptdb_file");
    cmd_AddParm(ts, "-uheader", CMD_FLAG, CMD_OPTIONAL,
		"Display UBIK header");
    cmd_AddParm(ts, "-pheader", CMD_FLAG, CMD_OPTIONAL,
		"Display KADB header");
    cmd_AddParm(ts, "-entries", CMD_FLAG, CMD_OPTIONAL, "Display entries");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "verbose");
    cmd_AddParm(ts, "-rebuild", CMD_SINGLE, CMD_OPTIONAL | CMD_HIDE,
		"out_file");

    return cmd_Dispatch(argc, argv);
}


#if defined(SUPERGROUPS)

/* new routines to deal with very large ID numbers */

void
zeromap(struct idused *idmap)
{
    while (idmap) {
	memset((char *)idmap->idcount, 0, sizeof idmap->idcount);
	idmap = idmap->idnext;
    }
}

void
inccount(struct idused **idmapp, int id)
{
    struct idused *idmap;

    if (IDCOUNT & (IDCOUNT - 1)) {
	fprintf(stderr, "IDCOUNT must be power of 2!\n");
	exit(1);
    }
    while (idmap = *idmapp) {
	if (idmap->idstart == (id & ~(IDCOUNT - 1)))
	    break;
	idmapp = &idmap->idnext;
    }
    if (!idmap) {
	idmap = (struct idused *)malloc(sizeof *idmap);
	if (!idmap) {
	    perror("idmap");
	    exit(1);
	}
	memset((char *)idmap, 0, sizeof idmap);
	idmap->idstart = id & ~(IDCOUNT - 1);
	idmap->idnext = *idmapp;
	*idmapp = idmap;
    }
    ++idmap->idcount[id & (IDCOUNT - 1)];
}

int
idcount(struct idused **idmapp, int id)
{
    struct idused *idmap;

    if (IDCOUNT & (IDCOUNT - 1)) {
	fprintf(stderr, "IDCOUNT must be power of 2!\n");
	exit(1);
    }
    while (idmap = *idmapp) {
	if (idmap->idstart == (id & ~(IDCOUNT - 1))) {
	    return idmap->idcount[id & (IDCOUNT - 1)];
	}
	idmapp = &idmap->idnext;
    }
    return 0;
}
#endif /* SUPERGROUPS */
