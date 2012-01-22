/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <sddl.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <strsafe.h>

#include <osi.h>

#include "afsd.h"

#include "smb.h"

#include "cm_rpc.h"
#include "afs/afsrpc.h"
#include "afs/auth.h"

smb_ioctlProc_t *smb_ioctlProcsp[SMB_IOCTL_MAXPROCS];

void
smb_InitIoctl(void)
{
    int i;
    for (i=0; i<SMB_IOCTL_MAXPROCS; i++)
        smb_ioctlProcsp[i] = NULL;

    smb_ioctlProcsp[VIOCGETAL] = smb_IoctlGetACL;
    smb_ioctlProcsp[VIOC_FILE_CELL_NAME] = smb_IoctlGetFileCellName;
    smb_ioctlProcsp[VIOCSETAL] = smb_IoctlSetACL;
    smb_ioctlProcsp[VIOC_FLUSHVOLUME] = smb_IoctlFlushVolume;
    smb_ioctlProcsp[VIOCFLUSH] = smb_IoctlFlushFile;
    smb_ioctlProcsp[VIOCSETVOLSTAT] = smb_IoctlSetVolumeStatus;
    smb_ioctlProcsp[VIOCGETVOLSTAT] = smb_IoctlGetVolumeStatus;
    smb_ioctlProcsp[VIOCWHEREIS] = smb_IoctlWhereIs;
    smb_ioctlProcsp[VIOC_AFS_STAT_MT_PT] = smb_IoctlStatMountPoint;
    smb_ioctlProcsp[VIOC_AFS_DELETE_MT_PT] = smb_IoctlDeleteMountPoint;
    smb_ioctlProcsp[VIOCCKSERV] = smb_IoctlCheckServers;
    smb_ioctlProcsp[VIOC_GAG] = smb_IoctlGag;
    smb_ioctlProcsp[VIOCCKBACK] = smb_IoctlCheckVolumes;
    smb_ioctlProcsp[VIOCSETCACHESIZE] = smb_IoctlSetCacheSize;
    smb_ioctlProcsp[VIOCGETCACHEPARMS] = smb_IoctlGetCacheParms;
    smb_ioctlProcsp[VIOCGETCELL] = smb_IoctlGetCell;
    smb_ioctlProcsp[VIOCNEWCELL] = smb_IoctlNewCell;
    smb_ioctlProcsp[VIOC_GET_WS_CELL] = smb_IoctlGetWsCell;
    smb_ioctlProcsp[VIOC_AFS_SYSNAME] = smb_IoctlSysName;
    smb_ioctlProcsp[VIOC_GETCELLSTATUS] = smb_IoctlGetCellStatus;
    smb_ioctlProcsp[VIOC_SETCELLSTATUS] = smb_IoctlSetCellStatus;
    smb_ioctlProcsp[VIOC_SETSPREFS] = smb_IoctlSetSPrefs;
    smb_ioctlProcsp[VIOC_GETSPREFS] = smb_IoctlGetSPrefs;
    smb_ioctlProcsp[VIOC_STOREBEHIND] = smb_IoctlStoreBehind;
    smb_ioctlProcsp[VIOC_AFS_CREATE_MT_PT] = smb_IoctlCreateMountPoint;
    smb_ioctlProcsp[VIOC_TRACECTL] = smb_IoctlTraceControl;
    smb_ioctlProcsp[VIOCSETTOK] = smb_IoctlSetToken;
    smb_ioctlProcsp[VIOCGETTOK] = smb_IoctlGetTokenIter;
    smb_ioctlProcsp[VIOCNEWGETTOK] = smb_IoctlGetToken;
    smb_ioctlProcsp[VIOCDELTOK] = smb_IoctlDelToken;
    smb_ioctlProcsp[VIOCDELALLTOK] = smb_IoctlDelAllToken;
    smb_ioctlProcsp[VIOC_SYMLINK] = smb_IoctlSymlink;
    smb_ioctlProcsp[VIOC_LISTSYMLINK] = smb_IoctlListlink;
    smb_ioctlProcsp[VIOC_DELSYMLINK] = smb_IoctlDeletelink;
    smb_ioctlProcsp[VIOC_MAKESUBMOUNT] = smb_IoctlMakeSubmount;
    smb_ioctlProcsp[VIOC_GETRXKCRYPT] = smb_IoctlGetRxkcrypt;
    smb_ioctlProcsp[VIOC_SETRXKCRYPT] = smb_IoctlSetRxkcrypt;
    smb_ioctlProcsp[VIOC_ISSYMLINK] = smb_IoctlIslink;
    smb_ioctlProcsp[VIOC_TRACEMEMDUMP] = smb_IoctlMemoryDump;
    smb_ioctlProcsp[VIOC_ISSYMLINK] = smb_IoctlIslink;
    smb_ioctlProcsp[VIOC_FLUSHALL] = smb_IoctlFlushAllVolumes;
    smb_ioctlProcsp[VIOCGETFID] = smb_IoctlGetFid;
    smb_ioctlProcsp[VIOCGETOWNER] = smb_IoctlGetOwner;
    smb_ioctlProcsp[VIOC_RXSTAT_PROC] = smb_IoctlRxStatProcess;
    smb_ioctlProcsp[VIOC_RXSTAT_PEER] = smb_IoctlRxStatPeer;
    smb_ioctlProcsp[VIOC_UUIDCTL] = smb_IoctlUUIDControl;
    smb_ioctlProcsp[VIOC_PATH_AVAILABILITY] = smb_IoctlPathAvailability;
    smb_ioctlProcsp[VIOC_GETFILETYPE] = smb_IoctlGetFileType;
    smb_ioctlProcsp[VIOC_VOLSTAT_TEST] = smb_IoctlVolStatTest;
    smb_ioctlProcsp[VIOC_UNICODECTL] = smb_IoctlUnicodeControl;
    smb_ioctlProcsp[VIOC_SETOWNER] = smb_IoctlSetOwner;
    smb_ioctlProcsp[VIOC_SETGROUP] = smb_IoctlSetGroup;
    smb_ioctlProcsp[VIOCNEWCELL2] = smb_IoctlNewCell2;
    smb_ioctlProcsp[VIOC_SETUNIXMODE] = smb_IoctlSetUnixMode;
    smb_ioctlProcsp[VIOC_GETUNIXMODE] = smb_IoctlGetUnixMode;
}

/* called to make a fid structure into an IOCTL fid structure */
void
smb_SetupIoctlFid(smb_fid_t *fidp, cm_space_t *prefix)
{
    smb_ioctl_t *iop;
    cm_space_t *copyPrefix;

    lock_ObtainMutex(&fidp->mx);
    fidp->flags |= SMB_FID_IOCTL;
    fidp->scp = &cm_data.fakeSCache;
    cm_HoldSCache(fidp->scp);
    if (fidp->ioctlp == NULL) {
        iop = malloc(sizeof(*iop));
        memset(iop, 0, sizeof(*iop));
        fidp->ioctlp = iop;
        iop->fidp = fidp;
    }
    if (prefix) {
        copyPrefix = cm_GetSpace();
        memcpy(copyPrefix->data, prefix->data, CM_UTILS_SPACESIZE);
        fidp->ioctlp->prefix = copyPrefix;
    }
    lock_ReleaseMutex(&fidp->mx);
}

/* called when we receive a read call, does the send of the received data if
 * this is the first read call.  This is the function that actually makes the
 * call to the ioctl code.
 */
afs_int32
smb_IoctlPrepareRead(struct smb_fid *fidp, smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    afs_int32 opcode;
    smb_ioctlProc_t *procp = NULL;
    afs_int32 code;

    if (ioctlp->ioctl.flags & CM_IOCTLFLAG_DATAIN) {
        ioctlp->ioctl.flags &= ~CM_IOCTLFLAG_DATAIN;
        ioctlp->ioctl.flags |= CM_IOCTLFLAG_DATAOUT;

        /* do the call now, or fail if we didn't get an opcode, or
         * enough of an opcode.
         */
        if (ioctlp->ioctl.inCopied < sizeof(afs_int32))
            return CM_ERROR_INVAL;
        memcpy(&opcode, ioctlp->ioctl.inDatap, sizeof(afs_int32));
        ioctlp->ioctl.inDatap += sizeof(afs_int32);

        osi_Log1(afsd_logp, "smb_IoctlPrepareRead opcode 0x%x", opcode);
        /* check for opcode out of bounds */
        if (opcode < 0 || opcode >= SMB_IOCTL_MAXPROCS) {
            osi_Log0(afsd_logp, "smb_IoctlPrepareRead - invalid opcode");
            return CM_ERROR_TOOBIG;
        }

        /* check for no such proc */
	procp = smb_ioctlProcsp[opcode];
        if (procp == NULL) {
            osi_Log0(afsd_logp, "smb_IoctlPrepareRead - unassigned opcode");
            return CM_ERROR_INVAL;
        }
        /* otherwise, make the call */
        ioctlp->ioctl.outDatap += sizeof(afs_int32); /* reserve room for return code */
        code = (*procp)(ioctlp, userp, pflags);
        osi_Log1(afsd_logp, "smb_IoctlPrepareRead operation returns code 0x%x", code);

        /* copy in return code */
        memcpy(ioctlp->ioctl.outAllocp, &code, sizeof(afs_int32));
    } else if (!(ioctlp->ioctl.flags & CM_IOCTLFLAG_DATAOUT)) {
        osi_Log0(afsd_logp, "Ioctl invalid state - dataout expected");
        return CM_ERROR_INVAL;
    }

    return 0;
}

/* called when we receive a write call.  If this is the first write call after
 * a series of reads (or the very first call), then we start a new call.
 * We also ensure that things are properly initialized for the start of a call.
 */
void
smb_IoctlPrepareWrite(smb_fid_t *fidp, smb_ioctl_t *ioctlp)
{
    /* make sure the buffer(s) are allocated */
    if (!ioctlp->ioctl.inAllocp)
        ioctlp->ioctl.inAllocp = malloc(SMB_IOCTL_MAXDATA);
    if (!ioctlp->ioctl.outAllocp)
        ioctlp->ioctl.outAllocp = malloc(SMB_IOCTL_MAXDATA);

    /* Fixes fs la problem.  We do a StrToOEM later and if this data isn't initialized we get memory issues. */
    (void) memset(ioctlp->ioctl.inAllocp, 0, SMB_IOCTL_MAXDATA);
    (void) memset(ioctlp->ioctl.outAllocp, 0, SMB_IOCTL_MAXDATA);

    /* and make sure that we've reset our state for the new incoming request */
    if (!(ioctlp->ioctl.flags & CM_IOCTLFLAG_DATAIN)) {
        ioctlp->ioctl.inCopied = 0;
        ioctlp->ioctl.outCopied = 0;
        ioctlp->ioctl.inDatap = ioctlp->ioctl.inAllocp;
        ioctlp->ioctl.outDatap = ioctlp->ioctl.outAllocp;
        ioctlp->ioctl.flags |= CM_IOCTLFLAG_DATAIN;
        ioctlp->ioctl.flags &= ~CM_IOCTLFLAG_DATAOUT;
    }
}

/* called from smb_ReceiveCoreRead when we receive a read on the ioctl fid */
afs_int32
smb_IoctlRead(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    long count;
    afs_int32 leftToCopy;
    char *op;
    afs_int32 code;
    smb_user_t *uidp;
    cm_user_t *userp = NULL;
    smb_t *smbp;
    int isSystem = 0;

    iop = fidp->ioctlp;
    count = smb_GetSMBParm(inp, 1);

    /* Get the user and determine if it is the local machine account */
    smbp = (smb_t *) inp;
    uidp = smb_FindUID(vcp, smbp->uid, 0);
    if (uidp) {
        if (uidp->unp) {
            osi_Log3(afsd_logp, "Ioctl uid %d user %x name %s",
                      uidp->userID, userp,
                      osi_LogSaveClientString(afsd_logp, uidp->unp->name));
        } else {
            osi_Log2(afsd_logp, "Ioctl uid %d user %x no name",
                      uidp->userID, userp);
        }
        isSystem = smb_userIsLocalSystem(uidp);
        userp = smb_GetUserFromUID(uidp);
        if (uidp->unp) {
            osi_Log3(afsd_logp, "smb_IoctlRead uid %d user %x name %s",
                      uidp->userID, userp,
                      osi_LogSaveClientString(afsd_logp, uidp->unp->name));
        } else {
            osi_Log2(afsd_logp, "smb_IoctlRead uid %d user %x no name",
                      uidp->userID, userp);
        }
        smb_ReleaseUID(uidp);
    } else {
        osi_Log1(afsd_logp, "smb_IoctlRead no uid user %x no name", userp);
        return CM_ERROR_BADSMB;
    }

    if (!userp) {
        userp = cm_rootUserp;
        cm_HoldUser(userp);
    }

    /* Identify tree */
    code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &iop->tidPathp);
    if (code) {
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHPATH;
    }

    /* turn the connection around, if required */
    code = smb_IoctlPrepareRead(fidp, iop, userp, isSystem ? AFSCALL_FLAG_LOCAL_SYSTEM : 0);

    if (code) {
        cm_ReleaseUser(userp);
        return code;
    }

    leftToCopy = (afs_int32)((iop->ioctl.outDatap - iop->ioctl.outAllocp) - iop->ioctl.outCopied);
    if (leftToCopy < 0) {
        osi_Log0(afsd_logp, "smb_IoctlRead leftToCopy went negative");
        cm_ReleaseUser(userp);
        return CM_ERROR_INVAL;
    }
    if (count > leftToCopy)
        count = leftToCopy;

    /* now set the parms for a read of count bytes */
    smb_SetSMBParm(outp, 0, count);
    smb_SetSMBParm(outp, 1, 0);
    smb_SetSMBParm(outp, 2, 0);
    smb_SetSMBParm(outp, 3, 0);
    smb_SetSMBParm(outp, 4, 0);

    smb_SetSMBDataLength(outp, count+3);

    op = smb_GetSMBData(outp, NULL);
    *op++ = 1;
    *op++ = (char)(count & 0xff);
    *op++ = (char)((count >> 8) & 0xff);

    /* now copy the data into the response packet */
    memcpy(op, iop->ioctl.outCopied + iop->ioctl.outAllocp, count);

    /* and adjust the counters */
    iop->ioctl.outCopied += count;

    cm_ReleaseUser(userp);

    return 0;
}

/* called from smb_ReceiveCoreWrite when we receive a write call on the IOCTL
 * file descriptor.
 */
afs_int32
smb_IoctlWrite(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    long count;
    afs_int32 code;
    char *op;
    int inDataBlockCount;

    code = 0;
    count = smb_GetSMBParm(inp, 1);
    iop = fidp->ioctlp;

    smb_IoctlPrepareWrite(fidp, iop);

    op = smb_GetSMBData(inp, NULL);
    op = smb_ParseDataBlock(op, NULL, &inDataBlockCount);

    if (count + iop->ioctl.inCopied > SMB_IOCTL_MAXDATA) {
        code = CM_ERROR_TOOBIG;
        goto done;
    }

    /* copy data */
    memcpy(iop->ioctl.inDatap + iop->ioctl.inCopied, op, count);

    /* adjust counts */
    iop->ioctl.inCopied += count;

  done:
    /* return # of bytes written */
    if (code == 0) {
        smb_SetSMBParm(outp, 0, count);
        smb_SetSMBDataLength(outp, 0);
    }

    return code;
}

/* called from smb_ReceiveV3WriteX when we receive a write call on the IOCTL
 * file descriptor.
 */
afs_int32
smb_IoctlV3Write(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    long count;
    afs_int32 code;
    char *op;
    int inDataBlockCount;

    code = 0;
    count = smb_GetSMBParm(inp, 10);
    iop = fidp->ioctlp;

    smb_IoctlPrepareWrite(fidp, iop);

    op = inp->data + smb_GetSMBParm(inp, 11);
    inDataBlockCount = count;

    if (count + iop->ioctl.inCopied > SMB_IOCTL_MAXDATA) {
        code = CM_ERROR_TOOBIG;
        goto done;
    }

    /* copy data */
    memcpy(iop->ioctl.inDatap + iop->ioctl.inCopied, op, count);

    /* adjust counts */
    iop->ioctl.inCopied += count;

  done:
    /* return # of bytes written */
    if (code == 0) {
        smb_SetSMBParm(outp, 2, count);
        smb_SetSMBParm(outp, 3, 0); /* reserved */
        smb_SetSMBParm(outp, 4, 0); /* reserved */
        smb_SetSMBParm(outp, 5, 0); /* reserved */
        smb_SetSMBDataLength(outp, 0);
    }

    return code;
}


/* called from V3 read to handle IOCTL descriptor reads */
afs_int32
smb_IoctlV3Read(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    unsigned short count;
    afs_int32 code;
    long leftToCopy;
    char *op;
    cm_user_t *userp;
    smb_user_t *uidp;
    int isSystem = 0;

    iop = fidp->ioctlp;
    count = smb_GetSMBParm(inp, 5);

    /* Get the user and determine if it is the local machine account */
    uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
    if (uidp) {
        isSystem = smb_userIsLocalSystem(uidp);
        userp = smb_GetUserFromUID(uidp);
        if (uidp->unp) {
            osi_Log3(afsd_logp, "smb_IoctlV3Read uid %d user %x name %s",
                      uidp->userID, userp,
                      osi_LogSaveClientString(afsd_logp, uidp->unp->name));
        } else {
            osi_Log2(afsd_logp, "smb_IoctlV3Read uid %d user %x no name",
                      uidp->userID, userp);
        }
     } else {
         osi_Log0(afsd_logp, "smb_IoctlV3Read no uid");
         return CM_ERROR_BADSMB;
     }

    if (!userp) {
        userp = cm_rootUserp;
        cm_HoldUser(userp);
    }

    iop->uidp = uidp;

    code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &iop->tidPathp);
    if (code) {
	if (uidp)
	    smb_ReleaseUID(uidp);
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHPATH;
    }

    code = smb_IoctlPrepareRead(fidp, iop, userp, isSystem ? AFSCALL_FLAG_LOCAL_SYSTEM : 0);
    if (uidp) {
        iop->uidp = 0;
        smb_ReleaseUID(uidp);
    }
    if (code) {
	cm_ReleaseUser(userp);
	return code;
    }

    leftToCopy = (long)((iop->ioctl.outDatap - iop->ioctl.outAllocp) - iop->ioctl.outCopied);
    if (leftToCopy < 0) {
        osi_Log0(afsd_logp, "smb_IoctlV3Read leftToCopy went negative");
        cm_ReleaseUser(userp);
        return CM_ERROR_INVAL;
    }
    if (count > leftToCopy)
        count = (unsigned short)leftToCopy;

    /* 0 and 1 are reserved for request chaining, were setup by our caller,
     * and will be further filled in after we return.
     */
    smb_SetSMBParm(outp, 2, 0);	/* remaining bytes, for pipes */
    smb_SetSMBParm(outp, 3, 0);	/* resvd */
    smb_SetSMBParm(outp, 4, 0);	/* resvd */
    smb_SetSMBParm(outp, 5, count);	/* # of bytes we're going to read */
    /* fill in #6 when we have all the parameters' space reserved */
    smb_SetSMBParm(outp, 7, 0);	/* resv'd */
    smb_SetSMBParm(outp, 8, 0);	/* resv'd */
    smb_SetSMBParm(outp, 9, 0);	/* resv'd */
    smb_SetSMBParm(outp, 10, 0);	/* resv'd */
    smb_SetSMBParm(outp, 11, 0);	/* reserved */

    /* get op ptr after putting in the last parm, since otherwise we don't
     * know where the data really is.
     */
    op = smb_GetSMBData(outp, NULL);

    /* now fill in offset from start of SMB header to first data byte (to op) */
    smb_SetSMBParm(outp, 6, ((int) (op - outp->data)));

    /* set the packet data length the count of the # of bytes */
    smb_SetSMBDataLength(outp, count);

    /* now copy the data into the response packet */
    memcpy(op, iop->ioctl.outCopied + iop->ioctl.outAllocp, count);

    /* and adjust the counters */
    iop->ioctl.outCopied += count;

    /* and cleanup things */
    cm_ReleaseUser(userp);

    return 0;
}

/* called from Read Raw to handle IOCTL descriptor reads */
afs_int32
smb_IoctlReadRaw(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp,
                 smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    long leftToCopy;
    NCB *ncbp;
    afs_int32 code;
    cm_user_t *userp;
    smb_user_t *uidp;
    int isSystem = 0;

    iop = fidp->ioctlp;

    /* Get the user and determine if it is the local machine account */
    uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
    if (uidp) {
        isSystem = smb_userIsLocalSystem(uidp);
        userp = smb_GetUserFromUID(uidp);
        if (uidp->unp) {
            osi_Log3(afsd_logp, "smb_IoctlRawRead uid %d user %x name %s",
                      uidp->userID, userp,
                      osi_LogSaveClientString(afsd_logp, uidp->unp->name));
        } else {
            osi_Log2(afsd_logp, "smb_IoctlRawRead uid %d user %x no name",
                      uidp->userID, userp);
        }
        smb_ReleaseUID(uidp);
    } else {
        osi_Log0(afsd_logp, "smb_IoctlRawRead no uid");
    }

    if (!userp) {
        userp = cm_rootUserp;
        cm_HoldUser(userp);
    }

    code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &iop->tidPathp);
    if (code) {
        code = CM_ERROR_NOSUCHPATH;
        goto done;
    }

    code = smb_IoctlPrepareRead(fidp, iop, userp, isSystem ? AFSCALL_FLAG_LOCAL_SYSTEM : 0);
    if (code) {
        goto done;
    }

    leftToCopy = (long)((iop->ioctl.outDatap - iop->ioctl.outAllocp) - iop->ioctl.outCopied);
    if (leftToCopy < 0) {
        osi_Log0(afsd_logp, "smb_IoctlReadRaw leftToCopy went negative");
        code = CM_ERROR_INVAL;
        goto done;
    }

    ncbp = outp->ncbp;
    memset(ncbp, 0, sizeof(NCB));

    ncbp->ncb_length = (unsigned short) leftToCopy;
    ncbp->ncb_lsn = (unsigned char) vcp->lsn;
    ncbp->ncb_command = NCBSEND;
    /*ncbp->ncb_lana_num = smb_LANadapter;*/
    ncbp->ncb_lana_num = vcp->lana;

    ncbp->ncb_buffer = iop->ioctl.outCopied + iop->ioctl.outAllocp;
    code = Netbios(ncbp);

    if (code != 0)
	osi_Log1(afsd_logp, "ReadRaw send failure code %d", code);

  done:
    cm_ReleaseUser(userp);

    return code;
}

/* parse the passed-in file name and do a namei on it.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
#define CM_PARSE_FLAG_LITERAL 1

afs_int32
smb_ParseIoctlPath(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                   cm_scache_t **scpp, afs_uint32 flags)
{
    long code;
    cm_scache_t  *substRootp = NULL;
    cm_scache_t  *iscp = NULL;
    char      *inPath;
    clientchar_t *relativePath = NULL;
    clientchar_t *lastComponent = NULL;
    afs_uint32 follow = (flags & CM_PARSE_FLAG_LITERAL ? CM_FLAG_NOMOUNTCHASE : CM_FLAG_FOLLOW);

    inPath = ioctlp->ioctl.inDatap;
    /* setup the next data value for the caller to use */
    ioctlp->ioctl.inDatap += (long)strlen(ioctlp->ioctl.inDatap) + 1;

    osi_Log1(afsd_logp, "cm_ParseIoctlPath %s", osi_LogSaveString(afsd_logp,inPath));

    /* This is usually the file name, but for StatMountPoint it is the path. */
    /* ioctlp->ioctl.inDatap can be either of the form:
     *    \path\.
     *    \path\file
     *    \\netbios-name\submount\path\.
     *    \\netbios-name\submount\path\file
     */

    /* We do not perform path name translation on the ioctl path data
     * because these paths were not translated by Windows through the
     * file system API.  Therefore, they are not OEM characters but
     * whatever the display character set is.
     */

    // TranslateExtendedChars(relativePath);

    /* This is usually nothing, but for StatMountPoint it is the file name. */
    // TranslateExtendedChars(ioctlp->ioctl.inDatap);

    /* If the string starts with our UTF-8 prefix (which is the
       sequence [ESC,'%','G'] as used by ISO-2022 to designate UTF-8
       strings), we assume that the provided path is UTF-8.  Otherwise
       we have to convert the string to UTF-8, since that is what we
       want to use everywhere else.*/

    if (memcmp(inPath, utf8_prefix, utf8_prefix_size) == 0) {
        /* String is UTF-8 */
        inPath += utf8_prefix_size;
        ioctlp->ioctl.flags |= CM_IOCTLFLAG_USEUTF8;

        relativePath = cm_Utf8ToClientStringAlloc(inPath, -1, NULL);
    } else {
        int cch;

        /* Not a UTF-8 string */
        /* TODO: If this is an OEM string, we should convert it to
           UTF-8. */
        if (smb_StoreAnsiFilenames) {
            cch = cm_AnsiToClientString(inPath, -1, NULL, 0);
#ifdef DEBUG
            osi_assert(cch > 0);
#endif
            relativePath = malloc(cch * sizeof(clientchar_t));
            cm_AnsiToClientString(inPath, -1, relativePath, cch);
        } else {
            TranslateExtendedChars(inPath);

            cch = cm_OemToClientString(inPath, -1, NULL, 0);
#ifdef DEBUG
            osi_assert(cch > 0);
#endif
            relativePath = malloc(cch * sizeof(clientchar_t));
            cm_OemToClientString(inPath, -1, relativePath, cch);
        }
    }

    if (relativePath[0] == relativePath[1] &&
        relativePath[1] == '\\' &&
        !cm_ClientStrCmpNI(cm_NetbiosNameC, relativePath+2,
                           (int)cm_ClientStrLen(cm_NetbiosNameC)))
    {
        clientchar_t shareName[256];
        clientchar_t *sharePath;
        int shareFound, i;

        /* We may have found a UNC path.
         * If the first component is the NetbiosName,
         * then throw out the second component (the submount)
         * since it had better expand into the value of ioctl->tidPathp
         */
        clientchar_t * p;
        p = relativePath + 2 + cm_ClientStrLen(cm_NetbiosNameC) + 1;			/* buffer overflow vuln.? */
        if ( !cm_ClientStrCmpNI(_C("all"),  p,  3) )
            p += 4;

        for (i = 0; *p && *p != '\\'; i++,p++ ) {
            shareName[i] = *p;
        }
        p++;                    /* skip past trailing slash */
        shareName[i] = 0;       /* terminate string */

        shareFound = smb_FindShare(ioctlp->fidp->vcp, ioctlp->uidp, shareName, &sharePath);
        if ( shareFound ) {
            /* we found a sharename, therefore use the resulting path */
            code = cm_NameI(cm_RootSCachep(userp, reqp), ioctlp->prefix->wdata,
                            CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, sharePath, reqp, &substRootp);
            free(sharePath);
            if (code) {
		osi_Log1(afsd_logp,"cm_ParseIoctlPath [1] code 0x%x", code);
                if (relativePath)
                    free(relativePath);
                return code;
	    }

	    lastComponent = cm_ClientStrRChr(p,  '\\');
	    if (lastComponent && (lastComponent - p) > 1 &&
                cm_ClientStrLen(lastComponent) > 1) {
		*lastComponent = '\0';
		lastComponent++;

		code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                                userp, NULL, reqp, &iscp);
		if (code == 0)
		    code = cm_NameI(iscp, lastComponent, CM_FLAG_CASEFOLD | follow,
				    userp, NULL, reqp, scpp);
		if (iscp)
		    cm_ReleaseSCache(iscp);
	    } else {
		code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD,
				userp, NULL, reqp, scpp);
	    }
	    cm_ReleaseSCache(substRootp);
            if (code) {
		osi_Log1(afsd_logp,"cm_ParseIoctlPath [2] code 0x%x", code);
                if (relativePath)
                    free(relativePath);
                return code;
	    }
        } else {
            /* otherwise, treat the name as a cellname mounted off the afs root.
             * This requires that we reconstruct the shareName string with
             * leading and trailing slashes.
             */
            p = relativePath + 2 + cm_ClientStrLen(cm_NetbiosNameC) + 1;
            if ( !cm_ClientStrCmpNI(_C("all"),  p,  3) )
                p += 4;

            shareName[0] = '/';
            for (i = 1; *p && *p != '\\'; i++,p++ ) {
                shareName[i] = *p;
            }
            p++;                    /* skip past trailing slash */
            shareName[i++] = '/';	/* add trailing slash */
            shareName[i] = 0;       /* terminate string */


            code = cm_NameI(cm_RootSCachep(userp, reqp), ioctlp->prefix->wdata,
                            CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, shareName, reqp, &substRootp);
            if (code) {
		osi_Log1(afsd_logp,"cm_ParseIoctlPath [3] code 0x%x", code);
                if (relativePath)
                    free(relativePath);
                return code;
	    }

	    lastComponent = cm_ClientStrRChr(p,  '\\');
	    if (lastComponent && (lastComponent - p) > 1 &&
                cm_ClientStrLen(lastComponent) > 1) {
		*lastComponent = '\0';
		lastComponent++;

		code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                                userp, NULL, reqp, &iscp);
		if (code == 0)
		    code = cm_NameI(iscp, lastComponent, CM_FLAG_CASEFOLD | follow,
				    userp, NULL, reqp, scpp);
		if (iscp)
		    cm_ReleaseSCache(iscp);
	    } else {
		code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD,
				userp, NULL, reqp, scpp);
	    }

	    if (code) {
		cm_ReleaseSCache(substRootp);
		osi_Log1(afsd_logp,"cm_ParseIoctlPath code [4] 0x%x", code);
                if (relativePath)
                    free(relativePath);
                return code;
	    }
        }
    } else {
        code = cm_NameI(cm_RootSCachep(userp, reqp), ioctlp->prefix->wdata,
                         CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                         userp, ioctlp->tidPathp, reqp, &substRootp);
        if (code) {
	    osi_Log1(afsd_logp,"cm_ParseIoctlPath [6] code 0x%x", code);
            if (relativePath)
                free(relativePath);
            return code;
	}

	lastComponent = cm_ClientStrRChr(relativePath,  '\\');
	if (lastComponent && (lastComponent - relativePath) > 1 &&
            cm_ClientStrLen(lastComponent) > 1) {
	    *lastComponent = '\0';
	    lastComponent++;

	    code = cm_NameI(substRootp, relativePath, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, NULL, reqp, &iscp);
	    if (code == 0)
		code = cm_NameI(iscp, lastComponent, CM_FLAG_CASEFOLD | follow,
                                userp, NULL, reqp, scpp);
	    if (iscp)
		cm_ReleaseSCache(iscp);
	} else {
	    code = cm_NameI(substRootp, relativePath, CM_FLAG_CASEFOLD | follow,
                            userp, NULL, reqp, scpp);
	}
        if (code) {
	    cm_ReleaseSCache(substRootp);
	    osi_Log1(afsd_logp,"cm_ParseIoctlPath [7] code 0x%x", code);
            if (relativePath)
                free(relativePath);
            return code;
	}
    }

    if (substRootp)
	cm_ReleaseSCache(substRootp);

    if (relativePath)
        free(relativePath);

    /* Ensure that the status object is up to date */
    lock_ObtainWrite(&(*scpp)->rw);
    code = cm_SyncOp( *scpp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone( *scpp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&(*scpp)->rw);

    /* and return success */
    osi_Log1(afsd_logp,"cm_ParseIoctlPath [8] code 0x%x", code);
    return 0;
}



#define LEAF_SIZE 256
/* parse the passed-in file name and do a namei on its parent.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
afs_int32
smb_ParseIoctlParent(smb_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                     cm_scache_t **scpp, clientchar_t *leafp)
{
    long code;
    clientchar_t tbuffer[1024];
    clientchar_t *tp, *jp;
    cm_scache_t *substRootp = NULL;
    clientchar_t *inpathp = NULL;
    char *inpathdatap;

    inpathdatap = ioctlp->ioctl.inDatap;

    /* If the string starts with our UTF-8 prefix (which is the
       sequence [ESC,'%','G'] as used by ISO-2022 to designate UTF-8
       strings), we assume that the provided path is UTF-8.  Otherwise
       we have to convert the string to UTF-8, since that is what we
       want to use everywhere else.*/

    if (memcmp(inpathdatap, utf8_prefix, utf8_prefix_size) == 0) {

        /* String is UTF-8 */
        inpathdatap += utf8_prefix_size;
        ioctlp->ioctl.flags |= CM_IOCTLFLAG_USEUTF8;

        inpathp = cm_Utf8ToClientStringAlloc(inpathdatap, -1, NULL);
    } else {
        int cch;

        /* Not a UTF-8 string */
        /* TODO: If this is an OEM string, we should convert it to
           UTF-8. */
        if (smb_StoreAnsiFilenames) {
            cch = cm_AnsiToClientString(inpathdatap, -1, NULL, 0);
#ifdef DEBUG
            osi_assert(cch > 0);
#endif
            inpathp = malloc(cch * sizeof(clientchar_t));
            cm_AnsiToClientString(inpathdatap, -1, inpathp, cch);
        } else {
            TranslateExtendedChars(inpathdatap);

            cch = cm_OemToClientString(inpathdatap, -1, NULL, 0);
#ifdef DEBUG
            osi_assert(cch > 0);
#endif
            inpathp = malloc(cch * sizeof(clientchar_t));
            cm_OemToClientString(inpathdatap, -1, inpathp, cch);
        }
    }

    cm_ClientStrCpy(tbuffer, lengthof(tbuffer), inpathp);
    tp = cm_ClientStrRChr(tbuffer, '\\');
    jp = cm_ClientStrRChr(tbuffer, '/');
    if (!tp)
        tp = jp;
    else if (jp && (tp - tbuffer) < (jp - tbuffer))
        tp = jp;
    if (!tp) {
        cm_ClientStrCpy(tbuffer, lengthof(tbuffer), _C("\\"));
        if (leafp)
            cm_ClientStrCpy(leafp, LEAF_SIZE, inpathp);
    }
    else {
        *tp = 0;
        if (leafp)
            cm_ClientStrCpy(leafp, LEAF_SIZE, tp+1);
    }

    free(inpathp);
    inpathp = NULL;             /* We don't need this from this point on */

    if (tbuffer[0] == tbuffer[1] &&
        tbuffer[1] == '\\' &&
        !cm_ClientStrCmpNI(cm_NetbiosNameC, tbuffer+2,
                           (int)cm_ClientStrLen(cm_NetbiosNameC)))
    {
        clientchar_t shareName[256];
        clientchar_t *sharePath;
        int shareFound, i;

        /* We may have found a UNC path.
         * If the first component is the NetbiosName,
         * then throw out the second component (the submount)
         * since it had better expand into the value of ioctl->tidPathp
         */
        clientchar_t * p;
        p = tbuffer + 2 + cm_ClientStrLen(cm_NetbiosNameC) + 1;
        if ( !cm_ClientStrCmpNI(_C("all"),  p,  3) )
            p += 4;

        for (i = 0; *p && *p != '\\'; i++,p++ ) {
            shareName[i] = *p;
        }
        p++;                    /* skip past trailing slash */
        shareName[i] = 0;       /* terminate string */

        shareFound = smb_FindShare(ioctlp->fidp->vcp, ioctlp->uidp, shareName, &sharePath);
        if ( shareFound ) {
            /* we found a sharename, therefore use the resulting path */
            code = cm_NameI(cm_RootSCachep(userp, reqp), ioctlp->prefix->wdata,
                            CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, sharePath, reqp, &substRootp);
            free(sharePath);
            if (code) return code;

            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, NULL, reqp, scpp);
	    cm_ReleaseSCache(substRootp);
            if (code) return code;
        } else {
            /* otherwise, treat the name as a cellname mounted off the afs root.
             * This requires that we reconstruct the shareName string with
             * leading and trailing slashes.
             */
            p = tbuffer + 2 + cm_ClientStrLen(cm_NetbiosNameC) + 1;
            if ( !cm_ClientStrCmpNI(_C("all"),  p,  3) )
                p += 4;

            shareName[0] = '/';
            for (i = 1; *p && *p != '\\'; i++,p++ ) {
                shareName[i] = *p;
            }
            p++;                    /* skip past trailing slash */
            shareName[i++] = '/';	/* add trailing slash */
            shareName[i] = 0;       /* terminate string */

            code = cm_NameI(cm_RootSCachep(userp, reqp), ioctlp->prefix->wdata,
                            CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, shareName, reqp, &substRootp);
            if (code) return code;

            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, NULL, reqp, scpp);
	    cm_ReleaseSCache(substRootp);
            if (code) return code;
        }
    } else {
        code = cm_NameI(cm_RootSCachep(userp, reqp), ioctlp->prefix->wdata,
                        CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                        userp, ioctlp->tidPathp, reqp, &substRootp);
        if (code) return code;

        code = cm_NameI(substRootp, tbuffer, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                        userp, NULL, reqp, scpp);
	cm_ReleaseSCache(substRootp);
        if (code) return code;
    }

    /* # of bytes of path */
    code = (long)strlen(ioctlp->ioctl.inDatap) + 1;
    ioctlp->ioctl.inDatap += code;

    /* Ensure that the status object is up to date */
    lock_ObtainWrite(&(*scpp)->rw);
    code = cm_SyncOp( *scpp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone( *scpp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&(*scpp)->rw);

    /* and return success */
    return 0;
}

afs_int32
smb_IoctlSetToken(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    char *saveDataPtr;
    char *tp;
    int ticketLen;
    char *ticket;
    int ctSize;
    struct ClearToken ct;
    cm_cell_t *cellp;
    cm_ucell_t *ucellp;
    afs_uuid_t uuid;
    int flags;
    char sessionKey[8];
    int release_userp = 0;
    clientchar_t *uname = NULL;
    clientchar_t *smbname = NULL;
    clientchar_t *wdir = NULL;
    clientchar_t *rpc_sid = NULL;
    afs_int32 code = 0;

    saveDataPtr = ioctlp->ioctl.inDatap;

    cm_SkipIoctlPath(&ioctlp->ioctl);

    tp = ioctlp->ioctl.inDatap;

    /* ticket length */
    memcpy(&ticketLen, tp, sizeof(ticketLen));
    tp += sizeof(ticketLen);
    if (ticketLen < MINKTCTICKETLEN || ticketLen > MAXKTCTICKETLEN)
        return CM_ERROR_INVAL;

    /* remember ticket and skip over it for now */
    ticket = tp;
    tp += ticketLen;

    /* clear token size */
    memcpy(&ctSize, tp, sizeof(ctSize));
    tp += sizeof(ctSize);
    if (ctSize != sizeof(struct ClearToken))
        return CM_ERROR_INVAL;

    /* clear token */
    memcpy(&ct, tp, ctSize);
    tp += ctSize;
    if (ct.AuthHandle == -1)
        ct.AuthHandle = 999;	/* more rxvab compat stuff */

    /* more stuff, if any */
    if (ioctlp->ioctl.inCopied > tp - saveDataPtr) {
        /* flags:  logon flag */
        memcpy(&flags, tp, sizeof(int));
        tp += sizeof(int);

        /* cell name */
        {
            fschar_t * cellnamep;
            clientchar_t * temp;

            temp = cm_ParseIoctlStringAlloc(&ioctlp->ioctl, tp);
            cellnamep = cm_ClientStringToFsStringAlloc(temp, -1, NULL);
            cellp = cm_GetCell(cellnamep, CM_FLAG_CREATE | CM_FLAG_NOPROBE);
            free(cellnamep);
            free(temp);
        }

        if (!cellp) {
            code = CM_ERROR_NOSUCHCELL;
            goto done;
        }
        tp += strlen(tp) + 1;

        /* user name */
        uname = cm_ParseIoctlStringAlloc(&ioctlp->ioctl, tp);
        tp += strlen(tp) + 1;

        if (flags & PIOCTL_LOGON) {
            /* SMB user name with which to associate tokens */
            smbname = cm_ParseIoctlStringAlloc(&ioctlp->ioctl, tp);
            osi_Log2(smb_logp,"smb_IoctlSetToken for user [%S] smbname [%S]",
                     osi_LogSaveClientString(smb_logp,uname),
                     osi_LogSaveClientString(smb_logp,smbname));
            fprintf(stderr, "SMB name = %S\n", smbname);
            tp += strlen(tp) + 1;
        } else {
            osi_Log1(smb_logp,"smb_IoctlSetToken for user [%S]",
                     osi_LogSaveClientString(smb_logp, uname));
        }

        /* uuid */
        memcpy(&uuid, tp, sizeof(uuid));
        if (!cm_FindTokenEvent(uuid, sessionKey, &rpc_sid)) {
            code = CM_ERROR_INVAL;
            goto done;
        }

        if (rpc_sid) {
            if (!(pflags & AFSCALL_FLAG_LOCAL_SYSTEM)) {
                osi_Log1(smb_logp,"smb_IoctlSetToken Rpc Sid [%S]",
                         osi_LogSaveClientString(smb_logp, rpc_sid));
                if (!cm_ClientStrCmp(NTSID_LOCAL_SYSTEM, rpc_sid))
                    pflags |= AFSCALL_FLAG_LOCAL_SYSTEM;
            }
            LocalFree(rpc_sid);
        }

        if (!(pflags & AFSCALL_FLAG_LOCAL_SYSTEM) && (flags & PIOCTL_LOGON)) {
            code = CM_ERROR_NOACCESS;
            goto done;
        }
    } else {
        cellp = cm_data.rootCellp;
        osi_Log0(smb_logp,"smb_IoctlSetToken - no name specified");
    }

    if ((pflags & AFSCALL_FLAG_LOCAL_SYSTEM) && (flags & PIOCTL_LOGON)) {
        PSID pSid = NULL;
        DWORD dwSize1 = 0, dwSize2 = 0;
        wchar_t *pszRefDomain = NULL;
        SID_NAME_USE snu = SidTypeGroup;
        clientchar_t * secSidString = NULL;
        DWORD gle;

        /*
         * The specified smbname is may not be a SID for the user.
         * See if we can obtain the SID for the specified name.
         * If we can, use that instead of the name provided.
         */

        LookupAccountNameW( NULL /* System Name to begin Search */,
                            smbname,
                            NULL, &dwSize1,
                            NULL, &dwSize2,
                            &snu);
        gle = GetLastError();
        if (gle == ERROR_INSUFFICIENT_BUFFER) {
            pSid = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, dwSize1);
            /*
             * Although dwSize2 is supposed to include the terminating
             * NUL character, on Win7 it does not.
             */
            pszRefDomain = malloc((dwSize2 + 1) * sizeof(wchar_t));
        }

        if ( pSid && pszRefDomain ) {
            if (LookupAccountNameW( NULL /* System Name to begin Search */,
                                    smbname,
                                    pSid, &dwSize1,
                                    pszRefDomain, &dwSize2,
                                    &snu))
                ConvertSidToStringSidW(pSid, &secSidString);
        }

        if (secSidString) {
            userp = smb_FindCMUserBySID( secSidString, ioctlp->fidp->vcp->rname,
                                         SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
            LocalFree(secSidString);
        } else {
            /* Free the SID so we can reuse the variable */
            if (pSid) {
                LocalFree(pSid);
                pSid = NULL;
            }

            /*
             * If the SID for the name could not be found,
             * perhaps it already is a SID
             */
            if (!ConvertStringSidToSidW( smbname, &pSid)) {
                userp = smb_FindCMUserBySID( smbname, ioctlp->fidp->vcp->rname,
                                             SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
            } else {
                userp = smb_FindCMUserByName( smbname, ioctlp->fidp->vcp->rname,
                                              SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
            }
        }

        if (pSid)
            LocalFree(pSid);
        if (pszRefDomain)
            free(pszRefDomain);

        release_userp = 1;
    }

    /* store the token */
    lock_ObtainMutex(&userp->mx);
    ucellp = cm_GetUCell(userp, cellp);
    osi_Log1(smb_logp,"smb_IoctlSetToken ucellp %lx", ucellp);
    ucellp->ticketLen = ticketLen;
    if (ucellp->ticketp)
        free(ucellp->ticketp);	/* Discard old token if any */
    ucellp->ticketp = malloc(ticketLen);
    memcpy(ucellp->ticketp, ticket, ticketLen);
    /*
     * Get the session key from the RPC, rather than from the pioctl.
     */
    /*
    memcpy(&ucellp->sessionKey, ct.HandShakeKey, sizeof(ct.HandShakeKey));
    */
    memcpy(ucellp->sessionKey.data, sessionKey, sizeof(sessionKey));
    ucellp->kvno = ct.AuthHandle;
    ucellp->expirationTime = ct.EndTimestamp;
    ucellp->gen++;
#ifdef QUERY_AFSID
    ucellp->uid = ANONYMOUSID;
#endif
    if (uname) {
        cm_ClientStringToFsString(uname, -1, ucellp->userName, MAXKTCNAMELEN);
#ifdef QUERY_AFSID
	cm_UsernameToId(uname, ucellp, &ucellp->uid);
#endif
    }
    _InterlockedOr(&ucellp->flags, CM_UCELLFLAG_RXKAD);
    lock_ReleaseMutex(&userp->mx);

    if ((pflags & AFSCALL_FLAG_LOCAL_SYSTEM) && (flags & PIOCTL_LOGON)) {
        ioctlp->ioctl.flags |= CM_IOCTLFLAG_LOGON;
    }

    cm_ResetACLCache(cellp, userp);

  done:
    SecureZeroMemory(sessionKey, sizeof(sessionKey));

    if (release_userp)
	cm_ReleaseUser(userp);

    if (uname)
        free(uname);

    if (smbname)
        free(smbname);

    return code;
}



afs_int32
smb_IoctlGetSMBName(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    smb_user_t *uidp = ioctlp->uidp;

    if (uidp && uidp->unp) {
        int cch;

        cch = cm_ClientStringToUtf8(uidp->unp->name,
                                    -1,
                                    ioctlp->ioctl.outDatap,
                                    (int)(SMB_IOCTL_MAXDATA -
                                     (ioctlp->ioctl.outDatap - ioctlp->ioctl.outAllocp))
                                    / sizeof(cm_utf8char_t));

        ioctlp->ioctl.outDatap += cch * sizeof(cm_utf8char_t);
    }

    return 0;
}

afs_int32
smb_IoctlGetACL(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    cm_scache_t *scp;
    afs_int32 code;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }

    if (code)
        return code;

    code = cm_IoctlGetACL(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
smb_IoctlSetACL(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    cm_scache_t *scp;
    afs_int32 code;
    cm_req_t req;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    if (code)
        return code;

    code = cm_IoctlSetACL(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
smb_IoctlGetFileCellName(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetFileCellName(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
smb_IoctlFlushAllVolumes(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_req_t req;

    smb_InitReq(&req);

    cm_SkipIoctlPath(&ioctlp->ioctl);	/* we don't care about the path */

    return cm_IoctlFlushAllVolumes(&ioctlp->ioctl, userp, &req);
}

afs_int32
smb_IoctlFlushVolume(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlFlushVolume(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
smb_IoctlFlushFile(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
	cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlFlushFile(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
smb_IoctlSetVolumeStatus(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, 0);
    if (code) return code;

    code = cm_IoctlSetVolumeStatus(&ioctlp->ioctl, userp, scp, &req);
    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
smb_IoctlGetVolumeStatus(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;
    cm_req_t req;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetVolumeStatus(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
smb_IoctlGetFid(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t * optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    if (code)
        return code;

    code = cm_IoctlGetFid(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
smb_IoctlGetFileType(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t * optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetFileType(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
smb_IoctlGetOwner(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetOwner(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
smb_IoctlWhereIs(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlWhereIs(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}


afs_int32
smb_IoctlStatMountPoint(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &dscp, 0);
    if (code)
        return code;

    code = cm_IoctlStatMountPoint(&ioctlp->ioctl, userp, dscp, &req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
smb_IoctlDeleteMountPoint(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &dscp, 0);
    if (code)
        return code;

    code = cm_IoctlDeleteMountPoint(&ioctlp->ioctl, userp, dscp, &req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
smb_IoctlCheckServers(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);	/* we don't care about the path */

    return cm_IoctlCheckServers(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGag(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    /* we don't print anything superfluous, so we don't support the gag call */
    return CM_ERROR_INVAL;
}

afs_int32
smb_IoctlCheckVolumes(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlCheckVolumes(&ioctlp->ioctl, userp);
}

afs_int32 smb_IoctlSetCacheSize(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetCacheSize(&ioctlp->ioctl, userp);
}


afs_int32
smb_IoctlTraceControl(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlTraceControl(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGetCacheParms(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetCacheParms(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGetCell(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetCell(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlNewCell(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlNewCell(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlNewCell2(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlNewCell2(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGetWsCell(smb_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetWsCell(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlSysName(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSysName(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGetCellStatus(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetCellStatus(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlSetCellStatus(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetCellStatus(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlSetSPrefs(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetSPrefs(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGetSPrefs(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetSPrefs(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlStoreBehind(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    /* we ignore default asynchrony since we only have one way
     * of doing this today.
     */
    return 0;
}

afs_int32
smb_IoctlCreateMountPoint(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    clientchar_t leaf[LEAF_SIZE];
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlParent(ioctlp, userp, &req, &dscp, leaf);
    if (code)
        return code;

    code = cm_IoctlCreateMountPoint(&ioctlp->ioctl, userp, dscp, &req, leaf);

    cm_ReleaseSCache(dscp);
    return code;
}

afs_int32
smb_IoctlSymlink(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    clientchar_t leaf[LEAF_SIZE];
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlParent(ioctlp, userp, &req, &dscp, leaf);
    if (code) return code;

    code = cm_IoctlSymlink(&ioctlp->ioctl, userp, dscp, &req, leaf);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
smb_IoctlListlink(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &dscp, 0);
    if (code) return code;

    code = cm_IoctlListlink(&ioctlp->ioctl, userp, dscp, &req);

    cm_ReleaseSCache(dscp);
    return code;
}

afs_int32
smb_IoctlIslink(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{/*CHECK FOR VALID SYMLINK*/
    afs_int32 code;
    cm_scache_t *dscp;
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &dscp, 0);
    if (code) return code;

    code = cm_IoctlIslink(&ioctlp->ioctl, userp, dscp, &req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
smb_IoctlDeletelink(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    cm_req_t req;

    smb_InitReq(&req);

    code = smb_ParseIoctlPath(ioctlp, userp, &req, &dscp, 0);
    if (code) return code;

    code = cm_IoctlDeletelink(&ioctlp->ioctl, userp, dscp, &req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
smb_IoctlGetTokenIter(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetTokenIter(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGetToken(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetToken(&ioctlp->ioctl, userp);
}


afs_int32
smb_IoctlDelToken(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlDelToken(&ioctlp->ioctl, userp);
}


afs_int32
smb_IoctlDelAllToken(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlDelAllToken(&ioctlp->ioctl, userp);
}


afs_int32
smb_IoctlMakeSubmount(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlMakeSubmount(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlGetRxkcrypt(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetRxkcrypt(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlSetRxkcrypt(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetRxkcrypt(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlRxStatProcess(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlRxStatProcess(&ioctlp->ioctl, userp);
}


afs_int32
smb_IoctlRxStatPeer(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlRxStatPeer(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlUnicodeControl(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlUnicodeControl(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlUUIDControl(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlUUIDControl(&ioctlp->ioctl, userp);
}


afs_int32
smb_IoctlMemoryDump(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlMemoryDump(&ioctlp->ioctl, userp);
}

afs_int32
smb_IoctlPathAvailability(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlPathAvailability(&ioctlp->ioctl, userp, scp, &req);
    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
smb_IoctlVolStatTest(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_req_t req;

    smb_InitReq(&req);

    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlVolStatTest(&ioctlp->ioctl, userp, &req);
}

/*
 * VIOC_SETOWNER
 *
 * This pioctl requires the use of the cm_ioctlQueryOptions_t structure.
 *
 */
afs_int32
smb_IoctlSetOwner(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp) {
        if (CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
            flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

        if (CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
            cm_fid_t fid;
            cm_SkipIoctlPath(&ioctlp->ioctl);
            cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                       optionsp->fid.vnode, optionsp->fid.unique);
            code = cm_GetSCache(&fid, &scp, userp, &req);
        } else {
            code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
        }
        if (code)
            return code;

        cm_IoctlSkipQueryOptions(&ioctlp->ioctl, userp);
    }

    code = cm_IoctlSetOwner(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

/*
 * VIOC_SETGROUP
 *
 * This pioctl requires the use of the cm_ioctlQueryOptions_t structure.
 *
 */
afs_int32
smb_IoctlSetGroup(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp) {
        if (CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
            flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

        if (CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
            cm_fid_t fid;
            cm_SkipIoctlPath(&ioctlp->ioctl);
            cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                       optionsp->fid.vnode, optionsp->fid.unique);
            code = cm_GetSCache(&fid, &scp, userp, &req);
        } else {
            code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
        }
        if (code)
            return code;

        cm_IoctlSkipQueryOptions(&ioctlp->ioctl, userp);
    }

    code = cm_IoctlSetGroup(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}


afs_int32
smb_IoctlGetUnixMode(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, &scp, userp, &req);
    } else {
        code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetUnixMode(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}


/*
 * VIOC_SETUNIXMODE
 *
 * This pioctl requires the use of the cm_ioctlQueryOptions_t structure.
 */
afs_int32
smb_IoctlSetUnixMode(struct smb_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_req_t req;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    smb_InitReq(&req);

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp) {
        if (CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
            flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

        if (CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
            cm_fid_t fid;
            cm_SkipIoctlPath(&ioctlp->ioctl);
            cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                       optionsp->fid.vnode, optionsp->fid.unique);
            code = cm_GetSCache(&fid, &scp, userp, &req);
        } else {
            code = smb_ParseIoctlPath(ioctlp, userp, &req, &scp, flags);
        }
        if (code)
            return code;

        cm_IoctlSkipQueryOptions(&ioctlp->ioctl, userp);
    }

    code = cm_IoctlSetUnixMode(&ioctlp->ioctl, userp, scp, &req);

    cm_ReleaseSCache(scp);

    return code;
}

