/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* TBD
 *	ht_lookupEntry - tape ids
 *	Truncate Tape - tape id's
 *	usetape - tape id's
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <sys/types.h>
#include <afs/stds.h>
#include <afs/bubasics.h>
#include <stdio.h>
#include <lock.h>
#include <ubik.h>
#include <lwp.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <des.h>
#include <afs/cellconfig.h>
#include <afs/auth.h>
#ifdef AFS_RXK5
#include <rx/rxk5.h>
#include <rx/rxk5errors.h>
#include <afs/rxk5_utilafs.h>
#endif
#include <errno.h>
#include "budb.h"
#include "budb_errs.h"
#include "database.h"
#include "error_macros.h"
#include "globals.h"
#include "afs/audit.h"
#include <afs/afsutil.h>

#undef min
#undef max


extern struct ubik_dbase *BU_dbase;
extern struct afsconf_dir *BU_conf;	/* for getting cell info */

afs_int32 AddVolume(), AddVolumes(), CreateDump(), DoDeleteDump(),
DoDeleteTape(), ListDumps();
afs_int32 DeleteVDP(), FindClone(), FindDump(), FindLatestDump();
afs_int32 FinishDump(), FinishTape(), GetDumps(), getExpiration(),
T_DumpDatabase();
afs_int32 makeAppended(), MakeDumpAppended(), FindLastTape(), GetTapes();
afs_int32 GetVolumes(), UseTape(), T_DumpHashTable(), T_GetVersion();

/* Text block management */

struct memTextBlock {
    struct memTextBlock *mtb_next;	/* next in chain */
    afs_int32 mtb_nbytes;	/* # of bytes in this block */
    struct blockHeader mtb_blkHeader;	/* in memory header */
    dbadr mtb_addr;		/* disk address of block */
};

typedef struct memTextBlock memTextBlockT;
typedef memTextBlockT *memTextBlockP;

/* These variable are for returning debugging info about the state of the
   server.  If they get trashed during multi-threaded operation it doesn't
   matter. */

/* This is global so COUNT_REQ in krb_udp.c can refer to it. */
char *lastOperation;		/* name of last operation */
static Date lastTrans;		/* time of last transaction */

/* procsInited is sort of a lock: during a transaction only one process runs
   while procsInited is false. */

static int procsInited = 0;

/* This variable is protected by the procsInited flag. */

static int (*rebuildDatabase) ();

/* AwaitInitialization 
 * Wait unitl budb has initialized (InitProcs). If it hasn't
 * within 5 seconds, then return no quorum.
 */
afs_int32
AwaitInitialization()
{
    afs_int32 start = 0;

    while (!procsInited) {
	if (!start)
	    start = time(0);
	else if (time(0) - start > 5)
	    return UNOQUORUM;
	IOMGR_Sleep(1);
    }
    return 0;
}

/* tailCompPtr
 *      name is a pathname style name, determine trailing name and return
 *      pointer to it
 */

char *
tailCompPtr(pathNamePtr)
     char *pathNamePtr;
{
    char *ptr;
    ptr = strrchr(pathNamePtr, '/');
    if (ptr == 0) {
	/* this should never happen */
	LogError(0, "tailCompPtr: could not find / in name(%s)\n",
		 pathNamePtr);
	return (pathNamePtr);
    } else
	ptr++;			/* skip the / */
    return (ptr);
}

/* callPermitted
 *      Check to see if the caller is a SuperUser.
 * exit:
 *	0 - no
 *	1 - yes
 */

int
callPermitted(call)
     struct rx_call *call;
{
    int permitted = 0;
    struct afsconf_dir *acdir;

    acdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!acdir)
	return 0;

    if (afsconf_SuperUser(acdir, call, NULL))
	permitted = 1;

    if (acdir)
	afsconf_Close(acdir);
    return (permitted);
}

/* InitRPC
 *	This is called by every RPC interface to create a Ubik transaction
 *	and read the database header into core
 * entry:
 *	ut
 *	lock
 *	this_op
 * notes:
 *	sets a lock on byte 1 of the database. Looks like it enforces
 *	single threading by use of the lock.
 */

afs_int32
InitRPC(ut, lock, this_op)
     struct ubik_trans **ut;
     int lock;			/* indicate read/write transaction */
     int this_op;		/* opcode of RCP, for COUNT_ABO */
{
    int code;
    float wait = 0.91;		/* start waiting for 1 second */

  start:
    /* wait for server initialization to finish if this is not InitProcs calling */
    if (this_op)
	if (code = AwaitInitialization())
	    return code;

    for (code = UNOQUORUM; code == UNOQUORUM;) {
	code =
	    ubik_BeginTrans(BU_dbase,
			    ((lock ==
			      LOCKREAD) ? UBIK_READTRANS : UBIK_WRITETRANS),
			    ut);
	if (code == UNOQUORUM) {	/* no quorum elected */
	    if (wait < 1)
		Log("Waiting for quorum election\n");
	    if (wait < 15.0)
		wait *= 1.1;
	    IOMGR_Sleep((int)wait);
	}
    }
    if (code)
	return code;
    if (wait > 1)
	Log("Have established quorum\n");

    /* set lock at posiion 1, for 1 byte of type lock */
    if (code = ubik_SetLock(*ut, 1, 1, lock)) {
	ubik_AbortTrans(*ut);
	return code;
    }

    /* check that dbase is initialized and setup cheader */
    if (lock == LOCKREAD) {
	/* init but don't fix because this is read only */
	if (code = CheckInit(*ut, 0)) {
	    ubik_AbortTrans(*ut);
	    if (code = InitRPC(ut, LOCKWRITE, 0)) {	/* Now fix the database */
		LogError(code, "InitRPC: InitRPC failed\n");
		return code;
	    }
	    if (code = ubik_EndTrans(*ut)) {
		LogError(code, "InitRPC: ubik_EndTrans failed\n");
		return code;
	    }
	    goto start;		/* now redo the read transaction */
	}
    } else {
	if (code = CheckInit(*ut, rebuildDatabase)) {
	    ubik_AbortTrans(*ut);
	    return code;
	}
    }
    lastTrans = time(0);
    return 0;
}

/* This is called to initialize a newly created database */
static int
initialize_database(ut)
     struct ubik_trans *ut;
{
    return 0;
}

static int noAuthenticationRequired;	/* global state */
static int recheckNoAuth;	/* global state */

afs_int32
InitProcs()
{
    struct ubik_trans *ut;
    afs_int32 code = 0;

    procsInited = 0;

    if ((globalConfPtr->myHost == 0) || (BU_conf == 0))
	ERROR(BUDB_INTERNALERROR);

    recheckNoAuth = 1;

    if (globalConfPtr->debugFlags & DF_NOAUTH)
	noAuthenticationRequired = 1;

    if (globalConfPtr->debugFlags & DF_RECHECKNOAUTH)
	recheckNoAuth = 0;

    if (recheckNoAuth)
	noAuthenticationRequired = afsconf_GetNoAuthFlag(BU_conf);

    if (noAuthenticationRequired)
	LogError(0, "Running server with security disabled\n");

    InitDB();

    rebuildDatabase = initialize_database;

    if (code = InitRPC(&ut, LOCKREAD, 0)) {
	LogError(code, "InitProcs: InitRPC failed\n");
	return code;
    }
    code = ubik_EndTrans(ut);
    if (code) {
	LogError(code, "InitProcs: ubik_EndTrans failed\n");
	return code;
    }

    rebuildDatabase = 0;	/* only do this during init */
    procsInited = 1;

  error_exit:
    return (code);
}

struct returnList {
    int nElements;		/* number in list */
    int allocSize;		/* number of elements allocated */
    dbadr *elements;		/* array of addresses */
};

static void
InitReturnList(list)
     struct returnList *list;
{
    list->nElements = 0;
    list->allocSize = 0;
    list->elements = (dbadr *) 0;
}

static void
FreeReturnList(list)
     struct returnList *list;
{
    if (list->elements)
	free(list->elements);
    list->elements = (dbadr *) 0;
    list->nElements = 0;
}

/* As entries are collected, they are added to a return list. Once all
 * entries have been collected, it is then placed in the return buffer
 * with SendReturnList(). The first *to_skipP are not recorded.
 */
static afs_int32
AddToReturnList(list, a, to_skipP)
     struct returnList *list;
     dbadr a;
     afs_int32 *to_skipP;
{
    char *tmp;
    afs_int32 size;

    if (a == 0)
	return 0;
    if (*to_skipP > 0) {
	(*to_skipP)--;
	return 0;
    }

    /* Up to 5 plus a maximum so SendReturnList() knows if we 
     * need to come back for more.
     */
    if (list->nElements >= BUDB_MAX_RETURN_LIST + 5)
	return BUDB_LIST2BIG;

    if (list->nElements >= list->allocSize) {
	if (list->elements == 0) {
	    size = 10;
	    tmp = (char *)malloc(sizeof(dbadr) * size);
	} else {
	    size = list->allocSize + 10;
	    tmp = (char *)realloc(list->elements, sizeof(dbadr) * size);
	}
	if (!tmp)
	    return BUDB_NOMEM;
	list->elements = (dbadr *) tmp;
	list->allocSize = size;
    }

    list->elements[list->nElements] = a;
    list->nElements++;
    return 0;
}

afs_int32
FillVolEntry(ut, va, vol)
     struct ubik_trans *ut;
     dbadr va;
     struct budb_volumeEntry *vol;
{
    struct dump d;
    struct tape t;
    struct volInfo vi;
    struct volFragment vf;

    if (dbread(ut, va, &vf, sizeof(vf)))
	return BUDB_IO;		/* The volFrag */
    if (dbread(ut, ntohl(vf.vol), &vi, sizeof(vi)))
	return BUDB_IO;		/* The volInfo */
    if (dbread(ut, ntohl(vf.tape), &t, sizeof(t)))
	return BUDB_IO;		/* The tape */
    if (dbread(ut, ntohl(t.dump), &d, sizeof(d)))
	return BUDB_IO;		/* The dump */

    strcpy(vol->name, vi.name);
    strcpy(vol->server, vi.server);
    strcpy(vol->tape, t.name);
    vol->tapeSeq = ntohl(t.seq);
    vol->dump = ntohl(d.id);
    vol->position = ntohl(vf.position);
    vol->clone = ntohl(vf.clone);
    vol->incTime = ntohl(vf.incTime);
    vol->nBytes = ntohl(vf.nBytes);
    vol->startByte = ntohl(vf.startByte);
    vol->flags = (ntohs(vf.flags) & VOLFRAGMENTFLAGS);	/* low  16 bits here */
    vol->flags |= (ntohl(vi.flags) & VOLINFOFLAGS);	/* high 16 bits here */
    vol->seq = ntohs(vf.sequence);
    vol->id = ntohl(vi.id);
    vol->partition = ntohl(vi.partition);

    return 0;
}

afs_int32
FillDumpEntry(ut, da, dump)
     struct ubik_trans *ut;
     dbadr da;
     struct budb_dumpEntry *dump;
{
    struct dump d, ad;

    if (dbread(ut, da, &d, sizeof(d)))
	return BUDB_IO;
    dump->id = ntohl(d.id);
    dump->flags = ntohl(d.flags);
    dump->created = ntohl(d.created);
    strncpy(dump->name, d.dumpName, sizeof(dump->name));
    strncpy(dump->dumpPath, d.dumpPath, sizeof(dump->dumpPath));
    strncpy(dump->volumeSetName, d.volumeSet, sizeof(dump->volumeSetName));

    dump->parent = ntohl(d.parent);
    dump->level = ntohl(d.level);
    dump->nVolumes = ntohl(d.nVolumes);

    tapeSet_ntoh(&d.tapes, &dump->tapes);

    if (strlen(d.dumper.name) < sizeof(dump->dumper.name))
	strcpy(dump->dumper.name, d.dumper.name);
    if (strlen(d.dumper.instance) < sizeof(dump->dumper.instance))
	strcpy(dump->dumper.instance, d.dumper.instance);
    if (strlen(d.dumper.cell) < sizeof(dump->dumper.cell))
	strcpy(dump->dumper.cell, d.dumper.cell);

    /* Get the initial dumpid and the appended dump id */
    dump->initialDumpID = ntohl(d.initialDumpID);
    if (d.appendedDumpChain) {
	if (dbread(ut, ntohl(d.appendedDumpChain), &ad, sizeof(ad)))
	    return BUDB_IO;
	dump->appendedDumpID = ntohl(ad.id);
    } else
	dump->appendedDumpID = 0;

    /* printf("dump name %s, parent %d, level %d\n",
     * d.dumpName, ntohl(d.parent), ntohl(d.level)); */

    return 0;
}

afs_int32
FillTapeEntry(ut, ta, tape)
     struct ubik_trans *ut;
     dbadr ta;
     struct budb_tapeEntry *tape;
{
    struct tape t;
    struct dump d;
    afs_int32 code;

    if (dbread(ut, ta, &t, sizeof(t)))
	return BUDB_IO;

    /* Get the tape's expiration date */
    if (code = getExpiration(ut, &t))
	return (code);

    strcpy(tape->name, t.name);
    tape->flags = ntohl(t.flags);
    tape->written = ntohl(t.written);
    tape->expires = ntohl(t.expires);
    tape->nMBytes = ntohl(t.nMBytes);
    tape->nBytes = ntohl(t.nBytes);
    tape->nFiles = ntohl(t.nFiles);
    tape->nVolumes = ntohl(t.nVolumes);
    tape->seq = ntohl(t.seq);
    tape->labelpos = ntohl(t.labelpos);
    tape->useCount = ntohl(t.useCount);
    tape->useKBytes = ntohl(t.useKBytes);

    if (dbread(ut, ntohl(t.dump), &d, sizeof(d)))
	return BUDB_IO;
    tape->dump = ntohl(d.id);
    return 0;
}

#define returnList_t budb_dumpList *

/* SendReturnList
 *      A list of elements of size e_size is in list, collected
 *      with AddToReturnList(). We will move this to a correspoding
 *      return list, eList, via FillProc(). nextInodeP tells us
 *      if there are more and how many to skip on the next request.
 */
static afs_int32
SendReturnList(ut, list, FillProc, e_size, index, nextIndexP, dbTimeP, eList)
     struct ubik_trans *ut;
     struct returnList *list;	/* list of elements to return */
afs_int32(*FillProc) ();	/* proc to fill entry */
     int e_size;		/* size of each element */
     afs_int32 index;		/* index from previous call */
     afs_int32 *nextIndexP;	/* if more elements are available */
     afs_int32 *dbTimeP;	/* time of last db update */
     budb_dumpList *eList;	/* rxgen list structure (e.g.) */
{
    afs_int32 code;
    int to_return;
    int i;
    char *e;

    *nextIndexP = -1;
    *dbTimeP = ntohl(db.h.lastUpdate);

    /* Calculate how many to return. Don't let if go over
     * BUDB_MAX_RETURN_LIST nor the size of our return list.
     */
    to_return = list->nElements;
    if (to_return > BUDB_MAX_RETURN_LIST)
	to_return = BUDB_MAX_RETURN_LIST;
    if (eList->budb_dumpList_len && (to_return > eList->budb_dumpList_len))
	to_return = eList->budb_dumpList_len;

    /* Allocate space for the return values if needed and zero it */
    if (eList->budb_dumpList_val == 0) {
	eList->budb_dumpList_val =
	    (struct budb_dumpEntry *)malloc(e_size * to_return);
	if (!eList->budb_dumpList_val)
	    return (BUDB_NOMEM);
    }
    memset(eList->budb_dumpList_val, 0, e_size * to_return);
    eList->budb_dumpList_len = to_return;

    e = (char *)(eList->budb_dumpList_val);
    for (i = 0; i < to_return; i++, e += e_size) {
	code = (*FillProc) (ut, list->elements[i], e);
	if (code)
	    return code;
    }

    if (list->nElements > i)
	*nextIndexP = index + i;
    return 0;
}

/* Come here to delete a volInfo structure. */

static afs_int32
DeleteVolInfo(ut, via, vi)
     struct ubik_trans *ut;
     dbadr via;
     struct volInfo *vi;
{
    afs_int32 code;
    dbadr hvia;
    struct volInfo hvi;

    if (vi->firstFragment)
	return 0;		/* still some frags, don't free yet */
    if (vi->sameNameHead == 0) {	/* this is the head */
	if (vi->sameNameChain)
	    return 0;		/* empty head, some non-heads left */

	code = ht_HashOut(ut, &db.volName, via, vi);
	if (code)
	    return code;
	code = FreeStructure(ut, volInfo_BLOCK, via);
	return code;
    }
    hvia = ntohl(vi->sameNameHead);
    if (dbread(ut, hvia, &hvi, sizeof(hvi)))
	return BUDB_IO;
    code =
	RemoveFromList(ut, hvia, &hvi, &hvi.sameNameChain, via, vi,
		       &vi->sameNameChain);
    if (code == -1)
	return BUDB_DATABASEINCONSISTENT;
    if (code == 0)
	code = FreeStructure(ut, volInfo_BLOCK, via);
    return code;
}

/* Detach a volume fragment from its volInfo structure.  Its tape chain is
   already freed.  This routine frees the structure and the caller must not
   write it out. */

static afs_int32
DeleteVolFragment(ut, va, v)
     struct ubik_trans *ut;
     dbadr va;
     struct volFragment *v;
{
    afs_int32 code;
    struct volInfo vi;
    dbadr via;

    via = ntohl(v->vol);
    if (dbread(ut, via, &vi, sizeof(vi)))
	return BUDB_IO;
    code =
	RemoveFromList(ut, via, &vi, &vi.firstFragment, va, v,
		       &v->sameNameChain);
    if (code == -1)
	return BUDB_DATABASEINCONSISTENT;
    if (code)
	return code;
    if (vi.firstFragment == 0)
	if (code = DeleteVolInfo(ut, via, &vi))
	    return code;
    if (code = FreeStructure(ut, volFragment_BLOCK, va))
	return code;

    /* decrement frag counter */
    code =
	set_word_addr(ut, via, &vi, &vi.nFrags, htonl(ntohl(vi.nFrags) - 1));
    if (code)
	return code;
    return 0;
}

/* DeleteTape - by freeing all its volumes and removing it from its dump chain.
 * The caller will remove it from the hash table if necessary.  The caller is
 * also responsible for writing the tape out if necessary. */

static afs_int32
DeleteTape(ut, ta, t)
     struct ubik_trans *ut;
     dbadr ta;
     struct tape *t;
{
    afs_int32 code;
    struct dump d;
    dbadr da;

    da = ntohl(t->dump);
    if (da == 0)
	return BUDB_DATABASEINCONSISTENT;
    if (dbread(ut, da, &d, sizeof(d)))
	return BUDB_IO;
    if (d.firstTape == 0)
	return BUDB_DATABASEINCONSISTENT;

    code = RemoveFromList(ut, da, &d, &d.firstTape, ta, t, &t->nextTape);
    if (code == -1)
	return BUDB_DATABASEINCONSISTENT;
    if (code)
	return code;

    /* Since the tape should have been truncated there should never be any
     * volumes in the tape. */
    if (t->firstVol || t->nVolumes)
	return BUDB_DATABASEINCONSISTENT;

    return 0;
}

static afs_int32
DeleteDump(ut, da, d)
     struct ubik_trans *ut;
     dbadr da;
     struct dump *d;
{
    afs_int32 code = 0;

    code = ht_HashOut(ut, &db.dumpIden, da, d);
    if (code)
	ERROR(code);

    code = ht_HashOut(ut, &db.dumpName, da, d);
    if (code)
	ERROR(code);

    /* Since the tape should have been truncated this should never happen. */
    if (d->firstTape || d->nVolumes)
	ERROR(BUDB_DATABASEINCONSISTENT);

    code = FreeStructure(ut, dump_BLOCK, da);
    if (code)
	ERROR(code);

  error_exit:
    return code;
}

/*
 * VolInfoMatch()
 *
 * This is called with a volumeEntry and a volInfo structure and compares
 * them.  It returns non-zero if they are equal.  It is used by GetVolInfo to
 * search volInfo structures once it has the head volInfo structure from the
 * volName hash table.
 *
 * When called from GetVolInfo the name compare is redundant.
 * Always AND the flags with VOLINFOFLAGS for backwards compatability (3.3).
 */

static int
VolInfoMatch(vol, vi)
     struct budb_volumeEntry *vol;
     struct volInfo *vi;
{
    return ((strcmp(vol->name, vi->name) == 0) &&	/* same volume name */
	    (vol->id == ntohl(vi->id)) &&	/* same volume id */
	    ((vol->flags & VOLINFOFLAGS) == (ntohl(vi->flags) & VOLINFOFLAGS)) &&	/* same flags */
	    (vol->partition == ntohl(vi->partition)) &&	/* same partition (N/A) */
	    (strcmp(vol->server, vi->server) == 0));	/* same server (N/A) */
}

/*
 * GetVolInfo()
 *     This routine takes a volumeEntry structure from an RPC interface and
 * returns the corresponding volInfo structure, creating it if necessary.
 *
 *     The caller must write the entry out.
 */

static afs_int32
GetVolInfo(ut, volP, viaP, viP)
     struct ubik_trans *ut;
     struct budb_volumeEntry *volP;
     dbadr *viaP;
     struct volInfo *viP;
{
    dbadr hvia, via;
    struct volInfo hvi;
    afs_int32 eval, code = 0;

    eval = ht_LookupEntry(ut, &db.volName, volP->name, &via, viP);
    if (eval)
	ERROR(eval);

    if (!via) {
	/* allocate a new volinfo structure */
	eval = AllocStructure(ut, volInfo_BLOCK, 0, &via, viP);
	if (eval)
	    ERROR(eval);

	strcpy(viP->name, volP->name);
	strcpy(viP->server, volP->server);
	viP->sameNameHead = 0;	/* The head of same name chain */
	viP->sameNameChain = 0;	/* Same name chain is empty */
	viP->firstFragment = 0;
	viP->nFrags = 0;
	viP->id = htonl(volP->id);
	viP->partition = htonl(volP->partition);
	viP->flags = htonl(volP->flags & VOLINFOFLAGS);

	/* Chain onto volname hash table */
	eval = ht_HashIn(ut, &db.volName, via, viP);
	if (eval)
	    ERROR(eval);

	LogDebug(4, "volume Info for %s placed at %d\n", db.volName, via);
    }

    else if (!VolInfoMatch(volP, viP)) {	/* Not the head volinfo struct */
	hvia = via;		/* remember the head volinfo struct */
	memcpy(&hvi, viP, sizeof(hvi));

	/* Search the same name chain for the correct volinfo structure */
	for (via = ntohl(viP->sameNameChain); via;
	     via = ntohl(viP->sameNameChain)) {
	    eval = dbread(ut, via, viP, sizeof(*viP));
	    if (eval)
		ERROR(eval);

	    if (VolInfoMatch(volP, viP))
		break;		/* found the one */
	}

	/* if the correct volinfo struct isn't found, create one */
	if (!via) {
	    eval = AllocStructure(ut, volInfo_BLOCK, 0, &via, viP);
	    if (eval)
		ERROR(eval);

	    strcpy(viP->name, volP->name);
	    strcpy(viP->server, volP->server);
	    viP->nameHashChain = 0;	/* not in hash table */
	    viP->sameNameHead = htonl(hvia);	/* chain to head of sameNameChain */
	    viP->sameNameChain = hvi.sameNameChain;
	    viP->firstFragment = 0;
	    viP->nFrags = 0;
	    viP->id = htonl(volP->id);
	    viP->partition = htonl(volP->partition);
	    viP->flags = htonl(volP->flags & VOLINFOFLAGS);

	    /* write the head entry's sameNameChain link */
	    eval =
		set_word_addr(ut, hvia, &hvi, &hvi.sameNameChain, htonl(via));
	    if (eval)
		ERROR(eval);
	}
    }

    *viaP = via;

  error_exit:
    return code;
}

/* deletesomevolumesfromtape
 *	Deletes a specified number of volumes from a tape. The tape 
 *	and dump are modified to reflect the smaller number of volumes.
 *	The transaction is not terminated, it is up to the caller to
 *	finish the transaction and start a new one (if desired).
 * entry:
 *	maxvolumestodelete - don't delete more than this many volumes
 */

afs_int32
deleteSomeVolumesFromTape(ut, tapeAddr, tapePtr, maxVolumesToDelete)
     struct ubik_trans *ut;
     dbadr tapeAddr;
     struct tape *tapePtr;
     int maxVolumesToDelete;
{
    dbadr volFragAddr, nextVolFragAddr, dumpAddr;
    struct volFragment volFrag;
    struct dump dump;
    int volumesDeleted = 0;
    afs_int32 eval, code = 0;

    if (!tapePtr)
	ERROR(0);

    for (volFragAddr = ntohl(tapePtr->firstVol);
	 (volFragAddr && (maxVolumesToDelete > 0));
	 volFragAddr = nextVolFragAddr) {
	eval = dbread(ut, volFragAddr, &volFrag, sizeof(volFrag));
	if (eval)
	    ERROR(eval);

	nextVolFragAddr = ntohl(volFrag.sameTapeChain);

	eval = DeleteVolFragment(ut, volFragAddr, &volFrag);
	if (eval)
	    ERROR(eval);

	maxVolumesToDelete--;
	volumesDeleted++;
    }

    /* reset the volume fragment pointer in the tape */
    tapePtr->firstVol = htonl(volFragAddr);

    /* diminish the tape's volume count */
    tapePtr->nVolumes = htonl(ntohl(tapePtr->nVolumes) - volumesDeleted);

    eval = dbwrite(ut, tapeAddr, tapePtr, sizeof(*tapePtr));
    if (eval)
	ERROR(eval);

    /* diminish the dump's volume count */
    dumpAddr = ntohl(tapePtr->dump);
    eval = dbread(ut, dumpAddr, &dump, sizeof(dump));
    if (eval)
	ERROR(eval);

    dump.nVolumes = htonl(ntohl(dump.nVolumes) - volumesDeleted);
    eval = dbwrite(ut, dumpAddr, &dump, sizeof(dump));
    if (eval)
	ERROR(eval);

  error_exit:
    return (code);
}

/* deleteDump
 *	deletes a dump in stages, by repeatedly deleting a small number of
 *	volumes from the dump until none are left. The dump is then deleted.
 *
 *	In the case where multiple calls are made to delete the same
 *	dump, the operation will succeed but contention for structures
 *	will result in someone getting back an error.
 *
 * entry:
 *	id - id of dump to delete
 */

afs_int32
deleteDump(call, id, dumps)
     struct rx_call *call;
     dumpId id;
     budb_dumpsList *dumps;
{
    struct ubik_trans *ut;
    dbadr dumpAddr, tapeAddr, appendedDump;
    struct dump dump;
    struct tape tape;
    dumpId dumpid;
    afs_int32 eval, code = 0;
    int partialDel = 0;

    /* iterate until the dump is truly deleted */

    dumpid = id;
    while (dumpid) {
	partialDel = 0;
	while (1) {
	    eval = InitRPC(&ut, LOCKWRITE, 1);
	    if (eval)
		ERROR(eval);	/* can't start transaction */

	    eval =
		ht_LookupEntry(ut, &db.dumpIden, &dumpid, &dumpAddr, &dump);
	    if (eval)
		ABORT(eval);
	    if (!dumpAddr)
		ABORT(BUDB_NOENT);	/* can't find dump */

	    if ((dumpid == id) && (dump.initialDumpID))	/* can't be an appended dump */
		ABORT(BUDB_NOTINITIALDUMP);

	    tapeAddr = ntohl(dump.firstTape);
	    if (tapeAddr == 0)
		break;

	    /* there is a tape to delete */
	    eval = dbread(ut, tapeAddr, &tape, sizeof(tape));
	    if (eval)
		ABORT(eval);

	    if (ntohl(tape.nVolumes)) {
		/* tape is not empty */
		eval = deleteSomeVolumesFromTape(ut, tapeAddr, &tape, 10);
		if (eval)
		    ABORT(eval);
	    }

	    if (ntohl(tape.nVolumes) == 0) {
		/* tape is now empty, delete it */
		eval = DeleteTape(ut, tapeAddr, &tape);
		if (eval)
		    ABORT(eval);
		eval = ht_HashOut(ut, &db.tapeName, tapeAddr, &tape);
		if (eval)
		    ABORT(eval);
		eval = FreeStructure(ut, tape_BLOCK, tapeAddr);
		if (eval)
		    ABORT(eval);
	    }

	    eval = ubik_EndTrans(ut);
	    if (eval)
		ERROR(eval);
	    partialDel = 1;
	}			/* next deletion portion */

	/* Record the dump just deleted */
	if (dumps && (dumps->budb_dumpsList_len < BUDB_MAX_RETURN_LIST)) {
	    if (dumps->budb_dumpsList_len == 0)
		dumps->budb_dumpsList_val =
		    (afs_int32 *) malloc(sizeof(afs_int32));
	    else
		dumps->budb_dumpsList_val =
		    (afs_int32 *) realloc(dumps->budb_dumpsList_val,
					  (dumps->budb_dumpsList_len +
					   1) * sizeof(afs_int32));

	    if (!dumps->budb_dumpsList_val)
		ABORT(BUDB_NOMEM);

	    dumps->budb_dumpsList_val[dumps->budb_dumpsList_len] = dumpid;
	    dumps->budb_dumpsList_len++;
	}

	appendedDump = ntohl(dump.appendedDumpChain);

	/* finally done. No more tapes left in the dump. Delete the dump itself */
	eval = DeleteDump(ut, dumpAddr, &dump);
	if (eval)
	    ABORT(eval);

	/* Now delete the appended dump too */
	if (appendedDump) {
	    eval = dbread(ut, appendedDump, &dump, sizeof(dump));
	    if (eval)
		ABORT(eval);

	    dumpid = ntohl(dump.id);
	} else
	    dumpid = 0;

	eval = ubik_EndTrans(ut);
	if (eval)
	    ERROR(eval);

	Log("Delete dump %s (DumpID %u), path %s\n", dump.dumpName,
	    ntohl(dump.id), dump.dumpPath);
    }

  error_exit:
    if (code && partialDel) {
	Log("Delete dump %s (DumpID %u), path %s - INCOMPLETE (code = %u)\n",
	    dump.dumpName, ntohl(dump.id), dump.dumpPath, code);
    }
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    goto error_exit;
}

/* --------------
 * dump selection routines - used by BUDB_GetDumps
 * --------------
 */

/* most recent dump selection */

struct chosenDump {
    struct chosenDump *next;
    dbadr addr;
    afs_uint32 date;
};

struct wantDumpRock {
    int maxDumps;		/* max wanted */
    int ndumps;			/* actual in chain */
    struct chosenDump *chain;
};


int
wantDump(dumpAddrParam, dumpParam, dumpListPtrParam)
     char *dumpAddrParam;
     char *dumpParam;
     char *dumpListPtrParam;
{
    dbadr dumpAddr;
    struct dump *dumpPtr;
    struct wantDumpRock *rockPtr;

    dumpAddr = (dbadr) dumpAddrParam;
    dumpPtr = (struct dump *)dumpParam;
    rockPtr = (struct wantDumpRock *)dumpListPtrParam;

    /* if we don't have our full complement, just add another */
    if (rockPtr->ndumps < rockPtr->maxDumps)
	return (1);

    /* got the number we need, select based on date */
    if ((afs_uint32) ntohl(dumpPtr->created) > rockPtr->chain->date)
	return (1);

    return (0);
}

int
rememberDump(dumpAddrParam, dumpParam, dumpListPtrParam)
     char *dumpAddrParam;
     char *dumpParam;
     char *dumpListPtrParam;
{
    dbadr dumpAddr;
    struct dump *dumpPtr;
    struct wantDumpRock *rockPtr;
    struct chosenDump *ptr, *deletedPtr, **nextPtr;

    dumpAddr = (dbadr) dumpAddrParam;
    dumpPtr = (struct dump *)dumpParam;
    rockPtr = (struct wantDumpRock *)dumpListPtrParam;

    ptr = (struct chosenDump *)malloc(sizeof(*ptr));
    if (!ptr)
	return (0);
    memset(ptr, 0, sizeof(*ptr));
    ptr->addr = dumpAddr;
    ptr->date = (afs_uint32) ntohl(dumpPtr->created);

    /* Don't overflow the max */
    while (rockPtr->ndumps >= rockPtr->maxDumps) {
	/* have to drop one */
	deletedPtr = rockPtr->chain;
	rockPtr->chain = deletedPtr->next;
	free(deletedPtr);
	rockPtr->ndumps--;
    }

    /* now insert in the right place */
    for (nextPtr = &rockPtr->chain; *nextPtr; nextPtr = &((*nextPtr)->next)) {
	if (ptr->date < (*nextPtr)->date)
	    break;
    }
    ptr->next = *nextPtr;
    *nextPtr = ptr;
    rockPtr->ndumps++;

    return (0);
}


/* ---------------------------------------------
 * general interface routines - alphabetic
 * ---------------------------------------------
 */

afs_int32
SBUDB_AddVolume(call, vol)
     struct rx_call *call;
     struct budb_volumeEntry *vol;
{
    afs_int32 code;

    code = AddVolume(call, vol);
    osi_auditU(call, BUDB_AddVolEvent, code, AUD_LONG, (vol ? vol->id : 0),
	       AUD_END);
    return code;
}

afs_int32
AddVolume(call, vol)
     struct rx_call *call;
     struct budb_volumeEntry *vol;
{
    struct ubik_trans *ut;
    dbadr da, ta, via, va;
    struct dump d;
    struct tape t;
    struct volInfo vi;
    struct volFragment v;
    afs_uint32 bytes;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if ((strlen(vol->name) >= sizeof(vi.name))
	|| (strlen(vol->server) >= sizeof(vi.server))
	|| (strlen(vol->tape) >= sizeof(t.name)))
	return BUDB_BADARGUMENT;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    /* Find the dump in dumpid hash table */
    eval = ht_LookupEntry(ut, &db.dumpIden, &vol->dump, &da, &d);
    if (eval)
	ABORT(eval);
    if (!da)
	ABORT(BUDB_NODUMPID);

    /* search for the right tape in the dump */
    for (ta = ntohl(d.firstTape); ta; ta = ntohl(t.nextTape)) {
	/* read the tape entry */
	eval = dbread(ut, ta, &t, sizeof(t));
	if (eval)
	    ABORT(eval);

	/* Check if the right tape name */
	if (strcmp(t.name, vol->tape) == 0)
	    break;
    }
    if (!ta)
	ABORT(BUDB_NOTAPENAME);

    if ((t.dump != htonl(da)) ||	/* tape must belong to dump   */
	((ntohl(t.flags) & BUDB_TAPE_BEINGWRITTEN) == 0) ||	/* tape must be being written */
	((ntohl(d.flags) & BUDB_DUMP_INPROGRESS) == 0))	/* dump must be in progress   */
	ABORT(BUDB_BADPROTOCOL);

    /* find or create a volume info structure */
    eval = GetVolInfo(ut, vol, &via, &vi);
    if (eval)
	ABORT(eval);

    /* Create a volume fragment */
    eval = AllocStructure(ut, volFragment_BLOCK, 0, &va, &v);
    if (eval)
	ABORT(eval);

    v.vol = htonl(via);		/* vol frag points to vol info */
    v.sameNameChain = vi.firstFragment;	/* vol frag is chained to vol info */
    vi.firstFragment = htonl(va);
    vi.nFrags = htonl(ntohl(vi.nFrags) + 1);

    eval = dbwrite(ut, via, &vi, sizeof(vi));	/* write the vol info struct */
    if (eval)
	ABORT(eval);

    v.tape = htonl(ta);		/* vol frag points to tape */
    v.sameTapeChain = t.firstVol;	/* vol frag is chained to tape info */
    t.firstVol = htonl(va);
    t.nVolumes = htonl(ntohl(t.nVolumes) + 1);
    bytes = ntohl(t.nBytes) + vol->nBytes;	/* update bytes on tape */
    t.nMBytes = htonl(ntohl(t.nMBytes) + bytes / (1024 * 1024));
    t.nBytes = htonl(bytes % (1024 * 1024));

    eval = dbwrite(ut, ta, &t, sizeof(t));	/* write the tape structure */
    if (eval)
	ABORT(eval);

    d.nVolumes = htonl(ntohl(d.nVolumes) + 1);	/* one more volume on dump */

    eval = dbwrite(ut, da, &d, sizeof(d));	/* write out the dump structure */
    if (eval)
	ABORT(eval);

    v.position = htonl(vol->position);	/* vol frag info */
    v.clone = htonl(vol->clone);
    v.incTime = htonl(vol->incTime);
    v.startByte = htonl(vol->startByte);
    v.nBytes = htonl(vol->nBytes);
    v.flags = htons(vol->flags & VOLFRAGMENTFLAGS);
    v.sequence = htons(vol->seq);

    eval = dbwrite(ut, va, &v, sizeof(v));	/* write out the vol frag struct */
    if (eval)
	ABORT(eval);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ABORT(eval);

    LogDebug(4, "added volume %s at %d\n", vol->name, va);

    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    return code;
}


afs_int32
SBUDB_AddVolumes(call, vols)
     struct rx_call *call;
     struct budb_volumeList *vols;
{
    afs_int32 code;

    code = AddVolumes(call, vols);
    osi_auditU(call, BUDB_AddVolEvent, code, AUD_LONG, 0, AUD_END);
    return code;
}

afs_int32
AddVolumes(call, vols)
     struct rx_call *call;
     struct budb_volumeList *vols;
{
    struct budb_volumeEntry *vol, *vol1;
    struct ubik_trans *ut;
    dbadr da, ta, via, va;
    struct dump d;
    struct tape t;
    struct volInfo vi;
    struct volFragment v;
    afs_uint32 bytes;
    afs_int32 eval, e, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (!vols || (vols->budb_volumeList_len <= 0)
	|| !vols->budb_volumeList_val)
	return BUDB_BADARGUMENT;

    /* The first volume in the list of volumes to add */
    vol1 = (struct budb_volumeEntry *)vols->budb_volumeList_val;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    /* Find the dump in dumpid hash table */
    eval = ht_LookupEntry(ut, &db.dumpIden, &vol1->dump, &da, &d);
    if (eval)
	ABORT(eval);
    if (!da)
	ABORT(BUDB_NODUMPID);

    /* search for the right tape in the dump */
    for (ta = ntohl(d.firstTape); ta; ta = ntohl(t.nextTape)) {
	/* read the tape entry */
	eval = dbread(ut, ta, &t, sizeof(t));
	if (eval)
	    ABORT(eval);

	/* Check if the right tape name */
	if (strcmp(t.name, vol1->tape) == 0)
	    break;
    }
    if (!ta)
	ABORT(BUDB_NOTAPENAME);

    if ((t.dump != htonl(da)) ||	/* tape must belong to dump   */
	((ntohl(t.flags) & BUDB_TAPE_BEINGWRITTEN) == 0) ||	/* tape must be being written */
	((ntohl(d.flags) & BUDB_DUMP_INPROGRESS) == 0))	/* dump must be in progress   */
	ABORT(BUDB_BADPROTOCOL);

    for (vol = vol1, e = 0; e < vols->budb_volumeList_len; vol++, e++) {
	/*v */
	if ((strlen(vol->name) >= sizeof(vi.name)) || (strcmp(vol->name, "") == 0) ||	/* no null volnames */
	    (strlen(vol->server) >= sizeof(vi.server))
	    || (strlen(vol->tape) >= sizeof(t.name))
	    || (strcmp(vol->tape, vol1->tape) != 0)) {
	    Log("Volume '%s' %u, tape '%s', dumpID %u is an invalid entry - not added\n", vol->name, vol->id, vol->tape, vol->dump);
	    continue;
	}

	/* find or create a volume info structure */
	eval = GetVolInfo(ut, vol, &via, &vi);
	if (eval)
	    ABORT(eval);
	if (*(afs_int32 *) (&vi) == 0) {
	    Log("Volume '%s', tape '%s', dumpID %u is an invalid entry - aborted\n", vol->name, vol->tape, vol->dump);
	    ABORT(BUDB_BADARGUMENT);
	}

	/* Create a volume fragment */
	eval = AllocStructure(ut, volFragment_BLOCK, 0, &va, &v);
	if (eval)
	    ABORT(eval);

	v.vol = htonl(via);	/* vol frag points to vol info */
	v.sameNameChain = vi.firstFragment;	/* vol frag is chained to vol info */
	vi.firstFragment = htonl(va);
	vi.nFrags = htonl(ntohl(vi.nFrags) + 1);
	eval = dbwrite(ut, via, &vi, sizeof(vi));	/* write the vol info struct */
	if (eval)
	    ABORT(eval);

	v.tape = htonl(ta);	/* vol frag points to tape */
	v.sameTapeChain = t.firstVol;	/* vol frag is chained to tape info */
	t.firstVol = htonl(va);
	t.nVolumes = htonl(ntohl(t.nVolumes) + 1);
	bytes = ntohl(t.nBytes) + vol->nBytes;	/* update bytes on tape */
	t.nMBytes = htonl(ntohl(t.nMBytes) + bytes / (1024 * 1024));
	t.nBytes = htonl(bytes % (1024 * 1024));

	d.nVolumes = htonl(ntohl(d.nVolumes) + 1);	/* one more volume on dump */

	v.position = htonl(vol->position);	/* vol frag info */
	v.clone = htonl(vol->clone);
	v.incTime = htonl(vol->incTime);
	v.startByte = htonl(vol->startByte);
	v.nBytes = htonl(vol->nBytes);
	v.flags = htons(vol->flags & VOLFRAGMENTFLAGS);
	v.sequence = htons(vol->seq);

	eval = dbwrite(ut, va, &v, sizeof(v));	/* write out the vol frag struct */
	if (eval)
	    ABORT(eval);

	LogDebug(4, "added volume %s at %d\n", vol->name, va);
    }				/*v */

    eval = dbwrite(ut, ta, &t, sizeof(t));	/* write the tape structure */
    if (eval)
	ABORT(eval);

    eval = dbwrite(ut, da, &d, sizeof(d));	/* write out the dump structure */
    if (eval)
	ABORT(eval);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ABORT(eval);

    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    return code;
}


/* BUDB_CreateDump
 *	records the existence of a dump in the database. This creates only
 *	the dump record, to which one must attach tape and volume records.
 * TBD
 *	1) record the volume set
 */

afs_int32
SBUDB_CreateDump(call, dump)
     struct rx_call *call;
     struct budb_dumpEntry *dump;
{
    afs_int32 code;

    code = CreateDump(call, dump);
    osi_auditU(call, BUDB_CrDmpEvent, code, AUD_DATE, (dump ? dump->id : 0),
	       AUD_END);
    if (dump && !code) {
	Log("Create dump %s (DumpID %u), path %s\n", dump->name, dump->id,
	    dump->dumpPath);
    }
    return code;
}

afs_int32
CreateDump(call, dump)
     struct rx_call *call;
     struct budb_dumpEntry *dump;
{
    struct ubik_trans *ut;
    dbadr findDumpAddr, da;
    struct dump findDump, d;
    afs_int32 eval, code = 0;

    rxkad_level level;
    afs_int32 kvno;
    Date expiration;		/* checked by Security Module */
    struct ktc_principal principal;
    afs_int32 secClass;
    afs_int32 authenticated = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (strlen(dump->name) >= sizeof(d.dumpName))
	return BUDB_BADARGUMENT;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    secClass = rx_SecurityClassOf(rx_ConnectionOf(call));
    if (secClass == 2) {

      eval =
	rxkad_GetServerInfo(rx_ConnectionOf(call), &level, &expiration,
			    principal.name, principal.instance,
			    principal.cell, &kvno);

      if (eval) {
	if (eval != RXKADNOAUTH)
	  ABORT(eval);
	
	strcpy(principal.name, "");
	strcpy(principal.instance, "");
	strcpy(principal.cell, "");
	expiration = 0;

      } else {
	authenticated = 1;
      }
    }

#ifdef AFS_RXK5
    else if (secClass == 5) {

      char *rxk5_princ;
      int expires;
      char *afsname = 0, *k4realm, *k4instance;
      
      eval = rxk5_GetServerInfo(call->conn, 0,
				&expires, &rxk5_princ, 0, 0);
      if(eval)
	goto out;

      expiration = expires;
      eval = afs_rxk5_parse_name_k5(BU_conf, rxk5_princ, &afsname, 1);
      if(eval)
	goto out;

      k4realm = strchr(afsname, '@');
      if (k4realm) *k4realm++ = 0;
      k4instance = strchr(afsname, '.');
      if (k4instance) *k4instance++ = 0;

      memset(&principal, 0, sizeof principal);
      strcpy(principal.name, afsname);
      if(k4instance) strcpy(principal.instance, k4instance);
      if(k4realm) strcpy(principal.cell, k4realm);

    out:
      
      if(afsname)
	free(afsname);

	if (eval && eval != RXK5NOAUTH)
		ABORT(eval);
    }
#endif

    if(authenticated) {
	/* authenticated. Take user supplied principal information */
	if (strcmp(dump->dumper.name, "") != 0)
	    strncpy(principal.name, dump->dumper.name,
		    sizeof(principal.name));

	if (strcmp(dump->dumper.instance, "") != 0)
	    strncpy(principal.instance, dump->dumper.instance,
		    sizeof(principal.instance));

	if (strcmp(dump->dumper.cell, "") != 0)
	    strncpy(principal.cell, dump->dumper.cell,
		    sizeof(principal.cell));
    }

    /* dump id's are time stamps */
    if (!dump->id) {
	while (1) {		/* allocate a unique dump id *//*w */
	    dump->id = time(0);

	    /* ensure it is unique - seach for dumpid in hash table */
	    eval =
		ht_LookupEntry(ut, &db.dumpIden, &dump->id, &findDumpAddr,
			       &findDump);
	    if (eval)
		ABORT(eval);

	    if (!findDumpAddr) {	/* dumpid not in use */
		/* update the last dump id allocated */
		eval = set_header_word(ut, lastDumpId, htonl(dump->id));
		if (eval)
		    ABORT(eval);
		break;
	    }

	    /* dump id is in use - wait a while */
	    IOMGR_Sleep(1);
	}			/*w */
    } else {
	/* dump id supplied (e.g. for database restore) */
	eval =
	    ht_LookupEntry(ut, &db.dumpIden, &dump->id, &findDumpAddr,
			   &findDump);
	if (eval)
	    ABORT(eval);

	/* Dump id must not already exist */
	if (findDumpAddr)
	    ABORT(BUDB_DUMPIDEXISTS);
    }

    /* Allocate a dump structure */
    memset(&d, 0, sizeof(d));
    eval = AllocStructure(ut, dump_BLOCK, 0, &da, &d);
    if (eval)
	ABORT(eval);

    strcpy(d.dumpName, dump->name);	/* volset.dumpname */
    strcpy(d.dumpPath, dump->dumpPath);	/* dump node path */
    strcpy(d.volumeSet, dump->volumeSetName);	/* volume set */
    d.id = htonl(dump->id);
    d.parent = htonl(dump->parent);	/* parent id */
    d.level = htonl(dump->level);

    LogDebug(4, "dump name %s, parent %d level %d\n", dump->name,
	     dump->parent, dump->level);

    /* if creation time specified, use that.  Else use the dumpid time */
    if (dump->created == 0)
	dump->created = dump->id;
    d.created = htonl(dump->created);

    principal_hton(&principal, &d.dumper);
    tapeSet_hton(&dump->tapes, &d.tapes);

    d.flags = htonl(dump->flags | BUDB_DUMP_INPROGRESS);

    eval = ht_HashIn(ut, &db.dumpName, da, &d);	/* Into dump name hash table */
    if (eval)
	ABORT(eval);

    eval = ht_HashIn(ut, &db.dumpIden, da, &d);	/* Into dumpid hash table */
    if (eval)
	ABORT(eval);

    eval = dbwrite(ut, da, (char *)&d, sizeof(d));	/* Write the dump structure */
    if (eval)
	ABORT(eval);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ABORT(eval);

    /* If to append this dump, then append it - will write the appended dump */
    eval = makeAppended(ut, dump->id, dump->initialDumpID, dump->tapes.b);
    if (eval)
	ABORT(eval);

    code = ubik_EndTrans(ut);
    LogDebug(5, "made dump %s, path %s\n", d.dumpName, d.dumpPath);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    return code;
}

afs_int32
SBUDB_DeleteDump(call, id, fromTime, toTime, dumps)
     struct rx_call *call;
     dumpId id;
     Date fromTime;
     Date toTime;
     budb_dumpsList *dumps;
{
    afs_int32 code;

    code = DoDeleteDump(call, id, fromTime, toTime, dumps);
    osi_auditU(call, BUDB_DelDmpEvent, code, AUD_DATE, id, AUD_END);
    return code;
}

#define MAXOFFS 30

afs_int32
DoDeleteDump(call, id, fromTime, toTime, dumps)
     struct rx_call *call;
     dumpId id;
     Date fromTime;
     Date toTime;
     budb_dumpsList *dumps;
{
    afs_int32 code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (id)
	code = deleteDump(call, id, dumps);
    return (code);
}

afs_int32
SBUDB_ListDumps(call, sflags, name, groupid, fromTime, toTime, dumps, flags)
     struct rx_call *call;
     afs_int32 sflags, groupid;
     char *name;
     Date fromTime, toTime;
     budb_dumpsList *dumps, *flags;
{
    afs_int32 code;

    code = ListDumps(call, sflags, groupid, fromTime, toTime, dumps, flags);
    osi_auditU(call, BUDB_LstDmpEvent, code, AUD_LONG, flags, AUD_END);
    return code;
}

afs_int32
ListDumps(call, sflags, groupid, fromTime, toTime, dumps, flags)
     struct rx_call *call;
     afs_int32 sflags, groupid;
     Date fromTime, toTime;
     budb_dumpsList *dumps, *flags;
{
    struct ubik_trans *ut;
    struct memoryHashTable *mht;
    struct dump diskDump, appDiskDump;
    dbadr dbAddr, dbAppAddr;

    afs_int32 eval, code = 0;
    int old, hash, length, entrySize, count = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return (eval);

    /* Search the database */
    mht = ht_GetType(HT_dumpIden_FUNCTION, &entrySize);
    if (!mht)
	return (BUDB_BADARGUMENT);

    for (old = 0; old <= 1; old++) {	/*o *//* old and new hash tables */
	length = (old ? mht->oldLength : mht->length);
	if (length == 0)
	    continue;

	for (hash = 0; hash < length; hash++) {	/*h *//* for each hash bucket */
	    for (dbAddr = ht_LookupBucket(ut, mht, hash, old); dbAddr; dbAddr = ntohl(diskDump.idHashChain)) {	/*d */

		/* read the entry */
		eval = dbread(ut, dbAddr, &diskDump, sizeof(diskDump));
		if (eval)
		    ABORT(eval);

		/* Skip appended dumps */
		if (ntohl(diskDump.initialDumpID) != 0) {
		    continue;
		}

		/* Skip dumps with different goup id */
		if ((sflags & BUDB_OP_GROUPID)
		    && (ntohl(diskDump.tapes.id) != groupid)) {
		    continue;	/*nope */
		}

		/* Look at this dump to see if it meets the criteria for listing */
		if (sflags & BUDB_OP_DATES) {
		    /* This and each appended dump should be in time */
		    for (dbAppAddr = dbAddr; dbAppAddr;
			 dbAppAddr = ntohl(appDiskDump.appendedDumpChain)) {
			eval =
			    dbread(ut, dbAppAddr, &appDiskDump,
				   sizeof(appDiskDump));
			if (eval)
			    ABORT(eval);

			if ((ntohl(appDiskDump.id) < fromTime)
			    || (ntohl(appDiskDump.id) > toTime))
			    break;	/*nope */
		    }
		    if (dbAppAddr)
			continue;	/*nope */
		}

		/* Add it and each of its appended dump to our list to return */
		for (dbAppAddr = dbAddr; dbAppAddr;
		     dbAppAddr = ntohl(appDiskDump.appendedDumpChain)) {
		    eval =
			dbread(ut, dbAppAddr, &appDiskDump,
			       sizeof(appDiskDump));
		    if (eval)
			ABORT(eval);

		    /* Make sure we have space to list it */
		    if (dumps->budb_dumpsList_len >= count) {
			count += 10;
			if (count == 10) {
			    dumps->budb_dumpsList_val =
				(afs_int32 *) malloc(count *
						     sizeof(afs_int32));
			    flags->budb_dumpsList_val =
				(afs_int32 *) malloc(count *
						     sizeof(afs_int32));
			} else {
			    dumps->budb_dumpsList_val =
				(afs_int32 *) realloc(dumps->
						      budb_dumpsList_val,
						      count *
						      sizeof(afs_int32));
			    flags->budb_dumpsList_val =
				(afs_int32 *) realloc(flags->
						      budb_dumpsList_val,
						      count *
						      sizeof(afs_int32));
			}
			if (!dumps->budb_dumpsList_val
			    || !dumps->budb_dumpsList_val)
			    ABORT(BUDB_NOMEM);
		    }

		    /* Add it to our list */
		    dumps->budb_dumpsList_val[dumps->budb_dumpsList_len] =
			ntohl(appDiskDump.id);
		    flags->budb_dumpsList_val[flags->budb_dumpsList_len] = 0;
		    if (ntohl(appDiskDump.initialDumpID) != 0) {
			flags->budb_dumpsList_val[flags->
						  budb_dumpsList_len] |=
			    BUDB_OP_APPDUMP;
		    }
		    if (strcmp(appDiskDump.dumpName, DUMP_TAPE_NAME) == 0) {
			flags->budb_dumpsList_val[flags->
						  budb_dumpsList_len] |=
			    BUDB_OP_DBDUMP;
		    }
		    dumps->budb_dumpsList_len++;
		    flags->budb_dumpsList_len++;
		}
	    }			/*d */
	}			/*h */
    }				/*o */

    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}

afs_int32
SBUDB_DeleteTape(call, tape)
     struct rx_call *call;
     struct budb_tapeEntry *tape;	/* tape info */
{
    afs_int32 code;

    code = DoDeleteTape(call, tape);
    osi_auditU(call, BUDB_DelTpeEvent, code, AUD_DATE,
	       (tape ? tape->dump : 0), AUD_END);
    return code;
}

afs_int32
DoDeleteTape(call, tape)
     struct rx_call *call;
     struct budb_tapeEntry *tape;	/* tape info */
{
    struct ubik_trans *ut;
    struct tape t;
    dbadr a;
    afs_int32 eval, code;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    eval = ht_LookupEntry(ut, &db.tapeName, tape->name, &a, &t);
    if (eval)
	ABORT(eval);

    eval = DeleteTape(ut, a, &t);
    if (eval)
	ABORT(eval);

    eval = FreeStructure(ut, tape_BLOCK, a);
    if (eval)
	ABORT(eval);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ABORT(eval);

    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    return code;
}

/* BUDB_DeleteVDP
 *      Deletes old information from the database for a particular dump path
 *      and volumset. This supercedes the old policy implemented in
 *      UseTape, which simply matched on the volumeset.dump. Consequently
 *      it was unable to handle name re-use.
 * entry:
 *      dsname - dumpset name, i.e. volumeset.dumpname
 *      dumpPath - full path of dump node
 *      curDumpID - current dump in progress - so that is may be excluded
 * exit:
 *      0 - ok
 *      n - some error. May or may not have deleted information.
 */

afs_int32
SBUDB_DeleteVDP(call, dsname, dumpPath, curDumpId)
     struct rx_call *call;
     char *dsname;
     char *dumpPath;
     afs_int32 curDumpId;
{
    afs_int32 code;

    code = DeleteVDP(call, dsname, dumpPath, curDumpId);
    osi_auditU(call, BUDB_DelVDPEvent, code, AUD_STR, dsname, AUD_END);
    return code;
}

afs_int32
DeleteVDP(call, dsname, dumpPath, curDumpId)
     struct rx_call *call;
     char *dsname;
     char *dumpPath;
     afs_int32 curDumpId;
{
    struct dump dump;
    dbadr dumpAddr;

    struct ubik_trans *ut;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    while (1) {
	eval = InitRPC(&ut, LOCKREAD, 1);
	if (eval)
	    return (eval);

	eval = ht_LookupEntry(ut, &db.dumpName, dsname, &dumpAddr, &dump);
	if (eval)
	    ABORT(eval);

	while (dumpAddr != 0) {	/*wd */
	    if ((strcmp(dump.dumpName, dsname) == 0)
		&& (strcmp(dump.dumpPath, dumpPath) == 0)
		&& (ntohl(dump.id) != curDumpId)) {
		eval = ubik_EndTrans(ut);
		if (eval)
		    return (eval);

		eval = deleteDump(call, ntohl(dump.id), 0);
		if (eval)
		    return (eval);

		/* start the traversal over since the various chains may
		 * have changed
		 */
		break;
	    }

	    dumpAddr = ntohl(dump.nameHashChain);
	    if (dumpAddr) {
		eval = dbread(ut, dumpAddr, &dump, sizeof(dump));
		if (eval)
		    ABORT(eval);
	    }
	}			/*wd */

	/* check if all the dumps have been examined - can terminate */
	if (!dumpAddr) {
	    eval = ubik_EndTrans(ut);
	    return (eval);
	}
    }

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}

/* BUDB_FindClone
 * notes:
 *      Given a volume name, and a dumpID, find the volume in that dump and 
 *      return the clone date of the volume (this is the clone date of the 
 *      volume at the time it was dumped).
 *   
 *      Hashes on the volume name and traverses the fragments. Will need to read 
 *      the volumes tape entry to determine if it belongs to the dump. If the
 *      volume is not found in the dump, then look for it in its parent dump.
 */

afs_int32
SBUDB_FindClone(call, dumpID, volName, clonetime)
     struct rx_call *call;
     afs_int32 dumpID;
     char *volName;
     afs_int32 *clonetime;
{
    afs_int32 code;

    code = FindClone(call, dumpID, volName, clonetime);
    osi_auditU(call, BUDB_FndClnEvent, code, AUD_STR, volName, AUD_END);
    return code;
}

afs_int32
FindClone(call, dumpID, volName, clonetime)
     struct rx_call *call;
     afs_int32 dumpID;
     char *volName;
     afs_int32 *clonetime;
{
    struct ubik_trans *ut;
    dbadr da, hvia, via, vfa;
    struct dump d;
    struct tape t;
    struct volFragment vf;
    struct volInfo vi;
    int rvi;			/* read the volInfo struct */
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return (eval);

    *clonetime = 0;

    /* Search for the volume by name */
    eval = ht_LookupEntry(ut, &db.volName, volName, &hvia, &vi);
    if (eval)
	ABORT(eval);
    if (!hvia)
	ABORT(BUDB_NOVOLUMENAME);
    rvi = 0;

    /* Follw the dump levels up */
    for (; dumpID; dumpID = ntohl(d.parent)) {	/*d */
	/* Get the dump entry */
	eval = ht_LookupEntry(ut, &db.dumpIden, &dumpID, &da, &d);
	if (eval)
	    ABORT(eval);
	if (!da)
	    ABORT(BUDB_NODUMPID);

	/* seach all the volInfo entries on the sameNameChain */
	for (via = hvia; via; via = ntohl(vi.sameNameChain)) {	/*via */
	    if (rvi) {		/* Read the volInfo entry - except first time */
		eval = dbread(ut, via, &vi, sizeof(vi));
		if (eval)
		    ABORT(eval);
	    }
	    rvi = 1;

	    /* search all the volFrag entries on the volFrag */
	    for (vfa = ntohl(vi.firstFragment); vfa; vfa = ntohl(vf.sameNameChain)) {	/*vfa */
		eval = dbread(ut, vfa, &vf, sizeof(vf));	/* Read the volFrag entry */
		if (eval)
		    ABORT(eval);

		eval = dbread(ut, ntohl(vf.tape), &t, sizeof(t));	/* Read the tape */
		if (eval)
		    ABORT(eval);

		/* Now check to see if this fragment belongs to the dump we have */
		if (ntohl(t.dump) == da) {
		    *clonetime = ntohl(vf.clone);	/* return the clone */
		    ERROR(0);
		}
	    }			/*vfa */
	}			/*via */
    }				/*d */

  error_exit:
    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_EndTrans(ut);
    return (code);
}

#ifdef notdef
/*
 *      Searches each tape and each volume in the dump until the volume is found.
 *      If the volume is not in the dump, then we search it's parent dump.
 *
 *	Re-write to do lookups by volume name.
 */
afs_int32
FindClone(call, dumpID, volName, clonetime)
     struct rx_call *call;
     afs_int32 dumpID;
     char *volName;
     afs_int32 *clonetime;
{
    struct ubik_trans *ut;
    dbadr diskAddr, tapeAddr, volFragmentAddr;
    struct dump dump;
    struct tape tape;
    struct volFragment volFragment;
    struct volInfo volInfo;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return (eval);

    *clonetime = 0;

    for (; dumpID; dumpID = ntohl(dump.parent)) {	/*d */
	/* Get the dump entry */
	eval = ht_LookupEntry(ut, &db.dumpIden, &dumpID, &diskAddr, &dump);
	if (eval)
	    ABORT(eval);
	if (!diskAddr)
	    ABORT(BUDB_NODUMPID);

	/* just to be sure */
	if (ntohl(dump.id) != dumpID) {
	    LogDebug(4, "BUDB_FindClone: requested %d, found %d\n", dumpID,
		     ntohl(dump.id));
	    ABORT(BUDB_INTERNALERROR);
	}

	/* search all the tapes in this dump */
	for (tapeAddr = ntohl(dump.firstTape); tapeAddr; tapeAddr = ntohl(tape.nextTape)) {	/*t */
	    /* Get the tape entry */
	    eval = dbread(ut, tapeAddr, &tape, sizeof(tape));
	    if (eval)
		ABORT(eval);

	    /* search all the volume fragments on this tape */
	    for (volFragmentAddr = ntohl(tape.firstVol); volFragmentAddr; volFragmentAddr = ntohl(volFragment.sameTapeChain)) {	/*vf */
		/* Get the volume fragment entry */
		eval =
		    dbread(ut, volFragmentAddr, &volFragment,
			   sizeof(volFragment));
		if (eval)
		    ABORT(eval);

		/* Get the volume info entry */
		eval =
		    dbread(ut, ntohl(volFragment.vol), &volInfo,
			   sizeof(volInfo));
		if (eval)
		    ABORT(eval);

		/* check if this volume is the one we want */
		if (strcmp(volInfo.name, volName) == 0) {
		    *clonetime = ntohl(volFragment.clone);
		    ERROR(0);
		}
	    }			/*vf */
	}			/*t */
    }				/*d */

  error_exit:
    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_EndTrans(ut);
    return (code);
}
#endif

/* BUDB_FindDump
 *      Find latest volume dump before adate.
 *      Used by restore code when restoring a user requested volume(s)
 * entry:
 *      volumeName - name of volume to match on
 *      beforeDate - look for dumps older than this date
 * exit:
 *      deptr - descriptor of most recent dump
 */

afs_int32
SBUDB_FindDump(call, volumeName, beforeDate, deptr)
     struct rx_call *call;
     char *volumeName;
     afs_int32 beforeDate;
     struct budb_dumpEntry *deptr;
{
    afs_int32 code;

    code = FindDump(call, volumeName, beforeDate, deptr);
    osi_auditU(call, BUDB_FndDmpEvent, code, AUD_STR, volumeName, AUD_END);
    return code;
}

afs_int32
FindDump(call, volumeName, beforeDate, deptr)
     struct rx_call *call;
     char *volumeName;
     afs_int32 beforeDate;
     struct budb_dumpEntry *deptr;
{
    struct ubik_trans *ut;
    dbadr volInfoAddr, volFragmentAddr;
    struct tape tape;
    struct volInfo volInfo;
    struct volFragment volFragment;

    dbadr selectedDumpAddr = 0;
    afs_int32 selectedDate = 0;
    afs_int32 volCloned;
    int rvoli;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return eval;

    /* Find volinfo struct for volume name in hash table */
    eval =
	ht_LookupEntry(ut, &db.volName, volumeName, &volInfoAddr, &volInfo);
    if (eval)
	ABORT(eval);
    if (!volInfoAddr)
	ABORT(BUDB_NOVOLUMENAME);

    /* Step through all the volinfo structures on the same name chain.
     * No need to read the first - we read it above.
     */
    for (rvoli = 0; volInfoAddr;
	 rvoli = 1, volInfoAddr = ntohl(volInfo.sameNameChain)) {
	if (rvoli) {		/* read the volinfo structure */
	    eval = dbread(ut, volInfoAddr, &volInfo, sizeof(volInfo));
	    if (eval)
		ABORT(eval);
	}

	/* step through the volfrag structures */
	for (volFragmentAddr = ntohl(volInfo.firstFragment); volFragmentAddr;
	     volFragmentAddr = ntohl(volFragment.sameNameChain)) {
	    /* read the volfrag struct */
	    eval =
		dbread(ut, volFragmentAddr, &volFragment,
		       sizeof(volFragment));
	    if (eval)
		ABORT(eval);

	    volCloned = ntohl(volFragment.clone);

	    /* now we can examine the date for most recent dump */
	    if ((volCloned > selectedDate) && (volCloned < beforeDate)) {
		/* from the volfrag struct, read the tape struct */
		eval =
		    dbread(ut, ntohl(volFragment.tape), &tape, sizeof(tape));
		if (eval)
		    ABORT(eval);

		selectedDate = volCloned;
		selectedDumpAddr = ntohl(tape.dump);
	    }
	}
    }

    if (!selectedDumpAddr)
	ABORT(BUDB_NOENT);

    eval = FillDumpEntry(ut, selectedDumpAddr, deptr);
    if (eval)
	ABORT(eval);

    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_EndTrans(ut);
    return (code);
}

/* BUDB_FindLatestDump
 *	Find the latest dump of volumeset vsname with dump name dname.
 * entry:
 *	vsname - volumeset name
 *	dname - dumpname
 */

afs_int32
SBUDB_FindLatestDump(call, vsname, dumpPath, dumpentry)
     struct rx_call *call;
     char *vsname, *dumpPath;
     struct budb_dumpEntry *dumpentry;
{
    afs_int32 code;

    code = FindLatestDump(call, vsname, dumpPath, dumpentry);
    osi_auditU(call, BUDB_FndLaDEvent, code, AUD_STR, vsname, AUD_END);
    return code;
}

afs_int32
FindLatestDump(call, vsname, dumpPath, dumpentry)
     struct rx_call *call;
     char *vsname, *dumpPath;
     struct budb_dumpEntry *dumpentry;
{
    struct ubik_trans *ut;
    dbadr curdbaddr, retdbaddr, firstdbaddr;
    struct dump d;
    Date latest;
    char dumpName[BU_MAXNAMELEN + 2];
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return (eval);

    if ((strcmp(vsname, "") == 0) && (strcmp(dumpPath, "") == 0)) {
	/* Construct a database dump name */
	strcpy(dumpName, DUMP_TAPE_NAME);
    } else if (strchr(dumpPath, '/') == 0) {
	int level, old, length, hash;
	struct dump hostDump, diskDump;
	struct memoryHashTable *mht;
	int entrySize;
	dbadr dbAddr;
	afs_uint32 bestDumpId = 0;

	level = atoi(dumpPath);
	if (level < 0) {
	    ABORT(BUDB_BADARGUMENT);
	}

	/* Brute force search of all the dumps in the database - yuck! */

	retdbaddr = 0;
	mht = ht_GetType(HT_dumpIden_FUNCTION, &entrySize);
	if (!mht)
	    ABORT(BUDB_BADARGUMENT);

	for (old = 0; old <= 1; old++) {	/*fo */
	    length = (old ? mht->oldLength : mht->length);
	    if (!length)
		continue;

	    for (hash = 0; hash < length; hash++) {
		/*f */
		for (dbAddr = ht_LookupBucket(ut, mht, hash, old); dbAddr;
		     dbAddr = hostDump.idHashChain) {
		    /*w */
		    eval = dbread(ut, dbAddr, &diskDump, sizeof(diskDump));
		    if (eval)
			ABORT(eval);
		    dump_ntoh(&diskDump, &hostDump);

		    if ((strcmp(hostDump.volumeSet, vsname) == 0) &&	/* the volumeset */
			(hostDump.level == level) &&	/* same level */
			(hostDump.id > bestDumpId)) {	/* more recent */
			bestDumpId = hostDump.id;
			retdbaddr = dbAddr;
		    }
		}		/*w */
	    }			/*f */
	}			/*fo */
	if (!retdbaddr)
	    ABORT(BUDB_NODUMPNAME);

	goto finished;
    } else {
	/* construct the name of the dump */
	if ((strlen(vsname) + strlen(tailCompPtr(dumpPath))) > BU_MAXNAMELEN)
	    ABORT(BUDB_NODUMPNAME);

	strcpy(dumpName, vsname);
	strcat(dumpName, ".");
	strcat(dumpName, tailCompPtr(dumpPath));
    }

    LogDebug(5, "lookup on :%s:\n", dumpName);

    /* Lookup on dumpname in hash table */
    eval = ht_LookupEntry(ut, &db.dumpName, dumpName, &firstdbaddr, &d);
    if (eval)
	ABORT(eval);

    latest = 0;
    retdbaddr = 0;

    /* folow remaining dumps in hash chain, looking for most latest dump */
    for (curdbaddr = firstdbaddr; curdbaddr;
	 curdbaddr = ntohl(d.nameHashChain)) {
	if (curdbaddr != firstdbaddr) {
	    eval = dbread(ut, curdbaddr, &d, sizeof(d));
	    if (eval)
		ABORT(eval);
	}

	if ((strcmp(d.dumpPath, dumpPath) == 0) &&	/* Same dumppath */
	    (strcmp(d.dumpName, dumpName) == 0) &&	/* Same dumpname */
	    (ntohl(d.created) > latest)) {	/* most recent */
	    latest = ntohl(d.created);
	    retdbaddr = curdbaddr;
	}
    }
    if (!retdbaddr)
	ABORT(BUDB_NODUMPNAME);

  finished:
    /* return the dump found */
    FillDumpEntry(ut, retdbaddr, dumpentry);

    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}


afs_int32
SBUDB_FinishDump(call, dump)
     struct rx_call *call;
     struct budb_dumpEntry *dump;
{
    afs_int32 code;

    code = FinishDump(call, dump);
    osi_auditU(call, BUDB_FinDmpEvent, code, AUD_DATE, (dump ? dump->id : 0),
	       AUD_END);
    return code;
}

afs_int32
FinishDump(call, dump)
     struct rx_call *call;
     struct budb_dumpEntry *dump;
{
    struct ubik_trans *ut;
    dbadr a;
    struct dump d;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    eval = ht_LookupEntry(ut, &db.dumpIden, &dump->id, &a, &d);
    if (eval)
	ABORT(eval);
    if (!a)
	ABORT(BUDB_NODUMPID);

    if ((ntohl(d.flags) & BUDB_DUMP_INPROGRESS) == 0)
	ABORT(BUDB_DUMPNOTINUSE);

    d.flags = htonl(dump->flags & ~BUDB_DUMP_INPROGRESS);

    /* if creation time specified set it */
    if (dump->created)
	d.created = htonl(dump->created);
    dump->created = ntohl(d.created);

    /* Write the dump entry out */
    eval = dbwrite(ut, a, &d, sizeof(d));
    if (eval)
	ABORT(eval);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ABORT(eval);

    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    return code;
}

afs_int32
SBUDB_FinishTape(call, tape)
     struct rx_call *call;
     struct budb_tapeEntry *tape;
{
    afs_int32 code;

    code = FinishTape(call, tape);
    osi_auditU(call, BUDB_FinTpeEvent, code, AUD_DATE,
	       (tape ? tape->dump : 0), AUD_END);
    return code;
}

afs_int32
FinishTape(call, tape)
     struct rx_call *call;
     struct budb_tapeEntry *tape;
{
    struct ubik_trans *ut;
    dbadr a;
    struct tape t;
    struct dump d;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    /* find the tape struct in the tapename hash chain */
    eval = ht_LookupEntry(ut, &db.tapeName, tape->name, &a, &t);
    if (eval)
	ABORT(eval);
    if (!a)
	ABORT(BUDB_NOTAPENAME);

    /* Read the dump structure */
    eval = dbread(ut, ntohl(t.dump), &d, sizeof(d));
    if (eval)
	ABORT(eval);

    /* search for the right tape on the rest of the chain */
    while (ntohl(d.id) != tape->dump) {
	a = ntohl(t.nameHashChain);
	if (!a)
	    ABORT(BUDB_NOTAPENAME);

	eval = dbread(ut, a, &t, sizeof(t));
	if (eval)
	    ABORT(eval);

	eval = dbread(ut, ntohl(t.dump), &d, sizeof(d));
	if (eval)
	    ABORT(eval);
    }

    if ((ntohl(t.flags) & BUDB_TAPE_BEINGWRITTEN) == 0)
	ABORT(BUDB_TAPENOTINUSE);

    /* t.nBytes = htonl(tape->nBytes); */
    t.nFiles = htonl(tape->nFiles);
    t.useKBytes = htonl(tape->useKBytes);
    t.flags = htonl(tape->flags & ~BUDB_TAPE_BEINGWRITTEN);

    eval = dbwrite(ut, a, &t, sizeof(t));
    if (eval)
	ABORT(BUDB_IO);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ABORT(eval);

    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    return code;
}

/* BUDB_GetDumps
 *	return a set of dumps that match the specified criteria
 * entry:
 *	call - rx call
 *	majorVersion - version of interface structures. Permits compatibility
 *		checks to be made
 *	flags - for search and select operations. Broken down into flags
 *		for name, start point, end point and time.
 *	name - name to search for. Interpretation based on flags
 *	end 
 *	index
 *	nextIndexP
 *	dbTimeP
 * exit:
 *	nextIndexP 
 *	dbTimeP - time at which the database was last modified. Up to
 *		caller (client) to take appropriate action if database
 *		modified between successive calls
 *	dumps - list of matching dumps
 * notes:
 *	currently supported are:
 *	BUDB_OP_DUMPNAME
 *	BUDB_OP_DUMPID
 */

afs_int32
SBUDB_GetDumps(call, majorVersion, flags, name, start, end, index, nextIndexP,
	       dbTimeP, dumps)
     struct rx_call *call;
     afs_int32 majorVersion;	/* version of interface structures */
     afs_int32 flags;		/* search & select controls */
     char *name;		/* s&s parameters */
     afs_int32 start;
     afs_int32 end;
     afs_int32 index;		/* start index of returned entries */
     afs_int32 *nextIndexP;	/* output index for next call */
     afs_int32 *dbTimeP;
     budb_dumpList *dumps;	/* pointer to buffer */
{
    afs_int32 code;

    code =
	GetDumps(call, majorVersion, flags, name, start, end, index,
		 nextIndexP, dbTimeP, dumps);
    osi_auditU(call, BUDB_GetDmpEvent, code, AUD_END);
    return code;
}

afs_int32
GetDumps(call, majorVersion, flags, name, start, end, index, nextIndexP,
	 dbTimeP, dumps)
     struct rx_call *call;
     afs_int32 majorVersion;	/* version of interface structures */
     afs_int32 flags;		/* search & select controls */
     char *name;		/* s&s parameters */
     afs_int32 start;
     afs_int32 end;
     afs_int32 index;		/* start index of returned entries */
     afs_int32 *nextIndexP;	/* output index for next call */
     afs_int32 *dbTimeP;
     budb_dumpList *dumps;	/* pointer to buffer */
{
    struct ubik_trans *ut;
    dbadr da;
    struct dump d;
    afs_int32 nameFlags, startFlags, endFlags, timeFlags;
    afs_int32 eval, code = 0;
    afs_int32 toskip;
    struct returnList list;

    /* Don't check permissions when we look up a specific dump id */
    if (((flags & BUDB_OP_STARTS) != BUDB_OP_DUMPID) && !callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (majorVersion != BUDB_MAJORVERSION)
	return BUDB_OLDINTERFACE;
    if (index < 0)
	return BUDB_ENDOFLIST;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return eval;

    nameFlags = flags & BUDB_OP_NAMES;
    startFlags = flags & BUDB_OP_STARTS;
    endFlags = flags & BUDB_OP_ENDS;
    timeFlags = flags & BUDB_OP_TIMES;

    InitReturnList(&list);
    toskip = index;

    if (nameFlags == BUDB_OP_DUMPNAME) {
	/* not yet implemented */
	if (startFlags || endFlags || timeFlags)
	    ABORT(BUDB_BADFLAGS);

	eval = ht_LookupEntry(ut, &db.dumpName, name, &da, &d);
	if (eval)
	    ABORT(eval);
	if (!da)
	    ABORT(BUDB_NODUMPNAME);

	while (1) {
	    if (strcmp(d.dumpName, name) == 0) {
		eval = AddToReturnList(&list, da, &toskip);
		if (eval == BUDB_LIST2BIG)
		    break;
		if (eval)
		    ABORT(eval);
	    }

	    da = ntohl(d.nameHashChain);	/* get next dump w/ name */
	    if (!da)
		break;

	    eval = dbread(ut, da, &d, sizeof(d));
	    if (eval)
		ABORT(eval);
	}
    } else if (nameFlags == BUDB_OP_VOLUMENAME) {
#ifdef PA
	struct volInfo vi;

	LogError(0, "NYI, BUDB_OP_VOLUMENAME\n");
	ABORT(BUDB_BADFLAGS);


	if (startFlags != BUDB_OP_STARTTIME)
	    ABORT(BUDB_BADFLAGS);

	/* lookup a dump by volumename and time stamp. Find the most recent
	 * dump of the specified volumename, that occured before the supplied
	 * time
	 */

	/* get us a volInfo for name */
	eval = ht_LookupEntry(ut, &db.volName, name, &da, &vi);
	if (eval)
	    ABORT(eval);

	while (1) {
	    /* now iterate over all the entries of this name */
	    for (va = vi.firstFragment; va != 0; va = v.sameNameChain) {
		va = ntohl(va);
		eval = dbread(ut, va, &v, sizeof(v));
		if (eval)
		    ABORT(eval);

		if date
		    on fragment > date ignore it - too recent;

		if (date on fragment < date && date on fragment > bestfound)
		    bestfound = date on fragment;

	    }			/* for va */

	    da = vi.sameNameChain;
	    if (da == 0)
		break;
	    da = ntohl(da);
	    eval = dbread(ut, da, &vi, sizeof(vi));
	    if (eval)
		ABORT(eval);
	}

/*
	if nothing found
		return error

	from saved volfragment address, compute dump.
	otherwise, return dump found
*/

#endif /* PA */

    } else if (startFlags == BUDB_OP_DUMPID) {
	if (endFlags || timeFlags)
	    ABORT(BUDB_BADFLAGS);
	if (nameFlags)
	    ABORT(BUDB_BADFLAGS);	/* NYI */

	eval = ht_LookupEntry(ut, &db.dumpIden, &start, &da, &d);
	if (eval)
	    ABORT(eval);
	if (!da)
	    ABORT(BUDB_NODUMPID);

	eval = AddToReturnList(&list, da, &toskip);
	if (eval)
	    ABORT(eval);
    } else if (endFlags == BUDB_OP_NPREVIOUS) {
	struct wantDumpRock rock;
	struct chosenDump *ptr, *nextPtr;

	extern wantDump(), rememberDump();

	/* no other flags should be set */

	/* end specifies how many dumps */
	if (!end)
	    ABORT(BUDB_BADFLAGS);

	memset(&rock, 0, sizeof(rock));
	rock.maxDumps = end;

	scanHashTable(ut, &db.dumpName, wantDump, rememberDump,
		      (char *)&rock);

	for (ptr = rock.chain; ptr; ptr = nextPtr) {
	    nextPtr = ptr->next;
	    AddToReturnList(&list, ptr->addr, &toskip);	/* ignore error for free */
	    free(ptr);
	}
    } else {
	ABORT(BUDB_BADFLAGS);
    }

    eval =
	SendReturnList(ut, &list, FillDumpEntry,
		       sizeof(struct budb_dumpEntry), index, nextIndexP,
		       dbTimeP, (returnList_t) dumps);
    if (eval)
	ABORT(eval);

    FreeReturnList(&list);
    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    FreeReturnList(&list);
    ubik_AbortTrans(ut);
    return code;
}

/* 
 * Get the expiration of a tape.  Since the dump could have appended dumps,
 * we should use the most recent expiration date. Put the most recent 
 * expiration tape into the given tape structure.
 */
afs_int32
getExpiration(ut, tapePtr)
     struct ubik_trans *ut;
     struct tape *tapePtr;
{
    dbadr ad;
    struct dump d;
    struct tape t;
    afs_int32 initDump;
    afs_int32 eval, code = 0;

    if (!tapePtr)
	ERROR(0);

    /* Get the dump for this tape */
    ad = ntohl(tapePtr->dump);
    eval = dbread(ut, ad, &d, sizeof(d));
    if (eval)
	ERROR(eval);

    /* If not an initial dump, get the initial dump */
    if (d.initialDumpID) {
	initDump = ntohl(d.initialDumpID);
	eval = ht_LookupEntry(ut, &db.dumpIden, &initDump, &ad, &d);
	if (eval)
	    ERROR(eval);
    }

    /* Cycle through the dumps and appended dumps */
    while (ad) {
	/* Get the first tape in this dump. No need to check the rest of the tapes */
	/* for this dump since they will all have the same expiration date */
	eval = dbread(ut, ntohl(d.firstTape), &t, sizeof(t));
	if (eval)
	    ERROR(eval);

	/* Take the greater of the expiration dates */
	if (ntohl(tapePtr->expires) < ntohl(t.expires))
	    tapePtr->expires = t.expires;

	/* Step to and read the next appended dump */
	if (ad = ntohl(d.appendedDumpChain)) {
	    eval = dbread(ut, ad, &d, sizeof(d));
	    if (eval)
		ERROR(eval);
	}
    }

  error_exit:
    return (code);
}

/* Mark the following dump as appended to another, intial dump */
afs_int32
makeAppended(ut, appendedDumpID, initialDumpID, startTapeSeq)
     struct ubik_trans *ut;
     afs_int32 appendedDumpID;
     afs_int32 initialDumpID;
     afs_int32 startTapeSeq;
{
    dbadr ada, da, lastDumpAddr;
    struct dump ad, d;
    afs_int32 eval, code = 0;

    if (!initialDumpID)
	ERROR(0);
    if (appendedDumpID == initialDumpID)
	ERROR(BUDB_INTERNALERROR);

    /* If there is an initial dump, append this dump to it */
    /* Find the appended dump via its id */
    eval = ht_LookupEntry(ut, &db.dumpIden, &appendedDumpID, &ada, &ad);
    if (eval)
	ERROR(eval);

    /* If the dump is already marked as appended,
     * then we have an internal error.
     */
    if (ad.initialDumpID) {
	if (ntohl(ad.initialDumpID) != initialDumpID)
	    ERROR(BUDB_INTERNALERROR);
    }

    /* Update the appended dump to point to the initial dump */
    ad.initialDumpID = htonl(initialDumpID);
    ad.tapes.b = htonl(startTapeSeq);

    /* find the initial dump via its id */
    eval = ht_LookupEntry(ut, &db.dumpIden, &initialDumpID, &da, &d);
    if (eval)
	ERROR(eval);

    /* Update the appended dump's tape format with that of the initial */
    strcpy(ad.tapes.format, d.tapes.format);

    /* starting with the initial dump step through its appended dumps till 
     * we reach the last appended dump. 
     */
    lastDumpAddr = da;
    while (d.appendedDumpChain) {
	lastDumpAddr = ntohl(d.appendedDumpChain);
	if (lastDumpAddr == ada)
	    ERROR(0);		/* Already appended */
	eval = dbread(ut, lastDumpAddr, &d, sizeof(d));
	if (eval)
	    ERROR(eval);
    }

    /* Update the last dump to point to our new appended dump.
     * The appended dump is the last one in the dump chain.
     */
    d.appendedDumpChain = htonl(ada);
    ad.appendedDumpChain = 0;

    /* Write the appended dump and the initial dump */
    eval = dbwrite(ut, ada, (char *)&ad, sizeof(ad));
    if (eval)
	ERROR(eval);

    eval = dbwrite(ut, lastDumpAddr, (char *)&d, sizeof(d));
    if (eval)
	ERROR(eval);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ERROR(eval);

  error_exit:
    return (code);
}

afs_int32
SBUDB_MakeDumpAppended(call, appendedDumpID, initialDumpID, startTapeSeq)
     struct rx_call *call;
     afs_int32 appendedDumpID;
     afs_int32 initialDumpID;
     afs_int32 startTapeSeq;
{
    afs_int32 code;

    code =
	MakeDumpAppended(call, appendedDumpID, initialDumpID, startTapeSeq);
    osi_auditU(call, BUDB_AppDmpEvent, code, AUD_LONG, appendedDumpID,
	       AUD_END);
    return code;
}

afs_int32
MakeDumpAppended(call, appendedDumpID, initialDumpID, startTapeSeq)
     struct rx_call *call;
     afs_int32 appendedDumpID;
     afs_int32 initialDumpID;
     afs_int32 startTapeSeq;
{
    struct ubik_trans *ut;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return (eval);

    eval = makeAppended(ut, appendedDumpID, initialDumpID, startTapeSeq);
    if (eval)
	ABORT(eval);

    code = ubik_EndTrans(ut);
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}

/* Find the last tape of a dump-set. This includes any appended dumps */
afs_int32
SBUDB_FindLastTape(call, dumpID, dumpEntry, tapeEntry, volEntry)
     struct rx_call *call;
     afs_int32 dumpID;
     struct budb_dumpEntry *dumpEntry;
     struct budb_tapeEntry *tapeEntry;
     struct budb_volumeEntry *volEntry;
{
    afs_int32 code;

    code = FindLastTape(call, dumpID, dumpEntry, tapeEntry, volEntry);
    osi_auditU(call, BUDB_FndLTpeEvent, code, AUD_LONG, dumpID, AUD_END);
    return code;
}

afs_int32
FindLastTape(call, dumpID, dumpEntry, tapeEntry, volEntry)
     struct rx_call *call;
     afs_int32 dumpID;
     struct budb_dumpEntry *dumpEntry;
     struct budb_tapeEntry *tapeEntry;
     struct budb_volumeEntry *volEntry;
{
    struct ubik_trans *ut;
    struct dump d;
    dbadr lastDump;
    struct tape t;
    dbadr lastTape, thisTape;
    afs_int32 lastTapeSeq;
    struct volFragment vf;
    dbadr lastVol, thisVol;
    afs_int32 lastVolPos;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (!dumpID)
	return (BUDB_BADARGUMENT);

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return (eval);

    /* find and read its initial dump via its id */
    eval = ht_LookupEntry(ut, &db.dumpIden, &dumpID, &lastDump, &d);
    if (eval)
	ABORT(eval);
    if (!lastDump)
	ABORT(BUDB_NODUMPID);

    /* Follow the append dumps link chain until we reach the last dump */
    while (d.appendedDumpChain) {
	lastDump = ntohl(d.appendedDumpChain);
	eval = dbread(ut, lastDump, &d, sizeof(d));
	if (eval)
	    ABORT(eval);
    }

    /* We now have the last dump of the last appended dump */
    /* Copy this into our return structure */
    eval = FillDumpEntry(ut, lastDump, dumpEntry);
    if (eval)
	ABORT(eval);

    /* Fail if the last dump has no tapes */
    if (!d.firstTape)
	ABORT(BUDB_NOTAPENAME);

    /* Follow the tapes in this dump until we reach the last tape */
    eval = dbread(ut, ntohl(d.firstTape), &t, sizeof(t));
    if (eval)
	ABORT(eval);

    lastTape = ntohl(d.firstTape);
    lastTapeSeq = ntohl(t.seq);
    lastVol = ntohl(t.firstVol);

    while (t.nextTape) {
	thisTape = ntohl(t.nextTape);
	eval = dbread(ut, thisTape, &t, sizeof(t));
	if (eval)
	    ABORT(eval);

	if (ntohl(t.seq) > lastTapeSeq) {
	    lastTape = thisTape;
	    lastTapeSeq = ntohl(t.seq);
	    lastVol = ntohl(t.firstVol);
	}
    }

    /* We now have the last tape of the last appended dump */
    /* Copy this into our return structure */
    eval = FillTapeEntry(ut, lastTape, tapeEntry);
    if (eval)
	ABORT(eval);

    /* Zero volume entry if the last tape has no volumes */
    if (!lastVol) {
	memset(volEntry, 0, sizeof(*volEntry));
    } else {
	/* Follow the volumes until we reach the last volume */
	eval = dbread(ut, lastVol, &vf, sizeof(vf));
	if (eval)
	    ABORT(eval);

	lastVolPos = vf.position;

	while (vf.sameTapeChain) {
	    thisVol = ntohl(vf.sameTapeChain);
	    eval = dbread(ut, thisVol, &vf, sizeof(vf));
	    if (eval)
		ABORT(eval);

	    if (vf.position > lastVolPos) {
		lastVol = thisVol;
		lastVolPos = vf.position;
	    }
	}

	/* We now have the last volume of this tape */
	/* Copy this into our return structure */
	eval = FillVolEntry(ut, lastVol, volEntry);
	if (eval)
	    ABORT(eval);
    }

    eval = ubik_EndTrans(ut);
    if (!code)
	code = eval;
    return (code);

  abort_exit:
    ubik_AbortTrans(ut);
    return (code);
}


afs_int32
SBUDB_GetTapes(call, majorVersion, flags, name, start, end, index, nextIndexP,
	       dbTimeP, tapes)
     struct rx_call *call;
     afs_int32 majorVersion;	/* version of interface structures */
     afs_int32 flags;		/* search & select controls */
     char *name;		/* s&s parameters */
     afs_int32 start;
     afs_int32 end;		/* reserved: MBZ */
     afs_int32 index;		/* start index of returned entries */
     afs_int32 *nextIndexP;	/* output index for next call */
     afs_int32 *dbTimeP;
     budb_tapeList *tapes;	/* pointer to buffer */
{
    afs_int32 code;

    code =
	GetTapes(call, majorVersion, flags, name, start, end, index,
		 nextIndexP, dbTimeP, tapes);
    osi_auditU(call, BUDB_GetTpeEvent, code, AUD_END);
    return code;
}

afs_int32
GetTapes(call, majorVersion, flags, name, start, end, index, nextIndexP,
	 dbTimeP, tapes)
     struct rx_call *call;
     afs_int32 majorVersion;	/* version of interface structures */
     afs_int32 flags;		/* search & select controls */
     char *name;		/* s&s parameters */
     afs_int32 start;
     afs_int32 end;		/* reserved: MBZ */
     afs_int32 index;		/* start index of returned entries */
     afs_int32 *nextIndexP;	/* output index for next call */
     afs_int32 *dbTimeP;
     budb_tapeList *tapes;	/* pointer to buffer */
{
    struct ubik_trans *ut;
    dbadr da, ta;
    struct dump d;
    struct tape t;
    afs_int32 nameFlags, startFlags, endFlags, timeFlags;
    struct returnList list;
    afs_int32 eval, code = 0;
    afs_int32 toskip;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (majorVersion != BUDB_MAJORVERSION)
	return BUDB_OLDINTERFACE;

    if (index < 0)
	return BUDB_ENDOFLIST;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return eval;

    nameFlags = flags & BUDB_OP_NAMES;
    startFlags = flags & BUDB_OP_STARTS;
    endFlags = flags & BUDB_OP_ENDS;
    timeFlags = flags & BUDB_OP_TIMES;

    InitReturnList(&list);
    toskip = index;

    if (nameFlags == BUDB_OP_TAPENAME) {	/*it */
	eval = ht_LookupEntry(ut, &db.tapeName, name, &ta, &t);
	if (eval)
	    ABORT(eval);
	if (!ta)
	    ABORT(BUDB_NOTAPENAME);

	/* NYI */
	if ((startFlags & ~BUDB_OP_DUMPID) || endFlags || timeFlags)
	    ABORT(BUDB_BADFLAGS);

	/* follow the hash chain to the end */
	while (ta) {		/*w */
	    if (startFlags & BUDB_OP_DUMPID) {
		/* read in the dump */
		eval = dbread(ut, ntohl(t.dump), &d, sizeof(d));
		if (eval)
		    ABORT(eval);

		/* check if both name and dump id match */
		if ((strcmp(name, t.name) == 0) && (ntohl(d.id) == start)) {
		    eval = AddToReturnList(&list, ta, &toskip);
		    if (eval && (eval != BUDB_LIST2BIG))
			ABORT(eval);
		    break;
		}
	    } else {
		/* Add to return list and continue search */
		if (strcmp(name, t.name) == 0) {
		    eval = AddToReturnList(&list, ta, &toskip);
		    if (eval == BUDB_LIST2BIG)
			break;
		    if (eval)
			ABORT(eval);
		}
	    }

	    ta = ntohl(t.nameHashChain);
	    if (ta)
		dbread(ut, ta, &t, sizeof(t));
	}			/*w */
    } /*it */
    else if (nameFlags == BUDB_OP_TAPESEQ) {
	eval = ht_LookupEntry(ut, &db.dumpIden, &start, &da, &d);
	if (eval)
	    ABORT(eval);
	if (!da)
	    ABORT(BUDB_NODUMPNAME);

	/* search for the right tape */
	ta = ntohl(d.firstTape);
	for (ta = ntohl(d.firstTape); ta; ta = ntohl(t.nextTape)) {
	    eval = dbread(ut, ta, &t, sizeof(t));
	    if (eval)
		ABORT(eval);

	    if (ntohl(t.seq) == end) {
		eval = AddToReturnList(&list, ta, &toskip);
		if (eval && (eval != BUDB_LIST2BIG))
		    ABORT(eval);
		break;
	    }
	}
    } else {
	ABORT(BUDB_BADFLAGS);
    }

    eval =
	SendReturnList(ut, &list, FillTapeEntry,
		       sizeof(struct budb_tapeEntry), index, nextIndexP,
		       dbTimeP, (returnList_t) tapes);
    if (eval)
	ABORT(eval);

    FreeReturnList(&list);
    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    FreeReturnList(&list);
    ubik_AbortTrans(ut);
    return (code);
}

/* BUDB_GetVolumes
 *	get a set of volumes according to the specified criteria.
 *	See BUDB_GetDumps for general information on parameters
 *	Currently supports:
 *	1) volume match - returns volumes based on volume name only.
 *	2) flags = BUDB_OP_DUMPID in which case name is a volume name
 *		and start is a dumpid. Returns all volumes of the specified
 *		name on the selected dumpid.
 */

afs_int32
SBUDB_GetVolumes(call, majorVersion, flags, name, start, end, index,
		 nextIndexP, dbTimeP, volumes)
     struct rx_call *call;
     afs_int32 majorVersion;	/* version of interface structures */
     afs_int32 flags;		/* search & select controls */
     char *name;		/*  - parameters for search */
     afs_int32 start;		/*  - usage depends which BUDP_OP_* */
     afs_int32 end;		/*  - bits are set */
     afs_int32 index;		/* start index of returned entries */
     afs_int32 *nextIndexP;	/* output index for next call */
     afs_int32 *dbTimeP;
     budb_volumeList *volumes;	/* pointer to buffer */
{
    afs_int32 code;

    code =
	GetVolumes(call, majorVersion, flags, name, start, end, index,
		   nextIndexP, dbTimeP, volumes);
    osi_auditU(call, BUDB_GetVolEvent, code, AUD_END);
    return code;
}

afs_int32
GetVolumes(call, majorVersion, flags, name, start, end, index, nextIndexP,
	   dbTimeP, volumes)
     struct rx_call *call;
     afs_int32 majorVersion;	/* version of interface structures */
     afs_int32 flags;		/* search & select controls */
     char *name;		/*  - parameters for search */
     afs_int32 start;		/*  - usage depends which BUDP_OP_* */
     afs_int32 end;		/*  - bits are set */
     afs_int32 index;		/* start index of returned entries */
     afs_int32 *nextIndexP;	/* output index for next call */
     afs_int32 *dbTimeP;
     budb_volumeList *volumes;	/* pointer to buffer */
{
    struct ubik_trans *ut;
    dbadr via;
    struct volInfo vi;
    afs_int32 nameFlags, startFlags, endFlags, timeFlags;
    afs_int32 eval, code = 0;
    struct returnList vollist;
    afs_int32 toskip;

    /* Don't check permissions when we look up a specific volume name */
    if (((flags & BUDB_OP_NAMES) != BUDB_OP_VOLUMENAME)
	&& !callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (majorVersion != BUDB_MAJORVERSION)
	return BUDB_OLDINTERFACE;
    if (index < 0)
	return BUDB_ENDOFLIST;

    eval = InitRPC(&ut, LOCKREAD, 1);
    if (eval)
	return eval;

    nameFlags = flags & BUDB_OP_NAMES;
    startFlags = flags & BUDB_OP_STARTS;
    endFlags = flags & BUDB_OP_ENDS;
    timeFlags = flags & BUDB_OP_TIMES;

    InitReturnList(&vollist);
    toskip = index;

    /* lookup a the volume (specified by name) in the dump (specified by id) */
    if (nameFlags == BUDB_OP_VOLUMENAME) {
	/* dumpid permissible, all others off */
	if (((startFlags & ~BUDB_OP_DUMPID) != 0) || endFlags || timeFlags)
	    ABORT(BUDB_BADFLAGS);

	/* returns ptr to volinfo of requested name */
	eval = ht_LookupEntry(ut, &db.volName, name, &via, &vi);
	if (eval)
	    ABORT(eval);
	if (!via)
	    ABORT(BUDB_NOVOLUMENAME);

	/* Iterate over all volume fragments with this name */
	while (1) {
	    struct volFragment v;
	    afs_int32 va;

	    /* traverse all the volume fragments for this volume info structure */
	    for (va = vi.firstFragment; va; va = v.sameNameChain) {
		va = ntohl(va);
		eval = dbread(ut, va, &v, sizeof(v));
		if (eval)
		    ABORT(eval);

		if (startFlags & BUDB_OP_DUMPID) {
		    struct tape atape;
		    struct dump adump;

		    /* get the dump id for this fragment */
		    eval = dbread(ut, ntohl(v.tape), &atape, sizeof(atape));
		    if (eval)
			ABORT(eval);

		    eval =
			dbread(ut, ntohl(atape.dump), &adump, sizeof(adump));
		    if (eval)
			ABORT(BUDB_IO);

		    /* dump id does not match */
		    if (ntohl(adump.id) != start)
			continue;
		}

		eval = AddToReturnList(&vollist, va, &toskip);
		if (eval == BUDB_LIST2BIG)
		    break;
		if (eval)
		    ABORT(eval);
	    }
	    if (eval == BUDB_LIST2BIG)
		break;

	    via = vi.sameNameChain;
	    if (via == 0)
		break;
	    via = ntohl(via);

	    eval = dbread(ut, via, &vi, sizeof(vi));
	    if (eval)
		ABORT(eval);
	}
    } else if (((nameFlags == 0) || (nameFlags == BUDB_OP_TAPENAME))
	       && (startFlags == BUDB_OP_DUMPID)) {
	struct dump dump;
	dbadr dumpAddr;
	struct tape tape;
	dbadr tapeAddr;
	struct volFragment volFrag;
	dbadr volFragAddr;

	/* lookup all volumes for a specified dump id */

	/* no other flags should be set */
	if (endFlags || timeFlags)
	    ABORT(BUDB_BADFLAGS);

	/* find the dump */
	eval = ht_LookupEntry(ut, &db.dumpIden, &start, &dumpAddr, &dump);
	if (eval)
	    ABORT(eval);

	/* traverse all the tapes */
	for (tapeAddr = ntohl(dump.firstTape); tapeAddr; tapeAddr = ntohl(tape.nextTape)) {	/*w */
	    eval = dbread(ut, tapeAddr, &tape, sizeof(tape));
	    if (eval)
		ABORT(eval);

	    if ((nameFlags != BUDB_OP_TAPENAME)
		|| ((nameFlags == BUDB_OP_TAPENAME)
		    && (strcmp(tape.name, name) == 0))) {
		/* now return all the volumes */
		for (volFragAddr = ntohl(tape.firstVol); volFragAddr;
		     volFragAddr = ntohl(volFrag.sameTapeChain)) {
		    eval = dbread(ut, volFragAddr, &volFrag, sizeof(volFrag));
		    if (eval)
			ABORT(eval);

		    eval = AddToReturnList(&vollist, volFragAddr, &toskip);
		    if (eval == BUDB_LIST2BIG)
			break;
		    if (eval)
			ABORT(eval);
		}
	    }
	    if (eval == BUDB_LIST2BIG)
		break;
	}			/*w */
    } else {
	ABORT(BUDB_BADFLAGS);
    }

    eval =
	SendReturnList(ut, &vollist, FillVolEntry,
		       sizeof(struct budb_volumeEntry), index, nextIndexP,
		       dbTimeP, (returnList_t) volumes);
    if (eval)
	ABORT(eval);

  error_exit:
    FreeReturnList(&vollist);
    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    FreeReturnList(&vollist);
    ubik_AbortTrans(ut);
    return code;
}

afs_int32
SBUDB_UseTape(call, tape, new)
     struct rx_call *call;
     struct budb_tapeEntry *tape;	/* tape info */
     afs_int32 *new;		/* set if tape is new */
{
    afs_int32 code;

    code = UseTape(call, tape, new);
    osi_auditU(call, BUDB_UseTpeEvent, code, AUD_DATE,
	       (tape ? tape->dump : 0), AUD_END);
    return code;
}

afs_int32
UseTape(call, tape, new)
     struct rx_call *call;
     struct budb_tapeEntry *tape;	/* tape info */
     int *new;			/* set if tape is new */
{
    struct ubik_trans *ut;
    dbadr da, a;
    struct dump d;
    struct tape t;
    afs_int32 eval, code;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (strlen(tape->name) >= sizeof(t.name))
	return BUDB_BADARGUMENT;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    *new = 0;

    memset(&t, 0, sizeof(t));
    eval = AllocStructure(ut, tape_BLOCK, 0, &a, &t);
    if (eval)
	ABORT(eval);

    strcpy(t.name, tape->name);

    eval = ht_HashIn(ut, &db.tapeName, a, &t);
    if (eval)
	ABORT(eval);

    *new = 1;

    /* Since deleting a tape may change the dump (if its the same one), read in
     * the dump after the call to DeleteTape. */

    eval = ht_LookupEntry(ut, &db.dumpIden, &tape->dump, &da, &d);
    if (eval)
	ABORT(eval);
    if (!da)
	ABORT(BUDB_NODUMPID);

    if (!tape->written)
	tape->written = time(0);	/* fill in tape struct */
    t.written = htonl(tape->written);
    t.expires = htonl(tape->expires);
    t.dump = htonl(da);
    t.seq = htonl(tape->seq);
    t.useCount = htonl(tape->useCount);
    t.labelpos = htonl(tape->labelpos);
    t.useKBytes = 0;
    t.flags = htonl(tape->flags | BUDB_TAPE_BEINGWRITTEN);

    t.nextTape = d.firstTape;	/* Chain the tape to the dump */
    d.firstTape = htonl(a);

    if (tape->seq >= ntohl(d.tapes.maxTapes))	/* inc # tapes in the dump */
	d.tapes.maxTapes = htonl(tape->seq);

    eval = dbwrite(ut, a, &t, sizeof(t));	/* write tape struct */
    if (eval)
	ABORT(eval);

    eval = dbwrite(ut, da, &d, sizeof(d));	/* write the dump struct */
    if (eval)
	ABORT(eval);

    eval = set_header_word(ut, lastUpdate, htonl(time(0)));
    if (eval)
	ABORT(eval);

    LogDebug(5, "added tape %s\n", tape->name);

    code = ubik_EndTrans(ut);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    return code;

}


/* ---------------------------------------------
 * debug interface routines
 * ---------------------------------------------
 */

afs_int32
SBUDB_T_DumpHashTable(call, type, filename)
     struct rx_call *call;
     afs_int32 type;
     char *filename;
{
    afs_int32 code;

    code = T_DumpHashTable(call, type, filename);
    osi_auditU(call, BUDB_TDmpHaEvent, code, AUD_STR, filename, AUD_END);
    return code;
}

afs_int32
T_DumpHashTable(call, type, filename)
     struct rx_call *call;
     int type;
     char *filename;
{
    struct ubik_trans *ut;
    struct memoryHashTable *mht;
    int ent;
    afs_int32 eval, code = 0;
    char path[64];
    FILE *DUMP;

    int length;
    afs_uint32 hash;
    dbadr a, first_a;
    char e[sizeof(struct block)];	/* unnecessarily conservative */
    int e_size;
    int old;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    if (strlen(filename) >= sizeof(path) - 5)
	return BUDB_BADARGUMENT;

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return eval;

    if ((mht = ht_GetType(type, &e_size)) == 0)
	return BUDB_BADARGUMENT;

    sprintf(path, "%s/%s", gettmpdir(), filename);

    DUMP = fopen(path, "w");
    if (!DUMP)
	ABORT(BUDB_BADARGUMENT);

    ent = 0;
    for (old = 0;; old++) {
	length = (old ? mht->oldLength : mht->length);
	if (length)
	    fprintf(DUMP, "Dumping %sHash Table:\n", (old ? "Old " : ""));

	for (hash = 0; hash < length; hash++) {
	    a = ht_LookupBucket(ut, mht, hash, old);
	    first_a = a;
	    while (a) {
		eval = dbread(ut, a, e, e_size);
		if (eval)
		    ABORT(eval);

		ent++;
		if (a == first_a)
		    fprintf(DUMP, "  in bucket %d at %d is ", hash, a);
		else
		    fprintf(DUMP, "    at %d is ", a);
		switch (type) {
		case HT_dumpIden_FUNCTION:
		    fprintf(DUMP, "%d\n", ntohl(((struct dump *)e)->id));
		    break;
		case HT_dumpName_FUNCTION:
		    fprintf(DUMP, "%s\n", ((struct dump *)e)->dumpName);
		    break;
		case HT_tapeName_FUNCTION:
		    fprintf(DUMP, "%s\n", ((struct tape *)e)->name);
		    break;
		case HT_volName_FUNCTION:
		    fprintf(DUMP, "%s\n", ((struct volInfo *)e)->name);
		    break;
		}
		if ((ht_HashEntry(mht, e) % length) != hash)
		    ABORT(BUDB_DATABASEINCONSISTENT);
		a = ntohl(*(dbadr *) (e + mht->threadOffset));
	    }
	}
	if (old)
	    break;
    }

    fprintf(DUMP, "%d entries found\n", ent);
    if (ntohl(mht->ht->entries) != ent)
	ABORT(BUDB_DATABASEINCONSISTENT);

    code = ubik_EndTrans(ut);
    if (DUMP)
	fclose(DUMP);
    return code;

  abort_exit:
    ubik_AbortTrans(ut);
    if (DUMP)
	fclose(DUMP);
    return code;
}

afs_int32
SBUDB_T_GetVersion(call, majorVersion)
     struct rx_call *call;
     afs_int32 *majorVersion;
{
    afs_int32 code;

    code = T_GetVersion(call, majorVersion);
    osi_auditU(call, BUDB_TGetVrEvent, code, AUD_END);
    return code;
}

afs_int32
T_GetVersion(call, majorVersion)
     struct rx_call *call;
     int *majorVersion;
{
    struct ubik_trans *ut;
    afs_int32 code;

    code = InitRPC(&ut, LOCKREAD, 0);
    if (code)
	return (code);

    *majorVersion = BUDB_MAJORVERSION;

    code = ubik_EndTrans(ut);
    return (code);
}

/* BUDB_T_DumpDatabase
 *	dump as much of the database as possible int /tmp/<filename>
 */

afs_int32
SBUDB_T_DumpDatabase(call, filename)
     struct rx_call *call;
     char *filename;
{
    afs_int32 code;

    code = T_DumpDatabase(call, filename);
    osi_auditU(call, BUDB_TDmpDBEvent, code, AUD_STR, filename, AUD_END);
    return code;
}

afs_int32
T_DumpDatabase(call, filename)
     struct rx_call *call;
     char *filename;
{
    FILE *dumpfid;
    int entrySize;
    struct ubik_trans *ut;
    char *path = 0;
    dbadr dbAddr;
    int type, old, length, hash;
    struct memoryHashTable *mht;
    afs_int32 eval, code = 0;

    if (!callPermitted(call))
	return BUDB_NOTPERMITTED;

    path = (char *)malloc(strlen(gettmpdir()) + 1 + strlen(filename) + 1);
    if (!path)
	return (BUDB_INTERNALERROR);

    sprintf(path, "%s/%s", gettmpdir(), filename);

    dumpfid = fopen(path, "w");
    if (!dumpfid)
	return (BUDB_BADARGUMENT);

    eval = InitRPC(&ut, LOCKWRITE, 1);
    if (eval)
	return (eval);

    /* dump all items in the database */
    for (type = 1; type <= HT_MAX_FUNCTION; type++) {	/*ft */
	mht = ht_GetType(type, &entrySize);
	if (!mht)
	    ERROR(BUDB_BADARGUMENT);

	for (old = 0; old <= 1; old++) {	/*fo */
	    length = (old ? mht->oldLength : mht->length);
	    if (!length)
		continue;

	    fprintf(dumpfid, "Dumping %s Hash Table:\n", (old ? "Old " : ""));

	    for (hash = 0; hash < length; hash++) {	/*f */
		dbAddr = ht_LookupBucket(ut, mht, hash, old);

		while (dbAddr) {	/*w */
		    switch (type) {	/*s */
		    case HT_dumpIden_FUNCTION:
			{
			    struct dump hostDump, diskDump;

			    eval =
				cdbread(ut, dump_BLOCK, dbAddr, &diskDump,
					sizeof(diskDump));
			    if (eval)
				ERROR(eval);

			    fprintf(dumpfid,
				    "\ndumpId hash %d, entry at %u\n",
				    hash, dbAddr);
			    fprintf(dumpfid,
				    "----------------------------\n");
			    dump_ntoh(&diskDump, &hostDump);
			    printDump(dumpfid, &hostDump);
			    dbAddr = hostDump.idHashChain;
			}
			break;

		    case HT_dumpName_FUNCTION:
			{
			    struct dump hostDump, diskDump;

			    eval =
				cdbread(ut, dump_BLOCK, dbAddr, &diskDump,
					sizeof(diskDump));
			    if (eval)
				ERROR(eval);

			    fprintf(dumpfid,
				    "\ndumpname hash %d, entry at %u\n",
				    hash, dbAddr);
			    fprintf(dumpfid,
				    "----------------------------\n");
			    dump_ntoh(&diskDump, &hostDump);
			    printDump(dumpfid, &hostDump);
			    dbAddr = hostDump.nameHashChain;
			}
			break;

		    case HT_tapeName_FUNCTION:
			{
			    struct tape hostTape, diskTape;

			    eval =
				cdbread(ut, tape_BLOCK, dbAddr, &diskTape,
					sizeof(diskTape));
			    if (eval)
				ERROR(eval);

			    fprintf(dumpfid,
				    "\ntapename hash %d, entry at %u\n",
				    hash, dbAddr);
			    fprintf(dumpfid,
				    "----------------------------\n");
			    tape_ntoh(&diskTape, &hostTape);
			    printTape(dumpfid, &hostTape);
			    dbAddr = hostTape.nameHashChain;
			}
			break;

		    case HT_volName_FUNCTION:
			{
			    struct volInfo hostVolInfo, diskVolInfo;

			    eval =
				cdbread(ut, volInfo_BLOCK, dbAddr,
					&diskVolInfo, sizeof(diskVolInfo));
			    if (eval)
				ERROR(eval);

			    fprintf(dumpfid,
				    "\nvolname hash %d, entry at %u\n",
				    hash, dbAddr);
			    fprintf(dumpfid,
				    "----------------------------\n");
			    volInfo_ntoh(&diskVolInfo, &hostVolInfo);
			    printVolInfo(dumpfid, &hostVolInfo);
			    dbAddr = hostVolInfo.nameHashChain;

			    volFragsDump(ut, dumpfid,
					 hostVolInfo.firstFragment);
			}
			break;

		    default:
			fprintf(dumpfid, "unknown type %d\n", type);
			break;

		    }		/*s */
		}		/*w */
	    }			/*f */
	}			/*fo */
    }				/*ft */

  error_exit:
    code = ubik_EndTrans(ut);	/* is this safe if no ut started ? */
    if (dumpfid)
	fclose(dumpfid);
    if (path)
	free(path);
    return (code);
}

int
volFragsDump(ut, dumpfid, dbAddr)
     struct ubik_trans *ut;
     FILE *dumpfid;
     dbadr dbAddr;
{
    struct volFragment hostVolFragment, diskVolFragment;
    afs_int32 code;

    while (dbAddr) {
	code =
	    cdbread(ut, volFragment_BLOCK, dbAddr, &diskVolFragment,
		    sizeof(diskVolFragment));
	if (code) {		/* don't be fussy about errors */
	    fprintf(dumpfid, "volFragsDump: Error reading database\n");
	    return (0);
	}

	fprintf(dumpfid, "\nvolfragment entry at %u\n", dbAddr);
	fprintf(dumpfid, "----------------------------\n");
	volFragment_ntoh(&diskVolFragment, &hostVolFragment);
	printVolFragment(dumpfid, &hostVolFragment);
	dbAddr = hostVolFragment.sameNameChain;
    }
    return (0);
}

#ifdef notdef
/* utilities - network to host conversion
 *	currently used for debug only
 */

volFragmentDiskToHost(diskVfPtr, hostVfPtr)
     struct volFragment *diskVfPtr, *hostVfPtr;
{
    hostVfPtr->vol = ntohl(diskVfPtr->vol);
    hostVfPtr->sameNameChain = ntohl(diskVfPtr->sameNameChain);
    hostVfPtr->tape = ntohl(diskVfPtr->tape);
    hostVfPtr->sameTapeChain = ntohl(diskVfPtr->sameTapeChain);
    hostVfPtr->position = ntohl(diskVfPtr->position);
    hostVfPtr->clone = ntohl(diskVfPtr->clone);
    hostVfPtr->incTime = ntohl(diskVfPtr->incTime);
    hostVfPtr->startByte = ntohl(diskVfPtr->startByte);
    hostVfPtr->nBytes = ntohl(diskVfPtr->nBytes);
    hostVfPtr->flags = ntohs(diskVfPtr->flags);
    hostVfPtr->sequence = ntohs(diskVfPtr->sequence);
}

volInfoDiskToHost(diskViPtr, hostViPtr)
     struct volInfo *diskViPtr, *hostViPtr;
{
    strcpy(hostViPtr->name, diskViPtr->name);
    hostViPtr->nameHashChain = ntohl(diskViPtr->nameHashChain);
    hostViPtr->id = ntohl(diskViPtr->id);
    strcpy(hostViPtr->server, diskViPtr->server);
    hostViPtr->partition = ntohl(diskViPtr->partition);
    hostViPtr->flags = ntohl(diskViPtr->flags);
    hostViPtr->sameNameHead = ntohl(diskViPtr->sameNameHead);
    hostViPtr->sameNameChain = ntohl(diskViPtr->sameNameChain);
    hostViPtr->firstFragment = ntohl(diskViPtr->firstFragment);
    hostViPtr->nFrags = ntohl(diskViPtr->nFrags);
}

tapeDiskToHost(diskTapePtr, hostTapePtr)
     struct tape *diskTapePtr, *hostTapePtr;
{
    strcpy(hostTapePtr->name, diskTapePtr->name);
    hostTapePtr->nameHashChain = ntohl(diskTapePtr->nameHashChain);
    hostTapePtr->flags = ntohl(diskTapePtr->flags);

    /* tape id conversion here */
    hostTapePtr->written = ntohl(diskTapePtr->written);
    hostTapePtr->nBytes = ntohl(diskTapePtr->nBytes);
    hostTapePtr->nFiles = ntohl(diskTapePtr->nFiles);
    hostTapePtr->nVolumes = ntohl(diskTapePtr->nVolumes);
    hostTapePtr->seq = ntohl(diskTapePtr->seq);
    hostTapePtr->dump = ntohl(diskTapePtr->dump);
    hostTapePtr->nextTape = ntohl(diskTapePtr->nextTape);
    hostTapePtr->firstVol = ntohl(diskTapePtr->firstVol);
    hostTapePtr->useCount = ntohl(diskTapePtr->useCount);
}

dumpDiskToHost(diskDumpPtr, hostDumpPtr)
     struct dump *diskDumpPtr, *hostDumpPtr;
{
    hostDumpPtr->id = ntohl(diskDumpPtr->id);
    hostDumpPtr->idHashChain = ntohl(diskDumpPtr->idHashChain);
    strcpy(hostDumpPtr->dumpName, diskDumpPtr->dumpName);
    strcpy(hostDumpPtr->dumpPath, diskDumpPtr->dumpPath);
    strcpy(hostDumpPtr->volumeSet, diskDumpPtr->volumeSet);
    hostDumpPtr->nameHashChain = ntohl(diskDumpPtr->nameHashChain);
    hostDumpPtr->flags = ntohl(diskDumpPtr->flags);
    hostDumpPtr->parent = ntohl(diskDumpPtr->parent);
    hostDumpPtr->created = ntohl(diskDumpPtr->created);
/*  hostDumpPtr->incTime = ntohl(diskDumpPtr->incTime); */
    hostDumpPtr->nVolumes = ntohl(diskDumpPtr->nVolumes);

    /* tapeset conversion here */

    hostDumpPtr->firstTape = ntohl(diskDumpPtr->firstTape);

    /* principal conversion here */
}

#endif /* notdef */

int
checkHash(ut, hashType)
     struct ubik_trans *ut;
     int hashType;
{
    struct memoryHashTable *mhtPtr;
    int entrySize, hashTableLength;
    int bucket;
    int old;
    afs_int32 code = 0;

    mhtPtr = ht_GetType(hashType, &entrySize);
    if (mhtPtr == 0)
	ERROR(-1);

    for (old = 0; old < 1; old++) {
	LogDebug(5, "\nold = %d\n", old);
	printMemoryHashTable(stdout, mhtPtr);
	LogDebug(5, "\n");
	hashTableLength = (old ? mhtPtr->oldLength : mhtPtr->length);

	for (bucket = 0; bucket < hashTableLength; bucket++) {
	    dbadr entryAddr;

	    entryAddr = ht_LookupBucket(ut, mhtPtr, bucket, old);
	    while (entryAddr != 0) {
		LogDebug(6, "bucket %d has disk addr %d\n", bucket,
			 entryAddr);
		switch (hashType) {
		case HT_dumpIden_FUNCTION:
		    {
			struct dump diskDump, hostDump;

			code = dbread(ut, entryAddr, &diskDump, entrySize);
			if (code)
			    ERROR(-1);

			dump_ntoh(&diskDump, &hostDump);
			printDump(stdout, &hostDump);
			entryAddr = hostDump.idHashChain;
		    }
		    break;

		case HT_dumpName_FUNCTION:
		    break;

		case HT_tapeName_FUNCTION:
		    break;

		case HT_volName_FUNCTION:
		    {
			struct volInfo diskVolInfo, hostVolInfo;

			code = dbread(ut, entryAddr, &diskVolInfo, entrySize);
			if (code)
			    ERROR(-1);

			volInfo_ntoh(&diskVolInfo, &hostVolInfo);
			printVolInfo(stdout, &hostVolInfo);
			entryAddr = hostVolInfo.nameHashChain;
			break;
		    }
		}
	    }
	}
    }
  error_exit:
    return (code);
}
