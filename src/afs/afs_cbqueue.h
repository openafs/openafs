/* (C) Copyright 1992 Transarc Corporation
 * Licensed materials - property of Transarc
 * All rights reserved.
 *
 * This package is used to actively manage the expiration of callbacks,
 * so that the rest of the cache manager doesn't need to compute
 * whether a callback has expired or not, but can tell with one simple
 * check, that is, whether the CStatd bit is on or off.
 */

/* how many seconds in a slot */
#define CBHTSLOTLEN 128
/* how many slots in the table */
#define CBHTSIZE    128
/* 7 is LOG2(slotlen) */
#define CBHash(t) (t>>7)

extern int afs_BumpBase();
extern void afs_InitCBQueue();
extern void afs_CheckCallbacks();
extern void afs_DequeueCallback();
extern void afs_QueueCallback();

#define	CBQTOV(e)	    ((struct vcache *)(((char *) (e)) - (((char *)(&(((struct vcache *)(e))->callsort))) - ((char *)(e)))))















