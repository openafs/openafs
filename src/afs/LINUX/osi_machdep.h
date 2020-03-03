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
# define quad_t __quad_t
# define u_quad_t __u_quad_t
#endif

#undef getuerror

#ifdef STRUCT_TASK_STRUCT_HAS_TGID
# define getpid() current->tgid
# ifdef STRUCT_TASK_STRUCT_HAS_REAL_PARENT
#  define getppid() current->real_parent->tgid
# elif defined(STRUCT_TASK_STRUCT_HAS_PARENT)
#  define getppid() current->parent->tgid
# else
#  define getppid() current->p_opptr->tgid
# endif
#else /* !STRUCT_TASK_STRUCT_HAS_TGID */
# define getpid() current->pid
# ifdef STRUCT_TASK_STRUCT_HAS_REAL_PARENT
#  define getppid() current->real_parent->pid
# elif defined(STRUCT_TASK_STRUCT_HAS_PARENT)
#  define getppid() current->parent->pid
# else
#  define getppid() current->p_opptr->pid
# endif
#endif /* STRUCT_TASK_STRUCT_HAS_TGID */

#ifdef RECALC_SIGPENDING_TAKES_VOID
# define RECALC_SIGPENDING(X) recalc_sigpending()
#else
# define RECALC_SIGPENDING(X) recalc_sigpending(X)
#endif

#if defined (STRUCT_TASK_STRUCT_HAS_SIGMASK_LOCK)
# define SIG_LOCK(X) spin_lock_irq(&X->sigmask_lock)
# define SIG_UNLOCK(X) spin_unlock_irq(&X->sigmask_lock)
#elif defined (STRUCT_TASK_STRUCT_HAS_SIGHAND)
# define SIG_LOCK(X) spin_lock_irq(&X->sighand->siglock)
# define SIG_UNLOCK(X) spin_unlock_irq(&X->sighand->siglock)
#else
# define SIG_LOCK(X) spin_lock_irq(&X->sig->siglock)
# define SIG_UNLOCK(X) spin_unlock_irq(&X->sig->siglock)
#endif

#if defined (STRUCT_TASK_STRUCT_HAS_RLIM)
# define TASK_STRUCT_RLIM rlim
#elif defined (STRUCT_TASK_STRUCT_HAS_SIGNAL_RLIM)
# define TASK_STRUCT_RLIM signal->rlim
#else
# error Not sure what to do about rlim (should be in the Linux task struct somewhere....)
#endif


#define afs_hz HZ
#include "h/sched.h"
/* in case cred.h is present but not included in sched.h */
#if defined(HAVE_LINUX_CRED_H)
#include "h/cred.h"
#endif

#if !defined(HAVE_LINUX_TIME_T)
typedef time64_t time_t;
#endif

#if defined(HAVE_LINUX_KTIME_GET_COARSE_REAL_TS64)
static inline time_t osi_Time(void) {
    struct timespec64 xtime;
    ktime_get_coarse_real_ts64(&xtime);
    return xtime.tv_sec;
}
#elif defined(HAVE_LINUX_CURRENT_KERNEL_TIME)
static inline time_t osi_Time(void) {
    struct timespec xtime;
    xtime = current_kernel_time();
    return xtime.tv_sec;
}
#else
# define osi_Time() (xtime.tv_sec)
#endif

#if defined(HAVE_LINUX_KTIME_GET_REAL_TS64)
# define osi_GetTime(V)                                      \
    do {                                                     \
	struct timespec64 __afs_tv;                          \
	ktime_get_real_ts64(&__afs_tv);                      \
	(V)->tv_sec = (afs_int32)__afs_tv.tv_sec;            \
	(V)->tv_usec = (afs_int32)__afs_tv.tv_nsec / 1000;   \
    } while(0)
#elif defined(AFS_LINUX_64BIT_KERNEL)
# define osi_GetTime(V)                                 \
    do {                                               \
       struct timeval __afs_tv;                              \
       do_gettimeofday(&__afs_tv);                           \
       (V)->tv_sec = (afs_int32)__afs_tv.tv_sec;             \
       (V)->tv_usec = (afs_int32)__afs_tv.tv_usec;           \
    } while (0)
#else
# define osi_GetTime(V) do_gettimeofday((V))
#endif

#undef gop_lookupname
#define gop_lookupname osi_lookupname

#undef gop_lookupname_user
#define gop_lookupname_user osi_lookupname

#define osi_vnhold(V, N) do { VN_HOLD(AFSTOV(V)); } while (0)
#define VN_HOLD(V) osi_Assert(igrab((V)) == (V))
#define VN_RELE(V) iput((V))

#define afs_suser(x) capable(CAP_SYS_ADMIN)
extern int afs_osi_Wakeup(void *event);
static inline void
wakeup(void *event)
{
    afs_osi_Wakeup(event);
}

#define vType(V) ((AFSTOV((V)))->i_mode & S_IFMT)
#define vSetType(V, type) AFSTOV((V))->i_mode = ((type) | (AFSTOV((V))->i_mode & ~S_IFMT))	/* preserve mode */
#define vSetVfsp(V, vfsp)				/* unused */
#define IsAfsVnode(V) ((V)->i_sb == afs_globalVFS)	/* test superblock instead */
#define SetAfsVnode(V)					/* unnecessary */

#if defined(HAVE_LINUX_UACCESS_H)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

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
#define printf(args...) printk(args)
#define uprintf(args...) printk(args)


#ifndef NGROUPS
#define NGROUPS NGROUPS_SMALL
#endif

#ifdef STRUCT_GROUP_INFO_HAS_GID
/* compat macro for Linux 4.9 */
#define GROUP_AT(gi,x)  ((gi)->gid[x])
#endif

typedef struct task_struct afs_proc_t;

#ifdef HAVE_LINUX_KUID_T

#include <linux/uidgid.h>
typedef kuid_t afs_kuid_t;
typedef kgid_t afs_kgid_t;
extern struct user_namespace *afs_ns;
# ifdef CONFIG_USER_NS
#  define afs_current_user_ns() current_user_ns()
# else
/* Here current_user_ns() expands to GPL-only init_user_ns symbol! */
#  define afs_current_user_ns() ((struct user_namespace *)NULL)
# endif

static inline kuid_t afs_make_kuid(uid_t uid) {
    return make_kuid(afs_ns, uid);
}
static inline kgid_t afs_make_kgid(gid_t gid) {
    return make_kgid(afs_ns, gid);
}
static inline uid_t afs_from_kuid(kuid_t kuid) {
    return from_kuid(afs_ns, kuid);
}
static inline uid_t afs_from_kgid(kgid_t kgid) {
    return from_kgid(afs_ns, kgid);
}

#else

typedef uid_t afs_kuid_t;
typedef gid_t afs_kgid_t;

static inline afs_kuid_t afs_make_kuid(uid_t uid) {return uid;}
static inline afs_kgid_t afs_make_kgid(gid_t gid) {return gid;}
static inline uid_t afs_from_kuid(afs_kuid_t kuid) {return kuid;}
static inline gid_t afs_from_kgid(afs_kgid_t kgid) {return kgid;}
static inline unsigned char uid_eq(uid_t a, uid_t b) {return a == b;}
static inline unsigned char gid_eq(gid_t a, gid_t b) {return a == b;}
static inline unsigned char uid_lt(uid_t a, uid_t b) {return a < b;}
static inline unsigned char gid_lt(gid_t a, gid_t b) {return a < b;}
#define GLOBAL_ROOT_UID ((afs_kuid_t) 0)
#define GLOBAL_ROOT_GID ((afs_kgid_t) 0)

#endif

/* Credentials.  For newer kernels we use the kernel structure directly. */
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)

typedef struct cred afs_ucred_t;
typedef struct cred cred_t;

# define afs_cr_uid(cred) (afs_from_kuid((cred)->fsuid))
# define afs_cr_gid(cred) (afs_from_kgid((cred)->fsgid))
# define afs_cr_ruid(cred) (afs_from_kuid((cred)->uid))
# define afs_cr_rgid(cred) (afs_from_kgid((cred)->gid))
# define afs_cr_group_info(cred) ((cred)->group_info)
# define crhold(c) (get_cred(c))
static inline void
afs_set_cr_uid(cred_t *cred, uid_t uid) {
    cred->fsuid = afs_make_kuid(uid);
}
static inline void
afs_set_cr_gid(cred_t *cred, gid_t gid) {
    cred->fsgid = afs_make_kgid(gid);
}
static inline void
afs_set_cr_ruid(cred_t *cred, uid_t uid) {
    cred->uid = afs_make_kuid(uid);
}
static inline void
afs_set_cr_rgid(cred_t *cred, gid_t gid) {
    cred->gid = afs_make_kgid(gid);
}
static inline void
afs_set_cr_group_info(cred_t *cred, struct group_info *group_info) {
    cred->group_info = group_info;
}

# define current_group_info() (current->cred->group_info)
# define task_gid(task) (task->cred->gid)
# define task_user(task) (task->cred->user)
# if defined(STRUCT_CRED_HAS_SESSION_KEYRING)
#  define task_session_keyring(task) (task->cred->session_keyring)
#  define current_session_keyring() (current->cred->session_keyring)
# else
#  define task_session_keyring(task) (task->cred->tgcred->session_keyring)
#  define current_session_keyring() (current->cred->tgcred->session_keyring)
# endif

#else

typedef struct afs_cred {
    atomic_t cr_ref;
    uid_t cr_uid;
    uid_t cr_ruid;
    gid_t cr_gid;
    gid_t cr_rgid;
    struct group_info *cr_group_info;
} cred_t;

typedef struct afs_cred afs_ucred_t;
# define afs_cr_group_info(cred) ((cred)->cr_group_info)
static inline void
afs_set_cr_group_info(cred_t *cred, struct group_info *group_info) {
    cred->cr_group_info = group_info;
}

# define current_group_info() (current->group_info)
# if !defined(task_gid)
#  define task_gid(task) (task->gid)
# endif
# if !defined(task_uid)
#  define task_uid(task) (task->uid)
# endif
# define task_user(task) (task->user)
# define task_session_keyring(task) (task->signal->session_keyring)
# define current_session_keyring() (current->signal->session_keyring)
# define crhold(c) atomic_inc(&(c)->cr_ref)

#endif /* defined(STRUCT_TASK_STRUCT_HAS_CRED) */

#if !defined(current_cred)
# define current_gid() (current->gid)
# define current_uid() (current->uid)
# define current_fsgid() (current->fsgid)
# define current_fsuid() (current->fsuid)
#endif

/* UIO manipulation */
typedef enum { AFS_UIOSYS, AFS_UIOUSER } uio_seg_t;
typedef enum { UIO_READ, UIO_WRITE } uio_flag_t;
struct uio {
    struct iovec *uio_iov;
    int uio_iovcnt;
    afs_offs_t uio_offset;
    uio_seg_t uio_seg;
    int uio_resid;
    uio_flag_t uio_flag;
};
#define	afsio_iov	uio_iov
#define	afsio_iovcnt	uio_iovcnt
#define	afsio_offset	uio_offset
#define	afsio_seg	uio_segflg
#define	afsio_fmode	uio_fmode
#define	afsio_resid	uio_resid

/* Get/set the inode in the osifile struct. */
#define FILE_INODE(F) (F)->f_dentry->d_inode

#define OSIFILE_INODE(a) FILE_INODE((a)->filp)

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX20_ENV) && !defined(AFS_IA64_LINUX20_ENV)
# define NEED_IOCTL32
#endif

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/wait.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
extern struct mutex afs_global_lock;
#else
extern struct semaphore afs_global_lock;
# define mutex_lock(lock) down(lock)
# define mutex_unlock(lock) up(lock)
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

#define osi_InitGlock()

#ifdef AFS_AMD64_LINUX20_ENV
/* RHEL5 beta's kernel doesn't define these. They aren't gonna change, so... */

# ifndef __NR_ia32_afs_syscall
#  define __NR_ia32_afs_syscall 137
# endif
# ifndef __NR_ia32_setgroups
#  define __NR_ia32_setgroups 81
# endif
# ifndef __NR_ia32_setgroups32
#  define __NR_ia32_setgroups32 206
# endif
# ifndef __NR_ia32_close
#  define __NR_ia32_close 6
# endif
# ifndef __NR_ia32_chdir
#  define __NR_ia32_chdir 12
# endif
# ifndef __NR_ia32_break
#  define __NR_ia32_break 17
# endif
# ifndef __NR_ia32_stty
#  define __NR_ia32_stty 31
# endif
# ifndef __NR_ia32_gtty
#  define __NR_ia32_gtty 32
# endif
# ifndef __NR_ia32_ftime
#  define __NR_ia32_ftime 35
# endif
# ifndef __NR_ia32_prof
#  define __NR_ia32_prof 44
# endif
# ifndef __NR_ia32_lock
#  define __NR_ia32_lock 53
# endif
# ifndef __NR_ia32_mpx
#  define __NR_ia32_mpx 56
# endif
# ifndef __NR_ia32_exit
#  define __NR_ia32_exit 1
# endif
# ifndef __NR_ia32_mount
#  define __NR_ia32_mount 21
# endif
# ifndef __NR_ia32_read
#  define __NR_ia32_read 3
# endif
# ifndef __NR_ia32_write
#  define __NR_ia32_write 4
# endif
# ifndef __NR_ia32_open
#  define __NR_ia32_open 5
# endif
# ifndef __NR_ia32_close
#  define __NR_ia32_close 6
# endif
# ifndef __NR_ia32_unlink
#  define __NR_ia32_unlink 10
# endif
#endif

#define osi_procname(procname, size) strncpy(procname, current->comm, size)

#endif /* OSI_MACHDEP_H_ */
