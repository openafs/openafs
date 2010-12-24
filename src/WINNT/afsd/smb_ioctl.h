/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_WINNT_AFSD_SMB_IOCTL_H
#define OPENAFS_WINNT_AFSD_SMB_IOCTL_H 1

#include <cm_ioctl.h>
#include <smb_iocons.h>

/* magic file name for ioctl opens */
#define SMB_IOCTL_FILENAME	   CM_IOCTL_FILENAME
#define SMB_IOCTL_FILENAME_NOSLASH CM_IOCTL_FILENAME_NOSLASH

/* max parms for ioctl, in either direction */
#define SMB_IOCTL_MAXDATA	   CM_IOCTL_MAXDATA
#define SMB_IOCTL_MAXPROCS         CM_IOCTL_MAXPROCS

struct smb_fid;
struct smb_user;
struct smb_vc;

/* ioctl parameter, while being assembled and/or processed */
typedef struct smb_ioctl {
    /* fid pointer */
    struct smb_fid *fidp;

    /* uid pointer */
    struct smb_user *uidp;

    /* pathname associated with the Tree ID */
    clientchar_t *tidPathp;

    /* prefix for subst drives */
    cm_space_t *prefix;

    cm_ioctl_t  ioctl;
} smb_ioctl_t;

/* procedure implementing an ioctl */
typedef long (smb_ioctlProc_t)(smb_ioctl_t *, struct cm_user *userp, afs_uint32 flags);

extern void
smb_InitIoctl(void);

extern void
smb_SetupIoctlFid(struct smb_fid *fidp, cm_space_t *prefix);

extern afs_int32
smb_IoctlRead(struct smb_fid *fidp, struct smb_vc *vcp,
              struct smb_packet *inp, struct smb_packet *outp);

extern afs_int32
smb_IoctlWrite(struct smb_fid *fidp, struct smb_vc *vcp,
               struct smb_packet *inp, struct smb_packet *outp);

extern afs_int32
smb_IoctlV3Write(struct smb_fid *fidp, struct smb_vc *vcp,
                 struct smb_packet *inp, struct smb_packet *outp);

extern afs_int32
smb_IoctlV3Read(struct smb_fid *fidp, struct smb_vc *vcp,
                struct smb_packet *inp, struct smb_packet *outp);

extern afs_int32
smb_IoctlReadRaw(struct smb_fid *fidp, struct smb_vc *vcp,
                 struct smb_packet *inp, struct smb_packet *outp);

extern afs_int32
smb_IoctlPrepareRead(struct smb_fid *fidp, smb_ioctl_t *ioctlp,
                     cm_user_t *userp, afs_uint32 pflags);

extern afs_int32
smb_ParseIoctlPath(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                   cm_scache_t **scpp, afs_uint32 flags);

extern afs_int32
smb_ParseIoctlParent(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                     cm_scache_t **scpp, clientchar_t *leafp);

extern afs_int32
smb_IoctlSetToken(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 flags);

extern afs_int32
smb_IoctlGetSMBName(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetACL(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetFileCellName(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetACL(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlFlushAllVolumes(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlFlushVolume(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlFlushFile(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetVolumeStatus(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetVolumeStatus(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetFid(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetOwner(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlWhereIs(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlStatMountPoint(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlDeleteMountPoint(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlCheckServers(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGag(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlCheckVolumes(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetCacheSize(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetCacheParms(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetCell(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlNewCell(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlNewCell2(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetWsCell(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSysName(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetCellStatus(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetCellStatus(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetSPrefs(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetSPrefs(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlStoreBehind(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlCreateMountPoint(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 cm_CleanFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_FlushFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 cm_FlushVolume(cm_user_t *, cm_req_t *reqp, afs_uint32 cell, afs_uint32 volume);

extern afs_int32 cm_FlushParent(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp);

extern afs_int32 smb_IoctlTraceControl(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetToken(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetTokenIter(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetToken(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlDelToken(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlDelAllToken(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSymlink(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlIslink(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlListlink(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlDeletelink(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlMakeSubmount(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetRxkcrypt(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetRxkcrypt(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlShutdown(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlFreemountAddCell(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlFreemountRemoveCell(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlMemoryDump(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlRxStatProcess(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlRxStatPeer(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlUUIDControl(struct smb_ioctl * ioctlp, struct cm_user *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlPathAvailability(struct smb_ioctl * ioctlp, struct cm_user *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetFileType(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlVolStatTest(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlUnicodeControl(struct smb_ioctl *ioctlp, struct cm_user * userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetOwner(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetGroup(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlGetUnixMode(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 flags);

extern afs_int32 smb_IoctlSetUnixMode(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 flags);

#endif /*  OPENAFS_WINNT_AFSD_SMB_IOCTL_H */
