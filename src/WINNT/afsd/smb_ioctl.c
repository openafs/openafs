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

#ifndef DJGPP
#include <windows.h>
#endif /* !DJGPP */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <osi.h>

#include "afsd.h"

#include "smb.h"

smb_ioctlProc_t *smb_ioctlProcsp[SMB_IOCTL_MAXPROCS];

/*extern unsigned char smb_LANadapter;*/

void smb_InitIoctl(void)
{
    int i;
    for (i=0; i<SMB_IOCTL_MAXPROCS; i++)
	smb_ioctlProcsp[i] = NULL;

    smb_ioctlProcsp[VIOCGETAL] = cm_IoctlGetACL;
    smb_ioctlProcsp[VIOC_FILE_CELL_NAME] = cm_IoctlGetFileCellName;
    smb_ioctlProcsp[VIOCSETAL] = cm_IoctlSetACL;
    smb_ioctlProcsp[VIOC_FLUSHVOLUME] = cm_IoctlFlushVolume;
    smb_ioctlProcsp[VIOCFLUSH] = cm_IoctlFlushFile;
#ifdef COMMENT
    /* This functions do not return the data expected by the 
     * Windows CIFS client.  Calling them only increases the 
     * number of roundtrips to the file server with no benefit.
     */
    smb_ioctlProcsp[VIOCSETVOLSTAT] = cm_IoctlSetVolumeStatus;
    smb_ioctlProcsp[VIOCGETVOLSTAT] = cm_IoctlGetVolumeStatus;
#endif
    smb_ioctlProcsp[VIOCWHEREIS] = cm_IoctlWhereIs;
    smb_ioctlProcsp[VIOC_AFS_STAT_MT_PT] = cm_IoctlStatMountPoint;
    smb_ioctlProcsp[VIOC_AFS_DELETE_MT_PT] = cm_IoctlDeleteMountPoint;
    smb_ioctlProcsp[VIOCCKSERV] = cm_IoctlCheckServers;
    smb_ioctlProcsp[VIOC_GAG] = cm_IoctlGag;
    smb_ioctlProcsp[VIOCCKBACK] = cm_IoctlCheckVolumes;
    smb_ioctlProcsp[VIOCSETCACHESIZE] = cm_IoctlSetCacheSize;
    smb_ioctlProcsp[VIOCGETCACHEPARMS] = cm_IoctlGetCacheParms;
    smb_ioctlProcsp[VIOCGETCELL] = cm_IoctlGetCell;
    smb_ioctlProcsp[VIOCNEWCELL] = cm_IoctlNewCell;
    smb_ioctlProcsp[VIOC_GET_WS_CELL] = cm_IoctlGetWsCell;
    smb_ioctlProcsp[VIOC_AFS_SYSNAME] = cm_IoctlSysName;
    smb_ioctlProcsp[VIOC_GETCELLSTATUS] = cm_IoctlGetCellStatus;
    smb_ioctlProcsp[VIOC_SETCELLSTATUS] = cm_IoctlSetCellStatus;
    smb_ioctlProcsp[VIOC_SETSPREFS] = cm_IoctlSetSPrefs;
    smb_ioctlProcsp[VIOC_GETSPREFS] = cm_IoctlGetSPrefs;
    smb_ioctlProcsp[VIOC_STOREBEHIND] = cm_IoctlStoreBehind;
    smb_ioctlProcsp[VIOC_AFS_CREATE_MT_PT] = cm_IoctlCreateMountPoint;
    smb_ioctlProcsp[VIOC_TRACECTL] = cm_IoctlTraceControl;
    smb_ioctlProcsp[VIOCSETTOK] = cm_IoctlSetToken;
    smb_ioctlProcsp[VIOCGETTOK] = cm_IoctlGetTokenIter;
    smb_ioctlProcsp[VIOCNEWGETTOK] = cm_IoctlGetToken;
    smb_ioctlProcsp[VIOCDELTOK] = cm_IoctlDelToken;
    smb_ioctlProcsp[VIOCDELALLTOK] = cm_IoctlDelAllToken;
    smb_ioctlProcsp[VIOC_SYMLINK] = cm_IoctlSymlink;
    smb_ioctlProcsp[VIOC_LISTSYMLINK] = cm_IoctlListlink;
    smb_ioctlProcsp[VIOC_DELSYMLINK] = cm_IoctlDeletelink;
    smb_ioctlProcsp[VIOC_MAKESUBMOUNT] = cm_IoctlMakeSubmount;
    smb_ioctlProcsp[VIOC_GETRXKCRYPT] = cm_IoctlGetRxkcrypt;
    smb_ioctlProcsp[VIOC_SETRXKCRYPT] = cm_IoctlSetRxkcrypt;
    smb_ioctlProcsp[VIOC_ISSYMLINK] = cm_IoctlIslink;
#ifdef DJGPP
    smb_ioctlProcsp[VIOC_SHUTDOWN] = cm_IoctlShutdown;
#endif
    smb_ioctlProcsp[VIOC_TRACEMEMDUMP] = cm_IoctlMemoryDump;
    smb_ioctlProcsp[VIOC_ISSYMLINK] = cm_IoctlIslink;
    smb_ioctlProcsp[VIOC_FLUSHALL] = cm_IoctlFlushAllVolumes;
    smb_ioctlProcsp[VIOCGETFID] = cm_IoctlGetFid;
    smb_ioctlProcsp[VIOCGETOWNER] = cm_IoctlGetOwner;
    smb_ioctlProcsp[VIOC_RXSTAT_PROC] = cm_IoctlRxStatProcess;
    smb_ioctlProcsp[VIOC_RXSTAT_PEER] = cm_IoctlRxStatPeer;
}	

/* called to make a fid structure into an IOCTL fid structure */
void smb_SetupIoctlFid(smb_fid_t *fidp, cm_space_t *prefix)
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
        strcpy(copyPrefix->data, prefix->data);
        fidp->ioctlp->prefix = copyPrefix;
    }
    lock_ReleaseMutex(&fidp->mx);
}

/* called when we receive a read call, does the send of the received data if
 * this is the first read call.  This is the function that actually makes the
 * call to the ioctl code.
 */
smb_IoctlPrepareRead(smb_fid_t *fidp, smb_ioctl_t *ioctlp, cm_user_t *userp)
{
    long opcode;
    smb_ioctlProc_t *procp;
    long code;

    if (ioctlp->flags & SMB_IOCTLFLAG_DATAIN) {
        ioctlp->flags &= ~SMB_IOCTLFLAG_DATAIN;

        /* do the call now, or fail if we didn't get an opcode, or
         * enough of an opcode.
         */
        if (ioctlp->inCopied < sizeof(long)) 
            return CM_ERROR_INVAL;
        memcpy(&opcode, ioctlp->inDatap, sizeof(long));
        ioctlp->inDatap += sizeof(long);

        osi_Log1(afsd_logp, "Ioctl opcode 0x%x", opcode);

        /* check for opcode out of bounds */
        if (opcode < 0 || opcode >= SMB_IOCTL_MAXPROCS)
            return CM_ERROR_TOOBIG;

        /* check for no such proc */
        procp = smb_ioctlProcsp[opcode];
        if (procp == NULL) 
            return CM_ERROR_BADOP;

        /* otherwise, make the call */
        ioctlp->outDatap += sizeof(long);	/* reserve room for return code */
        code = (*procp)(ioctlp, userp);

        osi_Log1(afsd_logp, "Ioctl return code 0x%x", code);

        /* copy in return code */
        memcpy(ioctlp->outAllocp, &code, sizeof(long));
    }
    return 0;
}

/* called when we receive a write call.  If this is the first write call after
 * a series of reads (or the very first call), then we start a new call.
 * We also ensure that things are properly initialized for the start of a call.
 */
void smb_IoctlPrepareWrite(smb_fid_t *fidp, smb_ioctl_t *ioctlp)
{
    /* make sure the buffer(s) are allocated */
    if (!ioctlp->inAllocp) ioctlp->inAllocp = malloc(SMB_IOCTL_MAXDATA);
    if (!ioctlp->outAllocp) ioctlp->outAllocp = malloc(SMB_IOCTL_MAXDATA);

    /* Fixes fs la problem.  We do a StrToOEM later and if this data isn't initialized we get memory issues. */
    (void) memset(ioctlp->inAllocp, 0, SMB_IOCTL_MAXDATA);
    (void) memset(ioctlp->outAllocp, 0, SMB_IOCTL_MAXDATA);

    /* and make sure that we've reset our state for the new incoming request */
    if (!(ioctlp->flags & SMB_IOCTLFLAG_DATAIN)) {
	ioctlp->inCopied = 0;
	ioctlp->outCopied = 0;
	ioctlp->inDatap = ioctlp->inAllocp;
	ioctlp->outDatap = ioctlp->outAllocp;
	ioctlp->flags |= SMB_IOCTLFLAG_DATAIN;
    }
}	

/* called from smb_ReceiveCoreRead when we receive a read on the ioctl fid */
long smb_IoctlRead(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp,
		   smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    long count;
    long leftToCopy;
    char *op;
    long code;
    cm_user_t *userp;

    iop = fidp->ioctlp;
    count = smb_GetSMBParm(inp, 1);
    userp = smb_GetUser(vcp, inp);

    /* Identify tree */
    code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &iop->tidPathp);
    if (code) {
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHPATH;
    }

    /* turn the connection around, if required */
    code = smb_IoctlPrepareRead(fidp, iop, userp);

    if (code) {
	cm_ReleaseUser(userp);
	return code;
    }

    leftToCopy = (iop->outDatap - iop->outAllocp) - iop->outCopied;
    if (count > leftToCopy) count = leftToCopy;

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
    memcpy(op, iop->outCopied + iop->outAllocp, count);

    /* and adjust the counters */
    iop->outCopied += count;

    cm_ReleaseUser(userp);

    return 0;
}	

/* called from smb_ReceiveCoreWrite when we receive a write call on the IOCTL
 * file descriptor.
 */
long smb_IoctlWrite(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    long count;
    long code;
    char *op;
    int inDataBlockCount;

    code = 0;
    count = smb_GetSMBParm(inp, 1);
    iop = fidp->ioctlp;
        
    smb_IoctlPrepareWrite(fidp, iop);

    op = smb_GetSMBData(inp, NULL);
    op = smb_ParseDataBlock(op, NULL, &inDataBlockCount);

    if (count + iop->inCopied > SMB_IOCTL_MAXDATA) {
	code = CM_ERROR_TOOBIG;
	goto done;
    }
        
    /* copy data */
    memcpy(iop->inDatap + iop->inCopied, op, count);

    /* adjust counts */
    iop->inCopied += count;

  done:
    /* return # of bytes written */
    if (code == 0) {
	smb_SetSMBParm(outp, 0, count);
	smb_SetSMBDataLength(outp, 0);
    }

    return code;
}

/* called from V3 read to handle IOCTL descriptor reads */
long smb_IoctlV3Read(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_ioctl_t *iop;
    long count;
    long code;
    long leftToCopy;
    char *op;
    cm_user_t *userp;
    smb_user_t *uidp;

    iop = fidp->ioctlp;
    count = smb_GetSMBParm(inp, 5);
	
    userp = smb_GetUser(vcp, inp);
    osi_assert(userp != 0);

    uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
    iop->uidp = uidp;
    if (uidp && uidp->unp) {
        osi_Log3(afsd_logp, "Ioctl uid %d user %x name %s",
                 uidp->userID, userp,
                 osi_LogSaveString(afsd_logp, uidp->unp->name));
    } else {
        if (uidp)
	    osi_Log2(afsd_logp, "Ioctl uid %d user %x no name",
                     uidp->userID, userp);
        else
	    osi_Log1(afsd_logp, "Ioctl no uid user %x no name",
		     userp);
    }

    code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &iop->tidPathp);
    if (code) {
	if (uidp)
	    smb_ReleaseUID(uidp);
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHPATH;
    }

    code = smb_IoctlPrepareRead(fidp, iop, userp);
    if (uidp) {
        iop->uidp = 0;
        smb_ReleaseUID(uidp);
    }
    if (code) {
	cm_ReleaseUser(userp);
	return code;
    }

    leftToCopy = (iop->outDatap - iop->outAllocp) - iop->outCopied;
    if (count > leftToCopy) count = leftToCopy;
        
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
    memcpy(op, iop->outCopied + iop->outAllocp, count);

    /* and adjust the counters */
    iop->outCopied += count;

    /* and cleanup things */
    cm_ReleaseUser(userp);

    return 0;
}

/* called from Read Raw to handle IOCTL descriptor reads */
long smb_IoctlReadRaw(smb_fid_t *fidp, smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *outp
#ifdef DJGPP
, dos_ptr rawBuf
#endif /* DJGPP */
)
{
    smb_ioctl_t *iop;
    long leftToCopy;
    NCB *ncbp;
    long code;
    cm_user_t *userp;
#ifdef DJGPP
    dos_ptr dos_ncb;

    if (rawBuf == 0)
    {
	osi_Log0(afsd_logp, "Failed to get raw buf for smb_IoctlReadRaw");
	return -1;
    }
#endif /* DJGPP */

    iop = fidp->ioctlp;

    userp = smb_GetUser(vcp, inp);

    /* Log the user */
    {
	smb_user_t *uidp;

	uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
	if (uidp && uidp->unp) {
	    osi_Log3(afsd_logp, "Ioctl uid %d user %x name %s",
		     uidp->userID, userp,
		     osi_LogSaveString(afsd_logp, uidp->unp->name));
	} else if (uidp) {
	    osi_Log2(afsd_logp, "Ioctl uid %d user %x no name",
		     uidp->userID, userp);
	} else {
	    osi_Log1(afsd_logp, "Ioctl no uid user %x no name",
		      userp);
	}
	if (uidp) 
	    smb_ReleaseUID(uidp);
    }

    code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &iop->tidPathp);
    if(code) {
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHPATH;
    }

    code = smb_IoctlPrepareRead(fidp, iop, userp);
    if (code) {
	cm_ReleaseUser(userp);
	return code;
    }

    leftToCopy = (iop->outDatap - iop->outAllocp) - iop->outCopied;

    ncbp = outp->ncbp;
    memset((char *)ncbp, 0, sizeof(NCB));

    ncbp->ncb_length = (unsigned short) leftToCopy;
    ncbp->ncb_lsn = (unsigned char) vcp->lsn;
    ncbp->ncb_command = NCBSEND;
    /*ncbp->ncb_lana_num = smb_LANadapter;*/
    ncbp->ncb_lana_num = vcp->lana;

#ifndef DJGPP
    ncbp->ncb_buffer = iop->outCopied + iop->outAllocp;
    code = Netbios(ncbp);
#else /* DJGPP */
    dosmemput(iop->outCopied + iop->outAllocp, ncbp->ncb_length, rawBuf);
    ncbp->ncb_buffer = rawBuf;
    dos_ncb = ((smb_ncb_t *)ncbp)->dos_ncb;
    code = Netbios(ncbp, dos_ncb);
#endif /* !DJGPP */

    if (code != 0)
	osi_Log1(afsd_logp, "ReadRaw send failure code %d", code);

    cm_ReleaseUser(userp);

    return 0;
}
