/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_XFSATTRS_H
#define AFS_XFSATTRS_H
#ifdef AFS_SGI_XFS_IOPS_ENV

/* This header file contains the definitions of the XFS attributes used to
 * implement vice inodes for the XFS fileserver.
 */

#include <sys/attributes.h>

/* Parameters in at_param are:
 * volume special file:
 * 0 volume
 * 1 INODESPECIAL
 * 2 special file type
 * 3 parent volume
 *
 * regular AFS file:
 * 0 RW volume
 * 1 vnode
 * 2 uniquifier
 * 3 data version
 */
typedef int afs_inode_params_t[4];	/* Use to pass param[4] to kernel. */

/* XFS attribute must be at most 36 bytes to fit in inode. This includes
 * the null terminated name of the attribute.
 */

#define AFS_XFS_ATTR "AFS"
/* The version number covers all of the attribute of the vice inode including
 * mode, uid and gid.
 */
#define AFS_XFS_ATTR_VERS ((char)1)

/* The name version is the version of the scheme used to name vice inodes.
 * version 1 is /vicepX/.<volume-id>/.<uniquifier>.<tag> All numbers are
 * in base 64. Use the utilities in util/base64.c for conversion.
 * Version 2 is /vicepX/.afsinodes.<volume-id>/.<uniquifier>.<tag>
 */
#define AFS_XFS_NAME_VERS1 ((char)1)
#define AFS_XFS_NAME_VERS2 ((char)2)
#define AFS_XFS_NAME_VERS  AFS_XFS_NAME_VERS2
#define AFS_INODE_DIR_NAME ".afsinodes."

/* Unfortunately, to get all this in 39 bytes requires proper packing.
 * So, the version number is at an offset instead of first.
 */
typedef struct {
#ifdef KERNEL
    ino_t at_pino;		/* inode# of parent directory. */
#else
    uint64_t at_pino;		/* inode# of parent directory. */
#endif
    u_char at_attr_version;	/* version number of struct, currently 0. */
    u_char at_name_version;	/* version number of file names. */
    u_short at_tag;		/* guarantees unique file names. */
    afs_inode_params_t at_param;	/* vice parameters. */
} afs_xfs_attr_t;

/* Use this for getting and setting attr. sizeof struct is too large. */
#define SIZEOF_XFS_ATTR_T (7*sizeof(int))

/* Additional info is stored in inode standard attributes:
 * mode - low 7 bits are link count.
 * uid - clipped volume id (inode_p1)
 * gid - XFS_VICEMAGIC
 * Only set low 31 bits of uid and gid so chown can fix in xfs_ListViceInodes.
 */
#define AFS_XFS_MODE_LINK_MASK 0x7
#define AFS_XFS_VNO_CLIP(V) (V & 0x7fffffff)

/* AFS_XFS_DATTR is used by ListViceInodes to restrict it's search to
 * AFS directories. Using an attribute, since it can't inadvertantly be
 * changed by Unix syscalls.
 */
#define AFS_XFS_DATTR "AFSDIR"
typedef struct {
#define AFS_XFS_ATD_VERS 1
    char atd_version;
    char atd_spare[3];
    int atd_volume;		/* RW volume id */
} afs_xfs_dattr_t;
#define SIZEOF_XFS_DATTR_T sizeof(afs_xfs_dattr_t)

/* vice_inode_info_t must match ViceInodeInfo for the salvager.
 * vice_inode_info_t and i_list_inode_t are used to get the inode
 * information from XFS partitions. The ili_name_version is
 * used to ensure we're using the correct naming algorithm
 * to fix file names in ListViceInodes.
 */
typedef struct {
    uint64_t inodeNumber;
    int byteCount;
    int linkCount;
    int param[4];
} vice_inode_info_t;

typedef struct {
#define AFS_XFS_ILI_VERSION 1
    u_int ili_version;		/* input is requested struct version. */
    u_char ili_attr_version;	/* attr's version */
    u_char ili_name_version;	/* attr's name version */
    u_short ili_tag;		/* at_tag number. */
    uint64_t ili_pino;		/* parent inode number. */
    vice_inode_info_t ili_info;	/* Must be at a 64 bit offset. */
    u_int ili_vno, ili_magic;
} i_list_inode_t;


#endif /* AFS_SGI_XFS_IOPS_ENV */
#endif /* AFS_XFSATTRS_H */
