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

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)
#define AFS_LINUX26_ONEGROUP_ENV 1
#endif

/* Only needed for xdr.h in glibc 2.1.x */
#ifndef quad_t
#define quad_t __quad_t
#define u_quad_t __u_quad_t
#endif

#undef getuerror

#ifdef STRUCT_TASK_STRUCT_HAS_TGID
#define getpid() current->tgid
#ifdef STRUCT_TASK_STRUCT_HAS_REAL_PARENT
#define getppid() current->real_parent->tgid
#elif defined(STRUCT_TASK_STRUCT_HAS_PARENT)
#define getppid() current->parent->tgid
#else
#define getppid() current->p_opptr->tgid
#endif
#else /* !STRUCT_TASK_STRUCT_HAS_TGID */
#define getpid() current->pid
#ifdef STRUCT_TASK_STRUCT_HAS_REAL_PARENT
#define getppid() current->real_parent->pid
#elif defined(STRUCT_TASK_STRUCT_HAS_PARENT)
#define getppid() current->parent->pid
#else
#define getppid() current->p_opptr->pid
#endif
#endif /* STRUCT_TASK_STRUCT_HAS_TGID */

#ifdef RECALC_SIGPENDING_TAKES_VOID
#define RECALC_SIGPENDING(X) recalc_sigpending()
#else
#define RECALC_SIGPENDING(X) recalc_sigpending(X)
#endif

#if defined (STRUCT_TASK_STRUCT_HAS_SIGMASK_LOCK)
#define SIG_LOCK(X) spin_lock_irq(&X->sigmask_lock)
#define SIG_UNLOCK(X) spin_unlock_irq(&X->sigmask_lock)
#elif defined (STRUCT_TASK_STRUCT_HAS_SIGHAND)
#define SIG_LOCK(X) spin_lock_irq(&X->sighand->siglock)
#define SIG_UNLOCK(X) spin_unlock_irq(&X->sighand->siglock)
#else
#define SIG_LOCK(X) spin_lock_irq(&X->sig->siglock)
#define SIG_UNLOCK(X) spin_unlock_irq(&X->sig->siglock)
#endif

#if defined (STRUCT_TASK_STRUCT_HAS_RLIM)
#define TASK_STRUCT_RLIM rlim
#elif defined (STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM)
#define TASK_STRUCT_RLIM signal->rlim
#else
#error Not sure what to do about rlim (should be in the Linux task struct somewhere....)
#endif


#define afs_hz HZ
#include "h/sched.h"
#if defined(HAVE_CURRENT_KERNEL_TIME)
static inline time_t osi_Time(void) { 
    struct timespec xtime;
    xtime = current_kernel_time();
    return xtime.tv_sec;
}
#else
#define osi_Time() (xtime.tv_sec)
#endif



#ifdef AFS_LINUX_64BIT_KERNEL
#define osi_GetTime(V)                                 \
    do {                                               \
       struct timeval __afs_tv;                              \
       do_gettimeofday(&__afs_tv);                           \
       (V)->tv_sec = (afs_int32)__afs_tv.tv_sec;             \
       (V)->tv_usec = (afs_int32)__afs_tv.tv_usec;           \
    } while (0)
#else
#define osi_GetTime(V) do_gettimeofday((V))
#endif

#undef gop_lookupname
#define gop_lookupname osi_lookupname

#define osi_vnhold(V, N) do { VN_HOLD(AFSTOV(V)); } while (0)
#define VN_HOLD(V) osi_Assert(igrab((V)) == (V))
#define VN_RELE(V) iput((V))

#define afs_suser(x) capable(CAP_SYS_ADMIN)
#define wakeup afs_osi_Wakeup

#undef vType
#define vType(V) ((AFSTOV((V)))->i_mode & S_IFMT)
#undef vSetType
#define vSetType(V, type) AFSTOV((V))->i_mode = ((type) | (AFSTOV((V))->i_mode & ~S_IFMT))	/* preserve mode */

#undef IsAfsVnode
#define IsAfsVnode(V) ((V)->i_sb == afs_globalVFS)	/* test superblock instead */
#undef SetAfsVnode
#define SetAfsVnode(V)					/* unnecessary */

/* We often need to pretend we're in user space to get memory transfers
 * right for the kernel calls we use.
 */
#include <asm/uaccess.h>

#ifdef KERNEL_SPACE_DECL
#undef KERNEL_SPACE_DECL
#undef TO_USER_SPACE
#undef TO_KERNEL_SPACE
#endif
#define KERNEL_SPACE_DECL mm_segment_t _fs_space_decl
#define TO_USER_SPACE() { _fs_space_decl = get_fs(); set_fs(get_ds()); }
#define TO_KERNEL_SPACE() set_fs(_fs_space_decl)

#define copyin(F, T, C)  (copy_from_user ((char*)(T), (char*)(F), (C)) > 0 ? EFAULT : 0)
static inline long copyinstr(char *from, char *to, int count, int *length) {
    long tmp;
    tmp = strncpy_from_user(to, from, count);
    if (tmp < 0)
            return EFAULT;
    *length = tmp;
    return 0;
}
#define copyout(F, T, C) (copy_to_user ((char*)(T), (char*)(F), (C)) > 0 ? EFAULT : 0)

/* kernel print statements */
#define printf printk
#define uprintf printk


#ifndef NGROUPS
#define NGROUPS NGROUPS_SMALL
#endif

/* cred struct */
typedef struct cred {		/* maps to task field: */
    int cr_ref;
    uid_t cr_uid;		/* euid */
    uid_t cr_ruid;		/* uid */
    gid_t cr_gid;		/* egid */
    gid_t cr_rgid;		/* gid */
#if defined(AFS_LINUX26_ENV)
    struct group_info *cr_group_info;
#else
    gid_t cr_groups[NGROUPS];	/* 32 groups - empty set to NOGROUP */
    int cr_ngroups;
#endif
    struct cred *cr_next;
} cred_t;
#define AFS_UCRED cred
#define AFS_PROC struct task_struct
#define crhold(c) (c)->cr_ref++

/* UIO manipulation */
typedef enum { AFS_UIOSYS, AFS_UIOUSER } uio_seg_t;
typedef enum { UIO_READ, UIO_WRITE } uio_flag_t;
typedef struct uio {
    struct iovec *uio_iov;
    int uio_iovcnt;
    afs_offs_t uio_offset;
    uio_seg_t uio_seg;
    int uio_resid;
    uio_flag_t uio_flag;
} uio_t;
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_fmode	uio_fmode
#define	afsio_resid	uio_resid

/* Get/set the inode in the osifile struct. */
#define FILE_INODE(F) (F)->f_dentry->d_inode

#ifdef AFS_LINUX26_ENV
#define OSIFILE_INODE(a) FILE_INODE((a)->filp)
#else
#define OSIFILE_INODE(a) FILE_INODE(&(a)->file)
#endif

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX20_ENV) && !defined(AFS_IA64_LINUX20_ENV)
#define NEED_IOCTL32
#endif

/* page offset is obtained and stored here during module initialization 
 * We need a variable to do this because, the PAGE_OFFSET macro defined in
 * include/asm/page.h can change from kernel to kernel and we cannot use
 * the hardcoded version.
 */
extern unsigned long afs_linux_page_offset;

/* function to help with the page offset stuff */
#define afs_linux_page_address(page) (afs_linux_page_offset + PAGE_SIZE * (page - mem_map))

#if defined(__KERNEL__)
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/wait.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
extern struct mutex afs_global_lock;
#else
extern struct semaphore afs_global_lock;
#define mutex_lock(lock) down(lock)
#define mutex_unlock(lock) up(lock)
#endif
extern int afs_global_owner;

#define AFS_GLOCK() \
do { \
	 mutex_lock(&afs_global_lock); \
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
    mutex_unlock(&afs_global_lock); \
} while (0)
#else
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define ISAFS_GLOCK() 1
#define AFS_ASSERT_GLOCK()
#endif

#ifdef AFS_AMD64_LINUX20_ENV
/* RHEL5 beta's kernel doesn't define these. They aren't gonna change, so... */

#ifndef __NR_ia32_afs_syscall
#define __NR_ia32_afs_syscall 137
#endif
#ifndef __NR_ia32_setgroups
#define __NR_ia32_setgroups 81
#endif
#ifndef __NR_ia32_setgroups32
#define __NR_ia32_setgroups32 206
#endif
#ifndef __NR_ia32_close
#define __NR_ia32_close 6
#endif
#ifndef __NR_ia32_chdir
#define __NR_ia32_chdir 12
#endif
#ifndef __NR_ia32_break
#define __NR_ia32_break 17
#endif
#ifndef __NR_ia32_stty
#define __NR_ia32_stty 31
#endif
#ifndef __NR_ia32_gtty
#define __NR_ia32_gtty 32
#endif
#ifndef __NR_ia32_ftime
#define __NR_ia32_ftime 35
#endif
#ifndef __NR_ia32_prof
#define __NR_ia32_prof 44
#endif
#ifndef __NR_ia32_lock
#define __NR_ia32_lock 53
#endif
#ifndef __NR_ia32_mpx
#define __NR_ia32_mpx 56
#endif
#ifndef __NR_ia32_exit
#define __NR_ia32_exit 1
#endif
#ifndef __NR_ia32_mount
#define __NR_ia32_mount 21
#endif
#ifndef __NR_ia32_read
#define __NR_ia32_read 3
#endif
#ifndef __NR_ia32_write
#define __NR_ia32_write 4
#endif
#ifndef __NR_ia32_open
#define __NR_ia32_open 5
#endif
#ifndef __NR_ia32_close
#define __NR_ia32_close 6
#endif
#ifndef __NR_ia32_unlink
#define __NR_ia32_unlink 10
#endif
#endif

#endif /* OSI_MACHDEP_H_ */
