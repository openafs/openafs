/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _VOL_PROTOTYPES_H
#define _VOL_PROTOTYPES_H

/* clone.c */
extern void CloneVolume(Error *, Volume *, Volume *, Volume *);
extern int (*vol_PollProc) (void);

/* nuke.c */
extern int nuke(char *, afs_int32);

/* vutil.c */
extern void AssignVolumeName(VolumeDiskData * vol, char *name, char *ext);
extern void AssignVolumeName_r(VolumeDiskData * vol, char *name, char *ext);
extern void ClearVolumeStats(VolumeDiskData * vol);
extern void ClearVolumeStats_r(VolumeDiskData * vol);
extern void CopyVolumeStats(VolumeDiskData * from, VolumeDiskData * to);
extern void CopyVolumeStats_r(VolumeDiskData * from, VolumeDiskData * to);
extern afs_int32 CopyVolumeHeader(VolumeDiskData *, VolumeDiskData *);

#endif

