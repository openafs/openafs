/* Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BUCOORD_PROTOTYPES_H
#define _BUCOORD_PROTOTYPES_H

/* dlq.c */
extern int dlqEmpty(dlqlinkP);
extern int dlqInit(dlqlinkP);
extern void dlqUnlink(dlqlinkP );
extern int dlqLinkb(dlqlinkP, dlqlinkP);
extern int dlqLinkf(dlqlinkP, dlqlinkP);

extern int dlqTraverseQueue(dlqlinkP, int (*)(void *), int (*)(void *));

/* status.c */
extern void initStatus(void);
extern statusP findStatus(afs_uint32);
extern void lock_Status(void);
extern void unlock_Status(void);
extern void deleteStatusNode(statusP ptr);
extern statusP createStatusNode(void);

/* volstub.c */
struct vldbentry;
extern afs_int32 bc_GetEntryByID(struct ubik_client *uclient,
				 afs_int32 volID, afs_int32 volType,
				 struct vldbentry *vldbEntryPtr);

/* ubik_db_if.c */
extern int bc_LockText(udbClientTextP ctPtr);
extern int bc_UnlockText(udbClientTextP ctPtr);
extern int bcdb_SaveTextFile(udbClientTextP ctPtr);
extern int bcdb_FindDumpByID(afs_int32, struct budb_dumpEntry *);
extern int bcdb_FindLastTape(afs_int32, struct budb_dumpEntry *,
			     struct budb_tapeEntry *,
			     struct budb_volumeEntry *);
extern afs_int32 bcdb_deleteDump(afs_int32, afs_int32, afs_int32,
				 budb_dumpsList *);
extern int bcdb_MakeDumpAppended(afs_int32, afs_int32, afs_int32);
extern afs_int32 bcdb_CreateDump(struct budb_dumpEntry *) ;
extern int bcdb_FindLatestDump(char *, char *, struct budb_dumpEntry *);
extern afs_int32 bcdb_FindClone(afs_int32, char *, afs_int32 *);
extern int bcdb_FinishDump(struct budb_dumpEntry *);
extern int bcdb_UseTape(struct budb_tapeEntry *, afs_int32 *);
extern int bcdb_FinishTape(struct budb_tapeEntry *);
extern int bcdb_FindTapeSeq(afs_int32 dumpid, afs_int32 tapeSeq,
		            struct budb_tapeEntry *teptr);
extern afs_int32 bcdb_AddVolume(struct budb_volumeEntry *);
extern afs_int32 bcdb_AddVolumes(struct budb_volumeEntry *,
				 afs_int32 );
extern afs_int32 udbClientInit(int noAuthFlag, int localauth, char *cellName);
struct ktc_token;
extern int vldbClientInit(int noAuthFlag, int localauth, char *cellName,
                          struct ubik_client **cstruct, time_t *expires);
#endif

