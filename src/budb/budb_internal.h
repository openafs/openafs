/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BUDB_PROTOTYPES_H
#define _BUDB_PROTOTYPES_H

/* db_alloc.c */
afs_int32 InitDBalloc(void);
afs_int32 AllocStructure(struct ubik_trans *ut, char type, dbadr related,
		         dbadr *saP, void *s);
afs_int32 FreeStructure(struct ubik_trans *ut, char type, dbadr sa);
afs_int32 AllocBlock(struct ubik_trans *, struct block *, dbadr *);
afs_int32 FreeBlock(struct ubik_trans *, struct blockHeader *, dbadr);

/* db_dump.c */
afs_int32 writeDatabase(struct ubik_trans *, int);

/* db_hash.c */

afs_int32 InitDBhash(void);
void ht_DBInit(void);
afs_int32 ht_HashOut(struct ubik_trans *ut, struct memoryHashTable *mht,
		     dbadr ea, void *e);
afs_int32 RemoveFromList(struct ubik_trans *ut, dbadr ea, void *e,
			 dbadr *head, dbadr ta, void *t, dbadr *thread);
afs_int32 ht_LookupEntry(struct ubik_trans *ut, struct memoryHashTable *mht,
			  void *key, dbadr *eaP, void *e);
afs_int32 ht_HashIn(struct ubik_trans *ut, struct memoryHashTable *mht,
		    dbadr ea, void *e);
afs_int32 scanHashTable(struct ubik_trans *ut, struct memoryHashTable *mhtPtr,
		        int (*selectFn) (dbadr, void *, void *),
			int (*operationFn) (dbadr, void *, void *),
			void *rockPtr);

/* db_lock.c */
int checkLockHandle(struct ubik_trans *, afs_uint32);

/* dbs_dump.c */
afs_int32 badEntry(afs_uint32 dbAddr);

/* ol_verify.c */
afs_int32 checkDiskAddress(unsigned long, int, int *, int *);

/* procs.c */
afs_int32 InitProcs(void);
int callPermitted(struct rx_call *);
afs_int32 InitRPC(struct ubik_trans **, int lock, int this_op);

/* server.c */

void Log(char *fmt, ... )
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);
void LogError(long code, char *fmt, ... )
    AFS_ATTRIBUTE_FORMAT(__printf__, 2, 3);
void LogDebug(int level, char *fmt, ... )
    AFS_ATTRIBUTE_FORMAT(__printf__, 2, 3);

/* struct_ops.c */

void tape_ntoh(struct tape *, struct tape *);
void principal_hton(struct budb_principal *, struct budb_principal *);
void principal_ntoh(struct budb_principal *, struct budb_principal *);
void structDumpHeader_hton(struct structDumpHeader *,
			   struct structDumpHeader *);
int tapeSet_ntoh(struct budb_tapeSet *, struct budb_tapeSet *);
int tapeSet_hton(struct budb_tapeSet *, struct budb_tapeSet *);
void volInfo_ntoh(struct volInfo *, struct volInfo *);
void volFragment_ntoh(struct volFragment *, struct volFragment *);
void dump_ntoh(struct dump *, struct dump *);
int dumpToBudbDump(dbDumpP, struct budb_dumpEntry *);
int tapeToBudbTape(struct tape *r, struct budb_tapeEntry *);
int volsToBudbVol(struct volFragment *, struct volInfo *,
		  struct budb_volumeEntry *);
void printDump(FILE *, struct dump *);
int printPrincipal(struct budb_principal *ptr);
int printTape(FILE *, struct tape *);
int printTapeSet(struct budb_tapeSet *, afs_int32);
int printVolInfo(FILE *, struct volInfo *);
int printVolFragment(FILE *, struct volFragment *);
int printMemoryHashTable(FILE *, struct memoryHashTable *);

/* database.c */
afs_int32 CheckInit(struct ubik_trans *, int (*db_init) (struct ubik_trans *));
afs_int32 InitDB(void);

#endif
