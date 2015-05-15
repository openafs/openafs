/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * Afs_init
 * gop_rdwr
 * aix_gnode_rele
 * afs_suser
 * aix_vattr_null
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "h/systm.h"
#include "h/types.h"
#include "h/errno.h"
#include "h/stat.h"
#include "h/user.h"
#include "h/uio.h"
#include "h/vattr.h"
#include "h/file.h"
#include "h/vfs.h"
#include "h/chownx.h"
#include "h/systm.h"
#include "h/access.h"
#include "rpc/types.h"
#include "osi_vfs.h"
#include "netinet/in.h"
#include "h/mbuf.h"
#include "rpc/types.h"
#include "rpc/xdr.h"
#include "h/vmuser.h"
#include "h/syspest.h"

#include "afs/stds.h"
#include "afs/afs_osi.h"
#define RFTP_INTERNALS 1
#include "afs/volerrors.h"
#include "afsint.h"
#include "vldbint.h"
#include "afs/lock.h"
#include "afs/exporter.h"
#include "afs/afs.h"
#include "afs/afs_stats.h"

/* In Aix one may specify an init routine routine which is called once during
 * initialization of all gfs; one day we might need to actual do somehing here.
 */
Afs_init(gfsp)
     char *gfsp;		/* this is really struct gfs *, but we do not use it */
{
    extern int afs_gn_strategy();
#define AFS_VM_BUFS 50

    /* For now nothing special is required during AFS initialization. */
    AFS_STATCNT(afs_init);

    (void)vm_mount(D_REMOTE, afs_gn_strategy, AFS_VM_BUFS);
    return 0;
}


/* Some extra handling is needed when calling the aix's version of the local
 * RDWR module, particularly the usage of the uio structure to the lower
 *  routine. Note of significant importance to the AFS port is the offset 
 * in/out parameter, which in two cases returns a new value back. The cases
 * are: (1) when it's a FIFO file (it's reset to zero) which we don't yet
 * support in AFS and (2) in a regular case when we write
 * (i.e. rw == UIO_WRITE) and are in appending mode (i.e. FAPPEND bit on)
 * where offset is set to the file's size. None of these cases apply to us
 * currently so no problems occur; caution if things change!
 */
int
gop_rdwr(rw, vp, base, len, offset, segflg, unit, aresid)
     enum uio_rw rw;
     struct vnode *vp;
     caddr_t base;
#ifdef AFS_64BIT_KERNEL
     offset_t *offset;
#else
     off_t *offset;
#endif
     int len, segflg;
     int *aresid;
     int unit;			/* Ignored */
{
    struct uio uio_struct;
    struct iovec uiovector;
    int code;

    memset(&uio_struct, 0, sizeof(uio_struct));
    memset(&uiovector, 0, sizeof(uiovector));

    AFS_STATCNT(gop_rdwr);
    /* Set up the uio structure */
    uiovector.iov_base = (caddr_t) base;
    uiovector.iov_len = len;

    uio_struct.uio_iov = &uiovector;
    uio_struct.uio_iovcnt = 1;
    uio_struct.uio_offset = *offset;
    uio_struct.uio_segflg = AFS_UIOSYS;
    uio_struct.uio_resid = len;
    uio_struct.uio_fmode = (rw == UIO_READ ? FREAD : FWRITE);

#ifdef AFS_64BIT_KERNEL
    code =
	VNOP_RDWR(vp, rw, (int32long64_t) (rw == UIO_READ ? FREAD : FWRITE),
		  &uio_struct, (ext_t) 0, (caddr_t) 0, (struct vattr *)0,
		  &afs_osi_cred);
#else
    code =
	VNOP_RDWR(vp, rw, (rw == UIO_READ ? FREAD : FWRITE), &uio_struct,
		  NULL, NULL, NULL, &afs_osi_cred);
#endif
    *aresid = uio_struct.uio_resid;
    return code;
}


/* Since in AIX a vnode is included in linked lists of its associated vfs and
 * gnode we need to remove these links when removing an AFS vnode (part of the
 * vcache entry).  Note that since the accompanied gnode was alloced during
 * vcache creation, we have to free it here too. We don't bother with the
 * vnode itself since it's part of the vcache entry and it's handled fine by
 * default.
 * 
 * Note that there is a 1-1 mapping from AFS vnodes to gnodes, so there is
 * no linked list of gnodes to remove this element from.
 */
aix_gnode_rele(vp)
     struct vnode *vp;
{
    struct vnode *tvp;
    struct vfs *vfsp = vp->v_vfsp;

    /* Unlink the vnode from the list the vfs has hanging of it */
    tvp = vfsp->vfs_vnodes;
    if (tvp == vp)
	vfsp->vfs_vnodes = vp->v_vfsnext;
    if (vp->v_vfsnext != NULL)
	vp->v_vfsnext->v_vfsprev = vp->v_vfsprev;
    if (vp->v_vfsprev != NULL)
	vp->v_vfsprev->v_vfsnext = vp->v_vfsnext;

  endgnode:
    /* Free the allocated gnode that was accompanying the vcache's vnode */
    if (vp->v_gnode) {
	osi_FreeSmallSpace(vp->v_gnode);
	vp->v_gnode = 0;
    }

    return 0;
}

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */

afs_suser(void *credp)
{
    int rc;
    char err;

    rc = suser(&err);
    return rc;
}
