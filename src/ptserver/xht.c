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
#include <lock.h>
#include <ubik.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include "ptserver.h"
#include "pterror.h"

#if defined(PRDB_EXTENSIONS)
struct pr_xht xtables[] = {
    { "authname", HASHSIZE, 0/*hash*/, 0/*ckname*/, 0/*setnxt*/ }
};
#define NTABLES (sizeof(xtables) / sizeof(xtables[0]))

int
pr_XHTInit(void) {
    int i;

    for (i = 0; i < NTABLES; i++) {
	xtables[i].nparts  = (xtables[i].nbuckets + 1) / 32;
	xtables[i].partids = malloc(xtables[i].nparts * sizeof(afs_int32));
	xtables[i].parts   = malloc(xtables[i].nparts * sizeof(struct hashentry));
	if (!xtables[i].partids || !xtables[i].parts)
	    return PRNOMEM;
	memset(xtables[i].partids, 0, xtables[i].nparts * sizeof(afs_int32));
	memset(xtables[i].parts,   0, xtables[i].nparts * sizeof(struct hashentry));
    }
    return PRSUCCESS;
}

int
pr_WriteXHTEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct hashentry *tentry)
{
    afs_int32 code;
    register afs_int32 i;
    struct hashentry nentry;

    if (ntohl(1) != 1) {	/* No need to swap */
	memset(&nentry, 0, sizeof(nentry));	/* make reseved fields zero */
	nentry.flags = htonl(tentry->flags);
	nentry.id = htonl(tentry->id);
	nentry.cellid = htonl(tentry->cellid);
	nentry.next = htonl(tentry->next);
	nentry.nextTable = htonl(tentry->nextTable);
	nentry.tableid = htonl(tentry->tableid);
	nentry.offset = htonl(tentry->offset);
	for (i = 0; i < 32; i++)
	    nentry.buckets[i] = htonl(tentry->buckets[i]);
	tentry = &nentry;
    }
    code = pr_Write(tt, afd, pos, (char *)tentry, sizeof(struct hashentry));
    return (code);
}

int
pr_ReadXHTEntry(struct ubik_trans *tt, afs_int32 afd, afs_int32 pos, struct hashentry *tentry)
{
    afs_int32 code;
    register afs_int32 i;
    struct hashentry nentry;
    code = ubik_Seek(tt, afd, pos);
    if (code)
	return (code);
    if (ntohl(1) == 1) {	/* No swapping needed. */
	code = ubik_Read(tt, (char *)tentry, sizeof(struct hashentry));
	return (code);
    }
    code = ubik_Read(tt, (char *)&nentry, sizeof(struct hashentry));
    if (code)
	return (code);

    memset(tentry, 0, sizeof(*tentry));	/* make reseved fields zero */
    tentry->flags = ntohl(nentry.flags);
    tentry->id = ntohl(nentry.id);
    tentry->cellid = ntohl(nentry.cellid);
    tentry->next = ntohl(nentry.next);
    tentry->nextTable = ntohl(nentry.nextTable);
    tentry->tableid = ntohl(nentry.tableid);
    tentry->offset = ntohl(nentry.offset);
    for (i = 0; i < 32; i++)
	tentry->buckets[i] = ntohl(nentry.buckets[i]);
    return (code);
}

int
pr_LoadXHTs(struct ubik_trans *tt)
{
    int tbl, i;
    afs_int32 code, loc, next_table;
    struct hashentry tentry;

    for (loc = ntohl(cheader.ext_hashtables); loc; loc = next_table) {
	code = pr_ReadXHTEntry(tt, 0, loc, &tentry);
	if (code)
	    return PRDBFAIL;
	if ((tentry.flags & PRTYPE) != PRHASHTBL)
	    return PRINCONSISTENT;
	next_table = tentry.nextTable;
	tbl = tentry.tableid;
	if (tbl < 0 || tbl >= NTABLES)
	    continue;
	for (i = 0; i < xtables[tbl].nparts; i++) {
	    xtables[tbl].partids[i] = loc;
	    if (i) {
		code = pr_ReadXHTEntry(tt, 0, loc, &xtables[tbl].parts[i]);
		if (code)
		    return PRDBFAIL;
	    } else {
		memcpy(&xtables[tbl].parts[i], &tentry, sizeof(tentry));
	    }
	    if (((xtables[tbl].parts[i].flags & PRTYPE) != PRHASHTBL) ||
		(xtables[tbl].parts[i].tableid != tbl) ||
		xtables[tbl].parts[i].offset != i >> 5) {
		return PRINCONSISTENT;
	    }
	    loc = xtables[tbl].parts[i].next;
	}
    }
    return PRSUCCESS;
}


int
pr_CreateXHT(struct ubik_trans *tt, struct pr_xht *table)
{
    afs_int32 code, next;
    int i;

    if ((table->flags & XHT_INITED))
	return PRSUCCESS;

    next = 0;
    for (i = table->nparts; i--;) {
	memset(&table->parts[i], 0, sizeof(struct hashentry));
	table->parts[i].flags     = PRHASHTBL;
	table->parts[i].next      = next;
	table->parts[i].nextTable = i ? 0 : ntohl(cheader.ext_hashtables);
	table->parts[i].tableid   = table - xtables;
	table->parts[i].offset    = i >> 5;
	table->partids[i] = next = AllocBlock(tt);
	if (!next)
	    return PRDBFAIL;
	code = pr_WriteXHTEntry(tt, 0, next, &table->parts[i]);
	if (code)
	    return PRDBFAIL;
    }
    cheader.ext_hashtables = htonl(next);
    code = pr_Write(tt, 0, 68, (char *)&cheader.ext_hashtables,
		    sizeof(cheader.ext_hashtables));
    if (code)
	return PRDBFAIL;

    table->flags |= XHT_INITED;
    return PRSUCCESS;
}

/* Remove an entry from an extended hash table. */
afs_int32
RemoveFromExtendedHash(struct ubik_trans *tt, struct pr_xht *table,
		       void *aname, int anamelen, afs_int32 *loc)
{
    afs_int32 code, current, next, trail, i, part, bucket;
    int found;

    if (!(table->flags & XHT_INITED))
	return PRSUCCESS;

    i = (table->hash)(aname, anamelen);
    if (i < 0 || i >= table->nbuckets)
	return PRINCONSISTENT;
    part = i >> 5;
    bucket = i & 0x1f;

    current = table->parts[part].buckets[bucket];
    trail = 0;
    for (;;) {
	if (current == 0)
	    return PRSUCCESS;	/* already gone */
	code = (table->ckname)(tt, current, aname, anamelen, &next, &found);
	if (code)
	    return PRDBFAIL;
	if (found) break;
	trail = current;
	current = next;
    }
    if (trail == 0) {
	next = htonl(next);
	code = pr_Write(tt, 0, table->partids[part] + 32 + 4*bucket,
			(char *)&next, sizeof(next));
	if (code)
	    return PRDBFAIL;
    } else {
	code = (table->setnxt)(tt, trail, next);
	if (code)
	    return PRDBFAIL;
    }
    *loc = current;
    return PRSUCCESS;
}

afs_int32
AddToExtendedHash(struct ubik_trans *tt, struct pr_xht *table,
		  void *aname, int anamelen, afs_int32 loc)
{
    afs_int32 code, i, part, bucket, next;

    if (!(table->flags & XHT_INITED)) {
	code = pr_CreateXHT(tt, table);
	if (code)
	    return PRDBFAIL;
    }

    i = (table->hash)(aname, anamelen);
    if (i < 0 || i >= table->nbuckets)
	return PRINCONSISTENT;
    part = i >> 5;
    bucket = i & 0x1f;

    code = (table->setnxt)(tt, loc, table->parts[part].buckets[bucket]);
    if (code)
	return PRDBFAIL;

    table->parts[part].buckets[bucket] = loc;
    next = htonl(loc);
    code = pr_Write(tt, 0, table->partids[part] + 32 + 4*bucket,
		    (char *)&next, sizeof(next));
    if (code)
	return PRDBFAIL;
    return PRSUCCESS;
}
#endif
/* vi:set cin noet sw=4 tw=70: */
