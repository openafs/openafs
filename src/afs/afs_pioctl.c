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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#ifdef AFS_OBSD_ENV
#include "h/syscallargs.h"
#endif
#ifdef AFS_FBSD_ENV
#include "h/sysproto.h"
#endif
#ifdef AFS_NBSD40_ENV
#include <sys/ioctl.h>
#include <sys/ioccom.h>
#endif
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "afs/vice.h"
#include "afs/afs_bypasscache.h"
#include "rx/rx_globals.h"

struct VenusFid afs_rootFid;
afs_int32 afs_waitForever = 0;
short afs_waitForeverCount = 0;
afs_int32 afs_showflags = GAGUSER | GAGCONSOLE;	/* show all messages */

afs_int32 afs_is_disconnected;
afs_int32 afs_is_discon_rw;
/* On reconnection, turn this knob on until it finishes,
 * then turn it off.
 */
afs_int32 afs_in_sync = 0;

struct afs_pdata {
    char *ptr;
    size_t remaining;
};

/*
 * A set of handy little functions for encoding and decoding
 * pioctls without losing your marbles, or memory integrity
 */

static_inline int
afs_pd_alloc(struct afs_pdata *apd, size_t size)
{
    /* Ensure that we give caller at least one trailing guard byte
     * for the NUL terminator. */
    if (size >= AFS_LRALLOCSIZ)
	apd->ptr = osi_Alloc(size + 1);
    else
	apd->ptr = osi_AllocLargeSpace(AFS_LRALLOCSIZ);

    if (apd->ptr == NULL)
	return ENOMEM;

    /* Clear it all now, including the guard byte. */
    if (size >= AFS_LRALLOCSIZ)
	memset(apd->ptr, 0, size + 1);
    else
	memset(apd->ptr, 0, AFS_LRALLOCSIZ);

    /* Don't tell the caller about the guard byte. */
    apd->remaining = size;

    return 0;
}

static_inline void
afs_pd_free(struct afs_pdata *apd)
{
    if (apd->ptr == NULL)
	return;

    if (apd->remaining >= AFS_LRALLOCSIZ)
	osi_Free(apd->ptr, apd->remaining + 1);
    else
	osi_FreeLargeSpace(apd->ptr);

    apd->ptr = NULL;
    apd->remaining = 0;
}

static_inline char *
afs_pd_where(struct afs_pdata *apd)
{
    return apd ? apd->ptr : NULL;
}

static_inline size_t
afs_pd_remaining(struct afs_pdata *apd)
{
    return apd ? apd->remaining : 0;
}

static_inline int
afs_pd_skip(struct afs_pdata *apd, size_t skip)
{
    if (apd == NULL || apd->remaining < skip)
	return EINVAL;
    apd->remaining -= skip;
    apd->ptr += skip;

    return 0;
}

static_inline int
afs_pd_getBytes(struct afs_pdata *apd, void *dest, size_t bytes)
{
    if (apd == NULL || apd->remaining < bytes)
	return EINVAL;
    apd->remaining -= bytes;
    memcpy(dest, apd->ptr, bytes);
    apd->ptr += bytes;
    return 0;
}

static_inline int
afs_pd_getInt(struct afs_pdata *apd, afs_int32 *val)
{
    return afs_pd_getBytes(apd, val, sizeof(*val));
}

static_inline int
afs_pd_getUint(struct afs_pdata *apd, afs_uint32 *val)
{
    return afs_pd_getBytes(apd, val, sizeof(*val));
}

static_inline void *
afs_pd_inline(struct afs_pdata *apd, size_t bytes)
{
    void *ret;

    if (apd == NULL || apd->remaining < bytes)
	return NULL;

    ret = apd->ptr;

    apd->remaining -= bytes;
    apd->ptr += bytes;

    return ret;
}

static_inline int
afs_pd_getString(struct afs_pdata *apd, char *str, size_t maxLen)
{
    size_t len;

    if (apd == NULL || apd->remaining <= 0)
	return EINVAL;
    len = strlen(apd->ptr) + 1;
    if (len > maxLen)
	return E2BIG;
    memcpy(str, apd->ptr, len);
    apd->ptr += len;
    apd->remaining -= len;
    return 0;
}

static_inline int
afs_pd_getStringPtr(struct afs_pdata *apd, char **str)
{
    size_t len;

    if (apd == NULL || apd->remaining <= 0)
	return EINVAL;
    len = strlen(apd->ptr) + 1;
    *str = apd->ptr;
    apd->ptr += len;
    apd->remaining -= len;
    return 0;
}

static_inline int
afs_pd_putBytes(struct afs_pdata *apd, const void *bytes, size_t len)
{
    if (apd == NULL || apd->remaining < len)
	return E2BIG;
    memcpy(apd->ptr, bytes, len);
    apd->ptr += len;
    apd->remaining -= len;
    return 0;
}

static_inline int
afs_pd_putInt(struct afs_pdata *apd, afs_int32 val)
{
    return afs_pd_putBytes(apd, &val, sizeof(val));
}

static_inline int
afs_pd_putString(struct afs_pdata *apd, char *str) {

    /* Add 1 so we copy the NULL too */
    return afs_pd_putBytes(apd, str, strlen(str) +1);
}

/*!
 * \defgroup pioctl Path IOCTL functions
 *
 * DECL_PIOCTL is a macro defined to contain the following parameters for functions:
 *
 * \param[in] avc
 * 	the AFS vcache structure in use by pioctl
 * \param[in] afun
 * 	not in use
 * \param[in] areq
 * 	the AFS vrequest structure
 * \param[in] ain
 * 	an afs_pdata block describing the data received from the caller
 * \param[in] aout
 * 	an afs_pdata block describing a pre-allocated block for output
 * \param[in] acred
 * 	UNIX credentials structure underlying the operation
 */

#define DECL_PIOCTL(x) \
	static int x(struct vcache *avc, int afun, struct vrequest *areq, \
		     struct afs_pdata *ain, struct afs_pdata *aout, \
		     afs_ucred_t **acred)

/* Prototypes for pioctl routines */
DECL_PIOCTL(PGetFID);
DECL_PIOCTL(PSetAcl);
DECL_PIOCTL(PStoreBehind);
DECL_PIOCTL(PGCPAGs);
DECL_PIOCTL(PGetAcl);
DECL_PIOCTL(PNoop);
DECL_PIOCTL(PBogus);
DECL_PIOCTL(PGetFileCell);
DECL_PIOCTL(PGetWSCell);
DECL_PIOCTL(PGetUserCell);
DECL_PIOCTL(PSetTokens);
DECL_PIOCTL(PGetVolumeStatus);
DECL_PIOCTL(PSetVolumeStatus);
DECL_PIOCTL(PFlush);
DECL_PIOCTL(PNewStatMount);
DECL_PIOCTL(PGetTokens);
DECL_PIOCTL(PUnlog);
DECL_PIOCTL(PMariner);
DECL_PIOCTL(PCheckServers);
DECL_PIOCTL(PCheckVolNames);
DECL_PIOCTL(PCheckAuth);
DECL_PIOCTL(PFindVolume);
DECL_PIOCTL(PViceAccess);
DECL_PIOCTL(PSetCacheSize);
DECL_PIOCTL(PGetCacheSize);
DECL_PIOCTL(PRemoveCallBack);
DECL_PIOCTL(PNewCell);
DECL_PIOCTL(PNewAlias);
DECL_PIOCTL(PListCells);
DECL_PIOCTL(PListAliases);
DECL_PIOCTL(PRemoveMount);
DECL_PIOCTL(PGetCellStatus);
DECL_PIOCTL(PSetCellStatus);
DECL_PIOCTL(PFlushVolumeData);
DECL_PIOCTL(PFlushAllVolumeData);
DECL_PIOCTL(PGetVnodeXStatus);
DECL_PIOCTL(PGetVnodeXStatus2);
DECL_PIOCTL(PSetSysName);
DECL_PIOCTL(PSetSPrefs);
DECL_PIOCTL(PSetSPrefs33);
DECL_PIOCTL(PGetSPrefs);
DECL_PIOCTL(PExportAfs);
DECL_PIOCTL(PGag);
DECL_PIOCTL(PTwiddleRx);
DECL_PIOCTL(PGetInitParams);
DECL_PIOCTL(PGetRxkcrypt);
DECL_PIOCTL(PSetRxkcrypt);
DECL_PIOCTL(PGetCPrefs);
DECL_PIOCTL(PSetCPrefs);
DECL_PIOCTL(PFlushMount);
DECL_PIOCTL(PRxStatProc);
DECL_PIOCTL(PRxStatPeer);
DECL_PIOCTL(PPrefetchFromTape);
DECL_PIOCTL(PFsCmd);
DECL_PIOCTL(PCallBackAddr);
DECL_PIOCTL(PDiscon);
DECL_PIOCTL(PNFSNukeCreds);
DECL_PIOCTL(PNewUuid);
DECL_PIOCTL(PPrecache);
DECL_PIOCTL(PGetPAG);
#if defined(AFS_CACHE_BYPASS) && defined(AFS_LINUX24_ENV)
DECL_PIOCTL(PSetCachingThreshold);
#endif

/*
 * A macro that says whether we're going to need HandleClientContext().
 * This is currently used only by the nfs translator.
 */
#if !defined(AFS_NONFSTRANS) || defined(AFS_AIX_IAUTH_ENV)
#define AFS_NEED_CLIENTCONTEXT
#endif

/* Prototypes for private routines */
#ifdef AFS_NEED_CLIENTCONTEXT
static int HandleClientContext(struct afs_ioctl *ablob, int *com,
			       afs_ucred_t **acred,
			       afs_ucred_t *credp);
#endif
int HandleIoctl(struct vcache *avc, afs_int32 acom,
		struct afs_ioctl *adata);
int afs_HandlePioctl(struct vnode *avp, afs_int32 acom,
		     struct afs_ioctl *ablob, int afollow,
		     afs_ucred_t **acred);
static int Prefetch(uparmtype apath, struct afs_ioctl *adata, int afollow,
		    afs_ucred_t *acred);

typedef int (*pioctlFunction) (struct vcache *, int, struct vrequest *,
			       struct afs_pdata *, struct afs_pdata *,
			       afs_ucred_t **);

static pioctlFunction VpioctlSw[] = {
    PBogus,			/* 0 */
    PSetAcl,			/* 1 */
    PGetAcl,			/* 2 */
    PSetTokens,			/* 3 */
    PGetVolumeStatus,		/* 4 */
    PSetVolumeStatus,		/* 5 */
    PFlush,			/* 6 */
    PBogus,			/* 7 */
    PGetTokens,			/* 8 */
    PUnlog,			/* 9 */
    PCheckServers,		/* 10 */
    PCheckVolNames,		/* 11 */
    PCheckAuth,			/* 12 */
    PBogus,			/* 13 -- used to be quick check time */
    PFindVolume,		/* 14 */
    PBogus,			/* 15 -- prefetch is now special-cased; see pioctl code! */
    PBogus,			/* 16 -- used to be testing code */
    PNoop,			/* 17 -- used to be enable group */
    PNoop,			/* 18 -- used to be disable group */
    PBogus,			/* 19 -- used to be list group */
    PViceAccess,		/* 20 */
    PUnlog,			/* 21 -- unlog *is* unpag in this system */
    PGetFID,			/* 22 -- get file ID */
    PBogus,			/* 23 -- used to be waitforever */
    PSetCacheSize,		/* 24 */
    PRemoveCallBack,		/* 25 -- flush only the callback */
    PNewCell,			/* 26 */
    PListCells,			/* 27 */
    PRemoveMount,		/* 28 -- delete mount point */
    PNewStatMount,		/* 29 -- new style mount point stat */
    PGetFileCell,		/* 30 -- get cell name for input file */
    PGetWSCell,			/* 31 -- get cell name for workstation */
    PMariner,			/* 32 - set/get mariner host */
    PGetUserCell,		/* 33 -- get cell name for user */
    PBogus,			/* 34 -- Enable/Disable logging */
    PGetCellStatus,		/* 35 */
    PSetCellStatus,		/* 36 */
    PFlushVolumeData,		/* 37 -- flush all data from a volume */
    PSetSysName,		/* 38 - Set system name */
    PExportAfs,			/* 39 - Export Afs to remote nfs clients */
    PGetCacheSize,		/* 40 - get cache size and usage */
    PGetVnodeXStatus,		/* 41 - get vcache's special status */
    PSetSPrefs33,		/* 42 - Set CM Server preferences... */
    PGetSPrefs,			/* 43 - Get CM Server preferences... */
    PGag,			/* 44 - turn off/on all CM messages */
    PTwiddleRx,			/* 45 - adjust some RX params       */
    PSetSPrefs,			/* 46 - Set CM Server preferences... */
    PStoreBehind,		/* 47 - set degree of store behind to be done */
    PGCPAGs,			/* 48 - disable automatic pag gc-ing */
    PGetInitParams,		/* 49 - get initial cm params */
    PGetCPrefs,			/* 50 - get client interface addresses */
    PSetCPrefs,			/* 51 - set client interface addresses */
    PFlushMount,		/* 52 - flush mount symlink data */
    PRxStatProc,		/* 53 - control process RX statistics */
    PRxStatPeer,		/* 54 - control peer RX statistics */
    PGetRxkcrypt,		/* 55 -- Get rxkad encryption flag */
    PSetRxkcrypt,		/* 56 -- Set rxkad encryption flag */
    PBogus,			/* 57 -- arla: set file prio */
    PBogus,			/* 58 -- arla: fallback getfh */
    PBogus,			/* 59 -- arla: fallback fhopen */
    PBogus,			/* 60 -- arla: controls xfsdebug */
    PBogus,			/* 61 -- arla: controls arla debug */
    PBogus,			/* 62 -- arla: debug interface */
    PBogus,			/* 63 -- arla: print xfs status */
    PBogus,			/* 64 -- arla: force cache check */
    PBogus,			/* 65 -- arla: break callback */
    PPrefetchFromTape,		/* 66 -- MR-AFS: prefetch file from tape */
    PFsCmd,			/* 67 -- RXOSD: generic commnd interface */
    PBogus,			/* 68 -- arla: fetch stats */
    PGetVnodeXStatus2,		/* 69 - get caller access and some vcache status */
};

static pioctlFunction CpioctlSw[] = {
    PBogus,                     /* 0 */
    PNewAlias,                  /* 1 -- create new cell alias */
    PListAliases,               /* 2 -- list cell aliases */
    PCallBackAddr,              /* 3 -- request addr for callback rxcon */
    PBogus,                     /* 4 */
    PDiscon,                    /* 5 -- get/set discon mode */
    PBogus,                     /* 6 */
    PBogus,                     /* 7 */
    PBogus,                     /* 8 */
    PNewUuid,                   /* 9 */
    PBogus,                     /* 10 */
    PBogus,                     /* 11 */
    PPrecache,                  /* 12 */
    PGetPAG,                    /* 13 */
    PFlushAllVolumeData,        /* 14 */
};

static pioctlFunction OpioctlSw[]  = {
    PBogus,			/* 0 */
    PNFSNukeCreds,		/* 1 -- nuke all creds for NFS client */
#if defined(AFS_CACHE_BYPASS) && defined(AFS_LINUX24_ENV)
    PSetCachingThreshold        /* 2 -- get/set cache-bypass size threshold */
#else
    PNoop                       /* 2 -- get/set cache-bypass size threshold */
#endif
};

#define PSetClientContext 99	/*  Special pioctl to setup caller's creds  */
int afs_nobody = NFS_NOBODY;

int
HandleIoctl(struct vcache *avc, afs_int32 acom,
	    struct afs_ioctl *adata)
{
    afs_int32 code;

    code = 0;
    AFS_STATCNT(HandleIoctl);

    switch (acom & 0xff) {
    case 1:
	avc->f.states |= CSafeStore;
	avc->asynchrony = 0;
	/* SXW - Should we force a MetaData flush for this flag setting */
	break;

	/* case 2 used to be abort store, but this is no longer provided,
	 * since it is impossible to implement under normal Unix.
	 */

    case 3:{
	    /* return the name of the cell this file is open on */
	    struct cell *tcell;
	    afs_int32 i;

	    tcell = afs_GetCell(avc->f.fid.Cell, READ_LOCK);
	    if (tcell) {
		i = strlen(tcell->cellName) + 1;	/* bytes to copy out */

		if (i > adata->out_size) {
		    /* 0 means we're not interested in the output */
		    if (adata->out_size != 0)
			code = EFAULT;
		} else {
		    /* do the copy */
		    AFS_COPYOUT(tcell->cellName, adata->out, i, code);
		}
		afs_PutCell(tcell, READ_LOCK);
	    } else
		code = ENOTTY;
	}
	break;

    case 49:			/* VIOC_GETINITPARAMS */
	if (adata->out_size < sizeof(struct cm_initparams)) {
	    code = EFAULT;
	} else {
	    AFS_COPYOUT(&cm_initParams, adata->out,
			sizeof(struct cm_initparams), code);
	}
	break;

    default:

	code = EINVAL;
#ifdef AFS_AIX51_ENV
	code = ENOSYS;
#endif
	break;
    }
    return code;		/* so far, none implemented */
}

#ifdef AFS_AIX_ENV
/* For aix we don't temporarily bypass ioctl(2) but rather do our
 * thing directly in the vnode layer call, VNOP_IOCTL; thus afs_ioctl
 * is now called from afs_gn_ioctl.
 */
int
afs_ioctl(struct vcache *tvc, int cmd, int arg)
{
    struct afs_ioctl data;
    int error = 0;

    AFS_STATCNT(afs_ioctl);
    if (((cmd >> 8) & 0xff) == 'V') {
	/* This is a VICEIOCTL call */
	AFS_COPYIN(arg, (caddr_t) & data, sizeof(data), error);
	if (error)
	    return (error);
	error = HandleIoctl(tvc, cmd, &data);
	return (error);
    } else {
	/* No-op call; just return. */
	return (ENOTTY);
    }
}
# if defined(AFS_AIX32_ENV)
#  if defined(AFS_AIX51_ENV)
#   ifdef __64BIT__
int
kioctl(int fdes, int com, caddr_t arg, caddr_t ext, caddr_t arg2,
	   caddr_t arg3)
#   else /* __64BIT__ */
int
kioctl32(int fdes, int com, caddr_t arg, caddr_t ext, caddr_t arg2,
	     caddr_t arg3)
#   endif /* __64BIT__ */
#  else
int
kioctl(int fdes, int com, caddr_t arg, caddr_t ext)
#  endif /* AFS_AIX51_ENV */
{
    struct a {
	int fd, com;
	caddr_t arg, ext;
#  ifdef AFS_AIX51_ENV
	caddr_t arg2, arg3;
#  endif
    } u_uap, *uap = &u_uap;
    struct file *fd;
    struct vcache *tvc;
    int ioctlDone = 0, code = 0;

    AFS_STATCNT(afs_xioctl);
    uap->fd = fdes;
    uap->com = com;
    uap->arg = arg;
#  ifdef AFS_AIX51_ENV
    uap->arg2 = arg2;
    uap->arg3 = arg3;
#  endif
    if (setuerror(getf(uap->fd, &fd))) {
	return -1;
    }
    if (fd->f_type == DTYPE_VNODE) {
	/* good, this is a vnode; next see if it is an AFS vnode */
	tvc = VTOAFS(fd->f_vnode);	/* valid, given a vnode */
	if (tvc && IsAfsVnode(AFSTOV(tvc))) {
	    /* This is an AFS vnode */
	    if (((uap->com >> 8) & 0xff) == 'V') {
		struct afs_ioctl *datap;
		AFS_GLOCK();
		datap =
		    (struct afs_ioctl *)osi_AllocSmallSpace(AFS_SMALLOCSIZ);
		code=copyin_afs_ioctl((char *)uap->arg, datap);
		if (code) {
		    osi_FreeSmallSpace(datap);
		    AFS_GUNLOCK();
#  if defined(AFS_AIX41_ENV)
		    ufdrele(uap->fd);
#  endif
		    return (setuerror(code), code);
		}
		code = HandleIoctl(tvc, uap->com, datap);
		osi_FreeSmallSpace(datap);
		AFS_GUNLOCK();
		ioctlDone = 1;
#  if defined(AFS_AIX41_ENV)
		ufdrele(uap->fd);
#  endif
	     }
	}
    }
    if (!ioctlDone) {
#  if defined(AFS_AIX41_ENV)
	ufdrele(uap->fd);
#   if defined(AFS_AIX51_ENV)
#    ifdef __64BIT__
	code = okioctl(fdes, com, arg, ext, arg2, arg3);
#    else /* __64BIT__ */
	code = okioctl32(fdes, com, arg, ext, arg2, arg3);
#    endif /* __64BIT__ */
#   else /* !AFS_AIX51_ENV */
	code = okioctl(fdes, com, arg, ext);
#   endif /* AFS_AIX51_ENV */
	return code;
#  elif defined(AFS_AIX32_ENV)
	okioctl(fdes, com, arg, ext);
#  endif
    }
#  if defined(KERNEL_HAVE_UERROR)
    if (!getuerror())
	setuerror(code);
#   if !defined(AFS_AIX41_ENV)
    return (getuerror()? -1 : u.u_ioctlrv);
#   else
    return getuerror()? -1 : 0;
#   endif
#  endif
    return 0;
}
# endif

#elif defined(AFS_SGI_ENV)
# if defined(AFS_SGI65_ENV)
afs_ioctl(OSI_VN_DECL(tvc), int cmd, void *arg, int flag, cred_t * cr,
	  rval_t * rvalp, struct vopbd * vbds)
# else
afs_ioctl(OSI_VN_DECL(tvc), int cmd, void *arg, int flag, cred_t * cr,
	  rval_t * rvalp, struct vopbd * vbds)
# endif
{
    struct afs_ioctl data;
    int error = 0;
    int locked;

    OSI_VN_CONVERT(tvc);

    AFS_STATCNT(afs_ioctl);
    if (((cmd >> 8) & 0xff) == 'V') {
	/* This is a VICEIOCTL call */
	error = copyin_afs_ioctl(arg, &data);
	if (error)
	    return (error);
	locked = ISAFS_GLOCK();
	if (!locked)
	    AFS_GLOCK();
	error = HandleIoctl(tvc, cmd, &data);
	if (!locked)
	    AFS_GUNLOCK();
	return (error);
    } else {
	/* No-op call; just return. */
	return (ENOTTY);
    }
}
#elif defined(AFS_SUN5_ENV)
struct afs_ioctl_sys {
    int fd;
    int com;
    int arg;
};

int
afs_xioctl(struct afs_ioctl_sys *uap, rval_t *rvp)
{
    struct file *fd;
    struct vcache *tvc;
    int ioctlDone = 0, code = 0;

    AFS_STATCNT(afs_xioctl);
# if defined(AFS_SUN57_ENV)
    fd = getf(uap->fd);
    if (!fd)
	return (EBADF);
# elif defined(AFS_SUN54_ENV)
    fd = GETF(uap->fd);
    if (!fd)
	return (EBADF);
# else
    if (code = getf(uap->fd, &fd)) {
	return (code);
    }
# endif
    if (fd->f_vnode->v_type == VREG || fd->f_vnode->v_type == VDIR) {
	tvc = VTOAFS(fd->f_vnode);	/* valid, given a vnode */
	if (tvc && IsAfsVnode(AFSTOV(tvc))) {
	    /* This is an AFS vnode */
	    if (((uap->com >> 8) & 0xff) == 'V') {
		struct afs_ioctl *datap;
		AFS_GLOCK();
		datap =
		    (struct afs_ioctl *)osi_AllocSmallSpace(AFS_SMALLOCSIZ);
		code=copyin_afs_ioctl((char *)uap->arg, datap);
		if (code) {
		    osi_FreeSmallSpace(datap);
		    AFS_GUNLOCK();
# if defined(AFS_SUN54_ENV)
		    releasef(uap->fd);
# else
		    releasef(fd);
# endif
		    return (EFAULT);
		}
		code = HandleIoctl(tvc, uap->com, datap);
		osi_FreeSmallSpace(datap);
		AFS_GUNLOCK();
		ioctlDone = 1;
	    }
	}
    }
# if defined(AFS_SUN57_ENV)
    releasef(uap->fd);
# elif defined(AFS_SUN54_ENV)
    RELEASEF(uap->fd);
# else
    releasef(fd);
# endif
    if (!ioctlDone)
	code = ioctl(uap, rvp);

    return (code);
}
#elif defined(AFS_LINUX22_ENV)
struct afs_ioctl_sys {
    unsigned int com;
    unsigned long arg;
};
int
afs_xioctl(struct inode *ip, struct file *fp, unsigned int com,
	   unsigned long arg)
{
    struct afs_ioctl_sys ua, *uap = &ua;
    struct vcache *tvc;
    int ioctlDone = 0, code = 0;

    AFS_STATCNT(afs_xioctl);
    ua.com = com;
    ua.arg = arg;

    tvc = VTOAFS(ip);
    if (tvc && IsAfsVnode(AFSTOV(tvc))) {
	/* This is an AFS vnode */
	if (((uap->com >> 8) & 0xff) == 'V') {
	    struct afs_ioctl *datap;
	    AFS_GLOCK();
	    datap = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
	    code = copyin_afs_ioctl((char *)uap->arg, datap);
	    if (code) {
		osi_FreeSmallSpace(datap);
		AFS_GUNLOCK();
		return -code;
	    }
	    code = HandleIoctl(tvc, uap->com, datap);
	    osi_FreeSmallSpace(datap);
	    AFS_GUNLOCK();
	    ioctlDone = 1;
	}
	else
	    code = EINVAL;
    }
    return -code;
}
#elif defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN80_ENV)
struct ioctl_args {
    int fd;
    u_long com;
    caddr_t arg;
};

int
afs_xioctl(afs_proc_t *p, struct ioctl_args *uap, register_t *retval)
{
    struct file *fd;
    struct vcache *tvc;
    int ioctlDone = 0, code = 0;

    AFS_STATCNT(afs_xioctl);
    if ((code = fdgetf(p, uap->fd, &fd)))
	return code;
    if (fd->f_type == DTYPE_VNODE) {
	tvc = VTOAFS((struct vnode *)fd->f_data);	/* valid, given a vnode */
	if (tvc && IsAfsVnode(AFSTOV(tvc))) {
	    /* This is an AFS vnode */
	    if (((uap->com >> 8) & 0xff) == 'V') {
		struct afs_ioctl *datap;
		AFS_GLOCK();
		datap = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
		code = copyin_afs_ioctl((char *)uap->arg, datap);
		if (code) {
		    osi_FreeSmallSpace(datap);
		    AFS_GUNLOCK();
		    return code;
		}
		code = HandleIoctl(tvc, uap->com, datap);
		osi_FreeSmallSpace(datap);
		AFS_GUNLOCK();
		ioctlDone = 1;
	    }
	}
    }

    if (!ioctlDone)
	return ioctl(p, uap, retval);

    return (code);
}
#elif defined(AFS_XBSD_ENV)
# if defined(AFS_FBSD_ENV)
#  define arg data
int
afs_xioctl(struct thread *td, struct ioctl_args *uap,
	   register_t *retval)
{
    afs_proc_t *p = td->td_proc;
# else
struct ioctl_args {
    int fd;
    u_long com;
    caddr_t arg;
};

int
afs_xioctl(afs_proc_t *p, struct ioctl_args *uap, register_t *retval)
{
# endif
    struct filedesc *fdp;
    struct vcache *tvc;
    int ioctlDone = 0, code = 0;
    struct file *fd;

    AFS_STATCNT(afs_xioctl);
#   if defined(AFS_NBSD40_ENV)
     fdp = p->l_proc->p_fd;
#   else
    fdp = p->p_fd;
#endif
#if defined(AFS_FBSD100_ENV)
    if ((uap->fd >= fdp->fd_nfiles)
	|| ((fd = fdp->fd_ofiles[uap->fd].fde_file) == NULL))
	return EBADF;
#else
    if ((u_int) uap->fd >= fdp->fd_nfiles
	|| (fd = fdp->fd_ofiles[uap->fd]) == NULL)
	return EBADF;
#endif
    if ((fd->f_flag & (FREAD | FWRITE)) == 0)
	return EBADF;
    /* first determine whether this is any sort of vnode */
    if (fd->f_type == DTYPE_VNODE) {
	/* good, this is a vnode; next see if it is an AFS vnode */
# if defined(AFS_OBSD_ENV)
	tvc =
	    IsAfsVnode((struct vnode *)fd->
		       f_data) ? VTOAFS((struct vnode *)fd->f_data) : NULL;
# else
	tvc = VTOAFS((struct vnode *)fd->f_data);	/* valid, given a vnode */
# endif
	if (tvc && IsAfsVnode(AFSTOV(tvc))) {
	    /* This is an AFS vnode */
	    if (((uap->com >> 8) & 0xff) == 'V') {
		struct afs_ioctl *datap;
		AFS_GLOCK();
		datap = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
		code = copyin_afs_ioctl((char *)uap->arg, datap);
		if (code) {
		    osi_FreeSmallSpace(datap);
		    AFS_GUNLOCK();
		    return code;
		}
		code = HandleIoctl(tvc, uap->com, datap);
		osi_FreeSmallSpace(datap);
		AFS_GUNLOCK();
		ioctlDone = 1;
	    }
	}
    }

    if (!ioctlDone) {
# if defined(AFS_FBSD_ENV)
#  if (__FreeBSD_version >= 900044)
	return sys_ioctl(td, uap);
#  else
	return ioctl(td, uap);
#  endif
# elif defined(AFS_OBSD_ENV)
	code = sys_ioctl(p, uap, retval);
# elif defined(AFS_NBSD_ENV)
           struct lwp *l = osi_curproc();
           code = sys_ioctl(l, uap, retval);
# endif
    }

    return (code);
}
#elif defined(UKERNEL)
int
afs_xioctl(void)
{
    struct a {
	int fd;
	int com;
	caddr_t arg;
    } *uap = (struct a *)get_user_struct()->u_ap;
    struct file *fd;
    struct vcache *tvc;
    int ioctlDone = 0, code = 0;

    AFS_STATCNT(afs_xioctl);

    fd = getf(uap->fd);
    if (!fd)
	return (EBADF);
    /* first determine whether this is any sort of vnode */
    if (fd->f_type == DTYPE_VNODE) {
	/* good, this is a vnode; next see if it is an AFS vnode */
	tvc = VTOAFS((struct vnode *)fd->f_data);	/* valid, given a vnode */
	if (tvc && IsAfsVnode(AFSTOV(tvc))) {
	    /* This is an AFS vnode */
	    if (((uap->com >> 8) & 0xff) == 'V') {
		struct afs_ioctl *datap;
		AFS_GLOCK();
		datap = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
		code=copyin_afs_ioctl((char *)uap->arg, datap);
		if (code) {
		    osi_FreeSmallSpace(datap);
		    AFS_GUNLOCK();

		    return (setuerror(code), code);
		}
		code = HandleIoctl(tvc, uap->com, datap);
		osi_FreeSmallSpace(datap);
		AFS_GUNLOCK();
		ioctlDone = 1;
	    }
	}
    }

    if (!ioctlDone) {
	ioctl();
    }

    return 0;
}
#endif /* AFS_HPUX102_ENV */

#if defined(AFS_SGI_ENV)
  /* "pioctl" system call entry point; just pass argument to the parameterized
   * call below */
struct pioctlargs {
    char *path;
    sysarg_t cmd;
    caddr_t cmarg;
    sysarg_t follow;
};
int
afs_pioctl(struct pioctlargs *uap, rval_t * rvp)
{
    int code;

    AFS_STATCNT(afs_pioctl);
    AFS_GLOCK();
    code = afs_syscall_pioctl(uap->path, uap->cmd, uap->cmarg, uap->follow);
    AFS_GUNLOCK();
# ifdef AFS_SGI64_ENV
    return code;
# else
    return u.u_error;
# endif
}

#elif defined(AFS_FBSD_ENV)
int
afs_pioctl(struct thread *td, void *args, int *retval)
{
    struct a {
	char *path;
	int cmd;
	caddr_t cmarg;
	int follow;
    } *uap = (struct a *)args;

    AFS_STATCNT(afs_pioctl);
    return (afs_syscall_pioctl
	    (uap->path, uap->cmd, uap->cmarg, uap->follow, td->td_ucred));
}

#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
int
afs_pioctl(afs_proc_t *p, void *args, int *retval)
{
    struct a {
	char *path;
	int cmd;
	caddr_t cmarg;
	int follow;
    } *uap = (struct a *)args;

    AFS_STATCNT(afs_pioctl);
# if defined(AFS_DARWIN80_ENV) || defined(AFS_NBSD40_ENV)
    return (afs_syscall_pioctl
	    (uap->path, uap->cmd, uap->cmarg, uap->follow,
	     kauth_cred_get()));
# else
    return (afs_syscall_pioctl
	    (uap->path, uap->cmd, uap->cmarg, uap->follow,
#  if defined(AFS_FBSD_ENV)
	     td->td_ucred));
#  else
	     p->p_cred->pc_ucred));
#  endif
# endif
}

#endif

/* macro to avoid adding any more #ifdef's to pioctl code. */
#if defined(AFS_LINUX22_ENV) || defined(AFS_AIX41_ENV)
#define PIOCTL_FREE_CRED() crfree(credp)
#else
#define PIOCTL_FREE_CRED()
#endif

int
#ifdef	AFS_SUN5_ENV
afs_syscall_pioctl(char *path, unsigned int com, caddr_t cmarg, int follow,
		   rval_t *vvp, afs_ucred_t *credp)
#else
#ifdef AFS_DARWIN100_ENV
afs_syscall64_pioctl(user_addr_t path, unsigned int com, user_addr_t cmarg,
		   int follow, afs_ucred_t *credp)
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
afs_syscall_pioctl(char *path, unsigned int com, caddr_t cmarg, int follow,
		   afs_ucred_t *credp)
#else
afs_syscall_pioctl(char *path, unsigned int com, caddr_t cmarg, int follow)
#endif
#endif
{
    struct afs_ioctl data;
#ifdef AFS_NEED_CLIENTCONTEXT
    afs_ucred_t *tmpcred = NULL;
#endif
#if defined(AFS_NEED_CLIENTCONTEXT) || defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_LINUX22_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    afs_ucred_t *foreigncreds = NULL;
#endif
    afs_int32 code = 0;
    struct vnode *vp = NULL;
#ifdef	AFS_AIX41_ENV
    struct ucred *credp = crref();	/* don't free until done! */
#endif
#ifdef AFS_LINUX22_ENV
    cred_t *credp = crref();	/* don't free until done! */
    struct dentry *dp;
#endif

    AFS_STATCNT(afs_syscall_pioctl);
    if (follow)
	follow = 1;		/* compat. with old venus */
    code = copyin_afs_ioctl(cmarg, &data);
    if (code) {
	PIOCTL_FREE_CRED();
#if defined(KERNEL_HAVE_UERROR)
	setuerror(code);
#endif
	return (code);
    }
    if ((com & 0xff) == PSetClientContext) {
#ifdef AFS_NEED_CLIENTCONTEXT
#if defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_LINUX22_ENV)
	code = HandleClientContext(&data, &com, &foreigncreds, credp);
#else
	code = HandleClientContext(&data, &com, &foreigncreds, osi_curcred());
#endif
	if (code) {
	    if (foreigncreds) {
		crfree(foreigncreds);
	    }
	    PIOCTL_FREE_CRED();
#if defined(KERNEL_HAVE_UERROR)
	    return (setuerror(code), code);
#else
	    return (code);
#endif
	}
#else /* AFS_NEED_CLIENTCONTEXT */
	return EINVAL;
#endif /* AFS_NEED_CLIENTCONTEXT */
    }
#ifdef AFS_NEED_CLIENTCONTEXT
    if (foreigncreds) {
	/*
	 * We could have done without temporary setting the u.u_cred below
	 * (foreigncreds could be passed as param the pioctl modules)
	 * but calls such as afs_osi_suser() doesn't allow that since it
	 * references u.u_cred directly.  We could, of course, do something
	 * like afs_osi_suser(cred) which, I think, is better since it
	 * generalizes and supports multi cred environments...
	 */
#if defined(AFS_SUN5_ENV) || defined(AFS_LINUX22_ENV)
	tmpcred = credp;
	credp = foreigncreds;
#elif defined(AFS_AIX41_ENV)
	tmpcred = crref();	/* XXX */
	crset(foreigncreds);
#elif defined(AFS_HPUX101_ENV)
	tmpcred = p_cred(u.u_procp);
	set_p_cred(u.u_procp, foreigncreds);
#elif defined(AFS_SGI_ENV)
	tmpcred = OSI_GET_CURRENT_CRED();
	OSI_SET_CURRENT_CRED(foreigncreds);
#else
	tmpcred = u.u_cred;
	u.u_cred = foreigncreds;
#endif
    }
#endif /* AFS_NEED_CLIENTCONTEXT */
    if ((com & 0xff) == 15) {
	/* special case prefetch so entire pathname eval occurs in helper process.
	 * otherwise, the pioctl call is essentially useless */
#if	defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_LINUX22_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	code =
	    Prefetch(path, &data, follow,
		     foreigncreds ? foreigncreds : credp);
#else
	code = Prefetch(path, &data, follow, osi_curcred());
#endif
	vp = NULL;
#if defined(KERNEL_HAVE_UERROR)
	setuerror(code);
#endif
	goto rescred;
    }
    if (path) {
	AFS_GUNLOCK();
#ifdef	AFS_AIX41_ENV
	code =
	    lookupname(path, USR, follow, NULL, &vp,
		       foreigncreds ? foreigncreds : credp);
#else
#ifdef AFS_LINUX22_ENV
	code = gop_lookupname_user(path, AFS_UIOUSER, follow, &dp);
	if (!code)
	    vp = (struct vnode *)dp->d_inode;
#else
	code = gop_lookupname_user(path, AFS_UIOUSER, follow, &vp);
#endif /* AFS_LINUX22_ENV */
#endif /* AFS_AIX41_ENV */
	AFS_GLOCK();
	if (code) {
	    vp = NULL;
#if defined(KERNEL_HAVE_UERROR)
	    setuerror(code);
#endif
	    goto rescred;
	}
    } else
	vp = NULL;

#if defined(AFS_SUN510_ENV)
    if (vp && !IsAfsVnode(vp)) {
	struct vnode *realvp;
	if
#ifdef AFS_SUN511_ENV
          (VOP_REALVP(vp, &realvp, NULL) == 0)
#else
	  (VOP_REALVP(vp, &realvp) == 0)
#endif
{
	    struct vnode *oldvp = vp;

	    VN_HOLD(realvp);
	    vp = realvp;
	    AFS_RELE(oldvp);
	}
    }
#endif
    /* now make the call if we were passed no file, or were passed an AFS file */
    if (!vp || IsAfsVnode(vp)) {
#if defined(AFS_SUN5_ENV)
	code = afs_HandlePioctl(vp, com, &data, follow, &credp);
#elif defined(AFS_AIX41_ENV)
	{
	    struct ucred *cred1, *cred2;

	    if (foreigncreds) {
		cred1 = cred2 = foreigncreds;
	    } else {
		cred1 = cred2 = credp;
	    }
	    code = afs_HandlePioctl(vp, com, &data, follow, &cred1);
	    if (cred1 != cred2) {
		/* something changed the creds */
		crset(cred1);
	    }
	}
#elif defined(AFS_HPUX101_ENV)
	{
	    struct ucred *cred = p_cred(u.u_procp);
	    code = afs_HandlePioctl(vp, com, &data, follow, &cred);
	}
#elif defined(AFS_SGI_ENV)
	{
	    struct cred *credp;
	    credp = OSI_GET_CURRENT_CRED();
	    code = afs_HandlePioctl(vp, com, &data, follow, &credp);
	}
#elif defined(AFS_LINUX22_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	code = afs_HandlePioctl(vp, com, &data, follow, &credp);
#elif defined(UKERNEL)
	code = afs_HandlePioctl(vp, com, &data, follow,
				&(get_user_struct()->u_cred));
#else
	code = afs_HandlePioctl(vp, com, &data, follow, &u.u_cred);
#endif
    } else {
#if defined(KERNEL_HAVE_UERROR)
	setuerror(EINVAL);
#else
	code = EINVAL;		/* not in /afs */
#endif
    }

  rescred:
#if defined(AFS_NEED_CLIENTCONTEXT)
    if (foreigncreds) {
#ifdef	AFS_AIX41_ENV
	crset(tmpcred);		/* restore original credentials */
#else
#if	defined(AFS_HPUX101_ENV)
	set_p_cred(u.u_procp, tmpcred);	/* restore original credentials */
#elif	defined(AFS_SGI_ENV)
	OSI_SET_CURRENT_CRED(tmpcred);	/* restore original credentials */
#elif	defined(AFS_SUN5_ENV) || defined(AFS_LINUX22_ENV)
	credp = tmpcred;		/* restore original credentials */
#else
	osi_curcred() = tmpcred;	/* restore original credentials */
#endif /* AFS_HPUX101_ENV */
	crfree(foreigncreds);
#endif /* AIX41 */
    }
#endif /* AFS_NEED_CLIENTCONTEXT */
    if (vp) {
#ifdef AFS_LINUX22_ENV
	/*
	 * Holding the global lock when calling dput can cause a deadlock
	 * when the kernel calls back into afs_dentry_iput
	 */
	AFS_GUNLOCK();
	dput(dp);
	AFS_GLOCK();
#else
#if defined(AFS_FBSD80_ENV)
    if (VOP_ISLOCKED(vp))
	VOP_UNLOCK(vp, 0);
#endif /* AFS_FBSD80_ENV */
	AFS_RELE(vp);		/* put vnode back */
#endif
    }
    PIOCTL_FREE_CRED();
#if defined(KERNEL_HAVE_UERROR)
    if (!getuerror())
	setuerror(code);
    return (getuerror());
#else
    return (code);
#endif
}

#ifdef AFS_DARWIN100_ENV
int
afs_syscall_pioctl(char * path, unsigned int com, caddr_t cmarg,
		   int follow, afs_ucred_t *credp)
{
    return afs_syscall64_pioctl(CAST_USER_ADDR_T(path), com,
				CAST_USER_ADDR_T((unsigned int)cmarg), follow,
				credp);
}
#endif

#define MAXPIOCTLTOKENLEN \
(3*sizeof(afs_int32)+MAXKTCTICKETLEN+sizeof(struct ClearToken)+MAXKTCREALMLEN)

int
afs_HandlePioctl(struct vnode *avp, afs_int32 acom,
		 struct afs_ioctl *ablob, int afollow,
		 afs_ucred_t **acred)
{
    struct vcache *avc;
    struct vrequest *treq = NULL;
    afs_int32 code;
    afs_int32 function, device;
    struct afs_pdata input, output;
    struct afs_pdata copyInput, copyOutput;
    size_t outSize;
    pioctlFunction *pioctlSw;
    int pioctlSwSize;
    struct afs_fakestat_state fakestate;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));

    avc = avp ? VTOAFS(avp) : NULL;
    afs_Trace3(afs_iclSetp, CM_TRACE_PIOCTL, ICL_TYPE_INT32, acom & 0xff,
	       ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, afollow);
    AFS_STATCNT(HandlePioctl);

    code = afs_CreateReq(&treq, *acred);
    if (code)
	return code;

    afs_InitFakeStat(&fakestate);
    if (avc) {
	code = afs_EvalFakeStat(&avc, &fakestate, treq);
	if (code)
	    goto out;
    }
    device = (acom & 0xff00) >> 8;
    switch (device) {
    case 'V':			/* Original pioctls */
	pioctlSw = VpioctlSw;
	pioctlSwSize = sizeof(VpioctlSw);
	break;
    case 'C':			/* Coordinated/common pioctls */
	pioctlSw = CpioctlSw;
	pioctlSwSize = sizeof(CpioctlSw);
	break;
    case 'O':			/* Coordinated/common pioctls */
	pioctlSw = OpioctlSw;
	pioctlSwSize = sizeof(OpioctlSw);
	break;
    default:
	code = EINVAL;
	goto out;
    }
    function = acom & 0xff;
    if (function >= (pioctlSwSize / sizeof(char *))) {
	code = EINVAL;
	goto out;
    }

    /* Do all range checking before continuing */
    if (ablob->in_size > MAXPIOCTLTOKENLEN ||
        ablob->in_size < 0 || ablob->out_size < 0) {
	code = EINVAL;
	goto out;
    }

    code = afs_pd_alloc(&input, ablob->in_size);
    if (code)
	goto out;

    if (ablob->in_size > 0) {
	AFS_COPYIN(ablob->in, input.ptr, ablob->in_size, code);
	input.ptr[input.remaining] = '\0';
    }
    if (code)
	goto out;

    if (function == 8 && device == 'V') {	/* PGetTokens */
	code = afs_pd_alloc(&output, MAXPIOCTLTOKENLEN);
    } else {
	code = afs_pd_alloc(&output, AFS_LRALLOCSIZ);
    }
    if (code)
	goto out;

    copyInput = input;
    copyOutput = output;

    code =
	(*pioctlSw[function]) (avc, function, treq, &copyInput,
			       &copyOutput, acred);

    outSize = copyOutput.ptr - output.ptr;

    if (code == 0 && ablob->out_size > 0) {
	if (outSize > ablob->out_size) {
	    code = E2BIG;	/* data wont fit in user buffer */
	} else if (outSize) {
	    AFS_COPYOUT(output.ptr, ablob->out, outSize, code);
	}
    }

out:
    afs_pd_free(&input);
    afs_pd_free(&output);

    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, treq, 41);
    afs_DestroyReq(treq);
    return code;
}

/*!
 * VIOCGETFID (22) - Get file ID quickly
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	fid of requested file
 *
 * \retval EINVAL	Error if some of the initial arguments aren't set
 *
 * \post get the file id of some file
 */
DECL_PIOCTL(PGetFID)
{
    AFS_STATCNT(PGetFID);
    if (!avc)
	return EINVAL;
    if (afs_pd_putBytes(aout, &avc->f.fid, sizeof(struct VenusFid)) != 0)
	return EINVAL;
    return 0;
}

/*!
 * VIOCSETAL (1) - Set access control list
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the ACL being set
 * \param[out] aout	the ACL being set returned
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 *
 * \post Changed ACL, via direct writing to the wire
 */
int
dummy_PSetAcl(char *ain, char *aout)
{
    return 0;
}

DECL_PIOCTL(PSetAcl)
{
    afs_int32 code;
    struct afs_conn *tconn;
    struct AFSOpaque acl;
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    struct rx_connection *rxconn;
    XSTATS_DECLS;

    AFS_STATCNT(PSetAcl);
    if (!avc)
	return EINVAL;

    if (afs_pd_getStringPtr(ain, &acl.AFSOpaque_val) != 0)
	return EINVAL;
    acl.AFSOpaque_len = strlen(acl.AFSOpaque_val) + 1;
    if (acl.AFSOpaque_len > 1024)
	return EINVAL;

    do {
	tconn = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tconn) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STOREACL);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_StoreACL(rxconn, (struct AFSFid *)&avc->f.fid.Fid,
			       &acl, &OutStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tconn, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_STOREACL,
	      SHARED_LOCK, NULL));

    /* now we've forgotten all of the access info */
    ObtainWriteLock(&afs_xcbhash, 455);
    avc->callback = 0;
    afs_DequeueCallback(avc);
    avc->f.states &= ~(CStatd | CUnique);
    ReleaseWriteLock(&afs_xcbhash);
    if (avc->f.fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	osi_dnlc_purgedp(avc);

    /* SXW - Should we flush metadata here? */
    return code;
}

int afs_defaultAsynchrony = 0;

/*!
 * VIOC_STOREBEHIND (47) Adjust store asynchrony
 *
 * \ingroup pioctl
 *
 * \param[in] ain	sbstruct (store behind structure) input
 * \param[out] aout	resulting sbstruct
 *
 * \retval EPERM
 * 	Error if the user doesn't have super-user credentials
 * \retval EACCES
 * 	Error if there isn't enough access to not check the mode bits
 *
 * \post
 * 	Changes either the default asynchrony (the amount of data that
 * 	can remain to be written when the cache manager returns control
 * 	to the user), or the asyncrony for the specified file.
 */
DECL_PIOCTL(PStoreBehind)
{
    struct sbstruct sbr;

    if (afs_pd_getBytes(ain, &sbr, sizeof(struct sbstruct)) != 0)
	return EINVAL;

    if (sbr.sb_default != -1) {
	if (afs_osi_suser(*acred))
	    afs_defaultAsynchrony = sbr.sb_default;
	else
	    return EPERM;
    }

    if (avc && (sbr.sb_thisfile != -1)) {
	if (afs_AccessOK
	    (avc, PRSFS_WRITE | PRSFS_ADMINISTER, areq, DONT_CHECK_MODE_BITS))
	    avc->asynchrony = sbr.sb_thisfile;
	else
	    return EACCES;
    }

    memset(&sbr, 0, sizeof(sbr));
    sbr.sb_default = afs_defaultAsynchrony;
    if (avc) {
	sbr.sb_thisfile = avc->asynchrony;
    }

    return afs_pd_putBytes(aout, &sbr, sizeof(sbr));
}

/*!
 * VIOC_GCPAGS (48) - Disable automatic PAG gc'ing
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	not in use
 *
 * \retval EACCES	Error if the user doesn't have super-user credentials
 *
 * \post set the gcpags to GCPAGS_USERDISABLED
 */
DECL_PIOCTL(PGCPAGs)
{
    if (!afs_osi_suser(*acred)) {
	return EACCES;
    }
    afs_gcpags = AFS_GCPAGS_USERDISABLED;
    return 0;
}

/*!
 * VIOCGETAL (2) - Get access control list
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	the ACL
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 * \retval ERANGE	Error if the vnode of the file id is too large
 * \retval -1		Error if getting the ACL failed
 *
 * \post Obtain the ACL, based on file ID
 *
 * \notes
 * 	There is a hack to tell which type of ACL is being returned, checks
 * 	the top 2-bytes of the input size to judge what type of ACL it is,
 * 	only for dfs xlator ACLs
 */
DECL_PIOCTL(PGetAcl)
{
    struct AFSOpaque acl;
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    afs_int32 code;
    struct afs_conn *tconn;
    struct AFSFid Fid;
    struct rx_connection *rxconn;
    XSTATS_DECLS;

    AFS_STATCNT(PGetAcl);
    if (!avc)
	return EINVAL;
    Fid.Volume = avc->f.fid.Fid.Volume;
    Fid.Vnode = avc->f.fid.Fid.Vnode;
    Fid.Unique = avc->f.fid.Fid.Unique;
    if (avc->f.states & CForeign) {
	/*
	 * For a dfs xlator acl we have a special hack so that the
	 * xlator will distinguish which type of acl will return. So
	 * we currently use the top 2-bytes (vals 0-4) to tell which
	 * type of acl to bring back. Horrible hack but this will
	 * cause the least number of changes to code size and interfaces.
	 */
	if (Fid.Vnode & 0xc0000000)
	    return ERANGE;
	Fid.Vnode |= (ain->remaining << 30);
    }
    acl.AFSOpaque_val = aout->ptr;
    do {
	tconn = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tconn) {
	    acl.AFSOpaque_val[0] = '\0';
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHACL);
	    RX_AFS_GUNLOCK();
	    code = RXAFS_FetchACL(rxconn, &Fid, &acl, &OutStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tconn, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_FETCHACL,
	      SHARED_LOCK, NULL));

    if (code == 0) {
	if (acl.AFSOpaque_len == 0)
	    afs_pd_skip(aout, 1); /* leave the NULL */
	else
	    afs_pd_skip(aout, acl.AFSOpaque_len); /* Length of the ACL */
    }
    return code;
}

/*!
 * PNoop returns success.  Used for functions which are not implemented
 * or are no longer in use.
 *
 * \ingroup pioctl
 *
 * \retval Always returns success
 *
 * \notes
 * 	Functions involved in this:
 * 	17 (VIOCENGROUP) -- used to be enable group;
 * 	18 (VIOCDISGROUP) -- used to be disable group;
 * 	2 (?) -- get/set cache-bypass size threshold
 */
DECL_PIOCTL(PNoop)
{
    AFS_STATCNT(PNoop);
    return 0;
}

/*!
 * PBogus returns fail.  Used for functions which are not implemented or
 * are no longer in use.
 *
 * \ingroup pioctl
 *
 * \retval EINVAL	Always returns this value
 *
 * \notes
 * 	Functions involved in this:
 *	0 (?);
 *	4 (?);
 *	6 (?);
 *	7 (VIOCSTAT);
 *	8 (?);
 *	13 (VIOCGETTIME) -- used to be quick check time;
 *	15 (VIOCPREFETCH) -- prefetch is now special-cased; see pioctl code!;
 *	16 (VIOCNOP) -- used to be testing code;
 *	19 (VIOCLISTGROUPS) -- used to be list group;
 *	23 (VIOCWAITFOREVER) -- used to be waitforever;
 *	57 (VIOC_FPRIOSTATUS) -- arla: set file prio;
 *	58 (VIOC_FHGET) -- arla: fallback getfh;
 *	59 (VIOC_FHOPEN) -- arla: fallback fhopen;
 *	60 (VIOC_XFSDEBUG) -- arla: controls xfsdebug;
 *	61 (VIOC_ARLADEBUG) -- arla: controls arla debug;
 *	62 (VIOC_AVIATOR) -- arla: debug interface;
 *	63 (VIOC_XFSDEBUG_PRINT) -- arla: print xfs status;
 *	64 (VIOC_CALCULATE_CACHE) -- arla: force cache check;
 *	65 (VIOC_BREAKCELLBACK) -- arla: break callback;
 *	68 (?) -- arla: fetch stats;
 */
DECL_PIOCTL(PBogus)
{
    AFS_STATCNT(PBogus);
    return EINVAL;
}

/*!
 * VIOC_FILE_CELL_NAME (30) - Get cell in which file lives
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use (avc used to pass in file id)
 * \param[out] aout	cell name
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 * \retval ESRCH	Error if the file isn't part of a cell
 *
 * \post Get a cell based on a passed in file id
 */
DECL_PIOCTL(PGetFileCell)
{
    struct cell *tcell;

    AFS_STATCNT(PGetFileCell);
    if (!avc)
	return EINVAL;
    tcell = afs_GetCell(avc->f.fid.Cell, READ_LOCK);
    if (!tcell)
	return ESRCH;

    if (afs_pd_putString(aout, tcell->cellName) != 0)
	return EINVAL;

    afs_PutCell(tcell, READ_LOCK);
    return 0;
}

/*!
 * VIOC_GET_WS_CELL (31) - Get cell in which workstation lives
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	cell name
 *
 * \retval EIO
 * 	Error if the afs daemon hasn't started yet
 * \retval ESRCH
 * 	Error if the machine isn't part of a cell, for whatever reason
 *
 * \post Get the primary cell that the machine is a part of.
 */
DECL_PIOCTL(PGetWSCell)
{
    struct cell *tcell = NULL;

    AFS_STATCNT(PGetWSCell);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    tcell = afs_GetPrimaryCell(READ_LOCK);
    if (!tcell)			/* no primary cell? */
	return ESRCH;

    if (afs_pd_putString(aout, tcell->cellName) != 0)
	return EINVAL;
    afs_PutCell(tcell, READ_LOCK);
    return 0;
}

/*!
 * VIOC_GET_PRIMARY_CELL (33) - Get primary cell for caller
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use (user id found via areq)
 * \param[out] aout	cell name
 *
 * \retval ESRCH
 * 	Error if the user id doesn't have a primary cell specified
 *
 * \post Get the primary cell for a certain user, based on the user's uid
 */
DECL_PIOCTL(PGetUserCell)
{
    afs_int32 i;
    struct unixuser *tu;
    struct cell *tcell;

    AFS_STATCNT(PGetUserCell);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    /* return the cell name of the primary cell for this user */
    i = UHash(areq->uid);
    ObtainWriteLock(&afs_xuser, 224);
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (tu->uid == areq->uid && (tu->states & UPrimary)) {
	    tu->refCount++;
	    ReleaseWriteLock(&afs_xuser);
	    break;
	}
    }
    if (tu) {
	tcell = afs_GetCell(tu->cell, READ_LOCK);
	afs_PutUser(tu, WRITE_LOCK);
	if (!tcell)
	    return ESRCH;
	else {
	    if (afs_pd_putString(aout, tcell->cellName) != 0)
		return E2BIG;
	    afs_PutCell(tcell, READ_LOCK);
	}
    } else {
	ReleaseWriteLock(&afs_xuser);
    }
    return 0;
}

/*!
 * VIOCSETTOK (3) - Set authentication tokens
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the krb tickets from which to set the afs tokens
 * \param[out] aout	not in use
 *
 * \retval EINVAL
 * 	Error if the ticket is either too long or too short
 * \retval EIO
 * 	Error if the AFS initState is below 101
 * \retval ESRCH
 * 	Error if the cell for which the Token is being set can't be found
 *
 * \post
 * 	Set the Tokens for a specific cell name, unless there is none set,
 * 	then default to primary
 *
 */
DECL_PIOCTL(PSetTokens)
{
    afs_int32 i;
    struct unixuser *tu;
    struct ClearToken clear;
    struct cell *tcell;
    char *stp, *stpNew;
    char *cellName;
    int stLen, stLenOld;
    struct vrequest *treq = NULL;
    afs_int32 flag, set_parent_pag = 0;
    int code;

    AFS_STATCNT(PSetTokens);
    if (!afs_resourceinit_flag) {
	return EIO;
    }

    if (afs_pd_getInt(ain, &stLen) != 0)
	return EINVAL;

    stp = afs_pd_where(ain);	/* remember where the ticket is */
    if (stLen < 0 || stLen > MAXKTCTICKETLEN)
	return EINVAL;		/* malloc may fail */
    if (afs_pd_skip(ain, stLen) != 0)
	return EINVAL;

    if (afs_pd_getInt(ain, &i) != 0)
	return EINVAL;
    if (i != sizeof(struct ClearToken))
	return EINVAL;

    if (afs_pd_getBytes(ain, &clear, sizeof(struct ClearToken)) !=0)
	return EINVAL;

    if (clear.AuthHandle == -1)
	clear.AuthHandle = 999;	/* more rxvab compat stuff */

    if (afs_pd_remaining(ain) != 0) {
	/* still stuff left?  we've got primary flag and cell name.
	 * Set these */

	if (afs_pd_getInt(ain, &flag) != 0)
	    return EINVAL;

	/* some versions of gcc appear to need != 0 in order to get this
	 * right */
	if ((flag & 0x8000) != 0) {	/* XXX Use Constant XXX */
	    flag &= ~0x8000;
	    set_parent_pag = 1;
	}

	if (afs_pd_getStringPtr(ain, &cellName) != 0)
	    return EINVAL;

	/* rest is cell name, look it up */
	tcell = afs_GetCellByName(cellName, READ_LOCK);
	if (!tcell)
	    goto nocell;
    } else {
	/* default to primary cell, primary id */
	flag = 1;		/* primary id */
	tcell = afs_GetPrimaryCell(READ_LOCK);
	if (!tcell)
	    goto nocell;
    }
    i = tcell->cellNum;
    afs_PutCell(tcell, READ_LOCK);
    if (set_parent_pag) {
	afs_uint32 pag;
#if defined(AFS_LINUX26_ENV)
	afs_ucred_t *old_cred = *acred;
#endif
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	char procname[256];
	osi_procname(procname, 256);
	afs_warnuser("Process %d (%s) tried to change pags in PSetTokens\n",
		     MyPidxx2Pid(MyPidxx), procname);
	if (!setpag(osi_curproc(), acred, -1, &pag, 1)) {
#else
	if (!setpag(acred, -1, &pag, 1)) {
#endif
#if defined(AFS_LINUX26_ENV)
	    /* setpag() may have changed our credentials */
	    *acred = crref();
	    crfree(old_cred);
#endif
	    code = afs_CreateReq(&treq, *acred);
	    if (code) {
		return code;
	    }
	    areq = treq;
	}
    }

    stpNew = (char *)afs_osi_Alloc(stLen);
    if (stpNew == NULL) {
	return ENOMEM;
    }
    memcpy(stpNew, stp, stLen);

    /* now we just set the tokens */
    tu = afs_GetUser(areq->uid, i, WRITE_LOCK);	/* i has the cell # */
    stp = tu->stp;
    stLenOld = tu->stLen;

    tu->vid = clear.ViceId;
    tu->stp = stpNew;
    tu->stLen = stLen;
    tu->ct = clear;
#ifndef AFS_NOSTATS
    afs_stats_cmfullperf.authent.TicketUpdates++;
    afs_ComputePAGStats();
#endif /* AFS_NOSTATS */
    tu->states |= UHasTokens;
    tu->states &= ~UTokensBad;
    afs_SetPrimary(tu, flag);
    tu->tokenTime = osi_Time();
    afs_ResetUserConns(tu);
    afs_NotifyUser(tu, UTokensObtained);
    afs_PutUser(tu, WRITE_LOCK);
    afs_DestroyReq(treq);

    if (stp) {
	afs_osi_Free(stp, stLenOld);
    }

    return 0;

  nocell:
    {
	int t1;
	t1 = afs_initState;
	if (t1 < 101)
	    return EIO;
	else
	    return ESRCH;
    }
}

/*!
 * VIOCGETVOLSTAT (4) - Get volume status
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	status of the volume
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 *
 * \post
 * 	The status of a volume (based on the FID of the volume), or an
 * 	offline message /motd
 */
DECL_PIOCTL(PGetVolumeStatus)
{
    char volName[32];
    char *offLineMsg = afs_osi_Alloc(256);
    char *motd = afs_osi_Alloc(256);
    struct afs_conn *tc;
    afs_int32 code = 0;
    struct AFSFetchVolumeStatus volstat;
    char *Name;
    struct rx_connection *rxconn;
    XSTATS_DECLS;

    osi_Assert(offLineMsg != NULL);
    osi_Assert(motd != NULL);
    AFS_STATCNT(PGetVolumeStatus);
    if (!avc) {
	code = EINVAL;
	goto out;
    }
    Name = volName;
    do {
	tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_GETVOLUMESTATUS);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_GetVolumeStatus(rxconn, avc->f.fid.Fid.Volume, &volstat,
				      &Name, &offLineMsg, &motd);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_GETVOLUMESTATUS,
	      SHARED_LOCK, NULL));

    if (code)
	goto out;
    /* Copy all this junk into msg->im_data, keeping track of the lengths. */
    if (afs_pd_putBytes(aout, &volstat, sizeof(VolumeStatus)) != 0)
	return E2BIG;
    if (afs_pd_putString(aout, volName) != 0)
	return E2BIG;
    if (afs_pd_putString(aout, offLineMsg) != 0)
	return E2BIG;
    if (afs_pd_putString(aout, motd) != 0)
	return E2BIG;
  out:
    afs_osi_Free(offLineMsg, 256);
    afs_osi_Free(motd, 256);
    return code;
}

/*!
 * VIOCSETVOLSTAT (5) - Set volume status
 *
 * \ingroup pioctl
 *
 * \param[in] ain
 * 	values to set the status at, offline message, message of the day,
 * 	volume name, minimum quota, maximum quota
 * \param[out] aout
 * 	status of a volume, offlines messages, minimum quota, maximumm quota
 *
 * \retval EINVAL
 * 	Error if some of the standard args aren't set
 * \retval EROFS
 * 	Error if the volume is read only, or a backup volume
 * \retval ENODEV
 * 	Error if the volume can't be accessed
 * \retval E2BIG
 * 	Error if the volume name, offline message, and motd are too big
 *
 * \post
 * 	Set the status of a volume, including any offline messages,
 * 	a minimum quota, and a maximum quota
 */
DECL_PIOCTL(PSetVolumeStatus)
{
    char *volName;
    char *offLineMsg;
    char *motd;
    struct afs_conn *tc;
    afs_int32 code = 0;
    struct AFSFetchVolumeStatus volstat;
    struct AFSStoreVolumeStatus storeStat;
    struct volume *tvp;
    struct rx_connection *rxconn;
    XSTATS_DECLS;

    AFS_STATCNT(PSetVolumeStatus);
    if (!avc)
	return EINVAL;
    memset(&storeStat, 0, sizeof(storeStat));

    tvp = afs_GetVolume(&avc->f.fid, areq, READ_LOCK);
    if (tvp) {
	if (tvp->states & (VRO | VBackup)) {
	    afs_PutVolume(tvp, READ_LOCK);
	    return EROFS;
	}
	afs_PutVolume(tvp, READ_LOCK);
    } else
	return ENODEV;


    if (afs_pd_getBytes(ain, &volstat, sizeof(AFSFetchVolumeStatus)) != 0)
	return EINVAL;

    if (afs_pd_getStringPtr(ain, &volName) != 0)
	return EINVAL;
    if (strlen(volName) > 32)
        return E2BIG;

    if (afs_pd_getStringPtr(ain, &offLineMsg) != 0)
	return EINVAL;
    if (strlen(offLineMsg) > 256)
	return E2BIG;

    if (afs_pd_getStringPtr(ain, &motd) != 0)
	return EINVAL;
    if (strlen(motd) > 256)
	return E2BIG;

    /* Done reading ... */

    storeStat.Mask = 0;
    if (volstat.MinQuota != -1) {
	storeStat.MinQuota = volstat.MinQuota;
	storeStat.Mask |= AFS_SETMINQUOTA;
    }
    if (volstat.MaxQuota != -1) {
	storeStat.MaxQuota = volstat.MaxQuota;
	storeStat.Mask |= AFS_SETMAXQUOTA;
    }
    do {
	tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_SETVOLUMESTATUS);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_SetVolumeStatus(rxconn, avc->f.fid.Fid.Volume, &storeStat,
				      volName, offLineMsg, motd);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_SETVOLUMESTATUS,
	      SHARED_LOCK, NULL));

    if (code)
	return code;
    /* we are sending parms back to make compat. with prev system.  should
     * change interface later to not ask for current status, just set new
     * status */

    if (afs_pd_putBytes(aout, &volstat, sizeof(VolumeStatus)) != 0)
	return EINVAL;
    if (afs_pd_putString(aout, volName) != 0)
	return EINVAL;
    if (afs_pd_putString(aout, offLineMsg) != 0)
	return EINVAL;
    if (afs_pd_putString(aout, motd) != 0)
	return EINVAL;

    return code;
}

/*!
 * VIOCFLUSH (6) - Invalidate cache entry
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	not in use
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 *
 * \post Flush any information the cache manager has on an entry
 */
DECL_PIOCTL(PFlush)
{
    AFS_STATCNT(PFlush);
    if (!avc)
	return EINVAL;
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonLock(&avc->pvnLock, avc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
    ObtainWriteLock(&avc->lock, 225);
    afs_ResetVCache(avc, *acred, 0);
    ReleaseWriteLock(&avc->lock);
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonUnlock(&avc->pvnLock, avc);
#endif
    return 0;
}

/*!
 * VIOC_AFS_STAT_MT_PT (29) - Stat mount point
 *
 * \ingroup pioctl
 *
 * \param[in] ain
 * 	the last component in a path, related to mountpoint that we're
 * 	looking for information about
 * \param[out] aout
 * 	volume, cell, link data
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 * \retval ENOTDIR	Error if the 'mount point' argument isn't a directory
 * \retval EIO		Error if the link data can't be accessed
 *
 * \post Get the volume, and cell, as well as the link data for a mount point
 */
DECL_PIOCTL(PNewStatMount)
{
    afs_int32 code;
    struct vcache *tvc;
    struct dcache *tdc;
    struct VenusFid tfid;
    char *bufp;
    char *name;
    struct sysname_info sysState;
    afs_size_t offset, len;

    AFS_STATCNT(PNewStatMount);
    if (!avc)
	return EINVAL;

    if (afs_pd_getStringPtr(ain, &name) != 0)
	return EINVAL;

    code = afs_VerifyVCache(avc, areq);
    if (code)
	return code;
    if (vType(avc) != VDIR) {
	return ENOTDIR;
    }
    tdc = afs_GetDCache(avc, (afs_size_t) 0, areq, &offset, &len, 1);
    if (!tdc)
	return ENOENT;
    Check_AtSys(avc, name, &sysState, areq);
    ObtainReadLock(&tdc->lock);
    do {
	code = afs_dir_Lookup(tdc, sysState.name, &tfid.Fid);
    } while (code == ENOENT && Next_AtSys(avc, areq, &sysState));
    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);		/* we're done with the data */
    bufp = sysState.name;
    if (code) {
	goto out;
    }
    tfid.Cell = avc->f.fid.Cell;
    tfid.Fid.Volume = avc->f.fid.Fid.Volume;
    if (!tfid.Fid.Unique && (avc->f.states & CForeign)) {
	tvc = afs_LookupVCache(&tfid, areq, NULL, avc, bufp);
    } else {
	tvc = afs_GetVCache(&tfid, areq, NULL, NULL);
    }
    if (!tvc) {
	code = ENOENT;
	goto out;
    }
    if (tvc->mvstat != 1) {
	afs_PutVCache(tvc);
	code = EINVAL;
	goto out;
    }
    ObtainWriteLock(&tvc->lock, 226);
    code = afs_HandleLink(tvc, areq);
    if (code == 0) {
	if (tvc->linkData) {
	    if ((tvc->linkData[0] != '#') && (tvc->linkData[0] != '%'))
		code = EINVAL;
	    else {
		/* we have the data */
		if (afs_pd_putString(aout, tvc->linkData) != 0)
		    code = EINVAL;
	    }
	} else
	    code = EIO;
    }
    ReleaseWriteLock(&tvc->lock);
    afs_PutVCache(tvc);
  out:
    if (sysState.allocked)
	osi_FreeLargeSpace(bufp);
    return code;
}

/*!
 * VIOCGETTOK (8) - Get authentication tokens
 *
 * \ingroup pioctl
 *
 * \param[in] ain       cellid to return tokens for
 * \param[out] aout     token
 *
 * \retval EIO
 * 	Error if the afs daemon hasn't started yet
 * \retval EDOM
 * 	Error if the input parameter is out of the bounds of the available
 * 	tokens
 * \retval ENOTCONN
 * 	Error if there aren't tokens for this cell
 *
 * \post
 * 	If the input paramater exists, get the token that corresponds to
 * 	the parameter value, if there is no token at this value, get the
 * 	token for the first cell
 *
 * \notes "it's a weird interface (from comments in the code)"
 */

DECL_PIOCTL(PGetTokens)
{
    struct cell *tcell;
    afs_int32 i;
    struct unixuser *tu;
    afs_int32 iterator = 0;
    int newStyle;
    int code = E2BIG;

    AFS_STATCNT(PGetTokens);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    /* weird interface.  If input parameter is present, it is an integer and
     * we're supposed to return the parm'th tokens for this unix uid.
     * If not present, we just return tokens for cell 1.
     * If counter out of bounds, return EDOM.
     * If no tokens for the particular cell, return ENOTCONN.
     * Also, if this mysterious parm is present, we return, along with the
     * tokens, the primary cell indicator (an afs_int32 0) and the cell name
     * at the end, in that order.
     */
    newStyle = (afs_pd_remaining(ain) > 0);
    if (newStyle) {
	if (afs_pd_getInt(ain, &iterator) != 0)
	    return EINVAL;
    }
    i = UHash(areq->uid);
    ObtainReadLock(&afs_xuser);
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (newStyle) {
	    if (tu->uid == areq->uid && (tu->states & UHasTokens)) {
		if (iterator-- == 0)
		    break;	/* are we done yet? */
	    }
	} else {
	    if (tu->uid == areq->uid && afs_IsPrimaryCellNum(tu->cell))
		break;
	}
    }
    if (tu) {
	/*
	 * No need to hold a read lock on each user entry
	 */
	tu->refCount++;
    }
    ReleaseReadLock(&afs_xuser);

    if (!tu) {
	return EDOM;
    }
    if (((tu->states & UHasTokens) == 0)
	|| (tu->ct.EndTimestamp < osi_Time())) {
	tu->states |= (UTokensBad | UNeedsReset);
	afs_NotifyUser(tu, UTokensDropped);
	afs_PutUser(tu, READ_LOCK);
	return ENOTCONN;
    }
    iterator = tu->stLen;	/* for compat, we try to return 56 byte tix if they fit */
    if (iterator < 56)
	iterator = 56;		/* # of bytes we're returning */

    if (afs_pd_putInt(aout, iterator) != 0)
	goto out;
    if (afs_pd_putBytes(aout, tu->stp, tu->stLen) != 0)
	goto out;
    if (tu->stLen < 56) {
	/* Tokens are always 56 bytes or larger */
	if (afs_pd_skip(aout, iterator - tu->stLen) != 0) {
	    goto out;
	}
    }

    if (afs_pd_putInt(aout, sizeof(struct ClearToken)) != 0)
	goto out;
    if (afs_pd_putBytes(aout, &tu->ct, sizeof(struct ClearToken)) != 0)
	goto out;

    if (newStyle) {
	/* put out primary id and cell name, too */
	iterator = (tu->states & UPrimary ? 1 : 0);
	if (afs_pd_putInt(aout, iterator) != 0)
	    goto out;
	tcell = afs_GetCell(tu->cell, READ_LOCK);
	if (tcell) {
	    if (afs_pd_putString(aout, tcell->cellName) != 0)
		goto out;
	    afs_PutCell(tcell, READ_LOCK);
	} else
	    if (afs_pd_putString(aout, "") != 0)
		goto out;
    }
    /* Got here, all is good */
    code = 0;
out:
    afs_PutUser(tu, READ_LOCK);
    return code;
}

/*!
 * VIOCUNLOG (9) - Invalidate tokens
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	not in use
 *
 * \retval EIO	Error if the afs daemon hasn't been started yet
 *
 * \post remove tokens from a user, specified by the user id
 *
 * \notes sets the token's time to 0, which then causes it to be removed
 * \notes Unlog is the same as un-pag in OpenAFS
 */
DECL_PIOCTL(PUnlog)
{
    afs_int32 i;
    struct unixuser *tu;

    AFS_STATCNT(PUnlog);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    i = UHash(areq->uid);
    ObtainWriteLock(&afs_xuser, 227);
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (tu->uid == areq->uid) {
	    tu->vid = UNDEFVID;
	    tu->states &= ~UHasTokens;
	    /* security is not having to say you're sorry */
	    memset(&tu->ct, 0, sizeof(struct ClearToken));
	    tu->refCount++;
	    ReleaseWriteLock(&afs_xuser);
	    afs_NotifyUser(tu, UTokensDropped);
	    /* We have to drop the lock over the call to afs_ResetUserConns,
	     * since it obtains the afs_xvcache lock.  We could also keep
	     * the lock, and modify ResetUserConns to take parm saying we
	     * obtained the lock already, but that is overkill.  By keeping
	     * the "tu" pointer held over the released lock, we guarantee
	     * that we won't lose our place, and that we'll pass over
	     * every user conn that existed when we began this call.
	     */
	    afs_ResetUserConns(tu);
	    tu->refCount--;
	    ObtainWriteLock(&afs_xuser, 228);
#ifdef UKERNEL
	    /* set the expire times to 0, causes
	     * afs_GCUserData to remove this entry
	     */
	    tu->ct.EndTimestamp = 0;
	    tu->tokenTime = 0;
#endif /* UKERNEL */
	}
    }
    ReleaseWriteLock(&afs_xuser);
    return 0;
}

/*!
 * VIOC_AFS_MARINER_HOST (32) - Get/set mariner (cache manager monitor) host
 *
 * \ingroup pioctl
 *
 * \param[in] ain	host address to be set
 * \param[out] aout	old host address
 *
 * \post
 * 	depending on whether or not a variable is set, either get the host
 * 	for the cache manager monitor, or set the old address and give it
 * 	a new address
 *
 * \notes Errors turn off mariner
 */
DECL_PIOCTL(PMariner)
{
    afs_int32 newHostAddr;
    afs_int32 oldHostAddr;

    AFS_STATCNT(PMariner);
    if (afs_mariner)
	memcpy((char *)&oldHostAddr, (char *)&afs_marinerHost,
	       sizeof(afs_int32));
    else
	oldHostAddr = 0xffffffff;	/* disabled */

    if (afs_pd_getInt(ain, &newHostAddr) != 0)
	return EINVAL;

    if (newHostAddr == 0xffffffff) {
	/* disable mariner operations */
	afs_mariner = 0;
    } else if (newHostAddr) {
	afs_mariner = 1;
	afs_marinerHost = newHostAddr;
    }

    if (afs_pd_putInt(aout, oldHostAddr) != 0)
	return E2BIG;

    return 0;
}

/*!
 * VIOCCKSERV (10) - Check that servers are up
 *
 * \ingroup pioctl
 *
 * \param[in] ain	name of the cell
 * \param[out] aout	current down server list
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval EACCES	Error if the user doesn't have super-user credentials
 * \retval ENOENT	Error if we are unable to obtain the cell
 *
 * \post
 * 	Either a fast check (where it doesn't contact servers) or a
 * 	local check (checks local cell only)
 */
DECL_PIOCTL(PCheckServers)
{
    int i;
    struct server *ts;
    afs_int32 temp;
    char *cellName = NULL;
    struct cell *cellp;
    struct chservinfo *pcheck;

    AFS_STATCNT(PCheckServers);

    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    /* This is tricky, because we need to peak at the datastream to see
     * what we're getting. For now, let's cheat. */

    /* ain contains either an int32 or a string */
    if (ain->remaining == 0)
	return EINVAL;

    if (*(afs_int32 *)ain->ptr == 0x12345678) {	/* For afs3.3 version */
	pcheck = afs_pd_inline(ain, sizeof(*pcheck));
        if (pcheck == NULL)
	    return EINVAL;

	if (pcheck->tinterval >= 0) {
	    if (afs_pd_putInt(aout, afs_probe_interval) != 0)
		return E2BIG;
	    if (pcheck->tinterval > 0) {
		if (!afs_osi_suser(*acred))
		    return EACCES;
		afs_probe_interval = pcheck->tinterval;
	    }
	    return 0;
	}
	temp = pcheck->tflags;
	if (pcheck->tsize)
	    cellName = pcheck->tbuffer;
    } else {			/* For pre afs3.3 versions */
	if (afs_pd_getInt(ain, &temp) != 0)
	    return EINVAL;
	if (afs_pd_remaining(ain) > 0) {
	    if (afs_pd_getStringPtr(ain, &cellName) != 0)
		return EINVAL;
	}
    }

    /*
     * 1: fast check, don't contact servers.
     * 2: local cell only.
     */
    if (cellName) {
	/* have cell name, too */
	cellp = afs_GetCellByName(cellName, READ_LOCK);
	if (!cellp)
	    return ENOENT;
    } else
	cellp = NULL;
    if (!cellp && (temp & 2)) {
	/* use local cell */
	cellp = afs_GetPrimaryCell(READ_LOCK);
    }
    if (!(temp & 1)) {		/* if not fast, call server checker routine */
	afs_CheckServers(1, cellp);	/* check down servers */
	afs_CheckServers(0, cellp);	/* check up servers */
    }
    /* now return the current down server list */
    ObtainReadLock(&afs_xserver);
    for (i = 0; i < NSERVERS; i++) {
	for (ts = afs_servers[i]; ts; ts = ts->next) {
	    if (cellp && ts->cell != cellp)
		continue;	/* cell spec'd and wrong */
	    if ((ts->flags & SRVR_ISDOWN)
		&& ts->addr->sa_portal != ts->cell->vlport) {
		afs_pd_putInt(aout, ts->addr->sa_ip);
	    }
	}
    }
    ReleaseReadLock(&afs_xserver);
    if (cellp)
	afs_PutCell(cellp, READ_LOCK);
    return 0;
}

/*!
 * VIOCCKBACK (11) - Check backup volume mappings
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	not in use
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 *
 * \post
 * 	Check the root volume, and then check the names if the volume
 * 	check variable is set to force, has expired, is busy, or if
 * 	the mount points variable is set
 */
DECL_PIOCTL(PCheckVolNames)
{
    AFS_STATCNT(PCheckVolNames);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    afs_CheckRootVolume();
    afs_CheckVolumeNames(AFS_VOLCHECK_FORCE | AFS_VOLCHECK_EXPIRED |
			 AFS_VOLCHECK_BUSY | AFS_VOLCHECK_MTPTS);
    return 0;
}

/*!
 * VIOCCKCONN (12) - Check connections for a user
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	not in use
 *
 * \retval EACCESS
 * 	Error if no user is specififed, the user has no tokens set,
 * 	or if the user's tokens are bad
 *
 * \post
 * 	check to see if a user has the correct authentication.
 * 	If so, allow access.
 *
 * \notes Check the connections to all the servers specified
 */
DECL_PIOCTL(PCheckAuth)
{
    int i;
    struct srvAddr *sa;
    struct afs_conn *tc;
    struct unixuser *tu;
    afs_int32 retValue;

    AFS_STATCNT(PCheckAuth);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    retValue = 0;
    tu = afs_GetUser(areq->uid, 1, READ_LOCK);	/* check local cell authentication */
    if (!tu)
	retValue = EACCES;
    else {
	/* we have a user */
	ObtainReadLock(&afs_xsrvAddr);
	ObtainReadLock(&afs_xconn);

	/* any tokens set? */
	if ((tu->states & UHasTokens) == 0)
	    retValue = EACCES;
	/* all connections in cell 1 working? */
	for (i = 0; i < NSERVERS; i++) {
	    for (sa = afs_srvAddrs[i]; sa; sa = sa->next_bkt) {
		for (tc = sa->conns; tc; tc = tc->next) {
		    if (tc->user == tu && (tu->states & UTokensBad))
			retValue = EACCES;
		}
	    }
	}
	ReleaseReadLock(&afs_xsrvAddr);
	ReleaseReadLock(&afs_xconn);
	afs_PutUser(tu, READ_LOCK);
    }
    if (afs_pd_putInt(aout, retValue) != 0)
	return E2BIG;
    return 0;
}

static int
Prefetch(uparmtype apath, struct afs_ioctl *adata, int afollow,
	 afs_ucred_t *acred)
{
    char *tp;
    afs_int32 code;
#if defined(AFS_SGI61_ENV) || defined(AFS_SUN57_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    size_t bufferSize;
#else
    u_int bufferSize;
#endif

    AFS_STATCNT(Prefetch);
    if (!apath)
	return EINVAL;
    tp = osi_AllocLargeSpace(1024);
    AFS_COPYINSTR(apath, tp, 1024, &bufferSize, code);
    if (code) {
	osi_FreeLargeSpace(tp);
	return code;
    }
    if (afs_BBusy()) {		/* do this as late as possible */
	osi_FreeLargeSpace(tp);
	return EWOULDBLOCK;	/* pretty close */
    }
    afs_BQueue(BOP_PATH, (struct vcache *)0, 0, 0, acred, (afs_size_t) 0,
	       (afs_size_t) 0, tp, (void *)0, (void *)0);
    return 0;
}

/*!
 * VIOCWHEREIS (14) - Find out where a volume is located
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	volume location
 *
 * \retval EINVAL	Error if some of the default arguments don't exist
 * \retval ENODEV	Error if there is no such volume
 *
 * \post fine a volume, based on a volume file id
 *
 * \notes check each of the servers specified
 */
DECL_PIOCTL(PFindVolume)
{
    struct volume *tvp;
    struct server *ts;
    afs_int32 i;
    int code = 0;

    AFS_STATCNT(PFindVolume);
    if (!avc)
	return EINVAL;
    tvp = afs_GetVolume(&avc->f.fid, areq, READ_LOCK);
    if (!tvp)
	return ENODEV;

    for (i = 0; i < AFS_MAXHOSTS; i++) {
	ts = tvp->serverHost[i];
	if (!ts)
	    break;
	if (afs_pd_putInt(aout, ts->addr->sa_ip) != 0) {
	    code = E2BIG;
	    goto out;
	}
    }
    if (i < AFS_MAXHOSTS) {
	/* still room for terminating NULL, add it on */
	if (afs_pd_putInt(aout, 0) != 0) {
	    code = E2BIG;
	    goto out;
	}
    }
out:
    afs_PutVolume(tvp, READ_LOCK);
    return code;
}

/*!
 * VIOCACCESS (20) - Access using PRS_FS bits
 *
 * \ingroup pioctl
 *
 * \param[in] ain	PRS_FS bits
 * \param[out] aout	not in use
 *
 * \retval EINVAL	Error if some of the initial arguments aren't set
 * \retval EACCES	Error if access is denied
 *
 * \post check to make sure access is allowed
 */
DECL_PIOCTL(PViceAccess)
{
    afs_int32 code;
    afs_int32 temp;

    AFS_STATCNT(PViceAccess);
    if (!avc)
	return EINVAL;

    code = afs_VerifyVCache(avc, areq);
    if (code)
	return code;

    if (afs_pd_getInt(ain, &temp) != 0)
	return EINVAL;

    code = afs_AccessOK(avc, temp, areq, CHECK_MODE_BITS);
    if (code)
	return 0;
    else
	return EACCES;
}

/*!
 * VIOC_GETPAG (13) - Get PAG value
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	PAG value or NOPAG
 *
 * \post get PAG value for the caller's cred
 */
DECL_PIOCTL(PGetPAG)
{
    afs_int32 pag;

    pag = PagInCred(*acred);

    return afs_pd_putInt(aout, pag);
}

DECL_PIOCTL(PPrecache)
{
    afs_int32 newValue;

    /*AFS_STATCNT(PPrecache);*/
    if (!afs_osi_suser(*acred))
	return EACCES;

    if (afs_pd_getInt(ain, &newValue) != 0)
	return EINVAL;

    afs_preCache = newValue*1024;
    return 0;
}

/*!
 * VIOCSETCACHESIZE (24) - Set venus cache size in 1000 units
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the size the venus cache should be set to
 * \param[out] aout	not in use
 *
 * \retval EACCES	Error if the user doesn't have super-user credentials
 * \retval EROFS	Error if the cache is set to be in memory
 *
 * \post
 * 	Set the cache size based on user input.  If no size is given,
 * 	set it to the default OpenAFS cache size.
 *
 * \notes
 * 	recompute the general cache parameters for every single block allocated
 */
DECL_PIOCTL(PSetCacheSize)
{
    afs_int32 newValue;
    int waitcnt = 0;

    AFS_STATCNT(PSetCacheSize);

    if (!afs_osi_suser(*acred))
	return EACCES;
    /* too many things are setup initially in mem cache version */
    if (cacheDiskType == AFS_FCACHE_TYPE_MEM)
	return EROFS;
    if (afs_pd_getInt(ain, &newValue) != 0)
	return EINVAL;
    if (newValue == 0)
	afs_cacheBlocks = afs_stats_cmperf.cacheBlocksOrig;
    else {
	if (newValue < afs_min_cache)
	    afs_cacheBlocks = afs_min_cache;
	else
	    afs_cacheBlocks = newValue;
    }
    afs_stats_cmperf.cacheBlocksTotal = afs_cacheBlocks;
    afs_ComputeCacheParms();	/* recompute basic cache parameters */
    afs_MaybeWakeupTruncateDaemon();
    while (waitcnt++ < 100 && afs_cacheBlocks < afs_blocksUsed) {
	afs_osi_Wait(1000, 0, 0);
	afs_MaybeWakeupTruncateDaemon();
    }
    return 0;
}

#define MAXGCSTATS	16
/*!
 * VIOCGETCACHEPARMS (40) - Get cache stats
 *
 * \ingroup pioctl
 *
 * \param[in] ain	afs index flags
 * \param[out] aout	cache blocks, blocks used, blocks files (in an array)
 *
 * \post Get the cache blocks, and how many of the cache blocks there are
 */
DECL_PIOCTL(PGetCacheSize)
{
    afs_int32 results[MAXGCSTATS];
    afs_int32 flags;
    struct dcache * tdc;
    int i, size;

    AFS_STATCNT(PGetCacheSize);

    if (afs_pd_remaining(ain) == sizeof(afs_int32)) {
	afs_pd_getInt(ain, &flags); /* can't error, we just checked size */
    } else if (afs_pd_remaining(ain) == 0) {
	flags = 0;
    } else {
	return EINVAL;
    }

    memset(results, 0, sizeof(results));
    results[0] = afs_cacheBlocks;
    results[1] = afs_blocksUsed;
    results[2] = afs_cacheFiles;

    if (1 == flags){
        for (i = 0; i < afs_cacheFiles; i++) {
	    if (afs_indexFlags[i] & IFFree) results[3]++;
	}
    } else if (2 == flags){
        for (i = 0; i < afs_cacheFiles; i++) {
	    if (afs_indexFlags[i] & IFFree) results[3]++;
	    if (afs_indexFlags[i] & IFEverUsed) results[4]++;
	    if (afs_indexFlags[i] & IFDataMod) results[5]++;
	    if (afs_indexFlags[i] & IFDirtyPages) results[6]++;
	    if (afs_indexFlags[i] & IFAnyPages) results[7]++;
	    if (afs_indexFlags[i] & IFDiscarded) results[8]++;

	    tdc = afs_indexTable[i];
	    if (tdc){
	        results[9]++;
	        size = tdc->validPos;
	        if ( 0 < size && size < (1<<12) ) results[10]++;
    	        else if (size < (1<<14) ) results[11]++;
	        else if (size < (1<<16) ) results[12]++;
	        else if (size < (1<<18) ) results[13]++;
	        else if (size < (1<<20) ) results[14]++;
	        else if (size >= (1<<20) ) results[15]++;
	    }
        }
    }
    return afs_pd_putBytes(aout, results, sizeof(results));
}

/*!
 * VIOCFLUSHCB (25) - Flush callback only
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	not in use
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 * \retval 0		0 returned if the volume is set to read-only
 *
 * \post
 * 	Flushes callbacks, by setting the length of callbacks to one,
 * 	setting the next callback to be sent to the CB_DROPPED value,
 * 	and then dequeues everything else.
 */
DECL_PIOCTL(PRemoveCallBack)
{
    struct afs_conn *tc;
    afs_int32 code = 0;
    struct AFSCallBack CallBacks_Array[1];
    struct AFSCBFids theFids;
    struct AFSCBs theCBs;
    struct rx_connection *rxconn;
    XSTATS_DECLS;

    AFS_STATCNT(PRemoveCallBack);
    if (!avc)
	return EINVAL;
    if (avc->f.states & CRO)
	return 0;		/* read-only-ness can't change */
    ObtainWriteLock(&avc->lock, 229);
    theFids.AFSCBFids_len = 1;
    theCBs.AFSCBs_len = 1;
    theFids.AFSCBFids_val = (struct AFSFid *)&avc->f.fid.Fid;
    theCBs.AFSCBs_val = CallBacks_Array;
    CallBacks_Array[0].CallBackType = CB_DROPPED;
    if (avc->callback) {
	do {
	    tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	    if (tc) {
		XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_GIVEUPCALLBACKS);
		RX_AFS_GUNLOCK();
		code = RXAFS_GiveUpCallBacks(rxconn, &theFids, &theCBs);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	    }
	    /* don't set code on failure since we wouldn't use it */
	} while (afs_Analyze
		 (tc, rxconn, code, &avc->f.fid, areq,
		  AFS_STATS_FS_RPCIDX_GIVEUPCALLBACKS, SHARED_LOCK, NULL));

	ObtainWriteLock(&afs_xcbhash, 457);
	afs_DequeueCallback(avc);
	avc->callback = 0;
	avc->f.states &= ~(CStatd | CUnique);
	ReleaseWriteLock(&afs_xcbhash);
	if (avc->f.fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	    osi_dnlc_purgedp(avc);
    }
    ReleaseWriteLock(&avc->lock);
    return 0;
}

/*!
 * VIOCNEWCELL (26) - Configure new cell
 *
 * \ingroup pioctl
 *
 * \param[in] ain
 * 	the name of the cell, the hosts that will be a part of the cell,
 * 	whether or not it's linked with another cell, the other cell it's
 * 	linked with, the file server port, and the volume server port
 * \param[out] aout
 * 	not in use
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval EACCES	Error if the user doesn't have super-user cedentials
 * \retval EINVAL	Error if some 'magic' var doesn't have a certain bit set
 *
 * \post creates a new cell
 */
DECL_PIOCTL(PNewCell)
{
    afs_int32 cellHosts[AFS_MAXCELLHOSTS], magic = 0;
    char *newcell = NULL;
    char *linkedcell = NULL;
    afs_int32 code, ls;
    afs_int32 linkedstate = 0;
    afs_int32 fsport = 0, vlport = 0;
    int skip;

    AFS_STATCNT(PNewCell);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (!afs_osi_suser(*acred))
	return EACCES;

    if (afs_pd_getInt(ain, &magic) != 0)
	return EINVAL;
    if (magic != 0x12345678)
	return EINVAL;

    /* A 3.4 fs newcell command will pass an array of AFS_MAXCELLHOSTS
     * server addresses while the 3.5 fs newcell command passes
     * AFS_MAXHOSTS. To figure out which is which, check if the cellname
     * is good.
     *
     * This whole logic is bogus, because it relies on the newer command
     * sending its 12th address as 0.
     */
    if ((afs_pd_remaining(ain) < AFS_MAXCELLHOSTS +3) * sizeof(afs_int32))
	return EINVAL;

    newcell = afs_pd_where(ain) + (AFS_MAXCELLHOSTS + 3) * sizeof(afs_int32);
    if (newcell[0] != '\0') {
	skip = 0;
    } else {
	skip = AFS_MAXHOSTS - AFS_MAXCELLHOSTS;
    }

    /* AFS_MAXCELLHOSTS (=8) is less than AFS_MAXHOSTS (=13) */
    if (afs_pd_getBytes(ain, &cellHosts,
			AFS_MAXCELLHOSTS * sizeof(afs_int32)) != 0)
	return EINVAL;
    if (afs_pd_skip(ain, skip * sizeof(afs_int32)) !=0)
	return EINVAL;

    if (afs_pd_getInt(ain, &fsport) != 0)
	return EINVAL;
    if (fsport < 1024)
	fsport = 0;		/* Privileged ports not allowed */

    if (afs_pd_getInt(ain, &vlport) != 0)
	return EINVAL;
    if (vlport < 1024)
	vlport = 0;		/* Privileged ports not allowed */

    if (afs_pd_getInt(ain, &ls) != 0)
	return EINVAL;

    if (afs_pd_getStringPtr(ain, &newcell) != 0)
	return EINVAL;

    if (ls & 1) {
	if (afs_pd_getStringPtr(ain, &linkedcell) != 0)
	    return EINVAL;
	linkedstate |= CLinkedCell;
    }

    linkedstate |= CNoSUID;	/* setuid is disabled by default for fs newcell */
    code =
	afs_NewCell(newcell, cellHosts, linkedstate, linkedcell, fsport,
		    vlport, (int)0);
    return code;
}

DECL_PIOCTL(PNewAlias)
{
    /* create a new cell alias */
    char *realName, *aliasName;

    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (!afs_osi_suser(*acred))
	return EACCES;

    if (afs_pd_getStringPtr(ain, &aliasName) != 0)
	return EINVAL;
    if (afs_pd_getStringPtr(ain, &realName) != 0)
	return EINVAL;

    return afs_NewCellAlias(aliasName, realName);
}

/*!
 * VIOCGETCELL (27) - Get cell info
 *
 * \ingroup pioctl
 *
 * \param[in] ain	The cell index of a specific cell
 * \param[out] aout	list of servers in the cell
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval EDOM		Error if there is no cell asked about
 *
 * \post Lists the cell's server names and and addresses
 */
DECL_PIOCTL(PListCells)
{
    afs_int32 whichCell;
    struct cell *tcell = 0;
    afs_int32 i;
    int code;

    AFS_STATCNT(PListCells);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (afs_pd_getInt(ain, &whichCell) != 0)
	return EINVAL;

    tcell = afs_GetCellByIndex(whichCell, READ_LOCK);
    if (!tcell)
	return EDOM;

    code = E2BIG;

    for (i = 0; i < AFS_MAXCELLHOSTS; i++) {
	if (tcell->cellHosts[i] == 0)
	    break;
	if (afs_pd_putInt(aout, tcell->cellHosts[i]->addr->sa_ip) != 0)
	    goto out;
    }
    for (;i < AFS_MAXCELLHOSTS; i++) {
	if (afs_pd_putInt(aout, 0) != 0)
	    goto out;
    }
    if (afs_pd_putString(aout, tcell->cellName) != 0)
	goto out;
    code = 0;

out:
    afs_PutCell(tcell, READ_LOCK);
    return code;
}

DECL_PIOCTL(PListAliases)
{
    afs_int32 whichAlias;
    struct cell_alias *tcalias = 0;
    int code;

    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (afs_pd_getInt(ain, &whichAlias) != 0)
	return EINVAL;

    tcalias = afs_GetCellAlias(whichAlias);
    if (tcalias == NULL)
	return EDOM;

    code = E2BIG;
    if (afs_pd_putString(aout, tcalias->alias) != 0)
	goto out;
    if (afs_pd_putString(aout, tcalias->cell) != 0)
	goto out;

    code = 0;
out:
    afs_PutCellAlias(tcalias);
    return code;
}

/*!
 * VIOC_AFS_DELETE_MT_PT (28) - Delete mount point
 *
 * \ingroup pioctl
 *
 * \param[in] ain 	the name of the file in this dir to remove
 * \param[out] aout	not in use
 *
 * \retval EINVAL
 * 	Error if some of the standard args aren't set
 * \retval ENOTDIR
 * 	Error if the argument to remove is not a directory
 * \retval ENOENT
 * 	Error if there is no cache to remove the mount point from or
 * 	if a vcache doesn't exist
 *
 * \post
 * 	Ensure that everything is OK before deleting the mountpoint.
 * 	If not, don't delete.  Delete a mount point based on a file id.
 */
DECL_PIOCTL(PRemoveMount)
{
    afs_int32 code;
    char *bufp;
    char *name;
    struct sysname_info sysState;
    afs_size_t offset, len;
    struct afs_conn *tc;
    struct dcache *tdc;
    struct vcache *tvc;
    struct AFSFetchStatus OutDirStatus;
    struct VenusFid tfid;
    struct AFSVolSync tsync;
    struct rx_connection *rxconn;
    XSTATS_DECLS;

    /* "ain" is the name of the file in this dir to remove */

    AFS_STATCNT(PRemoveMount);
    if (!avc)
	return EINVAL;
    if (afs_pd_getStringPtr(ain, &name) != 0)
	return EINVAL;

    code = afs_VerifyVCache(avc, areq);
    if (code)
	return code;
    if (vType(avc) != VDIR)
	return ENOTDIR;

    tdc = afs_GetDCache(avc, (afs_size_t) 0, areq, &offset, &len, 1);	/* test for error below */
    if (!tdc)
	return ENOENT;
    Check_AtSys(avc, name, &sysState, areq);
    ObtainReadLock(&tdc->lock);
    do {
	code = afs_dir_Lookup(tdc, sysState.name, &tfid.Fid);
    } while (code == ENOENT && Next_AtSys(avc, areq, &sysState));
    ReleaseReadLock(&tdc->lock);
    bufp = sysState.name;
    if (code) {
	afs_PutDCache(tdc);
	goto out;
    }
    tfid.Cell = avc->f.fid.Cell;
    tfid.Fid.Volume = avc->f.fid.Fid.Volume;
    if (!tfid.Fid.Unique && (avc->f.states & CForeign)) {
	tvc = afs_LookupVCache(&tfid, areq, NULL, avc, bufp);
    } else {
	tvc = afs_GetVCache(&tfid, areq, NULL, NULL);
    }
    if (!tvc) {
	code = ENOENT;
	afs_PutDCache(tdc);
	goto out;
    }
    if (tvc->mvstat != 1) {
	afs_PutDCache(tdc);
	afs_PutVCache(tvc);
	code = EINVAL;
	goto out;
    }
    ObtainWriteLock(&tvc->lock, 230);
    code = afs_HandleLink(tvc, areq);
    if (!code) {
	if (tvc->linkData) {
	    if ((tvc->linkData[0] != '#') && (tvc->linkData[0] != '%'))
		code = EINVAL;
	} else
	    code = EIO;
    }
    ReleaseWriteLock(&tvc->lock);
    osi_dnlc_purgedp(tvc);
    afs_PutVCache(tvc);
    if (code) {
	afs_PutDCache(tdc);
	goto out;
    }
    ObtainWriteLock(&avc->lock, 231);
    osi_dnlc_remove(avc, bufp, tvc);
    do {
	tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEFILE);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_RemoveFile(rxconn, (struct AFSFid *)&avc->f.fid.Fid, bufp,
				 &OutDirStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_REMOVEFILE,
	      SHARED_LOCK, NULL));

    if (code) {
	if (tdc)
	    afs_PutDCache(tdc);
	ReleaseWriteLock(&avc->lock);
	goto out;
    }
    if (tdc) {
	/* we have the thing in the cache */
	ObtainWriteLock(&tdc->lock, 661);
	if (afs_LocalHero(avc, tdc, &OutDirStatus, 1)) {
	    /* we can do it locally */
	    code = afs_dir_Delete(tdc, bufp);
	    if (code) {
		ZapDCE(tdc);	/* surprise error -- invalid value */
		DZap(tdc);
	    }
	}
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);	/* drop ref count */
    }
    avc->f.states &= ~CUnique;	/* For the dfs xlator */
    ReleaseWriteLock(&avc->lock);
    code = 0;
  out:
    if (sysState.allocked)
	osi_FreeLargeSpace(bufp);
    return code;
}

/*!
 * VIOC_GETCELLSTATUS (35) - Get cell status info
 *
 * \ingroup pioctl
 *
 * \param[in] ain	The cell you want status information on
 * \param[out] aout	cell state (as a struct)
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval ENOENT	Error if the cell doesn't exist
 *
 * \post Returns the state of the cell as defined in a struct cell
 */
DECL_PIOCTL(PGetCellStatus)
{
    struct cell *tcell;
    char *cellName;
    afs_int32 temp;

    AFS_STATCNT(PGetCellStatus);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (afs_pd_getStringPtr(ain, &cellName) != 0)
	return EINVAL;

    tcell = afs_GetCellByName(cellName, READ_LOCK);
    if (!tcell)
	return ENOENT;
    temp = tcell->states;
    afs_PutCell(tcell, READ_LOCK);

    return afs_pd_putInt(aout, temp);
}

/*!
 * VIOC_SETCELLSTATUS (36) - Set corresponding info
 *
 * \ingroup pioctl
 *
 * \param[in] ain
 * 	The cell you want to set information about, and the values you
 * 	want to set
 * \param[out] aout
 * 	not in use
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval EACCES	Error if the user doesn't have super-user credentials
 *
 * \post
 * 	Set the state of the cell in a defined struct cell, based on
 * 	whether or not SetUID is allowed
 */
DECL_PIOCTL(PSetCellStatus)
{
    struct cell *tcell;
    char *cellName;
    afs_int32 flags0, flags1;

    if (!afs_osi_suser(*acred))
	return EACCES;
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (afs_pd_getInt(ain, &flags0) != 0)
	return EINVAL;
    if (afs_pd_getInt(ain, &flags1) != 0)
	return EINVAL;
    if (afs_pd_getStringPtr(ain, &cellName) != 0)
	return EINVAL;

    tcell = afs_GetCellByName(cellName, WRITE_LOCK);
    if (!tcell)
	return ENOENT;
    if (flags0 & CNoSUID)
	tcell->states |= CNoSUID;
    else
	tcell->states &= ~CNoSUID;
    afs_PutCell(tcell, WRITE_LOCK);
    return 0;
}

static void
FlushVolumeData(struct VenusFid *afid, afs_ucred_t * acred)
{
    afs_int32 i;
    struct dcache *tdc;
    struct vcache *tvc;
    struct volume *tv;
    afs_int32 all = 0;
    afs_int32 cell = 0;
    afs_int32 volume = 0;
    struct afs_q *tq, *uq;
#ifdef AFS_DARWIN80_ENV
    vnode_t vp;
#endif

    if (!afid) {
	all = 1;
    } else {
	volume = afid->Fid.Volume;	/* who to zap */
	cell = afid->Cell;
    }

    /*
     * Clear stat'd flag from all vnodes from this volume; this will
     * invalidate all the vcaches associated with the volume.
     */
 loop:
    ObtainReadLock(&afs_xvcache);
    for (i = (afid ? VCHashV(afid) : 0); i < VCSIZE; i = (afid ? VCSIZE : i+1)) {
	for (tq = afs_vhashTV[i].prev; tq != &afs_vhashTV[i]; tq = uq) {
	    uq = QPrev(tq);
	    tvc = QTOVH(tq);
	    if (all || (tvc->f.fid.Fid.Volume == volume && tvc->f.fid.Cell == cell)) {
		if (tvc->f.states & CVInit) {
		    ReleaseReadLock(&afs_xvcache);
		    afs_osi_Sleep(&tvc->f.states);
		    goto loop;
		}
#ifdef AFS_DARWIN80_ENV
		if (tvc->f.states & CDeadVnode) {
		    ReleaseReadLock(&afs_xvcache);
		    afs_osi_Sleep(&tvc->f.states);
		    goto loop;
		}
		vp = AFSTOV(tvc);
		if (vnode_get(vp))
		    continue;
		if (vnode_ref(vp)) {
		    AFS_GUNLOCK();
		    vnode_put(vp);
		    AFS_GLOCK();
		    continue;
		}
#else
		AFS_FAST_HOLD(tvc);
#endif
		ReleaseReadLock(&afs_xvcache);
#ifdef AFS_BOZONLOCK_ENV
		afs_BozonLock(&tvc->pvnLock, tvc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
		ObtainWriteLock(&tvc->lock, 232);
		afs_ResetVCache(tvc, acred, 1);
		ReleaseWriteLock(&tvc->lock);
#ifdef AFS_BOZONLOCK_ENV
		afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
#ifdef AFS_DARWIN80_ENV
		vnode_put(AFSTOV(tvc));
#endif
		ObtainReadLock(&afs_xvcache);
		uq = QPrev(tq);
		/* our tvc ptr is still good until now */
		AFS_FAST_RELE(tvc);
	    }
	}
    }
    ReleaseReadLock(&afs_xvcache);


    ObtainWriteLock(&afs_xdcache, 328);	/* needed to flush any stuff */
    for (i = 0; i < afs_cacheFiles; i++) {
	if (!(afs_indexFlags[i] & IFEverUsed))
	    continue;		/* never had any data */
	tdc = afs_GetValidDSlot(i);
	if (!tdc) {
	    continue;
	}
	if (tdc->refCount <= 1) {    /* too high, in use by running sys call */
	    ReleaseReadLock(&tdc->tlock);
	    if (all || (tdc->f.fid.Fid.Volume == volume && tdc->f.fid.Cell == cell)) {
		if (!(afs_indexFlags[i] & (IFDataMod | IFFree | IFDiscarded))) {
		    /* if the file is modified, but has a ref cnt of only 1,
		     * then someone probably has the file open and is writing
		     * into it. Better to skip flushing such a file, it will be
		     * brought back immediately on the next write anyway.
		     *
		     * Skip if already freed.
		     *
		     * If we *must* flush, then this code has to be rearranged
		     * to call afs_storeAllSegments() first */
		    afs_FlushDCache(tdc);
		}
	    }
	} else {
	    ReleaseReadLock(&tdc->tlock);
	}
	afs_PutDCache(tdc);	/* bumped by getdslot */
    }
    ReleaseWriteLock(&afs_xdcache);

    ObtainReadLock(&afs_xvolume);
    for (i = all ? 0 : VHash(volume); i < NVOLS; i++) {
	for (tv = afs_volumes[i]; tv; tv = tv->next) {
	    if (all || tv->volume == volume) {
		afs_ResetVolumeInfo(tv);
		if (!all)
		    goto last;
	    }
	}
    }
 last:
    ReleaseReadLock(&afs_xvolume);

    /* probably, a user is doing this, probably, because things are screwed up.
     * maybe it's the dnlc's fault? */
    osi_dnlc_purge();
}

/*!
 * VIOC_FLUSHVOLUME (37) - Flush whole volume's data
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use (args in avc)
 * \param[out] aout	not in use
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 * \retval EIO		Error if the afs daemon hasn't started yet
 *
 * \post
 * 	Flush all cached contents of a volume.  Exactly what stays and what
 * 	goes depends on the platform.
 *
 * \notes
 * 	Does not flush a file that a user has open and is using, because
 * 	it will be re-created on next write.  Also purges the dnlc,
 * 	because things are screwed up.
 */
DECL_PIOCTL(PFlushVolumeData)
{
    AFS_STATCNT(PFlushVolumeData);
    if (!avc)
	return EINVAL;
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    FlushVolumeData(&avc->f.fid, *acred);
    return 0;
}

/*!
 * VIOC_FLUSHALL (14) - Flush whole volume's data for all volumes
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	not in use
 *
 * \retval EINVAL	Error if some of the standard args aren't set
 * \retval EIO		Error if the afs daemon hasn't started yet
 *
 * \post
 * 	Flush all cached contents.  Exactly what stays and what
 * 	goes depends on the platform.
 *
 * \notes
 * 	Does not flush a file that a user has open and is using, because
 * 	it will be re-created on next write.  Also purges the dnlc,
 * 	because things are screwed up.
 */
DECL_PIOCTL(PFlushAllVolumeData)
{
    AFS_STATCNT(PFlushAllVolumeData);

    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    FlushVolumeData(NULL, *acred);
    return 0;
}

/*!
 * VIOCGETVCXSTATUS (41) - gets vnode x status
 *
 * \ingroup pioctl
 *
 * \param[in] ain
 * 	not in use (avc used)
 * \param[out] aout
 * 	vcxstat: the file id, the data version, any lock, the parent vnode,
 * 	the parent unique id, the trunc position, the callback, cbExpires,
 * 	what access is being made, what files are open,
 * 	any users executing/writing, the flock count, the states,
 * 	the move stat
 *
 * \retval EINVAL
 * 	Error if some of the initial default arguments aren't set
 * \retval EACCES
 * 	Error if access to check the mode bits is denied
 *
 * \post
 * 	gets stats for the vnode, a struct listed in vcxstat
 */
DECL_PIOCTL(PGetVnodeXStatus)
{
    afs_int32 code;
    struct vcxstat stat;
    afs_int32 mode, i;

/*  AFS_STATCNT(PGetVnodeXStatus); */
    if (!avc)
	return EINVAL;
    code = afs_VerifyVCache(avc, areq);
    if (code)
	return code;
    if (vType(avc) == VDIR)
	mode = PRSFS_LOOKUP;
    else
	mode = PRSFS_READ;
    if (!afs_AccessOK(avc, mode, areq, CHECK_MODE_BITS))
	return EACCES;

    memset(&stat, 0, sizeof(struct vcxstat));
    stat.fid = avc->f.fid;
    hset32(stat.DataVersion, hgetlo(avc->f.m.DataVersion));
    stat.lock = avc->lock;
    stat.parentVnode = avc->f.parent.vnode;
    stat.parentUnique = avc->f.parent.unique;
    hset(stat.flushDV, avc->flushDV);
    hset(stat.mapDV, avc->mapDV);
    stat.truncPos = avc->f.truncPos;
    {			/* just grab the first two - won't break anything... */
	struct axscache *ac;

	for (i = 0, ac = avc->Access; ac && i < CPSIZE; i++, ac = ac->next) {
	    stat.randomUid[i] = ac->uid;
	    stat.randomAccess[i] = ac->axess;
	}
    }
    stat.callback = afs_data_pointer_to_int32(avc->callback);
    stat.cbExpires = avc->cbExpires;
    stat.anyAccess = avc->f.anyAccess;
    stat.opens = avc->opens;
    stat.execsOrWriters = avc->execsOrWriters;
    stat.flockCount = avc->flockCount;
    stat.mvstat = avc->mvstat;
    stat.states = avc->f.states;
    return afs_pd_putBytes(aout, &stat, sizeof(struct vcxstat));
}


DECL_PIOCTL(PGetVnodeXStatus2)
{
    afs_int32 code;
    struct vcxstat2 stat;
    afs_int32 mode;

    if (!avc)
        return EINVAL;
    code = afs_VerifyVCache(avc, areq);
    if (code)
        return code;
    if (vType(avc) == VDIR)
        mode = PRSFS_LOOKUP;
    else
        mode = PRSFS_READ;
    if (!afs_AccessOK(avc, mode, areq, CHECK_MODE_BITS))
        return EACCES;

    memset(&stat, 0, sizeof(struct vcxstat2));

    stat.cbExpires = avc->cbExpires;
    stat.anyAccess = avc->f.anyAccess;
    stat.mvstat = avc->mvstat;
    stat.callerAccess = afs_GetAccessBits(avc, ~0, areq);

    return afs_pd_putBytes(aout, &stat, sizeof(struct vcxstat2));
}


/*!
 * VIOC_AFS_SYSNAME (38) - Change @sys value
 *
 * \ingroup pioctl
 *
 * \param[in] ain	new value for @sys
 * \param[out] aout	count, entry, list (debug values?)
 *
 * \retval EINVAL
 * 	Error if afsd isn't running, the new sysname is too large,
 * 	the new sysname causes issues (starts with a . or ..),
 * 	there is no PAG set in the credentials, or the user of a PAG
 * 	can't be found
 * \retval EACCES
 * 	Error if the user doesn't have super-user credentials
 *
 * \post
 * 	Set the value of @sys if these things work: if the input isn't
 * 	too long or if input doesn't start with . or ..
 *
 * \notes
 * 	We require root for local sysname changes, but not for remote
 * 	(since we don't really believe remote uids anyway)
 * 	outname[] shouldn't really be needed- this is left as an
 * 	exercise for the reader.
 */
DECL_PIOCTL(PSetSysName)
{
    char *inname = NULL;
    char outname[MAXSYSNAME];
    afs_int32 setsysname;
    int foundname = 0;
    struct afs_exporter *exporter;
    struct unixuser *au;
    afs_int32 pag, error;
    int t, count, num = 0, allpags = 0;
    char **sysnamelist;
    struct afs_pdata validate;

    AFS_STATCNT(PSetSysName);
    if (!afs_globalVFS) {
	/* Afsd is NOT running; disable it */
#if defined(KERNEL_HAVE_UERROR)
	return (setuerror(EINVAL), EINVAL);
#else
	return (EINVAL);
#endif
    }
    if (afs_pd_getInt(ain, &setsysname) != 0)
	return EINVAL;
    if (setsysname & 0x8000) {
	allpags = 1;
	setsysname &= ~0x8000;
    }
    if (setsysname) {

	/* Check my args */
	if (setsysname < 0 || setsysname > MAXNUMSYSNAMES)
	    return EINVAL;
	validate = *ain;
	for (count = 0; count < setsysname; count++) {
	    if (afs_pd_getStringPtr(&validate, &inname) != 0)
	        return EINVAL;
	    t = strlen(inname);
	    if (t >= MAXSYSNAME || t <= 0)
		return EINVAL;
	    /* check for names that can shoot us in the foot */
	    if (inname[0] == '.' && (inname[1] == 0
		|| (inname[1] == '.' && inname[2] == 0)))
		return EINVAL;
	}
	/* args ok, so go back to the beginning of that section */

	if (afs_pd_getStringPtr(ain, &inname) != 0)
	    return EINVAL;
	num = count;
    }
    if (afs_cr_gid(*acred) == RMTUSER_REQ ||
	afs_cr_gid(*acred) == RMTUSER_REQ_PRIV) {   /* Handles all exporters */
	if (allpags && afs_cr_gid(*acred) != RMTUSER_REQ_PRIV) {
	    return EPERM;
	}
	pag = PagInCred(*acred);
	if (pag == NOPAG) {
	    return EINVAL;	/* Better than panicing */
	}
	if (!(au = afs_FindUser(pag, -1, READ_LOCK))) {
	    return EINVAL;	/* Better than panicing */
	}
	if (!(exporter = au->exporter)) {
	    afs_PutUser(au, READ_LOCK);
	    return EINVAL;	/* Better than panicing */
	}
	error = EXP_SYSNAME(exporter, inname, &sysnamelist,
			    &num, allpags);
	if (error) {
	    if (error == ENODEV)
		foundname = 0;	/* sysname not set yet! */
	    else {
		afs_PutUser(au, READ_LOCK);
		return error;
	    }
	} else {
	    foundname = num;
	    strcpy(outname, sysnamelist[0]);
	}
	afs_PutUser(au, READ_LOCK);
	if (setsysname)
	    afs_sysnamegen++;
    } else {
	/* Not xlating, so local case */
	if (!afs_sysname)
	    osi_Panic("PSetSysName: !afs_sysname\n");
	if (!setsysname) {	/* user just wants the info */
	    strcpy(outname, afs_sysname);
	    foundname = afs_sysnamecount;
	    sysnamelist = afs_sysnamelist;
	} else {		/* Local guy; only root can change sysname */
	    if (!afs_osi_suser(*acred))
		return EACCES;

	    /* allpags makes no sense for local use */
	    if (allpags)
		return EINVAL;

	    /* clear @sys entries from the dnlc, once afs_lookup can
	     * do lookups of @sys entries and thinks it can trust them */
	    /* privs ok, store the entry, ... */

	    if (strlen(inname) >= MAXSYSNAME-1)
		return EINVAL;
	    strcpy(afs_sysname, inname);

	    if (setsysname > 1) {	/* ... or list */
		for (count = 1; count < setsysname; ++count) {
		    if (!afs_sysnamelist[count])
			osi_Panic
			   ("PSetSysName: no afs_sysnamelist entry to write\n");
		    if (afs_pd_getString(ain, afs_sysnamelist[count],
					 MAXSYSNAME) != 0)
			return EINVAL;
		}
	    }
	    afs_sysnamecount = setsysname;
	    afs_sysnamegen++;
	}
    }
    if (!setsysname) {
	if (afs_pd_putInt(aout, foundname) != 0)
	    return E2BIG;
	if (foundname) {
	    if (afs_pd_putString(aout, outname) != 0)
		return E2BIG;
	    for (count = 1; count < foundname; ++count) {    /* ... or list. */
		if (!sysnamelist[count])
		    osi_Panic
			("PSetSysName: no afs_sysnamelist entry to read\n");
		t = strlen(sysnamelist[count]);
		if (t >= MAXSYSNAME)
		    osi_Panic("PSetSysName: sysname entry garbled\n");
		if (afs_pd_putString(aout, sysnamelist[count]) != 0)
		    return E2BIG;
	    }
	}
    }
    return 0;
}

/* sequential search through the list of touched cells is not a good
 * long-term solution here. For small n, though, it should be just
 * fine.  Should consider special-casing the local cell for large n.
 * Likewise for PSetSPrefs.
 *
 * s - number of ids in array l[] -- NOT index of last id
 * l - array of cell ids which have volumes that need to be sorted
 * vlonly - sort vl servers or file servers?
 */
static void *
ReSortCells_cb(struct cell *cell, void *arg)
{
    afs_int32 *p = (afs_int32 *) arg;
    afs_int32 *l = p + 1;
    int i, s = p[0];

    for (i = 0; i < s; i++) {
	if (l[i] == cell->cellNum) {
	    ObtainWriteLock(&cell->lock, 690);
	    afs_SortServers(cell->cellHosts, AFS_MAXCELLHOSTS);
	    ReleaseWriteLock(&cell->lock);
	}
    }

    return NULL;
}

static void
ReSortCells(int s, afs_int32 * l, int vlonly)
{
    int i;
    struct volume *j;
    int k;

    if (vlonly) {
	afs_int32 *p;
	p = afs_osi_Alloc(sizeof(afs_int32) * (s + 1));
        osi_Assert(p != NULL);
	p[0] = s;
	memcpy(p + 1, l, s * sizeof(afs_int32));
	afs_TraverseCells(&ReSortCells_cb, p);
	afs_osi_Free(p, sizeof(afs_int32) * (s + 1));
	return;
    }

    ObtainReadLock(&afs_xvolume);
    for (i = 0; i < NVOLS; i++) {
	for (j = afs_volumes[i]; j; j = j->next) {
	    for (k = 0; k < s; k++)
		if (j->cell == l[k]) {
		    ObtainWriteLock(&j->lock, 233);
		    afs_SortServers(j->serverHost, AFS_MAXHOSTS);
		    ReleaseWriteLock(&j->lock);
		    break;
		}
	}
    }
    ReleaseReadLock(&afs_xvolume);
}


static int debugsetsp = 0;
static int
afs_setsprefs(struct spref *sp, unsigned int num, unsigned int vlonly)
{
    struct srvAddr *sa;
    int i, j, k, matches, touchedSize;
    struct server *srvr = NULL;
    afs_int32 touched[34];
    int isfs;

    touchedSize = 0;
    for (k = 0; k < num; sp++, k++) {
	if (debugsetsp) {
	    afs_warn("sp host=%x, rank=%d\n", sp->host.s_addr, sp->rank);
	}
	matches = 0;
	ObtainReadLock(&afs_xserver);

	i = SHash(sp->host.s_addr);
	for (sa = afs_srvAddrs[i]; sa; sa = sa->next_bkt) {
	    if (sa->sa_ip == sp->host.s_addr) {
		srvr = sa->server;
		isfs = (srvr->cell && (sa->sa_portal == srvr->cell->fsport))
		    || (sa->sa_portal == AFS_FSPORT);
		if ((!vlonly && isfs) || (vlonly && !isfs)) {
		    matches++;
		    break;
		}
	    }
	}

	if (sa && matches) {	/* found one! */
	    if (debugsetsp) {
		afs_warn("sa ip=%x, ip_rank=%d\n", sa->sa_ip, sa->sa_iprank);
	    }
	    sa->sa_iprank = sp->rank + afs_randomMod15();
	    afs_SortOneServer(sa->server);

	    if (srvr->cell) {
		/* if we don't know yet what cell it's in, this is moot */
		for (j = touchedSize - 1;
		     j >= 0 && touched[j] != srvr->cell->cellNum; j--)
		    /* is it in our list of touched cells ?  */ ;
		if (j < 0) {	/* no, it's not */
		    touched[touchedSize++] = srvr->cell->cellNum;
		    if (touchedSize >= 32) {	/* watch for ovrflow */
			ReleaseReadLock(&afs_xserver);
			ReSortCells(touchedSize, touched, vlonly);
			touchedSize = 0;
			ObtainReadLock(&afs_xserver);
		    }
		}
	    }
	}

	ReleaseReadLock(&afs_xserver);
	/* if we didn't find one, start to create one. */
	/* Note that it doesn't have a cell yet...     */
	if (!matches) {
	    afs_uint32 temp = sp->host.s_addr;
	    srvr =
		afs_GetServer(&temp, 1, 0, (vlonly ? AFS_VLPORT : AFS_FSPORT),
			      WRITE_LOCK, (afsUUID *) 0, 0, NULL);
	    srvr->addr->sa_iprank = sp->rank + afs_randomMod15();
	    afs_PutServer(srvr, WRITE_LOCK);
	}
    }				/* for all cited preferences */

    ReSortCells(touchedSize, touched, vlonly);
    return 0;
}

/*!
 * VIOC_SETPREFS (46) - Set server ranks
 *
 * \param[in] ain	the sprefs value you want the sprefs to be set to
 * \param[out] aout	not in use
 *
 * \retval EIO
 * 	Error if the afs daemon hasn't started yet
 * \retval EACCES
 * 	Error if the user doesn't have super-user credentials
 * \retval EINVAL
 * 	Error if the struct setsprefs is too large or if it multiplied
 * 	by the number of servers is too large
 *
 * \post set the sprefs using the afs_setsprefs() function
 */
DECL_PIOCTL(PSetSPrefs)
{
    struct setspref *ssp;
    char *ainPtr;
    size_t ainSize;

    AFS_STATCNT(PSetSPrefs);

    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (!afs_osi_suser(*acred))
	return EACCES;

    /* The I/O handling here is ghastly, as it relies on overrunning the ends
     * of arrays. But, i'm not quite brave enough to change it yet. */
    ainPtr = ain->ptr;
    ainSize = ain->remaining;

    if (ainSize < sizeof(struct setspref))
	return EINVAL;

    ssp = (struct setspref *)ainPtr;
    if (ainSize < (sizeof(struct setspref)
		   + sizeof(struct spref) * (ssp->num_servers-1)))
	return EINVAL;

    afs_setsprefs(&(ssp->servers[0]), ssp->num_servers,
		  (ssp->flags & DBservers));
    return 0;
}

/*
 * VIOC_SETPREFS33 (42) - Set server ranks (deprecated)
 *
 * \param[in] ain	the server preferences to be set
 * \param[out] aout	not in use
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval EACCES	Error if the user doesn't have super-user credentials
 *
 * \post set the server preferences, calling a function
 *
 * \notes this may only be performed by the local root user.
 */
DECL_PIOCTL(PSetSPrefs33)
{
    AFS_STATCNT(PSetSPrefs);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */


    if (!afs_osi_suser(*acred))
	return EACCES;

    afs_setsprefs((struct spref *)afs_pd_where(ain),
		  afs_pd_remaining(ain) / sizeof(struct spref),
		  0 /*!vlonly */ );
    return 0;
}

/*
 * VIOC_GETSPREFS (43) - Get server ranks
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the server preferences to get
 * \param[out] aout	the server preferences information
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval ENOENT	Error if the sprefrequest is too large
 *
 * \post Get the sprefs
 *
 * \notes
 * 	in the hash table of server structs, all servers with the same
 * 	IP address; will be on the same overflow chain; This could be
 * 	sped slightly in some circumstances by having it cache the
 * 	immediately previous slot in the hash table and some
 * 	supporting information; Only reports file servers now.
 */
DECL_PIOCTL(PGetSPrefs)
{
    struct sprefrequest spin;	/* input */
    struct sprefinfo *spout;	/* output */
    struct spref *srvout;	/* one output component */
    int i, j;			/* counters for hash table traversal */
    struct server *srvr;	/* one of CM's server structs */
    struct srvAddr *sa;
    int vlonly;			/* just return vlservers ? */
    int isfs;

    AFS_STATCNT(PGetSPrefs);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    /* Work out from the size whether we've got a new, or old, style pioctl */
    if (afs_pd_remaining(ain) < sizeof(struct sprefrequest)) {
	if (afs_pd_getBytes(ain, &spin, sizeof(struct sprefrequest_33)) != 0)
	   return ENOENT;
	vlonly = 0;
	spin.flags = 0;
    } else {
	if (afs_pd_getBytes(ain, &spin, sizeof(struct sprefrequest)) != 0)
	   return EINVAL;
	vlonly = (spin.flags & DBservers);
    }

    /* This code relies on overflowing arrays. It's ghastly, but I'm not
     * quite brave enough to tackle it yet ...
     */

    /* struct sprefinfo includes 1 server struct...  that size gets added
     * in during the loop that follows.
     */
    spout = afs_pd_inline(aout,
		          sizeof(struct sprefinfo) - sizeof(struct spref));
    spout->next_offset = spin.offset;
    spout->num_servers = 0;
    srvout = spout->servers;

    ObtainReadLock(&afs_xserver);
    for (i = 0, j = 0; j < NSERVERS; j++) {	/* sift through hash table */
	for (sa = afs_srvAddrs[j]; sa; sa = sa->next_bkt, i++) {
	    if (spin.offset > (unsigned short)i) {
		continue;	/* catch up to where we left off */
	    }
	    spout->next_offset++;

	    srvr = sa->server;
	    isfs = (srvr->cell && (sa->sa_portal == srvr->cell->fsport))
		|| (sa->sa_portal == AFS_FSPORT);

	    if ((vlonly && isfs) || (!vlonly && !isfs)) {
		/* only report ranks for vl servers */
		continue;
	    }

	    /* Check we've actually got the space we're about to use */
	    if (afs_pd_inline(aout, sizeof(struct spref)) == NULL) {
		ReleaseReadLock(&afs_xserver);	/* no more room! */
		return 0;
	    }

	    srvout->host.s_addr = sa->sa_ip;
	    srvout->rank = sa->sa_iprank;
	    spout->num_servers++;
	    srvout++;
	}
    }
    ReleaseReadLock(&afs_xserver);

    spout->next_offset = 0;	/* start over from the beginning next time */

    return 0;
}

/* Enable/Disable the specified exporter. Must be root to disable an exporter */
int afs_NFSRootOnly = 1;
/*!
 * VIOC_EXPORTAFS (39) - Export afs to nfs clients
 *
 * \ingroup pioctl
 *
 * \param[in] ain
 * 	an integer containing the desired exportee flags
 * \param[out] aout
 * 	an integer containing the current exporter flags
 *
 * \retval ENODEV	Error if the exporter doesn't exist
 * \retval EACCES	Error if the user doesn't have super-user credentials
 *
 * \post
 * 	Changes the state of various values to reflect the change
 * 	of the export values between nfs and afs.
 *
 * \notes Legacy code obtained from IBM.
 */
DECL_PIOCTL(PExportAfs)
{
    afs_int32 export, newint = 0;
    afs_int32 type, changestate, handleValue, convmode, pwsync, smounts;
    afs_int32 rempags = 0, pagcb = 0;
    struct afs_exporter *exporter;

    AFS_STATCNT(PExportAfs);
    if (afs_pd_getInt(ain, &handleValue) != 0)
	return EINVAL;
    type = handleValue >> 24;
    if (type == 0x71) {
	newint = 1;
	type = 1;		/* nfs */
    }
    exporter = exporter_find(type);
    if (newint) {
	export = handleValue & 3;
	changestate = handleValue & 0xfff;
	smounts = (handleValue >> 2) & 3;
	pwsync = (handleValue >> 4) & 3;
	convmode = (handleValue >> 6) & 3;
	rempags = (handleValue >> 8) & 3;
	pagcb = (handleValue >> 10) & 3;
    } else {
	changestate = (handleValue >> 16) & 0x1;
	convmode = (handleValue >> 16) & 0x2;
	pwsync = (handleValue >> 16) & 0x4;
	smounts = (handleValue >> 16) & 0x8;
	export = handleValue & 0xff;
    }
    if (!exporter) {
	/*  Failed finding desired exporter; */
	return ENODEV;
    }
    if (!changestate) {
	handleValue = exporter->exp_states;
	if (afs_pd_putInt(aout, handleValue) != 0)
	    return E2BIG;
    } else {
	if (!afs_osi_suser(*acred))
	    return EACCES;	/* Only superuser can do this */
	if (newint) {
	    if (export & 2) {
		if (export & 1)
		    exporter->exp_states |= EXP_EXPORTED;
		else
		    exporter->exp_states &= ~EXP_EXPORTED;
	    }
	    if (convmode & 2) {
		if (convmode & 1)
		    exporter->exp_states |= EXP_UNIXMODE;
		else
		    exporter->exp_states &= ~EXP_UNIXMODE;
	    }
	    if (pwsync & 2) {
		if (pwsync & 1)
		    exporter->exp_states |= EXP_PWSYNC;
		else
		    exporter->exp_states &= ~EXP_PWSYNC;
	    }
	    if (smounts & 2) {
		if (smounts & 1) {
		    afs_NFSRootOnly = 0;
		    exporter->exp_states |= EXP_SUBMOUNTS;
		} else {
		    afs_NFSRootOnly = 1;
		    exporter->exp_states &= ~EXP_SUBMOUNTS;
		}
	    }
	    if (rempags & 2) {
		if (rempags & 1)
		    exporter->exp_states |= EXP_CLIPAGS;
		else
		    exporter->exp_states &= ~EXP_CLIPAGS;
	    }
	    if (pagcb & 2) {
		if (pagcb & 1)
		    exporter->exp_states |= EXP_CALLBACK;
		else
		    exporter->exp_states &= ~EXP_CALLBACK;
	    }
	    handleValue = exporter->exp_states;
	    if (afs_pd_putInt(aout, handleValue) != 0)
		return E2BIG;
	} else {
	    if (export)
		exporter->exp_states |= EXP_EXPORTED;
	    else
		exporter->exp_states &= ~EXP_EXPORTED;
	    if (convmode)
		exporter->exp_states |= EXP_UNIXMODE;
	    else
		exporter->exp_states &= ~EXP_UNIXMODE;
	    if (pwsync)
		exporter->exp_states |= EXP_PWSYNC;
	    else
		exporter->exp_states &= ~EXP_PWSYNC;
	    if (smounts) {
		afs_NFSRootOnly = 0;
		exporter->exp_states |= EXP_SUBMOUNTS;
	    } else {
		afs_NFSRootOnly = 1;
		exporter->exp_states &= ~EXP_SUBMOUNTS;
	    }
	}
    }

    return 0;
}

/*!
 * VIOC_GAG (44) - Silence Cache Manager
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the flags to either gag or de-gag the cache manager
 * \param[out] aout	not in use
 *
 * \retval EACCES	Error if the user doesn't have super-user credentials
 *
 * \post set the gag flags, then show these flags
 */
DECL_PIOCTL(PGag)
{
    struct gaginfo *gagflags;

    if (!afs_osi_suser(*acred))
	return EACCES;

    gagflags = afs_pd_inline(ain, sizeof(*gagflags));
    if (gagflags == NULL)
	return EINVAL;
    afs_showflags = gagflags->showflags;

    return 0;
}

/*!
 * VIOC_TWIDDLE (45) - Adjust RX knobs
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the previous settings of the 'knobs'
 * \param[out] aout	not in use
 *
 * \retval EACCES	Error if the user doesn't have super-user credentials
 *
 * \post build out the struct rxp, from a struct rx
 */
DECL_PIOCTL(PTwiddleRx)
{
    struct rxparams *rxp;

    if (!afs_osi_suser(*acred))
	return EACCES;

    rxp = afs_pd_inline(ain, sizeof(*rxp));
    if (rxp == NULL)
	return EINVAL;

    if (rxp->rx_initReceiveWindow)
	rx_initReceiveWindow = rxp->rx_initReceiveWindow;
    if (rxp->rx_maxReceiveWindow)
	rx_maxReceiveWindow = rxp->rx_maxReceiveWindow;
    if (rxp->rx_initSendWindow)
	rx_initSendWindow = rxp->rx_initSendWindow;
    if (rxp->rx_maxSendWindow)
	rx_maxSendWindow = rxp->rx_maxSendWindow;
    if (rxp->rxi_nSendFrags)
	rxi_nSendFrags = rxp->rxi_nSendFrags;
    if (rxp->rxi_nRecvFrags)
	rxi_nRecvFrags = rxp->rxi_nRecvFrags;
    if (rxp->rxi_OrphanFragSize)
	rxi_OrphanFragSize = rxp->rxi_OrphanFragSize;
    if (rxp->rx_maxReceiveSize) {
	rx_maxReceiveSize = rxp->rx_maxReceiveSize;
	rx_maxReceiveSizeUser = rxp->rx_maxReceiveSize;
    }
    if (rxp->rx_MyMaxSendSize)
	rx_MyMaxSendSize = rxp->rx_MyMaxSendSize;

    return 0;
}

/*!
 * VIOC_GETINITPARAMS (49) - Get initial cache manager parameters
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	initial cache manager params
 *
 * \retval E2BIG
 * 	Error if the initial parameters are bigger than some PIGGYSIZE
 *
 * \post return the initial cache manager parameters
 */
DECL_PIOCTL(PGetInitParams)
{
    if (sizeof(struct cm_initparams) > PIGGYSIZE)
	return E2BIG;

    return afs_pd_putBytes(aout, &cm_initParams,
			   sizeof(struct cm_initparams));
}

#ifdef AFS_SGI65_ENV
/* They took crget() from us, so fake it. */
static cred_t *
crget(void)
{
    cred_t *cr;
    cr = crdup(get_current_cred());
    memset(cr, 0, sizeof(cred_t));
#if CELL || CELL_PREPARE
    cr->cr_id = -1;
#endif
    return cr;
}
#endif

/*!
 * VIOC_GETRXKCRYPT (55) - Get rxkad encryption flag
 *
 * \ingroup pioctl
 *
 * \param[in] ain	not in use
 * \param[out] aout	value of cryptall
 *
 * \post Turns on, or disables, rxkad encryption by setting the cryptall global
 */
DECL_PIOCTL(PGetRxkcrypt)
{
    return afs_pd_putInt(aout, cryptall);
}

/*!
 * VIOC_SETRXKCRYPT (56) - Set rxkad encryption flag
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the argument whether or not things should be encrypted
 * \param[out] aout	not in use
 *
 * \retval EPERM
 * 	Error if the user doesn't have super-user credentials
 * \retval EINVAL
 * 	Error if the input is too big, or if the input is outside the
 * 	bounds of what it can be set to
 *
 * \post set whether or not things should be encrypted
 *
 * \notes
 * 	may need to be modified at a later date to take into account
 * 	other values for cryptall (beyond true or false)
 */
DECL_PIOCTL(PSetRxkcrypt)
{
    afs_int32 tmpval;

    if (!afs_osi_suser(*acred))
	return EPERM;
    if (afs_pd_getInt(ain, &tmpval) != 0)
	return EINVAL;
    /* if new mappings added later this will need to be changed */
    if (tmpval != 0 && tmpval != 1)
	return EINVAL;
    cryptall = tmpval;
    return 0;
}

#ifdef AFS_NEED_CLIENTCONTEXT
/*
 * Create new credentials to correspond to a remote user with given
 * <hostaddr, uid, g0, g1>.  This allows a server running as root to
 * provide pioctl (and other) services to foreign clients (i.e. nfs
 * clients) by using this call to `become' the client.
 */
#define	PSETPAG		110
#define	PIOCTL_HEADER	6
static int
HandleClientContext(struct afs_ioctl *ablob, int *com,
		    afs_ucred_t **acred, afs_ucred_t *credp)
{
    char *ain, *inData;
    afs_uint32 hostaddr;
    afs_int32 uid, g0, g1, i, code, pag, exporter_type, isroot = 0;
    struct afs_exporter *exporter, *outexporter;
    afs_ucred_t *newcred;
    struct unixuser *au;
    afs_uint32 comp = *com & 0xff00;
    afs_uint32 h, l;
#if defined(AFS_SUN510_ENV)
    gid_t gids[2];
#endif

#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(HandleClientContext);
    if (ablob->in_size < PIOCTL_HEADER * sizeof(afs_int32)) {
	/* Must at least include the PIOCTL_HEADER header words
	 * required by the protocol */
	return EINVAL;		/* Too small to be good  */
    }
    ain = inData = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    AFS_COPYIN(ablob->in, ain, PIOCTL_HEADER * sizeof(afs_int32), code);
    if (code) {
	osi_FreeLargeSpace(inData);
	return code;
    }

    /* Extract information for remote user */
    hostaddr = *((afs_uint32 *) ain);
    ain += sizeof(hostaddr);
    uid = *((afs_uint32 *) ain);
    ain += sizeof(uid);
    g0 = *((afs_uint32 *) ain);
    ain += sizeof(g0);
    g1 = *((afs_uint32 *) ain);
    ain += sizeof(g1);
    *com = *((afs_uint32 *) ain);
    ain += sizeof(afs_int32);
    exporter_type = *((afs_uint32 *) ain);/* In case we support more than NFS */

    /*
     * Of course, one must be root for most of these functions, but
     * we'll allow (for knfs) you to set things if the pag is 0 and
     * you're setting tokens or unlogging.
     */
    i = (*com) & 0xff;
    if (!afs_osi_suser(credp)) {
#if defined(AFS_SGI_ENV) && !defined(AFS_SGI64_ENV)
	/* Since SGI's suser() returns explicit failure after the call.. */
	u.u_error = 0;
#endif
	/* check for acceptable opcodes for normal folks, which are, so far,
	 * get/set tokens, sysname, and unlog.
	 */
	if (i != 9 && i != 3 && i != 38 && i != 8) {
	    osi_FreeLargeSpace(inData);
	    return EACCES;
	}
    }

    ablob->in_size -= PIOCTL_HEADER * sizeof(afs_int32);
    ablob->in += PIOCTL_HEADER * sizeof(afs_int32);
    osi_FreeLargeSpace(inData);
    if (uid == 0) {
	/*
	 * We map uid 0 to nobody to match the mapping that the nfs
	 * server does and to ensure that the suser() calls in the afs
	 * code fails for remote client roots.
	 */
	uid = afs_nobody;	/* NFS_NOBODY == -2 */
	isroot = 1;
    }
    newcred = crget();
#ifdef	AFS_AIX41_ENV
    setuerror(0);
#endif
    afs_set_cr_gid(newcred, isroot ? RMTUSER_REQ_PRIV : RMTUSER_REQ);
#ifdef AFS_AIX51_ENV
    newcred->cr_groupset.gs_union.un_groups[0] = g0;
    newcred->cr_groupset.gs_union.un_groups[1] = g1;
#elif defined(AFS_LINUX26_ENV)
# ifdef AFS_LINUX26_ONEGROUP_ENV
    afs_set_cr_group_info(newcred, groups_alloc(1)); /* nothing sets this */
    l = (((g0-0x3f00) & 0x3fff) << 14) | ((g1-0x3f00) & 0x3fff);
    h = ((g0-0x3f00) >> 14);
    h = ((g1-0x3f00) >> 14) + h + h + h;
    GROUP_AT(afs_cr_group_info(newcred), 0) = ((h << 28) | l);
# else
    afs_set_cr_group_info(newcred, groups_alloc(2));
    GROUP_AT(afs_cr_group_info(newcred), 0) = g0;
    GROUP_AT(afs_cr_group_info(newcred), 1) = g1;
# endif
#elif defined(AFS_SUN510_ENV)
    gids[0] = g0;
    gids[1] = g1;
    crsetgroups(newcred, 2, gids);
#else
    newcred->cr_groups[0] = g0;
    newcred->cr_groups[1] = g1;
#endif
#ifdef AFS_AIX_ENV
    newcred->cr_ngrps = 2;
#elif !defined(AFS_LINUX26_ENV) && !defined(AFS_SUN510_ENV)
# if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_LINUX22_ENV) || defined(AFS_FBSD80_ENV)
    newcred->cr_ngroups = 2;
# else
    for (i = 2; i < NGROUPS; i++)
	newcred->cr_groups[i] = NOGROUP;
# endif
#endif
    if (!(exporter = exporter_find(exporter_type))) {
	/* Exporter wasn't initialized or an invalid exporter type */
	crfree(newcred);
	return EINVAL;
    }
    if (exporter->exp_states & EXP_PWSYNC) {
	if (uid != afs_cr_uid(credp)) {
	    crfree(newcred);
	    return ENOEXEC;	/* XXX Find a better errno XXX */
	}
    }
    afs_set_cr_uid(newcred, uid);	/* Only temporary  */
    code = EXP_REQHANDLER(exporter, &newcred, hostaddr, &pag, &outexporter);
    /* The client's pag is the only unique identifier for it */
    afs_set_cr_uid(newcred, pag);
    *acred = newcred;
    if (!code && *com == PSETPAG) {
	/* Special case for 'setpag' */
	afs_uint32 pagvalue = genpag();

	au = afs_GetUser(pagvalue, -1, WRITE_LOCK); /* a new unixuser struct */
	/*
	 * Note that we leave the 'outexporter' struct held so it won't
	 * dissappear on us
	 */
	au->exporter = outexporter;
	if (ablob->out_size >= 4) {
	    AFS_COPYOUT((char *)&pagvalue, ablob->out, sizeof(afs_int32),
			code);
	}
	afs_PutUser(au, WRITE_LOCK);
	if (code)
	    return code;
	return PSETPAG;		/*  Special return for setpag  */
    } else if (!code) {
	EXP_RELE(outexporter);
    }
    if (!code)
	*com = (*com) | comp;
    return code;
}
#endif /* AFS_NEED_CLIENTCONTEXT */


/*!
 * VIOC_GETCPREFS (50) - Get client interface
 *
 * \ingroup pioctl
 *
 * \param[in] ain	sprefrequest input
 * \param[out] aout	spref information
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval EINVAL	Error if some of the standard args aren't set
 *
 * \post
 * 	get all interface addresses and other information of the client
 * 	interface
 */
DECL_PIOCTL(PGetCPrefs)
{
    struct sprefrequest *spin;	/* input */
    struct sprefinfo *spout;	/* output */
    struct spref *srvout;	/* one output component */
    int maxNumber;
    int i, j;

    AFS_STATCNT(PGetCPrefs);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    spin = afs_pd_inline(ain, sizeof(*spin));
    if (spin == NULL)
	return EINVAL;

    /* Output spout relies on writing past the end of arrays. It's horrible,
     * but I'm not quite brave enough to tackle it yet */
    spout = (struct sprefinfo *)aout->ptr;

    maxNumber = spin->num_servers;	/* max addrs this time */
    srvout = spout->servers;

    ObtainReadLock(&afs_xinterface);

    /* copy out the client interface information from the
     * kernel data structure "interface" to the output buffer
     */
    for (i = spin->offset, j = 0; (i < afs_cb_interface.numberOfInterfaces)
	 && (j < maxNumber); i++, j++, srvout++)
	srvout->host.s_addr = afs_cb_interface.addr_in[i];

    spout->num_servers = j;
    aout->ptr += sizeof(struct sprefinfo) + (j - 1) * sizeof(struct spref);

    if (i >= afs_cb_interface.numberOfInterfaces)
	spout->next_offset = 0;	/* start from beginning again */
    else
	spout->next_offset = spin->offset + j;

    ReleaseReadLock(&afs_xinterface);
    return 0;
}

/*!
 * VIOC_SETCPREFS (51) - Set client interface
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the interfaces you want set
 * \param[out] aout	not in use
 *
 * \retval EIO		Error if the afs daemon hasn't started yet
 * \retval EINVAL	Error if the input is too large for the struct
 * \retval ENOMEM	Error if there are too many servers
 *
 * \post set the callbak interfaces addresses to those of the hosts
 */
DECL_PIOCTL(PSetCPrefs)
{
    char *ainPtr;
    size_t ainSize;
    struct setspref *sin;
    int i;

    AFS_STATCNT(PSetCPrefs);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    /* Yuck. Input to this function relies on reading past the end of
     * structures. Bodge it for now.
     */
    ainPtr = ain->ptr;
    ainSize = ain->remaining;

    sin = (struct setspref *)ainPtr;

    if (ainSize < sizeof(struct setspref))
	return EINVAL;
#if 0				/* num_servers is unsigned */
    if (sin->num_servers < 0)
	return EINVAL;
#endif
    if (sin->num_servers > AFS_MAX_INTERFACE_ADDR)
	return ENOMEM;

    ObtainWriteLock(&afs_xinterface, 412);
    afs_cb_interface.numberOfInterfaces = sin->num_servers;
    for (i = 0; (unsigned short)i < sin->num_servers; i++)
	afs_cb_interface.addr_in[i] = sin->servers[i].host.s_addr;

    ReleaseWriteLock(&afs_xinterface);
    return 0;
}

/*!
 * VIOC_AFS_FLUSHMOUNT (52) - Flush mount symlink data
 *
 * \ingroup pioctl
 *
 * \param[in] ain
 * 	the last part of a path to a mount point, which tells us what to flush
 * \param[out] aout
 * 	not in use
 *
 * \retval EINVAL
 * 	Error if some of the initial arguments aren't set
 * \retval ENOTDIR
 * 	Error if the initial argument for the mount point isn't a directory
 * \retval ENOENT
 * 	Error if the dcache entry isn't set
 *
 * \post
 * 	remove all of the mount data from the dcache regarding a
 * 	certain mount point
 */
DECL_PIOCTL(PFlushMount)
{
    afs_int32 code;
    struct vcache *tvc;
    struct dcache *tdc;
    struct VenusFid tfid;
    char *bufp;
    char *mount;
    struct sysname_info sysState;
    afs_size_t offset, len;

    AFS_STATCNT(PFlushMount);
    if (!avc)
	return EINVAL;

    if (afs_pd_getStringPtr(ain, &mount) != 0)
	return EINVAL;

    code = afs_VerifyVCache(avc, areq);
    if (code)
	return code;
    if (vType(avc) != VDIR) {
	return ENOTDIR;
    }
    tdc = afs_GetDCache(avc, (afs_size_t) 0, areq, &offset, &len, 1);
    if (!tdc)
	return ENOENT;
    Check_AtSys(avc, mount, &sysState, areq);
    ObtainReadLock(&tdc->lock);
    do {
	code = afs_dir_Lookup(tdc, sysState.name, &tfid.Fid);
    } while (code == ENOENT && Next_AtSys(avc, areq, &sysState));
    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);		/* we're done with the data */
    bufp = sysState.name;
    if (code) {
	goto out;
    }
    tfid.Cell = avc->f.fid.Cell;
    tfid.Fid.Volume = avc->f.fid.Fid.Volume;
    if (!tfid.Fid.Unique && (avc->f.states & CForeign)) {
	tvc = afs_LookupVCache(&tfid, areq, NULL, avc, bufp);
    } else {
	tvc = afs_GetVCache(&tfid, areq, NULL, NULL);
    }
    if (!tvc) {
	code = ENOENT;
	goto out;
    }
    if (tvc->mvstat != 1) {
	afs_PutVCache(tvc);
	code = EINVAL;
	goto out;
    }
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonLock(&tvc->pvnLock, tvc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
    ObtainWriteLock(&tvc->lock, 649);
    ObtainWriteLock(&afs_xcbhash, 650);
    afs_DequeueCallback(tvc);
    tvc->f.states &= ~(CStatd | CDirty); /* next reference will re-stat cache entry */
    ReleaseWriteLock(&afs_xcbhash);
    /* now find the disk cache entries */
    afs_TryToSmush(tvc, *acred, 1);
    osi_dnlc_purgedp(tvc);
    if (tvc->linkData && !(tvc->f.states & CCore)) {
	afs_osi_Free(tvc->linkData, strlen(tvc->linkData) + 1);
	tvc->linkData = NULL;
    }
    ReleaseWriteLock(&tvc->lock);
#ifdef AFS_BOZONLOCK_ENV
    afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
    afs_PutVCache(tvc);
  out:
    if (sysState.allocked)
	osi_FreeLargeSpace(bufp);
    return code;
}

/*!
 * VIOC_RXSTAT_PROC (53) - Control process RX statistics
 *
 * \ingroup pioctl
 *
 * \param[in] ain	the flags that control which stats to use
 * \param[out] aout	not in use
 *
 * \retval EACCES	Error if the user doesn't have super-user credentials
 * \retval EINVAL	Error if the flag input is too long
 *
 * \post
 * 	either enable process RPCStats, disable process RPCStats,
 * 	or clear the process RPCStats
 */
DECL_PIOCTL(PRxStatProc)
{
    afs_int32 flags;

    if (!afs_osi_suser(*acred))
	return EACCES;

    if (afs_pd_getInt(ain, &flags) != 0)
	return EINVAL;

    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK))
	return EINVAL;

    if (flags & AFSCALL_RXSTATS_ENABLE) {
	rx_enableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
	rx_disableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
	rx_clearProcessRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
    return 0;
}


/*!
 * VIOC_RXSTAT_PEER (54) - Control peer RX statistics
 *
 * \ingroup pioctl
 *
 * \param[in] ain 	the flags that control which statistics to use
 * \param[out] aout	not in use
 *
 * \retval EACCES	Error if the user doesn't have super-user credentials
 * \retval EINVAL	Error if the flag input is too long
 *
 * \post
 * 	either enable peer RPCStatws, disable peer RPCStats,
 * 	or clear the peer RPCStats
 */
DECL_PIOCTL(PRxStatPeer)
{
    afs_int32 flags;

    if (!afs_osi_suser(*acred))
	return EACCES;

    if (afs_pd_getInt(ain, &flags) != 0)
	return EINVAL;

    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK))
	return EINVAL;

    if (flags & AFSCALL_RXSTATS_ENABLE) {
	rx_enablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
	rx_disablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
	rx_clearPeerRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
    return 0;
}

DECL_PIOCTL(PPrefetchFromTape)
{
    afs_int32 code;
    afs_int32 outval;
    struct afs_conn *tc;
    struct rx_call *tcall;
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
    struct VenusFid tfid;
    struct AFSFid *Fid;
    struct vcache *tvc;
    struct rx_connection *rxconn;

    AFS_STATCNT(PPrefetchFromTape);
    if (!avc)
	return EINVAL;

    Fid = afs_pd_inline(ain, sizeof(struct AFSFid));
    if (Fid == NULL)
	Fid = &avc->f.fid.Fid;

    tfid.Cell = avc->f.fid.Cell;
    tfid.Fid.Volume = Fid->Volume;
    tfid.Fid.Vnode = Fid->Vnode;
    tfid.Fid.Unique = Fid->Unique;

    tvc = afs_GetVCache(&tfid, areq, NULL, NULL);
    if (!tvc) {
	afs_Trace3(afs_iclSetp, CM_TRACE_PREFETCHCMD, ICL_TYPE_POINTER, tvc,
		   ICL_TYPE_FID, &tfid, ICL_TYPE_FID, &avc->f.fid);
	return ENOENT;
    }
    afs_Trace3(afs_iclSetp, CM_TRACE_PREFETCHCMD, ICL_TYPE_POINTER, tvc,
	       ICL_TYPE_FID, &tfid, ICL_TYPE_FID, &tvc->f.fid);

    do {
	tc = afs_Conn(&tvc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {

	    RX_AFS_GUNLOCK();
	    tcall = rx_NewCall(rxconn);
	    code =
		StartRXAFS_FetchData(tcall, (struct AFSFid *)&tvc->f.fid.Fid, 0,
				     0);
	    if (!code) {
		rx_Read(tcall, (char *)&outval, sizeof(afs_int32));
		code =
		    EndRXAFS_FetchData(tcall, &OutStatus, &CallBack, &tsync);
	    }
	    code = rx_EndCall(tcall, code);
	    RX_AFS_GLOCK();
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &tvc->f.fid, areq, AFS_STATS_FS_RPCIDX_RESIDENCYRPCS,
	      SHARED_LOCK, NULL));
    /* This call is done only to have the callback things handled correctly */
    afs_FetchStatus(tvc, &tfid, areq, &OutStatus);
    afs_PutVCache(tvc);

    if (code)
	return code;

    return afs_pd_putInt(aout, outval);
}

DECL_PIOCTL(PFsCmd)
{
    afs_int32 code;
    struct afs_conn *tc;
    struct vcache *tvc;
    struct FsCmdInputs *Inputs;
    struct FsCmdOutputs *Outputs;
    struct VenusFid tfid;
    struct AFSFid *Fid;
    struct rx_connection *rxconn;

    if (!avc)
	return EINVAL;

    Inputs = afs_pd_inline(ain, sizeof(*Inputs));
    if (Inputs == NULL)
	return EINVAL;

    Outputs = afs_pd_inline(aout, sizeof(*Outputs));
    if (Outputs == NULL)
	return E2BIG;

    Fid = &Inputs->fid;
    if (!Fid->Volume)
	Fid = &avc->f.fid.Fid;

    tfid.Cell = avc->f.fid.Cell;
    tfid.Fid.Volume = Fid->Volume;
    tfid.Fid.Vnode = Fid->Vnode;
    tfid.Fid.Unique = Fid->Unique;

    tvc = afs_GetVCache(&tfid, areq, NULL, NULL);
    afs_Trace3(afs_iclSetp, CM_TRACE_RESIDCMD, ICL_TYPE_POINTER, tvc,
	       ICL_TYPE_INT32, Inputs->command, ICL_TYPE_FID, &tfid);
    if (!tvc)
	return ENOENT;

    if (Inputs->command) {
	do {
	    tc = afs_Conn(&tvc->f.fid, areq, SHARED_LOCK, &rxconn);
	    if (tc) {
		RX_AFS_GUNLOCK();
		code =
		    RXAFS_FsCmd(rxconn, Fid, Inputs, Outputs);
		RX_AFS_GLOCK();
	    } else
		code = -1;
	} while (afs_Analyze
		 (tc, rxconn, code, &tvc->f.fid, areq,
		  AFS_STATS_FS_RPCIDX_RESIDENCYRPCS, SHARED_LOCK, NULL));
	/* This call is done to have the callback things handled correctly */
	afs_FetchStatus(tvc, &tfid, areq, &Outputs->status);
    } else {		/* just a status request, return also link data */
	code = 0;
	Outputs->code = afs_FetchStatus(tvc, &tfid, areq, &Outputs->status);
	Outputs->chars[0] = 0;
	if (vType(tvc) == VLNK) {
	    ObtainWriteLock(&tvc->lock, 555);
	    if (afs_HandleLink(tvc, areq) == 0)
		strncpy((char *)&Outputs->chars, tvc->linkData, MAXCMDCHARS);
	    ReleaseWriteLock(&tvc->lock);
	}
    }

    afs_PutVCache(tvc);

    return code;
}

DECL_PIOCTL(PNewUuid)
{
    /*AFS_STATCNT(PNewUuid); */
    if (!afs_resourceinit_flag)	/* afs deamons havn't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (!afs_osi_suser(*acred))
	return EACCES;

    ObtainWriteLock(&afs_xinterface, 555);
    afs_uuid_create(&afs_cb_interface.uuid);
    ReleaseWriteLock(&afs_xinterface);
    ForceAllNewConnections();
    return 0;
}

#if defined(AFS_CACHE_BYPASS) && defined(AFS_LINUX24_ENV)

DECL_PIOCTL(PSetCachingThreshold)
{
    afs_int32 getting = 1;
    afs_int32 setting = 1;
    afs_int32 threshold = AFS_CACHE_BYPASS_DISABLED;

    if (afs_pd_getInt(ain, &threshold) != 0)
	setting = 0;

    if (aout == NULL)
	getting = 0;

    if (setting == 0 && getting == 0)
	return EINVAL;

    /*
     * If setting, set first, and return the value now in effect
     */
    if (setting) {
	if (!afs_osi_suser(*acred))
	    return EPERM;
	cache_bypass_threshold = threshold;
        afs_warn("Cache Bypass Threshold set to: %d\n", threshold);
	/* TODO:  move to separate pioctl, or enhance pioctl */
	cache_bypass_strategy = LARGE_FILES_BYPASS_CACHE;
    }

    /* Return the current size threshold */
    if (getting)
	return afs_pd_putInt(aout, cache_bypass_threshold);

    return(0);
}

#endif /* defined(AFS_CACHE_BYPASS) */

DECL_PIOCTL(PCallBackAddr)
{
#ifndef UKERNEL
    afs_uint32 addr, code;
    int srvAddrCount;
    struct server *ts;
    struct srvAddr *sa;
    struct afs_conn *tc;
    afs_int32 i, j;
    struct unixuser *tu;
    struct srvAddr **addrs;
    struct rx_connection *rxconn;

    /*AFS_STATCNT(PCallBackAddr); */
    if (!afs_resourceinit_flag)	/* afs deamons havn't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (!afs_osi_suser(acred))
	return EACCES;

    if (afs_pd_getInt(ain, &addr) != 0)
	return EINVAL;

    ObtainReadLock(&afs_xinterface);
    for (i = 0; (unsigned short)i < afs_cb_interface.numberOfInterfaces; i++) {
	if (afs_cb_interface.addr_in[i] == addr)
	    break;
    }

    ReleaseWriteLock(&afs_xinterface);

    if (afs_cb_interface.addr_in[i] != addr)
	return EINVAL;

    ObtainReadLock(&afs_xserver);	/* Necessary? */
    ObtainReadLock(&afs_xsrvAddr);

    srvAddrCount = 0;
    for (i = 0; i < NSERVERS; i++) {
	for (sa = afs_srvAddrs[i]; sa; sa = sa->next_bkt) {
	    srvAddrCount++;
	}
    }

    addrs = afs_osi_Alloc(srvAddrCount * sizeof(*addrs));
    osi_Assert(addrs != NULL);
    j = 0;
    for (i = 0; i < NSERVERS; i++) {
	for (sa = afs_srvAddrs[i]; sa; sa = sa->next_bkt) {
	    if (j >= srvAddrCount)
		break;
	    addrs[j++] = sa;
	}
    }

    ReleaseReadLock(&afs_xsrvAddr);
    ReleaseReadLock(&afs_xserver);

    for (i = 0; i < j; i++) {
	sa = addrs[i];
	ts = sa->server;
	if (!ts)
	    continue;

	/* vlserver has no callback conn */
	if (sa->sa_portal == AFS_VLPORT) {
	    continue;
	}

	if (!ts->cell)		/* not really an active server, anyway, it must */
	    continue;		/* have just been added by setsprefs */

	/* get a connection, even if host is down; bumps conn ref count */
	tu = afs_GetUser(areq->uid, ts->cell->cellNum, SHARED_LOCK);
	tc = afs_ConnBySA(sa, ts->cell->fsport, ts->cell->cellNum, tu,
			  1 /*force */ , 1 /*create */ , SHARED_LOCK, 0, &rxconn);
	afs_PutUser(tu, SHARED_LOCK);
	if (!tc)
	    continue;

	if ((sa->sa_flags & SRVADDR_ISDOWN) || afs_HaveCallBacksFrom(ts)) {
	    if (sa->sa_flags & SRVADDR_ISDOWN) {
		rx_SetConnDeadTime(rxconn, 3);
	    }
#ifdef RX_ENABLE_LOCKS
	    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    code = RXAFS_CallBackRxConnAddr(rxconn, &addr);
#ifdef RX_ENABLE_LOCKS
	    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	}
	afs_PutConn(tc, rxconn, SHARED_LOCK);	/* done with it now */
    }				/* Outer loop over addrs */
#endif /* UKERNEL */
    return 0;
}

DECL_PIOCTL(PDiscon)
{
    static afs_int32 mode = 1; /* Start up in 'off' */
    afs_int32 force = 0;
    int code = 0;
    char flags[4];
    struct vrequest lreq;

    if (afs_pd_getBytes(ain, &flags, 4) == 0) {
	if (!afs_osi_suser(*acred))
	    return EPERM;

	if (flags[0])
	    mode = flags[0] - 1;
	if (flags[1])
	    afs_ConflictPolicy = flags[1] - 1;
	if (flags[2])
	    force = 1;
	if (flags[3]) {
	    /* Fake InitReq support for UID override */
	    memset(&lreq, 0, sizeof(lreq));
	    lreq.uid = flags[3];
	    areq = &lreq; /* override areq we got */
	}

	/*
	 * All of these numbers are hard coded in fs.c. If they
	 * change here, they should change there and vice versa
	 */
	switch (mode) {
	case 0: /* Disconnect ("offline" mode), breaking all callbacks */
	    if (!AFS_IS_DISCONNECTED) {
		ObtainWriteLock(&afs_discon_lock, 999);
		afs_DisconGiveUpCallbacks();
		afs_RemoveAllConns();
		afs_is_disconnected = 1;
		afs_is_discon_rw = 1;
		ReleaseWriteLock(&afs_discon_lock);
	    }
	    break;
	case 1: /* Fully connected, ("online" mode). */
	    ObtainWriteLock(&afs_discon_lock, 998);

	    afs_in_sync = 1;
	    afs_MarkAllServersUp();
	    code = afs_ResyncDisconFiles(areq, *acred);
	    afs_in_sync = 0;

	    if (code && !force) {
	    	afs_warnuser("Files not synchronized properly, still in discon state. \n"
		       "Please retry or use \"force\".\n");
		mode = 0;
	    } else {
		if (force) {
		    afs_DisconDiscardAll(*acred);
		}
	        afs_ClearAllStatdFlag();
		afs_is_disconnected = 0;
		afs_is_discon_rw = 0;
		afs_warnuser("\nSync succeeded. You are back online.\n");
	    }

	    ReleaseWriteLock(&afs_discon_lock);
	    break;
	default:
	    return EINVAL;
	}
    } else {
	return EINVAL;
    }

    if (code)
	return code;

    return afs_pd_putInt(aout, mode);
}

DECL_PIOCTL(PNFSNukeCreds)
{
    afs_uint32 addr;
    afs_int32 i;
    struct unixuser *tu;

    AFS_STATCNT(PUnlog);
    if (!afs_resourceinit_flag)	/* afs daemons haven't started yet */
	return EIO;		/* Inappropriate ioctl for device */

    if (afs_pd_getUint(ain, &addr) != 0)
	return EINVAL;

    if (afs_cr_gid(*acred) == RMTUSER_REQ_PRIV && !addr) {
	tu = afs_GetUser(areq->uid, -1, SHARED_LOCK);
	if (!tu->exporter || !(addr = EXP_GETHOST(tu->exporter))) {
	    afs_PutUser(tu, SHARED_LOCK);
	    return EACCES;
	}
	afs_PutUser(tu, SHARED_LOCK);
    } else if (!afs_osi_suser(acred)) {
	return EACCES;
    }

    ObtainWriteLock(&afs_xuser, 227);
    for (i = 0; i < NUSERS; i++) {
	for (tu = afs_users[i]; tu; tu = tu->next) {
	    if (tu->exporter && EXP_CHECKHOST(tu->exporter, addr)) {
		tu->vid = UNDEFVID;
		tu->states &= ~UHasTokens;
		/* security is not having to say you're sorry */
		memset(&tu->ct, 0, sizeof(struct ClearToken));
		tu->refCount++;
		ReleaseWriteLock(&afs_xuser);
		afs_ResetUserConns(tu);
		tu->refCount--;
		ObtainWriteLock(&afs_xuser, 228);
#ifdef UKERNEL
		/* set the expire times to 0, causes
		 * afs_GCUserData to remove this entry
		 */
		tu->ct.EndTimestamp = 0;
		tu->tokenTime = 0;
#endif /* UKERNEL */
	    }
	}
    }
    ReleaseWriteLock(&afs_xuser);
    return 0;
}
