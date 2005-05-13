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
#else
#include <sys/lock.h>
#include <sys/user.h>
#endif
#include <kern/thread.h>

#ifdef AFS_DARWIN80_ENV
#define getpid()                proc_selfpid()
#define getppid()               proc_selfppid()
#else
#define getpid()                current_proc()->p_pid
#define getppid()               current_proc()->p_pptr->p_pid
#endif
#undef gop_lookupname
#define gop_lookupname osi_lookupname

#define FTRUNC 0

/* vcexcl - used only by afs_create */
enum vcexcl { EXCL, NONEXCL };

#ifndef AFS_DARWIN80_ENV
#define ifaddr_address_family(x) (x)->ifa_addr->sa_family
#define ifaddr_address(x, y, z) memcpy(y, (x)->ifa_addr, z)
#define ifaddr_netmask(x, y, z) memcpy(y, (x)->ifa_netmask, z)
#define ifaddr_dstaddress(x, y, z) memcpy(y, (x)->ifa_dstaddr, z)
#define ifaddr_ifnet(x) (x?(x)->ifa_ifp:0)
#define ifnet_flags(x) (x?(x)->if_flags:0)
#define ifnet_metric(x) (x?(x)->if_data.ifi_metric:0)
#endif

#ifndef AFS_DARWIN80_ENV
#define ifaddr_address_family(x) (x)->ifa_addr->sa_family
#define ifaddr_address(x, y, z) memcpy(y, (x)->ifa_addr, z)
#define ifaddr_netmask(x, y, z) memcpy(y, (x)->ifa_netmask, z)
#define ifaddr_dstaddress(x, y, z) memcpy(y, (x)->ifa_dstaddr, z)
#define ifaddr_ifnet(x) (x?(x)->ifa_ifp:0)
#define ifnet_flags(x) (x?(x)->if_flags:0)
#define ifnet_metric(x) (x?(x)->if_data.ifi_metric:0)

#define vnode_clearfsnode(x) ((x)->v_data = 0)
#define vnode_fsnode(x) (x)->v_data
#define vnode_lock(x) vn_lock(x, LK_EXCLUSIVE | LK_RETRY, current_proc());
#endif

#ifdef AFS_DARWIN80_ENV
#define vrele vnode_rele
#define vput vnode_put
#define vref vnode_ref
#define vattr vnode_attr

#define SetAfsVnode(vn)         /* nothing; done in getnewvnode() */
/* vnode_vfsfsprivate is not declared, so no macro for us */
extern void * afs_fsprivate_data;
static inline int IsAfsVnode(vnode_t vn) {
   mount_t mp;
   int res = 0;
   mp = vnode_mount(vn);
   if (mp) {
      res = (vfs_fsprivate(mp) == &afs_fsprivate_data);
      vfs_mountrelease(mp);
   }
   return res;
}
#endif

/* 
 * Time related macros
 */
#ifndef AFS_DARWIN60_ENV
extern struct timeval time;
#endif
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

#define PAGESIZE 8192

#define AFS_UCRED       ucred

#define AFS_PROC        struct proc

#define osi_vnhold(avc,r)       VN_HOLD(AFSTOV(avc))
#define VN_HOLD(vp) darwin_vn_hold(vp)
#define VN_RELE(vp) vrele(vp);


#define gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid),current_proc())

#undef afs_suser

#ifdef KERNEL
extern thread_t afs_global_owner;
/* simple locks cannot be used since sleep can happen at any time */
#ifdef AFS_DARWIN80_ENV
/* mach locks still don't have an exported try, but we are forced to use them */
extern lck_mtx_t  *afs_global_lock;
#define AFS_GLOCK() \
    do { \
        lk_mtx_lock(afs_global_lock); \
	osi_Assert(afs_global_owner == 0); \
	afs_global_owner = current_thread(); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
	osi_Assert(afs_global_owner == current_thread()); \
	afs_global_owner = 0; \
        lk_mtx_unlock(afs_global_lock); \
    } while(0)
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

extern ino_t VnodeToIno(struct vnode * vp);
extern dev_t VnodeToDev(struct vnode * vp);

#define osi_curproc() current_proc()

/* FIXME */
#define osi_curcred() &afs_osi_cred 

#ifdef AFS_DARWIN80_ENV
uio_t afsio_darwin_partialcopy(uio_t auio, int size);
#endif

#endif /* KERNEL */

#endif /* _OSI_MACHDEP_H_ */
