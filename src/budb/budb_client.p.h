/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __BUDB_CLIENT__
#define __BUDB_CLIENT__

#include <ubik.h>
#ifndef BUDB_MAJORVERSION		/* get the installed RPC stuff */
#include <sys/types.h>
#include <rx/xdr.h>
#include <afs/budb.h>
#endif


/* for ubik_Call_SingleServer */

#define UF_SINGLESERVER         1       /* begin single server operation */
#define UF_END_SINGLESERVER     2       /* terminate single server operation */


/* handle for the ubik database connection */

struct udbHandleS
{
    afs_int32 uh_scIndex;                            /* what type of sec. object */
    struct rx_securityClass *uh_secobj;                 /* security object */
    struct rx_connection *uh_serverConn[MAXSERVERS];    /* server connections*/
    struct ubik_client *uh_client;              /* ubik client handle */
    afs_uint32 uh_instanceId;			/* instance of client */
};

typedef struct udbHandleS       udbHandleT;
typedef udbHandleT              *udbHandleP;

/* suggested text block management structure */

struct udbClientTextS
{
    char *textName;				/* for info. only */
    afs_int32  textType;				/* used as key for access */
    afs_uint32 textVersion;				/* version # for cache mgmt */
    afs_uint32 lockHandle;				/* for atomicity */
    afs_int32 textSize;				/* no. of bytes */
    FILE *textStream;				/* file stream or NULL */
};

typedef struct udbClientTextS	udbClientTextT;
typedef udbClientTextT		*udbClientTextP;

extern afs_int32 BUDB_AddVolume();
extern afs_int32 BUDB_AddVolumes();
extern afs_int32 BUDB_CreateDump ();
extern afs_int32 BUDB_DeleteDump ();
extern afs_int32 BUDB_ListDumps ();
extern afs_int32 BUDB_DeleteTape ();
extern afs_int32 BUDB_DeleteVDP();
extern afs_int32 BUDB_FindClone();
extern afs_int32 BUDB_FindDump();
extern afs_int32 BUDB_FindLatestDump();
extern afs_int32 BUDB_FindLastTape();
extern afs_int32 BUDB_MakeDumpAppended();
extern afs_int32 BUDB_FinishDump ();
extern afs_int32 BUDB_FinishTape ();
extern afs_int32 BUDB_GetDumps ();
extern afs_int32 BUDB_GetTapes ();
extern afs_int32 BUDB_GetVolumes ();
extern afs_int32 BUDB_UseTape ();

/* text mgmt interface */
extern afs_int32 BUDB_GetText();
extern afs_int32 BUDB_GetTextVersion();
extern afs_int32 BUDB_SaveText();
extern afs_int32 BUDB_SaveTextVersion();

/* text lock mgmt interface */

extern afs_int32 BUDB_FreeAllLocks();
extern afs_int32 BUDB_FreeLock();
extern afs_int32 BUDB_GetInstanceId();
extern afs_int32 BUDB_GetLock();

/* Database verification and dump */

extern afs_int32 BUDB_DbVerify();
extern afs_int32 BUDB_DumpDB();
extern afs_int32 BUDB_RestoreDbHeader();

/* testing interface */

extern afs_int32 BUDB_T_GetVersion();
extern afs_int32 BUDB_T_DumpHashTable ();
extern afs_int32 BUDB_T_DumpDatabase();

#endif
