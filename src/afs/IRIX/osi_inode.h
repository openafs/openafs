/* Copyright (C) 1995 Transarc Corporation - All rights reserved. */
/*
 * osi_inode.h
 *
 * Inode information required for IRIX servers and salvager.
 */
#ifndef _OSI_INODE_H_
#define _OSI_INODE_H_

#define BAD_IGET	-1000

#define VICEMAGIC       0x84fa1cb6
/* chown can't set the high bit - used for XFS based filesystem */
#define XFS_VICEMAGIC   0x74fa1cb6

/* These exist because HP requires more work to extract uid. */
#define DI_VICEP3(p)    ( (p)->di_vicep3 )
#define I_VICE3(p)      ( (p)->i_vicep3 )


/*
 * We use the 12 8-bit unused ex_magic fields!
 * Plus 2 values of di_version
 * di_version = 0 - current EFS
 *	    1 - AFS INODESPECIAL
 *	    2 - AFS inode
 * AFS inode:
 * magic[0-3] - VOLid
 * magic[4-6] - vnode number (24 bits)
 * magic[7-9] - disk uniqifier
 * magic[10-11]+di_spare - data version
 *
 * INODESPECIAL:
 * magic[0-3] - VOLid
 * magic[4-7] - parent
 * magic[8]   - type
 */
#define SGI_UNIQMASK	0xffffff
#define SGI_DATAMASK     0xffffff
#define SGI_DISKMASK     0xffffff

/* we hang this struct off of the incore inode */
struct afsparms {
        afs_int32 vicep1;
        afs_int32 vicep2;
        afs_int32 vicep3;
        afs_int32 vicep4;
    };

#define dmag(p,n)        ((p)->di_u.di_extents[n].ex_magic)

#define	IS_VICEMAGIC(ip)	(((ip)->i_version == EFS_IVER_AFSSPEC || \
				  (ip)->i_version == EFS_IVER_AFSINO) \
				 ?  1 : 0)
#define	IS_DVICEMAGIC(dp)	(((dp)->di_version == EFS_IVER_AFSSPEC || \
				  (dp)->di_version == EFS_IVER_AFSINO) \
				 ?  1 : 0)

#define  CLEAR_VICEMAGIC(ip)     (ip)->i_version = EFS_IVER_EFS
#define  CLEAR_DVICEMAGIC(dp)    dp->di_version = EFS_IVER_EFS

#endif /* _OSI_INODE_H_ */
