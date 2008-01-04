/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CM_IOCTL_H_ENV__
#define __CM_IOCTL_H_ENV__ 1

#ifndef __CM_IOCTL_INTERFACES_ONLY__
#include "smb.h"
#include "cm_user.h"
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


#define MAXNUMSYSNAMES    16      /* max that current constants allow */
#define   MAXSYSNAME      128     /* max sysname (i.e. @sys) size */
extern char *         cm_sysName;
extern unsigned int   cm_sysNameCount;
extern char *         cm_sysNameList[MAXNUMSYSNAMES];

/* flags for rxstats pioctl */

#define AFSCALL_RXSTATS_MASK    0x7     /* Valid flag bits */
#define AFSCALL_RXSTATS_ENABLE  0x1     /* Enable RX stats */
#define AFSCALL_RXSTATS_DISABLE 0x2     /* Disable RX stats */
#define AFSCALL_RXSTATS_CLEAR   0x4     /* Clear RX stats */

#ifndef __CM_IOCTL_INTERFACES_ONLY__

void cm_InitIoctl(void);

void cm_ResetACLCache(cm_user_t *userp);

extern long cm_IoctlGetACL(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetFileCellName(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSetACL(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlFlushAllVolumes(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlFlushVolume(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlFlushFile(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSetVolumeStatus(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetVolumeStatus(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetFid(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetOwner(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlWhereIs(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlStatMountPoint(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlDeleteMountPoint(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlCheckServers(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGag(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlCheckVolumes(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSetCacheSize(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetCacheParms(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetCell(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlNewCell(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetWsCell(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSysName(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetCellStatus(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSetCellStatus(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSetSPrefs(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetSPrefs(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlStoreBehind(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlCreateMountPoint(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_CleanFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern long cm_FlushFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern long cm_FlushVolume(cm_user_t *, cm_req_t *reqp, afs_uint32 cell, afs_uint32 volume);

extern long cm_FlushParent(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern long cm_IoctlTraceControl(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSetToken(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetTokenIter(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetToken(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlDelToken(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlDelAllToken(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSymlink(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlIslink(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlListlink(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlDeletelink(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlMakeSubmount(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlGetRxkcrypt(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlSetRxkcrypt(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlShutdown(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlFreemountAddCell(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlFreemountRemoveCell(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlMemoryDump(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlRxStatProcess(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlRxStatPeer(smb_ioctl_t *ioctlp, cm_user_t *userp);

extern long cm_IoctlUUIDControl(struct smb_ioctl * ioctlp, struct cm_user *userp);

extern long cm_IoctlPathAvailability(struct smb_ioctl * ioctlp, struct cm_user *userp);

extern long cm_IoctlVolStatTest(struct smb_ioctl *ioctlp, struct cm_user *userp);

#endif /* __CM_IOCTL_INTERFACES_ONLY__ */

#endif /*  __CM_IOCTL_H_ENV__ */
