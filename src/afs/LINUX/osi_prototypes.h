/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Exported linux support routines.
 */
#ifndef _OSI_PROTO_H_
#define _OSI_PROTO_H_

/* osi_alloc.c */
extern void *osi_linux_alloc(unsigned int size, int drop_glock);
extern void osi_linux_free(void *addr);
extern void osi_linux_free_afs_memory(void);
/* Debugging aid */
extern void osi_linux_verify_alloced_memory(void);

/* osi_cred.c */
extern cred_t *crget(void);
extern void crfree(cred_t *cr);
extern cred_t *crdup(cred_t *cr);
extern cred_t *crref(void);
extern void crset(cred_t *cr);


/* osi_misc.c */
extern int osi_lookupname(char *aname, uio_seg_t seg, int followlink,
			  vnode_t **dirvpp, struct dentry **dpp);
extern int osi_InitCacheInfo(char *aname);
extern int osi_rdwr(int rw, struct osi_file *file, caddr_t addrp, size_t asize,
		    size_t *resid);
extern void inline  setup_uio(uio_t *uiop, struct iovec *iovecp, char *buf,
			     afs_offs_t pos, int count, uio_flag_t flag,
			     uio_seg_t seg);
extern int osi_file_uio_rdwr(struct osi_file *osifile, uio_t *uiop, int rw);
extern void afs_osi_SetTime(osi_timeval_t *tvp);
extern void osi_linux_free_inode_pages(void);
extern void check_bad_parent(struct dentry *dp);

/* osi_vm.c */
extern int osi_VM_FlushVCache(struct vcache *avc, int *slept);
extern void osi_VM_TryToSmush(struct vcache *avc, struct AFS_UCRED *acred,
			      int sync);
extern void osi_VM_FSyncInval(struct vcache *avc);
extern void osi_VM_StoreAllSegments(struct vcache *avc);
extern void osi_VM_FlushPages(struct vcache *avc, struct AFS_UCRED *credp);
extern void osi_VM_Truncate(struct vcache *avc, int alen,
			    struct AFS_UCRED *acred);

/* osi_vfsops.c */
extern void set_inode_cache(struct inode *ip, struct vattr *vp);


#endif /* _OSI_PROTO_H_ */
