/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include <sys/namei.h>

#ifndef PATHBUFLEN
#define PATHBUFLEN 256
#endif

#ifdef AFS_DARWIN80_ENV
int
osi_lookupname(char *aname, enum uio_seg seg, int followlink,
	       struct vnode **vpp) {
  vfs_context_t ctx;
  char tname[PATHBUFLEN];
  int code, flags;
  size_t len;

  if (seg == AFS_UIOUSER) { /* XXX 64bit */
     AFS_COPYINSTR(aname, tname, sizeof(tname), &len, code);
     if (code)
       return code;
     aname=tname;
  }
  flags = 0;
  if (!followlink)
	flags |= VNODE_LOOKUP_NOFOLLOW;
  ctx=vfs_context_create(NULL);
  code = vnode_lookup(aname, flags, vpp, ctx);
  if (!code) { /* get a usecount */
    vnode_ref(*vpp);
    vnode_put(*vpp);
  }
  vfs_context_rele(ctx);
  return code;
}
#else
int
osi_lookupname(char *aname, enum uio_seg seg, int followlink,
	       struct vnode **vpp)
{
    struct nameidata n;
    int flags, error;
    flags = 0;
    flags = LOCKLEAF;
    if (followlink)
	flags |= FOLLOW;
    else
	flags |= NOFOLLOW;
    NDINIT(&n, LOOKUP, flags, seg, aname, current_proc());
    if (error = namei(&n))
	return error;
    *vpp = n.ni_vp;
   /* should we do this? */
    VOP_UNLOCK(n.ni_vp, 0, current_proc());
    return 0;
}
#endif

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */
int
afs_suser(void *credp)
{
    int error;
    struct proc *p = current_proc();

#ifdef AFS_DARWIN80_ENV
    if ((error = proc_suser(p)) == 0) {
	return (1);
    }
    return (0);
#else
    if ((error = suser(p->p_ucred, &p->p_acflag)) == 0) {
	return (1);
    }
    return (0);
#endif
}

#ifdef AFS_DARWIN80_ENV
uio_t afsio_darwin_partialcopy(uio_t auio, int size) {
   uio_t res;
   int i;
   user_addr_t iovaddr;
   user_size_t iovsize;

   /* XXX 64 bit userspaace? */
   res = uio_create(uio_iovcnt(auio), uio_offset(auio),
                    uio_isuserspace(auio) ? UIO_USERSPACE32 : UIO_SYSSPACE32,
                    uio_rw(auio));

   for (i = 0;i < uio_iovcnt(auio) && size > 0;i++) {
       if (uio_getiov(auio, i, &iovaddr, &iovsize))
           break;
       if (iovsize > size)
          iovsize = size;
       if (uio_addiov(res, iovaddr, iovsize))
          break;
       size -= iovsize;
   }
   return res;
}

vfs_context_t afs_osi_ctxtp;
int afs_osi_ctxtp_initialized;
static thread_t vfs_context_owner;
#define RECURSIVE_VFS_CONTEXT 1
#if RECURSIVE_VFS_CONTEXT
static proc_t vfs_context_curproc;
int vfs_context_ref;
#else 
#define vfs_context_ref 1
#endif
void get_vfs_context(void) {
  int isglock = ISAFS_GLOCK();

  if (!isglock)
     AFS_GLOCK();
  if (afs_osi_ctxtp_initialized) {
     if (!isglock)
        AFS_GUNLOCK();
      return;
  }
  osi_Assert(vfs_context_owner != current_thread());
#if RECURSIVE_VFS_CONTEXT
  if (afs_osi_ctxtp && current_proc() == vfs_context_curproc) {
     vfs_context_ref++;
     vfs_context_owner = current_thread();
     if (!isglock)
        AFS_GUNLOCK();
     return;
  }
#endif
  while (afs_osi_ctxtp && vfs_context_ref) {
     printf("[%d] waiting for afs_osi_ctxtp\n", proc_selfpid());
     afs_osi_Sleep(&afs_osi_ctxtp);
     if (afs_osi_ctxtp_initialized) {
       printf("[%d] ok\n", proc_selfpid());
       if (!isglock)
          AFS_GUNLOCK();
       return;
     }
     if (!afs_osi_ctxtp || !vfs_context_ref)
        printf("[%d] ok\n", proc_selfpid());
  }
#if RECURSIVE_VFS_CONTEXT
  vfs_context_rele(afs_osi_ctxtp);
  vfs_context_ref=1;
#else
  osi_Assert(vfs_context_owner == (thread_t)0);
#endif
  afs_osi_ctxtp = vfs_context_create(NULL);
  vfs_context_owner = current_thread();
  vfs_context_curproc = current_proc();
  if (!isglock)
     AFS_GUNLOCK();
}

void put_vfs_context(void) {
  int isglock = ISAFS_GLOCK();

  if (!isglock)
     AFS_GLOCK();
  if (afs_osi_ctxtp_initialized) {
     if (!isglock)
        AFS_GUNLOCK();
      return;
  }
#if RECURSIVE_VFS_CONTEXT
  if (vfs_context_owner == current_thread())
      vfs_context_owner = (thread_t)0;
  vfs_context_ref--;
#else
  osi_Assert(vfs_context_owner == current_thread());
  vfs_context_rele(afs_osi_ctxtp);
  afs_osi_ctxtp = NULL;
  vfs_context_owner = (thread_t)0;
#endif
  afs_osi_Wakeup(&afs_osi_ctxtp);
     if (!isglock)
        AFS_GUNLOCK();
}

extern int afs3_syscall();

int afs_cdev_nop_openclose(dev_t dev, int flags, int devtype,struct proc *p) {
  return 0;
}
int
afs_cdev_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p) {
   int retval=0;
   struct afssysargs *a = data;
   if (proc_is64bit(p))
     return EINVAL;

  if (cmd != VIOC_SYSCALL) {
     printf("ioctl mismatch 0x%lx (wanted 0x%lx)\n", cmd, VIOC_SYSCALL);
     /*return EINVAL;*/
  }

 afs3_syscall(p, (struct afssysargs *)data, &retval);
 return retval; 
}

#endif
