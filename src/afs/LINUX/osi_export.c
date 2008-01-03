/*
 * vi:set cin noet sw=4 tw=70:
 * Copyright 2006, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Filesystem export operations for Linux
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include <linux/module.h> /* early to avoid printf->printk mapping */
#include <linux/fs.h>
#ifdef EXPORTFS_H_EXISTS
#include <linux/exportfs.h>
#endif
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_dynroot.h"
#include "h/smp_lock.h"

/* #define OSI_EXPORT_DEBUG */

extern struct dentry_operations afs_dentry_operations;

struct get_name_data {
    char *name;
    struct VenusFid fid;
    int found;
};

/*
 * Linux reserved the following filehandle types:
 * - 0 is always the filesystem root; NFS deals with this for us
 * - 1,2 are reserved by Linux for inode-number-based filehandles
 * - 0xff is reserved by linux
 *
 * We encode filehandles for AFS files using the types defined below.
 * Internally, our "object ID" is a VenusFid; if we get a filehandle
 * with a more-stable cell ID, we'll turn it into a cell number in
 * the decode_fh wrapper.
 */

#define AFSFH_VENUSFID      0xa0 /* cell, volume, vnode, uniq           */
#define AFSFH_CELLFID       0xa1 /* cellhandle, volume, vnode, uniq     */
#define AFSFH_NET_VENUSFID  0xa2 /* net cell, volume, vnode, uniq       */
#define AFSFH_NET_CELLFID   0xa3 /* net cellhandle, volume, vnode, uniq */
#define AFSFH_DYN_RO_CELL   0xd0 /* cellhandle for RO root.cell mount   */
#define AFSFH_DYN_RW_CELL   0xd1 /* cellhandle for RW root.cell mount   */
#define AFSFH_DYN_RO_LINK   0xd2 /* cellhandle for RO root.cell symlink */
#define AFSFH_DYN_RW_LINK   0xd3 /* cellhandle for RW root.cell symlink */
#define AFSFH_DYN_MOUNT     0xd4 /* cellhandle, volume for mount point  */
#define AFSFH_DYN_SYMLINK   0xd5 /* hash of dynroot symlink target */

static int afs_encode_fh(struct dentry *de, __u32 *fh, int *max_len,
			 int connectable)
{
    struct vcache *tvc;
    struct cell *tc;
    int vntype;

    if (!de->d_inode) /* encode a negative dentry?! */
	return 255;
    if (*max_len < 4)  /* not enough space */
	return 255;

    tvc = VTOAFS(de->d_inode);

#ifdef OSI_EXPORT_DEBUG
    printk("afs: encode_fh(0x%08x/%d/%d.%d)\n",
	   tvc->fid.Cell,      tvc->fid.Fid.Volume,
	   tvc->fid.Fid.Vnode, tvc->fid.Fid.Unique);
#endif
    if (afs_IsDynrootAnyFid(&tvc->fid)) {
	vntype = VNUM_TO_VNTYPE(tvc->fid.Fid.Vnode);
	switch (vntype) {
	    case 0:
		/* encode as a normal filehandle */
		break;

	    case VN_TYPE_MOUNT:
		if (*max_len < 5) {
		    return 255;
		}
		/* fall through */

	    case VN_TYPE_CELL:
	    case VN_TYPE_ALIAS:
		AFS_GLOCK();
		tc = afs_GetCellByIndex(VNUM_TO_CIDX(tvc->fid.Fid.Vnode),
					READ_LOCK);
		if (!tc) {
		    AFS_GUNLOCK();
		    return 255;
		}
		memcpy((void *)fh, tc->cellHandle, 16);
		afs_PutCell(tc, READ_LOCK);
		AFS_GUNLOCK();
		if (vntype == VN_TYPE_MOUNT) {
		    fh[4] = htonl(tvc->fid.Fid.Unique);
		    *max_len = 5;
		    return AFSFH_DYN_MOUNT;
		}
		*max_len = 4;
		if (vntype == VN_TYPE_CELL) {
		    return AFSFH_DYN_RO_CELL | VNUM_TO_RW(tvc->fid.Fid.Vnode);
		} else {
		    return AFSFH_DYN_RO_LINK | VNUM_TO_RW(tvc->fid.Fid.Vnode);
		}

	    case VN_TYPE_SYMLINK:
		/* XXX fill in filehandle for dynroot symlink */
		/* XXX return AFSFH_DYN_SYMLINK; */

	    default:
		return 255;
	}
    }

    if (*max_len < 7) {
	/* not big enough for a migratable filehandle */
	/* always encode in network order */
	fh[0] = htonl(tvc->fid.Cell);
	fh[1] = htonl(tvc->fid.Fid.Volume);
	fh[2] = htonl(tvc->fid.Fid.Vnode);
	fh[3] = htonl(tvc->fid.Fid.Unique);
	*max_len = 4;
	return AFSFH_NET_VENUSFID;
    }

    AFS_GLOCK();
    tc = afs_GetCell(tvc->fid.Cell, READ_LOCK);
    if (!tc) {
	AFS_GUNLOCK();
	return 255;
    }
    memcpy((void *)fh, tc->cellHandle, 16);
    afs_PutCell(tc, READ_LOCK);
    AFS_GUNLOCK();
    /* always encode in network order */
    fh[4] = htonl(tvc->fid.Fid.Volume);
    fh[5] = htonl(tvc->fid.Fid.Vnode);
    fh[6] = htonl(tvc->fid.Fid.Unique);

    *max_len = 7;
    return AFSFH_NET_CELLFID;
}

static struct dentry *afs_decode_fh(struct super_block *sb, __u32 *fh,
				    int fh_len, int fh_type,
				    int (*acceptable)(void *, struct dentry *),
				    void *context)
{
    struct VenusFid fid;
    struct cell *tc;
    struct dentry *result;

    switch (fh_type) {
	case AFSFH_VENUSFID:
	    if (fh_len != 4)
		return NULL;
	    fid.Cell       = fh[0];
	    fid.Fid.Volume = fh[1];
	    fid.Fid.Vnode  = fh[2];
	    fid.Fid.Unique = fh[3];
	    break;

	case AFSFH_CELLFID:
	    if (fh_len != 7)
		return NULL;
	    AFS_GLOCK();
	    tc = afs_GetCellByHandle((void *)fh, READ_LOCK);
	    if (!tc) {
		AFS_GUNLOCK();
		return NULL;
	    }
	    fid.Cell       = tc->cellNum;
	    fid.Fid.Volume = fh[4];
	    fid.Fid.Vnode  = fh[5];
	    fid.Fid.Unique = fh[6];
	    afs_PutCell(tc, READ_LOCK);
	    AFS_GUNLOCK();
	    break;

	case AFSFH_NET_VENUSFID:
	    if (fh_len != 4)
		return NULL;
	    fid.Cell       = ntohl(fh[0]);
	    fid.Fid.Volume = ntohl(fh[1]);
	    fid.Fid.Vnode  = ntohl(fh[2]);
	    fid.Fid.Unique = ntohl(fh[3]);
	    break;

	case AFSFH_NET_CELLFID:
	    if (fh_len != 7)
		return NULL;
	    AFS_GLOCK();
	    tc = afs_GetCellByHandle((void *)fh, READ_LOCK);
	    if (!tc) {
		AFS_GUNLOCK();
		return NULL;
	    }
	    fid.Cell       = tc->cellNum;
	    fid.Fid.Volume = ntohl(fh[4]);
	    fid.Fid.Vnode  = ntohl(fh[5]);
	    fid.Fid.Unique = ntohl(fh[6]);
	    afs_PutCell(tc, READ_LOCK);
	    AFS_GUNLOCK();
	    break;

	case AFSFH_DYN_RO_CELL:
	case AFSFH_DYN_RW_CELL:
	    if (fh_len != 4)
		return NULL;
	    AFS_GLOCK();
	    tc = afs_GetCellByHandle((void *)fh, READ_LOCK);
	    if (!tc) {
		AFS_GUNLOCK();
		return NULL;
	    }
	    afs_GetDynrootFid(&fid);
	    fid.Fid.Vnode  = VNUM_FROM_CIDX_RW(tc->cellIndex, fh_type & 1);
	    fid.Fid.Unique = 1;
	    afs_PutCell(tc, READ_LOCK);
	    AFS_GUNLOCK();
	    break;

	case AFSFH_DYN_RO_LINK:
	case AFSFH_DYN_RW_LINK:
	    if (fh_len != 4)
		return NULL;
	    AFS_GLOCK();
	    tc = afs_GetCellByHandle((void *)fh, READ_LOCK);
	    if (!tc) {
		AFS_GUNLOCK();
		return NULL;
	    }
	    afs_GetDynrootFid(&fid);
	    fid.Fid.Vnode  = VNUM_FROM_CAIDX_RW(tc->cellIndex, fh_type & 1);
	    fid.Fid.Unique = 1;
	    afs_PutCell(tc, READ_LOCK);
	    AFS_GUNLOCK();
	    break;

	case AFSFH_DYN_MOUNT:
	    if (fh_len != 5)
		return NULL;
	    AFS_GLOCK();
	    tc = afs_GetCellByHandle((void *)fh, READ_LOCK);
	    if (!tc) {
		AFS_GUNLOCK();
		return NULL;
	    }
	    afs_GetDynrootFid(&fid);
	    fid.Fid.Vnode  = VNUM_FROM_TYPEID(VN_TYPE_MOUNT,
					      tc->cellIndex << 2);
	    fid.Fid.Unique = ntohl(fh[4]);
	    afs_PutCell(tc, READ_LOCK);
	    AFS_GUNLOCK();
	    break;

	case AFSFH_DYN_SYMLINK:
	    /* XXX parse dynroot symlink filehandle */
	    /* break; */

	default:
	    return NULL;
    }

    result = sb->s_export_op->find_exported_dentry(sb, &fid, 0,
						   acceptable, context);
#ifdef OSI_EXPORT_DEBUG
    if (!result) {
	printk("afs: decode_fh(0x%08x/%d/%d.%d): no dentry\n",
	       fid.Cell,      fid.Fid.Volume,
	       fid.Fid.Vnode, fid.Fid.Unique);
    } else if (IS_ERR(result)) {
	printk("afs: decode_fh(0x%08x/%d/%d.%d): error %ld\n",
	       fid.Cell,      fid.Fid.Volume,
	       fid.Fid.Vnode, fid.Fid.Unique, PTR_ERR(result));
    }
#endif
    return result;
}

static int update_dir_parent(struct vrequest *areq, struct vcache *adp)
{
    struct VenusFid tfid;
    struct dcache *tdc;
    afs_size_t dirOffset, dirLen;
    int code;

redo:
    if (!(adp->states & CStatd)) {
	if ((code = afs_VerifyVCache2(adp, areq))) {
#ifdef OSI_EXPORT_DEBUG
	    printk("afs: update_dir_parent(0x%08x/%d/%d.%d): VerifyVCache2: %d\n",
		   adp->fid.Cell,      adp->fid.Fid.Volume,
		   adp->fid.Fid.Vnode, adp->fid.Fid.Unique, code);
#endif
	    return code;
	}
    }

    tdc = afs_GetDCache(adp, (afs_size_t) 0, areq, &dirOffset, &dirLen, 1);
    if (!tdc) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: update_dir_parent(0x%08x/%d/%d.%d): no dcache\n",
	       adp->fid.Cell,      adp->fid.Fid.Volume,
	       adp->fid.Fid.Vnode, adp->fid.Fid.Unique);
#endif
	return EIO;
    }

    /* now we will just call dir package with appropriate inode.
     * Dirs are always fetched in their entirety for now */
    ObtainSharedLock(&adp->lock, 801);
    ObtainReadLock(&tdc->lock);

    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((adp->states & CStatd)
	   && (tdc->dflags & DFFetching)
	   && hsame(adp->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseSharedLock(&adp->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainSharedLock(&adp->lock, 802);
	ObtainReadLock(&tdc->lock);
    }
    if (!(adp->states & CStatd)
	|| !hsame(adp->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseSharedLock(&adp->lock);
	afs_PutDCache(tdc);
#ifdef OSI_EXPORT_DEBUG
	printk("afs: update_dir_parent(0x%08x/%d/%d.%d): dir changed; retrying\n",
	       adp->fid.Cell,      adp->fid.Fid.Volume,
	       adp->fid.Fid.Vnode, adp->fid.Fid.Unique);
#endif
	goto redo;
    }

    /* lookup the name in the appropriate dir, and return a cache entry
     * on the resulting fid */
    code = afs_dir_Lookup(tdc, "..", &tfid.Fid);

    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);

    if (!code) {
	UpgradeSToWLock(&adp->lock, 803);
	adp->parentVnode  = tfid.Fid.Vnode;
	adp->parentUnique = tfid.Fid.Unique;
    }
#ifdef OSI_EXPORT_DEBUG
    if (code) {
	printk("afs: update_dir_parent(0x%08x/%d/%d.%d): afs_dir_Lookup: %d\n",
	       adp->fid.Cell,      adp->fid.Fid.Volume,
	       adp->fid.Fid.Vnode, adp->fid.Fid.Unique, code);
    } else {
	printk("afs: update_dir_parent(0x%08x/%d/%d.%d) => %d.%d\n",
	       adp->fid.Cell,      adp->fid.Fid.Volume,
	       adp->fid.Fid.Vnode, adp->fid.Fid.Unique,
	       adp->parentVnode,   adp->parentUnique);
    }
#endif
    ReleaseSharedLock(&adp->lock);
    return code;
}


static int UnEvalFakeStat(struct vrequest *areq, struct vcache **vcpp)
{
    struct VenusFid tfid;
    struct volume *tvp;
    struct vcache *tvc;
    int code;

    if (!afs_fakestat_enable)
	return 0;

    if (*vcpp == afs_globalVp || vType(*vcpp) != VDIR || (*vcpp)->mvstat != 2)
	return 0;

    /* Figure out what FID to look for */
    tvp = afs_GetVolume(&(*vcpp)->fid, 0, READ_LOCK);
    if (!tvp) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: UnEvalFakeStat(0x%08x/%d/%d.%d): no volume\n",
	       (*vcpp)->fid.Cell,      (*vcpp)->fid.Fid.Volume,
	       (*vcpp)->fid.Fid.Vnode, (*vcpp)->fid.Fid.Unique);
#endif
	return ENOENT;
    }
    tfid = tvp->mtpoint;
    afs_PutVolume(tvp, READ_LOCK);

    tvc = afs_GetVCache(&tfid, areq, NULL, NULL);
    if (!tvc) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: UnEvalFakeStat(0x%08x/%d/%d.%d): GetVCache(0x%08x/%d/%d.%d) failed\n",
	       (*vcpp)->fid.Cell,      (*vcpp)->fid.Fid.Volume,
	       (*vcpp)->fid.Fid.Vnode, (*vcpp)->fid.Fid.Unique,
	       tfid.Cell,          tfid.Fid.Volume,
	       tfid.Fid.Vnode,     tfid.Fid.Unique);
#endif
	return ENOENT;
    }

    if (afs_fakestat_enable == 2) {
	ObtainWriteLock(&tvc->lock, 806);
	code = afs_HandleLink(tvc, areq);
	if (code) {
	    ReleaseWriteLock(&tvc->lock);
	    afs_PutVCache(tvc);
	    return code;
	}
	if (!strchr(tvc->linkData, ':')) {
	    ReleaseWriteLock(&tvc->lock);
	    afs_PutVCache(tvc);
	    return 0;
	}
	ReleaseWriteLock(&tvc->lock);
    }

    afs_PutVCache(*vcpp);
    *vcpp = tvc;
    return 0;
}


/* 
 * Given a FID, obtain or construct a dentry, or return an error.
 * This should be called with the BKL and AFS_GLOCK held.
 */
static struct dentry *get_dentry_from_fid(cred_t *credp, struct VenusFid *afid)
{
    struct vrequest treq;
    struct vcache *vcp;
    struct vattr vattr;
    struct inode *ip;
    struct dentry *dp;
    afs_int32 code;

    code = afs_InitReq(&treq, credp);
    if (code) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_dentry_from_fid(0x%08x/%d/%d.%d): afs_InitReq: %d\n",
	       afid->Cell, afid->Fid.Volume, afid->Fid.Vnode, afid->Fid.Unique,
	       code);
#endif
	return ERR_PTR(-afs_CheckCode(code, &treq, 101));
    }
    vcp = afs_GetVCache(afid, &treq, NULL, NULL);
    if (vcp == NULL) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_dentry_from_fid(0x%08x/%d/%d.%d): no vcache\n",
	       afid->Cell, afid->Fid.Volume, afid->Fid.Vnode, afid->Fid.Unique);
#endif
	return NULL;
    }

    /* 
     * Now, it might be that we just caused a directory vnode to
     * spring into existence, in which case its parent FID is unset.
     * We need to do something about that, but only because we care
     * in our own get_parent(), below -- the common code never looks
     * at parentVnode on directories, except for VIOCGETVCXSTATUS.
     * So, if this fails, we don't really care very much.
     */
    if (vType(vcp) == VDIR && vcp->mvstat != 2 && !vcp->parentVnode)
	update_dir_parent(&treq, vcp);

    /*
     * If this is a volume root directory and fakestat is enabled,
     * we might need to replace the directory by a mount point.
     */
    code = UnEvalFakeStat(&treq, &vcp);
    if (code) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_dentry_from_fid(0x%08x/%d/%d.%d): UnEvalFakeStat: %d\n",
	       afid->Cell, afid->Fid.Volume, afid->Fid.Vnode, afid->Fid.Unique,
	       code);
#endif
	afs_PutVCache(vcp);
	return ERR_PTR(-afs_CheckCode(code, &treq, 101));
    }

    ip = AFSTOV(vcp);
    afs_getattr(vcp, &vattr, credp);
    afs_fill_inode(ip, &vattr);

    /* d_alloc_anon might block, so we shouldn't hold the glock */
    AFS_GUNLOCK();
    dp = d_alloc_anon(ip);
    AFS_GLOCK();

    if (!dp) {
	iput(ip);
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_dentry_from_fid(0x%08x/%d/%d.%d): out of memory\n",
	       afid->Cell, afid->Fid.Volume, afid->Fid.Vnode, afid->Fid.Unique);
#endif
	return ERR_PTR(-ENOMEM);
    }

    dp->d_op = &afs_dentry_operations;
    return dp;
}

static struct dentry *afs_export_get_dentry(struct super_block *sb,
					    void *inump)
{
    struct dentry *dp;
    cred_t *credp;

    credp = crref();
    lock_kernel();
    AFS_GLOCK();

    dp = get_dentry_from_fid(credp, inump);

    AFS_GUNLOCK();
    unlock_kernel();
    crfree(credp);

    return dp;
}


static int get_name_hook(void *hdata, char *name,
			 afs_int32 vnode, afs_int32 unique)
{
    struct get_name_data *data = (struct get_name_data *)hdata;
    int len;

    if (vnode == data->fid.Fid.Vnode && unique == data->fid.Fid.Unique) {
	len = strlen(name);
	if (len > NAME_MAX) len = NAME_MAX;
	memcpy(data->name, name, len);
	data->name[len] = '\0';
	data->found = 1;
    }
    return 0;
}

static int afs_export_get_name(struct dentry *parent, char *name,
			       struct dentry *child)
{
    struct afs_fakestat_state fakestate;
    struct get_name_data data;
    struct vrequest treq;
    struct volume *tvp;
    struct vcache *vcp;
    struct dcache *tdc;
    cred_t *credp;
    afs_size_t dirOffset, dirLen;
    afs_int32 code = 0;

    if (!parent->d_inode) {
#ifdef OSI_EXPORT_DEBUG
	/* can't lookup name in a negative dentry */
	printk("afs: get_name(%s, %s): no parent inode\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       child->d_name.name  ? (char *)child->d_name.name  : "?");
#endif
	return -EIO;
    }
    if (!child->d_inode) {
#ifdef OSI_EXPORT_DEBUG
	/* can't find the FID of negative dentry */
	printk("afs: get_name(%s, %s): no child inode\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       child->d_name.name  ? (char *)child->d_name.name  : "?");
#endif
	return -ENOENT;
    }

    afs_InitFakeStat(&fakestate);

    credp = crref();
    lock_kernel();
    AFS_GLOCK();

    vcp = VTOAFS(child->d_inode);

    /* special case dynamic mount directory */
    if (afs_IsDynrootMount(vcp)) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): this is the dynmount dir\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       vcp->fid.Cell,      vcp->fid.Fid.Volume,
	       vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique);
#endif
	data.fid = vcp->fid;
	if (VTOAFS(parent->d_inode) == afs_globalVp)
	    strcpy(name, AFS_DYNROOT_MOUNTNAME);
	else
	    code = -ENOENT;
	goto done;
    }

    /* Figure out what FID to look for */
    if (vcp->mvstat == 2) { /* volume root */
	tvp = afs_GetVolume(&vcp->fid, 0, READ_LOCK);
	if (!tvp) {
#ifdef OSI_EXPORT_DEBUG
	    printk("afs: get_name(%s, 0x%08x/%d/%d.%d): no volume for root\n",
		   parent->d_name.name ? (char *)parent->d_name.name : "?",
		   vcp->fid.Cell,      vcp->fid.Fid.Volume,
		   vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique);
#endif
	    code = ENOENT;
	    goto done;
	}
	data.fid = tvp->mtpoint;
	afs_PutVolume(tvp, READ_LOCK);
    } else {
	data.fid = vcp->fid;
    }

    vcp = VTOAFS(parent->d_inode);
#ifdef OSI_EXPORT_DEBUG
    printk("afs: get_name(%s, 0x%08x/%d/%d.%d): parent is 0x%08x/%d/%d.%d\n",
	   parent->d_name.name ? (char *)parent->d_name.name : "?",
	   data.fid.Cell,      data.fid.Fid.Volume,
	   data.fid.Fid.Vnode, data.fid.Fid.Unique,
	   vcp->fid.Cell,      vcp->fid.Fid.Volume,
	   vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique);
#endif

    code = afs_InitReq(&treq, credp);
    if (code) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): afs_InitReq: %d\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique, code);
#endif
	goto done;
    }

    /* a dynamic mount point in the dynamic mount directory */
    if (afs_IsDynrootMount(vcp) && afs_IsDynrootAnyFid(&data.fid)
	&& VNUM_TO_VNTYPE(data.fid.Fid.Vnode) == VN_TYPE_MOUNT) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): dynamic mount point\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique);
#endif
	vcp = afs_GetVCache(&data.fid, &treq, NULL, NULL);
	if (vcp) {
	    ObtainReadLock(&vcp->lock);
	    if (strlen(vcp->linkData + 1) <= NAME_MAX)
		strcpy(name, vcp->linkData + 1);
	    else
		code = ENOENT;
	    ReleaseReadLock(&vcp->lock);
	    afs_PutVCache(vcp);
	} else {
#ifdef OSI_EXPORT_DEBUG
	    printk("afs: get_name(%s, 0x%08x/%d/%d.%d): no vcache\n",
		   parent->d_name.name ? (char *)parent->d_name.name : "?",
		   data.fid.Cell,      data.fid.Fid.Volume,
		   data.fid.Fid.Vnode, data.fid.Fid.Unique);
#endif
	    code = ENOENT;
	}
	goto done;
    }

    code = afs_EvalFakeStat(&vcp, &fakestate, &treq);
    if (code)
	goto done;

    if (vcp->fid.Cell != data.fid.Cell ||
	vcp->fid.Fid.Volume != data.fid.Fid.Volume) {
	/* parent is not the expected cell and volume; thus it
	 * cannot possibly contain the fid we are looking for */
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): wrong parent 0x%08x/%d\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique,
	       vcp->fid.Cell,      vcp->fid.Fid.Volume);
#endif
	code = ENOENT;
	goto done;
    }


redo:
    if (!(vcp->states & CStatd)) {
	if ((code = afs_VerifyVCache2(vcp, &treq))) {
#ifdef OSI_EXPORT_DEBUG
	    printk("afs: get_name(%s, 0x%08x/%d/%d.%d): VerifyVCache2(0x%08x/%d/%d.%d): %d\n",
		   parent->d_name.name ? (char *)parent->d_name.name : "?",
		   data.fid.Cell,      data.fid.Fid.Volume,
		   data.fid.Fid.Vnode, data.fid.Fid.Unique,
		   vcp->fid.Cell,      vcp->fid.Fid.Volume,
		   vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique, code);
#endif
	    goto done;
	}
    }

    tdc = afs_GetDCache(vcp, (afs_size_t) 0, &treq, &dirOffset, &dirLen, 1);
    if (!tdc) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): GetDCache(0x%08x/%d/%d.%d): %d\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique,
	       vcp->fid.Cell,      vcp->fid.Fid.Volume,
	       vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique, code);
#endif
	code = EIO;
	goto done;
    }

    ObtainReadLock(&vcp->lock);
    ObtainReadLock(&tdc->lock);

    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((vcp->states & CStatd)
	   && (tdc->dflags & DFFetching)
	   && hsame(vcp->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseReadLock(&vcp->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainReadLock(&vcp->lock);
	ObtainReadLock(&tdc->lock);
    }
    if (!(vcp->states & CStatd)
	|| !hsame(vcp->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseReadLock(&vcp->lock);
	afs_PutDCache(tdc);
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): dir (0x%08x/%d/%d.%d) changed; retrying\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique,
	       vcp->fid.Cell,      vcp->fid.Fid.Volume,
	       vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique);
#endif
	goto redo;
    }

    data.name  = name;
    data.found = 0;
    code = afs_dir_EnumerateDir(tdc, get_name_hook, &data);
    if (!code && !data.found) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): not found\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique);
#endif
	code = ENOENT;
    } else if (code) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d): Enumeratedir(0x%08x/%d/%d.%d): %d\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique,
	       vcp->fid.Cell,      vcp->fid.Fid.Volume,
	       vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique, code);
#endif
    }

    ReleaseReadLock(&tdc->lock);
    ReleaseReadLock(&vcp->lock);
    afs_PutDCache(tdc);

done:
    if (!code) {
	printk("afs: get_name(%s, 0x%08x/%d/%d.%d) => %s\n",
	       parent->d_name.name ? (char *)parent->d_name.name : "?",
	       data.fid.Cell,      data.fid.Fid.Volume,
	       data.fid.Fid.Vnode, data.fid.Fid.Unique, name);
    }
    afs_PutFakeStat(&fakestate);
    AFS_GUNLOCK();
    unlock_kernel();
    crfree(credp);
    code = afs_CheckCode(code, &treq, 102);
    return -code;
}


static struct dentry *afs_export_get_parent(struct dentry *child)
{
    struct VenusFid tfid;
    struct vrequest treq;
    struct cell *tcell;
    struct vcache *vcp;
    struct dentry *dp = NULL;
    cred_t *credp;
    afs_uint32 cellidx;
    int code;

    if (!child->d_inode) {
	/* can't find the parent of a negative dentry */
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_parent(%s): no inode\n",
	       child->d_name.name ? (char *)child->d_name.name : "?");
#endif
	return ERR_PTR(-EIO);
    }

    credp = crref();
    lock_kernel();
    AFS_GLOCK();

    vcp = VTOAFS(child->d_inode);

    if (afs_IsDynrootMount(vcp)) {
	/* the dynmount directory; parent is always the AFS root */
	tfid = afs_globalVp->fid;

    } else if (afs_IsDynrootAny(vcp) &&
	       VNUM_TO_VNTYPE(vcp->fid.Fid.Vnode) == VN_TYPE_MOUNT) {
	/* a mount point in the dynmount directory */
	afs_GetDynrootMountFid(&tfid);

    } else if (vcp->mvstat == 2) {
	/* volume root */
	ObtainReadLock(&vcp->lock);
	if (vcp->mvid && vcp->mvid->Fid.Volume) {
	    tfid = *vcp->mvid;
	    ReleaseReadLock(&vcp->lock);
	} else {
	    ReleaseReadLock(&vcp->lock);
	    tcell = afs_GetCell(vcp->fid.Cell, READ_LOCK);
	    if (!tcell) {
#ifdef OSI_EXPORT_DEBUG
		printk("afs: get_parent(0x%08x/%d/%d.%d): no cell\n",
		       vcp->fid.Cell, vcp->fid.Fid.Volume,
		       vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique);
#endif
		dp = ERR_PTR(-ENOENT);
		goto done;
	    }

	    cellidx = tcell->cellIndex;
	    afs_PutCell(tcell, READ_LOCK);

	    afs_GetDynrootMountFid(&tfid);
	    tfid.Fid.Vnode = VNUM_FROM_TYPEID(VN_TYPE_MOUNT, cellidx << 2);
	    tfid.Fid.Unique = vcp->fid.Fid.Volume;
	}

    } else {
	/* any other vnode */
	if (vType(vcp) == VDIR && !vcp->parentVnode && vcp->mvstat != 1) {
	    code = afs_InitReq(&treq, credp);
	    if (code) {
#ifdef OSI_EXPORT_DEBUG
		printk("afs: get_parent(0x%08x/%d/%d.%d): InitReq: %d\n",
		       vcp->fid.Cell, vcp->fid.Fid.Volume,
		       vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique, code);
#endif
		dp = ERR_PTR(-ENOENT);
		goto done;
	    } else {
		code = update_dir_parent(&treq, vcp);
		if (code) {
#ifdef OSI_EXPORT_DEBUG
		    printk("afs: get_parent(0x%08x/%d/%d.%d): update_dir_parent: %d\n",
			   vcp->fid.Cell, vcp->fid.Fid.Volume,
			   vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique, code);
#endif
		    dp = ERR_PTR(-ENOENT);
		    goto done;
		}
	    }
	}

	tfid.Cell       = vcp->fid.Cell;
	tfid.Fid.Volume = vcp->fid.Fid.Volume;
	tfid.Fid.Vnode  = vcp->parentVnode;
	tfid.Fid.Unique = vcp->parentUnique;
    }

#ifdef OSI_EXPORT_DEBUG
    printk("afs: get_parent(0x%08x/%d/%d.%d): => 0x%08x/%d/%d.%d\n",
	   vcp->fid.Cell, vcp->fid.Fid.Volume,
	   vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique,
	   tfid.Cell, tfid.Fid.Volume, tfid.Fid.Vnode, tfid.Fid.Unique);
#endif

    dp = get_dentry_from_fid(credp, &tfid);
    if (!dp) {
#ifdef OSI_EXPORT_DEBUG
	printk("afs: get_parent(0x%08x/%d/%d.%d): no dentry\n",
	       vcp->fid.Cell, vcp->fid.Fid.Volume,
	       vcp->fid.Fid.Vnode, vcp->fid.Fid.Unique);
#endif
	dp = ERR_PTR(-ENOENT);
    }

done:
    AFS_GUNLOCK();
    unlock_kernel();
    crfree(credp);

    return dp;
}


struct export_operations afs_export_ops = {
    .encode_fh  = afs_encode_fh,
    .decode_fh  = afs_decode_fh,
    .get_dentry = afs_export_get_dentry,
    .get_name   = afs_export_get_name,
    .get_parent = afs_export_get_parent,
};
