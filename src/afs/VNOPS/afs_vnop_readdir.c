/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_vnop_readdir.c - afs_readdir and bulk stat
 *
 * Implements:
 * BlobScan
 * afs_readdir_move
 * afs_bulkstat_send
 * afs_readdir/afs_readdir2(HP)
 * afs_readdir1 - HP and DUX NFS versions
 * 
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"


#if	defined(AFS_HPUX1122_ENV)
#define DIRPAD 7
#else
#define DIRPAD 3
#endif
/**
 * A few definitions. This is until we have a proper header file
 * which ahs prototypes for all functions
 */

extern struct DirEntry *afs_dir_GetBlob();
/*
 * AFS readdir vnodeop and bulk stat support.
 */

/* Saber C hates negative inode #s.  We're not going to talk about software
 * that could fail if it sees a negative inode #.
 */
#define FIXUPSTUPIDINODE(a)	((a) &= 0x7fffffff)

/* BlobScan is supposed to ensure that the blob reference refers to a valid
    directory entry.  It consults the allocation map in the page header
    to determine whether a blob is actually in use or not.

    More formally, BlobScan is supposed to return a new blob number which is just like
    the input parameter, only it is advanced over header or free blobs.
    
    Note that BlobScan switches pages if necessary.  BlobScan may return
    either 0 or an out-of-range blob number for end of file.

    BlobScan is used by the Linux port in a separate file, so it should not
    become static.
*/
int
BlobScan(struct dcache * afile, afs_int32 ablob)
{
    register afs_int32 relativeBlob;
    afs_int32 pageBlob;
    register struct PageHeader *tpe;
    register afs_int32 i;

    AFS_STATCNT(BlobScan);
    /* advance ablob over free and header blobs */
    while (1) {
	pageBlob = ablob & ~(EPP - 1);	/* base blob in same page */
	tpe = (struct PageHeader *)afs_dir_GetBlob(afile, pageBlob);
	if (!tpe)
	    return 0;		/* we've past the end */
	relativeBlob = ablob - pageBlob;	/* relative to page's first blob */
	/* first watch for headers */
	if (pageBlob == 0) {	/* first dir page has extra-big header */
	    /* first page */
	    if (relativeBlob < DHE + 1)
		relativeBlob = DHE + 1;
	} else {		/* others have one header blob */
	    if (relativeBlob == 0)
		relativeBlob = 1;
	}
	/* make sure blob is allocated */
	for (i = relativeBlob; i < EPP; i++) {
	    if (tpe->freebitmap[i >> 3] & (1 << (i & 7)))
		break;
	}
	/* now relativeBlob is the page-relative first allocated blob,
	 * or EPP (if there are none in this page). */
	DRelease((struct buffer *)tpe, 0);
	if (i != EPP)
	    return i + pageBlob;
	ablob = pageBlob + EPP;	/* go around again */
    }
    /* never get here */
}

#if !defined(AFS_LINUX20_ENV)
/* Changes to afs_readdir which affect dcache or vcache handling or use of
 * bulk stat data should also be reflected in the Linux specific verison of
 * the readdir routine.
 */

/*
 * The kernel don't like it so much to have large stuff on the stack.
 * Here we use a watered down version of the direct struct, since
 * its not too bright to double copy the strings anyway.
*/
#if !defined(UKERNEL)
#if defined(AFS_SGI_ENV)
/* Long form for 64 bit apps and kernel requests. */
struct min_dirent {		/* miniature dirent structure */
    /* If struct dirent changes, this must too */
    ino_t d_fileno;		/* This is 32 bits for 3.5, 64 for 6.2+ */
    off64_t d_off;
    u_short d_reclen;
};
/* Short form for 32 bit apps. */
struct irix5_min_dirent {	/* miniature dirent structure */
    /* If struct dirent changes, this must too */
    afs_uint32 d_fileno;
    afs_int32 d_off;
    u_short d_reclen;
};
#ifdef AFS_SGI62_ENV
#define AFS_DIRENT32BASESIZE IRIX5_DIRENTBASESIZE
#define AFS_DIRENT64BASESIZE DIRENT64BASESIZE
#else
#define AFS_DIRENT32BASESIZE IRIX5_DIRENTBASESIZE
#define AFS_DIRENT64BASESIZE DIRENTBASESIZE
#endif /* AFS_SGI62_ENV */
#else
struct min_direct {		/* miniature direct structure */
    /* If struct direct changes, this must too */
#if defined(AFS_DARWIN80_ENV)
    ino_t d_fileno;
    u_short d_reclen;
    u_char d_type;
    u_char d_namlen;
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    afs_uint32 d_fileno;
    u_short d_reclen;
    u_char d_type;
    u_char d_namlen;
#elif defined(AFS_SUN5_ENV)
    afs_uint32 d_fileno;
    afs_int32 d_off;
    u_short d_reclen;
#else
#if defined(AFS_AIX32_ENV)
    afs_int32 d_off;
#elif defined(AFS_HPUX100_ENV)
    unsigned long long d_off;
#endif
    afs_uint32 d_fileno;
    u_short d_reclen;
    u_short d_namlen;
#endif
};
#endif /* AFS_SGI_ENV */

#if	defined(AFS_HPUX_ENV) || defined(AFS_OSF_ENV)
struct minnfs_direct {
    afs_int32 d_off;		/* XXX */
    afs_uint32 d_fileno;
    u_short d_reclen;
    u_short d_namlen;
};
#define NDIRSIZ_LEN(len)   ((sizeof (struct dirent)+4 - (MAXNAMLEN+1)) + (((len)+1 + DIRPAD) &~ DIRPAD))
#endif
#endif /* !defined(UKERNEL) */


/*
 *------------------------------------------------------------------------------
 *
 * Keep a stack of about 256 fids for the bulk stat call.
 * Fill it during the readdir_move.  Later empty it...
 */

#define	READDIR_STASH	AFSCBMAX
struct AFSFid afs_readdir_stash[READDIR_STASH];
int afs_rd_stash_i = 0;

/*
 *------------------------------------------------------------------------------
 *
 * afs_readdir_move.
 *	mainly a kind of macro... makes getting the struct direct
 *	out to the user space easy... could take more parameters,
 *	but now just takes what it needs.
 *
 *
*/

#if	defined(AFS_HPUX100_ENV)
#define DIRSIZ_LEN(len) \
    ((sizeof (struct __dirent) - (_MAXNAMLEN+1)) + (((len)+1 + DIRPAD) &~ DIRPAD))
#else
#if	defined(AFS_SUN56_ENV)
#define DIRSIZ_LEN(len) ((18 + (len) + 1 + 7) & ~7 )
#else
#ifdef	AFS_SUN5_ENV
#define DIRSIZ_LEN(len)	((10 + (len) + 1 + (NBPW-1)) & ~(NBPW-1))
#else
#ifdef	AFS_DIRENT
#define DIRSIZ_LEN(len) \
    ((sizeof (struct dirent) - (MAXNAMLEN+1)) + (((len)+1 + 3) &~ 3))
#else
#if defined(AFS_SGI_ENV)
#ifndef AFS_SGI53_ENV
/* SGI 5.3 and later use 32/64 bit versions of directory size. */
#define DIRSIZ_LEN(len)		DIRENTSIZE(len)
#endif
#else /* AFS_SGI_ENV */
#define DIRSIZ_LEN(len) \
    ((sizeof (struct direct) - (MAXNAMLEN+1)) + (((len)+1 + 3) &~ 3))
#endif /* AFS_SGI_ENV */
#endif /* AFS_DIRENT */
#endif /* AFS_SUN5_ENV */
#endif /* AFS_SUN56_ENV */
#endif /* AFS_HPUX100_ENV */

#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
int
afs_readdir_type(avc, ade)
     struct DirEntry *ade;
     struct vcache *avc;
{
    struct VenusFid tfid;
    struct vcache *tvc;
    int vtype;
    tfid.Cell = avc->fid.Cell;
    tfid.Fid.Volume = avc->fid.Fid.Volume;
    tfid.Fid.Vnode = ntohl(ade->fid.vnode);
    tfid.Fid.Unique = ntohl(ade->fid.vunique);
    if ((avc->states & CForeign) == 0 && (ntohl(ade->fid.vnode) & 1)) {
	return DT_DIR;
    } else if ((tvc = afs_FindVCache(&tfid, 0, 0))) {
	if (tvc->mvstat) {
	    afs_PutVCache(tvc);
	    return DT_DIR;
	} else if (((tvc->states) & (CStatd | CTruth))) {
	    /* CTruth will be set if the object has
	     *ever* been statd */
	    vtype = vType(tvc);
	    afs_PutVCache(tvc);
	    if (vtype == VDIR)
		return DT_DIR;
	    else if (vtype == VREG)
		return DT_REG;
	    /* Don't do this until we're sure it can't be a mtpt */
	    /* else if (vtype == VLNK)
	     * type=DT_LNK; */
	    /* what other types does AFS support? */
	} else
	    afs_PutVCache(tvc);
    }
    return DT_UNKNOWN;
}
#endif

#ifdef AFS_AIX41_ENV
#define AFS_MOVE_LOCK()   AFS_GLOCK()
#define AFS_MOVE_UNLOCK() AFS_GUNLOCK()
#else
#define AFS_MOVE_LOCK()
#define AFS_MOVE_UNLOCK()
#endif
char bufofzeros[64];		/* gotta fill with something */

int
afs_readdir_move(de, vc, auio, slen, rlen, off)
     struct DirEntry *de;
     struct vcache *vc;
     struct uio *auio;
     int slen;
#ifdef AFS_SGI65_ENV
     ssize_t rlen;
#else
     int rlen;
#endif
     afs_size_t off;
{
    int code = 0;
    struct volume *tvp;
    afs_uint32 Volume = vc->fid.Fid.Volume;
    afs_uint32 Vnode  = de->fid.vnode;
#if	defined(AFS_SUN56_ENV)
    struct dirent64 *direntp;
#else
#if  defined(AFS_SUN5_ENV) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
    struct dirent *direntp;
#endif
#endif /* AFS_SUN56_ENV */
#ifndef	AFS_SGI53_ENV
    struct min_direct sdirEntry;
#endif /* AFS_SGI53_ENV */

    AFS_STATCNT(afs_readdir_move);

#define READDIR_CORRECT_INUMS
#ifdef READDIR_CORRECT_INUMS
    if (de->name[0] == '.' && !de->name[1]) {
	/* This is the '.' entry; if we are a volume root, we need to
	 * ignore the directory and use the inum for the mount point.
	 */
	if (!FidCmp(&afs_rootFid, &vc->fid)) {
	    Volume = 0;
	    Vnode  = 2;
	} else if (vc->mvstat == 2) {
	    tvp = afs_GetVolume(&vc->fid, 0, READ_LOCK);
	    if (tvp) {
		Volume = tvp->mtpoint.Fid.Volume;
		Vnode  = tvp->mtpoint.Fid.Vnode;
		afs_PutVolume(tvp, READ_LOCK);
	    }
	}
    }
    else if (de->name[0] == '.' && de->name[1] == '.' && !de->name[2]) {
	/* This is the '..' entry.  Getting this right is very tricky,
	 * because we might be a volume root (so our parent is in a
	 * different volume), or our parent might be a volume root
	 * (so we actually want the mount point) or BOTH! */
	if (!FidCmp(&afs_rootFid, &vc->fid)) {
	    /* We are the root of the AFS root, and thus our own parent */
	    Volume = 0;
	    Vnode  = 2;
	} else if (vc->mvstat == 2) {
	    /* We are a volume root, which means our parent is in another
	     * volume.  Luckily, we should have his fid cached... */
	    if (vc->mvid) {
		if (!FidCmp(&afs_rootFid, vc->mvid)) {
		    /* Parent directory is the root of the AFS root */
		    Volume = 0;
		    Vnode  = 2;
		} else if (vc->mvid->Fid.Vnode == 1
			   && vc->mvid->Fid.Unique == 1) {
		    /* XXX The above test is evil and probably breaks DFS */
		    /* Parent directory is the target of a mount point */
		    tvp = afs_GetVolume(vc->mvid, 0, READ_LOCK);
		    if (tvp) {
			Volume = tvp->mtpoint.Fid.Volume;
			Vnode  = tvp->mtpoint.Fid.Vnode;
			afs_PutVolume(tvp, READ_LOCK);
		    }
		} else {
		    /* Parent directory is not a volume root */
		    Volume = vc->mvid->Fid.Volume;
		    Vnode  = vc->mvid->Fid.Vnode;
		}
	    }
	} else if (de->fid.vnode == 1 && de->fid.vunique == 1) {
	    /* XXX The above test is evil and probably breaks DFS */
	    /* Parent directory is a volume root; use the right inum */
	    tvp = afs_GetVolume(&vc->fid, 0, READ_LOCK);
	    if (tvp) {
		if (tvp->cell == afs_rootFid.Cell
		    && tvp->volume == afs_rootFid.Fid.Volume) {
		    /* Parent directory is the root of the AFS root */
		    Volume = 0;
		    Vnode  = 2;
		} else {
		    /* Parent directory is the target of a mount point */
		    Volume = tvp->mtpoint.Fid.Volume;
		    Vnode  = tvp->mtpoint.Fid.Vnode;
		}
		afs_PutVolume(tvp, READ_LOCK);
	    }
	}
    }
#endif

#ifdef	AFS_SGI53_ENV
    {
	afs_int32 use64BitDirent;

#ifdef AFS_SGI61_ENV
#ifdef AFS_SGI62_ENV
	use64BitDirent =
	    ABI_IS(ABI_IRIX5_64, GETDENTS_ABI(OSI_GET_CURRENT_ABI(), auio));
#else
	use64BitDirent =
	    (auio->uio_segflg !=
	     UIO_USERSPACE) ? ABI_IRIX5_64 : (ABI_IS(ABI_IRIX5_64 |
						     ABI_IRIX5_N32,
						     u.u_procp->p_abi));
#endif
#else /* AFS_SGI61_ENV */
	use64BitDirent =
	    (auio->uio_segflg !=
	     UIO_USERSPACE) ? ABI_IRIX5_64 : (ABI_IS(ABI_IRIX5_64,
						     u.u_procp->p_abi));
#endif /* AFS_SGI61_ENV */

	if (use64BitDirent) {
	    struct min_dirent sdirEntry;
	    sdirEntry.d_fileno = (Volume << 16) + ntohl(Vnode);
	    FIXUPSTUPIDINODE(sdirEntry.d_fileno);
	    sdirEntry.d_reclen = rlen;
	    sdirEntry.d_off = (off_t) off;
	    AFS_UIOMOVE(&sdirEntry, AFS_DIRENT64BASESIZE, UIO_READ, auio,
			code);
	    if (code == 0)
		AFS_UIOMOVE(de->name, slen - 1, UIO_READ, auio, code);
	    if (code == 0)
		AFS_UIOMOVE(bufofzeros,
			    DIRENTSIZE(slen) - (AFS_DIRENT64BASESIZE + slen -
						1), UIO_READ, auio, code);
	    if (DIRENTSIZE(slen) < rlen) {
		while (DIRENTSIZE(slen) < rlen) {
		    int minLen = rlen - DIRENTSIZE(slen);
		    if (minLen > sizeof(bufofzeros))
			minLen = sizeof(bufofzeros);
		    AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
		    rlen -= minLen;
		}
	    }
	} else {
	    struct irix5_min_dirent sdirEntry;
	    sdirEntry.d_fileno = (Volume << 16) + ntohl(Vnode);
	    FIXUPSTUPIDINODE(sdirEntry.d_fileno);
	    sdirEntry.d_reclen = rlen;
	    sdirEntry.d_off = (afs_int32) off;
	    AFS_UIOMOVE(&sdirEntry, AFS_DIRENT32BASESIZE, UIO_READ, auio,
			code);
	    if (code == 0)
		AFS_UIOMOVE(de->name, slen - 1, UIO_READ, auio, code);
	    if (code == 0)
		AFS_UIOMOVE(bufofzeros,
			    IRIX5_DIRENTSIZE(slen) - (AFS_DIRENT32BASESIZE +
						      slen - 1), UIO_READ,
			    auio, code);
	    if (IRIX5_DIRENTSIZE(slen) < rlen) {
		while (IRIX5_DIRENTSIZE(slen) < rlen) {
		    int minLen = rlen - IRIX5_DIRENTSIZE(slen);
		    if (minLen > sizeof(bufofzeros))
			minLen = sizeof(bufofzeros);
		    AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
		    rlen -= minLen;
		}
	    }
	}
    }
#else /* AFS_SGI53_ENV */
#if  defined(AFS_SUN5_ENV) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
#if	defined(AFS_SUN56_ENV)
    direntp = (struct dirent64 *)osi_AllocLargeSpace(AFS_LRALLOCSIZ);
#else
    direntp = (struct dirent *)osi_AllocLargeSpace(AFS_LRALLOCSIZ);
#endif
    direntp->d_ino =  (Volume << 16) + ntohl(Vnode);
    FIXUPSTUPIDINODE(direntp->d_ino);
#if defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)
    direntp->d_offset = off;
    direntp->d_namlen = slen;
#else
    direntp->d_off = off;
#endif
    direntp->d_reclen = rlen;
    strcpy(direntp->d_name, de->name);
    AFS_UIOMOVE((caddr_t) direntp, rlen, UIO_READ, auio, code);
    osi_FreeLargeSpace((char *)direntp);
#else /* AFS_SUN5_ENV */
    /* Note the odd mechanism for building the inode number */
    sdirEntry.d_fileno = (Volume << 16) + ntohl(Vnode);
    FIXUPSTUPIDINODE(sdirEntry.d_fileno);
    sdirEntry.d_reclen = rlen;
#if !defined(AFS_SGI_ENV)
    sdirEntry.d_namlen = slen;
#endif
#if defined(AFS_AIX32_ENV) || defined(AFS_SGI_ENV)
    sdirEntry.d_off = off;
#endif
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    sdirEntry.d_type = afs_readdir_type(vc, de);
#endif

#if defined(AFS_SGI_ENV)
    AFS_UIOMOVE(&sdirEntry, DIRENTBASESIZE, UIO_READ, auio, code);
    if (code == 0)
	AFS_UIOMOVE(de->name, slen - 1, UIO_READ, auio, code);
    if (code == 0)
	AFS_UIOMOVE(bufofzeros,
		    DIRSIZ_LEN(slen) - (DIRENTBASESIZE + slen - 1), UIO_READ,
		    auio, code);
#else /* AFS_SGI_ENV */
    AFS_MOVE_UNLOCK();
    AFS_UIOMOVE((char *)&sdirEntry, sizeof(sdirEntry), UIO_READ, auio, code);

    if (code == 0) {
	AFS_UIOMOVE(de->name, slen, UIO_READ, auio, code);
    }

    /* pad out the remaining characters with zeros */
    if (code == 0) {
	AFS_UIOMOVE(bufofzeros, ((slen + 1 + DIRPAD) & ~DIRPAD) - slen,
		    UIO_READ, auio, code);
    }
    AFS_MOVE_LOCK();
#endif /* AFS_SGI_ENV */

    /* pad out the difference between rlen and slen... */
    if (DIRSIZ_LEN(slen) < rlen) {
	AFS_MOVE_UNLOCK();
	while (DIRSIZ_LEN(slen) < rlen) {
	    int minLen = rlen - DIRSIZ_LEN(slen);
	    if (minLen > sizeof(bufofzeros))
		minLen = sizeof(bufofzeros);
	    AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
	    rlen -= minLen;
	}
	AFS_MOVE_LOCK();
    }
#endif /* AFS_SUN5_ENV */
#endif /* AFS_SGI53_ENV */
    return (code);
}


/*
 *------------------------------------------------------------------------------
 *
 * Read directory entries.
 * There are some weird things to look out for here.  The uio_offset
 * field is either 0 or it is the offset returned from a previous
 * readdir.  It is an opaque value used by the server to find the
 * correct directory block to read.  The byte count must be at least
 * vtoblksz(vp) bytes.  The count field is the number of blocks to
 * read on the server.  This is advisory only, the server may return
 * only one block's worth of entries.  Entries may be compressed on
 * the server.
 *
 * This routine encodes knowledge of Vice dirs.
 */

void
afs_bulkstat_send(avc, req)
     struct vcache *avc;
     struct vrequest *req;
{
    afs_rd_stash_i = 0;
}

/*
 * Here is the bad, bad, really bad news.
 * It has to do with 'offset' (seek locations).
*/

int
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_readdir(OSI_VC_ARG(avc), auio, acred, eofp)
     int *eofp;
#else
#if defined(AFS_HPUX100_ENV)
afs_readdir2(OSI_VC_ARG(avc), auio, acred)
#else
afs_readdir(OSI_VC_ARG(avc), auio, acred)
#endif
#endif
     OSI_VC_DECL(avc);
     struct uio *auio;
     struct AFS_UCRED *acred;
{
    struct vrequest treq;
    register struct dcache *tdc;
    afs_size_t origOffset, tlen;
    afs_int32 len;
    int code = 0;
    struct DirEntry *ode = 0, *nde = 0;
    int o_slen = 0, n_slen = 0;
    afs_uint32 us;
    struct afs_fakestat_state fakestate;
#if defined(AFS_SGI53_ENV)
    afs_int32 use64BitDirent, dirsiz;
#endif /* defined(AFS_SGI53_ENV) */
#ifndef	AFS_HPUX_ENV
    OSI_VC_CONVERT(avc);
#else
    /*
     * XXX All the hacks for alloced sdirEntry and inlining of afs_readdir_move instead of calling
     * it is necessary for hpux due to stack problems that seem to occur when coming thru the nfs
     * translator side XXX
     */
    struct min_direct *sdirEntry =
	(struct min_direct *)osi_AllocSmallSpace(sizeof(struct min_direct));
    afs_int32 rlen;
#endif

    /* opaque value is pointer into a vice dir; use bit map to decide
     * if the entries are in use.  Always assumed to be valid.  0 is
     * special, means start of a new dir.  Int32 inode, followed by
     * short reclen and short namelen.  Namelen does not include
     * the null byte.  Followed by null-terminated string.
     */
    AFS_STATCNT(afs_readdir);

#if defined(AFS_SGI53_ENV)
#ifdef AFS_SGI61_ENV
#ifdef AFS_SGI62_ENV
    use64BitDirent =
	ABI_IS(ABI_IRIX5_64, GETDENTS_ABI(OSI_GET_CURRENT_ABI(), auio));
#else
    use64BitDirent =
	(auio->uio_segflg !=
	 UIO_USERSPACE) ? ABI_IRIX5_64 : (ABI_IS(ABI_IRIX5_64 | ABI_IRIX5_N32,
						 u.u_procp->p_abi));
#endif /* AFS_SGI62_ENV */
#else /* AFS_SGI61_ENV */
    use64BitDirent =
	(auio->uio_segflg !=
	 UIO_USERSPACE) ? ABI_IRIX5_64 : (ABI_IS(ABI_IRIX5_64,
						 u.u_procp->p_abi));
#endif /* AFS_SGI61_ENV */
#endif /* defined(AFS_SGI53_ENV) */

#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    /* Not really used by the callee so we ignore it for now */
    if (eofp)
	*eofp = 0;
#endif
    if (AfsLargeFileUio(auio)	/* file is large than 2 GB */
	||AfsLargeFileSize(AFS_UIO_OFFSET(auio), AFS_UIO_RESID(auio)))
	return EFBIG;

    if ((code = afs_InitReq(&treq, acred))) {
#ifdef	AFS_HPUX_ENV
	osi_FreeSmallSpace((char *)sdirEntry);
#endif
	return code;
    }
    /* update the cache entry */
    afs_InitFakeStat(&fakestate);
    code = afs_EvalFakeStat(&avc, &fakestate, &treq);
    if (code)
	goto done;
  tagain:
    code = afs_VerifyVCache(avc, &treq);
    if (code)
	goto done;
    /* get a reference to the entire directory */
    tdc = afs_GetDCache(avc, (afs_size_t) 0, &treq, &origOffset, &tlen, 1);
    len = tlen;
    if (!tdc) {
	code = ENOENT;
	goto done;
    }
    ObtainReadLock(&avc->lock);
    ObtainReadLock(&tdc->lock);

    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((avc->states & CStatd)
	   && (tdc->dflags & DFFetching)
	   && hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAIT, ICL_TYPE_STRING,
		   __FILE__, ICL_TYPE_INT32, __LINE__, ICL_TYPE_POINTER, tdc,
		   ICL_TYPE_INT32, tdc->dflags);
	ReleaseReadLock(&tdc->lock);
	ReleaseReadLock(&avc->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainReadLock(&avc->lock);
	ObtainReadLock(&tdc->lock);
    }
    if (!(avc->states & CStatd)
	|| !hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseReadLock(&avc->lock);
	afs_PutDCache(tdc);
	goto tagain;
    }

    /*
     *  iterator for the directory reads.  Takes the AFS DirEntry
     *  structure and slams them into UFS direct structures.
     *  uses afs_readdir_move to get the struct to the user space.
     *
     *  The routine works by looking ahead one AFS directory entry.
     *  That's because the AFS entry we are currenly working with
     *  may not fit into the buffer the user has provided.  If it
     *  doesn't we have to change the size of the LAST AFS directory
     *  entry, so that it will FIT perfectly into the block the
     *  user has provided.
     *  
     *  The 'forward looking' of the code makes it a bit tough to read.
     *  Remember we need to get an entry, see if it it fits, then
     *  set it up as the LAST entry, and find the next one.
     *
     *  Tough to take: We give out an EINVAL if we don't have enough
     *  space in the buffer, and at the same time, don't have an entry
     *  to put into the buffer. This CAN happen if the first AFS entry
     *  we get can't fit into the 512 character buffer provided.  Seems
     *  it ought not happen... 
     *
     *  Assumption: don't need to use anything but one dc entry:
     *  this means the directory ought not be greater than 64k.
     */
    len = 0;
#ifdef AFS_HPUX_ENV
    auio->uio_fpflags = 0;
#endif
    while (code == 0) {
	origOffset = AFS_UIO_OFFSET(auio);
	/* scan for the next interesting entry scan for in-use blob otherwise up point at
	 * this blob note that ode, if non-zero, also represents a held dir page */
	if (!(us = BlobScan(tdc, (origOffset >> 5)))
	    || !(nde = (struct DirEntry *)afs_dir_GetBlob(tdc, us))) {
	    /* failed to setup nde, return what we've got, and release ode */
	    if (len) {
		/* something to hand over. */
#ifdef	AFS_HPUX_ENV
		sdirEntry->d_fileno =
		    (avc->fid.Fid.Volume << 16) + ntohl(ode->fid.vnode);
		FIXUPSTUPIDINODE(sdirEntry->d_fileno);
		sdirEntry->d_reclen = rlen = AFS_UIO_RESID(auio);
		sdirEntry->d_namlen = o_slen;
#if defined(AFS_SUN5_ENV) || defined(AFS_AIX32_ENV) || defined(AFS_HPUX100_ENV)
		sdirEntry->d_off = origOffset;
#endif
		AFS_UIOMOVE((char *)sdirEntry, sizeof(*sdirEntry), UIO_READ,
			    auio, code);
		if (code == 0)
		    AFS_UIOMOVE(ode->name, o_slen, UIO_READ, auio, code);
		/* pad out the remaining characters with zeros */
		if (code == 0) {
		    AFS_UIOMOVE(bufofzeros,
				((o_slen + 1 + DIRPAD) & ~DIRPAD) - o_slen,
				UIO_READ, auio, code);
		}
		/* pad out the difference between rlen and slen... */
		if (DIRSIZ_LEN(o_slen) < rlen) {
		    while (DIRSIZ_LEN(o_slen) < rlen) {
			int minLen = rlen - DIRSIZ_LEN(o_slen);
			if (minLen > sizeof(bufofzeros))
			    minLen = sizeof(bufofzeros);
			AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
			rlen -= minLen;
		    }
		}
#else
		code = afs_readdir_move(ode, avc, auio, o_slen,
#if defined(AFS_SUN5_ENV)
					len, origOffset);
#else
					AFS_UIO_RESID(auio), origOffset);
#endif
#endif /* AFS_HPUX_ENV */
#if !defined(AFS_SUN5_ENV)
		AFS_UIO_SETRESID(auio, 0);
#endif
	    } else {
		/* nothin to hand over */
	    }
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	    if (eofp)
		*eofp = 1;	/* Set it properly */
#endif
	    if (ode)
		DRelease((struct buffer *)ode, 0);
	    goto dirend;
	}
	/* by here nde is set */

	/* Do we have enough user space to carry out our mission? */
#if defined(AFS_SGI_ENV)
	n_slen = strlen(nde->name) + 1;	/* NULL terminate */
#else
	n_slen = strlen(nde->name);
#endif
#ifdef	AFS_SGI53_ENV
	dirsiz =
	    use64BitDirent ? DIRENTSIZE(n_slen) : IRIX5_DIRENTSIZE(n_slen);
	if (dirsiz >= (AFS_UIO_RESID(auio) - len)) {
#else
	if (DIRSIZ_LEN(n_slen) >= (AFS_UIO_RESID(auio) - len)) {
#endif /* AFS_SGI53_ENV */
	    /* No can do no more now; ya know... at this time */
	    DRelease((struct buffer *)nde, 0);	/* can't use this one. */
	    if (len) {
#ifdef	AFS_HPUX_ENV
		sdirEntry->d_fileno =
		    (avc->fid.Fid.Volume << 16) + ntohl(ode->fid.vnode);
		FIXUPSTUPIDINODE(sdirEntry->d_fileno);
		sdirEntry->d_reclen = rlen = AFS_UIO_RESID(auio);
		sdirEntry->d_namlen = o_slen;
#if defined(AFS_SUN5_ENV) || defined(AFS_AIX32_ENV) || defined(AFS_HPUX100_ENV)
		sdirEntry->d_off = origOffset;
#endif
		AFS_UIOMOVE((char *)sdirEntry, sizeof(*sdirEntry), UIO_READ,
			    auio, code);
		if (code == 0)
		    AFS_UIOMOVE(ode->name, o_slen, UIO_READ, auio, code);
		/* pad out the remaining characters with zeros */
		if (code == 0) {
		    AFS_UIOMOVE(bufofzeros,
				((o_slen + 1 + DIRPAD) & ~DIRPAD) - o_slen,
				UIO_READ, auio, code);
		}
		/* pad out the difference between rlen and slen... */
		if (DIRSIZ_LEN(o_slen) < rlen) {
		    while (DIRSIZ_LEN(o_slen) < rlen) {
			int minLen = rlen - DIRSIZ_LEN(o_slen);
			if (minLen > sizeof(bufofzeros))
			    minLen = sizeof(bufofzeros);
			AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
			rlen -= minLen;
		    }
		}
#else /* AFS_HPUX_ENV */
		code =
		    afs_readdir_move(ode, avc, auio, o_slen,
				     AFS_UIO_RESID(auio), origOffset);
#endif /* AFS_HPUX_ENV */
		/* this next line used to be AFSVFS40 or AIX 3.1, but is
		 * really generic */
		AFS_UIO_SETOFFSET(auio, origOffset);
		AFS_UIO_SETRESID(auio, 0);
	    } else {		/* trouble, can't give anything to the user! */
		/* even though he has given us a buffer, 
		 * even though we have something to give us,
		 * Looks like we lost something somewhere.
		 */
		code = EINVAL;
	    }
	    if (ode)
		DRelease((struct buffer *)ode, 0);
	    goto dirend;
	}

	/*
	 * In any event, we move out the LAST de entry, getting ready
	 * to set up for the next one.
	 */
	if (len) {
#ifdef	AFS_HPUX_ENV
	    sdirEntry->d_fileno =
		(avc->fid.Fid.Volume << 16) + ntohl(ode->fid.vnode);
	    FIXUPSTUPIDINODE(sdirEntry->d_fileno);
	    sdirEntry->d_reclen = rlen = len;
	    sdirEntry->d_namlen = o_slen;
#if defined(AFS_SUN5_ENV) || defined(AFS_AIX32_ENV) || defined(AFS_HPUX100_ENV)
	    sdirEntry->d_off = origOffset;
#endif
	    AFS_UIOMOVE((char *)sdirEntry, sizeof(*sdirEntry), UIO_READ, auio,
			code);
	    if (code == 0)
		AFS_UIOMOVE(ode->name, o_slen, UIO_READ, auio, code);
	    /* pad out the remaining characters with zeros */
	    if (code == 0) {
		AFS_UIOMOVE(bufofzeros,
			    ((o_slen + 1 + DIRPAD) & ~DIRPAD) - o_slen,
			    UIO_READ, auio, code);
	    }
	    /* pad out the difference between rlen and slen... */
	    if (DIRSIZ_LEN(o_slen) < rlen) {
		while (DIRSIZ_LEN(o_slen) < rlen) {
		    int minLen = rlen - DIRSIZ_LEN(o_slen);
		    if (minLen > sizeof(bufofzeros))
			minLen = sizeof(bufofzeros);
		    AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
		    rlen -= minLen;
		}
	    }
#else /* AFS_HPUX_ENV */
	    code = afs_readdir_move(ode, avc, auio, o_slen, len, origOffset);
#endif /* AFS_HPUX_ENV */
	}
#ifdef	AFS_SGI53_ENV
	len = use64BitDirent ? DIRENTSIZE(o_slen =
					  n_slen) : IRIX5_DIRENTSIZE(o_slen =
								     n_slen);
#else
	len = DIRSIZ_LEN(o_slen = n_slen);
#endif /* AFS_SGI53_ENV */
	if (ode)
	    DRelease((struct buffer *)ode, 0);
	ode = nde;
	AFS_UIO_SETOFFSET(auio, (afs_int32) ((us + afs_dir_NameBlobs(nde->name)) << 5));
    }
    if (ode)
	DRelease((struct buffer *)ode, 0);

  dirend:
    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);
    ReleaseReadLock(&avc->lock);

  done:
#ifdef	AFS_HPUX_ENV
    osi_FreeSmallSpace((char *)sdirEntry);
#endif
    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, &treq, 28);
    return code;
}

#if	defined(AFS_HPUX_ENV) || defined(AFS_OSF_ENV)
#ifdef	AFS_OSF_ENV
afs1_readdir(avc, auio, acred, eofp)
     int *eofp;
#else
afs1_readdir(avc, auio, acred)
#endif
     struct vcache *avc;
     struct uio *auio;
     struct AFS_UCRED *acred;
{
    struct vrequest treq;
    register struct dcache *tdc;
    afs_size_t origOffset, len;
    int code = 0;
    struct DirEntry *ode = 0, *nde = 0;
    int o_slen = 0, n_slen = 0;
    afs_uint32 us;
#if	defined(AFS_HPUX_ENV) || defined(AFS_OSF_ENV)
    /*
     * XXX All the hacks for alloced sdirEntry and inlining of afs_readdir_move instead of calling
     * it is necessary for hpux due to stack problems that seem to occur when coming thru the nfs
     * translator side XXX
     */
    struct minnfs_direct *sdirEntry = (struct minnfs_direct *)
	osi_AllocSmallSpace(sizeof(struct min_direct));
    afs_int32 rlen;
#endif
    struct afs_fakestat_state fakestate;

    AFS_STATCNT(afs_readdir);
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV)
    if (eofp)
	*eofp = 0;
#endif
    if (code = afs_InitReq(&treq, acred)) {
#ifdef	AFS_HPUX_ENV
	osi_FreeSmallSpace((char *)sdirEntry);
#endif
	return code;
    }
    afs_InitFakeStat(&fakestate);
    code = afs_EvalFakeStat(&avc, &fakestate, &treq);
    if (code) {
#ifdef	AFS_HPUX_ENV
	osi_FreeSmallSpace((char *)sdirEntry);
#endif
	afs_PutFakeStat(&fakestate);
	return code;
    }
    /* update the cache entry */
  tagain:
    code = afs_VerifyVCache(avc, &treq);
    if (code)
	goto done;
    /* get a reference to the entire directory */
    tdc = afs_GetDCache(avc, (afs_size_t) 0, &treq, &origOffset, &len, 1);
    if (!tdc) {
	code = ENOENT;
	goto done;
    }
    ObtainReadLock(&avc->lock);
    ObtainReadLock(&tdc->lock);

    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((avc->states & CStatd)
	   && (tdc->dflags & DFFetching)
	   && hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAIT, ICL_TYPE_STRING,
		   __FILE__, ICL_TYPE_INT32, __LINE__, ICL_TYPE_POINTER, tdc,
		   ICL_TYPE_INT32, tdc->dflags);
	ReleaseReadLock(&tdc->lock);
	ReleaseReadLock(&avc->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainReadLock(&avc->lock);
	ObtainReadLock(&tdc->lock);
    }
    if (!(avc->states & CStatd)
	|| !hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseReadLock(&avc->lock);
	afs_PutDCache(tdc);
	goto tagain;
    }

    len = 0;
#ifdef AFS_HPUX_ENV
    auio->uio_fpflags = 0;
#endif
    while (code == 0) {
	origOffset = AFS_UIO_OFFSET(auio);

	/* scan for the next interesting entry scan for in-use blob otherwise up point at
	 * this blob note that ode, if non-zero, also represents a held dir page */
	if (!(us = BlobScan(tdc, (origOffset >> 5)))
	    || !(nde = (struct DirEntry *)afs_dir_GetBlob(tdc, us))) {
	    /* failed to setup nde, return what we've got, and release ode */
	    if (len) {
		/* something to hand over. */
#if	defined(AFS_HPUX_ENV) || defined(AFS_OSF_ENV)
		sdirEntry->d_fileno =
		    (avc->fid.Fid.Volume << 16) + ntohl(ode->fid.vnode);
		FIXUPSTUPIDINODE(sdirEntry->d_fileno);
		sdirEntry->d_reclen = rlen = AFS_UIO_RESID(auio);
		sdirEntry->d_namlen = o_slen;
		sdirEntry->d_off = origOffset;
		AFS_UIOMOVE((char *)sdirEntry, sizeof(*sdirEntry), UIO_READ,
			    auio, code);
		if (code == 0) {
		    AFS_UIOMOVE(ode->name, o_slen, UIO_READ, auio, code);
		}
		/* pad out the remaining characters with zeros */
		if (code == 0) {
		    AFS_UIOMOVE(bufofzeros,
				((o_slen + 1 + DIRPAD) & ~DIRPAD) - o_slen,
				UIO_READ, auio, code);
		}
		/* pad out the difference between rlen and slen... */
		if (NDIRSIZ_LEN(o_slen) < rlen) {
		    while (NDIRSIZ_LEN(o_slen) < rlen) {
			int minLen = rlen - NDIRSIZ_LEN(o_slen);
			if (minLen > sizeof(bufofzeros))
			    minLen = sizeof(bufofzeros);
			AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
			rlen -= minLen;
		    }
		}
#else
		code =
		    afs_readdir_move(ode, avc, auio, o_slen,
				     AFS_UIO_RESID(auio), origOffset);
#endif /* AFS_HPUX_ENV */
		AFS_UIO_SETRESID(auio, 0);
	    } else {
		/* nothin to hand over */
	    }
#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV)
	    if (eofp)
		*eofp = 1;
#endif
	    if (ode)
		DRelease(ode, 0);
	    goto dirend;
	}
	/* by here nde is set */

	/* Do we have enough user space to carry out our mission? */
#if defined(AFS_SGI_ENV)
	n_slen = strlen(nde->name) + 1;	/* NULL terminate */
#else
	n_slen = strlen(nde->name);
#endif
	if (NDIRSIZ_LEN(n_slen) >= (AFS_UIO_RESID(auio) - len)) {
	    /* No can do no more now; ya know... at this time */
	    DRelease(nde, 0);	/* can't use this one. */
	    if (len) {
#if	defined(AFS_HPUX_ENV) || defined(AFS_OSF_ENV)
		sdirEntry->d_fileno =
		    (avc->fid.Fid.Volume << 16) + ntohl(ode->fid.vnode);
		FIXUPSTUPIDINODE(sdirEntry->d_fileno);
		sdirEntry->d_reclen = rlen = AFS_UIO_RESID(auio);
		sdirEntry->d_namlen = o_slen;
		sdirEntry->d_off = origOffset;
		AFS_UIOMOVE((char *)sdirEntry, sizeof(*sdirEntry), UIO_READ,
			    auio, code);
		if (code == 0)
		    AFS_UIOMOVE(ode->name, o_slen, UIO_READ, auio, code);
		/* pad out the remaining characters with zeros */
		if (code == 0) {
		    AFS_UIOMOVE(bufofzeros,
				((o_slen + 1 + DIRPAD) & ~DIRPAD) - o_slen,
				UIO_READ, auio, code);
		}
		/* pad out the difference between rlen and slen... */
		if (NDIRSIZ_LEN(o_slen) < rlen) {
		    while (NDIRSIZ_LEN(o_slen) < rlen) {
			int minLen = rlen - NDIRSIZ_LEN(o_slen);
			if (minLen > sizeof(bufofzeros))
			    minLen = sizeof(bufofzeros);
			AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
			rlen -= minLen;
		    }
		}
#else
		code =
		    afs_readdir_move(ode, avc, auio, o_slen,
				     AFS_UIO_RESID(auio), origOffset);
#endif /* AFS_HPUX_ENV */
		/* this next line used to be AFSVFS40 or AIX 3.1, but is really generic */
		AFS_UIO_SETOFFSET(auio, origOffset);
		AFS_UIO_SETRESID(auio, 0);
	    } else {		/* trouble, can't give anything to the user! */
		/* even though he has given us a buffer, 
		 * even though we have something to give us,
		 * Looks like we lost something somewhere.
		 */
		code = EINVAL;
	    }
	    if (ode)
		DRelease(ode, 0);
	    goto dirend;
	}

	/*
	 * In any event, we move out the LAST de entry, getting ready
	 * to set up for the next one.
	 */
	if (len) {
#if	defined(AFS_HPUX_ENV) || defined(AFS_OSF_ENV)
	    sdirEntry->d_fileno =
		(avc->fid.Fid.Volume << 16) + ntohl(ode->fid.vnode);
	    FIXUPSTUPIDINODE(sdirEntry->d_fileno);
	    sdirEntry->d_reclen = rlen = len;
	    sdirEntry->d_namlen = o_slen;
	    sdirEntry->d_off = origOffset;
	    AFS_UIOMOVE((char *)sdirEntry, sizeof(*sdirEntry), UIO_READ, auio,
			code);
	    if (code == 0)
		AFS_UIOMOVE(ode->name, o_slen, UIO_READ, auio, code);
	    /* pad out the remaining characters with zeros */
	    if (code == 0) {
		AFS_UIOMOVE(bufofzeros,
			    ((o_slen + 1 + DIRPAD) & ~DIRPAD) - o_slen,
			    UIO_READ, auio, code);
	    }
	    /* pad out the difference between rlen and slen... */
	    if (NDIRSIZ_LEN(o_slen) < rlen) {
		while (NDIRSIZ_LEN(o_slen) < rlen) {
		    int minLen = rlen - NDIRSIZ_LEN(o_slen);
		    if (minLen > sizeof(bufofzeros))
			minLen = sizeof(bufofzeros);
		    AFS_UIOMOVE(bufofzeros, minLen, UIO_READ, auio, code);
		    rlen -= minLen;
		}
	    }
#else
	    code = afs_readdir_move(ode, avc, auio, o_slen, len, origOffset);
#endif /* AFS_HPUX_ENV */
	}
	len = NDIRSIZ_LEN(o_slen = n_slen);
	if (ode)
	    DRelease(ode, 0);
	ode = nde;
	AFS_UIO_OFFSET(auio) = ((us + afs_dir_NameBlobs(nde->name)) << 5);
    }
    if (ode)
	DRelease(ode, 0);

  dirend:
    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);
    ReleaseReadLock(&avc->lock);

  done:
#if	defined(AFS_HPUX_ENV) || defined(AFS_OSF_ENV)
    osi_FreeSmallSpace((char *)sdirEntry);
#endif
    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, &treq, 29);
    return code;
}

#endif
#endif /* !AFS_LINUX20_ENV */
