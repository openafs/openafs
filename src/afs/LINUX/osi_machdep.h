/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux implementation.
 *
 */

#ifndef OSI_MACHDEP_H_
#define OSI_MACHDEP_H_

/* Only needed for xdr.h in glibc 2.1.x */
#ifndef quad_t
#define quad_t __quad_t
#define u_quad_t __u_quad_t
#endif

#undef getuerror

#define getpid() current->pid
#define getppid() current->p_opptr->pid


#define afs_hz HZ
#include "h/sched.h"
#define osi_Time() (xtime.tv_sec)
#if  (CPU == sparc64)
#define osi_GetTime(V) do { (*(V)).tv_sec = xtime.tv_sec; (*(V)).tv_usec = xtime.tv_usec; } while (0)
#else
#define osi_GetTime(V) (*(V)=xtime)
#endif

#undef gop_lookupname
#define gop_lookupname osi_lookupname

#define osi_vnhold(v, n)  VN_HOLD(v)

#define osi_AllocSmall afs_osi_Alloc
#define osi_FreeSmall afs_osi_Free

#define afs_suser suser
#define wakeup afs_osi_Wakeup

#undef vType
#define vType(V) ((( vnode_t *)V)->v_type & S_IFMT)

/* IsAfsVnode relies on the fast that there is only one vnodeop table for AFS.
 * Use the same type of test as other OS's for compatibility.
 */
#undef IsAfsVnode
extern struct vnodeops afs_dir_iops, afs_symlink_iops;
#define IsAfsVnode(vc) (((vc)->v_op == afs_ops) ? 1 : \
			((vc)->v_op == &afs_dir_iops) ? 1 : \
			((vc)->v_op == &afs_symlink_iops))

#if 0
/* bcopy is in stds.h, just so fcrypt.c can pick it up. */
#define bzero(D,C)   memset((D), 0, (C))
#define bcmp(A,B,C)  memcmp((A), (B), (C))
#endif

/* We often need to pretend we're in user space to get memory transfers
 * right for the kernel calls we use.
 */
#ifdef KERNEL_SPACE_DECL
#undef KERNEL_SPACE_DECL
#undef TO_USER_SPACE
#undef TO_KERNEL_SPACE
#endif
#define KERNEL_SPACE_DECL mm_segment_t _fs_space_decl
#define TO_USER_SPACE() { _fs_space_decl = get_fs(); set_fs(get_ds()); }
#define TO_KERNEL_SPACE() set_fs(_fs_space_decl)

/* Backwards compatibilty macros - copyin/copyout are redone because macro
 * inside parentheses is not evalutated.
 */
#define memcpy_fromfs copy_from_user
#define memcpy_tofs copy_to_user
#define copyin(F, T, C)  (copy_from_user ((char*)(T), (char*)(F), (C)), 0)
#define copyinstr(F, T, C, L) (copyin(F, T, C), *(L)=strlen(T), 0)
#define copyout(F, T, C) (copy_to_user ((char*)(T), (char*)(F), (C)), 0)

/* kernel print statements */
#define printf printk
#define uprintf printk


#define PAGESIZE PAGE_SIZE

/* cred struct */
typedef struct cred {		/* maps to task field: */
#if (CPU == sparc64)
    long cr_ref;
#else
    int cr_ref;
#endif
    uid_t cr_uid;	/* euid */
    uid_t cr_ruid;	/* uid */
    gid_t cr_gid;	/* egid */
    gid_t cr_rgid;	/* gid */
    gid_t cr_groups[NGROUPS];	/* 32 groups - empty set to NOGROUP */
    int cr_ngroups;
} cred_t;
#define AFS_UCRED cred
#define AFS_PROC struct task_struct
#define crhold(c) (c)->cr_ref++

/* UIO manipulation */
typedef enum { AFS_UIOSYS, AFS_UIOUSER } uio_seg_t;
typedef enum { UIO_READ, UIO_WRITE } uio_flag_t;
typedef struct uio {
    struct  	iovec *uio_iov;
    int     	uio_iovcnt;
    afs_offs_t 	uio_offset;
    uio_seg_t   uio_seg;
    int     	uio_resid;
    uio_flag_t 	uio_flag;
} uio_t;
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_fmode	uio_fmode
#define	afsio_resid	uio_resid

/* Get/set the inode in the osifile struct. */
#define FILE_INODE(F) (F)->f_dentry->d_inode


/* page offset is obtained and stored here during module initialization 
 * We need a variable to do this because, the PAGE_OFFSET macro defined in
 * include/asm/page.h can change from kernel to kernel and we cannot use
 * the hardcoded version.
 */
extern unsigned long afs_linux_page_offset;

/* some more functions to help with the page offset stuff */
#define AFS_LINUX_MAP_NR(addr) ((((unsigned long)addr) - afs_linux_page_offset)  >> PAGE_SHIFT)

#define afs_linux_page_address(page) (afs_linux_page_offset + PAGE_SIZE * (page - mem_map))

#if defined(__KERNEL__) && defined(CONFIG_SMP)
#include "linux/wait.h"

extern struct semaphore afs_global_lock;
extern int afs_global_owner;

#define AFS_GLOCK() \
do { \
	 down(&afs_global_lock); \
	 if (afs_global_owner) \
	     osi_Panic("afs_global_lock already held by pid %d", \
		       afs_global_owner); \
	 afs_global_owner = current->pid; \
} while (0)

#define ISAFS_GLOCK() (afs_global_owner == current->pid)

#define AFS_GUNLOCK() \
do { \
    if (!ISAFS_GLOCK()) \
	osi_Panic("afs global lock not held at %s:%d", __FILE__, __LINE__); \
    afs_global_owner = 0; \
    up(&afs_global_lock); \
} while (0)


#else
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define ISAFS_GLOCK() 1
#define AFS_ASSERT_GLOCK()
#define AFS_ASSERT_RXGLOCK()
#endif

#define AFS_RXGLOCK()
#define AFS_RXGUNLOCK()
#define ISAFS_RXGLOCK() 1
#endif /* OSI_MACHDEP_H_ */
