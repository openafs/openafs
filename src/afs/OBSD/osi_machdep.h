/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *
 * OpenBSD OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

/* Id: $ */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#include <sys/lock.h>

#define getpid()		curproc
extern struct simplelock afs_rxglobal_lock;

/* 
 * Time related macros
 */
extern struct timeval time;
#define osi_Time() (time.tv_sec)
#define	afs_hz	    hz

#define M_AFSFID	(M_TEMP-1)
#define M_AFSBUFHDR	(M_TEMP-2)
#define M_AFSBUFFER	(M_TEMP-3)
#define M_AFSGENERIC	(M_TEMP-4)

#define PAGESIZE 8192

#define	AFS_UCRED	ucred
#define	AFS_PROC	struct proc

#define SetAfsVnode(vn) /* nothing; done in getnewvnode() */
#define	IsAfsVnode(vn)	    ((vn)->v_op == afs_vnodeop_p)

#define p_rcred         p_ucred

#define AFS_HOLD(vp)	afs_nbsd_ref(vp)
#define AFS_RELE(vp)	afs_nbsd_rele(vp)
#define osi_vnhold(avc, r) afs_vget(AFSTOV(avc), 0)

#define afsio_iov	uio_iov
#define afsio_iovcnt	uio_iovcnt
#define afsio_offset	uio_offset
#define afsio_resid	uio_resid
#define afsio_seg	uio_segflg
#define AFS_KALLOC(s)	afs_nbsd_Alloc(s)
#define AFS_KFREE(p, s)	afs_nbsd_Free((p), (s))
#define AFS_UIOSYS	UIO_SYSSPACE
#define AFS_UIOUSER	UIO_USERSPACE
#define afs_bufferpages bufpages
#define afs_suser()	afs_osi_suser(osi_curcred()) 
#define osi_curcred()	(curproc->p_cred->pc_ucred)
#define osi_curproc()	(curproc)
#define osi_GetTime(x)	microtime(x)
#define osi_vfs	mount
#define osi_vfs_bsize	mnt_stat.f_bsize
#define osi_vfs_fsid	mnt_stat.f_fsid
#define printk printf			/* for RX version of xdr_* */
#define setgroups	sys_setgroups
#define UVM
#define va_nodeid	va_fileid
#define vfs_bsize	mnt_stat.f_bsize
#define vfs_fsid	mnt_stat.f_fsid
#define vfs_vnodecovered mnt_vnodecovered
#define vnode_t		struct vnode
#define vSetType(vc, type)	AFSTOV(vc)->v_type = (type)
#define vSetVfsp(vc, vfsp)	AFSTOV(vc)->v_mount = (vfsp)
#define vType(vc)		(vc)->v->v_type
#define v_vfsp		v_mount

/* This is not always in scope yet */
struct vcache;

extern int afs_nbsd_lookupname(char *fnamep, enum uio_seg segflg, int followlink,
			       struct vnode **dirvpp, struct vnode **compvpp);
extern void afs_nbsd_getnewvnode(struct vcache *tvc);
extern void afs_nbsd_ref(struct vnode *);
extern void afs_nbsd_rele(struct vnode *);
extern void *afs_nbsd_Alloc(size_t asize);
extern void afs_nbsd_Free(void *p, size_t asize);
extern int afs_vget();

#define	gop_lookupname(fnamep, segflg, followlink, dirvpp, compvpp) \
	afs_nbsd_lookupname((fnamep), (segflg), (followlink), (dirvpp), (compvpp))

#ifdef KERNEL
extern int (**afs_vnodeop_p)();
extern struct simplelock afs_global_lock;

#ifndef AFS_GLOBAL_SUNLOCK
#define AFS_ASSERT_GLOCK()
#endif
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define AFS_RXGLOCK()
#define AFS_RXGUNLOCK()
#define ISAFS_RXGLOCK() 1
#define ISAFS_GLOCK() 1

#undef SPLVAR
#define SPLVAR
#undef NETPRI
#define NETPRI
#undef USERPRI
#define USERPRI
#endif /* KERNEL */

#endif /* _OSI_MACHDEP_H_ */
