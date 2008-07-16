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

/* Used by vutil.c and salvager.c */

private struct VolumeHeader tempHeader;

#ifdef AFS_NAMEI_ENV
#define NO_LINK_TABLE 0
#else
#define NO_LINK_TABLE 1
#endif
private struct stuff {
    struct versionStamp stamp;
    bit32 inodeType;
    int size;			/* size of any fixed size portion of the header */
    Inode *inode;
    char *description;
    /* if this is 1, then this inode is obsolete--
     * salvager may delete it, and shouldn't complain
     * if it isn't there; create can not bother to create it */
    int obsolete;
} stuff[] = {
    { {
    VOLUMEINFOMAGIC, VOLUMEINFOVERSION}, VI_VOLINFO, sizeof(VolumeDiskData),
	&tempHeader.volumeInfo, "Volume information", 0}
    , { {
    SMALLINDEXMAGIC, SMALLINDEXVERSION}
    , VI_SMALLINDEX, sizeof(struct versionStamp), &tempHeader.smallVnodeIndex,
	"small inode index", 0}, { {
    LARGEINDEXMAGIC, LARGEINDEXVERSION}, VI_LARGEINDEX,
	sizeof(struct versionStamp), &tempHeader.largeVnodeIndex,
	"large inode index", 0}, { {
    ACLMAGIC, ACLVERSION}, VI_ACL, sizeof(struct versionStamp),
	&tempHeader.volumeAcl, "access control list", 1}, { {
    MOUNTMAGIC, MOUNTVERSION}, VI_MOUNTTABLE, sizeof(struct versionStamp),
	&tempHeader.volumeMountTable, "mount table", 1}, { {
LINKTABLEMAGIC, LINKTABLEVERSION}, VI_LINKTABLE,
	sizeof(struct versionStamp), &tempHeader.linkTable, "link table",
	NO_LINK_TABLE},};
/* inodeType is redundant in the above table;  it used to be useful, but now
   we require the table to be ordered */
#define MAXINODETYPE VI_LINKTABLE

Volume *VWaitAttachVolume();

#endif /* __volinodes_h_ */
