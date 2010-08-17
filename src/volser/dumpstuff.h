/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _VOLSER_DUMPSTUFF_H
#define _VOLSER_DUMPSTUFF_H

/* This iod stuff is a silly little package to emulate the old qi_in stuff,
 * which emulated the stdio stuff.  There is a big assumption here, that the
 * rx_Read will never be called directly, by a routine like readFile, when
 * there is an old character that was pushed back with iod_ungetc.  This
 * is really bletchy, and is here for compatibility only.  Eventually,
 * we should define a volume format that doesn't require the pushing back
 * of characters (i.e. characters should not double both as an end marker
 * and a begin marker)
 */
struct iod {
    struct rx_call *call;	/* call to which to write, might be an array */
    int device;			/* dump device ID for volume */
    int parentId;		/* dump parent ID for volume */
    struct DiskPartition64 *dumpPartition;	/* Dump partition. */
    struct rx_call **calls;	/* array of pointers to calls */
    int ncalls;			/* how many calls/codes in array */
    int *codes;			/* one return code for each call */
    char haveOldChar;		/* state for pushing back a character */
    char oldChar;
};

extern int DumpVolume(struct rx_call *call, Volume *vp, afs_int32, int);
extern int DumpVolMulti(struct rx_call **, int, Volume *, afs_int32, int,
		        int *);
extern int RestoreVolume(struct rx_call *, Volume *, int,
			 struct restoreCookie *);
extern int SizeDumpVolume(struct rx_call *, Volume *, afs_int32, int,
			  struct volintSize *);

#endif
