/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include <sys/namei.h>

int osi_lookupname(char *aname, enum uio_seg seg, int followlink,
	                  struct vnode **dirvpp, struct vnode **vpp)
{
   struct nameidata n;
   int flags,error;
   flags=0;
   flags=LOCKLEAF;
   if (followlink)
     flags|=FOLLOW;
   else 
     flags|=NOFOLLOW;
/*   if (dirvpp) flags|=WANTPARENT;*/ /* XXX LOCKPARENT? */
   NDINIT(&n, LOOKUP, flags, seg, aname, current_proc());
   if (error=namei(&n))
      return error;
   *vpp=n.ni_vp;
/*
   if (dirvpp)
      *dirvpp = n.ni_dvp;
#/
   /* should we do this? */
   VOP_UNLOCK(n.ni_vp, 0, current_proc());
   return 0;
}

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */

afs_suser() {
    int error;
    struct proc *p=current_proc();

    if ((error = suser(p->p_ucred, &p->p_acflag)) == 0) {
	return(1);
    }
    return(0);
}
