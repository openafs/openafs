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
#include "../afs/afs_stats.h"
#include "../rx/rx_globals.h"
#if !defined(UKERNEL) && !defined(AFS_LINUX20_ENV)
#include "net/if.h"
#ifdef AFS_SGI62_ENV
#include "../h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV)
#include "netinet/in_var.h"
#endif
#endif /* !defined(UKERNEL) */
#ifdef AFS_LINUX22_ENV
#include "../h/smp_lock.h"
#endif


#if	defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_HPUX_ENV)
#define	AFS_MINBUFFERS	100
#else
#define	AFS_MINBUFFERS	50
#endif

struct afsop_cell {
    afs_int32 hosts[MAXCELLHOSTS];
    char cellName[100];
};

char afs_zeros[AFS_ZEROS];
char afs_rootVolumeName[64]="";
struct afs_icl_set *afs_iclSetp = (struct afs_icl_set*)0;
struct afs_icl_set *afs_iclLongTermSetp = (struct afs_icl_set*)0;

#if	defined(AFS_GLOBAL_SUNLOCK) && !defined(AFS_HPUX_ENV) && !defined(AFS_AIX41_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_LINUX22_ENV)
kmutex_t afs_global_lock;
kmutex_t afs_rxglobal_lock;

#if defined(AFS_SGI_ENV) && !defined(AFS_SGI64_ENV)
long afs_global_owner;
#endif
#endif

#if defined(AFS_OSF_ENV)
simple_lock_data_t afs_global_lock;
thread_t afs_global_owner;
#endif /* AFS_OSF_ENV */

#if defined(AFS_AIX41_ENV)
simple_lock_data afs_global_lock;
#endif

afs_int32 afs_initState = 0;
afs_int32 afs_termState = 0;
afs_int32 afs_setTime = 0;
int afs_cold_shutdown = 0;
char afs_SynchronousCloses = '\0';
static int afs_CB_Running = 0;
static int AFS_Running = 0;
static int afs_CacheInit_Done = 0;
static int afs_Go_Done = 0;
extern struct interfaceAddr afs_cb_interface;
static int afs_RX_Running = 0;

static int
Afscall_icl(long opcode, long p1, long p2, long p3, long p4, long *retval);

#if defined(AFS_HPUX_ENV)
extern int afs_vfs_mount();
#endif /* defined(AFS_HPUX_ENV) */

/* This is code which needs to be called once when the first daemon enters
 * the client. A non-zero return means an error and AFS should not start.
 */
static int afs_InitSetup(int preallocs)
{
    extern void afs_InitStats();
    int code;

#ifndef AFS_NOSTATS
    /*
     * Set up all the AFS statistics variables.  This should be done
     * exactly once, and it should be done here, the first resource-setting
     * routine to be called by the CM/RX.
     */
    afs_InitStats();
#endif /* AFS_NOSTATS */
    
    bzero(afs_zeros, AFS_ZEROS);

    /* start RX */
    rx_extraPackets = AFS_NRXPACKETS;	/* smaller # of packets */
    code = rx_Init(htons(7001));
    if (code) {
	printf("AFS: RX failed to initialize.\n");
	return code;
    }
    rx_SetRxDeadTime(AFS_RXDEADTIME);
    /* resource init creates the services */
    afs_ResourceInit(preallocs);

    return code;
}

afs_syscall_call(parm, parm2, parm3, parm4, parm5, parm6)
long parm, parm2, parm3, parm4, parm5, parm6;
{
    afs_int32 code = 0;

    AFS_STATCNT(afs_syscall_call);
#ifdef	AFS_SUN5_ENV
    if (!afs_suser(CRED()) && (parm != AFSOP_GETMTU) 
	&& (parm != AFSOP_GETMASK)) {
      /* only root can run this code */
	return (EACCES);
#else
    if (!afs_suser() && (parm != AFSOP_GETMTU)
	&& (parm != AFSOP_GETMASK)) {
      /* only root can run this code */
#if !defined(AFS_SGI_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_LINUX20_ENV)
	setuerror(EACCES);
	return(EACCES);
#else
#if	defined(AFS_OSF_ENV)
	return EACCES;
#else	/* AFS_OSF_ENV */
	return EPERM;
#endif
#endif
#endif
    }
    AFS_GLOCK();
    if (parm == AFSOP_START_RXCALLBACK) {
	if (afs_CB_Running) goto out;
	afs_CB_Running = 1;
#ifndef RXK_LISTENER_ENV
	code = afs_InitSetup(parm2);
	if (!code) 
#endif /* RXK_LISTENER_ENV */
	    {
#ifdef RXK_LISTENER_ENV
		while (afs_RX_Running != 2)
		    afs_osi_Sleep(&afs_RX_Running);
#else
		afs_initState = AFSOP_START_AFS;
		afs_osi_Wakeup(&afs_initState);
#endif /* RXK_LISTENER_ENV */
		afs_osi_Invisible();
		afs_RXCallBackServer();
	    }
#ifdef	AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, code);
#endif
    }
#ifdef RXK_LISTENER_ENV
    else if (parm == AFSOP_RXLISTENER_DAEMON) {
	if (afs_RX_Running) goto out;
	afs_RX_Running = 1;
	code = afs_InitSetup(parm2);
	if (parm3) {
	    rx_enablePeerRPCStats();
	}
	if (parm4) {
	    rx_enableProcessRPCStats();
	}
	if (!code) {
	    afs_initState = AFSOP_START_AFS;
	    afs_osi_Wakeup(&afs_initState);
	    afs_osi_Invisible();
	    afs_RX_Running = 2;
	    afs_osi_Wakeup(&afs_RX_Running);
	    rxk_Listener();
	}
#ifdef	AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, code);
#endif
    }
#endif
    else if (parm == AFSOP_START_AFS) {
	/* afs daemon */
	afs_int32 temp;

	if (AFS_Running) goto out;
	AFS_Running = 1;
	while (afs_initState < AFSOP_START_AFS) 
	    afs_osi_Sleep(&afs_initState);

#if defined(AFS_SUN_ENV) || defined(AFS_SGI_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_LINUX20_ENV)
	temp = AFS_MINBUFFERS;  /* Should fix this soon */
#else
	temp = ((afs_bufferpages * NBPG)>>11);	/* number of 2k buffers we could get from all of the buffer space */
	temp = temp>>2;	/* don't take more than 25% (our magic parameter) */
	if (temp < AFS_MINBUFFERS) temp = AFS_MINBUFFERS;   /* although we really should have this many */
#endif
	DInit(temp);
	afs_initState = AFSOP_START_BKG;
	afs_osi_Wakeup(&afs_initState);
	afs_osi_Invisible();
	afs_Daemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif
    }
    else if (parm == AFSOP_START_CS) {
	afs_osi_Invisible();
	afs_CheckServerDaemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif
    }
    else if (parm == AFSOP_START_BKG) {
	while (afs_initState < AFSOP_START_BKG) 
	    afs_osi_Sleep(&afs_initState);
	if (afs_initState < AFSOP_GO) {
	    afs_initState = AFSOP_GO;
	    afs_osi_Wakeup(&afs_initState);
	}
	/* start the bkg daemon */
	afs_osi_Invisible();
#ifdef	AFS_AIX32_ENV
	if (parm2)
	    afs_BioDaemon(parm2);
	else
#endif
	    afs_BackgroundDaemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif
    }
    else if (parm == AFSOP_START_TRUNCDAEMON) {
	while (afs_initState < AFSOP_GO) 
	    afs_osi_Sleep(&afs_initState);
	/* start the bkg daemon */
	afs_osi_Invisible();
	afs_CacheTruncateDaemon();
#ifdef	AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif
    }
#if defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV)
    else if (parm == AFSOP_RXEVENT_DAEMON) {
	while (afs_initState < AFSOP_START_BKG) afs_osi_Sleep(&afs_initState);
	afs_osi_Invisible();
	afs_rxevent_daemon();
#ifdef AFS_SGI_ENV
	AFS_GUNLOCK();
	exit(CLD_EXITED, 0);
#endif
    }
#endif	
    else if (parm == AFSOP_ADDCELL) {
	/* add a cell.  Parameter 2 is 8 hosts (in net order),  parm 3 is the null-terminated
	 name.  Parameter 4 is the length of the name, including the null.  Parm 5 is the
	 home cell flag (0x1 bit) and the nosuid flag (0x2 bit) */
	struct afsop_cell tcell;

	while (afs_initState < AFSOP_START_BKG) afs_osi_Sleep(&afs_initState);
	AFS_COPYIN((char *)parm2, (char *)tcell.hosts, sizeof(tcell.hosts), code);
	if (!code) {
	    if (parm4 > sizeof(tcell.cellName)) 
		code = EFAULT;
	    else {
		AFS_COPYIN((char *)parm3, tcell.cellName, parm4, code);
		if (!code) 
		    afs_NewCell(tcell.cellName, tcell.hosts, parm5,
				(char *)0, (u_short)0, (u_short)0);
	    }
	}
    } else if (parm == AFSOP_ADDCELL2) {
	struct afsop_cell tcell;
	char *tbuffer = osi_AllocSmallSpace(AFS_SMALLOCSIZ), *lcnamep = 0;
	char *tbuffer1 = osi_AllocSmallSpace(AFS_SMALLOCSIZ), *cnamep = 0;
#if defined(AFS_SGI61_ENV) || defined(AFS_SUN57_ENV)
	size_t bufferSize;	
#else /* AFS_SGI61_ENV */
	u_int bufferSize;	
#endif /* AFS_SGI61_ENV */
	int cflags = parm4;

	/* wait for basic init */
	while (afs_initState < AFSOP_START_BKG) afs_osi_Sleep(&afs_initState);

	AFS_COPYIN((char *)parm2, (char *)tcell.hosts, sizeof(tcell.hosts), code);
	if (!code) {
	    AFS_COPYINSTR((char *)parm3, tbuffer1, AFS_SMALLOCSIZ, &bufferSize, code);
	    if (!code) {
		if (parm4 & 4) {
		    AFS_COPYINSTR((char *)parm5, tbuffer, AFS_SMALLOCSIZ, &bufferSize, code);
		    if (!code) {
			lcnamep = tbuffer;
			cflags |= CLinkedCell;
		    }
		}
		if (!code)
		    afs_NewCell(tbuffer1, tcell.hosts, cflags, 
				lcnamep, (u_short)0, (u_short)0);
	    }
	}
	osi_FreeSmallSpace(tbuffer);
	osi_FreeSmallSpace(tbuffer1);
    }
    else if (parm == AFSOP_CACHEINIT) {
	struct afs_cacheParams cparms;

	if (afs_CacheInit_Done) goto out;

	/* wait for basic init */
	while (afs_initState < AFSOP_START_BKG) afs_osi_Sleep(&afs_initState);
	AFS_COPYIN((char *)parm2, (caddr_t) &cparms, sizeof(cparms), code);
	if (code) {
#if	defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined (AFS_SGI64_ENV) || defined(AFS_LINUX20_ENV)
	    goto out;
#else
	    setuerror(code);
	    code = -1;
	    goto out;
#endif
	}
	afs_CacheInit_Done = 1;
    {
	struct afs_icl_log *logp;
	/* initialize the ICL system */
	code = afs_icl_CreateLog("cmfx", 60*1024, &logp);
	if (code == 0)
	    code = afs_icl_CreateSetWithFlags("cm", logp,
					      (struct icl_log *) 0,
					      ICL_CRSET_FLAG_DEFAULT_OFF,
					      &afs_iclSetp);
	    code = afs_icl_CreateSet("cmlongterm", logp, (struct icl_log*) 0,
				 &afs_iclLongTermSetp);
    }
	afs_setTime = cparms.setTimeFlag;

	code = afs_CacheInit(cparms.cacheScaches,
			     cparms.cacheFiles,
			     cparms.cacheBlocks,
			     cparms.cacheDcaches,
			     cparms.cacheVolumes,
			     cparms.chunkSize,
			     cparms.memCacheFlag,
			     cparms.inodes,
			     cparms.users);

    }
    else if (parm == AFSOP_CACHEINODE) {
	ino_t ainode = parm2;
	/* wait for basic init */
	while (afs_initState < AFSOP_START_BKG) afs_osi_Sleep(&afs_initState);

	/* do it by inode */
#ifdef AFS_SGI62_ENV
	ainode = (ainode << 32) | (parm3 & 0xffffffff);
#endif
	code = afs_InitCacheFile((char *) 0, ainode);
    }
    else if (parm == AFSOP_ROOTVOLUME) {
#if defined(AFS_SGI61_ENV) || defined(AFS_SUN57_ENV)
	size_t bufferSize;
#else /* AFS_SGI61_ENV */
	u_int bufferSize;
#endif /* AFS_SGI61_ENV */

	/* wait for basic init */
	while (afs_initState < AFSOP_START_BKG) afs_osi_Sleep(&afs_initState);

	if (parm2) {
	    AFS_COPYINSTR((char *)parm2, afs_rootVolumeName, sizeof(afs_rootVolumeName), &bufferSize, code);
	    afs_rootVolumeName[sizeof(afs_rootVolumeName)-1] = 0;
	}
	else code = 0;
    }
    else if (parm == AFSOP_CACHEFILE || parm == AFSOP_CACHEINFO ||
	      parm == AFSOP_VOLUMEINFO || parm == AFSOP_AFSLOG) {
	char *tbuffer = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
#if defined(AFS_SGI61_ENV) || defined(AFS_SUN57_ENV)
	size_t bufferSize;
#else /* AFS_SGI61_ENV */
	u_int bufferSize;
#endif /* AFS_SGI61_ENV */

	/* wait for basic init */
	while (afs_initState < AFSOP_START_BKG) afs_osi_Sleep(&afs_initState);
	code = 0;
	AFS_COPYINSTR((char *)parm2, tbuffer, AFS_SMALLOCSIZ, &bufferSize, code);
	if (code) {
	    osi_FreeSmallSpace(tbuffer);
	    goto out;
	}
	if (!code) {
	    tbuffer[AFS_SMALLOCSIZ-1] = 0;	/* null-terminate the name */
	    /* we now have the cache dir copied in.  Call the cache init routines */
	    if (parm == AFSOP_CACHEFILE) code = afs_InitCacheFile(tbuffer, 0);
	    else if (parm == AFSOP_CACHEINFO) code = afs_InitCacheInfo(tbuffer);
	    else if (parm == AFSOP_VOLUMEINFO) code = afs_InitVolumeInfo(tbuffer);
	}
	osi_FreeSmallSpace(tbuffer);
    }
    else if (parm == AFSOP_GO) {
	/* the generic initialization calls come here.  One parameter: should we do the
	      set-time operation on this workstation */
	if (afs_Go_Done) goto out;
	afs_Go_Done = 1;
	while (afs_initState < AFSOP_GO) afs_osi_Sleep(&afs_initState);
	afs_initState = 101;
	afs_setTime = parm2;
	afs_osi_Wakeup(&afs_initState);
#if	(!defined(AFS_NONFSTRANS) && !defined(AFS_DEC_ENV)) || defined(AFS_AIX_IAUTH_ENV)
	afs_nfsclient_init();
#endif
	printf("found %d non-empty cache files (%d%%).\n", afs_stats_cmperf.cacheFilesReused,
	       (100*afs_stats_cmperf.cacheFilesReused) /
	       (afs_stats_cmperf.cacheNumEntries?afs_stats_cmperf.cacheNumEntries : 1));
    }
    else if (parm == AFSOP_ADVISEADDR) {
	/* pass in the host address to the rx package */
	afs_int32  	count        = parm2;
	afs_int32 	buffer[AFS_MAX_INTERFACE_ADDR];
	afs_int32 	maskbuffer[AFS_MAX_INTERFACE_ADDR];
	afs_int32 	mtubuffer[AFS_MAX_INTERFACE_ADDR];
	int 	i;
	int	code;

	if (  count > AFS_MAX_INTERFACE_ADDR ) {
	   code = ENOMEM;
	   count = AFS_MAX_INTERFACE_ADDR;
	}
	   
	AFS_COPYIN( (char *)parm3, (char *)buffer, count*sizeof(afs_int32), code);
	if (parm4)
	  AFS_COPYIN((char *)parm4, (char *)maskbuffer, count*sizeof(afs_int32), code);
	if (parm5)
	  AFS_COPYIN((char *)parm5, (char *)mtubuffer, count*sizeof(afs_int32), code);

	afs_cb_interface.numberOfInterfaces = count;
	for (i=0; i < count ; i++) {
	   afs_cb_interface.addr_in[i] = buffer[i];
#ifdef AFS_USERSPACE_IP_ADDR	
	   /* AFS_USERSPACE_IP_ADDR means we have no way of finding the
	    * machines IP addresses when in the kernel (the in_ifaddr
	    * struct is not available), so we pass the info in at
	    * startup. We also pass in the subnetmask and mtu size. The
	    * subnetmask is used when setting the rank:
	    * afsi_SetServerIPRank(); and the mtu size is used when
	    * finding the best mtu size. rxi_FindIfnet() is replaced
	    * with rxi_Findcbi().
	    */
	   afs_cb_interface.subnetmask[i] = (parm4 ? maskbuffer[i] : 0xffffffff);
	   afs_cb_interface.mtu[i]        = (parm5 ? mtubuffer[i]  : htonl(1500));
#endif
	}
	afs_uuid_create(&afs_cb_interface.uuid);
	rxi_setaddr(buffer[0]);
    }

#ifdef	AFS_SGI53_ENV
    else if (parm == AFSOP_NFSSTATICADDR) {
	extern int (*nfs_rfsdisptab_v2)();
	nfs_rfsdisptab_v2 = (int (*)())parm2;
    }
    else if (parm == AFSOP_NFSSTATICADDR2) {
	extern int (*nfs_rfsdisptab_v2)();
#ifdef _K64U64
	nfs_rfsdisptab_v2 = (int (*)())((parm2<<32) | (parm3 & 0xffffffff));
#else /* _K64U64 */
	nfs_rfsdisptab_v2 = (int (*)())(parm3 & 0xffffffff);
#endif /* _K64U64 */
    }
#if defined(AFS_SGI62_ENV) && !defined(AFS_SGI65_ENV)
    else if (parm == AFSOP_SBLOCKSTATICADDR2) {
	extern int (*afs_sblockp)();
	extern void (*afs_sbunlockp)();
#ifdef _K64U64
	afs_sblockp = (int (*)())((parm2<<32) | (parm3 & 0xffffffff));
	afs_sbunlockp = (void (*)())((parm4<<32) | (parm5 & 0xffffffff));
#else 
	afs_sblockp = (int (*)())(parm3 & 0xffffffff);
	afs_sbunlockp = (void (*)())(parm5 & 0xffffffff);
#endif /* _K64U64 */
    }
#endif /* AFS_SGI62_ENV && !AFS_SGI65_ENV */
#endif /* AFS_SGI53_ENV */
    else if (parm == AFSOP_SHUTDOWN) {
#if	defined(AFS_OSF_ENV)
	extern struct mount *afs_globalVFS;
#else	/* AFS_OSF_ENV */
	extern struct vfs *afs_globalVFS;
#endif
	afs_cold_shutdown = 0;
	if (parm == 1) afs_cold_shutdown = 1;
	if (afs_globalVFS != 0) {
	    afs_warn("AFS isn't unmounted yet! Call aborted\n");
	    code = EACCES;
	}
	afs_shutdown();
    }

#if	! defined(AFS_HPUX90_ENV) || defined(AFS_HPUX100_ENV)
    else if (parm == AFSOP_AFS_VFSMOUNT) {
#ifdef	AFS_HPUX_ENV
#if defined(AFS_HPUX100_ENV)
	vfsmount(parm2, parm3, parm4, parm5);
#else
      afs_vfs_mount(parm2, parm3, parm4, parm5);
#endif /* AFS_HPUX100_ENV */
#else /* defined(AFS_HPUX_ENV) */
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_LINUX20_ENV)
      code = EINVAL;
#else
      setuerror(EINVAL);
#endif
#endif /* defined(AFS_HPUX_ENV) */
    }
#endif
    else if (parm == AFSOP_CLOSEWAIT) {
	afs_SynchronousCloses = 'S';
    }
    else if (parm == AFSOP_GETMTU) { 
      afs_uint32 mtu = 0;
#if	!defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV)
#ifdef AFS_USERSPACE_IP_ADDR
      afs_int32 i;
      i = rxi_Findcbi(parm2);
      mtu = ((i == -1) ? htonl(1500) : afs_cb_interface.mtu[i]);
#else /* AFS_USERSPACE_IP_ADDR */
      struct ifnet *tifnp;
      struct in_ifaddr *tifadp = (struct in_ifaddr *) 0;
      extern struct ifnet *rxi_FindIfnet();
 
      tifnp = rxi_FindIfnet(parm2, &tifadp);  /*  make iterative */
      mtu = (tifnp ? tifnp->if_mtu : htonl(1500));
#endif /* else AFS_USERSPACE_IP_ADDR */
#endif /* !AFS_SUN5_ENV */
      if (!code) 
	 AFS_COPYOUT ((caddr_t)&mtu, (caddr_t)parm3, sizeof(afs_int32), code);
#ifdef AFS_AIX32_ENV
/* this is disabled for now because I can't figure out how to get access
 * to these kernel variables.  It's only for supporting user-mode rx
 * programs -- it makes a huge difference on the 220's in my testbed,
 * though I don't know why. The bosserver does this with /etc/no, so it's
 * being handled a different way for the servers right now.  */
/*      {
	static adjusted = 0;
	extern u_long sb_max_dflt;
	if (!adjusted) {
	  adjusted = 1;
	  if (sb_max_dflt < 131072) sb_max_dflt = 131072; 
	  if (sb_max < 131072) sb_max = 131072; 
	}
      } */
#endif /* AFS_AIX32_ENV */
    }
    else if (parm == AFSOP_GETMASK) {  /* parm2 == addr in net order */
      afs_uint32 mask = 0;
#if	!defined(AFS_SUN5_ENV)
#ifdef AFS_USERSPACE_IP_ADDR
      afs_int32 i;
      i = rxi_Findcbi(parm2);
      if (i != -1) {
	 mask = afs_cb_interface.subnetmask[i];
      } else {
	 code = -1;
      }
#else /* AFS_USERSPACE_IP_ADDR */
      struct ifnet *tifnp;
      struct in_ifaddr *tifadp = (struct in_ifaddr *) 0;
      extern struct ifnet *rxi_FindIfnet();
      tifnp = rxi_FindIfnet(parm2, &tifadp);  /* make iterative */
      if (tifnp && tifadp) {
	 mask = tifadp->ia_subnetmask;
      } else {
	 code = -1;
      }
#endif /* else AFS_USERSPACE_IP_ADDR */
#endif /* !AFS_SUN5_ENV */
      if (!code) 
	 AFS_COPYOUT ((caddr_t)&mask, (caddr_t)parm3, sizeof(afs_int32), code);
  } else
      code = EINVAL;

out:
  AFS_GUNLOCK();
#ifdef AFS_LINUX20_ENV
  return -code;
#else
  return code;
#endif
}

#ifdef AFS_AIX32_ENV

#include "sys/lockl.h"

/*
 * syscall -	this is the VRMIX system call entry point.
 *
 * NOTE:
 *	THIS SHOULD BE CHANGED TO afs_syscall(), but requires
 *	all the user-level calls to `syscall' to change.
 */
syscall(syscall, p1, p2, p3, p4, p5, p6) {
	register rval1=0, code;
	register monster;
	int retval=0;
#ifndef AFS_AIX41_ENV
	extern lock_t kernel_lock;
	monster = lockl(&kernel_lock, LOCK_SHORT);
#endif /* !AFS_AIX41_ENV */

	AFS_STATCNT(syscall);
	setuerror(0);
	switch (syscall) {
	    case AFSCALL_CALL:
		rval1 = afs_syscall_call(p1, p2, p3, p4, p5, p6);
		break;

	    case AFSCALL_SETPAG:
	        AFS_GLOCK();
		rval1 = afs_setpag();
	        AFS_GUNLOCK();
		break;

	    case AFSCALL_PIOCTL:
	        AFS_GLOCK();
		rval1 = afs_syscall_pioctl(p1, p2, p3, p4);
	        AFS_GUNLOCK();
		break;

	    case AFSCALL_ICREATE:
		rval1 = afs_syscall_icreate(p1, p2, p3, p4, p5, p6);
		break;

	    case AFSCALL_IOPEN:
		rval1 = afs_syscall_iopen(p1, p2, p3);
		break;

	    case AFSCALL_IDEC:
		rval1 = afs_syscall_iincdec(p1, p2, p3, -1);
		break;

	    case AFSCALL_IINC:
		rval1 = afs_syscall_iincdec(p1, p2, p3, 1);
		break;

	    case AFSCALL_ICL:
	        AFS_GLOCK();
		code = Afscall_icl(p1, p2, p3, p4, p5, &retval);
	        AFS_GUNLOCK();
		if (!code) rval1 = retval;
		if (!rval1) rval1 = code;
		break;

	    default:
		rval1 = EINVAL;
		setuerror(EINVAL);
		break;
	}

    out:
#ifndef AFS_AIX41_ENV
	if (monster != LOCK_NEST)
		unlockl(&kernel_lock);
#endif /* !AFS_AIX41_ENV */
	return getuerror() ? -1 : rval1;
}

/*
 * lsetpag -	interface to afs_setpag().
 */
lsetpag() {

    AFS_STATCNT(lsetpag);
    return syscall(AFSCALL_SETPAG, 0, 0, 0, 0, 0);
}

/*
 * lpioctl -	interface to pioctl()
 */
lpioctl(path, cmd, cmarg, follow)
char *path, *cmarg; {

    AFS_STATCNT(lpioctl);
    return syscall(AFSCALL_PIOCTL, path, cmd, cmarg, follow);
}

#else	/* !AFS_AIX32_ENV	*/

#if defined(AFS_SGI_ENV)
struct afsargs
{
	sysarg_t syscall;
	sysarg_t parm1;
	sysarg_t parm2;
	sysarg_t parm3;
	sysarg_t parm4;
	sysarg_t parm5;
};


int
Afs_syscall (struct afsargs *uap, rval_t *rvp)
{
    int error;
    long retval;

    AFS_STATCNT(afs_syscall);
    switch(uap->syscall) {
    case AFSCALL_ICL:
	retval = 0;
	AFS_GLOCK();
	error=Afscall_icl(uap->parm1,uap->parm2,uap->parm3,uap->parm4,uap->parm5, &retval);
	AFS_GUNLOCK();
	rvp->r_val1 = retval;
	break;
#ifdef AFS_SGI_XFS_IOPS_ENV
    case AFSCALL_IDEC64:
	error = afs_syscall_idec64(uap->parm1, uap->parm2, uap->parm3,
				   uap->parm4, uap->parm5);
	break;
    case AFSCALL_IINC64:
	error = afs_syscall_iinc64(uap->parm1, uap->parm2, uap->parm3,
				   uap->parm4, uap->parm5);
	break;
    case AFSCALL_ILISTINODE64:
	error = afs_syscall_ilistinode64(uap->parm1, uap->parm2, uap->parm3,
				   uap->parm4, uap->parm5);
	break;
    case AFSCALL_ICREATENAME64:
	error = afs_syscall_icreatename64(uap->parm1, uap->parm2, uap->parm3,
				   uap->parm4, uap->parm5);
	break;
#endif
#ifdef AFS_SGI_VNODE_GLUE
    case AFSCALL_INIT_KERNEL_CONFIG:
	error = afs_init_kernel_config(uap->parm1);
	break;
#endif
    default:
	error = afs_syscall_call(uap->syscall, uap->parm1, uap->parm2,
				 uap->parm3, uap->parm4, uap->parm5);
    }
    return error;
}

#else /* AFS_SGI_ENV */

struct iparam {
    long param1;
    long param2;
    long param3;
    long param4;
};

struct iparam32 {
    int param1;
    int param2;
    int param3;
    int param4;
};


static void
iparam32_to_iparam(const struct iparam32 *src, struct iparam *dst)
{
	dst->param1 = src->param1;
	dst->param2 = src->param2;
	dst->param3 = src->param3;
	dst->param4 = src->param4;
}

/*
 * If you need to change copyin_iparam(), you may also need to change
 * copyin_afs_ioctl().
 */

static int
copyin_iparam(caddr_t cmarg, struct iparam *dst)
{
	int code;

#if defined(AFS_HPUX_64BIT_ENV)
	struct iparam32 dst32;

	if (is_32bit(u.u_procp))	/* is_32bit() in proc_iface.h */
	{
		AFS_COPYIN(cmarg, (caddr_t) &dst32, sizeof dst32, code);
		if (!code)
			iparam32_to_iparam(&dst32, dst);
		return code;
	}
#endif /* AFS_HPUX_64BIT_ENV */

#if defined(AFS_SUN57_64BIT_ENV)
	struct iparam32 dst32;

	if (get_udatamodel() == DATAMODEL_ILP32) {
		AFS_COPYIN(cmarg, (caddr_t) &dst32, sizeof dst32, code);
		if (!code)
			iparam32_to_iparam(&dst32, dst);
		return code;
	}
#endif /* AFS_SUN57_64BIT_ENV */

#if defined(AFS_LINUX_64BIT_KERNEL)
	struct iparam32 dst32;

#ifdef AFS_SPARC64_LINUX20_ENV
	if (current->tss.flags & SPARC_FLAG_32BIT) {
#else
#error Not done for this linux version
#endif
		AFS_COPYIN(cmarg, (caddr_t) &dst32, sizeof dst32, code);
		if (!code)
			iparam32_to_iparam(&dst32, dst);
		return code;
	}
#endif /* AFS_LINUX_64BIT_KERNEL */

	AFS_COPYIN(cmarg, (caddr_t) dst, sizeof *dst, code);
	return code;
}

/* Main entry of all afs system calls */
#ifdef	AFS_SUN5_ENV
extern int afs_sinited;

/** The 32 bit OS expects the members of this structure to be 32 bit
 * quantities and the 64 bit OS expects them as 64 bit quanties. Hence
 * to accomodate both, *long* is used instead of afs_int32
 */

#ifdef AFS_SUN57_ENV
struct afssysa {
    long syscall;
    long parm1;
    long parm2;
    long parm3;
    long parm4;
    long parm5;
    long parm6;
};
#else
struct afssysa {
    afs_int32 syscall;
    afs_int32 parm1;
    afs_int32 parm2;
    afs_int32 parm3;
    afs_int32 parm4;
    afs_int32 parm5;
    afs_int32 parm6;
};
#endif

Afs_syscall (uap, rvp)
    register struct afssysa *uap;
    rval_t *rvp;
{
    int *retval = &rvp->r_val1;
#else /* AFS_SUN5_ENV */
#if	defined(AFS_OSF_ENV)
afs3_syscall(p, args, retval)
	struct proc *p;
	void *args;
	int *retval;
{
    register struct a {
	    long syscall;
	    long parm1;
	    long parm2;
	    long parm3;
	    long parm4;
	    long parm5;
	    long parm6;
	} *uap = (struct a *)args;
#else	/* AFS_OSF_ENV */
#ifdef AFS_LINUX20_ENV
struct afssysargs {
    long syscall;
    long parm1;
    long parm2;
    long parm3;
    long parm4;
    long parm5;
    long parm6; /* not actually used - should be removed */
};
/* Linux system calls only set up for 5 arguments. */
asmlinkage int afs_syscall(long syscall, long parm1, long parm2, long parm3,
			   long parm4)
{
    struct afssysargs args, *uap = &args;
    long linux_ret=0;
    long *retval = &linux_ret;
    long eparm[4]; /* matches AFSCALL_ICL in fstrace.c */
#else
#if defined(UKERNEL)
Afs_syscall ()
{
    register struct a {
	    long syscall;
	    long parm1;
	    long parm2;
	    long parm3;
	    long parm4;
	    long parm5;
	    long parm6;
	} *uap = (struct a *)u.u_ap;
#else /* UKERNEL */
#if defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
afs_syscall ()
#else
Afs_syscall ()
#endif /* SUN && !SUN5 */
{
    register struct a {
	    long syscall;
	    long parm1;
	    long parm2;
	    long parm3;
	    long parm4;
	    long parm5;
	    long parm6;
	} *uap = (struct a *)u.u_ap;
#endif /* UKERNEL */
#if  defined(AFS_DEC_ENV)
    int *retval = &u.u_r.r_val1;
#else
#if defined(AFS_HPUX_ENV)
    long *retval = &u.u_rval1;
#else
    int *retval = &u.u_rval1;
#endif
#endif
#endif /* AFS_LINUX20_ENV */
#endif /* AFS_OSF_ENV */
#endif /* AFS_SUN5_ENV */
    register int code = 0;

    AFS_STATCNT(afs_syscall);
#ifdef        AFS_SUN5_ENV
    rvp->r_vals = 0;
    if (!afs_sinited) {
	return (ENODEV);
    }
#endif
#ifdef AFS_LINUX20_ENV
    lock_kernel();
    /* setup uap for use below - pull out the magic decoder ring to know
     * which syscalls have folded argument lists.
     */
    uap->syscall = syscall;
    uap->parm1 = parm1;
    uap->parm2 = parm2;
    uap->parm3 = parm3;
    if (syscall == AFSCALL_ICL) {
	AFS_COPYIN((char*)parm4, (char*)eparm, sizeof(eparm), code);
	uap->parm4 = eparm[0];
	uap->parm5 = eparm[1];
	uap->parm6 = eparm[2];
    }
    else {
	uap->parm4 = parm4;
	uap->parm5 = 0;
	uap->parm6 = 0;
    }
#endif

#if defined(AFS_HPUX_ENV)
    /*
     * There used to be code here (duplicated from osi_Init()) for
     * initializing the semaphore used by AFS_GLOCK().  Was the
     * duplication to handle the case of a dynamically loaded kernel
     * module?
     */
    osi_InitGlock();
#endif
    if (uap->syscall == AFSCALL_CALL) {
#ifdef	AFS_SUN5_ENV
	code =  afs_syscall_call(uap->parm1, uap->parm2, uap->parm3, 
				uap->parm4, uap->parm5, uap->parm6, rvp, CRED());
#else
	code = afs_syscall_call(uap->parm1, uap->parm2, uap->parm3, uap->parm4, uap->parm5, uap->parm6);
#endif
    } else if (uap->syscall == AFSCALL_SETPAG) {
#ifdef	AFS_SUN5_ENV
	struct cred *cred;
	register proc_t *procp;

	procp = ttoproc(curthread);
	mutex_enter(&procp->p_crlock);
	cred = procp->p_cred; 
	AFS_GLOCK();
	code =  afs_setpag(&cred);
	AFS_GUNLOCK();
	procp->p_cred = cred;
	mutex_exit(&procp->p_crlock);
#else
	AFS_GLOCK();
#if	defined(AFS_OSF_ENV)
	code = afs_setpag(p, args, retval);
#else	/* AFS_OSF_ENV */
	code = afs_setpag();
#endif
	AFS_GUNLOCK();
#endif
    } else if (uap->syscall == AFSCALL_PIOCTL) {
	AFS_GLOCK();
#ifdef	AFS_SUN5_ENV
        code = afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3, uap->parm4, rvp, CRED());
#else
	code = afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3, uap->parm4);
#endif
	AFS_GUNLOCK();
    } else if (uap->syscall == AFSCALL_ICREATE) {
	struct iparam iparams;

	code = copyin_iparam((char *)uap->parm3, &iparams);
	if (code) {
#if !defined(AFS_SUN5_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_LINUX20_ENV)
	    setuerror(code);
#endif
	} else
#ifdef	AFS_SUN5_ENV
	code =  afs_syscall_icreate(uap->parm1, uap->parm2, iparams.param1, iparams.param2, 
				   iparams.param3, iparams.param4, rvp, CRED());
#else
	code =  afs_syscall_icreate(uap->parm1, uap->parm2, iparams.param1, iparams.param2,
#ifdef	AFS_OSF_ENV
				   iparams.param3, iparams.param4, retval);
#else
				   iparams.param3, iparams.param4);
#endif
#endif	/* AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_IOPEN) {
#ifdef	AFS_SUN5_ENV
	code = afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3, rvp, CRED());
#else
#ifdef	AFS_OSF_ENV
	code = afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3, retval);
#else
	code = afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3);
#endif
#endif	/* AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_IDEC) {
#ifdef	AFS_SUN5_ENV
	code = afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, -1, rvp, CRED());
#else
	code = afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, -1);
#endif	/* AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_IINC) {
#ifdef	AFS_SUN5_ENV
	code = afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, 1, rvp, CRED());
#else
	code = afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, 1);
#endif	/* AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_ICL) {
	AFS_GLOCK();
	code = Afscall_icl(uap->parm1, uap->parm2, uap->parm3, uap->parm4, uap->parm5, retval);
	AFS_GUNLOCK();
#ifdef AFS_LINUX20_ENV
	if (!code) {
	    /* ICL commands can return values. */
	    code = -linux_ret; /* Gets negated again at exit below */
	}
#else
        if (code) {
#if !defined(AFS_SUN5_ENV) && !defined(AFS_OSF_ENV)
	    setuerror(code);
#endif
        }
#endif /* !AFS_LINUX20_ENV */
    } else {
#if defined(AFS_SUN5_ENV) || defined(AFS_OSF_ENV) || defined(AFS_LINUX20_ENV)
	code = EINVAL;
#else
        setuerror(EINVAL);
#endif	/* AFS_SUN5_ENV */
    }
out:
#ifdef AFS_LINUX20_ENV
    code = -code;
    unlock_kernel();
#endif
    return code;
}
#endif /* AFS_SGI_ENV */
#endif	/* !AFS_AIX32_ENV	*/

/*
 * Initstate in the range 0 < x < 100 are early initialization states.
 * Initstate of 100 means a AFSOP_START operation has been done.  After this,
 *  the cache may be initialized.
 * Initstate of 101 means a AFSOP_GO operation has been done.  This operation
 *  is done after all the cache initialization has been done.
 * Initstate of 200 means that the volume has been looked up once, possibly
 *  incorrectly.
 * Initstate of 300 means that the volume has been *successfully* looked up.
 */
afs_CheckInit() {
    register int code = 0;

    AFS_STATCNT(afs_CheckInit);
    if (afs_initState <= 100)
	code =  ENXIO;   /* never finished init phase */
    else if (afs_initState == 101) {	/* init done, wait for afs_daemon */
	while (afs_initState < 200) afs_osi_Sleep(&afs_initState);
    } else  if (afs_initState == 200) 
	code =  ETIMEDOUT; /* didn't find root volume */
    return code;
}

int afs_shuttingdown = 0; 
void
afs_shutdown()
{
    extern short afs_brsDaemons;
    extern afs_int32 afs_CheckServerDaemonStarted;
    extern struct afs_osi_WaitHandle AFS_WaitHandler, AFS_CSWaitHandler;
    extern struct osi_file *afs_cacheInodep;

    AFS_STATCNT(afs_shutdown);
    if (afs_shuttingdown) return;
    afs_shuttingdown = 1;
    if (afs_cold_shutdown) afs_warn("COLD ");
    else afs_warn("WARM ");
    afs_warn("shutting down of: CB... "); 

    afs_termState = AFSOP_STOP_RXCALLBACK;
    rx_WakeupServerProcs();
    /* shutdown_rxkernel(); */
    while (afs_termState == AFSOP_STOP_RXCALLBACK)
	afs_osi_Sleep(&afs_termState);

    afs_warn("afs... ");
    while (afs_termState == AFSOP_STOP_AFS) {
	afs_osi_CancelWait(&AFS_WaitHandler);
	afs_osi_Sleep(&afs_termState);
    }
    if (afs_CheckServerDaemonStarted) {
	while (afs_termState == AFSOP_STOP_CS) {
	    afs_osi_CancelWait(&AFS_CSWaitHandler);
	    afs_osi_Sleep(&afs_termState);
	}
    }
    afs_warn("BkG... ");
    /* Wake-up afs_brsDaemons so that we don't have to wait for a bkg job! */
    while (afs_termState == AFSOP_STOP_BKG) {
	afs_osi_Wakeup(&afs_brsDaemons);
	afs_osi_Sleep(&afs_termState);
    }
    afs_warn("CTrunc... ");
    /* Cancel cache truncate daemon. */
    while (afs_termState == AFSOP_STOP_TRUNCDAEMON) {
	afs_osi_Wakeup((char*)&afs_CacheTruncateDaemon);
	afs_osi_Sleep(&afs_termState);
    }
#if	defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV)
    afs_warn("RxEvent... ");
    /* cancel rx event deamon */
    while (afs_termState == AFSOP_STOP_RXEVENT) 
	afs_osi_Sleep(&afs_termState);
#if defined(RXK_LISTENER_ENV)
    afs_warn("RxListener... ");
    /* cancel rx listener */
    osi_StopListener(); /* This closes rx_socket. */
    while (afs_termState == AFSOP_STOP_RXK_LISTENER) 
	afs_osi_Sleep(&afs_termState);
#endif
#else
    afs_termState =  AFSOP_STOP_COMPLETE;
#endif
    afs_warn("\n");

    /* Close file only after daemons which can write to it are stopped. */
    if (afs_cacheInodep)	/* memcache won't set this */
    {
	osi_UFSClose(afs_cacheInodep);    /* Since we always leave it open */
	afs_cacheInodep = 0;
    }
    return;	/* Just kill daemons for now */
#ifdef notdef
    shutdown_CB();  
    shutdown_AFS();
    shutdown_rxkernel();
    shutdown_rxevent(); 
    shutdown_rx();
    afs_shutdown_BKG(); 
    shutdown_bufferpackage();
    shutdown_daemons();
    shutdown_cache();
    shutdown_osi();
    shutdown_osinet();
    shutdown_osifile();
    shutdown_vnodeops();
    shutdown_vfsops();
    shutdown_exporter();
    shutdown_memcache();
#if !defined(AFS_NONFSTRANS) || defined(AFS_AIX_IAUTH_ENV)
#if !defined(AFS_DEC_ENV) && !defined(AFS_OSF_ENV)
    /* this routine does not exist in Ultrix systems... 93.01.19 */
    shutdown_nfsclnt();
#endif /* AFS_DEC_ENV */
#endif
    shutdown_afstest();
    /* The following hold the cm stats */
/*
    bzero(&afs_cmstats, sizeof(struct afs_CMStats));
    bzero(&afs_stats_cmperf, sizeof(struct afs_stats_CMPerf));
    bzero(&afs_stats_cmfullperf, sizeof(struct afs_stats_CMFullPerf));
*/
    afs_warn(" ALL allocated tables\n");
    afs_shuttingdown = 0;
#endif
}

shutdown_afstest()
{
    AFS_STATCNT(shutdown_afstest);
    afs_initState = afs_termState = afs_setTime = 0;
    AFS_Running = afs_CB_Running = 0;
    afs_CacheInit_Done = afs_Go_Done = 0;
    if (afs_cold_shutdown) {
      *afs_rootVolumeName = 0;
    }
}


/* In case there is a bunch of dynamically build bkg daemons to free */
afs_shutdown_BKG()
{ AFS_STATCNT(shutdown_BKG); }


#if defined(AFS_ALPHA_ENV) || defined(AFS_SGI61_ENV)
/* For SGI 6.2, this can is changed to 1 if it's a 32 bit kernel. */
#if defined(AFS_SGI62_ENV) && defined(KERNEL) && !defined(_K64U64)
int afs_icl_sizeofLong = 1;
#else
int afs_icl_sizeofLong = 2;
#endif /* SGI62 */
#else
int afs_icl_sizeofLong = 1;
#endif

int afs_icl_inited = 0;

/* init function, called once, under afs_icl_lock */
afs_icl_Init()
{
    afs_icl_inited = 1;
    return 0;
}

extern struct afs_icl_log *afs_icl_FindLog();
extern struct afs_icl_set *afs_icl_FindSet();


static int
Afscall_icl(long opcode, long p1, long p2, long p3, long p4, long *retval)
{
    register int i;
    afs_int32 *lp, elts, flags;
    register afs_int32 code;
    struct afs_icl_log *logp;
    struct afs_icl_set *setp;
#if defined(AFS_SGI61_ENV) || defined(AFS_SUN57_ENV)
    size_t temp;
#else /* AFS_SGI61_ENV */
    afs_uint32 temp;
#endif /* AFS_SGI61_ENV */
    char tname[65];
    afs_int32 startCookie;
    afs_int32 allocated;
    struct afs_icl_log *tlp;

#ifdef	AFS_SUN5_ENV
    if (!afs_suser(CRED())) {	/* only root can run this code */
	return (EACCES);
    }
#else
    if (!afs_suser()) {	/* only root can run this code */
#if !defined(AFS_SGI_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_LINUX20_ENV)
	setuerror(EACCES);
	return EACCES;
#else
	return EPERM;
#endif
    }
#endif
    switch (opcode) {
    case ICL_OP_COPYOUTCLR:	/* copy out data then clear */
    case ICL_OP_COPYOUT:	/* copy ouy data */
	/* copyout: p1=logname, p2=&buffer, p3=size(words), p4=&cookie
	 * return flags<<24 + nwords.
	 * updates cookie to updated start (not end) if we had to
	 * skip some records.
	 */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code) return code;
	AFS_COPYIN((char *)p4, (char *)&startCookie, sizeof(afs_int32), code);
	if (code) return code;
	logp = afs_icl_FindLog(tname);
	if (!logp) return ENOENT;
#define	BUFFERSIZE	AFS_LRALLOCSIZ
	lp = (afs_int32 *) osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	elts = BUFFERSIZE / sizeof(afs_int32);
	if (p3 < elts) elts = p3;
	flags = (opcode == ICL_OP_COPYOUT) ? 0 : ICL_COPYOUTF_CLRAFTERREAD;
	code = afs_icl_CopyOut(logp, lp, &elts, (afs_uint32 *) &startCookie,
			   &flags);
	if (code) {
	    osi_FreeLargeSpace((struct osi_buffer *) lp);
	    break;
	}
	AFS_COPYOUT((char *)lp, (char *)p2, elts * sizeof(afs_int32), code);
	if (code) goto done;
	AFS_COPYOUT((char *) &startCookie, (char *)p4, sizeof(afs_int32), code);
	if (code) goto done;
	*retval = (flags<<24) | (elts & 0xffffff);
      done:
	afs_icl_LogRele(logp);
	osi_FreeLargeSpace((struct osi_buffer *) lp);
	break;

    case ICL_OP_ENUMLOGS:	/* enumerate logs */
	/* enumerate logs: p1=index, p2=&name, p3=sizeof(name), p4=&size.
	 * return 0 for success, otherwise error.
	 */
	for(tlp = afs_icl_allLogs; tlp; tlp=tlp->nextp) {
	    if (p1-- == 0) break;
	}
	if (!tlp) return ENOENT;     /* past the end of file */
	temp = strlen(tlp->name)+1;
	if (temp > p3) return EINVAL;
	AFS_COPYOUT(tlp->name, (char *) p2, temp, code);
	if (!code)	/* copy out size of log */
	    AFS_COPYOUT((char *)&tlp->logSize, (char *)p4, sizeof (afs_int32), code);
	break;

    case ICL_OP_ENUMLOGSBYSET:	/* enumerate logs by set name */
	/* enumerate logs: p1=setname, p2=index, p3=&name, p4=sizeof(name).
	 * return 0 for success, otherwise error.
	 */
	AFS_COPYINSTR((char *)p1, tname, sizeof (tname), &temp, code);
	if (code) return code;
	setp = afs_icl_FindSet(tname);
	if (!setp) return ENOENT;
	if (p2 > ICL_LOGSPERSET)
	    return EINVAL;
	if (!(tlp = setp->logs[p2]))
	    return EBADF;
	temp = strlen(tlp->name)+1;
	if (temp > p4) return EINVAL;
	AFS_COPYOUT(tlp->name, (char *)p3, temp, code);
	break;

    case ICL_OP_CLRLOG:		/* clear specified log */
	/* zero out the specified log: p1=logname */
	AFS_COPYINSTR((char *)p1, tname, sizeof (tname), &temp, code);
	if (code) return code;
	logp = afs_icl_FindLog(tname);
	if (!logp) return ENOENT;
	code = afs_icl_ZeroLog(logp);
	afs_icl_LogRele(logp);
	break;

    case ICL_OP_CLRSET:		/* clear specified set */
	/* zero out the specified set: p1=setname */
	AFS_COPYINSTR((char *)p1, tname, sizeof (tname), &temp, code);
	if (code) return code;
	setp = afs_icl_FindSet(tname);
	if (!setp) return ENOENT;
	code = afs_icl_ZeroSet(setp);
	afs_icl_SetRele(setp);
	break;

    case ICL_OP_CLRALL:		/* clear all logs */
	/* zero out all logs -- no args */
	code = 0;
	ObtainWriteLock(&afs_icl_lock,178);
	for(tlp = afs_icl_allLogs; tlp; tlp=tlp->nextp) {
	    tlp->refCount++;	/* hold this guy */
	    ReleaseWriteLock(&afs_icl_lock);
	    /* don't clear persistent logs */
	    if ((tlp->states & ICL_LOGF_PERSISTENT) == 0)
		code = afs_icl_ZeroLog(tlp);
	    ObtainWriteLock(&afs_icl_lock,179);
	    if (--tlp->refCount == 0)
		afs_icl_ZapLog(tlp);
	    if (code) break;
	}
	ReleaseWriteLock(&afs_icl_lock);
	break;

    case ICL_OP_ENUMSETS:	/* enumerate all sets */
	/* enumerate sets: p1=index, p2=&name, p3=sizeof(name), p4=&states.
	 * return 0 for success, otherwise error.
	 */
	for(setp = afs_icl_allSets; setp; setp = setp->nextp) {
	    if (p1-- == 0) break;
	}
	if (!setp) return ENOENT;	/* past the end of file */
	temp = strlen(setp->name)+1;
	if (temp > p3) return EINVAL;
	AFS_COPYOUT(setp->name, (char *)p2, temp, code);
	if (!code)	/* copy out size of log */
	    AFS_COPYOUT((char *)&setp->states,(char *)p4, sizeof (afs_int32), code);
	break;

    case ICL_OP_SETSTAT:	/* set status on a set */
	/* activate the specified set: p1=setname, p2=op */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code) return code;
	setp = afs_icl_FindSet(tname);
	if (!setp) return ENOENT;
	code = afs_icl_SetSetStat(setp, p2);
	afs_icl_SetRele(setp);
	break;

    case ICL_OP_SETSTATALL:	/* set status on all sets */
	/* activate the specified set: p1=op */
	code = 0;
	ObtainWriteLock(&afs_icl_lock,180);
	for(setp = afs_icl_allSets; setp; setp=setp->nextp) {
	    setp->refCount++;	/* hold this guy */
	    ReleaseWriteLock(&afs_icl_lock);
	    /* don't set states on persistent sets */
	    if ((setp->states & ICL_SETF_PERSISTENT) == 0)
		code = afs_icl_SetSetStat(setp, p1);
	    ObtainWriteLock(&afs_icl_lock,181);
	    if (--setp->refCount == 0)
		afs_icl_ZapSet(setp);
	    if (code) break;
	}
	ReleaseWriteLock(&afs_icl_lock);
	break;

    case ICL_OP_SETLOGSIZE:		/* set size of log */
	/* set the size of the specified log: p1=logname, p2=size (in words) */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code) return code;
	logp = afs_icl_FindLog(tname);
	if (!logp) return ENOENT;
	code = afs_icl_LogSetSize(logp, p2);
	afs_icl_LogRele(logp);
	break;

    case ICL_OP_GETLOGINFO:		/* get size of log */
	/* zero out the specified log: p1=logname, p2=&logSize, p3=&allocated */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code) return code;
	logp = afs_icl_FindLog(tname);
	if (!logp) return ENOENT;
	allocated = !!logp->datap;
	AFS_COPYOUT((char *)&logp->logSize, (char *) p2, sizeof(afs_int32), code);
	if (!code)
	    AFS_COPYOUT((char *)&allocated, (char *) p3, sizeof(afs_int32), code);
	afs_icl_LogRele(logp);
	break;

    case ICL_OP_GETSETINFO:		/* get state of set */
	/* zero out the specified set: p1=setname, p2=&state */
	AFS_COPYINSTR((char *)p1, tname, sizeof(tname), &temp, code);
	if (code) return code;
	setp = afs_icl_FindSet(tname);
	if (!setp) return ENOENT;
	AFS_COPYOUT((char *)&setp->states, (char *) p2, sizeof(afs_int32), code);
	afs_icl_SetRele(setp);
	break;

    default:
	code = EINVAL;
    }

    return code;
}


afs_lock_t afs_icl_lock;

/* exported routine: a 4 parameter event */
afs_icl_Event4(setp, eventID, lAndT, p1, p2, p3, p4)
  register struct afs_icl_set *setp;
  afs_int32 eventID;
  afs_int32 lAndT;
  long p1, p2, p3, p4;
{
    register struct afs_icl_log *logp;
    afs_int32 mask;
    register int i;
    register afs_int32 tmask;
    int ix;

    /* If things aren't init'ed yet (or the set is inactive), don't panic */
    if (!ICL_SETACTIVE(setp)) return;

    AFS_ASSERT_GLOCK();
    mask = lAndT>>24 & 0xff;	/* mask of which logs to log to */
    ix = ICL_EVENTBYTE(eventID);
    ObtainReadLock(&setp->lock);
    if (setp->eventFlags[ix] & ICL_EVENTMASK(eventID)) {
	for(i=0, tmask = 1; i<ICL_LOGSPERSET; i++, tmask <<= 1) {
	    if (mask & tmask) {
		afs_icl_AppendRecord(setp->logs[i], eventID, lAndT & 0xffffff,
			      p1, p2, p3, p4);
	    }
	    mask &= ~tmask;
	    if (mask == 0) break;	/* break early */
	}
    }
    ReleaseReadLock(&setp->lock);
}

/* Next 4 routines should be implemented via var-args or something.
 * Whole purpose is to avoid compiler warnings about parameter # mismatches.
 * Otherwise, could call afs_icl_Event4 directly.
 */
afs_icl_Event3(setp, eventID, lAndT, p1, p2, p3)
  register struct afs_icl_set *setp;
  afs_int32 eventID;
  afs_int32 lAndT;
  long p1, p2, p3;
{
    return afs_icl_Event4(setp, eventID, lAndT, p1, p2, p3, (long)0);
}

afs_icl_Event2(setp, eventID, lAndT, p1, p2)
  register struct afs_icl_set *setp;
  afs_int32 eventID;
  afs_int32 lAndT;
  long p1, p2;
{
    return afs_icl_Event4(setp, eventID, lAndT, p1, p2, (long)0, (long)0);
}

afs_icl_Event1(setp, eventID, lAndT, p1)
  register struct afs_icl_set *setp;
  afs_int32 eventID;
  afs_int32 lAndT;
  long p1;
{
    return afs_icl_Event4(setp, eventID, lAndT, p1, (long)0, (long)0, (long)0);
}

afs_icl_Event0(setp, eventID, lAndT)
  register struct afs_icl_set *setp;
  afs_int32 eventID;
  afs_int32 lAndT;
{
    return afs_icl_Event4(setp, eventID, lAndT, (long)0, (long)0, (long)0, (long)0);
}

struct afs_icl_log *afs_icl_allLogs = 0;

/* function to purge records from the start of the log, until there
 * is at least minSpace long's worth of space available without
 * making the head and the tail point to the same word.
 *
 * Log must be write-locked.
 */
static afs_icl_GetLogSpace(logp, minSpace)
  register struct afs_icl_log *logp;
  afs_int32 minSpace;
{
    register unsigned int tsize;

    while (logp->logSize - logp->logElements <= minSpace) {
	/* eat a record */
	tsize = ((logp->datap[logp->firstUsed]) >> 24) & 0xff;
	logp->logElements -= tsize;
	logp->firstUsed += tsize;
	if (logp->firstUsed >= logp->logSize)
	    logp->firstUsed -= logp->logSize;
	logp->baseCookie += tsize;
    }
}

/* append string astr to buffer, including terminating null char.
 *
 * log must be write-locked.
 */
#define ICL_CHARSPERLONG	4
static afs_int32 afs_icl_AppendString(logp, astr)
  struct afs_icl_log *logp;
  char *astr;
{
    char *op;		/* ptr to char to write */
    int tc;
    register int bib;	/* bytes in buffer */

    bib = 0;
    op = (char *) &(logp->datap[logp->firstFree]);
    while (1) {
	tc = *astr++;
	*op++ = tc;
	if (++bib >= ICL_CHARSPERLONG) {
	    /* new word */
	    bib = 0;
	    if (++(logp->firstFree) >= logp->logSize) {
		logp->firstFree = 0;
		op = (char *) &(logp->datap[0]);
	    }
	    logp->logElements++;
	}
	if (tc == 0) break;
    }
    if (bib > 0) {
	/* if we've used this word at all, allocate it */
	if (++(logp->firstFree) >= logp->logSize) {
	    logp->firstFree = 0;
	}
	logp->logElements++;
    }
}

/* add a long to the log, ignoring overflow (checked already) */
#if defined(AFS_ALPHA_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64))
#define ICL_APPENDINT32(lp, x) \
    MACRO_BEGIN \
        (lp)->datap[(lp)->firstFree] = (x); \
	if (++((lp)->firstFree) >= (lp)->logSize) { \
		(lp)->firstFree = 0; \
	} \
        (lp)->logElements++; \
    MACRO_END

#define ICL_APPENDLONG(lp, x) \
    MACRO_BEGIN \
	ICL_APPENDINT32((lp), ((x) >> 32) & 0xffffffffL); \
	ICL_APPENDINT32((lp), (x) & 0xffffffffL); \
    MACRO_END

#else /* AFS_ALPHA_ENV */
#define ICL_APPENDLONG(lp, x) \
    MACRO_BEGIN \
        (lp)->datap[(lp)->firstFree] = (x); \
	if (++((lp)->firstFree) >= (lp)->logSize) { \
		(lp)->firstFree = 0; \
	} \
        (lp)->logElements++; \
    MACRO_END
#define ICL_APPENDINT32(lp, x) ICL_APPENDLONG((lp), (x))
#endif /* AFS_ALPHA_ENV */

/* routine to tell whether we're dealing with the address or the
 * object itself
 */
afs_icl_UseAddr(type)
  int type;
{
    if (type == ICL_TYPE_HYPER || type == ICL_TYPE_STRING
	|| type == ICL_TYPE_FID)
	return 1;
    else
	return 0;
}

/* Function to append a record to the log.  Written for speed
 * since we know that we're going to have to make this work fast
 * pretty soon, anyway.  The log must be unlocked.
 */

afs_icl_AppendRecord(logp, op, types, p1, p2, p3, p4)
  register struct afs_icl_log *logp;
  afs_int32 op;
  afs_int32 types;
  long p1, p2, p3, p4;
{
    int rsize;			/* record size in longs */
    register int tsize;		/* temp size */
    osi_timeval_t tv;
    int t1, t2, t3, t4;

    t4 = types & 0x3f;		/* decode types */
    types >>= 6;
    t3 = types & 0x3f;
    types >>= 6;
    t2 = types & 0x3f;
    types >>= 6;
    t1 = types & 0x3f;

    osi_GetTime(&tv);		/* It panics for solaris if inside */
    ObtainWriteLock(&logp->lock,182);
    if (!logp->datap) {
	ReleaseWriteLock(&logp->lock);
	return;
    }

    /* get timestamp as # of microseconds since some time that doesn't
     * change that often.  This algorithm ticks over every 20 minutes
     * or so (1000 seconds).  Write a timestamp record if it has.
     */
    if (tv.tv_sec - logp->lastTS > 1024)
    {
	/* the timer has wrapped -- write a timestamp record */
	if (logp->logSize - logp->logElements <= 5)
	    afs_icl_GetLogSpace(logp, 5);

	ICL_APPENDINT32(logp, (afs_int32)(5<<24) + (ICL_TYPE_UNIXDATE<<18));
	ICL_APPENDINT32(logp, (afs_int32)ICL_INFO_TIMESTAMP);
	ICL_APPENDINT32(logp, (afs_int32)0); /* use thread ID zero for clocks */
	ICL_APPENDINT32(logp,
			(afs_int32)(tv.tv_sec & 0x3ff) * 1000000 + tv.tv_usec);
	ICL_APPENDINT32(logp, (afs_int32)tv.tv_sec);

	logp->lastTS = tv.tv_sec;
    }

    rsize = 4;				/* base case */
    if (t1) {
	/* compute size of parameter p1.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t1, p1);
    }
    if (t2) {
	/* compute size of parameter p2.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t2, p2);
    }
    if (t3) {
	/* compute size of parameter p3.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t3, p3);
    }
    if (t4) {
	/* compute size of parameter p4.  Only tricky case is string.
	 * In that case, we have to call strlen to get the string length.
	 */
	ICL_SIZEHACK(t4, p4);
    }

    /* At this point, we've computed all of the parameter sizes, and
     * have in rsize the size of the entire record we want to append.
     * Next, we check that we actually have room in the log to do this
     * work, and then we do the append.
     */
    if (rsize > 255) {
	ReleaseWriteLock(&logp->lock);
	return;		/* log record too big to express */
    }

    if (logp->logSize - logp->logElements <= rsize)
	afs_icl_GetLogSpace(logp, rsize);

    ICL_APPENDINT32(logp,
		    (afs_int32)(rsize<<24) + (t1<<18) + (t2<<12) + (t3<<6) + t4);
    ICL_APPENDINT32(logp, (afs_int32)op);
    ICL_APPENDINT32(logp, (afs_int32)osi_ThreadUnique());
    ICL_APPENDINT32(logp, (afs_int32)(tv.tv_sec & 0x3ff) * 1000000 + tv.tv_usec);

    if (t1) {
	/* marshall parameter 1 now */
	if (t1 == ICL_TYPE_STRING) {
	    afs_icl_AppendString(logp, (char *) p1);
	}
	else if (t1 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p1)->high);
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p1)->low);
	}
	else if (t1 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p1)[0]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p1)[1]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p1)[2]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p1)[3]);
	}
#if defined(AFS_ALPHA_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64))
	else if (t1 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32)p1);
#endif /* AFS_ALPHA_ENV */
	else ICL_APPENDLONG(logp, p1); 
    }
    if (t2) {
	/* marshall parameter 2 now */
	if (t2 == ICL_TYPE_STRING) afs_icl_AppendString(logp, (char *) p2);
	else if (t2 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p2)->high);
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p2)->low);
	}
	else if (t2 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p2)[0]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p2)[1]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p2)[2]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p2)[3]);
	}
#if defined(AFS_ALPHA_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64))
	else if (t2 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32)p2);
#endif /* AFS_ALPHA_ENV */
	else ICL_APPENDLONG(logp, p2);
    }
    if (t3) {
	/* marshall parameter 3 now */
	if (t3 == ICL_TYPE_STRING) afs_icl_AppendString(logp, (char *) p3);
	else if (t3 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p3)->high);
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p3)->low);
	}
	else if (t3 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p3)[0]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p3)[1]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p3)[2]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p3)[3]);
	}
#if defined(AFS_ALPHA_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64))
	else if (t3 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32)p3);
#endif /* AFS_ALPHA_ENV */
	else ICL_APPENDLONG(logp, p3);
    }
    if (t4) {
	/* marshall parameter 4 now */
	if (t4 == ICL_TYPE_STRING) afs_icl_AppendString(logp, (char *) p4);
	else if (t4 == ICL_TYPE_HYPER) {
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p4)->high);
	    ICL_APPENDINT32(logp, (afs_int32)((struct afs_hyper_t *)p4)->low);
	}
	else if (t4 == ICL_TYPE_FID) {
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p4)[0]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p4)[1]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p4)[2]);
	    ICL_APPENDINT32(logp, (afs_int32)((afs_int32 *)p4)[3]);
	}
#if defined(AFS_ALPHA_ENV) || (defined(AFS_SGI61_ENV) && (_MIPS_SZLONG==64))
	else if (t4 == ICL_TYPE_INT32)
	    ICL_APPENDINT32(logp, (afs_int32)p4);
#endif /* AFS_ALPHA_ENV */
	else ICL_APPENDLONG(logp, p4);
    }
    ReleaseWriteLock(&logp->lock);
}

/* create a log with size logSize; return it in *outLogpp and tag
 * it with name "name."
 */
afs_icl_CreateLog(name, logSize, outLogpp)
  char *name;
  afs_int32 logSize;
  struct afs_icl_log **outLogpp;
{
    return afs_icl_CreateLogWithFlags(name, logSize, /*flags*/0, outLogpp);
}

/* create a log with size logSize; return it in *outLogpp and tag
 * it with name "name."  'flags' can be set to make the log unclearable.
 */
afs_icl_CreateLogWithFlags(name, logSize, flags, outLogpp)
  char *name;
  afs_int32 logSize;
  afs_uint32 flags;
  struct afs_icl_log **outLogpp;
{
    register struct afs_icl_log *logp;

    /* add into global list under lock */
    ObtainWriteLock(&afs_icl_lock,183);
    if (!afs_icl_inited) afs_icl_Init();

    for (logp = afs_icl_allLogs; logp; logp=logp->nextp) {
	if (strcmp(logp->name, name) == 0) {
	    /* found it already created, just return it */
	    logp->refCount++;
	    *outLogpp = logp;
	    if (flags & ICL_CRLOG_FLAG_PERSISTENT)
	    {
		ObtainWriteLock(&logp->lock,184);
		logp->states |= ICL_LOGF_PERSISTENT;
		ReleaseWriteLock(&logp->lock);
	    }
	    ReleaseWriteLock(&afs_icl_lock);
	    return 0;
	}
    }
    
    logp = (struct afs_icl_log *)
	osi_AllocSmallSpace(sizeof(struct afs_icl_log));
    bzero((caddr_t)logp, sizeof(*logp));

    logp->refCount = 1;
    logp->name = osi_AllocSmallSpace(strlen(name)+1);
    strcpy(logp->name, name);
    LOCK_INIT(&logp->lock, "logp lock");
    logp->logSize = logSize;
    logp->datap = (afs_int32 *)0;	/* don't allocate it until we need it */

    if (flags & ICL_CRLOG_FLAG_PERSISTENT)
	logp->states |= ICL_LOGF_PERSISTENT;

    logp->nextp = afs_icl_allLogs;
    afs_icl_allLogs = logp;
    ReleaseWriteLock(&afs_icl_lock);

    *outLogpp = logp;
    return 0;
}

/* called with a log, a pointer to a buffer, the size of the buffer
 * (in *bufSizep), the starting cookie (in *cookiep, use 0 at the start)
 * and returns data in the provided buffer, and returns output flags
 * in *flagsp.  The flag ICL_COPYOUTF_MISSEDSOME is set if we can't
 * find the record with cookie value cookie.
 */
afs_icl_CopyOut(logp, bufferp, bufSizep, cookiep, flagsp)
  register struct afs_icl_log *logp;
  afs_int32 *bufferp;
  afs_int32 *bufSizep;
  afs_uint32 *cookiep;
  afs_int32 *flagsp;
{
    afs_int32 nwords;		/* number of words to copy out */
    afs_uint32 startCookie;	/* first cookie to use */
    register afs_int32 i;
    afs_int32 outWords;		/* words we've copied out */
    afs_int32 inWords;		/* max words to copy out */
    afs_int32 code;			/* return code */
    afs_int32 ix;			/* index we're copying from */
    afs_int32 outFlags;		/* return flags */
    afs_int32 inFlags;		/* flags passed in */
    afs_int32 end;

    inWords = *bufSizep;	/* max to copy out */
    outWords = 0;		/* amount copied out */
    startCookie = *cookiep;
    outFlags = 0;
    inFlags = *flagsp;
    code = 0;

    ObtainWriteLock(&logp->lock,185);
    if (!logp->datap) {
	ReleaseWriteLock(&logp->lock);
	goto done;
    }

    /* first, compute the index of the start cookie we've been passed */
    while (1) {
	/* (re-)compute where we should start */
	if (startCookie < logp->baseCookie) {
	    if (startCookie)  /* missed some output */
		outFlags |= ICL_COPYOUTF_MISSEDSOME;
	    /* skip to the first available record */
	    startCookie = logp->baseCookie;
	    *cookiep = startCookie;
	}

	/* compute where we find the first element to copy out */
	ix = logp->firstUsed + startCookie - logp->baseCookie;
	if (ix >= logp->logSize) ix -= logp->logSize;

	/* if have some data now, break out and process it */
	if (startCookie - logp->baseCookie < logp->logElements) break;

	/* At end of log, so clear it if we need to */
	if (inFlags & ICL_COPYOUTF_CLRAFTERREAD)
	{
	    logp->firstUsed = logp->firstFree = 0;
	    logp->logElements = 0;
	}
	/* otherwise, either wait for the data to arrive, or return */
	if (!(inFlags & ICL_COPYOUTF_WAITIO)) {
	    ReleaseWriteLock(&logp->lock);
	    code = 0;
	    goto done;
	}
	logp->states |= ICL_LOGF_WAITING;
	ReleaseWriteLock(&logp->lock);
	afs_osi_Sleep(&logp->lock);
	ObtainWriteLock(&logp->lock,186);
    }
    /* copy out data from ix to logSize or firstFree, depending
     * upon whether firstUsed <= firstFree (no wrap) or otherwise.
     * be careful not to copy out more than nwords.
     */
    if (ix >= logp->firstUsed) {
	if (logp->firstUsed <= logp->firstFree)
	    /* no wrapping */
	    end = logp->firstFree;	/* first element not to copy */
	else
	    end = logp->logSize;
	nwords = inWords;	/* don't copy more than this */
	if (end - ix < nwords)
	    nwords = end - ix;
	if (nwords > 0) {
	    bcopy((char *) &logp->datap[ix], (char *) bufferp,
		  sizeof(afs_int32) * nwords);
	    outWords += nwords;
	    inWords -= nwords;
	    bufferp += nwords;
	}
	/* if we're going to copy more out below, we'll start here */
	ix = 0;
    }
    /* now, if active part of the log has wrapped, there's more stuff
     * starting at the head of the log.  Copy out more from there.
     */
    if (logp->firstUsed > logp->firstFree
	&& ix < logp->firstFree && inWords > 0) {
	/* (more to) copy out from the wrapped section at the
	 * start of the log.  May get here even if didn't copy any
	 * above, if the cookie points directly into the wrapped section.
	 */
	nwords = inWords;
	if (logp->firstFree - ix < nwords)
	    nwords = logp->firstFree - ix;
	bcopy((char *) &logp->datap[ix], (char *) bufferp,
	      sizeof(afs_int32) * nwords);
	outWords += nwords;
	inWords -= nwords;
	bufferp += nwords;
    }

    ReleaseWriteLock(&logp->lock);

  done:
    if (code == 0) {
	*bufSizep = outWords;
	*flagsp = outFlags;
    }
    return code;
}

/* return basic parameter information about a log */
afs_icl_GetLogParms(logp, maxSizep, curSizep)
  struct afs_icl_log *logp;
  afs_int32 *maxSizep;
  afs_int32 *curSizep;
{
    ObtainReadLock(&logp->lock);
    *maxSizep = logp->logSize;
    *curSizep = logp->logElements;
    ReleaseReadLock(&logp->lock);
    return 0;
}


/* hold and release logs */
afs_icl_LogHold(logp)
  register struct afs_icl_log *logp;
{
    ObtainWriteLock(&afs_icl_lock,187);
    logp->refCount++;
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* hold and release logs, called with lock already held */
afs_icl_LogHoldNL(logp)
  register struct afs_icl_log *logp;
{
    logp->refCount++;
    return 0;
}

/* keep track of how many sets believe the log itself is allocated */
afs_icl_LogUse(logp)
  register struct afs_icl_log *logp;
{
    ObtainWriteLock(&logp->lock,188);
    if (logp->setCount == 0) {
	/* this is the first set actually using the log -- allocate it */
	if (logp->logSize == 0)	{
	    /* we weren't passed in a hint and it wasn't set */
	    logp->logSize = ICL_DEFAULT_LOGSIZE;
	}
	logp->datap = (afs_int32 *) afs_osi_Alloc(sizeof(afs_int32) * logp->logSize);
#ifdef	AFS_AIX32_ENV
	pin((char *)logp->datap, sizeof(afs_int32) * logp->logSize);
#endif
    }
    logp->setCount++;
    ReleaseWriteLock(&logp->lock);
    return 0;
}

/* decrement the number of real users of the log, free if possible */
afs_icl_LogFreeUse(logp)
  register struct afs_icl_log *logp;
{
    ObtainWriteLock(&logp->lock,189);
    if (--logp->setCount == 0) {
	/* no more users -- free it (but keep log structure around)*/
	afs_osi_Free(logp->datap, sizeof(afs_int32) * logp->logSize);
#ifdef	AFS_AIX32_ENV
	unpin((char *)logp->datap, sizeof(afs_int32) * logp->logSize);
#endif
	logp->firstUsed = logp->firstFree = 0;
	logp->logElements = 0;
	logp->datap = (afs_int32 *)0;
    }
    ReleaseWriteLock(&logp->lock);
    return 0;
}

/* set the size of the log to 'logSize' */
afs_icl_LogSetSize(logp, logSize)
  register struct afs_icl_log *logp;
  afs_int32 logSize;
{  
    ObtainWriteLock(&logp->lock,190);
    if (!logp->datap) {
	/* nothing to worry about since it's not allocated */
	logp->logSize = logSize;
    }
    else {
	/* reset log */
	logp->firstUsed = logp->firstFree = 0;
	logp->logElements = 0;

	/* free and allocate a new one */
	afs_osi_Free(logp->datap, sizeof(afs_int32) * logp->logSize);
#ifdef	AFS_AIX32_ENV
	unpin((char *)logp->datap, sizeof(afs_int32) * logp->logSize);
#endif
	logp->datap = (afs_int32 *) afs_osi_Alloc(sizeof(afs_int32) * logSize);
#ifdef	AFS_AIX32_ENV
	pin((char *)logp->datap, sizeof(afs_int32) * logSize);
#endif
	logp->logSize = logSize;
    }
    ReleaseWriteLock(&logp->lock);

    return 0;
}

/* free a log.  Called with afs_icl_lock locked. */
afs_icl_ZapLog(logp)
  register struct afs_icl_log *logp;
{
    register struct afs_icl_log **lpp, *tp;

    for(lpp = &afs_icl_allLogs, tp = *lpp; tp; lpp = &tp->nextp, tp = *lpp) {
	if (tp == logp) {
	    /* found the dude we want to remove */
	    *lpp = logp->nextp;
	    osi_FreeSmallSpace(logp->name);
	    osi_FreeSmallSpace(logp->datap);
	    osi_FreeSmallSpace(logp);
	    break;	/* won't find it twice */
	}
    }
    return 0;
}

/* do the release, watching for deleted entries */
afs_icl_LogRele(logp)
  register struct afs_icl_log *logp;
{
    ObtainWriteLock(&afs_icl_lock,191);
    if (--logp->refCount == 0 && (logp->states & ICL_LOGF_DELETED)) {
	afs_icl_ZapLog(logp);	/* destroys logp's lock! */
    }
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* do the release, watching for deleted entries, log already held */
afs_icl_LogReleNL(logp)
  register struct afs_icl_log *logp;
{
    if (--logp->refCount == 0 && (logp->states & ICL_LOGF_DELETED)) {
	afs_icl_ZapLog(logp);	/* destroys logp's lock! */
    }
    return 0;
}

/* zero out the log */
afs_icl_ZeroLog(logp)
  register struct afs_icl_log *logp;
{
    ObtainWriteLock(&logp->lock,192);
    logp->firstUsed = logp->firstFree = 0;
    logp->logElements = 0;
    logp->baseCookie = 0;
    ReleaseWriteLock(&logp->lock);
    return 0;
}

/* free a log entry, and drop its reference count */
afs_icl_LogFree(logp)
  register struct afs_icl_log *logp;
{
    ObtainWriteLock(&logp->lock,193);
    logp->states |= ICL_LOGF_DELETED;
    ReleaseWriteLock(&logp->lock);
    afs_icl_LogRele(logp);
    return 0;
}

/* find a log by name, returning it held */
struct afs_icl_log *afs_icl_FindLog(name)
  char *name;
{
    register struct afs_icl_log *tp;
    ObtainWriteLock(&afs_icl_lock,194);
    for(tp = afs_icl_allLogs; tp; tp=tp->nextp) {
	if (strcmp(tp->name, name) == 0) {
	    /* this is the dude we want */
	    tp->refCount++;
	    break;
	}
    }
    ReleaseWriteLock(&afs_icl_lock);
    return tp;
}

afs_icl_EnumerateLogs(aproc, arock)
  int (*aproc)();
  char *arock;
{
    register struct afs_icl_log *tp;
    register afs_int32 code;

    code = 0;
    ObtainWriteLock(&afs_icl_lock,195);
    for(tp = afs_icl_allLogs; tp; tp=tp->nextp) {
	tp->refCount++;	/* hold this guy */
	ReleaseWriteLock(&afs_icl_lock);
	ObtainReadLock(&tp->lock);
	code = (*aproc)(tp->name, arock, tp);
	ReleaseReadLock(&tp->lock);
	ObtainWriteLock(&afs_icl_lock,196);
	if (--tp->refCount == 0)
	    afs_icl_ZapLog(tp);
	if (code) break;
    }
    ReleaseWriteLock(&afs_icl_lock);
    return code;
}

struct afs_icl_set *afs_icl_allSets = 0;

afs_icl_CreateSet(name, baseLogp, fatalLogp, outSetpp)
  char *name;
  struct afs_icl_log *baseLogp;
  struct afs_icl_log *fatalLogp;
  struct afs_icl_set **outSetpp;
{
    return afs_icl_CreateSetWithFlags(name, baseLogp, fatalLogp,
				      /*flags*/0, outSetpp);
}

/* create a set, given pointers to base and fatal logs, if any.
 * Logs are unlocked, but referenced, and *outSetpp is returned
 * referenced.  Function bumps reference count on logs, since it
 * addds references from the new afs_icl_set.  When the set is destroyed,
 * those references will be released.
 */
afs_icl_CreateSetWithFlags(name, baseLogp, fatalLogp, flags, outSetpp)
  char *name;
  struct afs_icl_log *baseLogp;
  struct afs_icl_log *fatalLogp;
  afs_uint32  flags;
  struct afs_icl_set **outSetpp;
{
    register struct afs_icl_set *setp;
    register int i;
    afs_int32 states = ICL_DEFAULT_SET_STATES;

    ObtainWriteLock(&afs_icl_lock,197);
    if (!afs_icl_inited) afs_icl_Init();

    for (setp = afs_icl_allSets; setp; setp = setp->nextp) {
	if (strcmp(setp->name, name) == 0) {
	    setp->refCount++;
	    *outSetpp = setp;
	    if (flags & ICL_CRSET_FLAG_PERSISTENT)
	    {
		ObtainWriteLock(&setp->lock,198);
		setp->states |= ICL_SETF_PERSISTENT;
		ReleaseWriteLock(&setp->lock);
	    }
	    ReleaseWriteLock(&afs_icl_lock);
	    return 0;
	}
    }
    
    /* determine initial state */
    if (flags & ICL_CRSET_FLAG_DEFAULT_ON)
	states = ICL_SETF_ACTIVE;
    else if (flags & ICL_CRSET_FLAG_DEFAULT_OFF)
	states = ICL_SETF_FREED;
    if (flags & ICL_CRSET_FLAG_PERSISTENT)
	states |= ICL_SETF_PERSISTENT;

    setp = (struct afs_icl_set *) afs_osi_Alloc(sizeof(struct afs_icl_set));
    bzero((caddr_t)setp, sizeof(*setp));
    setp->refCount = 1;
    if (states & ICL_SETF_FREED)
	states &= ~ICL_SETF_ACTIVE;	/* if freed, can't be active */
    setp->states = states;

    LOCK_INIT(&setp->lock, "setp lock");
    /* next lock is obtained in wrong order, hierarchy-wise, but
     * it doesn't matter, since no one can find this lock yet, since
     * the afs_icl_lock is still held, and thus the obtain can't block.
     */
    ObtainWriteLock(&setp->lock,199);
    setp->name = osi_AllocSmallSpace(strlen(name)+1);
    strcpy(setp->name, name);
    setp->nevents = ICL_DEFAULTEVENTS;
    setp->eventFlags = afs_osi_Alloc(ICL_DEFAULTEVENTS);
#ifdef	AFS_AIX32_ENV
    pin((char *)setp->eventFlags, ICL_DEFAULTEVENTS);
#endif
    for(i=0; i<ICL_DEFAULTEVENTS; i++)
	setp->eventFlags[i] = 0xff;	/* default to enabled */

    /* update this global info under the afs_icl_lock */
    setp->nextp = afs_icl_allSets;
    afs_icl_allSets = setp;
    ReleaseWriteLock(&afs_icl_lock);

    /* set's basic lock is still held, so we can finish init */
    if (baseLogp) {
	setp->logs[0] = baseLogp;
	afs_icl_LogHold(baseLogp);
	if (!(setp->states & ICL_SETF_FREED))
	    afs_icl_LogUse(baseLogp);	/* log is actually being used */
    }
    if (fatalLogp) {
	setp->logs[1] = fatalLogp;
	afs_icl_LogHold(fatalLogp);
	if (!(setp->states & ICL_SETF_FREED))
	    afs_icl_LogUse(fatalLogp);	/* log is actually being used */
    }
    ReleaseWriteLock(&setp->lock);

    *outSetpp = setp;
    return 0;
}

/* function to change event enabling information for a particular set */
afs_icl_SetEnable(setp, eventID, setValue)
  struct afs_icl_set *setp;
  afs_int32 eventID;
  int setValue;
{
    char *tp;

    ObtainWriteLock(&setp->lock,200);
    if (!ICL_EVENTOK(setp, eventID)) {
	ReleaseWriteLock(&setp->lock);
	return -1;
    }
    tp = &setp->eventFlags[ICL_EVENTBYTE(eventID)];
    if (setValue)
	*tp |= ICL_EVENTMASK(eventID);
    else
	*tp &= ~(ICL_EVENTMASK(eventID));
    ReleaseWriteLock(&setp->lock);
    return 0;
}

/* return indication of whether a particular event ID is enabled
 * for tracing.  If *getValuep is set to 0, the event is disabled,
 * otherwise it is enabled.  All events start out enabled by default.
 */
afs_icl_GetEnable(setp, eventID, getValuep)
  struct afs_icl_set *setp;
  afs_int32 eventID;
  int *getValuep;
{
    ObtainReadLock(&setp->lock);
    if (!ICL_EVENTOK(setp, eventID)) {
	ReleaseWriteLock(&setp->lock);
	return -1;
    }
    if (setp->eventFlags[ICL_EVENTBYTE(eventID)] & ICL_EVENTMASK(eventID))
	*getValuep = 1;
    else
	*getValuep = 0;
    ReleaseReadLock(&setp->lock);
    return 0;
}

/* hold and release event sets */
afs_icl_SetHold(setp)
  register struct afs_icl_set *setp;
{
    ObtainWriteLock(&afs_icl_lock,201);
    setp->refCount++;
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* free a set.  Called with afs_icl_lock locked */
afs_icl_ZapSet(setp)
  register struct afs_icl_set *setp;
{
    register struct afs_icl_set **lpp, *tp;
    int i;
    register struct afs_icl_log *tlp;

    for(lpp = &afs_icl_allSets, tp = *lpp; tp; lpp = &tp->nextp, tp = *lpp) {
	if (tp == setp) {
	    /* found the dude we want to remove */
	    *lpp = setp->nextp;
	    osi_FreeSmallSpace(setp->name);
	    afs_osi_Free(setp->eventFlags, ICL_DEFAULTEVENTS);
#ifdef	AFS_AIX32_ENV
	    unpin((char *)setp->eventFlags, ICL_DEFAULTEVENTS);
#endif
	    for(i=0; i < ICL_LOGSPERSET; i++) {
		if (tlp = setp->logs[i])
		    afs_icl_LogReleNL(tlp);
	    }
	    osi_FreeSmallSpace(setp);
	    break;	/* won't find it twice */
	}
    }
    return 0;
}

/* do the release, watching for deleted entries */
afs_icl_SetRele(setp)
  register struct afs_icl_set *setp;
{
    ObtainWriteLock(&afs_icl_lock,202);
    if (--setp->refCount == 0 && (setp->states & ICL_SETF_DELETED)) {
	afs_icl_ZapSet(setp);	/* destroys setp's lock! */
    }
    ReleaseWriteLock(&afs_icl_lock);
    return 0;
}

/* free a set entry, dropping its reference count */
afs_icl_SetFree(setp)
  register struct afs_icl_set *setp;
{
    ObtainWriteLock(&setp->lock,203);
    setp->states |= ICL_SETF_DELETED;
    ReleaseWriteLock(&setp->lock);
    afs_icl_SetRele(setp);
    return 0;
}

/* find a set by name, returning it held */
struct afs_icl_set *afs_icl_FindSet(name)
  char *name;
{
    register struct afs_icl_set *tp;
    ObtainWriteLock(&afs_icl_lock,204);
    for(tp = afs_icl_allSets; tp; tp=tp->nextp) {
	if (strcmp(tp->name, name) == 0) {
	    /* this is the dude we want */
	    tp->refCount++;
	    break;
	}
    }
    ReleaseWriteLock(&afs_icl_lock);
    return tp;
}

/* zero out all the logs in the set */
afs_icl_ZeroSet(setp)
   struct afs_icl_set *setp;
{
    register int i;
    int code = 0;
    int tcode;
    struct afs_icl_log *logp;
    
    ObtainReadLock(&setp->lock);
    for(i = 0; i < ICL_LOGSPERSET; i++) {
	logp = setp->logs[i];
	if (logp) {
	    afs_icl_LogHold(logp);
	    tcode = afs_icl_ZeroLog(logp);
	    if (tcode != 0) code = tcode;	/* save the last bad one */
	    afs_icl_LogRele(logp);
	}
    }
    ReleaseReadLock(&setp->lock);
    return code;
}

afs_icl_EnumerateSets(aproc, arock)
  int (*aproc)();
  char *arock;
{
    register struct afs_icl_set *tp, *np;
    register afs_int32 code;

    code = 0;
    ObtainWriteLock(&afs_icl_lock,205);
    for(tp = afs_icl_allSets; tp; tp=np) {
	tp->refCount++;	/* hold this guy */
	ReleaseWriteLock(&afs_icl_lock);
	code = (*aproc)(tp->name, arock, tp);
	ObtainWriteLock(&afs_icl_lock,206);
	np = tp->nextp;	/* tp may disappear next, but not np */
	if (--tp->refCount == 0 && (tp->states & ICL_SETF_DELETED))
	    afs_icl_ZapSet(tp);
	if (code) break;
    }
    ReleaseWriteLock(&afs_icl_lock);
    return code;
}

afs_icl_AddLogToSet(setp, newlogp)
  struct afs_icl_set *setp;
  struct afs_icl_log *newlogp;
{
    register int i;
    int code = -1;
    struct afs_icl_log *logp;
    
    ObtainWriteLock(&setp->lock,207);
    for(i = 0; i < ICL_LOGSPERSET; i++) {
	if (!setp->logs[i]) {
	    setp->logs[i] = newlogp;
	    code = i;
	    afs_icl_LogHold(newlogp);
	    if (!(setp->states & ICL_SETF_FREED)) {
		/* bump up the number of sets using the log */
		afs_icl_LogUse(newlogp);
	    }
	    break;
	}
    }
    ReleaseWriteLock(&setp->lock);
    return code;
}

afs_icl_SetSetStat(setp, op)
  struct afs_icl_set *setp;
  int op;
{
    int i;
    afs_int32 code;
    struct afs_icl_log *logp;

    ObtainWriteLock(&setp->lock,208);
    switch(op) {
    case ICL_OP_SS_ACTIVATE:	/* activate a log */
	/*
	 * If we are not already active, see if we have released
	 * our demand that the log be allocated (FREED set).  If
	 * we have, reassert our desire.
	 */
	if (!(setp->states & ICL_SETF_ACTIVE)) {
	    if (setp->states & ICL_SETF_FREED) {
		/* have to reassert desire for logs */
		for(i = 0; i < ICL_LOGSPERSET; i++) {
		    logp = setp->logs[i];
		    if (logp) {
			afs_icl_LogHold(logp);
			afs_icl_LogUse(logp);
			afs_icl_LogRele(logp);
		    }
		}
		setp->states &= ~ICL_SETF_FREED;
	    }
	    setp->states |= ICL_SETF_ACTIVE;
	}
	code = 0;
	break;

    case ICL_OP_SS_DEACTIVATE:	/* deactivate a log */
	/* this doesn't require anything beyond clearing the ACTIVE flag */
	setp->states &= ~ICL_SETF_ACTIVE;
	code = 0;
	break;

    case ICL_OP_SS_FREE:	/* deassert design for log */
	/* 
	 * if we are already in this state, do nothing; otherwise
	 * deassert desire for log
	 */
	if (setp->states & ICL_SETF_ACTIVE)
	    code = EINVAL;
	else {
	    if (!(setp->states & ICL_SETF_FREED)) {
		for(i = 0; i < ICL_LOGSPERSET; i++) {
		    logp = setp->logs[i];
		    if (logp) {
			afs_icl_LogHold(logp);
			afs_icl_LogFreeUse(logp);
			afs_icl_LogRele(logp);
		    }
		}
		setp->states |= ICL_SETF_FREED;
	    }
	    code = 0;
	}
	break;

    default:
	code = EINVAL;
    }
    ReleaseWriteLock(&setp->lock);
    return code;
}

