/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Inode information required for SOLARIS servers and salvager.
 */
#ifndef _OSI_INODE_H_
#define _OSI_INODE_H_

#define BAD_IGET	-1000

#define VICEMAGIC       0x84fa1cb6

/* These exist because HP requires more work to extract uid. */
#define DI_VICEP3(p)    ( (p)->di_vicep3 )
#define I_VICE3(p)      ( (p)->i_vicep3 )

#define di_vicep1         di_un . di_icom .ic_gen
#define di_vicep2         di_un . di_icom .ic_flags
#if	defined(AFS_SUN56_ENV)
#define di_vicep3       di_ic.ic_uid
#define di_vicemagic    di_ic.ic_gid
#else
#define di_vicep3         di_un . di_icom .ic_size.val[0]
#endif

#define i_vicep1 i_ic.ic_gen
#define i_vicep2 i_ic.ic_flags
#if	defined(AFS_SUN56_ENV)
#define i_vicep3 	i_ic.ic_uid
#define i_vicemagic	i_ic.ic_gid
#else
#define i_vicep3 i_ic.ic_size.val[0]
#endif

#if	defined(AFS_SUN56_ENV)
#define	IS_VICEMAGIC(ip)	((ip)->i_vicemagic == VICEMAGIC)
#define	IS_DVICEMAGIC(dp)	((dp)->di_vicemagic == VICEMAGIC)

#define	CLEAR_VICEMAGIC(ip)	(ip)->i_vicemagic = (ip)->i_vicep3 = 0
#define	CLEAR_DVICEMAGIC(dp)	(dp)->di_vicemagic = (dp)->di_vicep3 = 0
#else
#define  IS_VICEMAGIC(ip)        (((ip)->i_vicep2 || (ip)->i_vicep3) ? 1 : 0)
#define  IS_DVICEMAGIC(dp)       (((dp)->di_vicep2 || (dp)->di_vicep3) ? 1 : 0)

#define  CLEAR_VICEMAGIC(ip)     (ip)->i_vicep2 = (ip)->i_vicep3 = 0
#define  CLEAR_DVICEMAGIC(dp)    (dp)->di_vicep2 = (dp)->di_vicep3 = 0
#endif

#define AFS_SUN_UFS_CACHE 0
#ifdef AFS_HAVE_VXFS
#define AFS_SUN_VXFS_CACHE 1
#endif /* AFS_HAVE_VXFS */

#endif /* _OSI_INODE_H_ */
