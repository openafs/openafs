/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifdef UKERNEL
# include <UKERNEL/sysincludes.h>
#else

#ifndef __AFS_SYSINCLUDESH__
#define __AFS_SYSINCLUDESH__ 1

#ifdef AFS_OBSD_ENV
# include <sys/errno.h>
# include <sys/types.h>
# include <sys/param.h>
# include <sys/lock.h>
# include <sys/queue.h>
# include <sys/resourcevar.h>
# include <sys/kernel.h>
# include <sys/proc.h>
# include <sys/time.h>
# include <sys/filedesc.h>
# include <sys/file.h>
# include <sys/socket.h>
# include <sys/socketvar.h>
# include <sys/dirent.h>
# include <sys/user.h>
# include <sys/uio.h>
# include <sys/buf.h>
# include <sys/stat.h>
# include <sys/mount.h>
# include <sys/vnode.h>
# include <sys/malloc.h>
# include <ufs/ffs/fs.h>
# include <ufs/ufs/quota.h>
# include <ufs/ufs/inode.h>
# ifndef AFS_OBSD38_ENV
#  include <ufs/ufs/extattr.h>
# endif
# include <ufs/ufs/ufsmount.h>
# ifndef MLEN
#  include <sys/mbuf.h>
#  include <net/if.h>
# endif
# include <sys/protosw.h>
# include <sys/ioctl.h>
# include <sys/timeout.h>

#elif defined(AFS_NBSD40_ENV)
# include <sys/errno.h>
# include <sys/types.h>
# include <sys/mount.h> /* may define MOUNT_AFS */
# include <sys/param.h>
# include <sys/systm.h>
# include <sys/conf.h>
# include <sys/exec.h>
# include <sys/lock.h>
# include <sys/syscall.h>
# include <sys/syscallargs.h>
# include <sys/queue.h>
# include <sys/resourcevar.h>
# include <sys/kernel.h>
# include <sys/proc.h>
# include <sys/time.h>
# include <sys/filedesc.h>
# include <sys/exec.h>
# include <sys/lock.h>
# include <sys/syscall.h>
# include <sys/syscallargs.h>
# include <sys/queue.h>
# include <sys/resourcevar.h>
# include <sys/kernel.h>
# include <sys/proc.h>
# include <sys/time.h>
# include <sys/filedesc.h>
# include <sys/file.h>
# include <sys/socket.h>
# include <sys/socketvar.h>
# include <sys/dirent.h>
# include <sys/user.h>
# include <sys/kauth.h>
# include <sys/uio.h>
# include <sys/buf.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <sys/namei.h>
# include <sys/socket.h>
# include <sys/socketvar.h>
# include <sys/dirent.h>
# include <sys/user.h>
# include <sys/kauth.h>
# include <sys/uio.h>
# include <sys/buf.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <sys/namei.h>
# include <sys/vnode.h>
# include <ufs/ffs/fs.h>
# include <ufs/ufs/quota.h>
# include <ufs/ufs/inode.h>
# include <ufs/ufs/extattr.h>
# include <ufs/ufs/ufsmount.h>
# ifndef MLEN
#  if 0
#   include <sys/mbuf.h>
#  endif /* 0 */
#  include <net/if.h>
# endif /* !MLEN */
# include <sys/protosw.h>
# include <sys/ioctl.h>

#elif defined(AFS_LINUX22_ENV)
# include <linux/version.h>
# ifdef HAVE_LINUX_CONFIG_H
#  include <linux/config.h>
# endif
# include <linux/linkage.h>
# include <linux/limits.h>
# include <linux/wait.h>
# include <linux/types.h>
# include <linux/vfs.h>
# include <linux/net.h>
# include <linux/kdev_t.h>
# include <linux/ioctl.h>
# if defined(AFS_LINUX26_ENV)
#  include <linux/backing-dev.h>
#  include <linux/pagemap.h>
#  include <linux/namei.h>
#  ifdef STRUCT_INODE_HAS_I_SECURITY
#   include <linux/security.h>
#  endif
#  include <linux/suspend.h>
# endif
/* Avoid conflicts with coda overloading AFS type namespace. Must precede
 * inclusion of uaccess.h.
 */
# ifndef _LINUX_CODA_FS_I
#  define _LINUX_CODA_FS_I
#  define _CODA_HEADER_
#  define _CFS_HEADER_
struct coda_inode_info {
};
# endif
# ifndef _LINUX_XFS_FS_I
#  define _LINUX_XFS_FS_I
struct xfs_inode_info {
};
# endif
# include <asm/uaccess.h>
# include <linux/list.h>
# include <linux/dcache.h>
# include <linux/mount.h>
# include <linux/fs.h>
# include <linux/quota.h>
# include <linux/sched.h>
# include <linux/mm.h>
# include <linux/slab.h>
# include <linux/proc_fs.h>
# include <linux/string.h>
# if defined(HAVE_LINUX_SEMAPHORE_H)
#  include <linux/semaphore.h>
# else
#  include <asm/semaphore.h>
# endif
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
#  include <linux/mutex.h>
# endif
# include <linux/errno.h>
# ifdef HAVE_LINUX_COMPLETION_H
#  include <linux/completion.h>
# endif
# if defined(HAVE_LINUX_EXPORTFS_H)
#  include <linux/exportfs.h>
# endif
# ifdef AFS_LINUX24_ENV
#  include <linux/pagemap.h>
# endif

#else /* AFS_LINUX22_ENV */
# if defined(AFS_DARWIN_ENV)
#  ifndef _MACH_ETAP_H_
#   define _MACH_ETAP_H_
typedef unsigned short etap_event_t;
#  endif
# endif

# include "h/errno.h"
# include "h/types.h"
# include "h/param.h"

# ifdef	AFS_AUX_ENV
#  ifdef	PAGING
#   include "h/mmu.h"
#   include "h/seg.h"
#   include "h/page.h"
#   include "h/region.h"
#  endif /* PAGING */
#  include "h/sysmacros.h"
#  include "h/signal.h"
#  include "h/var.h"
# endif /* AFS_AUX_ENV */

# include "h/systm.h"
# include "h/time.h"

# ifdef AFS_FBSD_ENV
#  include <sys/socket.h>
#  include <net/if.h>
#  include <net/if_var.h>
# endif

# ifdef	AFS_AIX_ENV
#  ifdef AFS_AIX41_ENV
#   include "sys/statfs.h"
#  endif
#  ifdef AFS_AIX51_ENV
#   include "sys/acl.h"
#  endif
#  include "../h/file.h"
#  include "../h/fullstat.h"
#  include "../h/vattr.h"
#  include "../h/var.h"
#  include "../h/access.h"
# endif /* AFS_AIX_ENV */

# if defined(AFS_SGI_ENV)
#  include "values.h"
#  include "sys/sema.h"
#  include "sys/cmn_err.h"
#  ifdef AFS_SGI64_ENV
#   include <ksys/behavior.h>
/* in 6.5.20f, ksys/behavior.h doesn't bother to define BHV_IS_BHVL,
 * but sys/vnode.h uses it in VNODE_TO_FIRST_BHV. It looks like from
 * older headers like we want the old behavior, so we fake it. */
#   if defined(BHV_PREPARE) && !defined(CELL_CAPABLE)
#    define BHV_IS_BHVL(bhp) (0)
#   endif
#  endif /* AFS_SGI64_ENV */
#  include "fs/efs_inode.h"
#  include "sys/kmem.h"
#  include "sys/cred.h"
#  include "sys/resource.h"

/*
 * ../sys/debug.h defines ASSERT(), but it is nontrivial only if DEBUG
 * is on (see afs_osi.h).
 * In IRIX 6.5 we cannot have DEBUG turned on since certain
 * system-defined structures have different members with DEBUG on, and
 * this breaks our ability to interact with the rest of the kernel.
 *
 * Instead of using ASSERT(), we use our own osi_Assert().
 */
#  if defined(AFS_SGI65_ENV) && !defined(DEBUG)
#   define DEBUG
#   include "sys/debug.h"
#   undef DEBUG
#  else
#   include "sys/debug.h"
#  endif

#  include "sys/statvfs.h"
#  include "sys/sysmacros.h"
#  include "sys/fs_subr.h"
#  include "sys/siginfo.h"
#  include "limits.h"
# endif /* AFS_SGI_ENV */

# if !defined(AFS_AIX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_SGI_ENV)
#  include "h/kernel.h"
# endif /* !SUN5 && !SGI */

# ifdef	AFS_SUN5_ENV
#  include <sys/cmn_err.h>	/* for kernel printf() prototype */
# endif

# if	defined(AFS_SUN56_ENV)
#  include "h/vfs.h"		/* stops SUN56 socketvar.h warnings */
#  include "h/stropts.h"		/* stops SUN56 socketvar.h warnings */
#  include "h/stream.h"		/* stops SUN56 socketvar.h errors */
# endif

# ifdef AFS_SUN510_ENV
#  include <sys/cred_impl.h>
#  include <sys/policy.h>
# endif

# include "h/socket.h"
# include "h/socketvar.h"
# include "h/protosw.h"

# if defined(AFS_FBSD_ENV)
#  include "sys/limits.h"
# endif

# ifdef AFS_HPUX_ENV
#  include "limits.h"
# endif

# if defined(AFS_SGI_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) || \
  defined(AFS_FBSD_ENV) || defined(AFS_NBSD40_ENV)
#  include "h/dirent.h"
#  ifdef	AFS_SUN5_ENV
#   include "h/sysmacros.h"
#   include "h/fs/ufs_fsdir.h"
#  endif /* AFS_SUN5_ENV */
# else
#  include "h/dir.h"
# endif /* SGI || SUN || HPUX */

# if !defined(AFS_SGI64_ENV) && !defined(AFS_FBSD_ENV) && !defined(AFS_DARWIN80_ENV) && \
  !defined(AFS_NBSD40_ENV)
#  include "h/user.h"
# endif /* AFS_SGI64_ENV */
# define	MACH_USER_API	1
# if defined(AFS_FBSD_ENV)
#  include "h/bio.h"
#  include "h/filedesc.h"
# endif
# include "h/file.h"
# include "h/uio.h"
# include "h/buf.h"
# include "h/stat.h"


/* ----- The following mainly deal with vnodes/inodes stuff ------ */
# ifdef	AFS_SUN5_ENV
#  include "h/statvfs.h"
# endif /* AFS_SUN5_ENV */
# ifdef AFS_HPUX_ENV
struct vfspage;		/* for vnode.h compiler warnings */
#  include "h/swap.h"	/* for struct swpdbd, for vnode.h compiler warnings */
#  include "h/dbd.h"	/* for union idbd, for vnode.h compiler warnings */
#  ifdef AFS_HPUX110_ENV
#   include "h/resource.h"
#  endif
#  ifdef AFS_HPUX1123_ENV
#   include <sys/user.h>
#   include <sys/cred.h>
#  endif
# endif /* AFS_HPUX_ENV */
# if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
#  if defined(AFS_FBSD_ENV)
struct vop_getwritemount_args;
#  endif
#  include <sys/uio.h>
#  include <sys/mount.h>
#  include <sys/namei.h>
#  ifdef AFS_DARWIN80_ENV
#   include <sys/kauth.h>
#   include <string.h>
#  endif
#  include <sys/vnode.h>
#  include <sys/queue.h>
#  ifndef AFS_NBSD40_ENV
#   include <sys/malloc.h>
#  endif
#  ifdef AFS_FBSD_ENV
MALLOC_DECLARE(M_AFS);
#   include <ufs/ufs/dinode.h>
#   include <vm/vm.h>
#   include <vm/vm_extern.h>
#   include <vm/pmap.h>
#   include <vm/vm_map.h>
#   include <sys/lock.h>
#   include <sys/user.h>
#  else
#   include <sys/ubc.h>
#   define timeout_fcn_t mach_timeout_fcn_t
#   include <kern/sched_prim.h>
#  endif
#  undef timeout_fcn_t
#  define _DIR_H_
#  define doff_t          int32_t
#  ifndef AFS_DARWIN80_ENV
#   include <ufs/ufs/quota.h>
#   include <ufs/ufs/inode.h>
#   include <ufs/ffs/fs.h>
#  endif
# else
#  ifndef AFS_NBSD40_ENV
#   include "h/vfs.h"
#   include "h/vnode.h"
#  endif
#  ifdef	AFS_SUN5_ENV
#   include "h/fs/ufs_inode.h"
#   include "h/fs/ufs_mount.h"
#  else
#   if !defined(AFS_SGI_ENV) && !defined(AFS_AIX32_ENV) && !defined(AFS_NBSD40_ENV)
#    include "ufs/inode.h"
#    if !defined(AFS_SGI_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_NBSD40_ENV)
#     include "ufs/mount.h"
#    endif /* !AFS_HPUX_ENV */
#   endif /* !AFS_AIX32_ENV */
#  endif /* AFS_SUN5_ENV */
# endif /* AFS_DARWIN_ENV || AFS_FBSD_ENV */

/* These mainly deal with networking and rpc headers */
# include "netinet/in.h"
# undef	MFREE			/* defined at mount.h for AIX */
# ifdef	AFS_SUN5_ENV
#  include "h/time.h"
# else
#  if !defined(AFS_HPUX_ENV)
#   include "h/mbuf.h"
#  endif
# endif /* AFS_SUN5_ENV */

# include "rpc/types.h"
# include "rx/xdr.h"

/* Miscellaneous headers */
# include "h/proc.h"
# if !defined(AFS_FBSD_ENV)
#  include "h/ioctl.h"
# endif /* AFS_FBSD_ENV */

# if	defined(AFS_HPUX101_ENV) && !defined(AFS_HPUX1123_ENV)
#  include "h/proc_iface.h"
#  include "h/vas.h"
# endif

# if	defined(AFS_HPUX102_ENV)
#  include "h/unistd.h"
#  include "h/tty.h"
# endif

# if !defined(AFS_SGI_ENV) && !defined(AFS_AIX32_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_FBSD_ENV) && !defined(AFS_NBSD40_ENV)

#  include "h/text.h"
# endif


# if	defined(AFS_AIX_ENV)
#  include "h/flock.h"		/* fcntl.h is a user-level include in aix */
# else
#  include "h/fcntl.h"
# endif /* AIX */

# if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)
#  include "h/unistd.h"
# endif /* SGI || SUN */

# ifdef	AFS_AIX32_ENV
#  include "h/vmuser.h"
# endif /* AFS_AIX32_ENV */

# if	defined(AFS_SUN5_ENV)
#  include <sys/tiuser.h>
#  include <sys/t_lock.h>
#  include <sys/mutex.h>
#  include <sys/vtrace.h>
#  include <sys/pathname.h>
#  include <sys/debug.h>
# endif

#endif /* AFS_LINUX22_ENV */

#endif /* __AFS_SYSINCLUDESH__  so idempotent */

#endif
