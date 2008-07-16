/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Vnode numbers in dynroot are composed of a type field (upper 8 bits)
 * and a type-specific identifier in the lower 24 bits.
 */
#define VN_TYPE_CELL		0x01	/* Corresponds to a struct cell */
#define VN_TYPE_ALIAS		0x02	/* Corresponds to a struct cell_alias */
#define VN_TYPE_SYMLINK		0x03	/* User-created symlink in /afs */
#define VN_TYPE_MOUNT  		0x04	/* Mount point by volume ID */

#define VNUM_TO_VNTYPE(vnum)	((vnum) >> 24)
#define VNUM_TO_VNID(vnum)	((vnum) & 0x00ffffff)
#define VNUM_FROM_TYPEID(type, id) \
				((type) << 24 | (id))
#define VNUM_TO_CIDX(vnum)	(VNUM_TO_VNID(vnum) >> 2)
#define VNUM_TO_RW(vnum)	(VNUM_TO_VNID(vnum) >> 1 & 1)
#define VNUM_FROM_CIDX_RW(cidx, rw) \
				VNUM_FROM_TYPEID(VN_TYPE_CELL, \
						 ((cidx) << 2 | (rw) << 1))
#define VNUM_FROM_CAIDX_RW(caidx, rw) \
				VNUM_FROM_TYPEID(VN_TYPE_ALIAS, \
						 ((caidx) << 2 | (rw) << 1))

#define AFS_DYNROOT_MOUNTNAME	".:mount"
