/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */
#include "../afs/vice.h"
#include "../rx/rx_globals.h"

extern void afs_ComputePAGStats();
extern struct vcache *afs_LookupVCache();
struct VenusFid afs_rootFid;
afs_int32 afs_waitForever=0;
short afs_waitForeverCount = 0;
afs_int32 afs_showflags = GAGUSER | GAGCONSOLE;   /* show all messages */
extern afs_int32 PROBE_INTERVAL;

extern int cacheDiskType;
extern afs_int32 afs_cacheBlocks;
extern struct afs_q CellLRU;
extern char *afs_indexFlags; /* only one: is there data there? */
extern afs_int32 afs_blocksUsed;
extern struct unixuser *afs_users[NUSERS];
extern struct server *afs_servers[NSERVERS];
extern struct interfaceAddr afs_cb_interface; /* client interface addresses */
extern afs_rwlock_t afs_xserver;
extern afs_rwlock_t afs_xinterface;
extern afs_rwlock_t afs_xcell;
extern afs_rwlock_t afs_xuser;
#ifndef	AFS_FINEG_SUNLOCK
extern afs_rwlock_t afs_xconn;
#endif
extern afs_rwlock_t afs_xvolume;
extern afs_lock_t afs_xdcache;		    /* lock: alloc new disk cache entries */
extern afs_rwlock_t afs_xvcache;  
extern afs_rwlock_t afs_xcbhash;  
extern afs_int32 afs_mariner, afs_marinerHost;
extern struct srvAddr *afs_srvAddrs[NSERVERS];
extern int afs_resourceinit_flag;

static int PBogus(), PSetAcl(), PGetAcl(), PSetTokens(), PGetVolumeStatus();
static int PSetVolumeStatus(), PFlush(), PNewStatMount(), PGetTokens(), PUnlog();
static int PCheckServers(), PCheckVolNames(), PCheckAuth(), PFindVolume();
static int PViceAccess(), PSetCacheSize(), Prefetch();
static int PRemoveCallBack(), PNewCell(), PListCells(), PRemoveMount();
static int PMariner(), PGetUserCell(), PGetWSCell(), PGetFileCell();
static int PVenusLogging(), PNoop(), PSetCellStatus(), PGetCellStatus();
static int PFlushVolumeData(), PGetCacheSize();
static int PSetSysName(),PGetFID();
static int PGetVnodeXStatus();
static int PSetSPrefs(), PGetSPrefs(), PGag(), PTwiddleRx();
static int PSetSPrefs33(), PStoreBehind(), PGCPAGs();
static int PGetCPrefs(), PSetCPrefs(); /* client network addresses */
static int PGetInitParams(), PFlushMount(), PRxStatProc(), PRxStatPeer();
int PExportAfs();

static int HandleClientContext(struct afs_ioctl *ablob, int *com, struct AFS_UCRED **acred, struct AFS_UCRED *credp);

extern struct cm_initparams cm_initParams;

static int (*(pioctlSw[]))() = {
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
  PFindVolume,		/* 14*/
  PBogus,			/* 15 -- prefetch is now special-cased; see pioctl code! */
  PBogus,			/* 16 -- used to be testing code */
  PNoop,			/* 17 -- used to be enable group */
  PNoop,		    	/* 18 -- used to be disable group */
  PBogus,			/* 19 -- used to be list group */
  PViceAccess,		/* 20 */
  PUnlog,			/* 21 -- unlog *is* unpag in this system */
  PGetFID,			/* 22 -- get file ID */
  PBogus,			/* 23 -- used to be waitforever */
  PSetCacheSize,		/* 24 */
  PRemoveCallBack,		/* 25 -- flush only the callback */
  PNewCell,			/* 26 */
  PListCells,		    	/* 27 */
  PRemoveMount,		/* 28 -- delete mount point */
  PNewStatMount,		/* 29 -- new style mount point stat */
  PGetFileCell,		/* 30 -- get cell name for input file */
  PGetWSCell,			/* 31 -- get cell name for workstation */
  PMariner,			/* 32 - set/get mariner host */
  PGetUserCell,		/* 33 -- get cell name for user */
  PVenusLogging,		/* 34 -- Enable/Disable logging */
  PGetCellStatus,		/* 35 */
  PSetCellStatus,		/* 36 */
  PFlushVolumeData,		/* 37 -- flush all data from a volume */
  PSetSysName,		/* 38 - Set system name */
  PExportAfs,			/* 39 - Export Afs to remote nfs clients */
  PGetCacheSize,		/* 40 - get cache size and usage */
  PGetVnodeXStatus,		/* 41 - get vcache's special status */
  PSetSPrefs33,      		/* 42 - Set CM Server preferences... */
  PGetSPrefs,      		/* 43 - Get CM Server preferences... */
  PGag,                         /* 44 - turn off/on all CM messages */
  PTwiddleRx,                   /* 45 - adjust some RX params       */
  PSetSPrefs,      		/* 46 - Set CM Server preferences... */
  PStoreBehind,      		/* 47 - set degree of store behind to be done */
  PGCPAGs,			/* 48 - disable automatic pag gc-ing */
  PGetInitParams,		/* 49 - get initial cm params */
  PGetCPrefs,			/* 50 - get client interface addresses */
  PSetCPrefs,			/* 51 - set client interface addresses */
  PFlushMount,			/* 52 - flush mount symlink data */
  PRxStatProc,			/* 53 - control process RX statistics */
  PRxStatPeer,			/* 54 - control peer RX statistics */
};

#define PSetClientContext 99	/*  Special pioctl to setup caller's creds  */
int afs_nobody = NFS_NOBODY;

static void
afs_ioctl32_to_afs_ioctl(const struct afs_ioctl32 *src, struct afs_ioctl *dst)
{
	dst->in       = (char *)(unsigned long)src->in;
	dst->out      = (char *)(unsigned long)src->out;
	dst->in_size  = src->in_size;
	dst->out_size = src->out_size;
}

/*
 * If you need to change copyin_afs_ioctl(), you may also need to change
 * copyin_iparam().
 */

static int
copyin_afs_ioctl(caddr_t cmarg, struct afs_ioctl *dst)
{
	int code;

#if defined(AFS_HPUX_64BIT_ENV)
	struct afs_ioctl32 dst32;

	if (is_32bit(u.u_procp))	/* is_32bit() in proc_iface.h */
	{
		AFS_COPYIN(cmarg, (caddr_t) &dst32, sizeof dst32, code);
		if (!code)
			afs_ioctl32_to_afs_ioctl(&dst32, dst);
		return code;
	}
#endif /* defined(AFS_HPUX_64BIT_ENV) */

#if defined(AFS_SUN57_64BIT_ENV)
	struct afs_ioctl32 dst32;

	if (get_udatamodel() == DATAMODEL_ILP32) {
		AFS_COPYIN(cmarg, (caddr_t) &dst32, sizeof dst32, code);
		if (!code)
			afs_ioctl32_to_afs_ioctl(&dst32, dst);
		return code;
	}
#endif /* defined(AFS_SUN57_64BIT_ENV) */

#if defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64)
	struct afs_ioctl32 dst32;

	if (!ABI_IS_64BIT(get_current_abi()))
	{
		AFS_COPYIN(cmarg, (caddr_t) &dst32, sizeof dst32, code);
		if (!code)
			afs_ioctl32_to_afs_ioctl(&dst32, dst);
		return code;
	}
#endif /* defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64) */

	AFS_COPYIN(cmarg, (caddr_t) dst, sizeof *dst, code);
	return code;
}

HandleIoctl(avc, acom, adata)
     register struct vcache *avc;
     register afs_int32 acom;
     struct afs_ioctl *adata; {
       register afs_int32 code;
       
       code = 0;
       AFS_STATCNT(HandleIoctl);
       
       switch(acom & 0xff) {
       case 1:
	 avc->states |= CSafeStore;
	 avc->asynchrony = 0;
	 break;
	 
	 /* case 2 used to be abort store, but this is no longer provided,
	    since it is impossible to implement under normal Unix.
	    */
	 
       case 3: {
	 /* return the name of the cell this file is open on */
	 register struct cell *tcell;
	 register afs_int32 i;
	 
	 tcell = afs_GetCell(avc->fid.Cell, READ_LOCK);
	 if (tcell) {
	   i = strlen(tcell->cellName) + 1;    /* bytes to copy out */

	   if (i > adata->out_size) {
	     /* 0 means we're not interested in the output */
	     if (adata->out_size != 0) code = EFAULT;
	   }
	   else {
	     /* do the copy */
	     AFS_COPYOUT(tcell->cellName, adata->out, i, code);
	   }
	   afs_PutCell(tcell, READ_LOCK);
	 }
	 else code = ENOTTY;
       }
	 break;
	 
     case 49: /* VIOC_GETINITPARAMS */
	 if (adata->out_size < sizeof(struct cm_initparams)) {
	     code = EFAULT;
	 }
	 else {
	     AFS_COPYOUT(&cm_initParams, adata->out,
			 sizeof(struct cm_initparams), code);
	 }
	 break;
	 
       default:

	 code = EINVAL;
	 break;
       }
       return code;		/* so far, none implemented */
     }
     
     
#ifdef	AFS_AIX_ENV
/* For aix we don't temporarily bypass ioctl(2) but rather do our
 * thing directly in the vnode layer call, VNOP_IOCTL; thus afs_ioctl
 * is now called from afs_gn_ioctl.
 */
afs_ioctl(tvc, cmd, arg)
     struct	vcache *tvc;
     int	cmd;
     int	arg;
{
  struct afs_ioctl data;
  int error = 0;
  
  AFS_STATCNT(afs_ioctl);
  if (((cmd >> 8) & 0xff) == 'V') {
    /* This is a VICEIOCTL call */
    AFS_COPYIN(arg, (caddr_t) &data, sizeof(data), error);
    if (error)
      return(error);
    error = HandleIoctl(tvc, cmd, &data);
    return(error);
  } else {
    /* No-op call; just return. */
    return(ENOTTY);
  }
}
#endif /* AFS_AIX_ENV */

#if defined(AFS_SGI_ENV)
afs_ioctl(OSI_VN_DECL(tvc), int cmd, void * arg, int flag, cred_t *cr, rval_t *rvalp
#ifdef AFS_SGI65_ENV
	  , struct vopbd *vbds
#endif
	  )
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
      return(error);
    locked = ISAFS_GLOCK();
    if (!locked)
	AFS_GLOCK();
    error = HandleIoctl(tvc, cmd, &data);
    if (!locked)
	AFS_GUNLOCK();
    return(error);
  } else {
    /* No-op call; just return. */
    return(ENOTTY);
  }
}
#endif /* AFS_SGI_ENV */


/* unlike most calls here, this one uses u.u_error to return error conditions,
   since this is really an intercepted chapter 2 call, rather than a vnode
   interface call.
   */
/* AFS_HPUX102 and up uses VNODE ioctl instead */
#ifndef AFS_HPUX102_ENV
#if !defined(AFS_SGI_ENV)
#ifdef	AFS_AIX32_ENV
kioctl(fdes, com, arg, ext)
     int fdes, com;
     caddr_t arg, ext;
{
  struct a {
    int fd, com;
    caddr_t arg, ext;
  } u_uap, *uap = &u_uap;
#else
#ifdef	AFS_SUN5_ENV

struct afs_ioctl_sys {
    int fd;
    int com;
    int arg;
};

afs_xioctl (uap, rvp) 
    struct afs_ioctl_sys *uap;
    rval_t *rvp;
{
#else
#ifdef AFS_OSF_ENV
afs_xioctl (p, args, retval)
        struct proc *p;
        void *args;
        long *retval;
{
    struct a {
        long fd;
        u_long com;
        caddr_t arg;
    } *uap = (struct a *)args;
#else /* AFS_OSF_ENV */
#ifdef AFS_LINUX22_ENV
struct afs_ioctl_sys {
    unsigned int com;
    unsigned long arg;
};
asmlinkage int afs_xioctl(struct inode *ip, struct file *fp,
			  unsigned int com, unsigned long arg)
{
    struct afs_ioctl_sys ua, *uap = &ua;
#else
afs_xioctl () 
    {
      register struct a {
	int fd;
	int com;
	caddr_t arg;
      } *uap = (struct a *)u.u_ap;
#endif /* AFS_LINUX22_ENV */
#endif /* AFS_OSF_ENV */
#endif	/* AFS_SUN5_ENV */
#endif
#ifndef AFS_LINUX22_ENV
#if	defined(AFS_AIX32_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV)
      struct file *fd;
#else
      register struct file *fd;
#endif
#endif
      register struct vcache *tvc;
      register int ioctlDone = 0, code = 0;
      
      AFS_STATCNT(afs_xioctl);
#ifdef AFS_LINUX22_ENV
    ua.com = com;
    ua.arg = arg;
#else
#ifdef	AFS_AIX32_ENV
      uap->fd = fdes;
      uap->com = com;
      uap->arg = arg;
      if (setuerror(getf(uap->fd, &fd))) {
	return -1;
    }
#else
#ifdef  AFS_OSF_ENV
    fd = NULL;
    if (code = getf(&fd, uap->fd, FILE_FLAGS_NULL, &u.u_file_state))
        return code;
#else   /* AFS_OSF_ENV */
#ifdef	AFS_SUN5_ENV
#if defined(AFS_SUN57_ENV)
	fd = getf(uap->fd);
        if (!fd) return(EBADF);
#elif defined(AFS_SUN54_ENV)
      fd = GETF(uap->fd);
      if (!fd) return(EBADF);
#else
      if (code = getf(uap->fd, &fd)) {
	  return (code);
      }
#endif
#else
      fd = getf(uap->fd);
      if (!fd) return;
#endif
#endif
#endif
#endif
      
      /* first determine whether this is any sort of vnode */
#ifdef AFS_LINUX22_ENV
      tvc = (struct vcache *)ip;
      {
#else
#ifdef	AFS_SUN5_ENV
      if (fd->f_vnode->v_type == VREG || fd->f_vnode->v_type == VDIR) {
#else
      if (fd->f_type == DTYPE_VNODE) {
#endif
	/* good, this is a vnode; next see if it is an AFS vnode */
#if	defined(AFS_AIX32_ENV) || defined(AFS_SUN5_ENV)
	tvc = (struct vcache *) fd->f_vnode;	/* valid, given a vnode */
#else
	tvc = (struct vcache *) fd->f_data;	/* valid, given a vnode */
#endif
#endif /* AFS_LINUX22_ENV */
	if (tvc && IsAfsVnode((struct vnode *)tvc)) {
#ifdef AFS_DEC_ENV
	  tvc = (struct vcache *) afs_gntovn((struct gnode *) tvc);
	  if (!tvc) {	/* shouldn't happen with held gnodes */
	    u.u_error = ENOENT;
	    return;
	  }
#endif
	  /* This is an AFS vnode */
	  if (((uap->com >> 8) & 0xff) == 'V') {
	    register struct afs_ioctl *datap;
	    AFS_GLOCK();
	    datap = (struct afs_ioctl *) osi_AllocSmallSpace(AFS_SMALLOCSIZ);
	    AFS_COPYIN((char *)uap->arg, (caddr_t) datap, sizeof (struct afs_ioctl), code);
	    if (code) {
	      osi_FreeSmallSpace(datap);
	      AFS_GUNLOCK();
#if	defined(AFS_SUN5_ENV)
#ifdef	AFS_SUN54_ENV
	      releasef(uap->fd);
#else
	      releasef(fd);
#endif
	      return (EFAULT);
#else
#ifdef	AFS_OSF_ENV
#ifdef	AFS_OSF30_ENV
	      FP_UNREF_ALWAYS(fd);
#else
              FP_UNREF(fd);
#endif
              return code;
#else	/* AFS_OSF_ENV */
#ifdef	AFS_AIX41_ENV
	      ufdrele(uap->fd);
#endif
#ifdef AFS_LINUX22_ENV
	      return -code;
#else
	      setuerror(code);
	      return;
#endif
#endif
#endif
	    }
	    code = HandleIoctl(tvc, uap->com, datap);
	    osi_FreeSmallSpace(datap);
	    AFS_GUNLOCK();
	    ioctlDone = 1;
#ifdef	AFS_AIX41_ENV
	    ufdrele(uap->fd);
#endif
#ifdef	AFS_OSF_ENV
#ifdef	AFS_OSF30_ENV
	      FP_UNREF_ALWAYS(fd);
#else
              FP_UNREF(fd);
#endif
#endif
	  }
#if defined(AFS_LINUX22_ENV)
	  else 
	    code = EINVAL;
#endif
	}
      }

      if (!ioctlDone) {
#ifdef	AFS_AIX41_ENV
	  ufdrele(uap->fd);
	  code = okioctl(fdes, com, arg, ext);
	  return code;
#else
#ifdef	AFS_AIX32_ENV
	  okioctl(fdes, com, arg, ext);
#else
#if	defined(AFS_SUN5_ENV)
#if defined(AFS_SUN57_ENV)
	releasef(uap->fd);
#elif defined(AFS_SUN54_ENV)
	  RELEASEF(uap->fd);
#else
          releasef(fd);
#endif
          code = ioctl(uap, rvp);
#else
#ifdef  AFS_OSF_ENV
	  code = ioctl(p, args, retval);
#ifdef	AFS_OSF30_ENV
	  FP_UNREF_ALWAYS(fd);
#else
	  FP_UNREF(fd);
#endif
	  return code;
#else   /* AFS_OSF_ENV */
#ifndef AFS_LINUX22_ENV
          ioctl();
#endif
#endif
#endif
#endif
#endif
      }
#ifdef	AFS_SUN5_ENV
      if (ioctlDone)
#ifdef	AFS_SUN54_ENV
	  releasef(uap->fd);
#else
	  releasef(fd);
#endif
      return (code);
#else
#ifdef AFS_LINUX22_ENV
      return -code;
#else
#if	!defined(AFS_OSF_ENV)
      if (!getuerror())
	  setuerror(code);
#if	defined(AFS_AIX32_ENV) && !defined(AFS_AIX41_ENV)
      return (getuerror() ? -1 : u.u_ioctlrv);
#else
      return getuerror() ? -1 : 0;
#endif
#endif
#endif /* AFS_LINUX22_ENV */
#endif	/* AFS_SUN5_ENV */
#ifdef	AFS_OSF_ENV
      return (code);
#endif
    }
#endif /* AFS_SGI_ENV */
#endif /* AFS_HPUX102_ENV */
  
#if defined(AFS_SGI_ENV)
  /* "pioctl" system call entry point; just pass argument to the parameterized
		 call below */
struct pioctlargs {
	char		*path;
	sysarg_t	cmd;
	caddr_t 	cmarg;
	sysarg_t	follow;
};
int
afs_pioctl(struct pioctlargs *uap, rval_t *rvp)
{
    int code;

    AFS_STATCNT(afs_pioctl);
    AFS_GLOCK();
    code = afs_syscall_pioctl(uap->path, uap->cmd, uap->cmarg, uap->follow);
    AFS_GUNLOCK();
#ifdef AFS_SGI64_ENV
    return code;
#else
    return u.u_error;
#endif
}
#endif /* AFS_SGI_ENV */

#ifdef AFS_OSF_ENV
afs_pioctl(p, args, retval)
        struct proc *p;
        void *args;
        int *retval;
{
    struct a {
        char    *path;
        int     cmd;
        caddr_t cmarg;
        int     follow;
    } *uap = (struct a *) args;

    AFS_STATCNT(afs_pioctl);
    return (afs_syscall_pioctl(uap->path, uap->cmd, uap->cmarg, uap->follow));
}

extern struct mount *afs_globalVFS;
#else	/* AFS_OSF_ENV */
extern struct vfs *afs_globalVFS;
#endif

/* macro to avoid adding any more #ifdef's to pioctl code. */
#if defined(AFS_LINUX22_ENV) || defined(AFS_AIX41_ENV)
#define PIOCTL_FREE_CRED() crfree(credp)
#else
#define PIOCTL_FREE_CRED()
#endif

#ifdef	AFS_SUN5_ENV
afs_syscall_pioctl(path, com, cmarg, follow, rvp, credp)
    rval_t *rvp;
    struct AFS_UCRED *credp;
#else
afs_syscall_pioctl(path, com, cmarg, follow)
#endif
    char *path;
    int	com;
    caddr_t cmarg;
    int	follow;
{
    struct afs_ioctl data;
    struct AFS_UCRED *tmpcred, *foreigncreds = 0;
    register afs_int32 code = 0;
    struct vnode *vp;
#ifdef AFS_DEC_ENV
    struct vnode *gp;
#endif
#ifdef	AFS_AIX41_ENV
    struct ucred *credp = crref(); /* don't free until done! */
#endif
#ifdef AFS_LINUX22_ENV
    cred_t *credp = crref(); /* don't free until done! */
    struct dentry *dp;
#endif
    AFS_STATCNT(afs_syscall_pioctl);
    if (follow) follow = 1;	/* compat. with old venus */
#ifndef	AFS_SUN5_ENV
    if (! _VALIDVICEIOCTL(com)) {
	PIOCTL_FREE_CRED();
#ifdef AFS_OSF_ENV
        return EINVAL;
#else	/* AFS_OSF_ENV */
#if defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
        return EINVAL;
#else
	setuerror(EINVAL);
	return EINVAL;
#endif
#endif
    }
#endif
    code = copyin_afs_ioctl(cmarg, &data);
    if (code) {
	PIOCTL_FREE_CRED();
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
	return (code);
#else
	setuerror(code);
	return code;
#endif
  }
    if ((com & 0xff) == PSetClientContext) {
#ifdef AFS_LINUX22_ENV
	return EINVAL; /* Not handling these yet. */
#endif
#if	defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_LINUX22_ENV)
	code = HandleClientContext(&data, &com, &foreigncreds, credp);
#else
#if	defined(AFS_HPUX101_ENV)
	code=HandleClientContext(&data, &com, &foreigncreds, p_cred(u.u_procp));
#else
#ifdef AFS_SGI_ENV
	code = HandleClientContext(&data, &com, &foreigncreds, OSI_GET_CURRENT_CRED());
#else
	code = HandleClientContext(&data, &com, &foreigncreds, u.u_cred);
#endif /* AFS_SGI_ENV */
#endif
#endif
      if (code) {
	  if (foreigncreds) {
	      crfree(foreigncreds);
	  }
	  PIOCTL_FREE_CRED();
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
	  return (code);
#else
	  return (setuerror(code), code);
#endif
      }
    } 
#ifndef AFS_LINUX22_ENV
    if (foreigncreds) {
      /*
       * We could have done without temporary setting the u.u_cred below
       * (foreigncreds could be passed as param the pioctl modules)
       * but calls such as afs_osi_suser() doesn't allow that since it
       * references u.u_cred directly.  We could, of course, do something
       * like afs_osi_suser(cred) which, I think, is better since it
       * generalizes and supports multi cred environments...
       */
#ifdef	AFS_SUN5_ENV
	tmpcred = credp;
	credp = foreigncreds;
#else
#ifdef	AFS_AIX41_ENV
	tmpcred = crref();	/* XXX */
	crset(foreigncreds);
#else
#if	defined(AFS_HPUX101_ENV)
	tmpcred = p_cred(u.u_procp);
	set_p_cred(u.u_procp, foreigncreds);
#else
#ifdef AFS_SGI_ENV
        tmpcred = OSI_GET_CURRENT_CRED();
        OSI_SET_CURRENT_CRED(foreigncreds);
#else
        tmpcred = u.u_cred;
        u.u_cred = foreigncreds;
#endif /* AFS_SGI64_ENV */
#endif /* AFS_HPUX101_ENV */
#endif
#endif
    }
#endif
    if ((com & 0xff) == 15) {
      /* special case prefetch so entire pathname eval occurs in helper process.
	 otherwise, the pioctl call is essentially useless */
#if	defined(AFS_SUN5_ENV) || defined(AFS_AIX41_ENV) || defined(AFS_LINUX22_ENV)
	code =  Prefetch(path, &data, follow,
			 foreigncreds ? foreigncreds : credp);
#else
#if	defined(AFS_HPUX101_ENV)
	code =  Prefetch(path, &data, follow, p_cred(u.u_procp));
#else
#ifdef AFS_SGI_ENV 
	code =  Prefetch(path, &data, follow, OSI_GET_CURRENT_CRED());
#else
	code =  Prefetch(path, &data, follow, u.u_cred);
#endif /* AFS_SGI64_ENV */
#endif /* AFS_HPUX101_ENV */
#endif
#ifndef AFS_LINUX22_ENV
	if (foreigncreds) {
#ifdef	AFS_AIX41_ENV
 	    crset(tmpcred);	/* restore original credentials */
#else
#if	defined(AFS_HPUX101_ENV)
	set_p_cred(u.u_procp, tmpcred); /* restore original credentials */
#else
#ifndef	AFS_SUN5_ENV
#ifdef AFS_SGI_ENV
	    OSI_SET_CURRENT_CRED(tmpcred);	    /* restore original credentials */
#else
	    u.u_cred = tmpcred;	    /* restore original credentials */
#endif
#endif
#endif /* AFS_HPUX101_ENV */
	    crfree(foreigncreds);
#endif
	}
#endif /* AFS_LINUX22_ENV */
	PIOCTL_FREE_CRED();
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
	return (code);
#else
	return (setuerror(code), code);
#endif
    }
    if (path) {
	AFS_GUNLOCK();
#ifdef	AFS_AIX41_ENV
	code = lookupname(path, USR, follow, NULL, &vp,
			foreigncreds ? foreigncreds : credp);
#else
#ifdef AFS_LINUX22_ENV
	code = gop_lookupname(path, AFS_UIOUSER, follow,  (struct vnode **) 0, &dp);
	if (!code)
	    vp = (struct vcache *)dp->d_inode;
#else
	code = gop_lookupname(path, AFS_UIOUSER, follow,  (struct vnode **) 0, &vp);
#endif /* AFS_LINUX22_ENV */
#endif /* AFS_AIX41_ENV */
	AFS_GLOCK();
	if (code) {
#ifndef AFS_LINUX22_ENV
	    if (foreigncreds) {
#ifdef	AFS_AIX41_ENV
		crset(tmpcred);	/* restore original credentials */
#else
#if	defined(AFS_HPUX101_ENV)
	set_p_cred(u.u_procp, tmpcred); /* restore original credentials */
#else
#if	!defined(AFS_SUN5_ENV)
#ifdef AFS_SGI_ENV
		OSI_SET_CURRENT_CRED(tmpcred);	    /* restore original credentials */
#else
		u.u_cred = tmpcred;	    /* restore original credentials */
#endif /* AFS_SGI64_ENV */
#endif
#endif /* AFS_HPUX101_ENV */
		crfree(foreigncreds);
#endif
	    }
#endif /* AFS_LINUX22_ENV */
	    PIOCTL_FREE_CRED();
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
	    return (code);
#else
	    return(setuerror(code), code);
#endif
	}
    }
    else vp = (struct vnode *) 0;
    
    /* now make the call if we were passed no file, or were passed an AFS file */
    if (!vp || IsAfsVnode(vp)) {
#ifdef AFS_DEC_ENV
      /* Ultrix 4.0: can't get vcache entry unless we've got an AFS gnode.
       * So, we must test in this part of the code.  Also, must arrange to
       * GRELE the original gnode pointer when we're done, since in Ultrix 4.0,
       * we hold gnodes, whose references hold our vcache entries.
       */
      if (vp) {
	gp = vp;	/* remember for "put" */
	vp = (struct vnode *) afs_gntovn(vp);	/* get vcache from gp */
      }
      else gp = (struct vnode *) 0;
#endif 
#ifdef	AFS_SUN5_ENV
      code = afs_HandlePioctl(vp, com, &data, follow, &credp);
#else
#ifdef	AFS_AIX41_ENV
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
#else
#if	defined(AFS_HPUX101_ENV)
      {
	  struct ucred *cred = p_cred(u.u_procp);
	  code = afs_HandlePioctl(vp, com, &data, follow, &cred);
      }
#else
#ifdef AFS_SGI_ENV
      {
      struct cred *credp;
      credp = OSI_GET_CURRENT_CRED();
      code = afs_HandlePioctl(vp, com, &data, follow, &credp);
      }
#else
#ifdef AFS_LINUX22_ENV
      code = afs_HandlePioctl(vp, com, &data, follow, &credp);
#else
      code = afs_HandlePioctl(vp, com, &data, follow, &u.u_cred);
#endif
#endif /* AFS_SGI_ENV */
#endif /* AFS_HPUX101_ENV */
#endif /* AFS_AIX41_ENV */
#endif /* AFS_SUN5_ENV */
    } else {
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
	code = EINVAL;	/* not in /afs */
#else
	setuerror(EINVAL);
#endif
#ifdef AFS_DEC_ENV
	if (vp) {
	    GRELE(vp);
	    vp = (struct vnode *) 0;
	}
#endif
    }

#ifndef AFS_LINUX22_ENV
    if (foreigncreds) {
#ifdef	AFS_AIX41_ENV
	crset(tmpcred);
#else
#if	defined(AFS_HPUX101_ENV)
	set_p_cred(u.u_procp, tmpcred); /* restore original credentials */
#else
#ifndef	AFS_SUN5_ENV
#ifdef AFS_SGI_ENV
	OSI_SET_CURRENT_CRED(tmpcred);	    /* restore original credentials */
#else
	u.u_cred = tmpcred;	    /* restore original credentials */
#endif /* ASF_SGI64_ENV */
#endif
#endif /* AFS_HPUX101_ENV */
	crfree(foreigncreds);
#endif
    }
#endif /* AFS_LINUX22_ENV */
    if (vp) {
#ifdef AFS_LINUX22_ENV
	dput(dp);
#else
	AFS_RELE(vp);	/* put vnode back */
#endif
    }
    PIOCTL_FREE_CRED();
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
    return (code);
#else
    if (!getuerror()) 	
 	setuerror(code);
    return (getuerror());
#endif
}
  
  
afs_HandlePioctl(avc, acom, ablob, afollow, acred)
     register struct vcache *avc;
     afs_int32 acom;
     struct AFS_UCRED **acred;
     register struct afs_ioctl *ablob;
     int afollow;
{
    struct vrequest treq;
    register afs_int32 code;
    register afs_int32 function;
    afs_int32 inSize, outSize;
    char *inData, *outData;

    afs_Trace3(afs_iclSetp, CM_TRACE_PIOCTL, ICL_TYPE_INT32, acom & 0xff,
	       ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, afollow);
    AFS_STATCNT(HandlePioctl);
    if (code = afs_InitReq(&treq, *acred)) return code;
    function = acom & 0xff;
    if (function >= (sizeof(pioctlSw) / sizeof(char *))) {
      return EINVAL;	/* out of range */
    }
    inSize = ablob->in_size;
    if (inSize >= PIGGYSIZE) return E2BIG;
    inData = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    if (inSize > 0) {
      AFS_COPYIN(ablob->in, inData, inSize, code);
    }
    else code = 0;
    if (code) {
      osi_FreeLargeSpace(inData);
      return code;
    }
    outData = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    outSize = 0;
    if (function == 3)	/* PSetTokens */
	code = (*pioctlSw[function])(avc, function, &treq, inData, outData, inSize, &outSize, acred);
    else
	code = (*pioctlSw[function])(avc, function, &treq, inData, outData, inSize, &outSize, *acred);
    osi_FreeLargeSpace(inData);
    if (code == 0 && ablob->out_size > 0) {
      if (outSize > ablob->out_size) outSize = ablob->out_size;
      if (outSize >= PIGGYSIZE) code = E2BIG;
      else if	(outSize) 
	AFS_COPYOUT(outData, ablob->out, outSize, code);
    }
    osi_FreeLargeSpace(outData);
    return afs_CheckCode(code, &treq, 41);
  }
  
  static PGetFID(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
  int afun;
  struct vrequest *areq;
  char *ain, *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 code;
    
    AFS_STATCNT(PGetFID);
    if (!avc) return EINVAL;
    bcopy((char *)&avc->fid, aout, sizeof(struct VenusFid));
    *aoutSize = sizeof(struct VenusFid);
    return 0;
  }
  
static PSetAcl(avc, afun, areq, ain, aout, ainSize, aoutSize)
  struct vcache *avc;
  int afun;
  struct vrequest *areq;
  char *ain, *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 code;
    struct conn *tconn;
    struct AFSOpaque acl;
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    XSTATS_DECLS;
    
    AFS_STATCNT(PSetAcl);
    if (!avc)
      return EINVAL;
    if ((acl.AFSOpaque_len = strlen(ain)+1) > 1000)
      return EINVAL;

    acl.AFSOpaque_val = ain;
    do {
      tconn = afs_Conn(&avc->fid, areq, SHARED_LOCK);
      if (tconn) {
	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STOREACL);
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	code = RXAFS_StoreACL(tconn->id, (struct AFSFid *) &avc->fid.Fid,
			      &acl, &OutStatus, &tsync);
#ifdef RX_ENABLE_LOCKS
	AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	XSTATS_END_TIME;
      }
      else code = -1;
    } while
	(afs_Analyze(tconn, code, &avc->fid, areq,
		     AFS_STATS_FS_RPCIDX_STOREACL, SHARED_LOCK, (struct cell *)0));

    /* now we've forgotten all of the access info */
    ObtainWriteLock(&afs_xcbhash, 455);
    avc->callback = 0;
    afs_DequeueCallback(avc);
    avc->states &= ~(CStatd | CUnique);
    ReleaseWriteLock(&afs_xcbhash);
    if (avc->fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	osi_dnlc_purgedp(avc);
    return code;
  }

int afs_defaultAsynchrony = 0;

static PStoreBehind(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
     struct vcache *avc;
     int afun;
     struct vrequest *areq;
     char *ain, *aout;
     afs_int32 ainSize;
     afs_int32 *aoutSize;	/* set this */
     struct AFS_UCRED *acred;
{
  afs_int32 code = 0;
  struct sbstruct *sbr;

  sbr = (struct sbstruct *)ain;
  if (sbr->sb_default != -1) {
    if (afs_osi_suser(acred))
      afs_defaultAsynchrony = sbr->sb_default;
    else code = EPERM;
  }

  if (avc && (sbr->sb_thisfile != -1))
    if (afs_AccessOK(avc, PRSFS_WRITE | PRSFS_ADMINISTER, 
		      areq, DONT_CHECK_MODE_BITS))
      avc->asynchrony = sbr->sb_thisfile;
    else code = EACCES;

  *aoutSize = sizeof(struct sbstruct);
  sbr = (struct sbstruct *)aout;
  sbr->sb_default = afs_defaultAsynchrony;
  if (avc) {
    sbr->sb_thisfile = avc->asynchrony;
  }

  return code;
}

static PGCPAGs(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
     struct vcache *avc;
     int afun;
     struct vrequest *areq;
     char *ain, *aout;
     afs_int32 ainSize;
     afs_int32 *aoutSize;	/* set this */
     struct AFS_UCRED *acred;
{
  if (!afs_osi_suser(acred)) {
    return EACCES;
  }
  afs_gcpags = AFS_GCPAGS_USERDISABLED;
  return 0;
}
  
  static PGetAcl(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
  int afun;
  struct vrequest *areq;
  char *ain, *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ {
    struct AFSOpaque acl;
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    afs_int32 code;
    struct conn *tconn;
    struct AFSFid Fid;
    XSTATS_DECLS;

    AFS_STATCNT(PGetAcl);
    if (!avc) return EINVAL;
    Fid.Volume = avc->fid.Fid.Volume;
    Fid.Vnode = avc->fid.Fid.Vnode;
    Fid.Unique = avc->fid.Fid.Unique;
    if (avc->states & CForeign) {
	/*
	 * For a dfs xlator acl we have a special hack so that the
	 * xlator will distinguish which type of acl will return. So
	 * we currently use the top 2-bytes (vals 0-4) to tell which
	 * type of acl to bring back. Horrible hack but this will
	 * cause the least number of changes to code size and interfaces.
         */
	if (Fid.Vnode & 0xc0000000)
	    return ERANGE;
	Fid.Vnode |= (ainSize << 30);
    }
    acl.AFSOpaque_val = aout;
    do {
      tconn = afs_Conn(&avc->fid, areq, SHARED_LOCK);
      if (tconn) {
	*aout = 0;
	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHACL);
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	code = RXAFS_FetchACL(tconn->id, &Fid,
			      &acl, &OutStatus, &tsync);
#ifdef RX_ENABLE_LOCKS
	AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	XSTATS_END_TIME;
      }
      else code = -1;
    } while
	(afs_Analyze(tconn, code, &avc->fid, areq,
		     AFS_STATS_FS_RPCIDX_FETCHACL,
		     SHARED_LOCK, (struct cell *)0));

    if (code == 0) {
      *aoutSize = (acl.AFSOpaque_len == 0 ? 1 : acl.AFSOpaque_len);
    }
    return code;
  }
  
  static PNoop() {
    AFS_STATCNT(PNoop);
    return 0;
  }
  
  static PBogus() {
    AFS_STATCNT(PBogus);
    return EINVAL;
  }
  
  static PGetFileCell(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
  int afun;
  struct vrequest *areq;
  register char *ain;
  char *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ {
    register struct cell *tcell;
    
    AFS_STATCNT(PGetFileCell);
    if (!avc) return EINVAL;
    tcell = afs_GetCell(avc->fid.Cell, READ_LOCK);
    if (!tcell) return ESRCH;
    strcpy(aout, tcell->cellName);
    afs_PutCell(tcell, READ_LOCK);
    *aoutSize = strlen(aout) + 1;
    return 0;
  }
  
  static PGetWSCell(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
  int afun;
  struct vrequest *areq;
  register char *ain;
  char *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ {
    register struct cell *tcell=0, *cellOne=0;
    register struct afs_q *cq, *tq;
    
    AFS_STATCNT(PGetWSCell);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    ObtainReadLock(&afs_xcell);
    cellOne = (struct cell *) 0;

    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tcell = QTOC(cq); tq = QNext(cq);
	if (tcell->states & CPrimary) break;
	if (tcell->cell == 1) cellOne = tcell;
	tcell = 0;
    }
    ReleaseReadLock(&afs_xcell);
    if (!tcell)	{	    /* no primary cell, use cell #1 */
      if (!cellOne) return ESRCH;
      tcell = cellOne;
    }
    strcpy(aout, tcell->cellName);
    *aoutSize = strlen(aout) + 1;
    return 0;
  }
  
  static PGetUserCell(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
  int afun;
  struct vrequest *areq;
  register char *ain;
  char *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 i;
    register struct unixuser *tu;
    register struct cell *tcell;
    
    AFS_STATCNT(PGetUserCell);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    /* return the cell name of the primary cell for this user */
    i = UHash(areq->uid);
    ObtainWriteLock(&afs_xuser,224);
    for(tu = afs_users[i]; tu; tu = tu->next) {
      if (tu->uid == areq->uid && (tu->states & UPrimary)) {
	tu->refCount++;
	ReleaseWriteLock(&afs_xuser);
	break;
      }
    }
    if (tu) {
      tcell = afs_GetCell(tu->cell, READ_LOCK);
      afs_PutUser(tu, WRITE_LOCK);
      if (!tcell) return ESRCH;
      else {
	strcpy(aout, tcell->cellName);
	afs_PutCell(tcell, READ_LOCK);
	*aoutSize =	strlen(aout)+1;	    /* 1 for the null */
      }
    }
    else {
      ReleaseWriteLock(&afs_xuser);
      *aout = 0;
      *aoutSize = 1;
    }
    return 0;
  }
  
  static PSetTokens(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
  int afun;
  struct vrequest *areq;
  register char *ain;
  char *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ 
  struct AFS_UCRED **acred;
{
    afs_int32 i;
    register struct unixuser *tu;
    struct ClearToken clear;
    register struct cell *tcell;
    char *stp;
    int stLen;
    struct vrequest treq;
    afs_int32 flag, set_parent_pag = 0;
    
    AFS_STATCNT(PSetTokens);
    if (!afs_resourceinit_flag) {
      return EIO;
    }
    bcopy(ain, (char *)&i, sizeof(afs_int32));
    ain += sizeof(afs_int32);
    stp	= ain;	/* remember where the ticket is */
    if (i < 0 || i > 2000) return EINVAL;	/* malloc may fail */
    stLen = i;
    ain	+= i;	/* skip over ticket */
    bcopy(ain, (char *)&i, sizeof(afs_int32));
    ain += sizeof(afs_int32);
    if (i != sizeof(struct ClearToken)) {
      return EINVAL;
    }
    bcopy(ain, (char *)&clear, sizeof(struct ClearToken));
    if (clear.AuthHandle == -1)	clear.AuthHandle = 999;	/* more rxvab compat stuff */
    ain += sizeof(struct ClearToken);
    if (ainSize != 2*sizeof(afs_int32) + stLen + sizeof(struct ClearToken)) {
      /* still stuff left?  we've got primary flag and cell name.  Set these */
      bcopy(ain, (char *)&flag, sizeof(afs_int32));		/* primary id flag */
      ain += sizeof(afs_int32);			/* skip id field */
      /* rest is cell name, look it up */
      if (flag & 0x8000) {			/* XXX Use Constant XXX */
	  flag &= ~0x8000;
	  set_parent_pag = 1;
      }
      tcell = afs_GetCellByName(ain, READ_LOCK);
      if (tcell) {
	i = tcell->cell;
      }
      else {
	goto nocell;
      }
    }
    else {
      /* default to cell 1, primary id */
      flag = 1;		/* primary id */
      i = 1;		/* cell number */
      tcell = afs_GetCell(1, READ_LOCK);
      if (!tcell) goto nocell;
    }
    afs_PutCell(tcell, READ_LOCK);
    if (set_parent_pag) {
	int pag;
#ifdef	AFS_OSF_ENV
	if (!setpag(u.u_procp, acred, -1, &pag, 1)) {	/* XXX u.u_procp is a no-op XXX */
#else
	if (!setpag(acred, -1, &pag, 1)) {
#endif
	    afs_InitReq(&treq, *acred);
	    areq = &treq;
	}
    }
    /* now we just set the tokens */
    tu = afs_GetUser(areq->uid,	i, WRITE_LOCK); /* i has the cell # */
    tu->vid = clear.ViceId;
    if (tu->stp != (char *) 0) {
      afs_osi_Free(tu->stp, tu->stLen);
    }
    tu->stp = (char *) afs_osi_Alloc(stLen);
    tu->stLen = stLen;
    bcopy(stp, tu->stp, stLen);
    tu->ct = clear;
#ifndef AFS_NOSTATS
    afs_stats_cmfullperf.authent.TicketUpdates++;
    afs_ComputePAGStats();
#endif /* AFS_NOSTATS */
    tu->states |= UHasTokens;
    tu->states &= ~UTokensBad;
    afs_SetPrimary(tu, flag);
    tu->tokenTime =osi_Time();
    afs_ResetUserConns(tu);
    afs_PutUser(tu, WRITE_LOCK);

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

static PGetVolumeStatus(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
  int afun;
  struct vrequest *areq;
  char *ain, *aout;
  afs_int32 ainSize;
  afs_int32 *aoutSize;	/* set this */ {
    char volName[32];
    char offLineMsg[256];
    char motd[256];
    register struct conn *tc;
    register afs_int32 code;
    struct VolumeStatus volstat;
    register char *cp;
    char *Name, *OfflineMsg, *MOTD;
    XSTATS_DECLS;

    AFS_STATCNT(PGetVolumeStatus);
    if (!avc) return EINVAL;
    Name = volName;
    OfflineMsg = offLineMsg;
    MOTD = motd;
    do {
      tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
      if (tc) {
	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_GETVOLUMESTATUS);
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	code = RXAFS_GetVolumeStatus(tc->id, avc->fid.Fid.Volume, &volstat,
				     &Name, &OfflineMsg, &MOTD);
#ifdef RX_ENABLE_LOCKS
	AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	XSTATS_END_TIME;
      }
      else code = -1;
    } while
      (afs_Analyze(tc, code, &avc->fid, areq,
		   AFS_STATS_FS_RPCIDX_GETVOLUMESTATUS,
		   SHARED_LOCK, (struct cell *)0));

    if (code) return code;
    /* Copy all this junk into msg->im_data, keeping track of the lengths. */
    cp = aout;
    bcopy((char *)&volstat, cp, sizeof(VolumeStatus));
    cp += sizeof(VolumeStatus);
    strcpy(cp, volName);
    cp += strlen(volName)+1;
    strcpy(cp, offLineMsg);
    cp += strlen(offLineMsg)+1;
    strcpy(cp, motd);
    cp += strlen(motd)+1;
    *aoutSize = (cp - aout);
    return 0;
}

static PSetVolumeStatus(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    char volName[32];
    char offLineMsg[256];
    char motd[256];
    register struct conn *tc;
    register afs_int32 code;
    struct AFSFetchVolumeStatus volstat;
    struct AFSStoreVolumeStatus storeStat;
    register struct volume *tvp;
    register char *cp;
    XSTATS_DECLS;

    AFS_STATCNT(PSetVolumeStatus);
    if (!avc) return EINVAL;

    tvp	= afs_GetVolume(&avc->fid, areq, READ_LOCK);	   
    if (tvp) {
	if (tvp->states & (VRO | VBackup)) {
	    afs_PutVolume(tvp, READ_LOCK);
	    return EROFS;
	}
	afs_PutVolume(tvp, READ_LOCK);
    } else
	return ENODEV;
    /* Copy the junk out, using cp as a roving pointer. */
    cp = ain;
    bcopy(cp, (char *)&volstat, sizeof(AFSFetchVolumeStatus));
    cp += sizeof(AFSFetchVolumeStatus);
    strcpy(volName, cp);
    cp += strlen(volName)+1;
    strcpy(offLineMsg, cp);
    cp +=  strlen(offLineMsg)+1;
    strcpy(motd, cp);
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
	tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	if (tc) {
          XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_SETVOLUMESTATUS);
#ifdef RX_ENABLE_LOCKS
	    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    code = RXAFS_SetVolumeStatus(tc->id, avc->fid.Fid.Volume,
					&storeStat, volName, offLineMsg, motd);
#ifdef RX_ENABLE_LOCKS
	    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
          XSTATS_END_TIME;
	}
	else code = -1;
    } while
      (afs_Analyze(tc, code, &avc->fid, areq,
		   AFS_STATS_FS_RPCIDX_SETVOLUMESTATUS,
		   SHARED_LOCK, (struct cell *)0));

    if (code) return code;
    /* we are sending parms back to make compat. with prev system.  should
      change interface later to not ask for current status, just set new status */
    cp = aout;
    bcopy((char *)&volstat, cp, sizeof(VolumeStatus));
    cp += sizeof(VolumeStatus);
    strcpy(cp, volName);
    cp += strlen(volName)+1;
    strcpy(cp, offLineMsg);
    cp += strlen(offLineMsg)+1;
    strcpy(cp, motd);
    cp += strlen(motd)+1;
    *aoutSize = cp - aout;
    return 0;
}

static PFlush(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    register struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ 
    struct AFS_UCRED *acred;
{

    AFS_STATCNT(PFlush);
    if (!avc) return EINVAL;
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
    afs_BozonLock(&avc->pvnLock, avc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
    ObtainWriteLock(&avc->lock,225);
    ObtainWriteLock(&afs_xcbhash, 456);
    afs_DequeueCallback(avc);
    avc->states	&= ~(CStatd | CDirty);	/* next reference will re-stat cache entry */
    ReleaseWriteLock(&afs_xcbhash);
    /* now find the disk cache entries */
    afs_TryToSmush(avc, acred, 1);
    osi_dnlc_purgedp(avc);
    afs_symhint_inval(avc);
    if (avc->linkData && !(avc->states & CCore)) {
	afs_osi_Free(avc->linkData, strlen(avc->linkData)+1);
	avc->linkData = (char *) 0;
    }
    ReleaseWriteLock(&avc->lock);
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
    afs_BozonUnlock(&avc->pvnLock, avc);
#endif
    return 0;
}

static PNewStatMount(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 code;
    register struct vcache *tvc;
    register struct dcache *tdc;
    struct VenusFid tfid;
    char *bufp = 0;
    afs_int32 offset, len, hasatsys=0;

    AFS_STATCNT(PNewStatMount);
    if (!avc) return EINVAL;
    code = afs_VerifyVCache(avc, areq);
    if (code) return code;
    if (vType(avc) != VDIR) {
	return ENOTDIR;
    }
    tdc = afs_GetDCache(avc, 0, areq, &offset, &len, 1);
    if (!tdc) return ENOENT;
    hasatsys = Check_AtSys(avc, ain, &bufp, areq);
    code = afs_dir_Lookup(&tdc->f.inode, bufp, &tfid.Fid);
    if (code) {
	afs_PutDCache(tdc);
	goto out;
    }
    tfid.Cell = avc->fid.Cell;
    tfid.Fid.Volume = avc->fid.Fid.Volume;
    afs_PutDCache(tdc);	    /* we're done with the data */
    if (!tfid.Fid.Unique && (avc->states & CForeign)) {
	tvc = afs_LookupVCache(&tfid, areq, (afs_int32 *)0, WRITE_LOCK, avc, bufp);
    } else {
	tvc = afs_GetVCache(&tfid, areq, (afs_int32 *)0, (struct vcache*)0,
			    WRITE_LOCK);
    }
    if (!tvc) {
	code = ENOENT;
	goto out;
    }
    if (vType(tvc) != VLNK) {
	afs_PutVCache(tvc, WRITE_LOCK);
	code = EINVAL;
	goto out;
    }
    ObtainWriteLock(&tvc->lock,226);
    code = afs_HandleLink(tvc, areq);
    if (code == 0) {
	if (tvc->linkData) {
	    if ((tvc->linkData[0] != '#') && (tvc->linkData[0] != '%'))
		code =  EINVAL;
	    else {
		/* we have the data */
		strcpy(aout, tvc->linkData);
		*aoutSize = strlen(tvc->linkData)+1;
	    }
	} else 
	    code = EIO;
    }
    ReleaseWriteLock(&tvc->lock);
    afs_PutVCache(tvc, WRITE_LOCK);
out:
    if (hasatsys) osi_FreeLargeSpace(bufp);
    return code;
}

static PGetTokens(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register struct cell *tcell;
    register afs_int32 i;
    register struct unixuser *tu;
    register char *cp;
    afs_int32 iterator;
    int newStyle;

    AFS_STATCNT(PGetTokens);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    /* weird interface.  If input parameter is present, it is an integer and
	we're supposed to return the parm'th tokens for this unix uid.
	If not present, we just return tokens for cell 1.
	If counter out of bounds, return EDOM.
	If no tokens for the particular cell, return ENOTCONN.
	Also, if this mysterious parm is present, we return, along with the
	tokens, the primary cell indicator (an afs_int32 0) and the cell name
	at the end, in that order.
    */
    if (newStyle = (ainSize > 0)) {
	bcopy(ain, (char *)&iterator, sizeof(afs_int32));
    }
    i = UHash(areq->uid);
    ObtainReadLock(&afs_xuser);
    for(tu = afs_users[i]; tu; tu=tu->next) {
	if (newStyle) {
	    if (tu->uid == areq->uid && (tu->states & UHasTokens)) {
		if (iterator-- == 0) break;	/* are we done yet? */
	    }
	}
	else {
	    if (tu->uid == areq->uid && tu->cell == 1) break;
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
    if (((tu->states & UHasTokens) == 0) || (tu->ct.EndTimestamp < osi_Time())) {
	tu->states |= (UTokensBad | UNeedsReset);
	afs_PutUser(tu, READ_LOCK);
	return ENOTCONN;
    }
    /* use iterator for temp */
    cp = aout;
    iterator = tu->stLen;	/* for compat, we try to return 56 byte tix if they fit */
    if (iterator < 56) iterator	= 56;	/* # of bytes we're returning */
    bcopy((char *)&iterator, cp, sizeof(afs_int32));
    cp += sizeof(afs_int32);
    bcopy(tu->stp, cp, tu->stLen);	/* copy out st */
    cp += iterator;
    iterator = sizeof(struct ClearToken);
    bcopy((char *)&iterator, cp, sizeof(afs_int32));
    cp += sizeof(afs_int32);
    bcopy((char *)&tu->ct, cp, sizeof(struct ClearToken));
    cp += sizeof(struct ClearToken);
    if (newStyle) {
	/* put out primary id and cell name, too */
	iterator = (tu->states & UPrimary ? 1 : 0);
	bcopy((char *)&iterator, cp, sizeof(afs_int32));
	cp += sizeof(afs_int32);
	tcell = afs_GetCell(tu->cell, READ_LOCK);
	if (tcell) {
	    strcpy(cp, tcell->cellName);
	    cp += strlen(tcell->cellName)+1;
	    afs_PutCell(tcell, READ_LOCK);
	}
	else *cp++ = 0;
    }
    *aoutSize = cp - aout;
    afs_PutUser(tu, READ_LOCK);
    return 0;
}

static PUnlog(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 i;
    register struct unixuser *tu;

    AFS_STATCNT(PUnlog);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    i = UHash(areq->uid);
    ObtainWriteLock(&afs_xuser,227);
    for(tu=afs_users[i]; tu; tu=tu->next) {
	if (tu->uid == areq->uid) {
	    tu->vid = UNDEFVID;
	    tu->states &= ~UHasTokens;
	    /* security is not having to say you're sorry */
	    bzero((char *)&tu->ct, sizeof(struct ClearToken));
	    tu->refCount++;
	    ReleaseWriteLock(&afs_xuser);
	    /* We have to drop the lock over the call to afs_ResetUserConns, since
	     * it obtains the afs_xvcache lock.  We could also keep the lock, and
	     * modify ResetUserConns to take parm saying we obtained the lock
	     * already, but that is overkill.  By keeping the "tu" pointer
	     * held over the released lock, we guarantee that we won't lose our
	     * place, and that we'll pass over every user conn that existed when
	     * we began this call.
	     */
	    afs_ResetUserConns(tu);
	    tu->refCount--;
	    ObtainWriteLock(&afs_xuser,228);
	}
    }
    ReleaseWriteLock(&afs_xuser);
    return 0;
}

static PMariner(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    afs_int32 newHostAddr;
    afs_int32 oldHostAddr;
    
    AFS_STATCNT(PMariner);
    if (afs_mariner)
	bcopy((char *)&afs_marinerHost, (char *)&oldHostAddr, sizeof(afs_int32));
    else
	oldHostAddr = 0xffffffff;   /* disabled */
    
    bcopy(ain, (char *)&newHostAddr, sizeof(afs_int32));
    if (newHostAddr == 0xffffffff) {
	/* disable mariner operations */
	afs_mariner = 0;
    }
    else if (newHostAddr) {
	afs_mariner = 1;
	afs_marinerHost = newHostAddr;
    }
    bcopy((char *)&oldHostAddr, aout, sizeof(afs_int32));
    *aoutSize = sizeof(afs_int32);
    return 0;
}

static PCheckServers(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ 
    struct AFS_UCRED *acred;
{
    register char *cp = 0;
    register int i;
    register struct server *ts;
    afs_int32 temp, *lp = (afs_int32 *)ain, havecell=0;
    struct cell *cellp;
    struct chservinfo *pcheck;

    AFS_STATCNT(PCheckServers);

    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    if (*lp == 0x12345678) {	/* For afs3.3 version */
	pcheck=(struct chservinfo *)ain;
	if (pcheck->tinterval >= 0) {
	    cp = aout;	    
	    bcopy((char *)&PROBE_INTERVAL, cp, sizeof(afs_int32));
	    *aoutSize = sizeof(afs_int32);
	    if (pcheck->tinterval > 0) {
		if (!afs_osi_suser(acred))
		    return EACCES;
		PROBE_INTERVAL=pcheck->tinterval;
	    }
	    return 0;
	}
	if (pcheck->tsize)
	    havecell = 1;
	temp=pcheck->tflags;
	cp = pcheck->tbuffer;
    } else {	/* For pre afs3.3 versions */
	bcopy(ain, (char *)&temp, sizeof(afs_int32));
	cp = ain+sizeof(afs_int32);
	if (ainSize > sizeof(afs_int32)) 
	    havecell = 1;
    }

    /* 
     * 1: fast check, don't contact servers.
     * 2: local cell only.
     */
    if (havecell) {
	/* have cell name, too */
	cellp = afs_GetCellByName(cp, READ_LOCK);
	if (!cellp) return ENOENT;
    }
    else cellp = (struct cell *) 0;
    if (!cellp && (temp & 2)) {
	/* use local cell */
	cellp = afs_GetCell(1, READ_LOCK);
    }
    if (!(temp & 1)) {	/* if not fast, call server checker routine */
	afs_CheckServers(1, cellp);	/* check down servers */
	afs_CheckServers(0, cellp);	/* check up servers */
    }
    /* now return the current down server list */
    cp = aout;
    ObtainReadLock(&afs_xserver);
    for(i=0;i<NSERVERS;i++) {
	for(ts = afs_servers[i]; ts; ts=ts->next) {
	    if (cellp && ts->cell != cellp) continue;	/* cell spec'd and wrong */
	    if ((ts->flags & SRVR_ISDOWN) && ts->addr->sa_portal != ts->cell->vlport) {
		bcopy((char *)&ts->addr->sa_ip, cp, sizeof(afs_int32));
		cp += sizeof(afs_int32);
	    }
	}
    }
    ReleaseReadLock(&afs_xserver);
    if (cellp) afs_PutCell(cellp, READ_LOCK);
    *aoutSize = cp - aout;
    return 0;
}

static PCheckVolNames(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    AFS_STATCNT(PCheckVolNames);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    afs_CheckRootVolume();
    afs_CheckVolumeNames(AFS_VOLCHECK_FORCE |
			 AFS_VOLCHECK_EXPIRED |
			 AFS_VOLCHECK_BUSY |
			 AFS_VOLCHECK_MTPTS);
    return 0;
}

static PCheckAuth(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
       int i;
       struct srvAddr *sa;
       struct conn *tc;
       struct unixuser *tu;
       afs_int32 retValue;
       extern afs_rwlock_t afs_xsrvAddr;	

    AFS_STATCNT(PCheckAuth);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    retValue = 0;
    tu = afs_GetUser(areq->uid,	1, READ_LOCK);	/* check local cell authentication */
    if (!tu) retValue = EACCES;
    else {
	/* we have a user */
	ObtainReadLock(&afs_xsrvAddr);	
	ObtainReadLock(&afs_xconn);

	/* any tokens set? */
	if ((tu->states	& UHasTokens) == 0) retValue = EACCES;
	/* all connections in cell 1 working? */
	for(i=0;i<NSERVERS;i++) {
	    for(sa = afs_srvAddrs[i]; sa; sa=sa->next_bkt) {
	       for (tc = sa->conns; tc; tc=tc->next) {
		  if (tc->user == tu && (tu->states & UTokensBad))
		       retValue = EACCES;
	       }
	    }
	}
	ReleaseReadLock(&afs_xsrvAddr);
	ReleaseReadLock(&afs_xconn);
	afs_PutUser(tu, READ_LOCK);
    }
    bcopy((char *)&retValue, aout, sizeof(afs_int32));
    *aoutSize = sizeof(afs_int32);
    return 0;
}

static Prefetch(apath, adata, afollow, acred)
char *apath;
struct afs_ioctl *adata;
int afollow;
struct AFS_UCRED *acred; 
{
    register char *tp;
    register afs_int32 code;
    u_int bufferSize;

    AFS_STATCNT(Prefetch);
    if (!apath) return EINVAL;
    tp = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
    AFS_COPYINSTR(apath, tp, 1024, &bufferSize, code);
    if (code) {
	osi_FreeSmallSpace(tp);
	return code;
    }
    if (afs_BBusy()) {	/* do this as late as possible */
	osi_FreeSmallSpace(tp);
	return EWOULDBLOCK;	/* pretty close */
    }
    afs_BQueue(BOP_PATH, (struct vcache*)0, 0, 0, acred, (long)tp, 0L, 0L, 0L);
    return 0;
}

static PFindVolume(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register struct volume *tvp;
    register struct server *ts;
    register afs_int32 i;
    register char *cp;
    
    AFS_STATCNT(PFindVolume);
    if (!avc) return EINVAL;
    tvp = afs_GetVolume(&avc->fid, areq, READ_LOCK);
    if (tvp) {
	cp = aout;
	for(i=0;i<MAXHOSTS;i++) {
	    ts = tvp->serverHost[i];
	    if (!ts) break;
	    bcopy((char *)&ts->addr->sa_ip, cp, sizeof(afs_int32));
	    cp += sizeof(afs_int32);
	}
	if (i<MAXHOSTS) {
	    /* still room for terminating NULL, add it on */
	    ainSize = 0;	/* reuse vbl */
	    bcopy((char *)&ainSize, cp, sizeof(afs_int32));
	    cp += sizeof(afs_int32);
	}
	*aoutSize = cp - aout;
	afs_PutVolume(tvp, READ_LOCK);
	return 0;
    }
    return ENODEV;
}

static PViceAccess(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 code;
    afs_int32 temp;
    
    AFS_STATCNT(PViceAccess);
    if (!avc) return EINVAL;
    code = afs_VerifyVCache(avc, areq);
    if (code) return code;
    bcopy(ain, (char *)&temp, sizeof(afs_int32));
    code = afs_AccessOK(avc,temp, areq, CHECK_MODE_BITS);
    if (code) return 0;
    else return EACCES;
}

static PSetCacheSize(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ 
    struct AFS_UCRED *acred;
{
    afs_int32 newValue;
    int waitcnt = 0;
    
    AFS_STATCNT(PSetCacheSize);
    if (!afs_osi_suser(acred))
	return EACCES;
    /* too many things are setup initially in mem cache version */
    if (cacheDiskType == AFS_FCACHE_TYPE_MEM) return EROFS;
    bcopy(ain, (char *)&newValue, sizeof(afs_int32));
    if (newValue == 0) afs_cacheBlocks = afs_stats_cmperf.cacheBlocksOrig;
    else {
	extern u_int afs_min_cache;
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
static PGetCacheSize(avc, afun, areq, ain, aout, ainSize, aoutSize)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;	/* set this */ {
    afs_int32 results[MAXGCSTATS];

    AFS_STATCNT(PGetCacheSize);
    bzero((char *)results, sizeof(results));
    results[0] = afs_cacheBlocks;
    results[1] = afs_blocksUsed;
    bcopy((char *)results, aout, sizeof(results));
    *aoutSize = sizeof(results);
    return 0;
}

static PRemoveCallBack(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register struct conn *tc;
    register afs_int32 code;
    struct AFSCallBack CallBacks_Array[1];
    struct AFSCBFids theFids;
    struct AFSCBs theCBs;
    XSTATS_DECLS;

    AFS_STATCNT(PRemoveCallBack);
    if (!avc) return EINVAL;
    if (avc->states & CRO) return 0;	/* read-only-ness can't change */
    ObtainWriteLock(&avc->lock,229);
    theFids.AFSCBFids_len = 1;
    theCBs.AFSCBs_len = 1;
    theFids.AFSCBFids_val = (struct AFSFid *) &avc->fid.Fid;
    theCBs.AFSCBs_val = CallBacks_Array;
    CallBacks_Array[0].CallBackType = CB_DROPPED;
    if (avc->callback) {
	do {
	    tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	    if (tc) {
	      XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_GIVEUPCALLBACKS);
#ifdef RX_ENABLE_LOCKS
	      AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	      code = RXAFS_GiveUpCallBacks(tc->id, &theFids, &theCBs);
#ifdef RX_ENABLE_LOCKS
	      AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	      XSTATS_END_TIME;
	    }
	    /* don't set code on failure since we wouldn't use it */
	} while
	  (afs_Analyze(tc, code, &avc->fid, areq,
		       AFS_STATS_FS_RPCIDX_GIVEUPCALLBACKS,
		       SHARED_LOCK, (struct cell *)0));

	ObtainWriteLock(&afs_xcbhash, 457);
	afs_DequeueCallback(avc);
	avc->callback = 0;
	avc->states &= ~(CStatd | CUnique);
	ReleaseWriteLock(&afs_xcbhash);
	if (avc->fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	    osi_dnlc_purgedp(avc);
    }
    ReleaseWriteLock(&avc->lock);
    return 0;
}

static PNewCell(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    register char *ain;
    char *aout;
    afs_int32 ainSize;
    struct AFS_UCRED *acred;
    afs_int32 *aoutSize;	/* set this */ {
    /* create a new cell */
    afs_int32 cellHosts[MAXCELLHOSTS], *lp, magic=0;
    register struct cell *tcell;
    char *newcell=0, *linkedcell=0, *tp= ain;
    register afs_int32 code, linkedstate=0, ls;
    u_short fsport = 0, vlport = 0;
    afs_int32 scount;
    
    AFS_STATCNT(PNewCell);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    if (!afs_osi_suser(acred))
	return EACCES;

    bcopy(tp, (char *)&magic, sizeof(afs_int32));
    tp += sizeof(afs_int32);
    if (magic != 0x12345678)
        return EINVAL;

    /* A 3.4 fs newcell command will pass an array of MAXCELLHOSTS
     * server addresses while the 3.5 fs newcell command passes
     * MAXHOSTS. To figure out which is which, check if the cellname
     * is good.
     */
    newcell = tp + (MAXCELLHOSTS+3)*sizeof(afs_int32);
    scount = ((newcell[0] != '\0') ? MAXCELLHOSTS : MAXHOSTS);

    /* MAXCELLHOSTS (=8) is less than MAXHOSTS (=13) */
    bcopy(tp, (char *)cellHosts, MAXCELLHOSTS * sizeof(afs_int32));
    tp += (scount * sizeof(afs_int32));

    lp = (afs_int32 *)tp;
    fsport = *lp++;
    vlport = *lp++;
    if (fsport < 1024) fsport = 0;	/* Privileged ports not allowed */
    if (vlport < 1024) vlport = 0;	/* Privileged ports not allowed */
    tp += (3 * sizeof(afs_int32));
    newcell = tp;
    if ((ls = *lp) & 1) {
       linkedcell = tp + strlen(newcell)+1;
       linkedstate |= CLinkedCell;
    }

    linkedstate |= CNoSUID; /* setuid is disabled by default for fs newcell */
    code = afs_NewCell(newcell, cellHosts, linkedstate, linkedcell, fsport, vlport);
    return code;
}

static PListCells(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    afs_int32 whichCell;
    register struct cell *tcell=0;
    register afs_int32 i;
    register char *cp, *tp = ain;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(PListCells);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    bcopy(tp, (char *)&whichCell, sizeof(afs_int32));
    tp += sizeof(afs_int32);
    ObtainReadLock(&afs_xcell);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tcell = QTOC(cq); tq = QNext(cq);
	if (whichCell == 0) break;
	if (tq == &CellLRU) tcell = 0;
	whichCell--;
    }
    if (tcell) {
	cp = aout;
	bzero(cp, MAXCELLHOSTS * sizeof(afs_int32));
	for(i=0;i<MAXCELLHOSTS;i++) {
	    if (tcell->cellHosts[i] == 0) break;
	    bcopy((char *)&tcell->cellHosts[i]->addr->sa_ip, cp, sizeof(afs_int32));
	    cp += sizeof(afs_int32);
	}
	cp = aout + MAXCELLHOSTS * sizeof(afs_int32);
	strcpy(cp, tcell->cellName);
	cp += strlen(tcell->cellName)+1;
	*aoutSize = cp - aout;
    }
    ReleaseReadLock(&afs_xcell);
    if (tcell) return 0;
    else return EDOM;
}

static PRemoveMount(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    register char *ain;
    char *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 code;
    char *bufp = 0;
    afs_int32 offset, len, hasatsys = 0;
    register struct conn *tc;
    register struct dcache *tdc;
    register struct vcache *tvc;
    struct AFSFetchStatus OutDirStatus;
    struct VenusFid tfid;
    struct AFSVolSync tsync;
    XSTATS_DECLS;


    /* "ain" is the name of the file in this dir to remove */

    AFS_STATCNT(PRemoveMount);
    if (!avc) return EINVAL;
    code = afs_VerifyVCache(avc, areq);
    if (code) return code;
    if (vType(avc) != VDIR) return ENOTDIR;

    tdc	= afs_GetDCache(avc, 0,	areq, &offset,	&len, 1);	/* test for error below */
    if (!tdc) return ENOENT;
    hasatsys = Check_AtSys(avc, ain, &bufp, areq);
    code = afs_dir_Lookup(&tdc->f.inode, bufp, &tfid.Fid);
    if (code) {
	afs_PutDCache(tdc);
	goto out;
    }
    tfid.Cell = avc->fid.Cell;
    tfid.Fid.Volume = avc->fid.Fid.Volume;
    if (!tfid.Fid.Unique &&  (avc->states & CForeign)) {
	tvc = afs_LookupVCache(&tfid, areq, (afs_int32 *)0, WRITE_LOCK, avc, bufp);
    } else {
	tvc = afs_GetVCache(&tfid, areq, (afs_int32 *)0,
			    (struct vcache*)0/*xxx avc?*/, WRITE_LOCK);
    }
    if (!tvc) {
	code = ENOENT;
	afs_PutDCache(tdc);
	goto out;
    }
    if (vType(tvc) != VLNK) {
	afs_PutDCache(tdc);
	afs_PutVCache(tvc, WRITE_LOCK);
	code = EINVAL;
	goto out;
    }
    ObtainWriteLock(&tvc->lock,230);
    code = afs_HandleLink(tvc, areq);
    if (!code) {
	if (tvc->linkData) {
	    if ((tvc->linkData[0] != '#') && (tvc->linkData[0] != '%'))
		code =  EINVAL;
	} else 
	    code = EIO;
    }
    ReleaseWriteLock(&tvc->lock);
    osi_dnlc_purgedp(tvc);
    afs_PutVCache(tvc, WRITE_LOCK);
    if (code) {
	afs_PutDCache(tdc);
	goto out;
    }
    ObtainWriteLock(&avc->lock,231);
    osi_dnlc_remove(avc, bufp, tvc);
    do {
	tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	if (tc) {
          XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEFILE);
#ifdef RX_ENABLE_LOCKS
	  AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	  code = RXAFS_RemoveFile(tc->id, (struct AFSFid *) &avc->fid.Fid,
				  bufp, &OutDirStatus, &tsync);
#ifdef RX_ENABLE_LOCKS
	  AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
          XSTATS_END_TIME;
	}
	else code = -1;
    } while
	(afs_Analyze(tc, code, &avc->fid, areq,
		     AFS_STATS_FS_RPCIDX_REMOVEFILE,
		     SHARED_LOCK, (struct cell *)0));

    if (code) {
	if (tdc) afs_PutDCache(tdc);
	ReleaseWriteLock(&avc->lock);
	goto out;
    }
    if (tdc) {
	/* we have the thing in the cache */
	if (afs_LocalHero(avc, tdc, &OutDirStatus, 1)) {
	    /* we can do it locally */
	    code = afs_dir_Delete(&tdc->f.inode, bufp);
	    if (code) {
		ZapDCE(tdc);	/* surprise error -- invalid value */
		DZap(&tdc->f.inode);
	    }
	}
	afs_PutDCache(tdc);	/* drop ref count */
    }
    avc->states &= ~CUnique;		/* For the dfs xlator */
    ReleaseWriteLock(&avc->lock);
    code = 0;
out:
    if (hasatsys) osi_FreeLargeSpace(bufp);
    return code;    
}

static PVenusLogging(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    register char *ain;
    char *aout;
    afs_int32 ainSize;
    struct AFS_UCRED *acred;
    afs_int32 *aoutSize;	/* set this */ {
    return EINVAL;		/* OBSOLETE */
}

static PGetCellStatus(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register struct cell *tcell;
    afs_int32 temp;

    AFS_STATCNT(PGetCellStatus);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    tcell = afs_GetCellByName(ain, READ_LOCK);
    if (!tcell) return ENOENT;
    temp = tcell->states;
    afs_PutCell(tcell, READ_LOCK);
    bcopy((char *)&temp, aout, sizeof(afs_int32));
    *aoutSize = sizeof(afs_int32);
    return 0;
}

static PSetCellStatus(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    struct AFS_UCRED *acred;
    afs_int32 *aoutSize;	/* set this */ {
    register struct cell *tcell;
    afs_int32 temp;
    
    if (!afs_osi_suser(acred))
	return EACCES;
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    tcell = afs_GetCellByName(ain+2*sizeof(afs_int32), WRITE_LOCK);
    if (!tcell) return ENOENT;
    bcopy(ain, (char *)&temp, sizeof(afs_int32));
    if (temp & CNoSUID)
	tcell->states |= CNoSUID;
    else
	tcell->states &= ~CNoSUID;
    afs_PutCell(tcell, WRITE_LOCK);
    return 0;
}

static PFlushVolumeData(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;	/* set this */ 
struct AFS_UCRED *acred;
{
    extern struct volume *afs_volumes[NVOLS];
    register afs_int32 i;
    register struct dcache *tdc;
    register struct vcache *tvc;
    register struct volume *tv;
    afs_int32 cell, volume;

    AFS_STATCNT(PFlushVolumeData);
    if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

    volume = avc->fid.Fid.Volume;  /* who to zap */
    cell = avc->fid.Cell;

    /* 
     * Clear stat'd flag from all vnodes from this volume; this will invalidate all
     * the vcaches associated with the volume.
     */
    ObtainReadLock(&afs_xvcache);
    for(i = 0; i < VCSIZE; i++) {
	for(tvc = afs_vhashT[i]; tvc; tvc=tvc->hnext) {
	    if (tvc->fid.Fid.Volume == volume && tvc->fid.Cell == cell) {
#if	defined(AFS_SGI_ENV) || defined(AFS_ALPHA_ENV)  || defined(AFS_SUN5_ENV)  || defined(AFS_HPUX_ENV)
		VN_HOLD((struct vnode *)tvc);
#else
		tvc->vrefCount++;
#endif
		ReleaseReadLock(&afs_xvcache);
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
		afs_BozonLock(&tvc->pvnLock, tvc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
		ObtainWriteLock(&tvc->lock,232);

		ObtainWriteLock(&afs_xcbhash, 458);
		afs_DequeueCallback(tvc);
		tvc->states &= ~(CStatd | CDirty);
		ReleaseWriteLock(&afs_xcbhash);
		if (tvc->fid.Fid.Vnode & 1 || (vType(tvc) == VDIR))
		    osi_dnlc_purgedp(tvc);
		afs_TryToSmush(tvc, acred, 1);
		ReleaseWriteLock(&tvc->lock);
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
		afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
		ObtainReadLock(&afs_xvcache);
		/* our tvc ptr is still good until now */
		AFS_FAST_RELE(tvc);
	    }
	}
    }
    ReleaseReadLock(&afs_xvcache);


    MObtainWriteLock(&afs_xdcache,328);  /* needed if you're going to flush any stuff */
    for(i=0;i<afs_cacheFiles;i++) {
	if (!(afs_indexFlags[i] & IFEverUsed)) continue;	/* never had any data */
	tdc = afs_GetDSlot(i, (struct dcache *) 0);
	if (tdc->refCount <= 1) {    /* too high, in use by running sys call */
	    if (tdc->f.fid.Fid.Volume == volume && tdc->f.fid.Cell == cell) {
		if (! (afs_indexFlags[i] & IFDataMod)) {
		    /* if the file is modified, but has a ref cnt of only 1, then
		       someone probably has the file open and is writing into it.
		       Better to skip flushing such a file, it will be brought back
		       immediately on the next write anyway.

		       If we *must* flush, then this code has to be rearranged to call
		       afs_storeAllSegments() first */
			afs_FlushDCache(tdc);
		}
	    }
	}
	tdc->refCount--;	/* bumped by getdslot */
    }
    MReleaseWriteLock(&afs_xdcache);

    ObtainReadLock(&afs_xvolume);
    for (i=0;i<NVOLS;i++) {
	for (tv = afs_volumes[i]; tv; tv=tv->next) {
	    if (tv->volume == volume) {
		afs_ResetVolumeInfo(tv);
		break;
	    }
	}
    }
    ReleaseReadLock(&afs_xvolume);

    /* probably, a user is doing this, probably, because things are screwed up. 
     * maybe it's the dnlc's fault? */
    osi_dnlc_purge();  
    return 0;
}



static PGetVnodeXStatus(avc, afun, areq, ain, aout, ainSize, aoutSize)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;	/* set this */ {
    register afs_int32 code;
    struct vcxstat stat;
    afs_int32 mode, i;
    
/*  AFS_STATCNT(PGetVnodeXStatus); */
    if (!avc) return EINVAL;
    code = afs_VerifyVCache(avc, areq);
    if (code) return code;
    if (vType(avc) == VDIR)
	mode = PRSFS_LOOKUP;
    else
	mode = PRSFS_READ;
    if (!afs_AccessOK(avc, mode, areq, CHECK_MODE_BITS))
	return EACCES;    
    stat.fid = avc->fid;
    hset32(stat.DataVersion, hgetlo(avc->m.DataVersion));
    stat.lock = avc->lock;
    stat.parentVnode = avc->parentVnode;
    stat.parentUnique = avc->parentUnique;
    hset(stat.flushDV, avc->flushDV);
    hset(stat.mapDV, avc->mapDV);
    stat.truncPos = avc->truncPos;
    { /* just grab the first two - won't break anything... */
      struct axscache *ac;

      for (i=0, ac=avc->Access; ac && i < CPSIZE; i++, ac=ac->next) {
	stat.randomUid[i] = ac->uid;
	stat.randomAccess[i] = ac->axess;
      }
    }
    stat.callback = afs_data_pointer_to_int32(avc->callback);
    stat.cbExpires = avc->cbExpires;
    stat.anyAccess = avc->anyAccess;
    stat.opens = avc->opens;
    stat.execsOrWriters = avc->execsOrWriters;
    stat.flockCount = avc->flockCount;
    stat.mvstat = avc->mvstat;
    stat.states = avc->states;
    bcopy((char *)&stat, aout, sizeof(struct vcxstat));
    *aoutSize = sizeof(struct vcxstat);
    return 0;
}


/* We require root for local sysname changes, but not for remote */
/* (since we don't really believe remote uids anyway) */
 /* outname[] shouldn't really be needed- this is left as an excercise */
 /* for the reader.  */

static PSetSysName(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;	/* set this */
register struct AFS_UCRED *acred;
{
    char *cp, inname[MAXSYSNAME], outname[MAXSYSNAME];
    int setsysname, foundname=0;
    register struct afs_exporter *exporter;
    extern struct unixuser *afs_FindUser();
    extern char *afs_sysname;
    register struct unixuser *au;
    register afs_int32 pag, error;
    int t;


    AFS_STATCNT(PSetSysName);
    if (!afs_globalVFS) {
      /* Afsd is NOT running; disable it */
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SGI64_ENV) || defined(AFS_LINUX22_ENV)
	return (EINVAL);
#else
	return (setuerror(EINVAL), EINVAL);
#endif
    }
    bzero(inname, MAXSYSNAME);
    bcopy(ain, (char *)&setsysname, sizeof(afs_int32));
    ain += sizeof(afs_int32);
    if (setsysname) {
      t = strlen(ain);
      if (t > MAXSYSNAME)
	return EINVAL;
      bcopy(ain, inname, t+1);  /* include terminating null */
      ain += t + 1;
    }
    if (acred->cr_gid == RMTUSER_REQ) { /* Handles all exporters */
	pag = PagInCred(acred);
	if (pag == NOPAG) {
	    return EINVAL;  /* Better than panicing */
	}
	if (!(au = afs_FindUser(pag, -1, READ_LOCK))) {
	    return EINVAL;  /* Better than panicing */
	}
	if (!(exporter = au->exporter)) {
	    afs_PutUser(au, READ_LOCK);
	    return EINVAL;  /* Better than panicing */
	}
	error = EXP_SYSNAME(exporter, (setsysname? inname : (char *)0), outname);
	if (error) {
	    if (error == ENODEV) foundname = 0; /* sysname not set yet! */
	    else {
		afs_PutUser(au, READ_LOCK);
		return error;
	    }
	}
	else foundname = 1;
	afs_PutUser(au, READ_LOCK);
    } else {
	if (!afs_sysname) osi_Panic("PSetSysName: !afs_sysname\n");
	if (!setsysname) {
	    strcpy(outname, afs_sysname);
	    foundname = 1;
	} else {
	    if (!afs_osi_suser(acred))     /* Local guy; only root can change sysname */
		return EACCES;
	    strcpy(afs_sysname, inname);
	}
    }
    if (!setsysname) {
	cp = aout;
	bcopy((char *)&foundname, cp, sizeof(afs_int32));
	cp += sizeof(afs_int32);
	if (foundname) {
	    strcpy(cp, outname);
	    cp += strlen(outname)+1;
	}
	*aoutSize = cp - aout;
    }
    return 0;
}

/* sequential search through the list of touched cells is not a good
 * long-term solution here. For small n, though, it should be just
 * fine.  Should consider special-casing the local cell for large n.
 * Likewise for PSetSPrefs.
 */
static void ReSortCells(s,l, vlonly)  
  int s;     /* number of ids in array l[] -- NOT index of last id */
  afs_int32 l[];  /* array of cell ids which have volumes that need to be sorted */
  int vlonly; /* sort vl servers or file servers?*/
{
  extern struct volume *afs_volumes[NVOLS];   /* volume hash table */

  int i;
  struct volume *j;
  register int  k;

  if (vlonly) {
     struct cell *tcell;
     for(k=0;k<s;k++) {
        tcell = afs_GetCell(l[k], WRITE_LOCK);
	if (!tcell) continue;
	afs_SortServers(tcell->cellHosts, MAXCELLHOSTS);
	afs_PutCell(tcell, WRITE_LOCK);
     }
     return;
  }

  ObtainReadLock(&afs_xvolume);
  for (i= 0; i< NVOLS; i++) {
     for (j=afs_volumes[i];j;j=j->next) {
        for (k=0;k<s;k++)
	   if (j->cell == l[k]) {
	      ObtainWriteLock(&j->lock,233);
	      afs_SortServers(j->serverHost, MAXHOSTS);
	      ReleaseWriteLock(&j->lock);
	      break; 
	   }
     }
  }
  ReleaseReadLock(&afs_xvolume);
}

int debugsetsp = 0;

static int afs_setsprefs(sp, num, vlonly)
   struct spref *sp;
   unsigned int num;
   unsigned int vlonly;
{
   struct srvAddr *sa;
   int i,j,k,matches,touchedSize;
   struct server *srvr = NULL;
   afs_int32 touched[34];
   int isfs;
   
   touchedSize=0;
   for (k=0; k < num; sp++, k++) { 
      if (debugsetsp) {
	 printf ("sp host=%x, rank=%d\n",sp->host.s_addr, sp->rank);
      }
      matches=0;
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
      
      if (sa && matches) {  /* found one! */
	 if (debugsetsp) {
	    printf ("sa ip=%x, ip_rank=%d\n",sa->sa_ip, sa->sa_iprank);
	 }
	 sa->sa_iprank = sp->rank + afs_randomMod15();
	 afs_SortOneServer(sa->server);
	 
	 if (srvr->cell) {
	   /* if we don't know yet what cell it's in, this is moot */
	   for (j=touchedSize-1; j>=0 && touched[j] != srvr->cell->cell; j--)
		 /* is it in our list of touched cells ?  */ ;
	   if (j < 0) {                   /* no, it's not */
	     touched[touchedSize++] = srvr->cell->cell;
	     if (touchedSize >= 32) {                /* watch for ovrflow */
	       ReleaseReadLock(&afs_xserver);
	       ReSortCells(touchedSize, touched, vlonly);
	       touchedSize=0;
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
	 srvr = afs_GetServer(&temp, 1, NULL, (vlonly ? AFS_VLPORT : AFS_FSPORT), 
			      WRITE_LOCK, (afsUUID *)0,0);
	 srvr->addr->sa_iprank = sp->rank + afs_randomMod15();
	 afs_PutServer(srvr, WRITE_LOCK);
      }
   } /* for all cited preferences */
   
   ReSortCells(touchedSize, touched, vlonly);
   return 0;
}

 /* Note that this may only be performed by the local root user.
 */
static int 
PSetSPrefs(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
     struct vcache *avc;
     int afun;
     struct vrequest *areq;
     char *ain, *aout;
     afs_int32 ainSize;
     struct AFS_UCRED *acred;
     afs_int32 *aoutSize;
{
  struct setspref *ssp;
  AFS_STATCNT(PSetSPrefs);

  if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

  if (!afs_osi_suser(acred))
      return EACCES;

  if (ainSize < sizeof(struct setspref)) 
    return EINVAL;

  ssp = (struct setspref *)ain;
  if (ainSize < sizeof(struct spref)*ssp->num_servers) 
    return EINVAL;

  afs_setsprefs(&(ssp->servers[0]), ssp->num_servers, 
		(ssp->flags & DBservers));
  return 0;
}

static int 
PSetSPrefs33(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    struct AFS_UCRED *acred;
    afs_int32 *aoutSize;
{
  struct spref *sp;
  AFS_STATCNT(PSetSPrefs);
  if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */


  if (!afs_osi_suser(acred))
    return EACCES;

  sp = (struct spref *)ain;
  afs_setsprefs(sp, ainSize/(sizeof(struct spref)), 0 /*!vlonly*/);
  return 0;
}

/* some notes on the following code...
 * in the hash table of server structs, all servers with the same IP address
 * will be on the same overflow chain.  
 * This could be sped slightly in some circumstances by having it cache the 
 * immediately previous slot in the hash table and some supporting information
 * Only reports file servers now.
 */
static int 
     PGetSPrefs(avc, afun, areq, ain, aout, ainSize, aoutSize)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;
{
   struct sprefrequest *spin; /* input */
   struct sprefinfo *spout;   /* output */
   struct spref *srvout;      /* one output component */
   int i,j;                   /* counters for hash table traversal */
   struct server *srvr;       /* one of CM's server structs */
   struct srvAddr *sa;
   afs_uint32 prevh;
   int vlonly;                /* just return vlservers ? */
   int isfs;
   
   AFS_STATCNT(PGetSPrefs);
   if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	return EIO;          /* Inappropriate ioctl for device */

   
   if (ainSize < sizeof (struct sprefrequest_33)) {
      return ENOENT;
   }
   else {
      spin = ((struct sprefrequest *) ain);
   }
   
   if (ainSize > sizeof  (struct sprefrequest_33)) {
      vlonly = (spin->flags & DBservers);
   }
   else vlonly = 0;
   
   /* struct sprefinfo includes 1 server struct...  that size gets added
    * in during the loop that follows.
    */
   *aoutSize = sizeof(struct sprefinfo) - sizeof (struct spref);
   spout = (struct sprefinfo *) aout;
   spout->next_offset = spin->offset;
   spout->num_servers = 0;
   srvout = spout->servers;
   
   ObtainReadLock(&afs_xserver);
   for (i=0, j=0; j < NSERVERS; j++) {       /* sift through hash table */
      for (sa = afs_srvAddrs[j]; sa; sa = sa->next_bkt, i++) {
	 if (spin->offset > (unsigned short)i) {
	    continue;                /* catch up to where we left off */
	 }
	    spout->next_offset++;
	 
	    srvr = sa->server;
	    isfs = (srvr->cell && (sa->sa_portal == srvr->cell->fsport))  
		 || (sa->sa_portal == AFS_FSPORT); 
	 
	    if ((vlonly && isfs) || (!vlonly && !isfs)) {
	       /* only report ranks for vl servers */
	       continue;
	    }
	 
	 srvout->host.s_addr = sa->sa_ip;
	 srvout->rank = sa->sa_iprank;
	 *aoutSize += sizeof(struct spref);
	 spout->num_servers++;
	 srvout++;

	 if (*aoutSize > (PIGGYSIZE - sizeof(struct spref))) {
	    ReleaseReadLock(&afs_xserver);                /* no more room! */
	    return 0;
	 }
      }
   }
   ReleaseReadLock(&afs_xserver);
   
   spout->next_offset = 0;  /* start over from the beginning next time */
   return 0;
}

/* Enable/Disable the specified exporter. Must be root to disable an exporter */
int  afs_NFSRootOnly = 1;
/*static*/ PExportAfs(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;	/* set this */
struct AFS_UCRED *acred;
{
    afs_int32 export, newint=0, type, changestate, handleValue, convmode, pwsync, smounts;
    extern struct afs_exporter *exporter_find();
    register struct afs_exporter *exporter;

    AFS_STATCNT(PExportAfs);
    bcopy(ain, (char *)&handleValue, sizeof(afs_int32));
    type = handleValue >> 24;
    if (type == 0x71) {
	newint = 1;
	type = 1;	/* nfs */
    }
    exporter = exporter_find(type);
    if (newint) {
	export = handleValue & 3;
	changestate = handleValue & 0xff;
	smounts = (handleValue >> 2) & 3;
	pwsync = (handleValue >> 4) & 3;
	convmode = (handleValue >> 6) & 3;
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
	bcopy((char *)&handleValue, aout, sizeof(afs_int32));
	*aoutSize = sizeof(afs_int32);
    } else {
	if (!afs_osi_suser(acred))
	    return EACCES;    /* Only superuser can do this */
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
	    handleValue = exporter->exp_states;
	    bcopy((char *)&handleValue, aout, sizeof(afs_int32));
	    *aoutSize = sizeof(afs_int32);
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

static int
PGag(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
struct AFS_UCRED *acred;
afs_int32 *aoutSize;	/* set this */
{
struct gaginfo *gagflags;

  if (!afs_osi_suser(acred))
    return EACCES;

  gagflags = (struct gaginfo *) ain;
  afs_showflags = gagflags->showflags;

  return 0;
}


static int
PTwiddleRx(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
struct AFS_UCRED *acred;
afs_int32 *aoutSize;	
{
  struct rxparams *rxp;

  if (!afs_osi_suser(acred))
    return EACCES;

  rxp = (struct rxparams *) ain;

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
  if (rxp->rx_maxReceiveSize)
  {
    rx_maxReceiveSize = rxp->rx_maxReceiveSize;
    rx_maxReceiveSizeUser = rxp->rx_maxReceiveSize;
  }
  if (rxp->rx_MyMaxSendSize)
    rx_MyMaxSendSize = rxp->rx_MyMaxSendSize;
 
  return 0;
}

static int PGetInitParams(avc, afun, areq, ain, aout, ainSize, aoutSize)
     struct vcache *avc;
     int afun;
     struct vrequest *areq;
     register char *ain;
     char *aout;
     afs_int32 ainSize;
     afs_int32 *aoutSize;	/* set this */
{
    if (sizeof(struct cm_initparams) > PIGGYSIZE)
	return E2BIG;

    bcopy((char*)&cm_initParams, aout, sizeof(struct cm_initparams));
    *aoutSize = sizeof(struct cm_initparams);
    return 0;
}

#ifdef AFS_SGI65_ENV
/* They took crget() from us, so fake it. */
static cred_t *crget(void)
{
    cred_t *cr;
    cr = crdup(get_current_cred());
    bzero((char*)cr, sizeof(cred_t));
#if CELL || CELL_PREPARE
    cr->cr_id = -1;
#endif
    return cr;
}
#endif
/*
 * Create new credentials to correspond to a remote user with given
 * <hostaddr, uid, g0, g1>.  This allows a server running as root to
 * provide pioctl (and other) services to foreign clients (i.e. nfs
 * clients) by using this call to `become' the client.
 */
#define	PSETPAG		110
#define	PIOCTL_HEADER	6
static int HandleClientContext(struct afs_ioctl *ablob, int *com, struct AFS_UCRED **acred, struct AFS_UCRED *credp)
{
    char *ain, *inData;
    afs_uint32 hostaddr;
    afs_int32 uid, g0, g1, i, code, pag, exporter_type;
    extern struct afs_exporter *exporter_find();
    struct afs_exporter *exporter, *outexporter;
    struct AFS_UCRED *newcred;
    struct unixuser *au;

#if	defined(AFS_DEC_ENV) || (defined(AFS_NONFSTRANS) && !defined(AFS_AIX_IAUTH_ENV))
    return EINVAL;			/* NFS trans not supported for Ultrix */
#else
#if defined(AFS_SGIMP_ENV)
    osi_Assert(ISAFS_GLOCK());
#endif
    AFS_STATCNT(HandleClientContext);
    if (ablob->in_size < PIOCTL_HEADER*sizeof(afs_int32)) { 
	/* Must at least include the PIOCTL_HEADER header words required by the protocol */
	return EINVAL; /* Too small to be good  */
    }
    ain = inData = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    AFS_COPYIN(ablob->in, ain, PIOCTL_HEADER*sizeof(afs_int32), code);
    if (code) {
	osi_FreeLargeSpace(inData);
	return code;
    }

    /* Extract information for remote user */
    hostaddr = *((afs_uint32 *)ain);
    ain += sizeof(hostaddr);
    uid = *((afs_uint32 *)ain);
    ain += sizeof(uid);
    g0 = *((afs_uint32 *)ain);
    ain += sizeof(g0);
    g1 = *((afs_uint32 *)ain);
    ain += sizeof(g1);
    *com = *((afs_uint32 *)ain);
    ain += sizeof(afs_int32);
    exporter_type = *((afs_uint32 *)ain);	/* In case we support more than NFS */

    /*
     * Of course, one must be root for most of these functions, but
     * we'll allow (for knfs) you to set things if the pag is 0 and
     * you're setting tokens or unlogging.
     */
    i = (*com) & 0xff;
    if (!afs_osi_suser(credp)) {
#ifdef	AFS_SGI_ENV 
#ifndef AFS_SGI64_ENV
	/* Since SGI's suser() returns explicit failure after the call.. */
	u.u_error = 0;		
#endif 
#endif
	/* check for acceptable opcodes for normal folks, which are, so far,
	 * set tokens and unlog.
	 */
	if (i != 9 && i != 3 && i != 38 && i != 8)	{
	    osi_FreeLargeSpace(inData);
	    return EACCES;
	}
    }

    ablob->in_size -= PIOCTL_HEADER*sizeof(afs_int32);
    ablob->in += PIOCTL_HEADER*sizeof(afs_int32);
    osi_FreeLargeSpace(inData);
    if (uid == 0) {
        /*
         * We map uid 0 to nobody to match the mapping that the nfs
         * server does and to ensure that the suser() calls in the afs
         * code fails for remote client roots.
         */
	uid = afs_nobody;	/* NFS_NOBODY == -2 */
    }
    newcred = crget();
#ifdef	AFS_AIX41_ENV
    setuerror(0);	
#endif
    newcred->cr_gid = RMTUSER_REQ;
    newcred->cr_groups[0] = g0;
    newcred->cr_groups[1] = g1;
#ifdef AFS_AIX_ENV
    newcred->cr_ngrps = 2;
#else
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)
    newcred->cr_ngroups = 2;
#else
    for (i=2; i<NGROUPS; i++)
	newcred->cr_groups[i] = NOGROUP;
#endif
#endif
#if	!defined(AFS_OSF_ENV) && !defined(AFS_DEC_ENV)
    afs_nfsclient_init();	/* before looking for exporter, ensure one exists */
#endif
    if (!(exporter = exporter_find(exporter_type))) {
	/* Exporter wasn't initialized or an invalid exporter type */
	 crfree(newcred);
	 return EINVAL;
    }
    if (exporter->exp_states & EXP_PWSYNC) {
	if (uid != credp->cr_uid) {
	    crfree(newcred);
	    return ENOEXEC;	/* XXX Find a better errno XXX */
	}
    }
    newcred->cr_uid = uid; /* Only temporary  */
    code = EXP_REQHANDLER(exporter, &newcred, hostaddr, &pag, &outexporter);
    /* The client's pag is the only unique identifier for it */
    newcred->cr_uid = pag;
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
	AFS_COPYOUT((char *)&pagvalue, ablob->out, sizeof(afs_int32), code);
      }
      afs_PutUser(au, WRITE_LOCK);
      if (code) return code;
      return PSETPAG;		/*  Special return for setpag  */
    } else if (!code) {
	EXP_RELE(outexporter);
    }
    return code;
#endif	/*defined(AFS_DEC_ENV) || defined(AFS_NONFSTRANS)*/
}

/* get all interface addresses of this client */

static int
PGetCPrefs(avc, afun, areq, ain, aout, ainSize, aoutSize)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;
{
	struct sprefrequest *spin; /* input */
	struct sprefinfo *spout;   /* output */
	struct spref *srvout;      /* one output component */
	int maxNumber;
	int i,j;

	AFS_STATCNT(PGetCPrefs);
        if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	    return EIO;          /* Inappropriate ioctl for device */

	if ( ainSize < sizeof (struct sprefrequest ))
		return EINVAL;
	
	spin = (struct sprefrequest *) ain;
	spout = (struct sprefinfo *) aout;

	maxNumber = spin->num_servers; /* max addrs this time */
	srvout = spout->servers;

	ObtainReadLock(&afs_xinterface);

	/* copy out the client interface information from the
	** kernel data structure "interface" to the output buffer
	*/
	for ( i=spin->offset, j=0; (i < afs_cb_interface.numberOfInterfaces)
				   && ( j< maxNumber) ; i++, j++, srvout++)
		srvout->host.s_addr = afs_cb_interface.addr_in[i];
	
	spout->num_servers = j;
	*aoutSize = sizeof(struct sprefinfo) +(j-1)* sizeof (struct spref);

	if ( i >= afs_cb_interface.numberOfInterfaces )
		spout->next_offset = 0; /* start from beginning again */
	else
		spout->next_offset = spin->offset + j;
	
	ReleaseReadLock(&afs_xinterface);
	return 0;
}

static int
PSetCPrefs(avc, afun, areq, ain, aout, ainSize, aoutSize)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;
{
	struct setspref *sin;
	int i;

	AFS_STATCNT(PSetCPrefs);
        if ( !afs_resourceinit_flag ) 	/* afs deamons havn't started yet */
	    return EIO;          /* Inappropriate ioctl for device */

	sin = (struct setspref *)ain;

	if ( ainSize < sizeof(struct setspref) )
		return EINVAL;
	if ( sin->num_servers < 0 )
		return EINVAL;
	if ( sin->num_servers > AFS_MAX_INTERFACE_ADDR)
		return ENOMEM;

	ObtainWriteLock(&afs_xinterface, 412);
	afs_cb_interface.numberOfInterfaces = sin->num_servers;
	for ( i=0; (unsigned short)i < sin->num_servers; i++)
		afs_cb_interface.addr_in[i] = sin->servers[i].host.s_addr;

	ReleaseWriteLock(&afs_xinterface);
	return 0;
}

static PFlushMount(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;
    struct AFS_UCRED *acred; {
    register afs_int32 code;
    register struct vcache *tvc;
    register struct dcache *tdc;
    struct VenusFid tfid;
    char *bufp = 0;
    afs_int32 offset, len, hasatsys=0;

    AFS_STATCNT(PFlushMount);
    if (!avc) return EINVAL;
    code = afs_VerifyVCache(avc, areq);
    if (code) return code;
    if (vType(avc) != VDIR) {
	return ENOTDIR;
    }
    tdc = afs_GetDCache(avc, 0, areq, &offset, &len, 1);
    if (!tdc) return ENOENT;
    hasatsys = Check_AtSys(avc, ain, &bufp, areq);
    code = afs_dir_Lookup(&tdc->f.inode, bufp, &tfid.Fid);
    if (code) {
	afs_PutDCache(tdc);
	goto out;
    }
    tfid.Cell = avc->fid.Cell;
    tfid.Fid.Volume = avc->fid.Fid.Volume;
    afs_PutDCache(tdc);	    /* we're done with the data */
    if (!tfid.Fid.Unique && (avc->states & CForeign)) {
	tvc = afs_LookupVCache(&tfid, areq, (afs_int32 *)0, WRITE_LOCK, avc, bufp);
    } else {
	tvc = afs_GetVCache(&tfid, areq, (afs_int32 *)0, (struct vcache*)0,
			    WRITE_LOCK);
    }
    if (!tvc) {
	code = ENOENT;
	goto out;
    }
    if (vType(tvc) != VLNK) {
	afs_PutVCache(tvc, WRITE_LOCK);
	code = EINVAL;
	goto out;
    }
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
    afs_BozonLock(&tvc->pvnLock, tvc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
    ObtainWriteLock(&tvc->lock,645);
    ObtainWriteLock(&afs_xcbhash, 646);
    afs_DequeueCallback(tvc);
    tvc->states	&= ~(CStatd | CDirty);	/* next reference will re-stat cache entry */
    ReleaseWriteLock(&afs_xcbhash);
    /* now find the disk cache entries */
    afs_TryToSmush(tvc, acred, 1);
    osi_dnlc_purgedp(tvc);
    afs_symhint_inval(tvc);
    if (tvc->linkData && !(tvc->states & CCore)) {
	afs_osi_Free(tvc->linkData, strlen(tvc->linkData)+1);
	tvc->linkData = (char *) 0;
    }
    ReleaseWriteLock(&tvc->lock);
#if	defined(AFS_SUN_ENV) || defined(AFS_ALPHA_ENV) || defined(AFS_SUN5_ENV)
    afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
    afs_PutVCache(tvc, WRITE_LOCK);
out:
    if (hasatsys) osi_FreeLargeSpace(bufp);
    return code;
}

static PRxStatProc(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;
    struct AFS_UCRED *acred;
{
    int code = 0;
    afs_int32 flags;

    if (!afs_osi_suser(acred)) {
	code = EACCES;
	goto out;
    }
    if (ainSize != sizeof(afs_int32)) {
	code = EINVAL;
	goto out;
    }
    bcopy(ain, (char *)&flags, sizeof(afs_int32));
    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK)) {
	code = EINVAL;
	goto out;
    }
    if (flags & AFSCALL_RXSTATS_ENABLE) {
	rx_enableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
	rx_disableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
	rx_clearProcessRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
out:
    *aoutSize = 0;
    return code;
}


static PRxStatPeer(avc, afun, areq, ain, aout, ainSize, aoutSize, acred)
    struct vcache *avc;
    int afun;
    struct vrequest *areq;
    char *ain, *aout;
    afs_int32 ainSize;
    afs_int32 *aoutSize;
    struct AFS_UCRED *acred;
{
    int code = 0;
    afs_int32 flags;

    if (!afs_osi_suser(acred)) {
	code = EACCES;
	goto out;
    }
    if (ainSize != sizeof(afs_int32)) {
	code = EINVAL;
	goto out;
    }
    bcopy(ain, (char *)&flags, sizeof(afs_int32));
    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK)) {
	code = EINVAL;
	goto out;
    }
    if (flags & AFSCALL_RXSTATS_ENABLE) {
	rx_enablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
	rx_disablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
	rx_clearPeerRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
out:
    *aoutSize = 0;
    return code;
}

