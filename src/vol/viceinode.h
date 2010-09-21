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
	Module:		viceinode.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef _VICEINODE_H_
#define _VICEINODE_H_

/* The four inode parameters for most inodes (files, directories,
   symbolic links) */
struct InodeParams {
    VolId volumeId;
    VnodeId vnodeNumber;
    Unique vnodeUniquifier;
    FileVersion inodeDataVersion;
};

/* The four inode parameters for special inodes, i.e. the descriptive
   inodes for a volume */
struct SpecialInodeParams {
    VolId volumeId;
    VnodeId vnodeNumber;	/* this must be INODESPECIAL */
#ifdef	AFS_3DISPARES
    VolId parentId;
    int type;
#else
    int type;
    VolId parentId;
#endif
};

/* Structure of individual records output by fsck.
   When VICEMAGIC inodes are created, they are given four parameters;
   these correspond to the params.fsck array of this record.
 */
struct ViceInodeInfo {
    Inode inodeNumber;
    afs_fsize_t byteCount;
    int linkCount;
    union {
	bit32 param[4];
	struct InodeParams vnode;
	struct SpecialInodeParams special;
    } u;
};

#ifdef	AFS_3DISPARES
#define INODESPECIAL	0x1fffffff
#else
#define INODESPECIAL	0xffffffff
#endif
/* Special inode types.  Be careful of the ordering.  Must start at 1.
   See vutil.h */
#define VI_VOLINFO	1	/* The basic volume information file */
#define VI_SMALLINDEX	2	/* The index of small vnodes */
#define VI_LARGEINDEX	3	/* The index of large vnodes */
#define VI_ACL		4	/* The volume's access control list */
#define	VI_MOUNTTABLE	5	/* The volume's mount table */
#define VI_LINKTABLE	6	/* The volume's link counts */

#endif /* _VICEINODE_H_ */
