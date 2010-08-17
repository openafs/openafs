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
 * Exported UKERNEL support routines.
 */
#ifndef _OSI_PROTO_H_
#define _OSI_PROTO_H_
/* osi_vfsops.c */
extern int afs_statvfs(struct vfs *afsp, struct statvfs *abp);
extern int afs_mount(struct vfs *afsp, char *path, void *data);
extern int afs_unmount(struct vfs *afsp);
extern int afs_root(OSI_VFS_DECL(afsp), struct vnode **avpp);
extern int afs_sync(struct vfs *afsp);
extern int afs_statfs(struct vfs *afsp, struct statfs *abp);
extern int afs_mountroot(void);
extern int afs_swapvp(void);

/* osi_vnodeops.c */
extern int afs_vrdwr(struct usr_vnode *avc, struct usr_uio *uio, int rw,
		     int io, struct usr_ucred *cred);
extern int afs_inactive(struct vcache *avc, afs_ucred_t *acred);

#endif
