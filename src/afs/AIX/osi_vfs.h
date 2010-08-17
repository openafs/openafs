/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define VREAD	R_ACC
#define VEXEC	X_ACC
#define VWRITE	W_ACC
#define	VSUID	S_ISUID
#define	VSGID	S_ISGID
#define	VSVTX	S_ISVTX

#define min(a, b)	(((a) < (b)) ? (a) : (b))
#define	bcopyin	copyin		/* Aux has identical bcopyin */

#define BLKDEV_IOSIZE	BSIZE
/*#define MAXBSIZE	NFS_MAXDATA*/
#define VROOT		V_ROOT
#define VTEXT		V_TEXT
/*#define IO_SYNC		FSYNC*/
#define IO_APPEND	FAPPEND
/*#define IO_NDELAY	FNDELAY*/
#define	VTOI		VTOIP
/*#define v_pdata		v_data*/
#define v_op		v_gnode->gn_ops
#define iunlock(x)      irele(x)

#if !defined(_SUN)
/*
 * if _SUN is defined, this is in vnode.h (at least today... who knows where
 * they will hide it tomorrow)
 */
enum vcexcl { NONEXCL, EXCL };	/* (non)exclusive create */
#endif

struct buf *getblk(), *geteblk(), *breada(), *bread();
#define	b_actf		av_forw
#define	dbtob(db)	((unsigned)(db) << 9)	/* (db * 512) */
