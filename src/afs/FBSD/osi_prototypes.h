/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
/*
 * osi_prototypes.h
 *
 * Exported support routines.
 */
#ifndef _OSI_PROTO_H_
#define _OSI_PROTO_H_

/* osi_file.c */
extern afs_rwlock_t afs_xosi;

/* osi_misc.c */
extern int osi_lookupname(char *aname, enum uio_seg seg, int followlink,
			  struct vnode **vpp);
extern void *osi_fbsd_alloc(size_t size, int dropglobal);
extern void osi_fbsd_free(void *p);

/* osi_vfsops.c */
int afs_init(struct vfsconf *vfc);
int afs_uninit(struct vfsconf *vfc);
#ifdef AFS_FBSD50_ENV
extern int afs_statfs(struct mount *mp, struct statfs *abp, struct thread *th);
#else
extern int afs_statfs(struct mount *mp, struct statfs *abp, struct proc *p);
#endif

#endif /* _OSI_PROTO_H_ */
