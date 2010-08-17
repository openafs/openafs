/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
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
#define CBHash(t) ((t)>>7)

#define	CBQTOV(e)	    ((struct vcache *)(((char *) (e)) - (((char *)(&(((struct vcache *)(e))->callsort))) - ((char *)(e)))))
