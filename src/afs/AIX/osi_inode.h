/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Inode information required for AIX servers and salvager.
 */
#ifndef _OSI_INODE_H_
#define _OSI_INODE_H_

#define BAD_IGET	-1000

#define VICEMAGIC       0x84fa1cb6

/* These exist because HP requires more work to extract uid. */
#define DI_VICEP3(p)    ( (p)->di_vicep3 )
#define I_VICE3(p)      ( (p)->i_vicep3 )

/* rsvrd[4] is in use in large files filesystems for file size. */
#define  di_vicemagic    di_rsrvd[0]
#define  di_vicep1       di_rsrvd[1]
#define  di_vicep2       di_rsrvd[2]
#define  di_vicep3       di_rsrvd[3]
#define  di_vicep4       di_rsrvd[4]

#define  i_vicemagic     i_dinode.di_vicemagic
#define  i_vicep1        i_dinode.di_vicep1
#define  i_vicep2        i_dinode.di_vicep2
#define  i_vicep3        i_dinode.di_vicep3
#define  i_vicep4        i_dinode.di_vicep4

#define  IS_VICEMAGIC(ip)        ((ip)->i_vicemagic == VICEMAGIC ?  1 : 0)
#define  IS_DVICEMAGIC(dp)       ((dp)->di_vicemagic == VICEMAGIC ?  1 : 0)

#define  CLEAR_VICEMAGIC(ip)     (ip)->i_vicemagic = 0
#define  CLEAR_DVICEMAGIC(dp)    (dp)->di_vicemagic = 0

#endif /* _OSI_INODE_H_ */
