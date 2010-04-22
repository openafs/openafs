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
	Module:		volinodes.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef __volinodes_h_
#define __volinodes_h_

#include <stddef.h>

/* Used by vutil.c and salvager.c */

#ifdef AFS_NAMEI_ENV
#define NO_LINK_TABLE 0
#else
#define NO_LINK_TABLE 1
#endif

#define MAXINODETYPE VI_LINKTABLE
static struct afs_inode_info {
    struct versionStamp stamp;
    bit32 inodeType;
    int size;			/* size of any fixed size portion of the header */
    Inode *inode;
    char *description;
    /* if this is 1, then this inode is obsolete--
     * salvager may delete it, and shouldn't complain
     * if it isn't there; create can not bother to create it */
    int obsolete;
} afs_common_inode_info[MAXINODETYPE] = {
    {
	{VOLUMEINFOMAGIC, VOLUMEINFOVERSION},
	VI_VOLINFO,
	sizeof(VolumeDiskData),
	(Inode*)offsetof(struct VolumeHeader, volumeInfo),
	"Volume information",
	0
    },
    {
	{SMALLINDEXMAGIC, SMALLINDEXVERSION},
	VI_SMALLINDEX,
	sizeof(struct versionStamp),
	(Inode*)offsetof(struct VolumeHeader, smallVnodeIndex),
	"small inode index",
	0
    },
    {
	{LARGEINDEXMAGIC, LARGEINDEXVERSION},
	VI_LARGEINDEX,
	sizeof(struct versionStamp),
	(Inode*)offsetof(struct VolumeHeader, largeVnodeIndex),
	"large inode index",
	0
    },
    {
	{ACLMAGIC, ACLVERSION},
	VI_ACL,
	sizeof(struct versionStamp),
	(Inode*)offsetof(struct VolumeHeader, volumeAcl),
	"access control list",
	1
    },
    {
	{MOUNTMAGIC, MOUNTVERSION},
	VI_MOUNTTABLE,
	sizeof(struct versionStamp),
	(Inode*)offsetof(struct VolumeHeader, volumeMountTable),
	"mount table",
	1
    },
    {
	{LINKTABLEMAGIC, LINKTABLEVERSION},
	VI_LINKTABLE,
	sizeof(struct versionStamp),
	(Inode*)offsetof(struct VolumeHeader, linkTable),
	"link table",
	NO_LINK_TABLE
    },
};
/* inodeType is redundant in the above table;  it used to be useful, but now
   we require the table to be ordered */

/**
 * initialize a struct afs_inode_info
 *
 * @param[in] header  the volume header struct to use when referencing volume
 *                    header fields in 'stuff'
 * @param[out] stuff  the struct afs_inode_info to initialize
 *
 */
static_inline void
init_inode_info(struct VolumeHeader *header,
                struct afs_inode_info *stuff)
{
    int i;
    memcpy(stuff, afs_common_inode_info, sizeof(afs_common_inode_info));
    for (i = 0; i < MAXINODETYPE; i++) {
	stuff[i].inode = (Inode*)((char*)header + (uintptr_t)stuff[i].inode);
    }
}

#endif /* __volinodes_h_ */
