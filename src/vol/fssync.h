/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		fssync.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */


/* FSYNC commands */

#define FSYNC_ON		1	/* Volume online */
#define FSYNC_OFF		2	/* Volume offline */
#define FSYNC_LISTVOLUMES	3	/* Update local volume list */
#define FSYNC_NEEDVOLUME	4	/* Put volume in whatever mode (offline, or whatever)
					 * best fits the attachment mode provided in reason */
#define FSYNC_MOVEVOLUME	5	/* Generate temporary relocation information
					 * for this volume to another site, to be used
					 * if this volume disappears */
#define	FSYNC_RESTOREVOLUME	6	/* Break all the callbacks on this volume since                                   it is being restored */
#define FSYNC_DONE		7	/* Done with this volume (used after a delete).
					 * Don't put online, but remove from list */


/* Reasons (these could be communicated to venus or converted to messages) */

#define FSYNC_WHATEVER		0	/* XXXX */
#define FSYNC_SALVAGE		1	/* volume is being salvaged */
#define FSYNC_MOVE		2	/* volume is being moved */
#define FSYNC_OPERATOR		3	/* operator forced volume offline */


/* Replies (1 byte) */

#define FSYNC_DENIED		0
#define FSYNC_OK		1


/* Prototypes from fssync.c */
void FSYNC_clientFinis(void);
int FSYNC_clientInit(int);
void FSYNC_fsInit(void);
int FSYNC_askfs(VolumeId volume, char *partName, int com, int reason);
