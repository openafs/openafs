/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_CM_IOCTL_H
#define OPENAFS_WINNT_AFSD_CM_IOCTL_H 1

#ifndef __CM_IOCTL_INTERFACES_ONLY__
#include "cm_user.h"
#else
typedef struct cm_fid {
        unsigned long cell;
        unsigned long volume;
        unsigned long vnode;
        unsigned long unique;
} cm_fid_t;
#endif /* __CM_IOCTL_INTERFACES_ONLY__ */

/* the following four structures are used for fs get/set serverprefs command*/
#define		CM_SPREF_VLONLY		0x01
typedef struct cm_SPref {
        struct in_addr host;
        unsigned short rank;
} cm_SPref_t;

typedef struct cm_SPrefRequest {
        unsigned short offset;
        unsigned short num_servers;
        unsigned short flags;
} cm_SPrefRequest_t;

typedef struct cm_SPrefInfo {
        unsigned short next_offset;
        unsigned short num_servers;
        struct cm_SPref servers[1];/* we overrun this array intentionally...*/
} cm_SPrefInfo_t;

typedef struct cm_SSetPref {
        unsigned short flags;
        unsigned short num_servers;
        struct cm_SPref servers[1];/* we overrun this array intentionally...*/
} cm_SSetPref_t;

#define CM_IOCTLCACHEPARMS		16
typedef struct cm_cacheParms {
        afs_uint64 parms[CM_IOCTLCACHEPARMS];
} cm_cacheParms_t;

typedef struct cm_ioctl {
    /* input side */
    char *inDatap;			/* ioctl func's current position
					 * in input parameter block */
    char *inAllocp;			/* allocated input parameter block */
    afs_uint32 inCopied;		/* # of input bytes copied in so far
					 * by write calls */
    /* output side */
    char *outDatap;			/* output results assembled so far */
    char *outAllocp;		        /* output results assembled so far */
    afs_uint32 outCopied;		/* # of output bytes copied back so far

    /* flags */
    afs_uint32 flags;
} cm_ioctl_t;

/* flags for smb_ioctl_t */
#define CM_IOCTLFLAG_DATAIN	1	/* reading data from client to server */
#define CM_IOCTLFLAG_LOGON	2	/* got tokens from integrated logon */
#define CM_IOCTLFLAG_USEUTF8    4       /* this request is using UTF-8 strings */
#define CM_IOCTLFLAG_DATAOUT    8       /* sending data from server to client */


/*
 * The cm_IoctlQueryOptions structure is designed to be extendible.
 * None of the fields are required but when specified
 * by the client and understood by the server will be
 * used to more precisely specify the desired data.
 *
 * size must be set to the size of the structure
 * sent by the client including any variable length
 * data appended to the end of the static structure.
 *
 * field_flags are used to determine which fields have
 * been filled in and should be used.
 *
 * variable length data can be specified with fields
 * that include offsets to data appended to the
 * structure.
 *
 * when adding new fields you must:
 *  - add the field
 *  - define a CM_IOCTL_QOPTS_FIELD_xxx bit flag
 *  - define a CM_IOCTL_QOPTS_HAVE_xxx macro
 *
 * It is critical that flags be consistent across all
 * implementations of the pioctl interface for a given
 * platform.  This should be considered a public
 * interface used by third party application developers.
 */

typedef struct cm_IoctlQueryOptions {
    afs_uint32  size;
    afs_uint32  field_flags;
    afs_uint32  literal;
    cm_fid_t    fid;
} cm_ioctlQueryOptions_t;

/* field flags -  */
#define CM_IOCTL_QOPTS_FIELD_LITERAL 1
#define CM_IOCTL_QOPTS_FIELD_FID     2

#define CM_IOCTL_QOPTS_HAVE_LITERAL(p) (p->size >= 12 && (p->field_flags & CM_IOCTL_QOPTS_FIELD_LITERAL))
#define CM_IOCTL_QOPTS_HAVE_FID(p)     (p->size >= 28 && (p->field_flags & CM_IOCTL_QOPTS_FIELD_FID))

#define MAXNUMSYSNAMES    16      /* max that current constants allow */
#define   MAXSYSNAME      128     /* max sysname (i.e. @sys) size */
extern clientchar_t  *cm_sysName;
extern unsigned int   cm_sysNameCount;
extern clientchar_t  *cm_sysNameList[MAXNUMSYSNAMES];

/* Paths that are passed into pioctl calls can be specified using
   UTF-8.  These strings are prefixed with UTF8_PREFIX defined below.
   The sequence ESC '%' 'G' is used by ISO-2022 to designate UTF-8
   strings. */
#define UTF8_PREFIX "\33%G"

extern const char utf8_prefix[];
extern const int  utf8_prefix_size;

/* flags for rxstats pioctl */

#define AFSCALL_RXSTATS_MASK    0x7     /* Valid flag bits */
#define AFSCALL_RXSTATS_ENABLE  0x1     /* Enable RX stats */
#define AFSCALL_RXSTATS_DISABLE 0x2     /* Disable RX stats */
#define AFSCALL_RXSTATS_CLEAR   0x4     /* Clear RX stats */

/* pioctl flags */

#define AFSCALL_FLAG_LOCAL_SYSTEM 0x1

#ifndef __CM_IOCTL_INTERFACES_ONLY__

extern void cm_InitIoctl(void);

extern cm_ioctlQueryOptions_t *
cm_IoctlGetQueryOptions(struct cm_ioctl *ioctlp, struct cm_user *userp);

extern void
cm_IoctlSkipQueryOptions(struct cm_ioctl *ioctlp, struct cm_user *userp);

extern void
cm_NormalizeAfsPath(clientchar_t *outpathp, long outlen, clientchar_t *inpathp);

extern void cm_SkipIoctlPath(cm_ioctl_t *ioctlp);

extern clientchar_t * cm_ParseIoctlStringAlloc(cm_ioctl_t *ioctlp, const char * ext_instrp);

extern int cm_UnparseIoctlString(cm_ioctl_t *ioctlp, char * ext_outp, const clientchar_t * cstr, int cchlen);

extern afs_int32 cm_IoctlGetACL(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlGetFileCellName(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlSetACL(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlFlushAllVolumes(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_IoctlFlushVolume(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlFlushFile(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlSetVolumeStatus(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlGetVolumeStatus(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlGetFid(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlGetOwner(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlSetOwner(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlSetGroup(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlWhereIs(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlStatMountPoint(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlDeleteMountPoint(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlCheckServers(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGag(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlCheckVolumes(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlSetCacheSize(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetCacheParms(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetCell(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlNewCell(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlNewCell2(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetWsCell(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlSysName(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetCellStatus(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlSetCellStatus(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlSetSPrefs(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetSPrefs(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlStoreBehind(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlCreateMountPoint(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *dscp, cm_req_t *reqp, clientchar_t *leaf);

extern afs_int32 cm_CleanFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_FlushFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_FlushVolume(cm_user_t *, cm_req_t *reqp, afs_uint32 cell, afs_uint32 volume);

extern afs_int32 cm_FlushParent(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_IoctlTraceControl(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlSetToken(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetTokenIter(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetToken(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlDelToken(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlDelAllToken(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlSymlink(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *dscp, cm_req_t *reqp, clientchar_t *leaf);

extern afs_int32 cm_IoctlIslink(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlListlink(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlDeletelink(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *dscp, cm_req_t *reqp);

extern afs_int32 cm_IoctlMakeSubmount(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlGetRxkcrypt(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlSetRxkcrypt(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlShutdown(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlFreemountAddCell(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlFreemountRemoveCell(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlMemoryDump(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlRxStatProcess(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlRxStatPeer(cm_ioctl_t *ioctlp, cm_user_t *userp);

extern afs_int32 cm_IoctlUUIDControl(struct cm_ioctl * ioctlp, struct cm_user *userp);

extern afs_int32 cm_IoctlPathAvailability(struct cm_ioctl * ioctlp, struct cm_user *userp, struct cm_scache *scp, struct cm_req *reqp);

extern afs_int32 cm_IoctlGetFileType(cm_ioctl_t *ioctlp, cm_user_t *userp, struct cm_scache *scp, struct cm_req *reqp);

extern afs_int32 cm_IoctlVolStatTest(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_req_t *reqp);

extern afs_int32 cm_IoctlUnicodeControl(struct cm_ioctl *ioctlp, struct cm_user * userp);

extern void TranslateExtendedChars(char *str);

extern afs_int32 cm_IoctlGetUnixMode(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

extern afs_int32 cm_IoctlSetUnixMode(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp);

#endif /* __CM_IOCTL_INTERFACES_ONLY__ */

#endif /*  OPENAFS_WINNT_AFSD_CM_IOCTL_H */
