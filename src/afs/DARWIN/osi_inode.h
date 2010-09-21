/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
/*
 * osi_inode.h
 *
 * Inode information required for MACOS servers and salvager.
 */
#ifndef _OSI_INODE_H_
#define _OSI_INODE_H_

#define BAD_IGET        -1000

#define VICEMAGIC       0x84fa1cb6

#define DI_VICEP3(p) ((p)->di_vicep3)
#define I_VICEP3(p) ((p)->i_vicep3)

#define i_vicemagic     i_din.di_flags
#define i_vicep1        i_din.di_gen
#define i_vicep2        i_din.di_uid
#define i_vicep3        i_din.di_gid
#define i_vicep4        i_din.di_spare[0]	/* not used */

#define di_vicemagic    di_flags
#define di_vicep1       di_gen
#define di_vicep2       di_uid
#define di_vicep3       di_gid
#define di_vicep4       di_spare[0]	/* not used */

#define  IS_VICEMAGIC(ip)        ((ip)->i_vicemagic == VICEMAGIC)
#define  IS_DVICEMAGIC(dp)       ((dp)->di_vicemagic == VICEMAGIC)

#define  CLEAR_VICEMAGIC(ip)     (ip)->i_vicemagic = 0
#define  CLEAR_DVICEMAGIC(dp)    (dp)->di_vicemagic = 0

#endif /* _OSI_INODE_H_ */
