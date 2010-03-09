/* Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BUTC_INTERNAL_H
#define _BUTC_INTERNAL_H

/* dbentries.c */
extern afs_int32 useTape(struct budb_tapeEntry *, afs_int32, char *,
		         afs_int32, afs_int32, Date, Date, afs_int32);
extern afs_int32 addVolume(struct budb_volumeEntry *, afs_int32, char *,
			   char *, afs_int32, Date, afs_int32, afs_int32,
			   int, afs_int32);
extern afs_int32 finishTape(struct budb_tapeEntry *, afs_int32);
extern afs_int32 flushSavedEntries(afs_int32);
extern void waitDbWatcher(void);
extern afs_int32 finishDump(struct budb_dumpEntry *);
extern afs_int32 threadEntryDir(void *, afs_int32, afs_int32);

/* list.c */
extern afs_int32 allocTaskId(void);

/* lwps.h */
extern int ReadLabel(struct tc_tapeLabel *);
extern void unmountTape(afs_int32, struct butm_tapeInfo *);
extern int tapeExpired(struct butm_tapeLabel *);
extern afs_int32 PromptForTape(int, char *, afs_uint32, afs_uint32, int);
extern void GetNewLabel(struct butm_tapeInfo *, char *, char *,
			struct butm_tapeLabel *);
extern void FFlushInput(void);
extern afs_int32 ReadVolHeader(afs_int32, struct butm_tapeInfo *,
			       struct volumeHeader *);
extern int FindVolTrailer(char *, afs_int32, afs_int32 *,
			  struct volumeHeader *);
extern int FindVolTrailer2(char *, afs_int32, afs_int32 *, char *, afs_int32,
			   afs_int32 *, struct volumeHeader *);
extern int GetResponseKey(int, char *);


/* recoverDb.c */
extern afs_int32 Ask(char *);
extern int extractTapeSeq(char *);
extern int databaseTape(char *);

/* tcprocs.c */

extern int callPermitted(struct rx_call *);

/* tcstatus.c */
extern int checkAbortByTaskId(afs_uint32);

#endif

