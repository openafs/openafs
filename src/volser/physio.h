/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _VOLSER_PHYSIO_H
#define _VOLSER_PHYSIO_H

/* physio.c */
extern void SetSalvageDirHandle(DirHandle *, afs_uint32, afs_int32, Inode);
extern void FidZap(DirHandle *);

#endif
