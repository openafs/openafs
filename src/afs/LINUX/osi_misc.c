/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux support routines.
 *
 */
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/afs_stats.h"

char *crash_addr = 0; /* Induce an oops by writing here. */

/* Lookup name and return vnode for same. */
int osi_lookupname(char *aname, uio_seg_t seg, int followlink,
	       vnode_t **dirvpp, struct dentry **dpp)
{
#if defined(AFS_LINUX24_ENV)
    struct nameidata nd;
#else
    struct dentry *dp = NULL;
#endif
    int code;

    code = ENOENT;
#if defined(AFS_LINUX24_ENV)
    if (seg == AFS_UIOUSER) {
        code = followlink ?
	    user_path_walk(aname, &nd) : user_path_walk_link(aname, &nd);
    }
    else {
        if (path_init(aname, followlink ? LOOKUP_FOLLOW : 0, &nd))
	    code = path_walk(aname, &nd);
    }

    if (!code) {
	if (nd.dentry->d_inode) {
	    *dpp = nd.dentry;
	    code = 0;
	}
	else
	    path_release(&nd);
    }
#else
    if (seg == AFS_UIOUSER) {
	dp = followlink ? namei(aname) : lnamei(aname);
    }
    else {
	dp = lookup_dentry(aname, NULL, followlink ? 1 : 0);
    }

    if (dp && !IS_ERR(dp)) {
	if (dp->d_inode) {
	    *dpp = dp;
	    code = 0;
	}
	else
	    dput(dp);
    }
#endif
	    
    return code;
}

/* Intialize cache device info and fragment size for disk cache partition. */
int osi_InitCacheInfo(char *aname)
{
    int code;
    struct dentry *dp;
    extern ino_t cacheInode;
    extern struct osi_dev cacheDev;
    extern afs_int32 afs_fsfragsize;
    extern struct super_block *afs_cacheSBp;

    code = osi_lookupname(aname, AFS_UIOSYS, 1, NULL, &dp);
    if (code) return ENOENT;

    cacheInode = dp->d_inode->i_ino;
    cacheDev.dev = dp->d_inode->i_dev;
    afs_fsfragsize = dp->d_inode->i_sb->s_blocksize;
    afs_cacheSBp = dp->d_inode->i_sb;

    dput(dp);

    return 0;
}


#define FOP_READ(F, B, C) (F)->f_op->read(F, B, (size_t)(C), &(F)->f_pos)
#define FOP_WRITE(F, B, C) (F)->f_op->write(F, B, (size_t)(C), &(F)->f_pos)

/* osi_rdwr
 * Seek, then read or write to an open inode. addrp points to data in
 * kernel space.
 */
int osi_rdwr(int rw, struct osi_file *file, caddr_t addrp, size_t asize,
	     size_t *resid)
{
    int code = 0;
    KERNEL_SPACE_DECL;
    struct file *filp = &file->file;
    off_t offset = file->offset;

    /* Seek to the desired position. Return -1 on error. */
    if (filp->f_op->llseek) {
	if (filp->f_op->llseek(filp, (loff_t)offset, 0) != offset)
	    return -1;
    }
    else
	filp->f_pos = offset;

    /* Read/Write the data. */
    TO_USER_SPACE();
    if (rw == UIO_READ)
	code = FOP_READ(filp, addrp, asize);
    else if (rw == UIO_WRITE)
	code = FOP_WRITE(filp, addrp, asize);
    else /* all is well? */
	code = asize;
    TO_KERNEL_SPACE();

    if (code >=0) {
	*resid = asize - code;
	return 0;
    }
    else
	return -1;
}

/* This variant is called from AFS read/write routines and takes a uio
 * struct and, if successful, returns 0.
 */
int osi_file_uio_rdwr(struct osi_file *osifile, uio_t *uiop, int rw)
{
    struct file *filp = &osifile->file;
    struct inode *ip = FILE_INODE(&osifile->file);
    KERNEL_SPACE_DECL;
    int code = 0;
    struct iovec *iov;
    int count;

    if (uiop->uio_seg == AFS_UIOSYS)
	TO_USER_SPACE();

    filp->f_pos = uiop->uio_offset;
    while (code == 0 && uiop->uio_resid > 0 && uiop->uio_iovcnt > 0) {
	iov = uiop->uio_iov;
	count = iov->iov_len;
	if (count == 0) {
	    uiop->uio_iov++;
	    uiop->uio_iovcnt--;
	    continue;
	}
	
	if (rw == UIO_READ)
	    code = FOP_READ(filp, iov->iov_base, count);
	else
	    code = FOP_WRITE(filp, iov->iov_base, count);

	if (code < 0) {
	    code = -code;
	    break;
	}
	
	iov->iov_base += code;
	iov->iov_len -= code;
	uiop->uio_resid -= code;
	uiop->uio_offset += code;
	code = 0;
    }

    if (uiop->uio_seg == AFS_UIOSYS)
	TO_KERNEL_SPACE();

    return code;
}

/* setup_uio 
 * Setup a uio struct.
 */
void setup_uio(uio_t *uiop, struct iovec *iovecp, char *buf,
			     int pos, int count, uio_flag_t flag,
			     uio_seg_t seg)
{
    iovecp->iov_base = buf;
    iovecp->iov_len = count;
    uiop->uio_iov = iovecp;
    uiop->uio_iovcnt = 1;
    uiop->uio_offset = pos;
    uiop->uio_seg = seg;
    uiop->uio_resid = count;
    uiop->uio_flag = flag;
}


/* uiomove
 * UIO_READ : dp -> uio
 * UIO_WRITE : uio -> dp
 */
int uiomove(char *dp, int length, uio_flag_t rw, uio_t *uiop)
{
    int count, n;
    struct iovec *iov;
    int code;

    while (length > 0 && uiop->uio_resid > 0 && uiop->uio_iovcnt > 0) {
	iov = uiop->uio_iov;
	count = iov->iov_len;

	if (!count) {
	    uiop->uio_iov++;
	    uiop->uio_iovcnt--;
	    continue;
	}

	if (count > length)
	    count = length;
	
	switch(uiop->uio_seg) {
	case AFS_UIOSYS:
	    switch(rw) {
	    case UIO_READ:
		memcpy(iov->iov_base, dp, count); break;
	    case UIO_WRITE:
		memcpy(dp, iov->iov_base, count); break;
	    default:
		printf("uiomove: Bad rw = %d\n", rw);
		return -EINVAL;
	    }
	    break;
	case AFS_UIOUSER:
	    switch(rw) {
	    case UIO_READ:
		AFS_COPYOUT(dp, iov->iov_base, count, code); break;
	    case UIO_WRITE:
		AFS_COPYIN(iov->iov_base, dp, count, code); break;
	    default:
		printf("uiomove: Bad rw = %d\n", rw);
		return -EINVAL;
	    }
	    break;
	default:
	    printf("uiomove: Bad seg = %d\n", uiop->uio_seg);
	    return -EINVAL;
	}

	dp += count;
	length -= count;
	iov->iov_base += count;
	iov->iov_len -= count;
	uiop->uio_offset += count;
	uiop->uio_resid -= count;
    }
    return 0;
}

void afs_osi_SetTime(osi_timeval_t *tvp)
{
    extern int (*sys_settimeofdayp)(struct timeval *tv, struct timezone *tz);
    KERNEL_SPACE_DECL;

    AFS_STATCNT(osi_SetTime);

    TO_USER_SPACE();
    (void) (*sys_settimeofdayp)(tvp, NULL);
    TO_KERNEL_SPACE();
}

/* Free all the pages on any of the vnodes in the vlru. Must be done before
 * freeing all memory.
 */
void osi_linux_free_inode_pages(void)
{
    int i;
    struct vcache *tvc;
    struct inode *ip;
    extern struct vcache *afs_vhashT[VCSIZE];

    for (i=0; i<VCSIZE; i++) {
	for(tvc = afs_vhashT[i]; tvc; tvc=tvc->hnext) {
	    ip = (struct inode*)tvc;
#if defined(AFS_LINUX24_ENV)
	    if (ip->i_data.nrpages) {
#else
	    if (ip->i_nrpages) {
#endif
		invalidate_inode_pages(ip);
#if defined(AFS_LINUX24_ENV)
		if (ip->i_data.nrpages) {
#else
		if (ip->i_nrpages) {
#endif
		    printf("Failed to invalidate all pages on inode 0x%x\n",
			   ip);
		}
	    }
	}
    }
}

/* iput an inode. Since we still have a separate inode pool, we don't want
 * to call iput on AFS inodes, since they would then end up on Linux's
 * inode_unsed list.
 */
void osi_iput(struct inode *ip)
{
    extern void afs_delete_inode(struct inode *ip);
    extern struct vfs *afs_globalVFS;

    
#if defined(AFS_LINUX24_ENV)
    if (atomic_read(&ip->i_count) == 0 || atomic_read(&ip->i_count) & 0xffff0000) {
#else
    if (ip->i_count == 0 || ip->i_count & 0xffff0000) {
#endif
	osi_Panic("IPUT Bad refCount %d on inode 0x%x\n",
#if defined(AFS_LINUX24_ENV)
		  atomic_read(&ip->i_count), ip);
#else
		  ip->i_count, ip);
#endif
    }
    if (afs_globalVFS && afs_globalVFS == ip->i_sb ) {
#if defined(AFS_LINUX24_ENV)
	atomic_dec(&ip->i_count);
	if (!atomic_read(&ip->i_count))
#else
	ip->i_count --;
	if (!ip->i_count)
#endif
	    afs_delete_inode(ip);
    }
    else { 
	iput(ip);
    }
}

/* check_bad_parent() : Checks if this dentry's vcache is a root vcache
 * that has its mvid (parent dir's fid) pointer set to the wrong directory
 * due to being mounted in multiple points at once. If so, check_bad_parent()
 * calls afs_lookup() to correct the vcache's mvid, as well as the volume's
 * dotdotfid and mtpoint fid members.
 * Parameters:
 *  dp - dentry to be checked.
 * Return Values:
 *  None.
 * Sideeffects:
 *   This dentry's vcache's mvid will be set to the correct parent directory's
 *   fid.
 *   This root vnode's volume will have its dotdotfid and mtpoint fids set
 *    to the correct parent and mountpoint fids.
 */

void check_bad_parent(struct dentry *dp)
{
  cred_t *credp;
  struct vcache *vcp = (struct vcache*)dp->d_inode, *avc = NULL;
  struct vcache *pvc = (struct vcache *)dp->d_parent->d_inode;

  if (vcp->mvid->Fid.Volume != pvc->fid.Fid.Volume) { /* bad parent */
    credp = crref();


    /* force a lookup, so vcp->mvid is fixed up */
    afs_lookup(pvc, dp->d_name.name, &avc, credp);
    if (!avc || vcp != avc) {    /* bad, very bad.. */
      afs_Trace4(afs_iclSetp, CM_TRACE_TMP_1S3L, ICL_TYPE_STRING,
		 "afs_linux_revalidate : bad pointer returned from afs_lookup origvc newvc dentry",
		 ICL_TYPE_POINTER, vcp,
		 ICL_TYPE_POINTER, avc,
		 ICL_TYPE_POINTER, dp);
    }
    
  } /* if bad parent */
 
  return;
}
