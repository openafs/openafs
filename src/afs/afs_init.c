/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_init.c - initialize AFS client.
 *
 * Implements:
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/stds.h"
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */


/* Imported variables */
extern afs_int32 afs_waitForever;
extern short afs_waitForeverCount;
extern afs_int32 afs_FVIndex;
extern struct server *afs_setTimeHost;
extern struct server *afs_servers[NSERVERS];
extern struct unixuser *afs_users[NUSERS];
extern struct volume *afs_freeVolList;
extern struct volume *afs_volumes[NVOLS];
extern afs_int32 afs_volCounter;

extern afs_rwlock_t afs_xaxs;
extern afs_rwlock_t afs_xvolume;
extern afs_rwlock_t afs_xuser;
extern afs_rwlock_t afs_xserver;
#ifndef AFS_AIX41_ENV
extern afs_lock_t osi_fsplock;
#endif
extern afs_lock_t osi_flplock;
extern afs_int32 fvTable[NFENTRIES];

/* afs_cell.c */
extern afs_rwlock_t afs_xcell;
extern struct afs_q CellLRU;
extern afs_int32 afs_cellindex;

/* afs_conn.c */
extern afs_rwlock_t afs_xconn;
extern afs_rwlock_t afs_xinterface;

/* afs_mariner.c */
extern struct rx_service *afs_server;


/* afs_mariner.c */
extern afs_int32 afs_mariner;
extern afs_int32 afs_marinerHost;

/* afs_volume.c */
extern ino_t volumeInode;

/* afs_osi_pag.c */
extern afs_uint32 pag_epoch;

/* afs_dcache.c */
extern afs_rwlock_t afs_xdcache;
extern int cacheDiskType;
extern afs_int32 afs_fsfragsize;
extern ino_t cacheInode;
extern struct osi_file *afs_cacheInodep;
extern afs_int32 afs_freeDCList;		/*Free list for disk cache entries*/


/* afs_vcache.c */
extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xvcb;

/* VNOPS/afs_vnop_read.c */
extern afs_int32 maxIHint;
extern afs_int32 nihints;                   /* # of above actually in-use */
extern afs_int32 usedihint;

/* afs_server.c */
extern afs_int32 afs_setTime;

/* Imported functions. */
extern struct rx_securityClass *rxnull_NewServerSecurityObject();
extern int RXAFSCB_ExecuteRequest();
extern int RXSTATS_ExecuteRequest();


/* afs_osi.c */
extern afs_lock_t afs_ftf;

/* Exported variables */
struct osi_dev cacheDev;           /*Cache device*/
afs_int32 cacheInfoModTime;			/*Last time cache info modified*/
#if	defined(AFS_OSF_ENV) || defined(AFS_DEC_ENV)
struct mount *afs_cacheVfsp=0;
#elif defined(AFS_LINUX20_ENV)
struct super_block *afs_cacheSBp = 0;
#else
struct vfs *afs_cacheVfsp=0;
#endif
afs_rwlock_t afs_puttofileLock; /* not used */
char *afs_sysname = 0;			/* So that superuser may change the
					 * local value of @sys */
struct volume *Initialafs_freeVolList;
int afs_memvolumes = 0;

/* Local variables */


/*
 * Initialization order is important.  Must first call afs_CacheInit,
 * then cache file and volume file initialization routines.  Next, the
 * individual cache entry initialization routines are called.
 */


/*
 * afs_CacheInit
 *
 * Description:
 *
 * Parameters:
 *	astatSize : The number of stat cache (vnode) entries to
 *		    allocate.
 *	afiles	  : The number of disk files to allocate to the cache
 *	ablocks	  : The max number of 1 Kbyte blocks that all of
 *		    the files in the cache may occupy.
 *	aDentries : Number of dcache entries to allocate.
 *	aVolumes  : Number of volume cache entries to allocate.
 *	achunk	  : Power of 2 to make the chunks.
 *	aflags	  : Flags passed in.
 *      inodes    : max inodes to pin down in inode[]
 *      users     : what should size of per-user access cache be?
 *
 * Environment:
 *	This routine should only be called at initialization time, since
 *	it reclaims no resources and doesn't sufficiently synchronize
 *	with other processes.
 */

struct cm_initparams cm_initParams;
static int afs_cacheinit_flag = 0;
int
afs_CacheInit(astatSize, afiles, ablocks, aDentries, aVolumes, achunk, aflags,
	      ninodes, nusers)
    afs_int32 afiles;
    afs_int32 astatSize, ablocks; 
    afs_int32 achunk, aflags, ninodes, nusers;
    afs_int32 aDentries;
{ /*afs_CacheInit*/
    extern int afs_memvolumes;
    register afs_int32 i, preallocs;
    register struct volume *tv;
    long code;

    AFS_STATCNT(afs_CacheInit);
    /*
     * Jot down the epoch time, namely when this incarnation of the
     * Cache Manager started.
     */
    afs_stats_cmperf.epoch = pag_epoch = osi_Time();
#ifdef SYS_NAME_ID
    afs_stats_cmperf.sysName_ID = SYS_NAME_ID;
#else
    afs_stats_cmperf.sysName_ID = SYS_NAME_ID_UNDEFINED;
#endif /* SYS_NAME_ID */

    printf("Starting AFS cache scan...");
    if (afs_cacheinit_flag)
	return 0;
    afs_cacheinit_flag = 1;
    cacheInfoModTime = 0;
    maxIHint = ninodes;
    nihints = 0;
    usedihint = 0;

    LOCK_INIT(&afs_ftf, "afs_ftf");
    RWLOCK_INIT(&afs_xaxs, "afs_xaxs");
    osi_dnlc_init();


#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    /*
     * We want to also reserve space for the gnode struct which is associated
     * with each vnode (vcache) one; we want to use the pinned pool for them   
     * since they're referenced at interrupt level.
     */
    if (afs_stats_cmperf.SmallBlocksAlloced + astatSize < 3600)
      preallocs = astatSize;
    else {
      preallocs = 3600 - afs_stats_cmperf.SmallBlocksAlloced;
      if (preallocs <= 0) preallocs = 10;
    }
    osi_AllocMoreSSpace(preallocs);
#endif
    /* 
     * create volume list structure 
     */
    if ( aVolumes < 50 )     aVolumes = 50;  
    if (aVolumes > 3000) aVolumes = 3000;

    tv = (struct volume *) afs_osi_Alloc(aVolumes * sizeof(struct volume));
    for (i=0;i<aVolumes-1;i++)
	tv[i].next = &tv[i+1];
    tv[aVolumes-1].next = (struct volume *) 0;
    afs_freeVolList = Initialafs_freeVolList = tv;
    afs_memvolumes = aVolumes;

    afs_cacheFiles = afiles;
    afs_cacheStats = astatSize;
    afs_vcacheInit(astatSize);
    afs_dcacheInit(afiles, ablocks, aDentries, achunk, aflags);

#if defined(AFS_AIX_ENV)
    {
	static void afs_procsize_init(void);

	afs_procsize_init();
    }
#endif

    /* Save the initialization parameters for later pioctl queries. */
    cm_initParams.cmi_version = CMI_VERSION;
    cm_initParams.cmi_nChunkFiles = afiles;
    cm_initParams.cmi_nStatCaches = astatSize;
    cm_initParams.cmi_nDataCaches = aDentries;
    cm_initParams.cmi_nVolumeCaches = aVolumes;
    cm_initParams.cmi_firstChunkSize = AFS_FIRSTCSIZE;
    cm_initParams.cmi_otherChunkSize = AFS_OTHERCSIZE;
    cm_initParams.cmi_cacheSize = ablocks;
    cm_initParams.cmi_setTime = afs_setTime;
    cm_initParams.cmi_memCache = (aflags & AFSCALL_INIT_MEMCACHE) ? 1 : 0;

    return 0;

} /*afs_CacheInit*/


/*
  * afs_ComputeCacheParams
  *
  * Description:
  *	Set some cache parameters.
  *
  * Parameters:
  *	None.
  */

void
afs_ComputeCacheParms()

{ /*afs_ComputeCacheParms*/

    register afs_int32 i;
    afs_int32 afs_maxCacheDirty;

    /*
     * Don't allow more than 2/3 of the files in the cache to be dirty.
     */
    afs_maxCacheDirty = (2*afs_cacheFiles) / 3;

    /*
     * Also, don't allow more than 2/3 of the total space get filled
     * with dirty chunks.  Compute the total number of chunks required
     * to fill the cache, make sure we don't set out limit above 2/3 of
     * that. If the cache size is greater than 1G, avoid overflow at
     * the expense of precision on the chunk size.
     */
    if (afs_cacheBlocks & 0xffe00000) {
	i = afs_cacheBlocks / (AFS_FIRSTCSIZE >> 10);
    }
    else {
	i = (afs_cacheBlocks << 10) / AFS_FIRSTCSIZE;
    }
    i = (2*i) / 3;
    if (afs_maxCacheDirty > i)
	afs_maxCacheDirty = i;
    if (afs_maxCacheDirty < 1)
	afs_maxCacheDirty = 1;
    afs_stats_cmperf.cacheMaxDirtyChunks = afs_maxCacheDirty;
} /*afs_ComputeCacheParms*/


/*
 * afs_InitVolumeInfo
 *
 * Description:
 *	Set up the volume info storage file.
 *
 * Parameters:
 *	afile : the file to be declared to be the volume info storage
 *		file for AFS.  It must be already truncated to 0 length.
 *
 * Environment:
 *	This function is called only during initialization.
 *
 *	WARNING: Data will be written to this file over time by AFS.
 */

afs_InitVolumeInfo(afile)
    register char *afile;

{ /*afs_InitVolumeInfo*/

    afs_int32 code;
    struct osi_file *tfile;
    struct vnode *filevp;
    struct fcache fce;

    AFS_STATCNT(afs_InitVolumeInfo);
#ifdef AFS_LINUX22_ENV
    {
	struct dentry *dp;
	code = gop_lookupname(afile, AFS_UIOSYS, 0, (struct vnode **) 0, &dp);
	if (code) return ENOENT;
	fce.inode = volumeInode = dp->d_inode->i_ino;
	dput(dp);
    }
#else
    code = gop_lookupname(afile, AFS_UIOSYS, 0, (struct vnode **) 0, &filevp);
    if (code) return ENOENT;
    fce.inode = volumeInode = afs_vnodeToInumber(filevp);
#ifdef AFS_DEC_ENV
    grele(filevp);
#else
    AFS_RELE((struct vnode *)filevp);
#endif
#endif /* AFS_LINUX22_ENV */
    tfile = afs_CFileOpen(fce.inode);
    afs_CFileTruncate(tfile, 0);
    afs_CFileClose(tfile);
    return 0;

} /*afs_InitVolumeInfo*/

/*
 * afs_InitCacheInfo
 *
 * Description:
 *	Set up the given file as the AFS cache info file.
 *
 * Parameters:
 *	afile : Name of the file assumed to be the cache info file
 *		for the Cache Manager; it will be used as such.
 * Side Effects:  This sets afs_fragsize, which is used in the cache usage 
 *                calculations such as in afs_adjustsize()
 *
 * Environment:
 *	This function is called only during initialization.  The given
 *	file should NOT be truncated to 0 lenght; its contents descrebe
 *	what data is really in the cache.
 *
 *	WARNING: data will be written to this file over time by AFS.
 *
 * NOTE: Starting to use separate osi_InitCacheInfo() routines to clean up
 * code.
 *
 */
afs_InitCacheInfo(afile)
    register char *afile;

{ /*afs_InitCacheInfo*/

    register afs_int32 code;
    struct osi_stat tstat;
    register struct osi_file *tfile;
    struct afs_fheader theader;
    struct vnode *filevp;
    int goodFile;

    AFS_STATCNT(afs_InitCacheInfo);
    if(cacheDiskType != AFS_FCACHE_TYPE_UFS)
	osi_Panic("afs_InitCacheInfo --- called for non-ufs cache!");
#ifdef AFS_LINUX22_ENV
    code = osi_InitCacheInfo(afile);
    if (code) return code;
#else
    code = gop_lookupname(afile, AFS_UIOSYS, 0, (struct vnode **) 0, &filevp);
    if (code || !filevp) return ENOENT;
    {
#if	defined(AFS_SUN56_ENV)
      struct statvfs64 st;
#else
#if	defined(AFS_HPUX102_ENV)
      struct k_statvfs st;
#else
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) ||defined(AFS_HPUX100_ENV)
      struct statvfs st;
#else 
#if defined(AFS_DUX40_ENV)
      struct nstatfs st;
#else
      struct statfs st;
#endif /* DUX40 */
#endif /* SUN5 SGI */
#endif /* HP 10.20 */
#endif /* SUN56 */

#if	defined(AFS_SGI_ENV)
#ifdef AFS_SGI65_ENV
      VFS_STATVFS(filevp->v_vfsp, &st, (struct vnode *)0, code);
      if (!code) 
#else
      if (!VFS_STATFS(filevp->v_vfsp, &st, (struct vnode *)0))
#endif /* AFS_SGI65_ENV */
#else /* AFS_SGI_ENV */
#if	defined(AFS_SUN5_ENV) || defined(AFS_HPUX100_ENV)
      if (!VFS_STATVFS(filevp->v_vfsp, &st)) 
#else
#ifdef	AFS_OSF_ENV
      
      VFS_STATFS(filevp->v_vfsp, code);
      /* struct copy */
      st = filevp->v_vfsp->m_stat;
      if (code == 0)
#else	/* AFS_OSF_ENV */
#ifdef AFS_AIX41_ENV
      if (!VFS_STATFS(filevp->v_vfsp, &st, &afs_osi_cred))
#else
#ifdef AFS_LINUX20_ENV
	  {
	      KERNEL_SPACE_DECL;
	      TO_USER_SPACE();

	      VFS_STATFS(filevp->v_vfsp, &st);
	      TO_KERNEL_SPACE();
	  }
#else
	if (!VFS_STATFS(filevp->v_vfsp, &st))  
#endif /* AFS_LINUX20_ENV */
#endif /* AIX41 */
#endif /* OSF */
#endif /* SUN5 HP10 */
#endif /* SGI */
#if	defined(AFS_SUN5_ENV) || defined(AFS_HPUX100_ENV)
	afs_fsfragsize = st.f_frsize - 1; 
#else
	afs_fsfragsize = st.f_bsize - 1; 
#endif
    }
#ifdef AFS_LINUX20_ENV
    cacheInode = filevp->i_ino;
    afs_cacheSBp = filevp->i_sb;
#else
    cacheInode = afs_vnodeToInumber(filevp);
    cacheDev.dev = afs_vnodeToDev(filevp);
#if defined(AFS_SGI62_ENV) || defined(AFS_HAVE_VXFS)
    afs_InitDualFSCacheOps(filevp);
#endif
    afs_cacheVfsp = filevp->v_vfsp;
#endif /* AFS_LINUX20_ENV */
    AFS_RELE((struct vnode *)filevp);
#endif /* AFS_LINUX22_ENV */
    tfile = osi_UFSOpen(cacheInode);
    afs_osi_Stat(tfile, &tstat);
    cacheInfoModTime = tstat.mtime;
    code = afs_osi_Read(tfile, -1, &theader, sizeof(theader));
    goodFile = 0;
    if (code == sizeof(theader)) {
	/* read the header correctly */
	if (theader.magic == AFS_FHMAGIC &&
	    theader.firstCSize == AFS_FIRSTCSIZE &&
	    theader.otherCSize == AFS_OTHERCSIZE &&
	    theader.version == AFS_CI_VERSION
	)
	    goodFile = 1;
    }
    if (!goodFile) {
	/* write out a good file label */
	theader.magic = AFS_FHMAGIC;
	theader.firstCSize = AFS_FIRSTCSIZE;
	theader.otherCSize = AFS_OTHERCSIZE;
	theader.version = AFS_CI_VERSION;
	afs_osi_Write(tfile, 0, &theader, sizeof(theader));
	/*
	 * Truncate the rest of the file, since it may be arbitrarily
	 * wrong
	 */
	osi_UFSTruncate(tfile, sizeof(struct afs_fheader));
    }
    /* Leave the file open now, since reopening the file makes public pool
     * vnode systems (like OSF/Alpha) much harder to handle, That's because
     * they can do a vnode recycle operation any time we open a file, which
     * we'd do on any afs_GetDSlot call, etc.
     */
    afs_cacheInodep = (struct osi_file *)tfile;
    return 0;

} /*afs_InitCacheInfo*/

int afs_resourceinit_flag = 0;
afs_ResourceInit(preallocs)
  int preallocs;
{
    register afs_int32 i;
    static struct rx_securityClass *secobj;

    AFS_STATCNT(afs_ResourceInit);
    RWLOCK_INIT(&afs_xuser, "afs_xuser");
    RWLOCK_INIT(&afs_xvolume, "afs_xvolume");
    RWLOCK_INIT(&afs_xcell, "afs_xcell");
    RWLOCK_INIT(&afs_xserver, "afs_xserver");
    RWLOCK_INIT(&afs_xinterface, "afs_xinterface");
    LOCK_INIT(&afs_puttofileLock, "afs_puttofileLock");
#ifndef	AFS_AIX32_ENV
    LOCK_INIT(&osi_fsplock, "osi_fsplock");
#endif
    LOCK_INIT(&osi_flplock, "osi_flplock");
    RWLOCK_INIT(&afs_xconn, "afs_xconn");

    afs_InitCBQueue(1);  /* initialize callback queues */

    if (afs_resourceinit_flag == 0) {
	afs_resourceinit_flag = 1;
	for (i=0;i<NFENTRIES;i++)
	    fvTable[i] = 0;
	afs_sysname = afs_osi_Alloc(MAXSYSNAME);
	strcpy(afs_sysname, SYS_NAME);
	QInit(&CellLRU);	
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    {  extern afs_int32 afs_preallocs;

       if ((preallocs > 256) && (preallocs < 3600))
	   afs_preallocs = preallocs;
       osi_AllocMoreSSpace(afs_preallocs);
       osi_AllocMoreMSpace(100); 
    }
#endif
    }
    
    secobj = rxnull_NewServerSecurityObject();
    afs_server =
	rx_NewService(0, 1, "afs", &secobj, 1, RXAFSCB_ExecuteRequest);
    afs_server =
	rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats", &secobj, 1,
		      RXSTATS_ExecuteRequest);
    rx_StartServer(0);
    afs_osi_Wakeup(&afs_server);	/* wakeup anyone waiting for it */
    return 0;

} /*afs_ResourceInit*/

#if defined(AFS_AIX_ENV)

/*
 * AIX dynamic sizeof(struct proc)
 *
 * AIX keeps its proc structures in an array.  The size of struct proc
 * varies from release to release of the OS.  In order to maintain
 * binary compatibility with releases later than what we build on, we
 * need to determine the size of struct proc at run time.
 *
 * We need this in order to walk the proc[] array to do PAG garbage
 * collection.
 *
 * We also need this in order to support 'klog -setpag', since the
 * kernel code needs to locate the proc structure for the parent process
 * of the current process.
 *
 * To compute sizeof(struct proc), we need the addresses of two proc
 * structures and their corresponding pids.  Given the pids, we can use
 * the PROCMASK() macro to compute their corresponding indices in the
 * proc[] array.  By dividing the distance between the pointers by the
 * number of proc structures, we can compute the size of a single proc
 * structure.
 *
 * We know the base address of the proc table from v.vb_proc:
 *
 * <sys/sysconfig.h> declares sysconfig() and SYS_GETPARMS;
 * (we don't use this, but I note it here for completeness)
 *
 * <sys/var.h> declares struct var and external variable v;
 *
 * v.v_proc		NPROC
 * v.vb_proc		&proc[0]
 * v.ve_proc		&proc[x] (current highwater mark for
 *				  proc[] array usage)
 *
 * The first proc pointer is v.vb_proc, which is the proc structure for
 * process 0.  Process 0's pointer to its first child is the other proc
 * pointer.  If process 0 has no children, we simply give up and do not
 * support features that require knowing the size of struct proc.
 */

static void
afs_procsize_init(void)
{
    struct proc *p0;	/* pointer to process 0 */
    struct proc *pN;	/* pointer to process 0's first child */
    int pN_index;
    ptrdiff_t pN_offset;
    int procsize;

    p0 = (struct proc *)v.vb_proc;
    if (!p0) {
	afs_gcpags = AFS_GCPAGS_EPROC0;
	return;
    }

    pN = p0->p_child;
    if (!pN) {
	afs_gcpags = AFS_GCPAGS_EPROCN;
	return;
    }

    if (pN->p_pid == p0->p_pid) {
	afs_gcpags = AFS_GCPAGS_EEQPID;
	return;
    }

    pN_index = PROCMASK(pN->p_pid);
    pN_offset = ((char *)pN - (char *)p0);
    procsize = pN_offset / pN_index;

    /*
     * check that the computation was exact
     */

    if (pN_index * procsize != pN_offset) {
	afs_gcpags = AFS_GCPAGS_EINEXACT;
	return;
    }

    /*
     * check that the proc table size is a multiple of procsize.
     */

    if ((((char *)v.ve_proc - (char *)v.vb_proc) % procsize) != 0) {
	afs_gcpags = AFS_GCPAGS_EPROCEND;
	return;
    }

    /* okay, use it */

    afs_gcpags_procsize = procsize;
}

#endif

/*
 * shutdown_cache
 *
 * Description:
 *	Clean up and shut down the AFS cache.
 *
 * Parameters:
 *	None.
 *
 * Environment:
 *	Nothing interesting.
 */
void
shutdown_cache()

{ /*shutdown_cache*/
    register struct afs_cbr *tsp, *nsp;
    extern int afs_cold_shutdown;
    extern int pagCounter;
    int i;

  AFS_STATCNT(shutdown_cache);
  afs_WriteThroughDSlots();
  if (afs_cold_shutdown) {
    afs_cacheinit_flag = 0;
    shutdown_dcache();
    shutdown_vcache();

    afs_cacheStats = 0;
    afs_cacheFiles = afs_cacheBlocks = 0;
    pag_epoch = maxIHint = nihints = usedihint = 0;
    pagCounter = 0;
    cacheInode = volumeInode = (ino_t)0;


    cacheInfoModTime = 0;

    afs_fsfragsize = 1023;
    bzero((char *)&afs_stats_cmperf, sizeof(afs_stats_cmperf));
    bzero((char *)&cacheDev, sizeof(struct osi_dev));
    osi_dnlc_shutdown();
  }
} /*shutdown_cache*/


void shutdown_vnodeops()
{
    extern int afs_cold_shutdown;
#ifndef AFS_LINUX20_ENV
    extern int afs_rd_stash_i;
#endif
#ifndef AFS_SUN5_ENV
    extern int lastWarnTime;
#endif
#if !defined(AFS_SGI_ENV) && !defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
    struct buf *afs_bread_freebp = 0;
#endif
    

    AFS_STATCNT(shutdown_vnodeops);
    if (afs_cold_shutdown) {
#ifndef	AFS_SUN5_ENV	/* XXX */
      lastWarnTime = 0;
#endif
#ifndef AFS_LINUX20_ENV
      afs_rd_stash_i = 0;      
#endif
#if !defined(AFS_SGI_ENV) && !defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
      afs_bread_freebp = 0;
#endif
      shutdown_mariner();
  }
}


void shutdown_AFS()

{
    int i;
    register struct srvAddr *sa;
    extern int afs_cold_shutdown;

    AFS_STATCNT(shutdown_AFS);
    if (afs_cold_shutdown) {
      afs_resourceinit_flag = 0; 
      /* 
       * Free Cells table allocations 
       */
      { 
	struct cell *tc;
	register struct afs_q *cq, *tq;
	for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	    tc = QTOC(cq); tq = QNext(cq);
	    if (tc->cellName)
		afs_osi_Free(tc->cellName, strlen(tc->cellName)+1);
	    afs_osi_Free(tc, sizeof(struct cell));
	}
      }
      /* 
       * Free Volumes table allocations 
       */
      { 
	struct volume *tv;
	for (i = 0; i < NVOLS; i++) {
	    for (tv = afs_volumes[i]; tv; tv = tv->next) {
		if (tv->name) {
		    afs_osi_Free(tv->name, strlen(tv->name)+1);
		    tv->name = 0;
		}
	    }
	    afs_volumes[i] = 0;
	}
      }

      /* 
       * Free FreeVolList allocations 
       */
      afs_osi_Free(Initialafs_freeVolList, afs_memvolumes * sizeof(struct volume));
      afs_freeVolList = Initialafs_freeVolList = 0;

      /* XXX HACK fort MEM systems XXX 
       *
       * For -memcache cache managers when we run out of free in memory volumes
       * we simply malloc more; we won't be able to free those additional volumes.
       */

      

      /* 
       * Free Users table allocation 
       */
      { 
	struct unixuser *tu, *ntu;
	for (i=0; i < NUSERS; i++) {
	    for (tu=afs_users[i]; tu; tu = ntu) {
		ntu = tu->next;
		if (tu->stp)
		    afs_osi_Free(tu->stp, tu->stLen);
		if (tu->exporter)
		    EXP_RELE(tu->exporter);
		afs_osi_Free(tu, sizeof(struct unixuser));
	    }
	    afs_users[i] = 0;
	}
      }

      /* 
       * Free Servers table allocation 
       */
      { 
	struct server *ts, *nts;
	struct conn *tc, *ntc;
	register struct afs_cbr *tcbrp, *tbrp;
	struct afs_cbr **lcbrpp;

	for (i=0; i < NSERVERS; i++) {
	    for (ts = afs_servers[i]; ts; ts = nts) {
		nts = ts->next;
		for (sa = ts->addr; sa; sa = sa->next_sa) {	
		    if (sa->conns) {
			/*
			 * Free all server's connection structs
			 */
			tc = sa->conns;
			while (tc) {
			    ntc = tc->next;
			    AFS_GUNLOCK();
			    rx_DestroyConnection(tc->id);
			    AFS_GLOCK();
			    afs_osi_Free(tc, sizeof(struct conn));
			    tc = ntc;
			}
		    }
		}
		for (tcbrp = ts->cbrs; tcbrp;  tcbrp = tbrp) {
		    /*
		     * Free all server's callback structs
		     */
		    tbrp = tcbrp->next;
		    afs_FreeCBR(tcbrp);
		}
		afs_osi_Free(ts, sizeof(struct server));
	    }
	    afs_servers[i] = 0;
	}
      }
      for (i=0; i<NFENTRIES; i++)
	fvTable[i] = 0;
      /* Reinitialize local globals to defaults */
      afs_osi_Free(afs_sysname, MAXSYSNAME);
      afs_sysname = 0;
      afs_marinerHost = 0;
      QInit(&CellLRU);      
      afs_setTimeHost = (struct server *)0;
      afs_volCounter = 1;
      afs_waitForever = afs_waitForeverCount = 0;
      afs_cellindex = 0;
      afs_FVIndex = -1;
      afs_server = (struct rx_service *)0;
      RWLOCK_INIT(&afs_xconn, "afs_xconn");
      bzero((char *)&afs_rootFid, sizeof(struct VenusFid));
      RWLOCK_INIT(&afs_xuser, "afs_xuser");
      RWLOCK_INIT(&afs_xvolume, "afs_xvolume"), RWLOCK_INIT(&afs_xcell, "afs_xcell");
      RWLOCK_INIT(&afs_xserver, "afs_xserver"), LOCK_INIT(&afs_puttofileLock, "afs_puttofileLock");
    }
    
} /*shutdown_AFS*/
