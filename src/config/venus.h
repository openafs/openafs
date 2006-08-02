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

/* The VIOC* constants are defined here. */
#include <afs/vioc.h>

#if !defined(UKERNEL)

#include <netinet/in.h>

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

#endif /* AFS_VENUS_H */
