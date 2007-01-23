/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* /usr/andrew/include/vice/ioctl.h

Definitions of Venus-specific ioctls for Venus 2.
 */

#ifndef AFS_VENUS_H
#define AFS_VENUS_H

#if !defined(UKERNEL)

#ifndef _IOW
#include <sys/ioctl.h>
#endif

#ifndef _VICEIOCTL
#include <afs/vice.h>
#endif

/* some structures used with CM pioctls */

/* structs for Get/Set server preferences pioctl
 */
struct spref {
    struct in_addr server;
    unsigned short rank;
};

struct sprefrequest_33 {
    unsigned short offset;
    unsigned short num_servers;
};

struct sprefrequest {		/* new struct for 3.4 */
    unsigned short offset;
    unsigned short num_servers;
    unsigned short flags;
};
#define DBservers 1

struct sprefinfo {
    unsigned short next_offset;
    unsigned short num_servers;
    struct spref servers[1];	/* we overrun this array intentionally... */
};

struct setspref {
    unsigned short flags;
    unsigned short num_servers;
    struct spref servers[1];	/* we overrun this array intentionally... */
};
/* struct for GAG pioctl
 */
struct gaginfo {
    afs_uint32 showflags, logflags, logwritethruflag, spare[3];
    unsigned char spare2[128];
};
#define GAGUSER    1
#define GAGCONSOLE 2
#define logwritethruON	1


struct rxparams {
    afs_int32 rx_initReceiveWindow, rx_maxReceiveWindow, rx_initSendWindow,
	rx_maxSendWindow, rxi_nSendFrags, rxi_nRecvFrags, rxi_OrphanFragSize;
    afs_int32 rx_maxReceiveSize, rx_MyMaxSendSize;
    afs_uint32 spare[21];
};

/* struct for checkservers */

struct chservinfo {
    int magic;
    char tbuffer[128];
    int tsize;
    afs_int32 tinterval;
    afs_int32 tflags;
};

struct sbstruct {
    int sb_thisfile;
    int sb_default;
};

/* CM inititialization parameters. What CM actually used after calculations
 * based on passed in arguments.
 */
#define CMI_VERSION 1		/* increment when adding new fields. */
struct cm_initparams {
    int cmi_version;
    int cmi_nChunkFiles;
    int cmi_nStatCaches;
    int cmi_nDataCaches;
    int cmi_nVolumeCaches;
    int cmi_firstChunkSize;
    int cmi_otherChunkSize;
    int cmi_cacheSize;		/* The original cache size, in 1K blocks. */
    unsigned cmi_setTime:1;
    unsigned cmi_memCache:1;
    int spare[16 - 9];		/* size of struct is 16 * 4 = 64 bytes */
};

#endif /* !defined(UKERNEL) */

/* IOCTLS to Venus.  Apply these to open file decriptors. */
#define	VIOCCLOSEWAIT		_VICEIOCTL(1)	/* Force close to wait for store */
#define	VIOCABORT		_VICEIOCTL(2)	/* Abort close on this fd */
#define	VIOCIGETCELL		_VICEIOCTL(3)	/* ioctl to get cell name */

/* PIOCTLS to Venus.  Apply these to path names with pioctl. */
#define	VIOCSETAL		_VICEIOCTL(1)	/* Set access control list */
#define	VIOCGETAL		_VICEIOCTL(2)	/* Get access control list */
#define	VIOCSETTOK		_VICEIOCTL(3)	/* Set authentication tokens */
#define	VIOCGETVOLSTAT		_VICEIOCTL(4)	/* Get volume status */
#define	VIOCSETVOLSTAT		_VICEIOCTL(5)	/* Set volume status */
#define	VIOCFLUSH		_VICEIOCTL(6)	/* Invalidate cache entry */
#define	VIOCSTAT		_VICEIOCTL(7)	/* Get file status */
#define	VIOCGETTOK		_VICEIOCTL(8)	/* Get authentication tokens */
#define	VIOCUNLOG		_VICEIOCTL(9)	/* Invalidate tokens */
#define	VIOCCKSERV		_VICEIOCTL(10)	/* Check that servers are up */
#define	VIOCCKBACK		_VICEIOCTL(11)	/* Check backup volume mappings */
#define	VIOCCKCONN		_VICEIOCTL(12)	/* Check connections for a user */
#define	VIOCGETTIME		_VICEIOCTL(13)	/* Do a vice gettime for performance testing */
#define	VIOCWHEREIS		_VICEIOCTL(14)	/* Find out where a volume is located */
#define	VIOCPREFETCH		_VICEIOCTL(15)	/* Prefetch a file */
#define	VIOCNOP			_VICEIOCTL(16)	/* Do nothing (more preformance) */
#define	VIOCENGROUP		_VICEIOCTL(17)	/* Enable group access for a group */
#define	VIOCDISGROUP		_VICEIOCTL(18)	/* Disable group access */
#define	VIOCLISTGROUPS		_VICEIOCTL(19)	/* List enabled and disabled groups */
#define	VIOCACCESS		_VICEIOCTL(20)	/* Access using PRS_FS bits */
#define	VIOCUNPAG		_VICEIOCTL(21)	/* Invalidate pag */
#define	VIOCGETFID		_VICEIOCTL(22)	/* Get file ID quickly */
#define	VIOCWAITFOREVER		_VICEIOCTL(23)	/* Wait for dead servers forever */
#define	VIOCSETCACHESIZE	_VICEIOCTL(24)	/* Set venus cache size in 1k units */
#define	VIOCFLUSHCB		_VICEIOCTL(25)	/* Flush callback only */
#define	VIOCNEWCELL		_VICEIOCTL(26)	/* Configure new cell */
#define VIOCGETCELL		_VICEIOCTL(27)	/* Get cell info */
#define	VIOC_AFS_DELETE_MT_PT	_VICEIOCTL(28)	/* [AFS] Delete mount point */
#define VIOC_AFS_STAT_MT_PT	_VICEIOCTL(29)	/* [AFS] Stat mount point */
#define	VIOC_FILE_CELL_NAME	_VICEIOCTL(30)	/* Get cell in which file lives */
#define	VIOC_GET_WS_CELL	_VICEIOCTL(31)	/* Get cell in which workstation lives */
#define VIOC_AFS_MARINER_HOST	_VICEIOCTL(32)	/* [AFS] Get/set mariner host */
#define VIOC_GET_PRIMARY_CELL	_VICEIOCTL(33)	/* Get primary cell for caller */
#define	VIOC_VENUSLOG		_VICEIOCTL(34)	/* Enable/Disable venus logging */
#define	VIOC_GETCELLSTATUS	_VICEIOCTL(35)	/* get cell status info */
#define	VIOC_SETCELLSTATUS	_VICEIOCTL(36)	/* set corresponding info */
#define	VIOC_FLUSHVOLUME	_VICEIOCTL(37)	/* flush whole volume's data */
#define	VIOC_AFS_SYSNAME	_VICEIOCTL(38)	/* Change @sys value */
#define	VIOC_EXPORTAFS		_VICEIOCTL(39)	/* Export afs to nfs clients */
#define VIOCGETCACHEPARMS	_VICEIOCTL(40)	/* Get cache stats */
#define VIOCGETVCXSTATUS	_VICEIOCTL(41)
#define VIOC_SETSPREFS33  	_VICEIOCTL(42)	/* Set server ranks */
#define VIOC_GETSPREFS  	_VICEIOCTL(43)	/* Get server ranks */
#define VIOC_GAG    	        _VICEIOCTL(44)	/* silence CM */
#define VIOC_TWIDDLE    	_VICEIOCTL(45)	/* adjust RX knobs */
#define VIOC_SETSPREFS  	_VICEIOCTL(46)	/* Set server ranks */
#define VIOC_STORBEHIND  	_VICEIOCTL(47)	/* adjust store asynchrony */
#define VIOC_GCPAGS		_VICEIOCTL(48)	/* disable automatic pag gc-ing */
#define VIOC_GETINITPARAMS	_VICEIOCTL(49)	/* get initial cm params */
#define VIOC_GETCPREFS  	_VICEIOCTL(50)	/* Get client interface */
#define VIOC_SETCPREFS  	_VICEIOCTL(51)	/* Set client interface */
#define VIOC_AFS_FLUSHMOUNT	_VICEIOCTL(52)	/* Flush mount symlink data */
#define VIOC_RXSTAT_PROC	_VICEIOCTL(53)	/* Control process RX stats */
#define VIOC_RXSTAT_PEER	_VICEIOCTL(54)	/* Control peer RX stats */
#define VIOC_GETRXKCRYPT        _VICEIOCTL(55)	/* Set rxkad enc flag */
#define VIOC_SETRXKCRYPT        _VICEIOCTL(56)	/* Set rxkad enc flag */
#define VIOC_PREFETCHTAPE       _VICEIOCTL(66)	/* MR-AFS prefetch from tape */
#define VIOC_RESIDENCY_CMD      _VICEIOCTL(67)	/* generic MR-AFS cmds */
#define VIOC_STATISTICS         _VICEIOCTL(68)	/* arla: fetch statistics */
#define VIOC_GETVCXSTATUS2      _VICEIOCTL(69)  /* vcache statistics */

/* Coordinated 'C' pioctl's */
#define VIOC_NEWALIAS		_CVICEIOCTL(1)	/* create new cell alias */
#define VIOC_GETALIAS		_CVICEIOCTL(2)	/* get alias info */
#define VIOC_CBADDR		_CVICEIOCTL(3)	/* push callback addr */
#define VIOC_DISCON		_CVICEIOCTL(5)	/* set/get discon mode */
#define VIOC_GETCAPABILITES	_CVICEIOCTL(6)	/* cache manager capabilities */
#define VIOC_GETTOKNEW		_CVICEIOCTL(7)	/* fetch tokens (K5, ...) */
#define VIOC_SETTOKNEW		_CVICEIOCTL(8)	/* set tokens (K5, ...) */

/* OpenAFS-specific 'O' pioctl's */
#define VIOC_NFS_NUKE_CREDS	_OVICEIOCTL(1)	/* nuke creds for all PAG's */

#endif /* AFS_VENUS_H */
