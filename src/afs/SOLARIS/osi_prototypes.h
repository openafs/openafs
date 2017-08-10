/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PROTOTYPES_H_
#define _OSI_PROTOTYPES_H_

/* osi_vnodeops.c */

#ifdef AFS_SUN511_ENV
int afs_putpage(struct vnode *vp, offset_t off, size_t len, int flags,
		afs_ucred_t *cred, caller_context_t *ct);
#else
int afs_putpage(struct vnode *vp, offset_t off, u_int len, int flags,
		afs_ucred_t *cred);
#endif

int afs_putapage(struct vnode *vp, struct page *pages, u_offset_t * offp,
		 size_t * lenp, int flags, afs_ucred_t *credp);

#endif /* _OSI_PROTOTYPES_H_ */
