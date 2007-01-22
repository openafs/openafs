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

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

/* Exported variables */
struct osi_dev cacheDev;	/*Cache device */
afs_int32 cacheInfoModTime;	/*Last time cache info modified */
#if defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
struct mount *afs_cacheVfsp = 0;
#elif defined(AFS_LINUX20_ENV)
struct super_block *afs_cacheSBp = 0;
#else
struct vfs *afs_cacheVfsp = 0;
#endif
afs_rwlock_t afs_puttofileLock;	/* not used */
char *afs_sysname = 0;		/* So that superuser may change the
				 * local value of @sys */
char *afs_sysnamelist[MAXNUMSYSNAMES];	/* For support of a list of sysname */
int afs_sysnamecount = 0;
int afs_sysnamegen = 0;
struct volume *Initialafs_freeVolList;
int afs_memvolumes = 0;
#if defined(AFS_XBSD_ENV)
static struct vnode *volumeVnode;
#endif

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
afs_CacheInit(afs_int32 astatSize, afs_int32 afiles, afs_int32 ablocks,
	      afs_int32 aDentries, afs_int32 aVolumes, afs_int32 achunk,
	      afs_int32 aflags, afs_int32 ninodes, afs_int32 nusers)
{
    register afs_int32 i;
    register struct volume *tv;

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

    /* 
     * create volume list structure 
     */
    if (aVolumes < 50)
	aVolumes = 50;
    else if (aVolumes > 3000)
	aVolumes = 3000;

    tv = (struct volume *)afs_osi_Alloc(aVolumes * sizeof(struct volume));
    for (i = 0; i < aVolumes - 1; i++)
	tv[i].next = &tv[i + 1];
    tv[aVolumes - 1].next = NULL;
    afs_freeVolList = Initialafs_freeVolList = tv;
    afs_memvolumes = aVolumes;

    afs_cacheFiles = afiles;
    afs_cacheStats = astatSize;
    afs_vcacheInit(astatSize);
    afs_dcacheInit(afiles, ablocks, aDentries, achunk, aflags);
#ifdef AFS_64BIT_CLIENT
#ifdef AFS_VM_RDWR_ENV
    afs_vmMappingEnd = AFS_CHUNKBASE(0x7fffffff);
#endif /* AFS_VM_RDWR_ENV */
#endif /* AFS_64BIT_CLIENT */

#if defined(AFS_AIX_ENV) && !defined(AFS_AIX51_ENV)
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
    cm_initParams.cmi_cacheSize = afs_cacheBlocks;
    cm_initParams.cmi_setTime = afs_setTime;
    cm_initParams.cmi_memCache = (aflags & AFSCALL_INIT_MEMCACHE) ? 1 : 0;

    return 0;

}				/*afs_CacheInit */


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
afs_ComputeCacheParms(void)
{
    register afs_int32 i;
    afs_int32 afs_maxCacheDirty;

    /*
     * Don't allow more than 2/3 of the files in the cache to be dirty.
     */
    afs_maxCacheDirty = (2 * afs_cacheFiles) / 3;

    /*
     * Also, don't allow more than 2/3 of the total space get filled
     * with dirty chunks.  Compute the total number of chunks required
     * to fill the cache, make sure we don't set out limit above 2/3 of
     * that. If the cache size is greater than 1G, avoid overflow at
     * the expense of precision on the chunk size.
     */
    if (afs_cacheBlocks & 0xffe00000) {
	i = afs_cacheBlocks / (AFS_FIRSTCSIZE >> 10);
    } else {
	i = (afs_cacheBlocks << 10) / AFS_FIRSTCSIZE;
    }
    i = (2 * i) / 3;
    if (afs_maxCacheDirty > i)
	afs_maxCacheDirty = i;
    if (afs_maxCacheDirty < 1)
	afs_maxCacheDirty = 1;
    afs_stats_cmperf.cacheMaxDirtyChunks = afs_maxCacheDirty;
}				/*afs_ComputeCacheParms */


/*
 * LookupInodeByPath
 *
 * Look up inode given a file name.
 * Optionally return the vnode too.
 * If the vnode is not returned, we rele it.
 */
static int
LookupInodeByPath(char *filename, ino_t * inode, struct vnode **fvpp)
{
    afs_int32 code;

#ifdef AFS_LINUX22_ENV
    struct dentry *dp;
    code = gop_lookupname(filename, AFS_UIOSYS, 0, &dp);
    if (code)
	return code;
    *inode = dp->d_inode->i_ino;
    dput(dp);
#else
    struct vnode *filevp;
    code = gop_lookupname(filename, AFS_UIOSYS, 0, &filevp);
    if (code)
	return code;
    *inode = afs_vnodeToInumber(filevp);
    if (fvpp)
	*fvpp = filevp;
    else {
	AFS_RELE(filevp);
    }
#endif /* AFS_LINUX22_ENV */

    return 0;
}

int
afs_InitCellInfo(char *afile)
{
    ino_t inode;
    int code;

    code = LookupInodeByPath(afile, &inode, NULL);
    return afs_cellname_init(inode, code);
}

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

int
afs_InitVolumeInfo(char *afile)
{
    int code;
    struct osi_file *tfile;

    AFS_STATCNT(afs_InitVolumeInfo);
#if defined(AFS_XBSD_ENV)
    /*
     * On Open/Free/NetBSD, we can get into big trouble if we don't hold the volume file
     * vnode.  SetupVolume holds afs_xvolume lock exclusive.
     * SetupVolume->GetVolSlot->UFSGetVolSlot->{GetVolCache or WriteVolCache}
     * ->osi_UFSOpen->VFS_VGET()->ffs_vget->getnewvnode->vgone on some vnode.
     * If it's AFS, then ->vclean->afs_nbsd_reclaim->FlushVCache->QueueVCB->
     * GetVolume->FindVolume-> waits on afs_xvolume lock !
     *
     * In general, anything that's called with afs_xvolume locked must not
     * end up calling getnewvnode().  The only cases I've found so far
     * are things which try to get the volumeInode, and since we keep
     * it in the cache...
     */
    code = LookupInodeByPath(afile, &volumeInode, &volumeVnode);
#else
    code = LookupInodeByPath(afile, &volumeInode, NULL);
#endif
    if (code)
	return code;
    tfile = afs_CFileOpen(volumeInode);
    afs_CFileTruncate(tfile, 0);
    afs_CFileClose(tfile);
    return 0;
}

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
int
afs_InitCacheInfo(register char *afile)
{
    register afs_int32 code;
    struct osi_stat tstat;
    register struct osi_file *tfile;
    struct afs_fheader theader;
#ifndef AFS_LINUX22_ENV
    struct vnode *filevp;
#endif
    int goodFile;

    AFS_STATCNT(afs_InitCacheInfo);
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS)
	osi_Panic("afs_InitCacheInfo --- called for non-ufs cache!");
#ifdef AFS_LINUX22_ENV
    code = osi_InitCacheInfo(afile);
    if (code)
	return code;
#else
    code = gop_lookupname(afile, AFS_UIOSYS, 0, &filevp);
    if (code || !filevp)
	return ENOENT;
    {
#if	defined(AFS_SUN56_ENV)
	struct statvfs64 st;
#elif	defined(AFS_HPUX102_ENV)
	struct k_statvfs st;
#elif	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV) ||defined(AFS_HPUX100_ENV)
	struct statvfs st;
#elif defined(AFS_DUX40_ENV)
	struct nstatfs st;
#elif defined(AFS_DARWIN80_ENV)
	struct vfsstatfs st;
#else
	struct statfs st;
#endif /* SUN56 */

#if	defined(AFS_SGI_ENV)
#ifdef AFS_SGI65_ENV
	VFS_STATVFS(filevp->v_vfsp, &st, NULL, code);
	if (!code)
#else
	if (!VFS_STATFS(filevp->v_vfsp, &st, NULL))
#endif /* AFS_SGI65_ENV */
#elif	defined(AFS_SUN5_ENV) || defined(AFS_HPUX100_ENV)
	if (!VFS_STATVFS(filevp->v_vfsp, &st))
#elif defined(AFS_OSF_ENV)

	VFS_STATFS(filevp->v_vfsp, code);
	/* struct copy */
	st = filevp->v_vfsp->m_stat;
	if (code == 0)
#elif defined(AFS_AIX41_ENV)
	if (!VFS_STATFS(filevp->v_vfsp, &st, &afs_osi_cred))
#elif defined(AFS_LINUX20_ENV)
	{
	    KERNEL_SPACE_DECL;
	    TO_USER_SPACE();

	    VFS_STATFS(filevp->v_vfsp, &st);
	    TO_KERNEL_SPACE();
	}
#elif defined(AFS_DARWIN80_ENV)
        afs_cacheVfsp = vnode_mount(filevp);
	if (afs_cacheVfsp && ((st = *(vfs_statfs(afs_cacheVfsp))),1))
#elif defined(AFS_DARWIN_ENV)
	if (!VFS_STATFS(filevp->v_mount, &st, current_proc()))
#elif defined(AFS_FBSD50_ENV)
	if (!VFS_STATFS(filevp->v_mount, &st, curthread))
#elif defined(AFS_XBSD_ENV)
	if (!VFS_STATFS(filevp->v_mount, &st, curproc))
#else
	if (!VFS_STATFS(filevp->v_vfsp, &st))
#endif /* SGI... */
#if	defined(AFS_SUN5_ENV) || defined(AFS_HPUX100_ENV)
	    afs_fsfragsize = st.f_frsize - 1;
#else
	    afs_fsfragsize = st.f_bsize - 1;
#endif
    }
#if defined(AFS_LINUX20_ENV)
    cacheInode = filevp->i_ino;
    afs_cacheSBp = filevp->i_sb;
#elif defined(AFS_XBSD_ENV)
    cacheInode = VTOI(filevp)->i_number;
    cacheDev.mp = filevp->v_mount;
    cacheDev.held_vnode = filevp;
    vref(filevp);		/* Make sure mount point stays busy. XXX */
#if !defined(AFS_OBSD_ENV)
    afs_cacheVfsp = filevp->v_vfsp;
#endif
#else
#if defined(AFS_SGI62_ENV) || defined(AFS_HAVE_VXFS) || defined(AFS_DARWIN_ENV)
    afs_InitDualFSCacheOps(filevp);
#endif
    cacheInode = afs_vnodeToInumber(filevp);
    cacheDev.dev = afs_vnodeToDev(filevp);
#ifndef AFS_DARWIN80_ENV
    afs_cacheVfsp = filevp->v_vfsp;
#endif
#endif /* AFS_LINUX20_ENV */
    AFS_RELE(filevp);
#endif /* AFS_LINUX22_ENV */
    tfile = osi_UFSOpen(cacheInode);
    afs_osi_Stat(tfile, &tstat);
    cacheInfoModTime = tstat.mtime;
    code = afs_osi_Read(tfile, -1, &theader, sizeof(theader));
    goodFile = 0;
    if (code == sizeof(theader)) {
	/* read the header correctly */
	if (theader.magic == AFS_FHMAGIC
	    && theader.firstCSize == AFS_FIRSTCSIZE
	    && theader.otherCSize == AFS_OTHERCSIZE
	    && theader.version == AFS_CI_VERSION)
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
}

int afs_resourceinit_flag = 0;
int
afs_ResourceInit(int preallocs)
{
    register afs_int32 i;
    static struct rx_securityClass *secobj;

    AFS_STATCNT(afs_ResourceInit);
    RWLOCK_INIT(&afs_xuser, "afs_xuser");
    RWLOCK_INIT(&afs_xvolume, "afs_xvolume");
    RWLOCK_INIT(&afs_xserver, "afs_xserver");
    RWLOCK_INIT(&afs_xsrvAddr, "afs_xsrvAddr");
    RWLOCK_INIT(&afs_xinterface, "afs_xinterface");
    LOCK_INIT(&afs_puttofileLock, "afs_puttofileLock");
#ifndef AFS_FBSD_ENV
    LOCK_INIT(&osi_fsplock, "osi_fsplock");
    LOCK_INIT(&osi_flplock, "osi_flplock");
#endif
    RWLOCK_INIT(&afs_xconn, "afs_xconn");

    afs_CellInit();
    afs_InitCBQueue(1);		/* initialize callback queues */

    if (afs_resourceinit_flag == 0) {
	afs_resourceinit_flag = 1;
	for (i = 0; i < NFENTRIES; i++)
	    fvTable[i] = 0;
	for (i = 0; i < MAXNUMSYSNAMES; i++)
	    afs_sysnamelist[i] = afs_osi_Alloc(MAXSYSNAME);
	afs_sysname = afs_sysnamelist[0];
	strcpy(afs_sysname, SYS_NAME);
	afs_sysnamecount = 1;
	afs_sysnamegen++;
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

}				/*afs_ResourceInit */

#if defined(AFS_AIX_ENV) && !defined(AFS_AIX51_ENV)

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
    struct proc *p0;		/* pointer to process 0 */
    struct proc *pN;		/* pointer to process 0's first child */
#ifdef AFS_AIX51_ENV
    struct pvproc *pV;
#endif
    int pN_index;
    ptrdiff_t pN_offset;
    int procsize;

    p0 = (struct proc *)v.vb_proc;
    if (!p0) {
	afs_gcpags = AFS_GCPAGS_EPROC0;
	return;
    }
#ifdef AFS_AIX51_ENV
    pN = (struct proc *)0;
    pV = p0->p_pvprocp;
    if (pV) {
	pV = pV->pv_child;
	if (pV)
	    pN = pV->pv_procp;
    }
#else
    pN = p0->p_child;
#endif
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
shutdown_cache(void)
{
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
#if defined(AFS_XBSD_ENV)
	vrele(volumeVnode);	/* let it go, finally. */
	volumeVnode = NULL;
	if (cacheDev.held_vnode) {
	    vrele(cacheDev.held_vnode);
	    cacheDev.held_vnode = NULL;
	}
#endif
	cacheInode = volumeInode = (ino_t) 0;

	cacheInfoModTime = 0;

	afs_fsfragsize = 1023;
	memset((char *)&afs_stats_cmperf, 0, sizeof(afs_stats_cmperf));
	memset((char *)&cacheDev, 0, sizeof(struct osi_dev));
	osi_dnlc_shutdown();
    }
}				/*shutdown_cache */


void
shutdown_vnodeops(void)
{
#if !defined(AFS_SGI_ENV) && !defined(AFS_SUN5_ENV)
    struct buf *afs_bread_freebp = 0;
#endif


    AFS_STATCNT(shutdown_vnodeops);
    if (afs_cold_shutdown) {
#ifndef	AFS_SUN5_ENV		/* XXX */
	lastWarnTime = 0;
#endif
#ifndef AFS_LINUX20_ENV
	afs_rd_stash_i = 0;
#endif
#if !defined(AFS_SGI_ENV) && !defined(AFS_SUN5_ENV)
	afs_bread_freebp = 0;
#endif
	shutdown_mariner();
    }
}


void
shutdown_AFS(void)
{
    int i;
    register struct srvAddr *sa;

    AFS_STATCNT(shutdown_AFS);
    if (afs_cold_shutdown) {
	afs_resourceinit_flag = 0;
	/* 
	 * Free Volumes table allocations 
	 */
	{
	    struct volume *tv;
	    for (i = 0; i < NVOLS; i++) {
		for (tv = afs_volumes[i]; tv; tv = tv->next) {
		    if (tv->name) {
			afs_osi_Free(tv->name, strlen(tv->name) + 1);
			tv->name = 0;
		    }
		}
		afs_volumes[i] = 0;
	    }
	}

	/* 
	 * Free FreeVolList allocations 
	 */
	afs_osi_Free(Initialafs_freeVolList,
		     afs_memvolumes * sizeof(struct volume));
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
	    for (i = 0; i < NUSERS; i++) {
		for (tu = afs_users[i]; tu; tu = ntu) {
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

	    for (i = 0; i < NSERVERS; i++) {
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
		    for (tcbrp = ts->cbrs; tcbrp; tcbrp = tbrp) {
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
	for (i = 0; i < NFENTRIES; i++)
	    fvTable[i] = 0;
	/* Reinitialize local globals to defaults */
	for (i = 0; i < MAXNUMSYSNAMES; i++)
	    afs_osi_Free(afs_sysnamelist[i], MAXSYSNAME);
	afs_sysname = 0;
	afs_sysnamecount = 0;
	afs_marinerHost = 0;
	afs_setTimeHost = NULL;
	afs_volCounter = 1;
	afs_waitForever = afs_waitForeverCount = 0;
	afs_FVIndex = -1;
	afs_server = (struct rx_service *)0;
	RWLOCK_INIT(&afs_xconn, "afs_xconn");
	memset((char *)&afs_rootFid, 0, sizeof(struct VenusFid));
	RWLOCK_INIT(&afs_xuser, "afs_xuser");
	RWLOCK_INIT(&afs_xvolume, "afs_xvolume");
	RWLOCK_INIT(&afs_xserver, "afs_xserver");
	LOCK_INIT(&afs_puttofileLock, "afs_puttofileLock");

	shutdown_cell();
	shutdown_server();
    }
}
