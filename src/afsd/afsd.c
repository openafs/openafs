/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
  *	    Information Technology Center
  *	    October 1987
  *
  * Description:
  * 	The Andrew File System startup process.  It is responsible for
  * reading and parsing the various configuration files, starting up
  * the kernel processes required by AFS and feeding the configuration
  * information to the kernel.
  *
  * Recognized flags are:
  *	-blocks	    The number of blocks available in the workstation cache.
  *	-files	    The target number of files in the workstation cache (Default:
  *		    1000).
  *	-rootvol    The name of the root volume to use.
  *	-stat	    The number of stat cache entries.
  *	-hosts	    [OBSOLETE] List of servers to check for volume location info FOR THE
  *		    HOME CELL.
  *     -memcache   Use an in-memory cache rather than disk.
  *	-cachedir   The base directory for the workstation cache.
  *	-mountdir   The directory on which the AFS is to be mounted.
  *	-confdir    The configuration directory.
  *	-nosettime  Don't keep checking the time to avoid drift (default).
  *     -settime    [IGNORED] Keep checking the time to avoid drift.
  *	-rxmaxmtu   Set the max mtu to help with VPN issues.
  *	-verbose    Be chatty.
  *	-disable-dynamic-vcaches     Disable the use of -stat value as the starting size of
  *                          the size of the vcache/stat cache pool,
  *                          but increase that pool dynamically as needed.
  *	-debug	   Print out additional debugging info.
  *	-kerndev    [OBSOLETE] The kernel device for AFS.
  *	-dontfork   [OBSOLETE] Don't fork off as a new process.
  *	-daemons   The number of background daemons to start (Default: 2).
  *	-rmtsys	   Also fires up an afs remote sys call (e.g. pioctl, setpag)
  *                support daemon
  *     -chunksize [n]   2^n is the chunksize to be used.  0 is default.
  *     -dcache    The number of data cache entries.
  *     -volumes    The number of volume entries.
  *     -biods     Number of bkg I/O daemons (AIX3.1 only)
  *	-prealloc  Number of preallocated "small" memory blocks
  *	-logfile    [IGNORED] Place where to put the logfile (default in
  *                <cache>/etc/AFSLog.
  *	-waitclose make close calls always synchronous (slows em down, tho)
  *	-files_per_subdir [n]	number of files per cache subdir. (def=2048)
  *	-shutdown  Shutdown afs daemons
  *	-enable_peer_stats	Collect RPC statistics by peer.
  *	-enable_process_stats	Collect RPC statistics for this process.
  *	-mem_alloc_sleep [IGNORED] Sleep when allocating memory.
  *	-afsdb	    Enable AFSDB support.
  *	-dynroot	Enable dynroot support.
  *	-dynroot-sparse	Enable dynroot support with minimal cell list.
  *	-fakestat	Enable fake stat() for cross-cell mounts.
  *	-fakestat-all	Enable fake stat() for all mounts.
  *	-nomount    Do not mount /afs.
  *	-backuptree Prefer backup volumes for mountpoints in backup volumes.
  *	-rxbind	    Bind the rx socket.
  *	-rxpck	    Value for rx_extraPackets.
  *	-splitcache RW/RO ratio for cache.
  *	-rxmaxfrags Max number of UDP fragments per rx packet.
  *	-inumcalc  inode number calculation method; 0=compat, 1=MD5 digest
  *	-volume-ttl vldb cache timeout in seconds
  *---------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>
/* darwin dirent.h doesn't give us the prototypes we want if KERNEL is
 * defined, and roken includes dirent */
#if defined(UKERNEL) && defined(AFS_USR_DARWIN_ENV)
# undef KERNEL
# include <roken.h>
# define KERNEL
#else
# include <roken.h>
#endif

#define VFS 1

#include <afs/stds.h>
#include <afs/opr.h>
#include <afs/opr_assert.h>

#include <afs/cmd.h>

#include "afsd.h"

#include <afs/afsutil.h>

#include <sys/file.h>
#include <sys/wait.h>
#include <hcrypto/rand.h>

/* darwin dirent.h doesn't give us the prototypes we want if KERNEL is
 * defined */
#if defined(UKERNEL) && defined(AFS_USR_DARWIN_ENV)
# undef KERNEL
# include <dirent.h>
# define KERNEL
#else
# include <dirent.h>
#endif

#ifdef HAVE_SYS_FS_TYPES_H
#include <sys/fs_types.h>
#endif

#if defined(HAVE_SYS_MOUNT_H) && !defined(AFS_ARM_DARWIN_ENV)
#include <sys/mount.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

#ifdef HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif

#ifdef HAVE_SYS_MNTENT_H
#include <sys/mntent.h>
#endif

#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_FSTYP_H
#include <sys/fstyp.h>
#endif

#include <ctype.h>

#include <afs/afs_args.h>
#include <afs/cellconfig.h>
#include <afs/afssyscalls.h>
#include <afs/afsutil.h>
#include <afs/sys_prototypes.h>

#ifdef AFS_SGI_ENV
#include <sched.h>
#endif

#ifdef AFS_DARWIN_ENV
#ifdef AFS_DARWIN80_ENV
#include <sys/xattr.h>
#endif
#include <CoreFoundation/CoreFoundation.h>

static int event_pid;
#if !defined(AFS_ARM_DARWIN_ENV) && !defined(AFS_ARM64_DARWIN_ENV)
# define MACOS_EVENT_HANDLING 1
#endif
#endif /* AFS_DARWIN_ENV */

#if AFS_HAVE_STATVFS || defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
#else
#if defined(AFS_SUN_ENV)
#include <sys/vfs.h>
#else
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#endif
#endif

#undef	VIRTUE
#undef	VICE

#ifdef AFS_SOCKPROXY_ENV
# include <sys/types.h>
# include <sys/socket.h>
#endif

#define CACHEINFOFILE   "cacheinfo"
#define	DCACHEFILE	"CacheItems"
#define	VOLINFOFILE	"VolumeItems"
#define CELLINFOFILE	"CellItems"

#define MAXIPADDRS 1024

char LclCellName[64];

#define MAX_CACHE_LOOPS 4


/*
 * Internet address (old style... should be updated).  This was pulled out of the old 4.2
 * version of <netinet/in.h>, since it's still useful.
 */
struct in_addr_42 {
    union {
	struct {
	    u_char s_b1, s_b2, s_b3, s_b4;
	} S_un_b;
	struct {
	    u_short s_w1, s_w2;
	} S_un_w;
	afs_uint32 S_addr;
    } S_un;
};

#define	mPrintIPAddr(ipaddr)  printf("[%d.%d.%d.%d] ",		\
      ((struct in_addr_42 *) &(ipaddr))->S_un.S_un_b.s_b1,	\
      ((struct in_addr_42 *) &(ipaddr))->S_un.S_un_b.s_b2,	\
      ((struct in_addr_42 *) &(ipaddr))->S_un.S_un_b.s_b3,	\
      ((struct in_addr_42 *) &(ipaddr))->S_un.S_un_b.s_b4)

/*
 * Global configuration variables.
 */
static int enable_rxbind = 0;
static int cacheBlocks;		/*Num blocks in the cache */
static int cacheFiles;		/*Optimal # of files in workstation cache */
static int rwpct = 0;
static int ropct = 0;
static int cacheStatEntries;	/*Number of stat cache entries */
static char *cacheBaseDir;	/*Where the workstation AFS cache lives */
static char *confDir;		/*Where the workstation AFS configuration lives */
static char fullpn_DCacheFile[1024];	/*Full pathname of DCACHEFILE */
static char fullpn_VolInfoFile[1024];	/*Full pathname of VOLINFOFILE */
static char fullpn_CellInfoFile[1024];	/*Full pathanem of CELLINFOFILE */
static char fullpn_CacheInfo[1024];	/*Full pathname of CACHEINFO */
static char fullpn_VFile[1024];	/*Full pathname of data cache files */
static char *vFilePtr;			/*Ptr to the number part of above pathname */
static int sawCacheMountDir = 0;	/* from cmd line */
static int sawCacheBaseDir = 0;
static int sawCacheBlocks = 0;
static int sawDCacheSize = 0;
#ifdef AFS_AIX32_ENV
static int sawBiod = 0;
#endif
static int sawCacheStatEntries = 0;
char *afsd_cacheMountDir;
static char *rootVolume = NULL;
#ifdef AFS_XBSD_ENV
static int createAndTrunc = O_RDWR | O_CREAT | O_TRUNC;	/*Create & truncate on open */
#else
static int createAndTrunc = O_CREAT | O_TRUNC;	/*Create & truncate on open */
#endif
static int ownerRWmode = 0600;		/*Read/write OK by owner */
static int filesSet = 0;	/*True if number of files explicitly set */
static int nFilesPerDir = 2048;	/* # files per cache dir */
#if defined(AFS_CACHE_BYPASS)
#define AFSD_NDAEMONS 4
#else
#define AFSD_NDAEMONS 2
#endif
static int nDaemons = AFSD_NDAEMONS;	/* Number of background daemons */
static int chunkSize = 0;	/* 2^chunkSize bytes per chunk */
static int dCacheSize;		/* # of dcache entries */
static int vCacheSize = 200;	/* # of volume cache entries */
static int rootVolSet = 0;	/*True if root volume name explicitly set */
int addrNum;			/*Cell server address index being printed */
static int cacheFlags = 0;	/*Flags to cache manager */
#ifdef AFS_AIX32_ENV
static int nBiods = 5;		/* AIX3.1 only */
#endif
static int preallocs = 400;	/* Def # of allocated memory blocks */
static int enable_peer_stats = 0;	/* enable rx stats */
static int enable_process_stats = 0;	/* enable rx stats */
static int enable_afsdb = 0;	/* enable AFSDB support */
static int enable_dynroot = 0;	/* enable dynroot support */
static int enable_fakestat = 0;	/* enable fakestat support */
static int enable_backuptree = 0;	/* enable backup tree support */
static int enable_nomount = 0;	/* do not mount */
static int enable_splitcache = 0;
static char *inumcalc = NULL;        /* inode number calculation method */
static int afsd_dynamic_vcaches = 0;	/* Enable dynamic-vcache support */
int afsd_verbose = 0;		/*Are we being chatty? */
int afsd_debug = 0;		/*Are we printing debugging info? */
static int afsd_CloseSynch = 0;	/*Are closes synchronous or not? */
static int rxmaxmtu = 0;       /* Are we forcing a limit on the mtu? */
static int rxmaxfrags = 0;      /* Are we forcing a limit on frags? */
static int volume_ttl = 0;      /* enable vldb cache timeout support */

#ifdef AFS_SGI_ENV
#define AFSD_INO_T ino64_t
#else
#define AFSD_INO_T afs_uint32
#endif
struct afsd_file_list {
    int fileNum;
    struct afsd_file_list *next;
};
struct afsd_file_list **cache_dir_filelist = NULL;
int *cache_dir_list = NULL;	/* Array of cache subdirs */
int *dir_for_V = NULL;		/* Array: dir of each cache file.
				 * -1: file does not exist
				 * -2: file exists in top-level
				 * >=0: file exists in Dxxx
				 */
#if !defined(AFS_CACHE_VNODE_PATH) && !defined(AFS_LINUX_ENV)
AFSD_INO_T *inode_for_V;	/* Array of inodes for desired
				 * cache files */
#endif
int missing_DCacheFile = 1;	/*Is the DCACHEFILE missing? */
int missing_VolInfoFile = 1;	/*Is the VOLINFOFILE missing? */
int missing_CellInfoFile = 1;	/*Is the CELLINFOFILE missing? */
int afsd_rmtsys = 0;		/* Default: don't support rmtsys */
struct afs_cacheParams cparams;	/* params passed to cache manager */

int PartSizeOverflow(char *path, int cs);

static int afsd_syscall(int code, ...);

#if defined(AFS_SUN510_ENV) && defined(RXK_LISTENER_ENV)
static void fork_rx_syscall_wait(const char *rn, int syscall, ...);
#endif
static void fork_rx_syscall(const char *rn, int syscall, ...);
static void fork_syscall(const char *rn, int syscall, ...);

enum optionsList {
    OPT_blocks,
    OPT_files,
    OPT_rootvol,
    OPT_stat,
    OPT_memcache,
    OPT_cachedir,
    OPT_mountdir,
    OPT_daemons,
    OPT_nosettime,
    OPT_verbose,
    OPT_rmtsys,
    OPT_debug,
    OPT_chunksize,
    OPT_dcache,
    OPT_volumes,
    OPT_biods,
    OPT_prealloc,
    OPT_confdir,
    OPT_logfile,
    OPT_waitclose,
    OPT_shutdown,
    OPT_peerstats,
    OPT_processstats,
    OPT_memallocsleep,
    OPT_afsdb,
    OPT_filesdir,
    OPT_dynroot,
    OPT_fakestat,
    OPT_fakestatall,
    OPT_nomount,
    OPT_backuptree,
    OPT_rxbind,
    OPT_settime,
    OPT_rxpck,
    OPT_splitcache,
    OPT_nodynvcache,
    OPT_rxmaxmtu,
    OPT_dynrootsparse,
    OPT_rxmaxfrags,
    OPT_inumcalc,
    OPT_volume_ttl,
};

#ifdef MACOS_EVENT_HANDLING
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStore.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

static io_connect_t root_port;
static IONotificationPortRef notify;
static io_object_t iterator;
static CFRunLoopSourceRef source;

static void
afsd_sleep_callback(void * refCon, io_service_t service,
		    natural_t messageType, void * messageArgument )
{
    switch (messageType) {
    case kIOMessageCanSystemSleep:
	/* Idle sleep is about to kick in; can
	   prevent sleep by calling IOCancelPowerChange, otherwise
	   if we don't ack in 30s the system sleeps anyway */

	/* allow it */
	IOAllowPowerChange(root_port, (long)messageArgument);
	break;

    case kIOMessageSystemWillSleep:
	/* The system WILL go to sleep. Ack or suffer delay */

	IOAllowPowerChange(root_port, (long)messageArgument);
	break;

    case kIOMessageSystemWillRestart:
	/* The system WILL restart. Ack or suffer delay */

	IOAllowPowerChange(root_port, (long)messageArgument);
	break;

    case kIOMessageSystemWillPowerOn:
    case kIOMessageSystemHasPoweredOn:
	/* coming back from sleep */

	IOAllowPowerChange(root_port, (long)messageArgument);
	break;

    default:
	IOAllowPowerChange(root_port, (long)messageArgument);
	break;
    }
}

static void
afsd_update_addresses(CFRunLoopTimerRef timer, void *info)
{
    /* parse multihomed address files */
    afs_uint32 addrbuf[MAXIPADDRS], maskbuf[MAXIPADDRS],
	mtubuf[MAXIPADDRS];
    char reason[1024];
    int code;

    code = afsconf_ParseNetFiles(addrbuf, maskbuf, mtubuf, MAXIPADDRS,
				 reason, AFSDIR_CLIENT_NETINFO_FILEPATH,
				 AFSDIR_CLIENT_NETRESTRICT_FILEPATH);

    if (code > 0) {
	/* Note we're refreshing */
	code = code | 0x40000000;
	afsd_syscall(AFSOP_ADVISEADDR, code, addrbuf, maskbuf, mtubuf);
    } else
	printf("ADVISEADDR: Error in specifying interface addresses:%s\n",
	       reason);

    /* Since it's likely this means our DNS server changed, reinit now */
    if (enable_afsdb)
	res_init();
}

/* This function is called when the system's ip addresses may have changed. */
static void
afsd_ipaddr_callback (SCDynamicStoreRef store, CFArrayRef changed_keys, void *info)
{
      CFRunLoopTimerRef timer;

      timer = CFRunLoopTimerCreate (NULL, CFAbsoluteTimeGetCurrent () + 1.0,
				    0.0, 0, 0, afsd_update_addresses, NULL);
      CFRunLoopAddTimer (CFRunLoopGetCurrent (), timer,
			 kCFRunLoopDefaultMode);
      CFRelease (timer);
}

static void
afsd_event_cleanup(int signo) {

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
    CFRelease (source);
    IODeregisterForSystemPower(&iterator);
    IOServiceClose(root_port);
    IONotificationPortDestroy(notify);
    exit(0);
}

/* Adapted from "Living in a Dynamic TCP/IP Environment" technote. */
static void
afsd_install_events(void)
{
    SCDynamicStoreContext ctx = {0};
    SCDynamicStoreRef store;

    root_port = IORegisterForSystemPower(0,&notify,afsd_sleep_callback,&iterator);

    if (root_port) {
	CFRunLoopAddSource(CFRunLoopGetCurrent(),
			   IONotificationPortGetRunLoopSource(notify),
			   kCFRunLoopDefaultMode);
    }


    store = SCDynamicStoreCreate (NULL,
				  CFSTR ("AddIPAddressListChangeCallbackSCF"),
				  afsd_ipaddr_callback, &ctx);

    if (store) {
	const void *keys[1];

	/* Request IPV4 address change notification */
	keys[0] = (SCDynamicStoreKeyCreateNetworkServiceEntity
		   (NULL, kSCDynamicStoreDomainState,
		    kSCCompAnyRegex, kSCEntNetIPv4));

	if (keys[0] != NULL) {
	    CFArrayRef pattern_array;

	    pattern_array = CFArrayCreate (NULL, keys, 1,
					   &kCFTypeArrayCallBacks);

	    if (pattern_array != NULL)
	    {
		SCDynamicStoreSetNotificationKeys (store, NULL, pattern_array);
		source = SCDynamicStoreCreateRunLoopSource (NULL, store, 0);

		CFRelease (pattern_array);
	    }

	    if (keys[0] != NULL)
		CFRelease (keys[0]);
	}

	CFRelease (store);
    }

    if (source != NULL) {
	CFRunLoopAddSource (CFRunLoopGetCurrent(),
			    source, kCFRunLoopDefaultMode);
    }

    signal(SIGTERM, afsd_event_cleanup);

    CFRunLoopRun();
}
#endif

/* ParseArgs is now obsolete, being handled by cmd */

/*------------------------------------------------------------------------------
  * ParseCacheInfoFile
  *
  * Description:
  *	Open the file containing the description of the workstation's AFS cache
  *	and pull out its contents.  The format of this file is as follows:
  *
  *	    cacheMountDir:cacheBaseDir:cacheBlocks
  *
  * Arguments:
  *	None.
  *
  * Returns:
  *	0 if everything went well,
  *	1 otherwise.
  *
  * Environment:
  *	Nothing interesting.
  *
  *  Side Effects:
  *	Sets globals.
  *---------------------------------------------------------------------------*/

int
ParseCacheInfoFile(void)
{
    static char rn[] = "ParseCacheInfoFile";	/*This routine's name */
    FILE *cachefd;		/*Descriptor for cache info file */
    int parseResult;		/*Result of our fscanf() */
    int tCacheBlocks;
    char tCacheBaseDir[1025], *tbd, tCacheMountDir[1025], *tmd;

    if (afsd_debug)
	printf("%s: Opening cache info file '%s'...\n", rn, fullpn_CacheInfo);

    cachefd = fopen(fullpn_CacheInfo, "r");
    if (!cachefd) {
	printf("%s: Can't read cache info file '%s'\n", rn, fullpn_CacheInfo);
	return (1);
    }

    /*
     * Parse the contents of the cache info file.  All chars up to the first
     * colon are the AFS mount directory, all chars to the next colon are the
     * full path name of the workstation cache directory and all remaining chars
     * represent the number of blocks in the cache.
     */
    tCacheMountDir[0] = tCacheBaseDir[0] = '\0';
    parseResult =
	fscanf(cachefd, "%1024[^:]:%1024[^:]:%d", tCacheMountDir,
	       tCacheBaseDir, &tCacheBlocks);

    /*
     * Regardless of how the parse went, we close the cache info file.
     */
    fclose(cachefd);

    if (parseResult == EOF || parseResult < 3) {
	printf("%s: Format error in cache info file!\n", rn);
	if (parseResult == EOF)
	    printf("\tEOF encountered before any field parsed.\n");
	else
	    printf("\t%d out of 3 fields successfully parsed.\n",
		   parseResult);

	return (1);
    }

    for (tmd = tCacheMountDir; *tmd == '\n' || *tmd == ' ' || *tmd == '\t';
	 tmd++);
    for (tbd = tCacheBaseDir; *tbd == '\n' || *tbd == ' ' || *tbd == '\t';
	 tbd++);
    /* now copy in the fields not explicitly overridden by cmd args */
    if (!sawCacheMountDir)
	afsd_cacheMountDir = strdup(tmd);
    if (!sawCacheBaseDir)
	cacheBaseDir = strdup(tbd);
    if (!sawCacheBlocks)
	cacheBlocks = tCacheBlocks;

    if (afsd_debug) {
	printf("%s: Cache info file successfully parsed:\n", rn);
	printf
	    ("\tcacheMountDir: '%s'\n\tcacheBaseDir: '%s'\n\tcacheBlocks: %d\n",
	     tmd, tbd, tCacheBlocks);
    }
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	return (PartSizeOverflow(tbd, cacheBlocks));
    }

    return (0);
}

/*
 * All failures to open the partition are ignored. Also if the cache dir
 * isn't a mounted partition it's also ignored since we can't guarantee
 * what will be stored afterwards. Too many if's. This is now purely
 * advisory. ODS with over 2G partition also gives warning message.
 *
 * Returns:
 *	0 if everything went well,
 *	1 otherwise.
 */
int
PartSizeOverflow(char *path, int cs)
{
  int bsize = -1;
  afs_int64 totalblks, mint;
#if AFS_HAVE_STATVFS || defined(HAVE_SYS_STATVFS_H)
    struct statvfs statbuf;

    if (statvfs(path, &statbuf) != 0) {
	if (afsd_debug)
	    printf
		("statvfs failed on %s; skip checking for adequate partition space\n",
		 path);
	return 0;
    }
    totalblks = statbuf.f_blocks;
    bsize = statbuf.f_frsize;
#if AFS_AIX51_ENV
    if (strcmp(statbuf.f_basetype, "jfs")) {
	fprintf(stderr, "Cache filesystem '%s' must be jfs (now %s)\n",
		path, statbuf.f_basetype);
	return 1;
    }
#endif /* AFS_AIX51_ENV */

#else /* AFS_HAVE_STATVFS */
    struct statfs statbuf;

    if (statfs(path, &statbuf) < 0) {
	if (afsd_debug)
	    printf
		("statfs failed on %s; skip checking for adequate partition space\n",
		 path);
	return 0;
    }
    totalblks = statbuf.f_blocks;
    bsize = statbuf.f_bsize;
#endif
    if (bsize == -1)
	return 0;		/* success */

    /* now free and totalblks are in fragment units, but we want them in 1K units */
    if (bsize >= 1024) {
	totalblks *= (bsize / 1024);
    } else {
	totalblks /= (1024 / bsize);
    }

    mint = totalblks / 100 * 95;
    if (cs > mint) {
	printf
	    ("Cache size (%d) must be less than 95%% of partition size (which is %lld). Lower cache size\n",
	     cs, mint);
	return 1;
    }

    return 0;
}

/*-----------------------------------------------------------------------------
  * GetVFileNumber
  *
  * Description:
  *	Given the final component of a filename expected to be a data cache file,
  *	return the integer corresponding to the file.  Note: we reject names that
  *	are not a ``V'' followed by an integer.  We also reject those names having
  *	the right format but lying outside the range [0..cacheFiles-1].
  *
  * Arguments:
  *	fname : Char ptr to the filename to parse.
  *	max   : integer for the highest number to accept
  *
  * Returns:
  *	>= 0 iff the file is really a data cache file numbered from 0 to cacheFiles-1, or
  *	-1      otherwise.
  *
  * Environment:
  *	Nothing interesting.
  *
  * Side Effects:
  *	None.
  *---------------------------------------------------------------------------*/

static int
doGetXFileNumber(char *fname, char filechar, int maxNum)
{
    int computedVNumber;	/*The computed file number we return */
    int filenameLen;		/*Number of chars in filename */
    int currDigit;		/*Current digit being processed */

    /*
     * The filename must have at least two characters, the first of which must be a ``filechar''
     * and the second of which cannot be a zero unless the file is exactly two chars long.
     */
    filenameLen = strlen(fname);
    if (filenameLen < 2)
	return (-1);
    if (fname[0] != filechar)
	return (-1);
    if ((filenameLen > 2) && (fname[1] == '0'))
	return (-1);

    /*
     * Scan through the characters in the given filename, failing immediately if a non-digit
     * is found.
     */
    for (currDigit = 1; currDigit < filenameLen; currDigit++)
	if (isdigit(fname[currDigit]) == 0)
	    return (-1);

    /*
     * All relevant characters are digits.  Pull out the decimal number they represent.
     * Reject it if it's out of range, otherwise return it.
     */
    computedVNumber = atoi(++fname);
    if (computedVNumber < maxNum)
	return (computedVNumber);
    else
	return (-1);
}

int
GetVFileNumber(char *fname, int maxFile)
{
    return doGetXFileNumber(fname, 'V', maxFile);
}

int
GetDDirNumber(char *fname, int maxDir)
{
    return doGetXFileNumber(fname, 'D', maxDir);
}


/*-----------------------------------------------------------------------------
  * CreateCacheFile
  *
  * Description:
  *	Given a full pathname for a file we need to create for the workstation AFS
  *	cache, go ahead and create the file.
  *
  * Arguments:
  *	fname : Full pathname of file to create.
  *	statp : A pointer to a stat buffer which, if NON-NULL, will be
  *		filled by fstat()
  *
  * Returns:
  *	0   iff the file was created,
  *	-1  otherwise.
  *
  * Environment:
  *	The given cache file has been found to be missing.
  *
  * Side Effects:
  *	As described.
  *---------------------------------------------------------------------------*/

static int
CreateCacheSubDir(char *basename, int dirNum)
{
    static char rn[] = "CreateCacheSubDir";	/* Routine Name */
    char dir[1024];
    int ret;

    /* Build the new cache subdirectory */
    sprintf(dir, "%s/D%d", basename, dirNum);

    if (afsd_debug)
	printf("%s: Creating cache subdir '%s'\n", rn, dir);

    if ((ret = mkdir(dir, 0700)) != 0) {
	printf("%s: Can't create '%s', error return is %d (%d)\n", rn, dir,
	       ret, errno);
	if (errno != EEXIST)
	    return (-1);
    }

    /* Mark this directory as created */
    cache_dir_list[dirNum] = 0;

    /* And return success */
    return (0);
}

static void
SetNoBackupAttr(char *fullpn)
{
#ifdef AFS_DARWIN80_ENV
    int ret;

    ret = setxattr(fullpn, "com.apple.metadata:com_apple_backup_excludeItem",
		   "com.apple.backupd", strlen("com.apple.backupd"), 0,
		   XATTR_CREATE);
    if(ret < 0)
    {
	if(errno != EEXIST)
	    fprintf(stderr, "afsd: Warning: failed to set attribute to preclude cache backup: %s\n", strerror(errno));
    }
#endif
    return;
}

static int
MoveCacheFile(char *basename, int fromDir, int toDir, int cacheFile,
	      int maxDir)
{
    static char rn[] = "MoveCacheFile";
    char from[1024], to[1024];
    int ret;

    if (cache_dir_list[toDir] < 0
	&& (ret = CreateCacheSubDir(basename, toDir))) {
	printf("%s: Can't create directory '%s/D%d'\n", rn, basename, toDir);
	return ret;
    }

    /* Build the from,to dir */
    if (fromDir < 0) {
	/* old-style location */
	snprintf(from, sizeof(from), "%s/V%d", basename, cacheFile);
    } else {
	snprintf(from, sizeof(from), "%s/D%d/V%d", basename, fromDir,
		 cacheFile);
    }

    snprintf(to, sizeof(from), "%s/D%d/V%d", basename, toDir, cacheFile);

    if (afsd_verbose)
	printf("%s: Moving cacheFile from '%s' to '%s'\n", rn, from, to);

    if ((ret = rename(from, to)) != 0) {
	printf("%s: Can't rename '%s' to '%s', error return is %d (%d)\n", rn,
	       from, to, ret, errno);
	return -1;
    }
    SetNoBackupAttr(to);

    /* Reset directory pointer; fix file counts */
    dir_for_V[cacheFile] = toDir;
    cache_dir_list[toDir]++;
    if (fromDir < maxDir && fromDir >= 0)
	cache_dir_list[fromDir]--;

    return 0;
}

int
CreateCacheFile(char *fname, struct stat *statp)
{
    static char rn[] = "CreateCacheFile";	/*Routine name */
    int cfd;			/*File descriptor to AFS cache file */
    int closeResult;		/*Result of close() */

    if (afsd_debug)
	printf("%s: Creating cache file '%s'\n", rn, fname);
    cfd = open(fname, createAndTrunc, ownerRWmode);
    if (cfd <= 0) {
	printf("%s: Can't create '%s', error return is %d (%d)\n", rn, fname,
	       cfd, errno);
	return (-1);
    }
    if (statp != NULL) {
	closeResult = fstat(cfd, statp);
	if (closeResult) {
	    printf
		("%s: Can't stat newly-created AFS cache file '%s' (code %d)\n",
		 rn, fname, errno);
	    return (-1);
	}
    }
    closeResult = close(cfd);
    if (closeResult) {
	printf
	    ("%s: Can't close newly-created AFS cache file '%s' (code %d)\n",
	     rn, fname, errno);
	return (-1);
    }

    return (0);
}

static void
CreateFileIfMissing(char *fullpn, int missing)
{
    if (missing) {
	if (CreateCacheFile(fullpn, NULL))
	    printf("CreateFileIfMissing: Can't create '%s'\n", fullpn);
    }
}

static void
UnlinkUnwantedFile(char *rn, char *fullpn_FileToDelete, char *fileToDelete)
{
    if (unlink(fullpn_FileToDelete)) {
	if ((errno == EISDIR || errno == EPERM) && *fileToDelete == 'D') {
	    if (rmdir(fullpn_FileToDelete)) {
		printf("%s: Can't rmdir '%s', errno is %d\n", rn,
		       fullpn_FileToDelete, errno);
	    }
	} else
	    printf("%s: Can't unlink '%s', errno is %d\n", rn,
		   fullpn_FileToDelete, errno);
    }
}

/*-----------------------------------------------------------------------------
  * SweepAFSCache
  *
  * Description:
  *	Sweep through the AFS cache directory, recording the inode number for
  *	each valid data cache file there.  Also, delete any file that doesn't belong
  *	in the cache directory during this sweep, and remember which of the other
  *	residents of this directory were seen.  After the sweep, we create any data
  *	cache files that were missing.
  *
  * Arguments:
  *	vFilesFound : Set to the number of data cache files found.
  *
  * Returns:
  *	0   if everything went well,
  *	-1 otherwise.
  *
  * Environment:
  *	This routine may be called several times.  If the number of data cache files
  *	found is less than the global cacheFiles, then the caller will need to call it
  *	again to record the inodes of the missing zero-length data cache files created
  *	in the previous call.
  *
  * Side Effects:
  *	Fills up the global inode_for_V array, may create and/or delete files as
  *	explained above.
  *---------------------------------------------------------------------------*/


static int
doSweepAFSCache(int *vFilesFound,
     	        char *directory,	/* /path/to/cache/directory */
		int dirNum,		/* current directory number */
		int maxDir)		/* maximum directory number */
{
    static char rn[] = "doSweepAFSCache";	/* Routine Name */
    char fullpn_FileToDelete[1024];	/*File to be deleted from cache */
    char *fileToDelete;		/*Ptr to last component of above */
    DIR *cdirp;			/*Ptr to cache directory structure */
#ifdef AFS_SGI_ENV
    struct dirent64 *currp;	/*Current directory entry */
#else
    struct dirent *currp;	/*Current directory entry */
#endif
    int vFileNum;		/*Data cache file's associated number */
    int thisDir;		/* A directory number */
    int highDir = 0;

    if (afsd_debug)
	printf("%s: Opening cache directory '%s'\n", rn, directory);

    if (chmod(directory, 0700)) {	/* force it to be 700 */
	printf("%s: Can't 'chmod 0700' the cache dir, '%s'.\n", rn,
	       directory);
	return (-1);
    }
    cdirp = opendir(directory);
    if (cdirp == (DIR *) 0) {
	printf("%s: Can't open AFS cache directory, '%s'.\n", rn, directory);
	return (-1);
    }

    /*
     * Scan the directory entries, remembering data cache file inodes
     * and the existance of other important residents.  Recurse into
     * the data subdirectories.
     *
     * Delete all files and directories that don't belong here.
     */
    sprintf(fullpn_FileToDelete, "%s/", directory);
    fileToDelete = fullpn_FileToDelete + strlen(fullpn_FileToDelete);

#ifdef AFS_SGI_ENV
    for (currp = readdir64(cdirp); currp; currp = readdir64(cdirp))
#else
    for (currp = readdir(cdirp); currp; currp = readdir(cdirp))
#endif
    {
	if (afsd_debug) {
	    printf("%s: Current directory entry:\n", rn);
#if defined(AFS_SGI_ENV) || defined(AFS_DARWIN90_ENV)
	    printf("\tinode=%" AFS_INT64_FMT ", reclen=%d, name='%s'\n", currp->d_ino,
		   currp->d_reclen, currp->d_name);
#elif defined(AFS_DFBSD_ENV) || defined(AFS_USR_DFBSD_ENV)
	    printf("\tinode=%ld, name='%s'\n", (long)currp->d_ino, currp->d_name);
#else
	    printf("\tinode=%ld, reclen=%d, name='%s'\n", (long)currp->d_ino,
		   currp->d_reclen, currp->d_name);
#endif
	}

	/*
	 * If dirNum < 0, we are a top-level cache directory and should
	 * only contain sub-directories and other sundry files.  Therefore,
	 * V-files are valid only if dirNum >= 0, and Directories are only
	 * valid if dirNum < 0.
	 */

	if (*(currp->d_name) == 'V'
	    && ((vFileNum = GetVFileNumber(currp->d_name, cacheFiles)) >=
		0)) {
	    /*
	     * Found a valid data cache filename.  Remember this
	     * file's inode, directory, and bump the number of files found
	     * total and in this directory.
	     */
#if !defined(AFS_CACHE_VNODE_PATH) && !defined(AFS_LINUX_ENV)
	    inode_for_V[vFileNum] = currp->d_ino;
#endif
	    dir_for_V[vFileNum] = dirNum;	/* remember this directory */

	    if (!maxDir) {
		/* If we're in a real subdir, mark this file to be moved
		 * if we've already got too many files in this directory
		 */
		assert(dirNum >= 0);
		cache_dir_list[dirNum]++;	/* keep directory's file count */
		if (cache_dir_list[dirNum] > nFilesPerDir) {
		    /* Too many files -- add to filelist */
		    struct afsd_file_list *tmp = malloc(sizeof(*tmp));
		    if (!tmp)
			printf
			    ("%s: MALLOC FAILED allocating file_list entry\n",
			     rn);
		    else {
			tmp->fileNum = vFileNum;
			tmp->next = cache_dir_filelist[dirNum];
			cache_dir_filelist[dirNum] = tmp;
		    }
		}
	    }
	    (*vFilesFound)++;
	} else if (dirNum < 0 && (*(currp->d_name) == 'D')
		   && GetDDirNumber(currp->d_name, 1 << 30) >= 0) {
	    int retval = 0;
	    if ((vFileNum = GetDDirNumber(currp->d_name, maxDir)) >= 0) {
		/* Found a valid cachefile sub-Directory.  Remember this number
		 * and recurse into it.  Note that subdirs cannot have subdirs.
		 */
		retval = 1;
	    } else if ((vFileNum = GetDDirNumber(currp->d_name, 1 << 30)) >=
		       0) {
		/* This directory is going away, but figure out if there
		 * are any cachefiles in here that should be saved by
		 * moving them to other cache directories.  This directory
		 * will be removed later.
		 */
		retval = 2;
	    }

	    /* Save the highest directory number we've seen */
	    if (vFileNum > highDir)
		highDir = vFileNum;

	    /* If this directory is staying, be sure to mark it as 'found' */
	    if (retval == 1)
		cache_dir_list[vFileNum] = 0;

	    /* Print the dirname for recursion */
	    sprintf(fileToDelete, "%s", currp->d_name);

	    /* Note: vFileNum is the directory number */
	    retval =
		doSweepAFSCache(vFilesFound, fullpn_FileToDelete, vFileNum,
				(retval == 1 ? 0 : -1));
	    if (retval) {
		printf("%s: Recursive sweep failed on directory %s\n", rn,
		       currp->d_name);
		return retval;
	    }
	} else if (dirNum < 0 && strcmp(currp->d_name, DCACHEFILE) == 0) {
	    /*
	     * Found the file holding the dcache entries.
	     */
	    missing_DCacheFile = 0;
	    SetNoBackupAttr(fullpn_DCacheFile);
	} else if (dirNum < 0 && strcmp(currp->d_name, VOLINFOFILE) == 0) {
	    /*
	     * Found the file holding the volume info.
	     */
	    missing_VolInfoFile = 0;
	    SetNoBackupAttr(fullpn_VolInfoFile);
	} else if (dirNum < 0 && strcmp(currp->d_name, CELLINFOFILE) == 0) {
	    /*
	     * Found the file holding the cell info.
	     */
	    missing_CellInfoFile = 0;
	    SetNoBackupAttr(fullpn_CellInfoFile);
	} else if ((strcmp(currp->d_name, ".") == 0)
		   || (strcmp(currp->d_name, "..") == 0) ||
#ifdef AFS_LINUX_ENV
		   /* this is the ext3 journal file */
		   (strcmp(currp->d_name, ".journal") == 0) ||
#endif
		   (strcmp(currp->d_name, "lost+found") == 0)) {
	    /*
	     * Don't do anything - this file is legit, and is to be left alone.
	     */
	} else {
	    /*
	     * This file/directory doesn't belong in the cache.  Nuke it.
	     */
	    sprintf(fileToDelete, "%s", currp->d_name);
	    if (afsd_verbose)
		printf("%s: Deleting '%s'\n", rn, fullpn_FileToDelete);
	    UnlinkUnwantedFile(rn, fullpn_FileToDelete, fileToDelete);
	}
    }

    if (dirNum < 0) {

	/*
	 * Create all the cache files that are missing.
	 */
	CreateFileIfMissing(fullpn_DCacheFile, missing_DCacheFile);
	CreateFileIfMissing(fullpn_VolInfoFile, missing_VolInfoFile);
	CreateFileIfMissing(fullpn_CellInfoFile, missing_CellInfoFile);

	/* ADJUST CACHE FILES */

	/* First, let's walk through the list of files and figure out
	 * if there are any leftover files in extra directories or
	 * missing files.  Move the former and create the latter in
	 * subdirs with extra space.
	 */

	thisDir = 0;		/* Keep track of which subdir has space */

	for (vFileNum = 0; vFileNum < cacheFiles; vFileNum++) {
	    if (dir_for_V[vFileNum] == -1) {
		/* This file does not exist.  Create it in the first
		 * subdir that still has extra space.
		 */
		while (thisDir < maxDir
		       && cache_dir_list[thisDir] >= nFilesPerDir)
		    thisDir++;
		if (thisDir >= maxDir)
		    printf("%s: can't find directory to create V%d\n", rn,
			   vFileNum);
		else {
		    struct stat statb;
#if !defined(AFS_CACHE_VNODE_PATH) && !defined(AFS_LINUX_ENV)
		    assert(inode_for_V[vFileNum] == (AFSD_INO_T) 0);
#endif
		    sprintf(vFilePtr, "D%d/V%d", thisDir, vFileNum);
		    if (afsd_verbose)
			printf("%s: Creating '%s'\n", rn, fullpn_VFile);
		    if (cache_dir_list[thisDir] < 0
			&& CreateCacheSubDir(directory, thisDir))
			printf("%s: Can't create directory for '%s'\n", rn,
			       fullpn_VFile);
		    if (CreateCacheFile(fullpn_VFile, &statb))
			printf("%s: Can't create '%s'\n", rn, fullpn_VFile);
		    else {
#if !defined(AFS_CACHE_VNODE_PATH) && !defined(AFS_LINUX_ENV)
			inode_for_V[vFileNum] = statb.st_ino;
#endif
			dir_for_V[vFileNum] = thisDir;
			cache_dir_list[thisDir]++;
			(*vFilesFound)++;
		    }
		    SetNoBackupAttr(fullpn_VFile);
		}

	    } else if (dir_for_V[vFileNum] >= maxDir
		       || dir_for_V[vFileNum] == -2) {
		/* This file needs to move; move it to the first subdir
		 * that has extra space.  (-2 means it's in the toplevel)
		 */
		while (thisDir < maxDir
		       && cache_dir_list[thisDir] >= nFilesPerDir)
		    thisDir++;
		if (thisDir >= maxDir)
		    printf("%s: can't find directory to move V%d\n", rn,
			   vFileNum);
		else {
		    if (MoveCacheFile
			(directory, dir_for_V[vFileNum], thisDir, vFileNum,
			 maxDir)) {
			/* Cannot move.  Ignore this file??? */
			/* XXX */
		    }
		}
	    }
	}			/* for */

	/* At this point, we've moved all of the valid cache files
	 * into the valid subdirs, and created all the extra
	 * cachefiles we need to create.  Next, rebalance any subdirs
	 * with too many cache files into the directories with not
	 * enough cache files.  Note that thisDir currently sits at
	 * the lowest subdir that _may_ have room.
	 */

	for (dirNum = 0; dirNum < maxDir; dirNum++) {
	    struct afsd_file_list *thisFile;

	    for (thisFile = cache_dir_filelist[dirNum];
		 thisFile && cache_dir_list[dirNum] >= nFilesPerDir;
		 thisFile = thisFile->next) {
		while (thisDir < maxDir
		       && cache_dir_list[thisDir] >= nFilesPerDir)
		    thisDir++;
		if (thisDir >= maxDir)
		    printf("%s: can't find directory to move V%d\n", rn,
			   vFileNum);
		else {
		    if (MoveCacheFile
			(directory, dirNum, thisDir, thisFile->fileNum,
			 maxDir)) {
			/* Cannot move.  Ignore this file??? */
			/* XXX */
		    }
		}
	    }			/* for each file to move */
	}			/* for each directory */

	/* Remove any directories >= maxDir -- they should be empty */
	for (; highDir >= maxDir; highDir--) {
	    sprintf(fileToDelete, "D%d", highDir);
	    UnlinkUnwantedFile(rn, fullpn_FileToDelete, fileToDelete);
	}
    }

    /* dirNum < 0 */
    /*
     * Close the directory, return success.
     */
    if (afsd_debug)
	printf("%s: Closing cache directory.\n", rn);
    closedir(cdirp);
    return (0);
}

char *
CheckCacheBaseDir(char *dir)
{
    struct stat statbuf;

    if (!dir) {
	return "cache base dir not specified";
    }
    if (stat(dir, &statbuf) != 0) {
	return "unable to stat cache base directory";
    }

    /* might want to check here for anything else goofy, like cache pointed at a non-dedicated directory, etc */

#ifdef AFS_LINUX_ENV
    {
	int res;
	struct statfs statfsbuf;

	res = statfs(dir, &statfsbuf);
	if (res != 0) {
	    return "unable to statfs cache base directory";
	}
    }
#endif

#ifdef AFS_HPUX_ENV
    {
	int res;
	struct statfs statfsbuf;
	char name[FSTYPSZ];

	res = statfs(dir, &statfsbuf);
	if (res != 0) {
	    return "unable to statfs cache base directory";
	}

	if (sysfs(GETFSTYP, statfsbuf.f_fsid[1], name) != 0) {
	    return "unable to determine filesystem type for cache base dir";
	}

	if (strcmp(name, "hfs")) {
	    return "can only use hfs filesystem for cache partition on hpux";
	}
    }
#endif

#ifdef AFS_SUN5_ENV
    {
	FILE *vfstab;
	struct mnttab mnt;
	struct stat statmnt, statci;

	if ((stat(dir, &statci) == 0)
	    && ((vfstab = fopen(MNTTAB, "r")) != NULL)) {
	    while (getmntent(vfstab, &mnt) == 0) {
		if (strcmp(dir, mnt.mnt_mountp) != 0) {
		    char *cp;
		    int rdev = 0;

		    if (cp = hasmntopt(&mnt, "dev="))
			rdev =
			    (int)strtol(cp + strlen("dev="), NULL,
					16);

		    if ((rdev == 0) && (stat(mnt.mnt_mountp, &statmnt) == 0))
			rdev = statmnt.st_dev;

		    if ((rdev == statci.st_dev)
			&& (hasmntopt(&mnt, "logging") != NULL)) {

			fclose(vfstab);
			return
			    "mounting a multi-use partition which contains the AFS cache with the\n\"logging\" option may deadlock your system.\n\n";
		    }
		}
	    }

	    fclose(vfstab);
	}
    }
#endif

    return NULL;
}

int
SweepAFSCache(int *vFilesFound)
{
    static char rn[] = "SweepAFSCache";	/*Routine name */
    int maxDir = (cacheFiles + nFilesPerDir - 1) / nFilesPerDir;
    int i;

    *vFilesFound = 0;

    if (cacheFlags & AFSCALL_INIT_MEMCACHE) {
	if (afsd_debug)
	    printf("%s: Memory Cache, no cache sweep done\n", rn);
	return 0;
    }

    if (cache_dir_list == NULL) {
	cache_dir_list = malloc(maxDir * sizeof(*cache_dir_list));
	if (cache_dir_list == NULL) {
	    printf("%s: Malloc Failed!\n", rn);
	    return (-1);
	}
	for (i = 0; i < maxDir; i++)
	    cache_dir_list[i] = -1;	/* Does not exist */
    }

    if (cache_dir_filelist == NULL) {
	cache_dir_filelist = calloc(maxDir, sizeof(*cache_dir_filelist));
	if (cache_dir_filelist == NULL) {
	    printf("%s: Malloc Failed!\n", rn);
	    return (-1);
	}
    }

    if (dir_for_V == NULL) {
	dir_for_V = malloc(cacheFiles * sizeof(*dir_for_V));
	if (dir_for_V == NULL) {
	    printf("%s: Malloc Failed!\n", rn);
	    return (-1);
	}
	for (i = 0; i < cacheFiles; i++)
	    dir_for_V[i] = -1;	/* Does not exist */
    }

    /* Note, setting dirNum to -2 here will cause cachefiles found in
     * the toplevel directory to be marked in directory "-2".  This
     * allows us to differentiate between 'file not seen' (-1) and
     * 'file seen in top-level' (-2).  Then when we try to move the
     * file into a subdirectory, we know it's in the top-level instead
     * of some other cache subdir.
     */
    return doSweepAFSCache(vFilesFound, cacheBaseDir, -2, maxDir);
}

static int
ConfigCell(struct afsconf_cell *aci, void *arock, struct afsconf_dir *adir)
{
    int isHomeCell;
    int i, code;
    int cellFlags = 0;
    afs_int32 hosts[MAXHOSTSPERCELL];

    /* figure out if this is the home cell */
    isHomeCell = (strcmp(aci->name, LclCellName) == 0);
    if (!isHomeCell) {
	cellFlags = 2;		/* not home, suid is forbidden */
	if (enable_dynroot == 2)
	    cellFlags |= 8; /* don't display foreign cells until looked up */
    }
    /* build address list */
    for (i = 0; i < MAXHOSTSPERCELL; i++)
	memcpy(&hosts[i], &aci->hostAddr[i].sin_addr, sizeof(afs_int32));

    if (aci->linkedCell)
	cellFlags |= 4;		/* Flag that linkedCell arg exists,
				 * for upwards compatibility */

    /* configure one cell */
    code = afsd_syscall(AFSOP_ADDCELL2, hosts,	/* server addresses */
			aci->name,	/* cell name */
			cellFlags,	/* is this the home cell? */
			aci->linkedCell);	/* Linked cell, if any */
    if (code)
	printf("Adding cell '%s': error %d\n", aci->name, code);
    return 0;
}

static int
ConfigCellAlias(struct afsconf_cellalias *aca,
		void *arock, struct afsconf_dir *adir)
{
    /* push the alias into the kernel */
    afsd_syscall(AFSOP_ADDCELLALIAS, aca->aliasName, aca->realName);
    return 0;
}

static void
AfsdbLookupHandler(void)
{
    afs_int32 kernelMsg[64];
    char acellName[128];
    afs_int32 code;
    struct afsconf_cell acellInfo;
    int i;

    kernelMsg[0] = 0;
    kernelMsg[1] = 0;
    acellName[0] = '\0';

#ifdef MACOS_EVENT_HANDLING
    /* Fork the event handler also. */
    code = fork();
    if (code == 0) {
	afsd_install_events();
	return;
    } else if (code != -1) {
	event_pid = code;
    }
#endif
    while (1) {
	/* On some platforms you only get 4 args to an AFS call */
	int sizeArg = ((sizeof acellName) << 16) | (sizeof kernelMsg);
	code =
	    afsd_syscall(AFSOP_AFSDB_HANDLER, acellName, kernelMsg, sizeArg);
	if (code) {		/* Something is wrong? */
	    sleep(1);
	    continue;
	}

	if (*acellName == 1)	/* Shutting down */
	    break;

	code = afsconf_GetAfsdbInfo(acellName, 0, &acellInfo);
	if (code) {
	    kernelMsg[0] = 0;
	    kernelMsg[1] = 0;
	} else {
	    kernelMsg[0] = acellInfo.numServers;
	    if (acellInfo.timeout)
		kernelMsg[1] = acellInfo.timeout - time(0);
	    else
		kernelMsg[1] = 0;
	    for (i = 0; i < acellInfo.numServers; i++)
		kernelMsg[i + 2] = acellInfo.hostAddr[i].sin_addr.s_addr;
	    strncpy(acellName, acellInfo.name, sizeof(acellName));
	    acellName[sizeof(acellName) - 1] = '\0';
	}
    }
#ifdef AFS_DARWIN_ENV
    kill(event_pid, SIGTERM);
#endif
}

#ifdef AFS_NEW_BKG
static void
BkgHandler(void)
{
    afs_int32 code;
    struct afs_uspc_param *uspc;
    char srcName[256];
    char dstName[256];

    uspc = calloc(1, sizeof(struct afs_uspc_param));
    memset(srcName, 0, sizeof(srcName));
    memset(dstName, 0, sizeof(dstName));

    /* brscount starts at 0 */
    uspc->ts = -1;

    while (1) {
	/* pushing in a buffer this large */
	uspc->bufSz = 256;

	code = afsd_syscall(AFSOP_BKG_HANDLER, uspc, srcName, dstName);
	if (code) {		/* Something is wrong? */
	    if (code == -2) {
		/*
		 * Before AFS_USPC_SHUTDOWN existed, the kernel module used to
		 * indicate it was shutting down by returning -2. Treat this
		 * like a AFS_USPC_SHUTDOWN, in case we're running with an
		 * older kernel module.
		 */
		return;
	    }

	    sleep(1);
	    uspc->retval = -1;
	    continue;
	}

	switch (uspc->reqtype) {
	case AFS_USPC_SHUTDOWN:
	    /* Client is shutting down */
	    return;

	case AFS_USPC_NOOP:
	    /* noop */
	    memset(srcName, 0, sizeof(srcName));
	    memset(dstName, 0, sizeof(dstName));
	    break;

# ifdef AFS_DARWIN_ENV
	case AFS_USPC_UMV:
            {
                pid_t child = 0;
                int status;
                char srcpath[BUFSIZ];
                char dstpath[BUFSIZ];
                snprintf(srcpath, BUFSIZ, "/afs/.:mount/%d:%d:%d:%d/%s",
                         uspc->req.umv.sCell, uspc->req.umv.sVolume,
                         uspc->req.umv.sVnode, uspc->req.umv.sUnique, srcName);
                snprintf(dstpath, BUFSIZ, "/afs/.:mount/%d:%d:%d:%d/%s",
                         uspc->req.umv.dCell, uspc->req.umv.dVolume,
                         uspc->req.umv.dVnode, uspc->req.umv.dUnique, dstName);
                if ((child = fork()) == 0) {
                    /* first child does cp; second, rm. mv would re-enter. */

                    switch (uspc->req.umv.idtype) {
                    case IDTYPE_UID:
                        if (setuid(uspc->req.umv.id) != 0) {
                            exit(-1);
                        }
                        break;
                    default:
                        exit(-1);
                        break; /* notreached */
                    }
                    execl("/bin/cp", "(afsd EXDEV helper)", "-PRp", "--", srcpath,
                          dstpath, (char *) NULL);
                }
                if (child == (pid_t) -1) {
                    uspc->retval = -1;
                    continue;
                }

                if (waitpid(child, &status, 0) == -1)
                    uspc->retval = EIO;
                else if (WIFEXITED(status) != 0 && WEXITSTATUS(status) == 0) {
                    if ((child = fork()) == 0) {
                        switch (uspc->req.umv.idtype) {
                        case IDTYPE_UID:
                            if (setuid(uspc->req.umv.id) != 0) {
                                exit(-1);
                            }
                            break;
                        default:
                            exit(-1);
                            break; /* notreached */
                        }
                        execl("/bin/rm", "(afsd EXDEV helper)", "-rf", "--",
                              srcpath, (char *) NULL);
                    }
                    if (child == (pid_t) -1) {
                        uspc->retval = -1;
                        continue;
                    }
                    if (waitpid(child, &status, 0) == -1)
                        uspc->retval = EIO;
                    else if (WIFEXITED(status) != 0) {
                        /* rm exit status */
                        uspc->retval = WEXITSTATUS(status);
                    } else {
                        /* rm signal status */
                        uspc->retval = -(WTERMSIG(status));
                    }
                } else {
                    /* error from cp: exit or signal status */
                    uspc->retval = (WIFEXITED(status) != 0) ?
                        WEXITSTATUS(status) : -(WTERMSIG(status));
                }
            }
	    memset(srcName, 0, sizeof(srcName));
	    memset(dstName, 0, sizeof(dstName));
	    break;
# endif /* AFS_DARWIN_ENV */

	default:
	    /* unknown req type */
	    uspc->retval = -1;
	    break;
	}
    }
}
#endif

#ifdef AFS_SOCKPROXY_ENV

# define AFS_SOCKPROXY_RECV_IDX	0
# define AFS_SOCKPROXY_INIT_IDX	1

/*
 * Must be less than or equal to the limits supported by libafs:
 * AFS_SOCKPROXY_PKT_MAX and AFS_SOCKPROXY_PAYLOAD_MAX.
 */
# define AFS_SOCKPROXY_PKT_ALLOC	32
# define AFS_SOCKPROXY_PAYLOAD_ALLOC	2832

/**
 * Create socket, set send and receive buffer size, and bind a name to it.
 *
 * @param[in]  a_addr  address to be assigned to the socket
 * @param[in]  a_port  port to be assigned to the socket
 * @param[out] a_sock  socket
 *
 * @return 0 on success; errno otherwise.
 */
static int
SockProxyStart(afs_int32 a_addr, afs_int32 a_port, int *a_sock)
{
    int code;
    int attempt_i;
    int blen, bsize;
    int sock;
    struct sockaddr_in ip4;

    *a_sock = -1;

    /* create an endpoint for communication */
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
	code = errno;
	goto done;
    }

    /* set options on socket */
    blen = 50000;
    bsize = sizeof(blen);
    for (attempt_i = 0; attempt_i < 2; attempt_i++) {
	code  = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &blen, bsize);
	code |= setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &blen, bsize);
	if (code == 0) {
	    break;
	}
	/* setsockopt didn't succeed. try a smaller size. */
	blen = 32766;
    }
    if (code != 0) {
	code = EINVAL;
	goto done;
    }
    /* assign addr to the socket */
    ip4.sin_family = AF_INET;
    ip4.sin_port = a_port;
    ip4.sin_addr.s_addr = a_addr;

    code = bind(sock, (struct sockaddr *)&ip4, sizeof(ip4));
    if (code != 0) {
	code = errno;
	goto done;
    }

    /* success */
    *a_sock = sock;
    sock = -1;

  done:
    if (sock >= 0) {
	close(sock);
    }
    return code;
}

/**
 * Create and initialize socket with address received from kernel-space.
 *
 * @param[in]  a_idx   index of the process responsible for this task
 * @param[out] a_sock  socket
 */
static void
SockProxyStartProc(int a_idx, int *a_sock)
{
    int code, initialized;
    struct afs_uspc_param uspc;

    initialized = 0;

    memset(&uspc, 0, sizeof(uspc));
    uspc.req.usp.idx = a_idx;

    while (!initialized) {
	uspc.reqtype = AFS_USPC_SOCKPROXY_START;

	code = afsd_syscall(AFSOP_SOCKPROXY_HANDLER, &uspc, NULL);
	if (code) {
	    /* Error; try again. */
	    uspc.retval = -1;
	    sleep(10);
	    continue;
	}

	switch (uspc.reqtype) {
	case AFS_USPC_SHUTDOWN:
	    exit(0);
	    break;

	case AFS_USPC_SOCKPROXY_START:
	    uspc.retval =
		SockProxyStart(uspc.req.usp.addr, uspc.req.usp.port, a_sock);
	    if (uspc.retval == 0) {
		/* We've setup the socket successfully. */
		initialized = 1;
	    }
	    break;

	default:
	    /* Error; try again. We must handle AFS_USPC_SOCKPROXY_START before
	     * doing anything else. */
	    uspc.retval = -1;
	    sleep(10);
	}
    }
    /* Startup is done. */
}

/**
 * Send list of packets received from kernel-space.
 *
 * @param[in]  a_sock     socket
 * @param[in]  a_pktlist  list of packets
 * @param[in]  a_npkts    number of packets
 *
 * @return number of packets successfully sent; -1 if couldn't send any packet.
 */
static int
SockProxySend(int a_sock, struct afs_pkt_hdr *a_pktlist, int a_npkts)
{
    int code;
    int pkt_i, n_sent;

    n_sent = 0;

    for (pkt_i = 0; pkt_i < a_npkts; pkt_i++) {
	struct afs_pkt_hdr *pkt = &a_pktlist[pkt_i];
	struct sockaddr_in addr;
	struct iovec iov;
	struct msghdr msg;

	memset(&iov, 0, sizeof(iov));
	memset(&msg, 0, sizeof(msg));
	memset(&addr, 0, sizeof(addr));

	addr.sin_addr.s_addr = pkt->addr;
	addr.sin_port = pkt->port;

	iov.iov_base = pkt->payload;
	iov.iov_len = pkt->size;

	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	code = sendmsg(a_sock, &msg, 0);
	if (code < 0) {
	    break;
	}
	n_sent++;
    }

    if (n_sent > 0) {
	return n_sent;
    }
    return -1;
}

/**
 * Alloc maximum number of packets, each with the largest payload possible.
 *
 * @param[out]  a_pktlist  allocated packets
 * @param[out]  a_npkts    number of packets allocated
 */
static void
pkts_alloc(struct afs_pkt_hdr **a_pktlist, int *a_npkts)
{
    int pkt_i, n_pkts;
    struct afs_pkt_hdr *pktlist;

    n_pkts = AFS_SOCKPROXY_PKT_ALLOC;
    pktlist = calloc(n_pkts, sizeof(pktlist[0]));
    opr_Assert(pktlist != NULL);

    for (pkt_i = 0; pkt_i < n_pkts; pkt_i++) {
	struct afs_pkt_hdr *pkt = &pktlist[pkt_i];
	pkt->size = AFS_SOCKPROXY_PAYLOAD_ALLOC;

	pkt->payload = calloc(1, pkt->size);
	opr_Assert(pkt->payload != NULL);
    }

    *a_pktlist = pktlist;
    *a_npkts = n_pkts;
}

/**
 * Reset pkt->size for all packets.
 *
 * @param[in]  a_pktlist  list of packets
 * @param[in]  a_npkts    number of packets
 */
static void
pkts_resetsize(struct afs_pkt_hdr *a_pktlist, int a_npkts)
{
    int pkt_i;

    for (pkt_i = 0; pkt_i < a_npkts; pkt_i++) {
	struct afs_pkt_hdr *pkt = &a_pktlist[pkt_i];
	pkt->size = AFS_SOCKPROXY_PAYLOAD_ALLOC;
    }
}

/**
 * Serve packet sending requests from kernel-space.
 *
 * @param[in]  a_sock     socket
 * @param[in]  a_idx      index of the process responsible for this task
 * @param[in]  a_recvpid  pid of process responsible for receiving packets
 *                        (used to simplify shutdown procedures)
 */
static void
SockProxySendProc(int a_sock, int a_idx, int a_recvpid)
{
    int code, n_pkts;
    struct afs_pkt_hdr *pktlist;
    struct afs_uspc_param uspc;

    n_pkts = 0;
    pktlist = NULL;
    memset(&uspc, 0, sizeof(uspc));

    pkts_alloc(&pktlist, &n_pkts);
    uspc.reqtype = AFS_USPC_SOCKPROXY_SEND;
    uspc.req.usp.idx = a_idx;

    while (1) {
	uspc.req.usp.npkts = n_pkts;
	pkts_resetsize(pktlist, n_pkts);

	code = afsd_syscall(AFSOP_SOCKPROXY_HANDLER, &uspc, pktlist);
	if (code) {
	    uspc.retval = -1;
	    sleep(10);
	    continue;
	}

	switch (uspc.reqtype) {
	case AFS_USPC_SHUTDOWN:
	    if (a_recvpid != 0) {
		/*
		 * We're shutting down, so kill the SockProxyReceiveProc
		 * process. We don't wait for that process to get its own
		 * AFS_USPC_SHUTDOWN response, since it may be stuck in
		 * recvmsg() waiting for packets from the net.
		 */
		kill(a_recvpid, SIGKILL);
	    }
	    exit(0);
	    break;

	case AFS_USPC_SOCKPROXY_SEND:
	    uspc.retval = SockProxySend(a_sock, pktlist, uspc.req.usp.npkts);
	    break;

	default:
	    /* Some other operation? */
	    uspc.retval = -1;
	    sleep(10);
	}
    }
    /* never reached */
}

/**
 * Receive list of packets from the socket received as an argument.
 *
 * @param[in]   a_sock     socket
 * @param[out]  a_pkts     packets received
 * @param[in]   a_maxpkts  maximum number of packets we can receive
 *
 * @return number of packets received.
 */
static int
SockProxyRecv(int a_sock, struct afs_pkt_hdr *a_pkts, int a_maxpkts)
{
    int pkt_i, n_recv;
    int flags;

    struct iovec iov;
    struct msghdr msg;
    struct sockaddr_in from;

    n_recv = 0;
    flags = 0;

    for (pkt_i = 0; pkt_i < a_maxpkts; pkt_i++) {
	struct afs_pkt_hdr *pkt = &a_pkts[pkt_i];
	ssize_t nbytes;

	pkt->size = AFS_SOCKPROXY_PAYLOAD_ALLOC;

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = pkt->payload;
	iov.iov_len = pkt->size;

	memset(&from, 0, sizeof(from));
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	nbytes = recvmsg(a_sock, &msg, flags);
	if (nbytes < 0) {
	    break;
	}

	pkt->size = nbytes;
	pkt->addr = from.sin_addr.s_addr;
	pkt->port = from.sin_port;

	n_recv++;
	flags = MSG_DONTWAIT;
    }
    return n_recv;
}

/**
 * Receive packets addressed to kernel-space and deliver those packages to it.
 *
 * @param[in]  a_sock  socket
 * @param[in]  a_idx   index of the process responsible for this task
 */
static void
SockProxyReceiveProc(int a_sock, int a_idx)
{
    int code, n_pkts;
    struct afs_pkt_hdr *pktlist;
    struct afs_uspc_param uspc;

    n_pkts = 0;
    pktlist = NULL;
    pkts_alloc(&pktlist, &n_pkts);

    memset(&uspc, 0, sizeof(uspc));
    uspc.req.usp.idx = a_idx;
    uspc.req.usp.npkts = 0;

    while (1) {
	uspc.reqtype = AFS_USPC_SOCKPROXY_RECV;

	code = afsd_syscall(AFSOP_SOCKPROXY_HANDLER, &uspc, pktlist);
	if (code) {
	    uspc.retval = -1;
	    sleep(10);
	    continue;
	}

	switch (uspc.reqtype) {
	case AFS_USPC_SHUTDOWN:
	    exit(0);
	    break;

	case AFS_USPC_SOCKPROXY_RECV:
	    uspc.req.usp.npkts = SockProxyRecv(a_sock, pktlist, n_pkts);
	    uspc.retval = 0;
	    break;

	default:
	    /* Some other operation? */
	    uspc.retval = -1;
	    uspc.req.usp.npkts = 0;
	    sleep(10);
	}
    }
    /* never reached */
}

/**
 * Start processes responsible for sending and receiving packets for libafs.
 *
 * @param[in]  a_rock  not used
 */
static void *
sockproxy_thread(void *a_rock)
{
    int idx;
    int sock;
    int recvpid;

    sock = -1;
    /*
     * Since the socket proxy handler runs as a user process,
     * need to drop the controlling TTY, etc.
     */
    if (afsd_daemon(0, 0) == -1) {
	printf("Error starting socket proxy handler: %s\n", strerror(errno));
	exit(1);
    }

    /*
     * Before we do anything else, we must first wait for a
     * AFS_USPC_SOCKPROXY_START request to setup our udp socket.
     */
    SockProxyStartProc(AFS_SOCKPROXY_INIT_IDX, &sock);

    /* Now fork our AFS_USPC_SOCKPROXY_RECV process. */
    recvpid = fork();
    opr_Assert(recvpid >= 0);
    if (recvpid == 0) {
	SockProxyReceiveProc(sock, AFS_SOCKPROXY_RECV_IDX);
	exit(1);
    }

    /* Now fork our AFS_USPC_SOCKPROXY_SEND processes. */
    for (idx = 0; idx < AFS_SOCKPROXY_NPROCS; idx++) {
	pid_t child;

	if (idx == AFS_SOCKPROXY_RECV_IDX) {
	    /* Receiver already forked. */
	    continue;
	}
	if (idx == AFS_SOCKPROXY_INIT_IDX) {
	    /* We'll start the handler for this index in this process, below. */
	    continue;
	}

	child = fork();
	opr_Assert(child >= 0);
	if (child == 0) {
	    SockProxySendProc(sock, idx, 0);
	    exit(1);
	}
    }
    SockProxySendProc(sock, AFS_SOCKPROXY_INIT_IDX, recvpid);

    return NULL;
}
#endif	/* AFS_SOCKPROXY_ENV */

static void *
afsdb_thread(void *rock)
{
    /* Since the AFSDB lookup handler runs as a user process,
     * need to drop the controlling TTY, etc.
     */
    if (afsd_daemon(0, 0) == -1) {
	printf("Error starting AFSDB lookup handler: %s\n",
	       strerror(errno));
	exit(1);
    }
    AfsdbLookupHandler();
    return NULL;
}

static void *
daemon_thread(void *rock)
{
#ifdef AFS_NEW_BKG
    /* Since the background daemon runs as a user process,
     * need to drop the controlling TTY, etc.
     */
    if (afsd_daemon(0, 0) == -1) {
	printf("Error starting background daemon: %s\n",
	       strerror(errno));
	exit(1);
    }
    BkgHandler();
#else
    afsd_syscall(AFSOP_START_BKG, 0);
#endif
    return NULL;
}

#ifndef UKERNEL
static void *
rmtsysd_thread(void *rock)
{
    rmtsysd();
    return NULL;
}
#endif /* !UKERNEL */

/**
 * Check the command line and cacheinfo options.
 *
 * @param[in] as  parsed command line arguments
 *
 * @note Invokes the shutdown syscall and exits with 0 when
 *       -shutdown is given.
 */
static int
CheckOptions(struct cmd_syndesc *as)
{
    afs_int32 code;		/*Result of fork() */
#ifdef	AFS_SUN5_ENV
    struct stat st;
#endif
#ifdef AFS_SGI_ENV
    struct sched_param sp;
#endif

#ifdef AFS_SGI_VNODE_GLUE
    if (afs_init_kernel_config(-1) < 0) {
	printf("Can't determine NUMA configuration, not starting AFS.\n");
	exit(1);
    }
#endif

    cmd_OpenConfigFile(AFSDIR_CLIENT_CONFIG_FILE_FILEPATH);
    cmd_SetCommandName("afsd");

    /* call atoi on the appropriate parsed results */
    if (cmd_OptionAsInt(as, OPT_blocks, &cacheBlocks) == 0)
	sawCacheBlocks = 1;

    if (cmd_OptionAsInt(as, OPT_files, &cacheFiles) == 0)
	filesSet = 1;

    if (cmd_OptionAsString(as, OPT_rootvol, &rootVolume) == 0)
	rootVolSet = 1;

    if (cmd_OptionAsInt(as, OPT_stat, &cacheStatEntries) == 0)
	sawCacheStatEntries = 1;

    if (cmd_OptionPresent(as, OPT_memcache)) {
	cacheBaseDir = NULL;
	sawCacheBaseDir = 1;
	cacheFlags |= AFSCALL_INIT_MEMCACHE;
    }

    if (cmd_OptionAsString(as, OPT_cachedir, &cacheBaseDir) == 0)
	sawCacheBaseDir = 1;

    if (cmd_OptionAsString(as, OPT_mountdir, &afsd_cacheMountDir) == 0)
	sawCacheMountDir = 1;

    cmd_OptionAsInt(as, OPT_daemons, &nDaemons);

    afsd_verbose = cmd_OptionPresent(as, OPT_verbose);

    if (cmd_OptionPresent(as, OPT_rmtsys)) {
	afsd_rmtsys = 1;
#ifdef UKERNEL
	printf("-rmtsys not supported for UKERNEL\n");
	return -1;
#endif
    }

    if (cmd_OptionPresent(as, OPT_debug)) {
	afsd_debug = 1;
	afsd_verbose = 1;
    }

    if (cmd_OptionAsInt(as, OPT_chunksize, &chunkSize) == 0) {
	if (chunkSize < 0 || chunkSize > 30) {
	    printf
		("afsd:invalid chunk size (not in range 0-30), using default\n");
	    chunkSize = 0;
	}
    }

    if (cmd_OptionAsInt(as, OPT_dcache, &dCacheSize) == 0)
	sawDCacheSize = 1;

    cmd_OptionAsInt(as, OPT_volumes, &vCacheSize);

    if (cmd_OptionPresent(as, OPT_biods)) {
	/* -biods */
#ifndef	AFS_AIX32_ENV
	printf
	    ("afsd: [-biods] currently only enabled for aix3.x VM supported systems\n");
#else
	cmd_OptionAsInt(as, OPT_biods, &nBiods);
#endif
    }
    cmd_OptionAsInt(as, OPT_prealloc, &preallocs);

    if (cmd_OptionAsString(as, OPT_confdir, &confDir) == CMD_MISSING) {
	confDir = strdup(AFSDIR_CLIENT_ETC_DIRPATH);
    }
    sprintf(fullpn_CacheInfo, "%s/%s", confDir, CACHEINFOFILE);

    if (cmd_OptionPresent(as, OPT_logfile)) {
        printf("afsd: Ignoring obsolete -logfile flag\n");
    }

    afsd_CloseSynch = cmd_OptionPresent(as, OPT_waitclose);

    if (cmd_OptionPresent(as, OPT_shutdown)) {
	/* -shutdown */
	/*
	 * Cold shutdown is the default
	 */
	printf("afsd: Shutting down all afs processes and afs state\n");
	code = afsd_syscall(AFSOP_SHUTDOWN, 1);		/* always AFS_COLD */
	if (code) {
	    printf("afsd: AFS still mounted; Not shutting down\n");
	    exit(1);
	}
	exit(0);
    }

    enable_peer_stats = cmd_OptionPresent(as, OPT_peerstats);
    enable_process_stats = cmd_OptionPresent(as, OPT_processstats);

    if (cmd_OptionPresent(as, OPT_memallocsleep)) {
	printf("afsd: -mem_alloc_sleep is deprecated -- ignored\n");
    }

    enable_afsdb = cmd_OptionPresent(as, OPT_afsdb);
    if (cmd_OptionPresent(as, OPT_filesdir)) {
	/* -files_per_subdir */
	int res;
        cmd_OptionAsInt(as, OPT_filesdir, &res);
	if (res < 10 || res > (1 << 30)) {
	    printf
		("afsd:invalid number of files per subdir, \"%s\". Ignored\n",
		 as->parms[25].items->data);
	} else {
	    nFilesPerDir = res;
	}
    }
    enable_dynroot = cmd_OptionPresent(as, OPT_dynroot);

    if (cmd_OptionPresent(as, OPT_fakestat)) {
	enable_fakestat = 2;
    }
    if (cmd_OptionPresent(as, OPT_fakestatall)) {
	enable_fakestat = 1;
    }
    if (cmd_OptionPresent(as, OPT_settime)) {
	/* -settime */
	printf("afsd: -settime ignored\n");
	printf("afsd: the OpenAFS client no longer sets the system time; "
	       "please use NTP or\n");
	printf("afsd: another such system to synchronize client time\n");
    }

    enable_nomount = cmd_OptionPresent(as, OPT_nomount);
    enable_backuptree = cmd_OptionPresent(as, OPT_backuptree);
    enable_rxbind = cmd_OptionPresent(as, OPT_rxbind);

    /* set rx_extraPackets */
    if (cmd_OptionPresent(as, OPT_rxpck)) {
	/* -rxpck */
	int rxpck;
        cmd_OptionAsInt(as, OPT_rxpck, &rxpck);
	printf("afsd: set rxpck = %d\n", rxpck);
	code = afsd_syscall(AFSOP_SET_RXPCK, rxpck);
	if (code) {
	    printf("afsd: failed to set rxpck\n");
	    exit(1);
	}
    }
    if (cmd_OptionPresent(as, OPT_splitcache)) {
	char *c;
	char *var = NULL;

	cmd_OptionAsString(as, OPT_splitcache, &var);

	if (var == NULL || ((c = strchr(var, '/')) == NULL))
	    printf
		("ignoring splitcache (specify as RW/RO percentages: 60/40)\n");
	else {
	    ropct = atoi(c + 1);
	    *c = '\0';
	    rwpct = atoi(var);
	    if ((rwpct != 0) && (ropct != 0) && (ropct + rwpct == 100)) {
		/* -splitcache */
		enable_splitcache = 1;
	    }
	}
	free(var);
    }
    if (cmd_OptionPresent(as, OPT_nodynvcache)) {
#ifdef AFS_MAXVCOUNT_ENV
       afsd_dynamic_vcaches = 0;
#else
       printf("afsd: Error toggling flag, dynamically allocated vcaches not supported on your platform\n");
       exit(1);
#endif
    }
#ifdef AFS_MAXVCOUNT_ENV
    else {
       /* -dynamic-vcaches */
       afsd_dynamic_vcaches = 1;
    }

    if (afsd_verbose)
    printf("afsd: %s dynamically allocated vcaches\n", ( afsd_dynamic_vcaches ? "enabling" : "disabling" ));
#endif

    cmd_OptionAsInt(as, OPT_rxmaxmtu, &rxmaxmtu);

    if (cmd_OptionPresent(as, OPT_dynrootsparse)) {
	enable_dynroot = 2;
    }

    cmd_OptionAsInt(as, OPT_rxmaxfrags, &rxmaxfrags);
    if (cmd_OptionPresent(as, OPT_inumcalc)) {
	cmd_OptionAsString(as, OPT_inumcalc, &inumcalc);
    }
    cmd_OptionAsInt(as, OPT_volume_ttl, &volume_ttl);

    /* parse cacheinfo file if this is a diskcache */
    if (ParseCacheInfoFile()) {
	exit(1);
    }

    return 0;
}

int
afsd_run(void)
{
    static char rn[] = "afsd";	/*Name of this routine */
    struct afsconf_dir *cdir;	/* config dir */
    int lookupResult;		/*Result of GetLocalCellName() */
    int i;
    int code;			/*Result of fork() */
    char *fsTypeMsg = NULL;
    int cacheIteration;		/*How many times through cache verification */
    int vFilesFound;		/*How many data cache files were found in sweep */
    int currVFile;		/*Current AFS cache file number passed in */

	/*
     * Pull out all the configuration info for the workstation's AFS cache and
     * the cellular community we're willing to let our users see.
     */
    cdir = afsconf_Open(confDir);
    if (!cdir) {
	printf("afsd: some file missing or bad in %s\n", confDir);
	exit(1);
    }

    lookupResult =
	afsconf_GetLocalCell(cdir, LclCellName, sizeof(LclCellName));
    if (lookupResult) {
	printf("%s: Can't get my home cell name!  [Error is %d]\n", rn,
	       lookupResult);
    } else {
	if (afsd_verbose)
	    printf("%s: My home cell is '%s'\n", rn, LclCellName);
    }

    if (!enable_nomount) {
	if (afsd_check_mount(rn, afsd_cacheMountDir)) {
	    return -1;
	}
    }

    /* do some random computations in memcache case to get things to work
     * reasonably no matter which parameters you set.
     */
    if (cacheFlags & AFSCALL_INIT_MEMCACHE) {
	/* memory cache: size described either as blocks or dcache entries, but
	 * not both.
	 */
	if (filesSet) {
	    fprintf(stderr, "%s: -files ignored with -memcache\n", rn);
	}
	if (sawDCacheSize) {
	    if (chunkSize == 0) {
		chunkSize = 13;	/* 8k default chunksize for memcache */
	    }
	    if (sawCacheBlocks) {
		printf
		    ("%s: can't set cache blocks and dcache size simultaneously when diskless.\n",
		     rn);
		exit(1);
	    }
	    /* compute the cache size based on # of chunks times the chunk size */
	    i = (1 << chunkSize);	/* bytes per chunk */
	    cacheBlocks = i * dCacheSize;
	    sawCacheBlocks = 1;	/* so that ParseCacheInfoFile doesn't overwrite */
	} else {
	    if (chunkSize == 0) {
		/* Try to autotune the memcache chunksize based on size
		 * of memcache. This is done on the assumption that approx
		 * 1024 chunks is suitable, it's a balance between enough
		 * chunks to be useful and ramping up nicely when using larger
		 * memcache to improve bulk read/write performance
		 */
		for (i = 14;
		     i <= 21 && (1 << i) / 1024 < (cacheBlocks / 1024); i++);
		chunkSize = i - 1;
	    }
	    /* compute the dcache size from overall cache size and chunk size */
	    if (chunkSize > 10) {
		dCacheSize = (cacheBlocks >> (chunkSize - 10));
	    } else if (chunkSize < 10) {
		dCacheSize = (cacheBlocks << (10 - chunkSize));
	    } else {
		dCacheSize = cacheBlocks;
	    }
	    /* don't have to set sawDCacheSize here since it isn't overwritten
	     * by ParseCacheInfoFile.
	     */
	}
	if (afsd_verbose)
	    printf("%s: chunkSize autotuned to %d\n", rn, chunkSize);

	/* kernel computes # of dcache entries as min of cacheFiles and
	 * dCacheSize, so we now make them equal.
	 */
	cacheFiles = dCacheSize;
    } else {
	/* Disk cache:
	 * Compute the number of cache files based on cache size,
	 * but only if -files isn't given on the command line.
	 * Don't let # files be so small as to prevent full utilization
	 * of the cache unless user has explicitly asked for it.
	 */
	if (chunkSize == 0) {
	    /* Set chunksize to 256kB - 1MB depending on cache size */
	    if (cacheBlocks < 500000) {
		chunkSize = 18;
	    } else if (cacheBlocks < 1000000) {
		chunkSize = 19;
	    } else {
		chunkSize = 20;
	    }
	}

	if (!filesSet) {
	    cacheFiles = cacheBlocks / 32;	/* Assume 32k avg filesize */

	    cacheFiles = max(cacheFiles, 1000);

	    /* Always allow more files than chunks.  Presume average V-file
	     * is ~67% of a chunk...  (another guess, perhaps Honeyman will
	     * have a grad student write a paper).  i is KILOBYTES.
	     */
	    i = 1 << (chunkSize < 10 ? 0 : chunkSize - 10);
	    cacheFiles = max(cacheFiles, 1.5 * (cacheBlocks / i));

	    /* never permit more files than blocks, while leaving space for
	     * VolumeInfo and CacheItems files.  VolumeInfo is usually 20K,
	     * CacheItems is 50 Bytes / file (== 1K/20)
	     */
#define CACHEITMSZ (cacheFiles / 20)
#define VOLINFOSZ 50		/* 40kB has been seen, be conservative */
#define CELLINFOSZ 4		/* Assuming disk block size is 4k ... */
#define INFOSZ (VOLINFOSZ+CELLINFOSZ+CACHEITMSZ)

	    /* Sanity check: If the obtained number of disk cache files
	     * is larger than the number of available (4k) disk blocks, we're
	     * doing something wrong. Fail hard so we can fix the bug instead
	     * of silently hiding it like before */

	    if (cacheFiles > (cacheBlocks - INFOSZ) / 4) {
		fprintf(stderr,
			"%s: ASSERT: cacheFiles %d  diskblocks %d\n",
			rn, cacheFiles, (cacheBlocks - INFOSZ) / 4);
		exit(1);
	    }
	    if (cacheFiles < 100)
		fprintf(stderr, "%s: WARNING: cache probably too small!\n",
			rn);

	    if (afsd_verbose)
		printf("%s: cacheFiles autotuned to %d\n", rn, cacheFiles);
	}
	if (!sawDCacheSize) {
	    dCacheSize = cacheFiles / 2;
	    if (dCacheSize > 10000) {
		dCacheSize = 10000;
	    }
	    if (dCacheSize < 2000) {
		dCacheSize = 2000;
	    }
	    if (afsd_verbose)
		printf("%s: dCacheSize autotuned to %d\n", rn, dCacheSize);
	}
    }
    if (!sawCacheStatEntries) {
	if (chunkSize <= 13) {
	    cacheStatEntries = dCacheSize / 4;
	} else if (chunkSize >= 16) {
	    cacheStatEntries = dCacheSize * 1.5;
	} else {
	    cacheStatEntries = dCacheSize;
	}
	if (afsd_verbose)
	    printf("%s: cacheStatEntries autotuned to %d\n", rn,
		   cacheStatEntries);
    }

#if !defined(AFS_CACHE_VNODE_PATH) && !defined(AFS_LINUX_ENV)
    /*
     * Create and zero the inode table for the desired cache files.
     */
    inode_for_V = calloc(cacheFiles, sizeof(AFSD_INO_T));
    if (inode_for_V == (AFSD_INO_T *) 0) {
	printf
	    ("%s: malloc() failed for cache file inode table with %d entries.\n",
	     rn, cacheFiles);
	exit(1);
    }
    if (afsd_debug)
	printf("%s: %d inode_for_V entries at %p, %lu bytes\n", rn,
	       cacheFiles, inode_for_V,
	       (unsigned long)cacheFiles * sizeof(AFSD_INO_T));
#endif

    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	/*
	 * Set up all the pathnames we'll need for later.
	 */
	sprintf(fullpn_DCacheFile, "%s/%s", cacheBaseDir, DCACHEFILE);
	sprintf(fullpn_VolInfoFile, "%s/%s", cacheBaseDir, VOLINFOFILE);
	sprintf(fullpn_CellInfoFile, "%s/%s", cacheBaseDir, CELLINFOFILE);
	sprintf(fullpn_VFile, "%s/", cacheBaseDir);
	vFilePtr = fullpn_VFile + strlen(fullpn_VFile);

	fsTypeMsg = CheckCacheBaseDir(cacheBaseDir);
	if (fsTypeMsg) {
#ifdef AFS_SUN5_ENV
	    printf("%s: WARNING: Cache dir check failed (%s)\n", rn, fsTypeMsg);
#else
	    printf("%s: ERROR: Cache dir check failed (%s)\n", rn, fsTypeMsg);
	    exit(1);
#endif
	}
    }

    /*
     * Set up all the kernel processes needed for AFS.
     */
#ifdef mac2
    setpgrp(getpid(), 0);
#endif /* mac2 */

    /*
     * Set the primary cell name.
     */
    afsd_syscall(AFSOP_SET_THISCELL, LclCellName);

    /* Initialize RX daemons and services */

    /* initialize the rx random number generator from user space */
    {
	/* rand-fortuna wants at least 128 bytes of seed; be generous. */
	unsigned char seedbuf[256];
	if (RAND_bytes(seedbuf, sizeof(seedbuf)) != 1) {
	    printf("SEED_ENTROPY: Error retrieving seed entropy\n");
	}
	afsd_syscall(AFSOP_SEED_ENTROPY, seedbuf, sizeof(seedbuf));
	memset(seedbuf, 0, sizeof(seedbuf));
	/* parse multihomed address files */
	afs_uint32 addrbuf[MAXIPADDRS], maskbuf[MAXIPADDRS],
	    mtubuf[MAXIPADDRS];
	char reason[1024];
	code = afsconf_ParseNetFiles(addrbuf, maskbuf, mtubuf, MAXIPADDRS, reason,
				     AFSDIR_CLIENT_NETINFO_FILEPATH,
				     AFSDIR_CLIENT_NETRESTRICT_FILEPATH);
	if (code > 0) {
	    if (enable_rxbind)
		code = code | 0x80000000;
	    afsd_syscall(AFSOP_ADVISEADDR, code, addrbuf, maskbuf, mtubuf);
	} else
	    printf("ADVISEADDR: Error in specifying interface addresses:%s\n",
		   reason);
    }

    /* Set realtime priority for most threads to same as for biod's. */
    afsd_set_afsd_rtpri();

    /* Start listener, then callback listener. Lastly, start rx event daemon.
     * Change in ordering is so that Linux port has socket fd in listener
     * process.
     * preallocs are passed for both listener and callback server. Only
     * the one which actually does the initialization uses them though.
     */
    if (preallocs < cacheStatEntries + 50)
	preallocs = cacheStatEntries + 50;
#ifdef RXK_LISTENER_ENV
    if (afsd_verbose)
	printf("%s: Forking rx listener daemon.\n", rn);
# ifdef AFS_SUN510_ENV
    fork_rx_syscall_wait(rn, AFSOP_RXLISTENER_DAEMON, preallocs,
                         enable_peer_stats, enable_process_stats);
# else /* !AFS_SUN510_ENV */
    fork_rx_syscall(rn, AFSOP_RXLISTENER_DAEMON, preallocs, enable_peer_stats,
                    enable_process_stats);
# endif /* !AFS_SUN510_ENV */
#endif
    if (afsd_verbose)
	printf("%s: Forking rx callback listener.\n", rn);
#ifndef RXK_LISTENER_ENV
    fork_rx_syscall(rn, AFSOP_START_RXCALLBACK, preallocs, enable_peer_stats,
                    enable_process_stats);
#else
    fork_syscall(rn, AFSOP_START_RXCALLBACK, preallocs, 0, 0);
#endif
#if defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV) || defined(RXK_UPCALL_ENV)
    if (afsd_verbose)
	printf("%s: Forking rxevent daemon.\n", rn);
    fork_rx_syscall(rn, AFSOP_RXEVENT_DAEMON);
#endif

#ifdef AFS_SOCKPROXY_ENV
    if (afsd_verbose)
	printf("%s: Forking socket proxy handlers.\n", rn);
    afsd_fork(0, sockproxy_thread, NULL);
#endif

    if (enable_afsdb) {
	if (afsd_verbose)
	    printf("%s: Forking AFSDB lookup handler.\n", rn);
	afsd_fork(0, afsdb_thread, NULL);
    }
    code = afsd_syscall(AFSOP_BASIC_INIT, 1);
    if (code) {
	printf("%s: Error %d in basic initialization.\n", rn, code);
        exit(1);
    }

    /*
     * Tell the kernel some basic information about the workstation's cache.
     */
    if (afsd_verbose)
	printf
	    ("%s: Calling AFSOP_CACHEINIT: %d stat cache entries, %d optimum cache files, %d blocks in the cache, flags = 0x%x, dcache entries %d\n",
	     rn, cacheStatEntries, cacheFiles, cacheBlocks, cacheFlags,
	     dCacheSize);
    memset(&cparams, '\0', sizeof(cparams));
    cparams.cacheScaches = cacheStatEntries;
    cparams.cacheFiles = cacheFiles;
    cparams.cacheBlocks = cacheBlocks;
    cparams.cacheDcaches = dCacheSize;
    cparams.cacheVolumes = vCacheSize;
    cparams.chunkSize = chunkSize;
    cparams.setTimeFlag = 0;
    cparams.memCacheFlag = cacheFlags;
    cparams.dynamic_vcaches = afsd_dynamic_vcaches;
    code = afsd_syscall(AFSOP_CACHEINIT, &cparams);
    if (code) {
	printf("%s: Error %d during cache init.\n", rn, code);
        exit(1);
    }

    /* do it before we init the cache inodes */
    if (enable_splitcache) {
	afsd_syscall(AFSOP_BUCKETPCT, 1, rwpct);
	afsd_syscall(AFSOP_BUCKETPCT, 2, ropct);
    }

    if (afsd_CloseSynch)
	afsd_syscall(AFSOP_CLOSEWAIT);

    /*
     * Sweep the workstation AFS cache directory, remembering the inodes of
     * valid files and deleting extraneous files.  Keep sweeping until we
     * have the right number of data cache files or we've swept too many
     * times.
     *
     * This also creates files in the cache directory like VolumeItems and
     * CellItems, and thus must be ran before those are sent to the kernel.
     */
    if (afsd_verbose)
	printf("%s: Sweeping workstation's AFS cache directory.\n", rn);
    cacheIteration = 0;
    /* Memory-cache based system doesn't need any of this */
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	do {
	    cacheIteration++;
	    if (SweepAFSCache(&vFilesFound)) {
		printf("%s: Error on sweep %d of workstation AFS cache \
                       directory.\n", rn, cacheIteration);
		exit(1);
	    }
	    if (afsd_verbose)
		printf
		    ("%s: %d out of %d data cache files found in sweep %d.\n",
		     rn, vFilesFound, cacheFiles, cacheIteration);
	} while ((vFilesFound < cacheFiles)
		 && (cacheIteration < MAX_CACHE_LOOPS));
    } else if (afsd_verbose)
	printf("%s: Using memory cache, not swept\n", rn);

    /*
     * Pass the kernel the name of the workstation cache file holding the
     * dcache entries.
     */
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	if (afsd_debug)
	    printf("%s: Calling AFSOP_CACHEINFO: dcache file is '%s'\n", rn,
		   fullpn_DCacheFile);
	afsd_syscall(AFSOP_CACHEINFO, fullpn_DCacheFile);
    }

    /*
     * Pass the kernel the name of the workstation cache file holding the
     * cell information.
     */
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	if (afsd_debug)
	    printf("%s: Calling AFSOP_CELLINFO: cell info file is '%s'\n", rn,
		   fullpn_CellInfoFile);
	afsd_syscall(AFSOP_CELLINFO, fullpn_CellInfoFile);
    }

    if (rxmaxfrags) {
	if (afsd_verbose)
            printf("%s: Setting rxmaxfrags in kernel = %d\n", rn, rxmaxfrags);
	code = afsd_syscall(AFSOP_SET_RXMAXFRAGS, rxmaxfrags);
        if (code)
            printf("%s: Error seting rxmaxfrags\n", rn);
    }

    if (rxmaxmtu) {
	if (afsd_verbose)
            printf("%s: Setting rxmaxmtu in kernel = %d\n", rn, rxmaxmtu);
	code = afsd_syscall(AFSOP_SET_RXMAXMTU, rxmaxmtu);
        if (code)
            printf("%s: Error seting rxmaxmtu\n", rn);
    }

    if (inumcalc != NULL) {
	if (strcmp(inumcalc, "compat") == 0) {
	    if (afsd_verbose) {
		printf("%s: Setting original inode number calculation method in kernel.\n",
		       rn);
	    }
	    code = afsd_syscall(AFSOP_SET_INUMCALC, AFS_INUMCALC_COMPAT);
	    if (code) {
		printf("%s: Error setting inode calculation method: code=%d.\n",
		       rn, code);
	    }
	} else if (strcmp(inumcalc, "md5") == 0) {
	    if (afsd_verbose) {
		printf("%s: Setting md5 digest inode number calculation in kernel.\n",
		       rn);
	    }
	    code = afsd_syscall(AFSOP_SET_INUMCALC, AFS_INUMCALC_MD5);
	    if (code) {
		printf("%s: Error setting inode calculation method: code=%d.\n",
		       rn, code);
	    }
	} else {
	    printf("%s: Unknown value for -inumcalc: %s."
		   "Using default inode calculation method.\n", rn, inumcalc);
	}
    }

    if (enable_dynroot) {
	if (afsd_verbose)
	    printf("%s: Enabling dynroot support in kernel%s.\n", rn,
		   (enable_dynroot==2)?", not showing cells.":"");
	code = afsd_syscall(AFSOP_SET_DYNROOT, 1);
	if (code)
	    printf("%s: Error enabling dynroot support.\n", rn);
    }

    if (enable_fakestat) {
	if (afsd_verbose)
	    printf("%s: Enabling fakestat support in kernel%s.\n", rn,
		   (enable_fakestat==1)?" for all mountpoints."
		   :" for crosscell mountpoints");
	code = afsd_syscall(AFSOP_SET_FAKESTAT, enable_fakestat);
	if (code)
	    printf("%s: Error enabling fakestat support.\n", rn);
    }

    if (enable_backuptree) {
	if (afsd_verbose)
	    printf("%s: Enabling backup tree support in kernel.\n", rn);
	code = afsd_syscall(AFSOP_SET_BACKUPTREE, enable_backuptree);
	if (code)
	    printf("%s: Error enabling backup tree support.\n", rn);
    }

    /*
     * Tell the kernel about each cell in the configuration.
     */
    afsconf_CellApply(cdir, ConfigCell, NULL);
    afsconf_CellAliasApply(cdir, ConfigCellAlias, NULL);

    /* Initialize AFS daemon threads. */
    if (afsd_verbose)
	printf("%s: Forking AFS daemon.\n", rn);
    fork_syscall(rn, AFSOP_START_AFS);

    if (afsd_verbose)
	printf("%s: Forking Check Server Daemon.\n", rn);
    fork_syscall(rn, AFSOP_START_CS);

    if (afsd_verbose)
	printf("%s: Forking %d background daemons.\n", rn, nDaemons);
#if defined(AFS_SGI_ENV) && defined(AFS_SGI_SHORTSTACK)
    /* Add one because for sgi we always "steal" the first daemon for a
     * different task if we only have a 4K stack.
     */
    nDaemons++;
#endif
    for (i = 0; i < nDaemons; i++) {
	afsd_fork(0, daemon_thread, NULL);
    }
#ifdef	AFS_AIX32_ENV
    if (!sawBiod)
	nBiods = nDaemons * 2;
    if (nBiods < 5)
	nBiods = 5;
    for (i = 0; i < nBiods; i++) {
	fork_syscall(rn, AFSOP_START_BKG, nBiods);
    }
#endif

    /*
     * If the root volume has been explicitly set, tell the kernel.
     */
    if (rootVolSet) {
	if (afsd_verbose)
	    printf("%s: Calling AFSOP_ROOTVOLUME with '%s'\n", rn,
		   rootVolume);
	afsd_syscall(AFSOP_ROOTVOLUME, rootVolume);
    }

    if (volume_ttl != 0) {
	if (afsd_verbose)
	    printf("%s: Calling AFSOP_SET_VOLUME_TTL with '%d'\n", rn, volume_ttl);
	code = afsd_syscall(AFSOP_SET_VOLUME_TTL, volume_ttl);
	if (code == EFAULT) {
	    if (volume_ttl < AFS_MIN_VOLUME_TTL)
		printf("%s: Failed to set volume ttl to %d seconds; "
		       "value is too low.\n", rn, volume_ttl);
	    else if (volume_ttl > AFS_MAX_VOLUME_TTL)
		printf("%s: Failed to set volume ttl to %d seconds; "
		       "value is too high.\n", rn, volume_ttl);
	    else
		printf("%s: Failed to set volume ttl to %d seconds; "
		       "value is out of range.\n", rn, volume_ttl);
	} else if (code != 0) {
	    printf("%s: Failed to set volume ttl to %d seconds; "
		   "code=%d.\n", rn, volume_ttl, code);
	}
    }

    /*
     * Pass the kernel the name of the workstation cache file holding the
     * volume information.
     */
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	if (afsd_debug)
	    printf("%s: Calling AFSOP_VOLUMEINFO: volume info file is '%s'\n", rn,
		   fullpn_VolInfoFile);
	afsd_syscall(AFSOP_VOLUMEINFO, fullpn_VolInfoFile);
    }

    /*
     * Give the kernel the names of the AFS files cached on the workstation's
     * disk.
     */
    if (!(cacheFlags & AFSCALL_INIT_MEMCACHE)) {
	int nocachefile = 0;
	if (afsd_debug)
	    printf
	        ("%s: Calling AFSOP_CACHEFILE for each of the %d files in '%s'\n",
	         rn, cacheFiles, cacheBaseDir);
	/* ... and again ... */
	for (currVFile = 0; currVFile < cacheFiles; currVFile++) {
	    if (!nocachefile) {
		sprintf(fullpn_VFile, "%s/D%d/V%d", cacheBaseDir, dir_for_V[currVFile], currVFile);
		code = afsd_syscall(AFSOP_CACHEFILE, fullpn_VFile);
		if (code) {
		    if (currVFile == 0) {
			if (afsd_debug)
			    printf
				("%s: Calling AFSOP_CACHEINODE for each of the %d files in '%s'\n",
				 rn, cacheFiles, cacheBaseDir);
			nocachefile = 1;
		    } else {
			printf
			    ("%s: Error calling AFSOP_CACHEFILE for '%s'\n",
			     rn, fullpn_VFile);
			exit(1);
		    }
		} else {
		    continue;
		}
		/* fall through to setup-by-inode */
	    }
#if defined(AFS_SGI_ENV) || !(defined(AFS_LINUX_ENV) || defined(AFS_CACHE_VNODE_PATH))
	    afsd_syscall(AFSOP_CACHEINODE, inode_for_V[currVFile]);
#else
	    printf
		("%s: Error calling AFSOP_CACHEINODE: not configured\n",
		 rn);
	    exit(1);
#endif
	}
    }
    /*end for */
    /*
     * All the necessary info has been passed into the kernel to run an AFS
     * system.  Give the kernel our go-ahead.
     */
    if (afsd_debug)
	printf("%s: Calling AFSOP_GO with cacheSetTime = %d\n", rn,
	       0);
    afsd_syscall(AFSOP_GO, 0);

    /*
     * At this point, we have finished passing the kernel all the info
     * it needs to set up the AFS.  Mount the AFS root.
     */
    printf("%s: All AFS daemons started.\n", rn);

    if (afsd_verbose)
	printf("%s: Forking trunc-cache daemon.\n", rn);
    fork_syscall(rn, AFSOP_START_TRUNCDAEMON);

    if (!enable_nomount) {
	afsd_mount_afs(rn, afsd_cacheMountDir);
    }

#ifndef UKERNEL
    if (afsd_rmtsys) {
	if (afsd_verbose)
	    printf("%s: Forking 'rmtsys' daemon.\n", rn);
	afsd_fork(0, rmtsysd_thread, NULL);
	code = afsd_syscall(AFSOP_SET_RMTSYS_FLAG, 1);
	if (code)
	    printf("%s: Error enabling rmtsys support.\n", rn);
    }
#endif /* !UKERNEL */
    /*
     * Exit successfully.
     */
    return 0;
}

#ifndef UKERNEL
#include "AFS_component_version_number.c"
#endif

void
afsd_init(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, NULL, NULL, 0, "start AFS");

    /* 0 - 10 */
    cmd_AddParmAtOffset(ts, OPT_blocks, "-blocks", CMD_SINGLE,
		        CMD_OPTIONAL, "1024 byte blocks in cache");
    cmd_AddParmAtOffset(ts, OPT_files, "-files", CMD_SINGLE,
		        CMD_OPTIONAL, "files in cache");
    cmd_AddParmAtOffset(ts, OPT_rootvol, "-rootvol", CMD_SINGLE,
		        CMD_OPTIONAL, "name of AFS root volume");
    cmd_AddParmAtOffset(ts, OPT_stat, "-stat", CMD_SINGLE,
		        CMD_OPTIONAL, "number of stat entries");
    cmd_AddParmAtOffset(ts, OPT_memcache, "-memcache", CMD_FLAG,
		        CMD_OPTIONAL, "run diskless");
    cmd_AddParmAtOffset(ts, OPT_cachedir, "-cachedir", CMD_SINGLE,
		        CMD_OPTIONAL, "cache directory");
    cmd_AddParmAtOffset(ts, OPT_mountdir, "-mountdir", CMD_SINGLE,
		        CMD_OPTIONAL, "mount location");
    cmd_AddParmAtOffset(ts, OPT_daemons, "-daemons", CMD_SINGLE,
		        CMD_OPTIONAL, "number of daemons to use");
    cmd_AddParmAtOffset(ts, OPT_nosettime, "-nosettime", CMD_FLAG,
		        CMD_OPTIONAL, "don't set the time");
    cmd_AddParmAtOffset(ts, OPT_verbose, "-verbose", CMD_FLAG,
		        CMD_OPTIONAL, "display lots of information");
    cmd_AddParmAtOffset(ts, OPT_rmtsys, "-rmtsys", CMD_FLAG,
		        CMD_OPTIONAL, "start NFS rmtsysd program");
    cmd_AddParmAtOffset(ts, OPT_debug, "-debug", CMD_FLAG,
			CMD_OPTIONAL, "display debug info");
    cmd_AddParmAtOffset(ts, OPT_chunksize, "-chunksize", CMD_SINGLE,
		        CMD_OPTIONAL, "log(2) of chunk size");
    cmd_AddParmAtOffset(ts, OPT_dcache, "-dcache", CMD_SINGLE,
		        CMD_OPTIONAL, "number of dcache entries");
    cmd_AddParmAtOffset(ts, OPT_volumes, "-volumes", CMD_SINGLE,
		        CMD_OPTIONAL, "number of volume entries");
    cmd_AddParmAtOffset(ts, OPT_biods, "-biods", CMD_SINGLE,
		        CMD_OPTIONAL, "number of bkg I/O daemons (aix vm)");
    cmd_AddParmAtOffset(ts, OPT_prealloc, "-prealloc", CMD_SINGLE,
		        CMD_OPTIONAL, "number of 'small' preallocated blocks");
    cmd_AddParmAtOffset(ts, OPT_confdir, "-confdir", CMD_SINGLE,
			CMD_OPTIONAL, "configuration directory");
    cmd_AddParmAtOffset(ts, OPT_logfile, "-logfile", CMD_SINGLE,
		        CMD_OPTIONAL, "Place to keep the CM log");
    cmd_AddParmAtOffset(ts, OPT_waitclose, "-waitclose", CMD_FLAG,
		        CMD_OPTIONAL, "make close calls synchronous");
    cmd_AddParmAtOffset(ts, OPT_shutdown, "-shutdown", CMD_FLAG,
		        CMD_OPTIONAL, "Shutdown all afs state");
    cmd_AddParmAtOffset(ts, OPT_peerstats, "-enable_peer_stats", CMD_FLAG,
			CMD_OPTIONAL, "Collect rpc statistics by peer");
    cmd_AddParmAtOffset(ts, OPT_processstats, "-enable_process_stats",
		        CMD_FLAG, CMD_OPTIONAL, "Collect rpc statistics for this process");
    cmd_AddParmAtOffset(ts, OPT_memallocsleep, "-mem_alloc_sleep",
			CMD_FLAG, CMD_OPTIONAL | CMD_HIDE,
			"Allow sleeps when allocating memory cache");
    cmd_AddParmAtOffset(ts, OPT_afsdb, "-afsdb", CMD_FLAG,
		        CMD_OPTIONAL, "Enable AFSDB support");
    cmd_AddParmAtOffset(ts, OPT_filesdir, "-files_per_subdir", CMD_SINGLE,
			CMD_OPTIONAL,
		        "log(2) of the number of cache files per "
			"cache subdirectory");
    cmd_AddParmAtOffset(ts, OPT_dynroot, "-dynroot", CMD_FLAG,
		        CMD_OPTIONAL, "Enable dynroot support");
    cmd_AddParmAtOffset(ts, OPT_fakestat, "-fakestat", CMD_FLAG,
		        CMD_OPTIONAL,
			"Enable fakestat support for cross-cell mounts");
    cmd_AddParmAtOffset(ts, OPT_fakestatall, "-fakestat-all", CMD_FLAG,
		        CMD_OPTIONAL,
			"Enable fakestat support for all mounts");
    cmd_AddParmAtOffset(ts, OPT_nomount, "-nomount", CMD_FLAG,
		        CMD_OPTIONAL, "Do not mount AFS");
    cmd_AddParmAtOffset(ts, OPT_backuptree, "-backuptree", CMD_FLAG,
		        CMD_OPTIONAL,
			"Prefer backup volumes for mountpoints in backup "
			"volumes");
    cmd_AddParmAtOffset(ts, OPT_rxbind, "-rxbind", CMD_FLAG,
			CMD_OPTIONAL,
			"Bind the Rx socket (one interface only)");
    cmd_AddParmAtOffset(ts, OPT_settime, "-settime", CMD_FLAG,
			CMD_OPTIONAL, "set the time");
    cmd_AddParmAtOffset(ts, OPT_rxpck, "-rxpck", CMD_SINGLE, CMD_OPTIONAL,
			"set rx_extraPackets to this value");
    cmd_AddParmAtOffset(ts, OPT_splitcache, "-splitcache", CMD_SINGLE,
			CMD_OPTIONAL,
			"Percentage RW versus RO in cache (specify as 60/40)");
    cmd_AddParmAtOffset(ts, OPT_nodynvcache, "-disable-dynamic-vcaches",
			CMD_FLAG, CMD_OPTIONAL,
			"disable stat/vcache cache growing as needed");
    cmd_AddParmAtOffset(ts, OPT_rxmaxmtu, "-rxmaxmtu", CMD_SINGLE,
			CMD_OPTIONAL, "set rx max MTU to use");
    cmd_AddParmAtOffset(ts, OPT_dynrootsparse, "-dynroot-sparse", CMD_FLAG,
			CMD_OPTIONAL,
			"Enable dynroot support with minimal cell list");
    cmd_AddParmAtOffset(ts, OPT_rxmaxfrags, "-rxmaxfrags", CMD_SINGLE,
			CMD_OPTIONAL,
			"Set the maximum number of UDP fragments Rx should "
			"send/receive per Rx packet");
    cmd_AddParmAtOffset(ts, OPT_inumcalc, "-inumcalc", CMD_SINGLE, CMD_OPTIONAL,
			"Set inode number calculation method");
    cmd_AddParmAtOffset(ts, OPT_volume_ttl, "-volume-ttl", CMD_SINGLE,
			CMD_OPTIONAL,
			"Set the vldb cache timeout value in seconds.");
}

/**
 * Parse and check the command line options.
 *
 * @note The -shutdown command is handled in CheckOptions().
 */
int
afsd_parse(int argc, char **argv)
{
    struct cmd_syndesc *ts = NULL;
    int code;

    code = cmd_Parse(argc, argv, &ts);
    if (code) {
	return code;
    }
    code = CheckOptions(ts);
    cmd_FreeOptions(&ts);
    return code;
}

/**
 * entry point for calling a syscall from another proc/thread.
 *
 * @param[in] rock  a struct afsd_syscall_args* specifying what syscall to call
 *
 * @return unused
 *   @retval NULL always
 */
static void *
call_syscall_thread(void *rock)
{
    struct afsd_syscall_args *args = rock;
    int code;

    if (args->rxpri) {
	afsd_set_rx_rtpri();
    }

    code = afsd_call_syscall(args);
    if (code && args->syscall == AFSOP_START_CS) {
	printf("%s: No check server daemon in client.\n", args->rn);
    }

    free(args);

    return NULL;
}

static void
afsd_syscall_populate(struct afsd_syscall_args *args, int syscall, va_list ap)
{
    afsd_syscall_param_t *params;

    memset(args, 0, sizeof(struct afsd_syscall_args));

    args->syscall = syscall;
    params = args->params;

    switch (syscall) {
    case AFSOP_RXEVENT_DAEMON:
    case AFSOP_CLOSEWAIT:
    case AFSOP_START_AFS:
    case AFSOP_START_CS:
    case AFSOP_START_TRUNCDAEMON:
	break;
    case AFSOP_START_BKG:
    case AFSOP_SHUTDOWN:
    case AFSOP_SET_RXPCK:
    case AFSOP_BASIC_INIT:
    case AFSOP_SET_RXMAXFRAGS:
    case AFSOP_SET_RXMAXMTU:
    case AFSOP_SET_DYNROOT:
    case AFSOP_SET_FAKESTAT:
    case AFSOP_SET_BACKUPTREE:
    case AFSOP_BUCKETPCT:
    case AFSOP_GO:
    case AFSOP_SET_RMTSYS_FLAG:
    case AFSOP_SET_INUMCALC:
    case AFSOP_SET_VOLUME_TTL:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, int)));
	break;
    case AFSOP_SET_THISCELL:
    case AFSOP_ROOTVOLUME:
    case AFSOP_VOLUMEINFO:
    case AFSOP_CACHEFILE:
    case AFSOP_CACHEINFO:
    case AFSOP_CACHEINIT:
    case AFSOP_CELLINFO:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	break;
    case AFSOP_ADDCELLALIAS:
#ifdef AFS_SOCKPROXY_ENV
    case AFSOP_SOCKPROXY_HANDLER:
#endif
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[1] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	break;
    case AFSOP_AFSDB_HANDLER:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[1] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[2] = CAST_SYSCALL_PARAM((va_arg(ap, int)));
	break;
    case AFSOP_BKG_HANDLER:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[1] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[2] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	break;
    case AFSOP_RXLISTENER_DAEMON:
    case AFSOP_START_RXCALLBACK:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, int)));
	params[1] = CAST_SYSCALL_PARAM((va_arg(ap, int)));
	params[2] = CAST_SYSCALL_PARAM((va_arg(ap, int)));
	break;
    case AFSOP_ADVISEADDR:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, int)));
	params[1] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[2] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[3] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	break;
    case AFSOP_ADDCELL2:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[1] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[2] = CAST_SYSCALL_PARAM((va_arg(ap, afs_int32)));
	params[3] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	break;
    case AFSOP_CACHEINODE:
#if defined AFS_SGI_ENV
	{
	    afs_int64 tmp = va_arg(ap, afs_int64);
	    params[0] = CAST_SYSCALL_PARAM((afs_uint32)(tmp >> 32));
	    params[1] = CAST_SYSCALL_PARAM((afs_uint32)(tmp & 0xffffffff));
	}
#else
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, afs_uint32)));
#endif
	break;
    case AFSOP_SEED_ENTROPY:
	params[0] = CAST_SYSCALL_PARAM((va_arg(ap, void *)));
	params[1] = CAST_SYSCALL_PARAM((va_arg(ap, afs_uint32)));
	break;
    default:
	printf("Unknown syscall enountered: %d\n", syscall);
	opr_Assert(0);
    }
}

/**
 * common code for calling a syscall in another proc/thread.
 *
 * @param[in] rx   1 if this is a thread for RX stuff, 0 otherwise
 * @param[in] wait 1 if we should wait for the new proc/thread to finish, 0 to
 *                 let it run in the background
 * @param[in] rn   the name of the running program
 * @param[in] syscall syscall to run
 */
static void
fork_syscall_impl(int rx, int wait, const char *rn, int syscall, va_list ap)
{
    struct afsd_syscall_args *args;

    args = malloc(sizeof(*args));
    afsd_syscall_populate(args, syscall, ap);
    args->rxpri = rx;
    args->rn = rn;

    afsd_fork(wait, call_syscall_thread, args);
}

/**
 * call a syscall in another process or thread.
 */
static void
fork_syscall(const char *rn, int syscall, ...)
{
    va_list ap;

    va_start(ap, syscall);
    fork_syscall_impl(0, 0, rn, syscall, ap);
    va_end(ap);
}

/**
 * call a syscall in another process or thread, and give it RX priority.
 */
static void
fork_rx_syscall(const char *rn, int syscall, ...)
{
    va_list ap;

    va_start(ap, syscall);
    fork_syscall_impl(1, 0, rn, syscall, ap);
    va_end(ap);
}

#if defined(AFS_SUN510_ENV) && defined(RXK_LISTENER_ENV)
/**
 * call a syscall in another process or thread, give it RX priority, and wait
 * for it to finish before returning.
 */
static void
fork_rx_syscall_wait(const char *rn, int syscall, ...)
{
    va_list ap;

    va_start(ap, syscall);
    fork_syscall_impl(1, 1, rn, syscall, ap);
    va_end(ap);
}
#endif /* AFS_SUN510_ENV && RXK_LISTENER_ENV */

static int
afsd_syscall(int syscall, ...)
{
    va_list ap;
    struct afsd_syscall_args args;

    va_start(ap, syscall);
    afsd_syscall_populate(&args, syscall, ap);
    va_end(ap);

    return afsd_call_syscall(&args);
}
