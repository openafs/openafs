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
 * Inode information required for FreeBSD servers and salvager.
 */
#ifndef _OSI_INODE_H_
#define _OSI_INODE_H_

#define BAD_IGET	-1000

#define VICEMAGIC       0xb61cfa84

#define DI_VICEP3(p) \
	( ((u_int)((p)->di_vicep3a)) << 16 | ((u_int)((p)->di_vicep3b)) )
#define I_VICE3(p) \
	( ((u_int)((p)->i_vicep3a)) << 16 | ((u_int)((p)->i_vicep3b)) )

#define FAKE_INODE_SIZE    (sizeof(struct vnode)+sizeof(struct inode))
#define MOUNTLIST_UNLOCK() simple_lock_unlock(&mountlist_slock)
#define MOUNTLIST_LOCK()   simple_lock(&mountlist_slock)

/* FreeBSD doesn't actually have a di_proplb, so we use di_spare[0] */
#define di_proplb       di_spare[0]
/* For some reason, they're called "oldids" instead of "bc_{u,g}id" */
#define di_bcuid        di_u.oldids[0]
#define di_bcgid        di_u.oldids[1]

#define i_vicemagic	i_din.di_spare[0]
#define i_vicep1	i_din.di_uid
#define i_vicep2	i_din.di_gid
#define i_vicep3a	i_din.di_u.oldids[0]
#define i_vicep3b	i_din.di_u.oldids[1]
#define i_vicep4	i_din.di_spare[1]	/* not used */

#define di_vicemagic	di_spare[0]
#define di_vicep1	di_uid
#define di_vicep2	di_gid
#define di_vicep3a	di_u.oldids[0]
#define di_vicep3b	di_u.oldids[1]
#define di_vicep4	di_spare[1]	/* not used */

/*
 * Macros for handling inode numbers:
 *     inode number to file system block offset.
 *     inode number to cylinder group number.
 *     inode number to file system block address.
 */
#define itoo(fs, x)     ((x) % INOPB(fs))
#define itog(fs, x)     ((x) / (fs)->fs_ipg)
#define itod(fs, x) \
        ((daddr_t)(cgimin(fs, itog(fs, x)) + \
        (blkstofrags((fs), (((x) % (fs)->fs_ipg) / INOPB(fs))))))


#define  IS_VICEMAGIC(ip)        ((ip)->i_vicemagic == VICEMAGIC)
#define  IS_DVICEMAGIC(dp)       ((dp)->di_vicemagic == VICEMAGIC)

#define  CLEAR_VICEMAGIC(ip)     (ip)->i_vicemagic = 0
#define  CLEAR_DVICEMAGIC(dp)    (dp)->di_vicemagic = 0

#endif /* _OSI_INODE_H_ */
