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

#define afs_bufferpages bufpages

#define osi_vnhold(avc,r)  do { \
       if ((avc)->vrefCount) { VN_HOLD((struct vnode *)(avc)); } \
       else osi_Panic("refcnt==0");  } while(0)

#define	gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid), curproc)

#define afs_suser() afs_osi_suser(osi_curcred()) 

extern int (**afs_vnodeop_p)();
#define SetAfsVnode(vn) /* nothing; done in getnewvnode() */
#define	IsAfsVnode(vn)	    ((vn)->v_op == afs_vnodeop_p)

#define AFS_HOLD(vp) afs_nbsd_ref(vp)
#define AFS_RELE(vp) afs_nbsd_rele(vp)
extern void afs_nbsd_ref(struct vnode *);
extern void afs_nbsd_rele(struct vnode *);

#define va_nodeid va_fileid
#define v_vfsp v_mount
#define vfs_vnodecovered mnt_vnodecovered
#define vfs_bsize mnt_stat.f_bsize
typedef void (*osi_timeout_t)(void *);
#define osi_timeout_t_done
#define osi_curproc()		(curproc)
#define osi_curcred()		(curproc->p_cred->pc_ucred)
#define osi_vfs	mount
#define osi_vfs_bsize mnt_stat.f_bsize
#define osi_vfs_fsid mnt_stat.f_fsid
#define afs_osi_alloc osi_Alloc
#define afs_osi_free osi_Free
#define printk printf			/* for RX version of xdr_* */
#define	vType(vc)		(vc)->v->v_type
#define vSetType(vc, type)	AFSTOV(vc)->v_type = (type)
#define	vSetVfsp(vc, vfsp)	AFSTOV(vc)->v_mount = (vfsp)
#define osi_sleep(chan, pri)	tsleep((chan), pri, "afs", 0)
#define afs_osi_Sleep(chan)	tsleep((chan), PVFS, "afs", 0)
#define afs_osi_Wakeup(chan)	(wakeup(chan), 1)
#define FTRUNC O_TRUNC
#define FEXLOCK O_EXLOCK
#define FSHLOCK O_SHLOCK
#define UVM

/* no protoytpe 'cuz our includers don't always have uio_rw in scope first */
extern int afs_nbsd_rdwr();
extern int afs_nbsd_lookupname();
extern void afs_nbsd_getnewvnode();

#define VOP_RDWR afs_nbsd_rdwr
#define	gop_lookupname(fnamep, segflg, followlink, dirvpp, compvpp) \
	afs_nbsd_lookupname((fnamep), (segflg), (followlink), (dirvpp), (compvpp))

#ifdef KERNEL
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
