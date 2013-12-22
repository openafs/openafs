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
 * MACOS OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#ifdef XAFS_DARWIN_ENV
#ifndef _MACH_ETAP_H_
#define _MACH_ETAP_H_
typedef unsigned short etap_event_t;
#endif
#endif

#ifdef AFS_DARWIN80_ENV
#include <kern/locks.h>
#include <sys/mount.h>
#include <h/vnode.h>
#else
#include <sys/lock.h>
#include <sys/user.h>
#include <sys/vnode.h>
#endif
#include <sys/kauth.h>
#include <kern/thread.h>

#ifdef AFS_DARWIN80_ENV
#define vop_proc vfs_context_proc(ap->a_context)
#define vop_cred vfs_context_ucred(ap->a_context)
#define cn_proc(cnp) vfs_context_proc(ap->a_context)
#define cn_cred(cnp) vfs_context_ucred(ap->a_context)
#define vop_cn_proc vfs_context_proc(ap->a_context)
#define vop_cn_cred vfs_context_ucred(ap->a_context)
#define getpid()                proc_selfpid()
#define getppid()               proc_selfppid()
#else
#define vop_proc ap->a_p
#define vop_cred ap->a_cred
#define cn_proc(cnp) (cnp)->cn_proc
#define cn_cred(cnp) (cnp)->cn_cred
#define vop_cn_proc cn_proc(ap->a_cnp)
#define vop_cn_cred cn_cred(ap->a_cnp)
#define getpid()                current_proc()->p_pid
#define getppid()               current_proc()->p_pptr->p_pid
#endif
#undef gop_lookupname
#define gop_lookupname osi_lookupname
#undef gop_lookupname_user
#define gop_lookupname_user osi_lookupname_user

#define FTRUNC 0

/* vcexcl - used only by afs_create */
enum vcexcl { EXCL, NONEXCL };

#ifndef AFS_DARWIN80_ENV
#define vnode_clearfsnode(x) ((x)->v_data = 0)
#define vnode_fsnode(x) (x)->v_data
#define vnode_lock(x) vn_lock(x, LK_EXCLUSIVE | LK_RETRY, current_proc());
#define vnode_isvroot(x) (((x)->v_flag & VROOT)?1:0)
#define vnode_vtype(x) (x)->v_type
#define vnode_isdir(x) ((x)->v_type == VDIR)

#define vfs_flags(x) (x)->v_flags
#define vfs_setflags(x, y) (x)->mnt_flag |= (y)
#define vfs_clearflags(x, y) (x)->mnt_flag &= (~(y))
#define vfs_isupdate(x) ((x)->mnt_flag & MNT_UPDATE)
#define vfs_fsprivate(x) (x)->mnt_data
#define vfs_setfsprivate(x,y) (x)->mnt_data = (y)
#define vfs_typenum(x) (x)->mnt_vfc->vfc_typenum
#endif

#ifdef AFS_DARWIN80_ENV
#define vrele vnode_rele
#define vput vnode_rele
#define vref vnode_ref
#define vattr vnode_attr
#if 0
#define vn_lock(v, unused1, unused2) vnode_get((v))
#define VOP_LOCK(v, unused1, unused2) vnode_get((v))
#define VOP_UNLOCK(v, unused1, unused2) vnode_put((v))
#endif

#define va_size va_data_size
#define va_atime va_access_time
#define va_mtime va_modify_time
#define va_ctime va_change_time
#define va_bytes va_total_alloc
#define va_blocksize va_iosize
#define va_nodeid va_fileid

#define crref kauth_cred_get_with_ref
#define crhold kauth_cred_ref
#ifdef AFS_DARWIN100_ENV
static inline void crfree(kauth_cred_t X) { kauth_cred_unref(&X); }
#else
#define crfree kauth_cred_rele
#endif
#define crdup kauth_cred_dup
#ifdef AFS_DARWIN100_ENV
#define ubc_msync_range(X,Y,Z,A) ubc_msync(X,Y,Z,NULL,A)
#else
#define ubc_msync_range(X,Y,Z,A) ubc_sync_range(X,Y,Z,A)
#endif
extern vfs_context_t afs_osi_ctxtp;
extern int afs_osi_ctxtp_initialized;
#endif
extern u_int32_t afs_darwin_realmodes;
extern u_int32_t afs_darwin_fsevents;

/*
 * Time related macros
 */
#ifdef AFS_DARWIN80_ENV
static inline time_t osi_Time(void) {
    struct timeval _now;
    microtime(&_now);
    return _now.tv_sec;
}
#else
#define osi_Time() (time.tv_sec)
#endif
#define afs_hz      hz
#ifdef AFS_DARWIN80_ENV
extern int hz;
#endif

typedef struct ucred afs_ucred_t;
typedef struct proc afs_proc_t;

#define osi_vnhold(avc,r)       VN_HOLD(AFSTOV(avc))
#define VN_HOLD(vp) darwin_vn_hold(vp)
#define VN_RELE(vp) vrele(vp);

void darwin_vn_hold(struct vnode *vp);

#define gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid),current_proc())

#undef afs_suser

extern thread_t afs_global_owner;
/* simple locks cannot be used since sleep can happen at any time */
#ifdef AFS_DARWIN80_ENV
/* mach locks still don't have an exported try, but we are forced to use them */
extern lck_mtx_t  *afs_global_lock;
#define AFS_GLOCK() \
    do { \
	osi_Assert(afs_global_owner != current_thread()); \
	lck_mtx_lock(afs_global_lock); \
	osi_Assert(afs_global_owner == 0); \
	afs_global_owner = current_thread(); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
	osi_Assert(afs_global_owner == current_thread()); \
	afs_global_owner = 0; \
        lck_mtx_unlock(afs_global_lock); \
    } while(0)
#define osi_InitGlock() \
    do { \
	afs_global_owner = 0; \
    } while (0)
#else
/* Should probably use mach locks rather than bsd locks, since we use the
   mach thread control api's elsewhere (mach locks not used for consistency
   with rx, since rx needs lock_write_try() in order to use mach locks
 */
extern struct lock__bsd__ afs_global_lock;
#define AFS_GLOCK() \
    do { \
        lockmgr(&afs_global_lock, LK_EXCLUSIVE, 0, current_proc()); \
	osi_Assert(afs_global_owner == 0); \
	afs_global_owner = current_thread(); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
	osi_Assert(afs_global_owner == current_thread()); \
	afs_global_owner = 0; \
        lockmgr(&afs_global_lock, LK_RELEASE, 0, current_proc()); \
    } while(0)
#define osi_InitGlock() \
    do { \
	lockinit(&afs_global_lock, PLOCK, "afs global lock", 0, 0); \
	afs_global_owner = 0; \
    } while (0)
#endif
#define ISAFS_GLOCK() (afs_global_owner == current_thread())

#define SPLVAR
#define NETPRI
#define USERPRI
#if 0
#undef SPLVAR
#define SPLVAR int x;
#undef NETPRI
#define NETPRI x=splnet();
#undef USERPRI
#define USERPRI splx(x);
#endif

#define AFS_APPL_UFS_CACHE 1
#define AFS_APPL_HFS_CACHE 2

extern ino_t VnodeToIno(vnode_t avp);
extern dev_t VnodeToDev(vnode_t vp);
extern int igetinode(mount_t vfsp, dev_t dev , ino_t inode, vnode_t *vpp,
              struct vattr *va, int *perror);

#define osi_curproc() current_proc()

/* FIXME */
#define osi_curcred() &afs_osi_cred

#ifdef AFS_DARWIN80_ENV
uio_t afsio_darwin_partialcopy(uio_t auio, int size);

#define uprintf printf
#endif

/* Vnode related macros */

#if defined(AFS_DARWIN80_ENV)
extern int afs_vfs_typenum;
# define vType(vc)               vnode_vtype(AFSTOV(vc))
# define vSetVfsp(vc, vfsp)
# define vSetType(vc, type)      (vc)->f.m.Type = (type)
# define SetAfsVnode(vn)         /* nothing; done in getnewvnode() */
# define IsAfsVnode(v) (vfs_typenum(vnode_mount((v))) == afs_vfs_typenum)
#else
extern int (**afs_vnodeop_p) ();
# define vType(vc)               AFSTOV(vc)->v_type
# define vSetVfsp(vc, vfsp)      AFSTOV(vc)->v_mount = (vfsp)
# define vSetType(vc, type)      AFSTOV(vc)->v_type = (type)
# define IsAfsVnode(v)      ((v)->v_op == afs_vnodeop_p)
# define SetAfsVnode(v)     /* nothing; done in getnewvnode() */
#endif

#ifdef AFS_DARWIN80_ENV
#define osi_procname(procname, size) proc_selfname(procname, size)
#else
#define osi_procname(procname, size) strncpy(procname, curproc->p_comm, size)
#endif

#endif /* _OSI_MACHDEP_H_ */
