/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * AFS system call opcodes
 */

#ifndef _AFS_ARGS_H_
#define _AFS_ARGS_H_

#define	AFSOP_START_RXCALLBACK	  0	/* no aux parms */
#define	AFSOP_START_AFS		  1	/* no aux parms */
#define	AFSOP_START_BKG		  2	/* no aux parms */
#define	AFSOP_START_TRUNCDAEMON	  3	/* no aux parms */
#define AFSOP_START_CS		  4	/* no aux parms */

#define	AFSOP_ADDCELL		  5	/* parm 2 = cell str */
#define	AFSOP_CACHEINIT		  6	/* parms 2-4 -> cache sizes */
#define	AFSOP_CACHEINFO		  7	/* the cacheinfo file */
#define	AFSOP_VOLUMEINFO	  8	/* the volumeinfo file */
#define	AFSOP_CACHEFILE		  9	/* a random cache file (V*) */
#define	AFSOP_CACHEINODE	 10	/* random cache file by inode */
#define	AFSOP_AFSLOG		 11	/* output log file */
#define	AFSOP_ROOTVOLUME	 12	/* non-standard root volume name */
#define	AFSOP_STARTLOG		 14	/* temporary: Start afs logging */
#define	AFSOP_ENDLOG		 15	/* temporary: End afs logging */
#define AFSOP_AFS_VFSMOUNT	 16	/* vfsmount cover for hpux */
#define AFSOP_ADVISEADDR	 17	/* to init rx cid generator */
#define AFSOP_CLOSEWAIT 	 18	/* make all closes synchronous */
#define	AFSOP_RXEVENT_DAEMON	 19	/* rxevent daemon */
#define	AFSOP_GETMTU		 20	/* stand-in for SIOCGIFMTU, for now */
#define	AFSOP_GETIFADDRS	 21	/* get machine's ethernet interfaces */

#define	AFSOP_ADDCELL2		 29	/* 2nd add cell protocol interface */
#define	AFSOP_AFSDB_HANDLER	 30	/* userspace AFSDB lookup handler */
#define	AFSOP_SET_DYNROOT	 31	/* enable/disable dynroot support */
#define	AFSOP_ADDCELLALIAS	 32	/* create alias for existing cell */
#define	AFSOP_SET_FAKESTAT	 33	/* enable/disable fakestat support */
#define	AFSOP_CELLINFO		 34	/* set the cellinfo file name */
#define	AFSOP_SET_THISCELL	 35	/* set the primary cell */
#define AFSOP_BASIC_INIT	 36	/* used to be part of START_AFS */
#define AFSOP_SET_BACKUPTREE	 37	/* enable backup tree support */
#define AFSOP_SET_RXPCK		 38	/*set rx_extraPackets*/

/* The range 20-30 is reserved for AFS system offsets in the afs_syscall */
#define	AFSCALL_PIOCTL		20
#define	AFSCALL_SETPAG		21
#define	AFSCALL_IOPEN		22
#define	AFSCALL_ICREATE		23
#define	AFSCALL_IREAD		24
#define	AFSCALL_IWRITE		25
#define	AFSCALL_IINC		26
#define	AFSCALL_IDEC		27
#define	AFSCALL_CALL		28

#define AFSCALL_ICL             30

/* 64 bit versions of inode system calls. */
#define AFSCALL_IOPEN64		41
#define AFSCALL_ICREATE64	42
#define AFSCALL_IINC64		43
#define AFSCALL_IDEC64		44
#define AFSCALL_ILISTINODE64	45	/* Used by ListViceInodes */
#define AFSCALL_ICREATENAME64	46	/* pass in platform specific pointer
					 * used to create a name in in a
					 * directory.
					 */
#ifdef AFS_SGI_VNODE_GLUE
#define AFSCALL_INIT_KERNEL_CONFIG 47	/* set vnode glue ops. */
#endif

#ifdef	AFS_SGI53_ENV
#define AFSOP_NFSSTATICADDR	 32	/* to contents addr of nfs kernel addr */
#define AFSOP_NFSSTATICADDRPTR	 33	/* pass addr of variable containing 
					 * address into kernel. */
#define AFSOP_NFSSTATICADDR2	 34	/* pass address in as hyper. */
#define AFSOP_SBLOCKSTATICADDR2  35	/* for sblock and sbunlock */
#endif
#define	AFSOP_GETMASK		 42	/* stand-in for SIOCGIFNETMASK */
/* For SGI, this can't interfere with any of the 64 bit inode calls. */
#define AFSOP_RXLISTENER_DAEMON  48	/* starts kernel RX listener */

/* these are for initialization flags */

#define AFSCALL_INIT_MEMCACHE 0x1
#define AFSCALL_INIT_MEMCACHE_SLEEP 0x2	/* Use osi_Alloc to allocate memcache
					 * instead of osi_Alloc_NoSleep */

/* flags for rxstats pioctl */

#define AFSCALL_RXSTATS_MASK	0x7	/* Valid flag bits */
#define AFSCALL_RXSTATS_ENABLE	0x1	/* Enable RX stats */
#define AFSCALL_RXSTATS_DISABLE	0x2	/* Disable RX stats */
#define AFSCALL_RXSTATS_CLEAR	0x4	/* Clear RX stats */

#define	AFSOP_GO		100	/* whether settime is being done */
/* not for initialization: debugging assist */
#define	AFSOP_CHECKLOCKS	200	/* dump lock state */
#define	AFSOP_SHUTDOWN		201	/* Totally shutdown afs (deallocate all) */

/* The following aren't used by afs_initState but by afs_termState! */
#define	AFSOP_STOP_RXCALLBACK	210	/* Stop CALLBACK process */
#define	AFSOP_STOP_AFS		211	/* Stop AFS process */
#define AFSOP_STOP_CS		216	/* Stop CheckServer daemon. */
#define	AFSOP_STOP_BKG		212	/* Stop BKG process */
#define	AFSOP_STOP_TRUNCDAEMON	213	/* Stop cache truncate daemon */
/* #define AFSOP_STOP_RXEVENT   214     defined in osi.h	      */
/* #define AFSOP_STOP_COMPLETE     215  defined in osi.h	      */
/* #define AFSOP_STOP_RXK_LISTENER   217     defined in osi.h	      */
#define AFSOP_STOP_AFSDB	218	/* Stop AFSDB handler */

/* Main afs syscall entry; this number may vary per system (i.e. defined in afs/param.h) */
#ifndef	AFS_SYSCALL
#define	AFS_SYSCALL		31
#endif

/* arguments passed by afsd */
struct afs_cacheParams {
    afs_int32 cacheScaches;
    afs_int32 cacheFiles;
    afs_int32 cacheBlocks;
    afs_int32 cacheDcaches;
    afs_int32 cacheVolumes;
    afs_int32 chunkSize;
    afs_int32 setTimeFlag;
    afs_int32 memCacheFlag;
    afs_int32 inodes;
    afs_int32 users;
};

/*
 * Note that the AFS_*ALLOCSIZ values should be multiples of sizeof(void*) to
 * accomodate pointer alignment.
 */
/* Used in rx.c as well as afs directory. */
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
/* XXX Because of rxkad_cprivate... XXX */
#define	AFS_MDALLOCSIZ 	(127*sizeof(void *))	/* "Medium" allocated size */
#define	AFS_MALLOC_LOW_WATER	50	/* Min free blocks before allocating more */
#define	AFS_SMALLOCSIZ 	(38*sizeof(void *))	/* "Small" allocated size */
#else
#define	AFS_SMALLOCSIZ 	(64*sizeof(void *))	/*  "Small" allocated size */
#endif

/* Cache configuration available through the client callback interface */
typedef struct cm_initparams_v1 {
    afs_uint32 nChunkFiles;
    afs_uint32 nStatCaches;
    afs_uint32 nDataCaches;
    afs_uint32 nVolumeCaches;
    afs_uint32 firstChunkSize;
    afs_uint32 otherChunkSize;
    afs_uint32 cacheSize;
    afs_uint32 setTime;
    afs_uint32 memCache;
} cm_initparams_v1;

/*
 * If you need to change afs_cacheParams, you should probably create a brand
 * new structure.  Keeping the old structure will allow backwards compatibility
 * with old clients (even if it is only used to calculate allocation size).
 * If you do change the size or the format, you'll need to bump
 * AFS_CLIENT_CONFIG_RETRIEVAL_VERSION.  This allows some primitive form
 * of versioning a la rxdebug.
 */

#define AFS_CLIENT_RETRIEVAL_VERSION		1	/* latest version */
#define AFS_CLIENT_RETRIEVAL_FIRST_EDITION	1	/* first version */

/* Defines and structures for the AFS proc replacement layer for the original syscall (AFS_SYSCALL) strategy */

#ifdef AFS_LINUX20_ENV
 
#define PROC_FSDIRNAME "openafs"
#define PROC_SYSCALL_NAME "afs_ioctl"
#define PROC_SYSCALL_FNAME "/proc/fs/openafs/afs_ioctl"
#define PROC_SYSCALL_ARLA_FNAME "/proc/fs/nnpfs/afs_ioctl"
#define PROC_CELLSERVDB_NAME "CellServDB"
#define VIOC_SYSCALL_TYPE 'C' 
#define VIOC_SYSCALL _IOW(VIOC_SYSCALL_TYPE,1,void *)
#define VIOC_SYSCALL32 _IOW(VIOC_SYSCALL_TYPE,1,int)
 
struct afsprocdata {
  long param4;
  long param3;
  long param2;
  long param1;
  long syscall;
};

struct afsprocdata32 {
  unsigned int param4;
  unsigned int param3;
  unsigned int param2;
  unsigned int param1;
  unsigned int syscall;
};
 
#endif


#endif /* _AFS_ARGS_H_ */
