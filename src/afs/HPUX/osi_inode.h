/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Inode information required for HPUX servers and salvager.
 */
#ifndef _OSI_INODE_H_
#define _OSI_INODE_H_

#define BAD_IGET	-1000

#define VICEMAGIC       0x84fa1cb6

/* These exist because HP requires more work to extract uid. */
#define DI_VICEP3(p)    ( *(int32_t *) &((p)->di_ic.ic_uid_msb) )
#define I_VICE3(p)      ( *(int32_t *)( &((p)->i_icun.i_ic.ic_uid_msb) ) )

#define _GET_DC_UID(DC) ((uid_t)((DC).ic_uid_msb << 16 | (DC).ic_uid_lsb))
#define _GET_D_UID(DP)  _GET_DC_UID((DP)->di_ic)

/* the VICEMAGIC field is moved to ic_flags. This is because VICEMAGIC
** happens to set the LARGEUID bit in the ic_flags field.  UFS interprets
** this as an inode which supports large uid's, hence does not do any UID
** length conversion on the fly. We use the uid fields to store i_vicep3.
*/
#define i_vicemagic     i_icun.i_ic.ic_flags
#define i_vicep1        i_icun.i_ic.ic_spare[0]
#define i_vicep2        i_icun.i_ic.ic_spare[1]
#define i_vicep4        i_icun.i_ic.ic_gen
#define i_vicep3        ( *(int32_t *)( &(i_icun.i_ic.ic_uid_msb) ) )

#define di_vicemagic     di_ic.ic_flags
#define di_vicep1       di_ic.ic_spare[0]
#define di_vicep2       di_ic.ic_spare[1]
#define di_vicep4       di_ic.ic_gen
#define di_vicep3       ( *(int32_t *)( &(di_ic.ic_uid_msb) ) )

#define  IS_VICEMAGIC(ip)        ((ip)->i_vicemagic == VICEMAGIC ?  1 : 0)
#define  IS_DVICEMAGIC(dp)       ((dp)->di_vicemagic == VICEMAGIC ?  1 : 0)

#define  CLEAR_VICEMAGIC(ip)     (ip)->i_vicemagic = 0
#define  CLEAR_DVICEMAGIC(dp)    (dp)->di_vicemagic = 0

struct inode *igetinode(struct vfs *vfsp, dev_t dev, ino_t inode,
			int *perror);

#endif /* _OSI_INODE_H_ */
