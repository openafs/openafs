/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __SMB_IOCONS_H_ENV_
#define __SMB_IOCONS_H_ENV_ 1

/* included in both AFSD and fs commands */

typedef struct chservinfo {
	int magic;
	char tbuffer[128];
        int tsize;
        long tinterval;
        long tflags;
} chservinfo_t;

struct gaginfo {
	unsigned long showflags, logflags, logwritethruflag, spare[3];
        unsigned char spare2[128];
};

#define GAGUSER		1
#define GAGCONSOLE	2

struct ClearToken {
	int AuthHandle;
	char HandShakeKey[8];
	int ViceId;
	int BeginTimestamp;
	int EndTimestamp;
};

struct sbstruct {
	int sb_thisfile;
        int sb_default;
};

#define CM_IOCTLCACHEPARMS		16
typedef struct cm_cacheParms {
	long parms[CM_IOCTLCACHEPARMS];
} cm_cacheParms_t;

/* set cell flags */
#define CM_SETCELLFLAG_SUID		2

#define VIOC_FILE_CELL_NAME		0x1
#define VIOCGETAL			0x2
#define VIOCSETAL			0x3
#define VIOC_FLUSHVOLUME		0x4
#define VIOCFLUSH			0x5
#define VIOCSETVOLSTAT			0x6
#define VIOCGETVOLSTAT			0x7
#define VIOCWHEREIS			0x8
#define VIOC_AFS_STAT_MT_PT		0x9
#define VIOC_AFS_DELETE_MT_PT		0xa
#define VIOCCKSERV			0xb
#define VIOC_GAG			0xc
#define VIOCCKBACK			0xd
#define VIOCSETCACHESIZE		0xe
#define VIOCGETCACHEPARMS		0xf
#define VIOCGETCELL			0x10
#define VIOCNEWCELL			0x11
#define VIOC_GET_WS_CELL		0x12
#define VIOC_AFS_MARINER_HOST		0x13
#define VIOC_AFS_SYSNAME		0x14
#define VIOC_EXPORTAFS			0x15
#define VIOC_GETCELLSTATUS		0x16
#define VIOC_SETCELLSTATUS		0x17
#define VIOC_SETSPREFS			0x18
#define VIOC_GETSPREFS			0x19
#define VIOC_STOREBEHIND		0x1a
#define VIOC_AFS_CREATE_MT_PT		0x1b
#define VIOC_TRACECTL			0x1c
#define VIOCSETTOK			0x1d
#define VIOCGETTOK			0x1e
#define VIOCNEWGETTOK			0x1f
#define VIOCDELTOK			0x20
#define VIOCDELALLTOK			0x21
#define VIOC_ISSYMLINK			0x22
#define VIOC_SYMLINK			0x23
#define VIOC_LISTSYMLINK		0x24
#define VIOC_DELSYMLINK			0x25
#define VIOC_MAKESUBMOUNT		0x26
#define VIOC_GETRXKCRYPT		0x27
#define VIOC_SETRXKCRYPT		0x28
#define VIOC_TRACEMEMDUMP               0x29
#define VIOC_SHUTDOWN                   0x2a
#define VIOC_FLUSHALL                   0x2b
#define VIOCGETFID                      0x2c
#define VIOCGETOWNER                    0x2d
#define VIOC_RXSTAT_PROC                0x2e
#define VIOC_RXSTAT_PEER                0x2f
#endif /*  __SMB_IOCONS_H_ENV_ */
