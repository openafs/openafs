/* Copyright (C) 1995 Transarc Corporation - All rights reserved. */
/*
 * osi_inode.h
 *
 * Inode information required for DUX servers and salvager.
 */
#ifndef _OSI_INODE_H_
#define _OSI_INODE_H_

#define BAD_IGET	-1000

#define VICEMAGIC       0xb61cfa84

#define DI_VICEP3(p) \
	( ((u_int)((p)->di_vicep3a)) << 16 | ((u_int)((p)->di_vicep3b)) )
#define I_VICE3(p) \
	( ((u_int)((p)->i_vicep3a)) << 16 | ((u_int)((p)->i_vicep3b)) )

#define i_vicemagic	i_din.di_proplb
#define i_vicep1	i_din.di_uid
#define i_vicep2	i_din.di_gid
#define i_vicep3a	i_din.di_bcuid
#define i_vicep3b	i_din.di_bcgid
#define i_vicep4	i_din.di_spare[0]	/* not used */

#define di_vicemagic	di_proplb
#define di_vicep1	di_uid
#define di_vicep2	di_gid
#define di_vicep3a	di_bcuid
#define di_vicep3b	di_bcgid
#define di_vicep4	di_spare[0]		/* not used */

#define  IS_VICEMAGIC(ip)        ((ip)->i_vicemagic == VICEMAGIC)
#define  IS_DVICEMAGIC(dp)       ((dp)->di_vicemagic == VICEMAGIC)

#define  CLEAR_VICEMAGIC(ip)     (ip)->i_vicemagic = 0
#define  CLEAR_DVICEMAGIC(dp)    (dp)->di_vicemagic = 0

#endif /* _OSI_INODE_H_ */
