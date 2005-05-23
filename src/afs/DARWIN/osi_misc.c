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
    return proc_suser(p);
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
#endif
