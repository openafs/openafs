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
#include <afs/cellconfig.h>
#include <afs/afs_consts.h>
#include <afs/ptserver.h>
#include <ubik.h>

#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <osi.h>

#include "afsd.h"
#include "afsd_init.h"
#include <WINNT\afsreg.h>

#include "smb.h"

#include <rx/rxkad.h>
#include "afsrpc.h"

#include "cm_rpc.h"
#include <strsafe.h>
#include <winioctl.h>
#include <rx\rx.h>

#include "cm_btree.h"

#ifdef _DEBUG
#include <crtdbg.h>
#endif

/* Copied from afs_tokens.h */
#define PIOCTL_LOGON	0x1
#define MAX_PATH 260

const char utf8_prefix[] = UTF8_PREFIX;
const int  utf8_prefix_size = sizeof(utf8_prefix) -  sizeof(char);

osi_mutex_t cm_Afsdsbmt_Lock;

extern afs_int32 cryptall;
extern char cm_NetbiosName[];
extern clientchar_t cm_NetbiosNameC[];

extern void afsi_log(char *pattern, ...);

void cm_InitIoctl(void)
{
    lock_InitializeMutex(&cm_Afsdsbmt_Lock, "AFSDSBMT.INI Access Lock",
                          LOCK_HIERARCHY_AFSDBSBMT_GLOBAL);
}

/*
 * Utility function.  (Not currently in use.)
 * This function forces all dirty buffers to the file server and
 * then discards the status info.
 */
afs_int32
cm_CleanFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;

    code = cm_FSync(scp, userp, reqp, FALSE);
    if (!code) {
        lock_ObtainWrite(&scp->rw);
        cm_DiscardSCache(scp);
        lock_ReleaseWrite(&scp->rw);
    }
    osi_Log2(afsd_logp,"cm_CleanFile scp 0x%x returns error: [%x]",scp, code);
    return code;
}

/*
 * Utility function.  Used within this file.
 * scp must be held but not locked.
 */
afs_int32
cm_FlushFile(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    afs_int32 code;

#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	cm_noteLocalMountPointChange(FALSE);
	return 0;
    }
#endif

    code = buf_FlushCleanPages(scp, userp, reqp);

    if (scp->fileType == CM_SCACHETYPE_DIRECTORY)
        lock_ObtainWrite(&scp->dirlock);
    lock_ObtainWrite(&scp->rw);
    cm_DiscardSCache(scp);
    if (scp->fileType == CM_SCACHETYPE_DIRECTORY) {
        cm_ResetSCacheDirectory(scp, 1);
        lock_ReleaseWrite(&scp->dirlock);
    }
    lock_ReleaseWrite(&scp->rw);

    osi_Log2(afsd_logp,"cm_FlushFile scp 0x%x returns error: [%x]",scp, code);
    return code;
}

/*
 * Utility function.  (Not currently in use)
 * IoctlPath must be parsed or skipped prior to calling.
 * scp must be held but not locked.
 */
afs_int32
cm_FlushParent(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    cm_scache_t * pscp;

    pscp = cm_FindSCacheParent(scp);

    /* now flush the file */
    code = cm_FlushFile(pscp, userp, reqp);
    cm_ReleaseSCache(pscp);

    return code;
}

/*
 * Utility function.  Used within this function.
 */
afs_int32
cm_FlushVolume(cm_user_t *userp, cm_req_t *reqp, afs_uint32 cell, afs_uint32 volume)
{
    afs_int32 code = 0;
    cm_scache_t *scp;
    unsigned int i;

#ifdef AFS_FREELANCE_CLIENT
    if ( cell == AFS_FAKE_ROOT_CELL_ID && volume == AFS_FAKE_ROOT_VOL_ID ) {
	cm_noteLocalMountPointChange(FALSE);
	return 0;
    }
#endif

    lock_ObtainWrite(&cm_scacheLock);
    for (i=0; i<cm_data.scacheHashTableSize; i++) {
        for (scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp) {
            if (scp->fid.volume == volume && scp->fid.cell == cell) {
                cm_HoldSCacheNoLock(scp);
                lock_ReleaseWrite(&cm_scacheLock);

                /* now flush the file */
                code = cm_FlushFile(scp, userp, reqp);
                lock_ObtainWrite(&cm_scacheLock);
                cm_ReleaseSCacheNoLock(scp);
            }
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);

    return code;
}

/*
 *  TranslateExtendedChars - This is a fix for TR 54482.
 *
 *  If an extended character (80 - FF) is entered into a file
 *  or directory name in Windows, the character is translated
 *  into the OEM character map before being passed to us.
 *  The pioctl functions must match
 *  this translation for paths given via our own commands (like
 *  fs).  If we do not do this, then we will try to perform an
 *  operation on a non-translated path, which we will fail to
 *  find, since the path was created with the translated chars.
 *  This function performs the required translation.
 *
 *  OEM character code pages are used by the non-Unicode SMB
 *  mode.  Do not use if the CM_IOCTLFLAG_USEUTF8 is set.
 */
void
TranslateExtendedChars(char *str)
{
    if (!str || !*str)
        return;

    CharToOemA(str, str);
}

void cm_SkipIoctlPath(cm_ioctl_t *ioctlp)
{
    size_t temp;

    temp = strlen(ioctlp->inDatap) + 1;
    ioctlp->inDatap += temp;
}


clientchar_t * cm_ParseIoctlStringAlloc(cm_ioctl_t *ioctlp, const char * ext_instrp)
{
    clientchar_t * rs = NULL;
    const char * instrp;

    instrp = (ext_instrp)?ext_instrp:ioctlp->inDatap;

    if ((ioctlp->flags & CM_IOCTLFLAG_USEUTF8) == CM_IOCTLFLAG_USEUTF8) {
        rs = cm_Utf8ToClientStringAlloc(instrp, -1, NULL);
    } else {
        int cch;

        /* Not a UTF-8 string */
        if (smb_StoreAnsiFilenames) {
            cch = cm_AnsiToClientString(instrp, -1, NULL, 0);
#ifdef DEBUG
            osi_assert(cch > 0);
#endif
            rs = malloc(cch * sizeof(clientchar_t));
            cm_AnsiToClientString(instrp, -1, rs, cch);
        } else {
            cch = cm_OemToClientString(instrp, -1, NULL, 0);
#ifdef DEBUG
            osi_assert(cch > 0);
#endif
            rs = malloc(cch * sizeof(clientchar_t));
            cm_OemToClientString(instrp, -1, rs, cch);
        }
    }

    if (ext_instrp == NULL) {
        ioctlp->inDatap += strlen(ioctlp->inDatap) + 1;
    }
    return rs;
}

int cm_UnparseIoctlString(cm_ioctl_t *ioctlp,
                          char * ext_outp,
                          const clientchar_t * cstr, int cchlen)
{
    char *outp;
    int cchout;

    outp = ((ext_outp == NULL)? ioctlp->outDatap : ext_outp);

    if ((ioctlp->flags & CM_IOCTLFLAG_USEUTF8) == CM_IOCTLFLAG_USEUTF8) {
        cchout = cm_ClientStringToUtf8(cstr, cchlen, outp,
                                       (int)(SMB_IOCTL_MAXDATA - (outp - ioctlp->outAllocp)));
    } else {
        if (smb_StoreAnsiFilenames) {
            cchout = WideCharToMultiByte(CP_ACP, 0, cstr, cchlen,
                                         outp,
                                         (int)(SMB_IOCTL_MAXDATA - (outp - ioctlp->outAllocp)),
                                         NULL, NULL);
        } else {
            cchout = WideCharToMultiByte(CP_OEMCP, 0, cstr, cchlen,
                                         outp,
                                         (int)(SMB_IOCTL_MAXDATA - (outp - ioctlp->outAllocp)),
                                         NULL, NULL);
        }
    }

    if (cchout > 0 && ext_outp == NULL) {
        ioctlp->outDatap += cchout;
    }

    return cchout;
}

/*
 * Must be called before XXX_ParseIoctlPath or cm_SkipIoctlPath
 */
cm_ioctlQueryOptions_t *
cm_IoctlGetQueryOptions(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_uint32 pathlen = (afs_uint32) strlen(ioctlp->inDatap) + 1;
    char *p = ioctlp->inDatap + pathlen;
    cm_ioctlQueryOptions_t * optionsp = NULL;

    if (ioctlp->inCopied > p - ioctlp->inAllocp) {
        optionsp = (cm_ioctlQueryOptions_t *)p;
        if (optionsp->size < 12 /* minimum size of struct */)
            optionsp = NULL;
    }

    return optionsp;
}

/*
 * Must be called after smb_ParseIoctlPath or cm_SkipIoctlPath
 * or any other time that ioctlp->inDatap points at the
 * cm_ioctlQueryOptions_t object.
 */
void
cm_IoctlSkipQueryOptions(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    cm_ioctlQueryOptions_t * optionsp = (cm_ioctlQueryOptions_t *)ioctlp->inDatap;
    ioctlp->inDatap += optionsp->size;
    ioctlp->inCopied -= optionsp->size;
}

/* format the specified path to look like "/afs/<cellname>/usr", by
 * adding "/afs" (if necessary) in front, changing any \'s to /'s, and
 * removing any trailing "/"'s. One weirdo caveat: "/afs" will be
 * intentionally returned as "/afs/"--this makes submount manipulation
 * easier (because we can always jump past the initial "/afs" to find
 * the AFS path that should be written into afsdsbmt.ini).
 */
void
cm_NormalizeAfsPath(clientchar_t *outpathp, long cchlen, clientchar_t *inpathp)
{
    clientchar_t *cp;
    clientchar_t bslash_mountRoot[256];

    cm_ClientStrCpy(bslash_mountRoot, lengthof(bslash_mountRoot), cm_mountRootC);
    bslash_mountRoot[0] = '\\';

    if (!cm_ClientStrCmpNI(inpathp, cm_mountRootC, cm_mountRootCLen))
        cm_ClientStrCpy(outpathp, cchlen, inpathp);
    else if (!cm_ClientStrCmpNI(inpathp, bslash_mountRoot,
                                (int)cm_ClientStrLen(bslash_mountRoot)))
        cm_ClientStrCpy(outpathp, cchlen, inpathp);
    else if ((inpathp[0] == '/') || (inpathp[0] == '\\'))
        cm_ClientStrPrintfN(outpathp, cchlen, _C("%s%s"), cm_mountRootC, inpathp);
    else // inpathp looks like "<cell>/usr"
        cm_ClientStrPrintfN(outpathp, cchlen, _C("%s/%s"), cm_mountRootC, inpathp);

    for (cp = outpathp; *cp != 0; ++cp) {
        if (*cp == '\\')
            *cp = '/';
    }

    if (cm_ClientStrLen(outpathp) && (outpathp[cm_ClientStrLen(outpathp)-1] == '/')) {
        outpathp[cm_ClientStrLen(outpathp)-1] = 0;
    }

    if (!cm_ClientStrCmpI(outpathp, cm_mountRootC)) {
        cm_ClientStrCpy(outpathp, cchlen, cm_mountRootC);
    }
}

void cm_NormalizeAfsPathAscii(char *outpathp, long outlen, char *inpathp)
{
    char *cp;
    char bslash_mountRoot[256];

    strncpy(bslash_mountRoot, cm_mountRoot, sizeof(bslash_mountRoot) - 1);
    bslash_mountRoot[0] = '\\';

    if (!strnicmp (inpathp, cm_mountRoot, strlen(cm_mountRoot)))
        StringCbCopy(outpathp, outlen, inpathp);
    else if (!strnicmp (inpathp, bslash_mountRoot, strlen(bslash_mountRoot)))
        StringCbCopy(outpathp, outlen, inpathp);
    else if ((inpathp[0] == '/') || (inpathp[0] == '\\'))
        StringCbPrintfA(outpathp, outlen, "%s%s", cm_mountRoot, inpathp);
    else // inpathp looks like "<cell>/usr"
        StringCbPrintfA(outpathp, outlen, "%s/%s", cm_mountRoot, inpathp);

    for (cp = outpathp; *cp != 0; ++cp) {
        if (*cp == '\\')
            *cp = '/';
    }

    if (strlen(outpathp) && (outpathp[strlen(outpathp)-1] == '/')) {
        outpathp[strlen(outpathp)-1] = 0;
    }

    if (!_stricmp (outpathp, cm_mountRoot)) {
        StringCbCopy(outpathp, outlen, cm_mountRoot);
    }
}


/*
 * VIOCGETAL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlGetACL(cm_ioctl_t *ioctlp, cm_user_t *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_conn_t *connp;
    AFSOpaque acl;
    AFSFetchStatus fileStatus;
    AFSVolSync volSync;
    AFSFid afid;
    int tlen;
    struct rx_connection * rxconnp;

    memset(&volSync, 0, sizeof(volSync));

    /* now make the get acl call */
#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	code = 0;
        ioctlp->outDatap[0] ='\0';
    } else
#endif
    {
        afid.Volume = scp->fid.volume;
        afid.Vnode = scp->fid.vnode;
        afid.Unique = scp->fid.unique;
        do {
            acl.AFSOpaque_val = ioctlp->outDatap;
            acl.AFSOpaque_len = 0;
            code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
            if (code)
                continue;

            rxconnp = cm_GetRxConn(connp);
            code = RXAFS_FetchACL(rxconnp, &afid, &acl, &fileStatus, &volSync);
            rx_PutConnection(rxconnp);

        } while (cm_Analyze(connp, userp, reqp, &scp->fid, 0, &volSync, NULL, NULL, code));
        code = cm_MapRPCError(code, reqp);

        if (code)
            return code;
    }
    /* skip over return data */
    tlen = (int)strlen(ioctlp->outDatap) + 1;
    ioctlp->outDatap += tlen;

    /* and return success */
    return 0;
}


/*
 * VIOC_FILE_CELL_NAME internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlGetFileCellName(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_cell_t *cellp;

#ifdef AFS_FREELANCE_CLIENT
    if ( cm_freelanceEnabled &&
         scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
         scp->fid.volume==AFS_FAKE_ROOT_VOL_ID &&
         scp->fid.vnode==0x1 && scp->fid.unique==0x1 ) {
        StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), "Freelance.Local.Root");
        ioctlp->outDatap += strlen(ioctlp->outDatap) + 1;
        code = 0;
    } else
#endif /* AFS_FREELANCE_CLIENT */
    {
        cellp = cm_FindCellByID(scp->fid.cell, CM_FLAG_NOPROBE);
        if (cellp) {
            clientchar_t * cellname;

            cellname = cm_FsStringToClientStringAlloc(cellp->name, -1, NULL);
            if (cellname == NULL) {
                code = CM_ERROR_NOSUCHCELL;
            } else {
                cm_UnparseIoctlString(ioctlp, NULL, cellname, -1);
                free(cellname);
                code = 0;
            }
        } else
            code = CM_ERROR_NOSUCHCELL;
    }

    return code;
}


/*
 * VIOCSETAL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlSetACL(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_conn_t *connp;
    AFSOpaque acl;
    AFSFetchStatus fileStatus;
    AFSVolSync volSync;
    AFSFid fid;
    struct rx_connection * rxconnp;

    memset(&volSync, 0, sizeof(volSync));

#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	code = CM_ERROR_NOACCESS;
    } else
#endif
    {
        /* now make the get acl call */
        fid.Volume = scp->fid.volume;
        fid.Vnode = scp->fid.vnode;
        fid.Unique = scp->fid.unique;
        do {
            acl.AFSOpaque_val = ioctlp->inDatap;
            acl.AFSOpaque_len = (u_int)strlen(ioctlp->inDatap)+1;
            code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
            if (code)
                continue;

            rxconnp = cm_GetRxConn(connp);
            code = RXAFS_StoreACL(rxconnp, &fid, &acl, &fileStatus, &volSync);
            rx_PutConnection(rxconnp);

        } while (cm_Analyze(connp, userp, reqp, &scp->fid, 1, &volSync, NULL, NULL, code));
        code = cm_MapRPCError(code, reqp);

        /* invalidate cache info, since we just trashed the ACL cache */
        lock_ObtainWrite(&scp->rw);
        cm_DiscardSCache(scp);
        lock_ReleaseWrite(&scp->rw);
    }

    return code;
}

/*
 * VIOC_FLUSHALL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlFlushAllVolumes(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_scache_t *scp;
    unsigned int i;

    lock_ObtainWrite(&cm_scacheLock);
    for (i=0; i<cm_data.scacheHashTableSize; i++) {
        for (scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp) {
	    cm_HoldSCacheNoLock(scp);
	    lock_ReleaseWrite(&cm_scacheLock);

	    /* now flush the file */
	    code = cm_FlushFile(scp, userp, reqp);
	    lock_ObtainWrite(&cm_scacheLock);
	    cm_ReleaseSCacheNoLock(scp);
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);

    return code;
}

/*
 * VIOC_FLUSHVOLUME internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlFlushVolume(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;
    afs_uint32 volume;
    afs_uint32 cell;

#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	code = CM_ERROR_NOACCESS;
    } else
#endif
    {
        volume = scp->fid.volume;
        cell = scp->fid.cell;
        code = cm_FlushVolume(userp, reqp, cell, volume);
    }
    return code;
}

/*
 * VIOCFLUSH internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlFlushFile(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;

#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	code = CM_ERROR_NOACCESS;
    } else
#endif
    {
        cm_FlushFile(scp, userp, reqp);
    }
    return 0;
}


/*
 * VIOCSETVOLSTAT internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlSetVolumeStatus(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;
    char volName[32];
    char offLineMsg[256];
    char motd[256];
    cm_conn_t *tcp;
    AFSFetchVolumeStatus volStat;
    AFSStoreVolumeStatus storeStat;
    cm_volume_t *tvp;
    cm_cell_t *cellp;
    char *cp;
    clientchar_t *strp;
    struct rx_connection * rxconnp;

    memset(&storeStat, 0, sizeof(storeStat));
#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	code = CM_ERROR_NOACCESS;
    } else
#endif
    {
        cellp = cm_FindCellByID(scp->fid.cell, 0);
        osi_assertx(cellp, "null cm_cell_t");

        if (scp->flags & CM_SCACHEFLAG_RO)
            return CM_ERROR_READONLY;

        code = cm_FindVolumeByID(cellp, scp->fid.volume, userp, reqp,
                                 CM_GETVOL_FLAG_CREATE, &tvp);
        if (code)
            return code;

        cm_PutVolume(tvp);

        /* Copy the junk out, using cp as a roving pointer. */
        memcpy((char *)&volStat, ioctlp->inDatap, sizeof(AFSFetchVolumeStatus));
        ioctlp->inDatap += sizeof(AFSFetchVolumeStatus);

        strp = cm_ParseIoctlStringAlloc(ioctlp, NULL);
        cm_ClientStringToFsString(strp, -1, volName, lengthof(volName));
        free(strp);

        strp = cm_ParseIoctlStringAlloc(ioctlp, NULL);
        cm_ClientStringToFsString(strp, -1, offLineMsg, lengthof(offLineMsg));
        free(strp);

        strp = cm_ParseIoctlStringAlloc(ioctlp, NULL);
        cm_ClientStringToFsString(strp, -1, motd, lengthof(motd));
        free(strp);

        strp = NULL;

        storeStat.Mask = 0;
        if (volStat.MinQuota != -1) {
            storeStat.MinQuota = volStat.MinQuota;
            storeStat.Mask |= AFS_SETMINQUOTA;
        }
        if (volStat.MaxQuota != -1) {
            storeStat.MaxQuota = volStat.MaxQuota;
            storeStat.Mask |= AFS_SETMAXQUOTA;
        }

        do {
            code = cm_ConnFromFID(&scp->fid, userp, reqp, &tcp);
            if (code)
                continue;

            rxconnp = cm_GetRxConn(tcp);
            code = RXAFS_SetVolumeStatus(rxconnp, scp->fid.volume,
                                         &storeStat, volName, offLineMsg, motd);
            rx_PutConnection(rxconnp);

        } while (cm_Analyze(tcp, userp, reqp, &scp->fid, 1, NULL, NULL, NULL, code));
        code = cm_MapRPCError(code, reqp);
    }

    /* return on failure */
    if (code)
        return code;

    /* we are sending parms back to make compat. with prev system.  should
     * change interface later to not ask for current status, just set
     * new status
     */
    cp = ioctlp->outDatap;
    memcpy(cp, (char *)&volStat, sizeof(VolumeStatus));
    cp += sizeof(VolumeStatus);
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), volName);
    cp += strlen(volName)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), offLineMsg);
    cp += strlen(offLineMsg)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), motd);
    cp += strlen(motd)+1;

    /* now return updated return data pointer */
    ioctlp->outDatap = cp;

    return 0;
}


/*
 * VIOCGETVOLSTAT internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlGetVolumeStatus(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;
    char volName[32]="(unknown)";
    char offLineMsg[256]="server temporarily inaccessible";
    char motd[256]="server temporarily inaccessible";
    cm_conn_t *connp;
    AFSFetchVolumeStatus volStat;
    char *cp;
    char *Name;
    char *OfflineMsg;
    char *MOTD;
    struct rx_connection * rxconnp;

#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	code = 0;
	strncpy(volName, "Freelance.Local.Root", sizeof(volName));
	offLineMsg[0] = '\0';
	strncpy(motd, "Freelance mode in use.", sizeof(motd));
	volStat.Vid = scp->fid.volume;
	volStat.MaxQuota = 0;
	volStat.BlocksInUse = 100;
	volStat.PartBlocksAvail = 0;
	volStat.PartMaxBlocks = 100;
    } else
#endif
    {
	Name = volName;
	OfflineMsg = offLineMsg;
	MOTD = motd;
	do {
	    code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
	    if (code) continue;

	    rxconnp = cm_GetRxConn(connp);
	    code = RXAFS_GetVolumeStatus(rxconnp, scp->fid.volume,
					 &volStat, &Name, &OfflineMsg, &MOTD);
	    rx_PutConnection(rxconnp);

	} while (cm_Analyze(connp, userp, reqp, &scp->fid, 0, NULL, NULL, NULL, code));
	code = cm_MapRPCError(code, reqp);
    }

    if (code)
        return code;

    /* Copy all this junk into msg->im_data, keeping track of the lengths. */
    cp = ioctlp->outDatap;
    memcpy(cp, (char *)&volStat, sizeof(AFSFetchVolumeStatus));
    cp += sizeof(AFSFetchVolumeStatus);
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), volName);
    cp += strlen(volName)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), offLineMsg);
    cp += strlen(offLineMsg)+1;
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), motd);
    cp += strlen(motd)+1;

    /* return new size */
    ioctlp->outDatap = cp;

    return 0;
}

/*
 * VIOCGETFID internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlGetFid(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    char *cp;
    cm_fid_t fid;

    memset(&fid, 0, sizeof(cm_fid_t));
    fid.cell   = scp->fid.cell;
    fid.volume = scp->fid.volume;
    fid.vnode  = scp->fid.vnode;
    fid.unique = scp->fid.unique;

    /* Copy all this junk into msg->im_data, keeping track of the lengths. */
    cp = ioctlp->outDatap;
    memcpy(cp, (char *)&fid, sizeof(cm_fid_t));
    cp += sizeof(cm_fid_t);

    /* return new size */
    ioctlp->outDatap = cp;

    return 0;
}

/*
 * VIOC_GETFILETYPE internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlGetFileType(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    char *cp;
    afs_uint32 fileType = 0;

    if (scp->fileType == 0) {
        lock_ObtainWrite(&scp->rw);
        code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                         CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code == 0)
            cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&scp->rw);
    }

    if (code == 0) {
        fileType = scp->fileType;

        /* Copy all this junk into msg->im_data, keeping track of the lengths. */
        cp = ioctlp->outDatap;
        memcpy(cp, (char *)&fileType, sizeof(fileType));
        cp += sizeof(fileType);

        /* return new size */
        ioctlp->outDatap = cp;
    }
    return code;
}

/*
 * VIOCGETOWNER internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlGetOwner(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    char *cp;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    if (code == 0) {
        /* Copy all this junk into msg->im_data, keeping track of the lengths. */
        cp = ioctlp->outDatap;
        memcpy(cp, (char *)&scp->owner, sizeof(afs_uint32));
        cp += sizeof(afs_uint32);
        memcpy(cp, (char *)&scp->group, sizeof(afs_uint32));
        cp += sizeof(afs_uint32);

        /* return new size */
        ioctlp->outDatap = cp;
    }
    return code;
}


/*
 * VIOC_SETOWNER internals.
 *
 * Assumes that pioctl path has been parsed or skipped
 * and that cm_ioctlQueryOptions_t have been parsed and skipped.
 *
 * scp is held but not locked.
 *
 */
afs_int32
cm_IoctlSetOwner(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    char *cp;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    if (code == 0) {
        afs_uint32 owner;
        cm_attr_t attr;

        memset(&attr, 0, sizeof(attr));

        cp = ioctlp->inDatap;
        memcpy((char *)&owner, cp, sizeof(afs_uint32));

        attr.mask = CM_ATTRMASK_OWNER;
        attr.owner = owner;

        code = cm_SetAttr(scp, &attr, userp, reqp);
    }
    return code;
}


/*
 * VIOC_SETGROUP internals.
 *
 * Assumes that pioctl path has been parsed or skipped
 * and that cm_ioctlQueryOptions_t have been parsed and skipped.
 *
 */
afs_int32
cm_IoctlSetGroup(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    char *cp;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    if (code == 0) {
        afs_uint32 group;
        cm_attr_t attr;

        memset(&attr, 0, sizeof(attr));

        cp = ioctlp->inDatap;
        memcpy((char *)&group, cp, sizeof(afs_uint32));

        attr.mask = CM_ATTRMASK_GROUP;
        attr.group = group;

        code = cm_SetAttr(scp, &attr, userp, reqp);
    }
    return code;
}


/*
 * VIOCWHEREIS internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlWhereIs(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    cm_cell_t *cellp;
    cm_volume_t *tvp;
    cm_serverRef_t **tsrpp, *current;
    cm_server_t *tsp;
    afs_uint32 volume;
    char *cp;

    volume = scp->fid.volume;

    cellp = cm_FindCellByID(scp->fid.cell, 0);

    if (!cellp)
	return CM_ERROR_NOSUCHCELL;

#ifdef AFS_FREELANCE_CLIENT
    if ( cellp->cellID == AFS_FAKE_ROOT_CELL_ID) {
        struct in_addr addr;

        addr.s_net = 127;
        addr.s_host = 0;
        addr.s_lh = 0;
        addr.s_impno = 1;

        cp = ioctlp->outDatap;

        memcpy(cp, (char *)&addr, sizeof(addr));
        cp += sizeof(addr);

        /* still room for terminating NULL, add it on */
        addr.s_addr = 0;
        memcpy(cp, (char *)&addr, sizeof(addr));
        cp += sizeof(addr);

        ioctlp->outDatap = cp;
    } else
#endif
    {
        code = cm_FindVolumeByID(cellp, volume, userp, reqp, CM_GETVOL_FLAG_CREATE, &tvp);
        if (code)
            return code;

        cp = ioctlp->outDatap;

        tsrpp = cm_GetVolServers(tvp, volume, userp, reqp);
        if (tsrpp == NULL) {
            code = CM_ERROR_NOSUCHVOLUME;
        } else {
            lock_ObtainRead(&cm_serverLock);
            for (current = *tsrpp; current; current = current->next) {
                tsp = current->server;
                memcpy(cp, (char *)&tsp->addr.sin_addr.s_addr, sizeof(long));
                cp += sizeof(long);
            }
            lock_ReleaseRead(&cm_serverLock);
            cm_FreeServerList(tsrpp, 0);
        }
        /* still room for terminating NULL, add it on */
        volume = 0;	/* reuse vbl */
        memcpy(cp, (char *)&volume, sizeof(long));
        cp += sizeof(long);

        ioctlp->outDatap = cp;
        cm_PutVolume(tvp);
    }
    return code;
}

/*
 * VIOC_AFS_STAT_MT_PT internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlStatMountPoint(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *dscp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_scache_t *scp;
    clientchar_t *cp;

    cp = cm_ParseIoctlStringAlloc(ioctlp, NULL);

    code = cm_Lookup(dscp, cp[0] ? cp : L".", CM_FLAG_NOMOUNTCHASE, userp, reqp, &scp);
    if (code)
        goto done_2;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    else
        goto done;

    /* now check that this is a real mount point */
    if (scp->fileType != CM_SCACHETYPE_MOUNTPOINT) {
        code = CM_ERROR_INVAL;
        goto done;
    }

    code = cm_ReadMountPoint(scp, userp, reqp);
    if (code == 0) {
        char * strp;
        strp = ioctlp->outDatap;
        StringCbCopyA(strp, SMB_IOCTL_MAXDATA - (strp - ioctlp->outAllocp), scp->mountPointStringp);
        strp += strlen(strp) + 1;
        ioctlp->outDatap = strp;
    }

  done:
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

 done_2:
    if (cp)
        free(cp);

    return code;
}

/*
 * VIOC_AFS_DELETE_MT_PT internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlDeleteMountPoint(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *dscp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_scache_t *scp;
    clientchar_t *cp = NULL;
    fschar_t *originalName = NULL;
    cm_dirOp_t dirop;

    cp = cm_ParseIoctlStringAlloc(ioctlp, NULL);

    code = cm_Lookup(dscp, cp[0] ? cp : L".", CM_FLAG_NOMOUNTCHASE, userp, reqp, &scp);

    /* if something went wrong, bail out now */
    if (code)
        goto done3;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code)
        goto done2;

    /* now check that this is a real mount point */
    if (scp->fileType != CM_SCACHETYPE_MOUNTPOINT) {
        code = CM_ERROR_INVAL;
        goto done1;
    }

    /* time to make the RPC, so drop the lock */
    lock_ReleaseWrite(&scp->rw);

#ifdef USE_BPLUS
    code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_READ,
                         CM_DIROP_FLAG_NONE, &dirop);
    if (code == 0) {
        code = cm_BPlusDirLookupOriginalName(&dirop, cp, &originalName);
        /* The cm_Dir* functions can't be used to lookup the
           originalName.  Those functions only know of the original
           name. */
        cm_EndDirOp(&dirop);
    }
#endif

    /* If this name doesn't have a non-normalized name associated with
       it, we assume that what we had is what is actually present on
       the file server. */

    if (originalName == NULL) {
        originalName = cm_ClientStringToFsStringAlloc(cp, -1, NULL);
    }

    /* cp is a normalized name.  originalName is the actual name we
       saw on the fileserver. */
#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* we are removing the mount point to the root dir., so call
         * the freelance code to do the deletion. */
        osi_Log0(afsd_logp,"IoctlDeleteMountPoint from Freelance root dir");
        code = cm_FreelanceRemoveMount(originalName);
    } else
#endif
    {
        /* easier to do it this way */
        code = cm_Unlink(dscp, originalName, cp, userp, reqp);
    }
    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_REMOVED,
                         FILE_NOTIFY_CHANGE_DIR_NAME,
                         dscp, cp, NULL, TRUE);

    lock_ObtainWrite(&scp->rw);
  done1:
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

  done2:
    cm_DiscardSCache(scp);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

  done3:
    if (originalName != NULL)
        free(originalName);

    if (cp != NULL)
        free(cp);

    return code;
}

/*
 * VIOCCKSERV internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlCheckServers(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    cm_cell_t *cellp;
    chservinfo_t csi;
    char *tp;
    char *cp;
    long temp;
    cm_server_t *tsp, *csp;
    int haveCell;

    tp = ioctlp->inDatap;
    haveCell = 0;

    memcpy(&temp, tp, sizeof(temp));
    if (temp == 0x12345678) {	/* For afs3.3 version */
        memcpy(&csi, tp, sizeof(csi));
        if (csi.tinterval >= 0) {
            cp = ioctlp->outDatap;
            memcpy(cp, (char *)&cm_daemonCheckDownInterval, sizeof(long));
            ioctlp->outDatap += sizeof(long);
            if (csi.tinterval > 0) {
                if (!smb_SUser(userp))
                    return CM_ERROR_NOACCESS;
                cm_daemonCheckDownInterval = csi.tinterval;
            }
            return 0;
        }
        if (csi.tsize)
            haveCell = 1;
        temp = csi.tflags;
        cp = csi.tbuffer;
    } else {	/* For pre afs3.3 versions */
        memcpy((char *)&temp, ioctlp->inDatap, sizeof(long));
        ioctlp->inDatap = cp = ioctlp->inDatap + sizeof(long);
        if (cp - ioctlp->inAllocp < ioctlp->inCopied)	/* still more data available */
            haveCell = 1;
    }

    /*
     * 1: fast check, don't contact servers.
     * 2: local cell only.
     */
    if (haveCell) {
        /* have cell name, too */
        cellp = cm_GetCell(cp, (temp & 1) ? CM_FLAG_NOPROBE : 0);
        if (!cellp)
            return CM_ERROR_NOSUCHCELL;
    }
    else
        cellp = (cm_cell_t *) 0;
    if (!cellp && (temp & 2)) {
        /* use local cell */
        fschar_t wscell[CELL_MAXNAMELEN+1];
        cm_GetRootCellName(wscell);
        cellp = cm_GetCell(wscell, 0);
    }
    if (!(temp & 1)) {	/* if not fast, call server checker routine */
        /* check down servers */
        cm_CheckServers(CM_FLAG_CHECKDOWNSERVERS | CM_FLAG_CHECKUPSERVERS, cellp);
    }

    /* now return the current down server list */
    cp = ioctlp->outDatap;
    lock_ObtainRead(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp=tsp->allNextp) {
        if (cellp && tsp->cellp != cellp)
            continue;	/* cell spec'd and wrong */
        if (tsp->flags & CM_SERVERFLAG_DOWN) {
            /*
             * for a multi-homed file server, if one of the interfaces
             * is up, do not report the server as down.
             */
            if (tsp->type == CM_SERVER_FILE) {
                for (csp = cm_allServersp; csp; csp=csp->allNextp) {
                    if (csp->type == CM_SERVER_FILE &&
                        !(csp->flags & CM_SERVERFLAG_DOWN) &&
                        afs_uuid_equal(&tsp->uuid, &csp->uuid)) {
                        break;
                    }
                }
                if (csp)    /* found alternate up interface */
                    continue;
            }

            /*
             * all server types are being reported by ipaddr.  only report
             * a server once regardless of how many services are down.
             */
            for (tp = ioctlp->outDatap; tp < cp; tp += sizeof(long)) {
                if (!memcmp(tp, (char *)&tsp->addr.sin_addr.s_addr, sizeof(long)))
                    break;
            }

            if (tp == cp) {
                memcpy(cp, (char *)&tsp->addr.sin_addr.s_addr, sizeof(long));
                cp += sizeof(long);
            }
        }
    }
    lock_ReleaseRead(&cm_serverLock);

    ioctlp->outDatap = cp;
    return 0;
}

/*
 * VIOCCKBACK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlCheckVolumes(cm_ioctl_t *ioctlp, cm_user_t *userp)
{
    cm_RefreshVolumes(0);
    return 0;
}

/*
 * VIOCSETCACHESIZE internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 *
 * This function is no longer meaningful in the current day world
 * of persistent caches.  The buf_SetNBuffers() function will
 * inevitably fail.
 */
afs_int32
cm_IoctlSetCacheSize(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_int32 code;
    afs_uint64 temp;

    memcpy(&temp, ioctlp->inDatap, sizeof(temp));
    if (temp == 0)
        temp = cm_data.buf_nOrigBuffers;
    else {
        /* temp is in 1K units, convert to # of buffers */
        temp = temp / (cm_data.buf_blockSize / 1024);
    }

    /* now adjust the cache size */
    code = buf_SetNBuffers(temp);

    return code;
}

/*
 * VIOC_TRACECTL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlTraceControl(cm_ioctl_t *ioctlp, cm_user_t *userp)
{
    afs_uint32 inValue;

    memcpy(&inValue, ioctlp->inDatap, sizeof(afs_uint32));

    /* print trace */
    if (inValue & 8) {
        afsd_ForceTrace(FALSE);
        buf_ForceTrace(FALSE);
    }

    if (inValue & 2) {
        /* set tracing value to low order bit */
        if ((inValue & 1) == 0) {
            /* disable tracing */
            osi_LogDisable(afsd_logp);
	    rx_DebugOnOff(FALSE);
        }
        else {
            /* enable tracing */
            osi_LogEnable(afsd_logp);
	    rx_DebugOnOff(TRUE);
        }
    }

    /* see if we're supposed to do a reset, too */
    if (inValue & 4) {
        osi_LogReset(afsd_logp);
    }

    /* and copy out tracing flag */
    inValue = afsd_logp->enabled;	/* use as a temp vbl */
    memcpy(ioctlp->outDatap, &inValue, sizeof(afs_uint32));
    ioctlp->outDatap += sizeof(afs_uint32);
    return 0;
}

/*
 * VIOCGETCACHEPARMS internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetCacheParms(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    cm_cacheParms_t parms;

    memset(&parms, 0, sizeof(parms));

    /* first we get, in 1K units, the cache size */
    parms.parms[0] = cm_data.buf_nbuffers * (cm_data.buf_blockSize / 1024);

    /* and then the actual # of buffers in use (not in the free list, I guess,
     * will be what we do).
     */
    parms.parms[1] = (cm_data.buf_nbuffers - buf_CountFreeList()) * (cm_data.buf_blockSize / 1024);

    memcpy(ioctlp->outDatap, &parms, sizeof(parms));
    ioctlp->outDatap += sizeof(parms);

    return 0;
}

/*
 * VIOCGETCELL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetCell(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    long whichCell;
    long magic = 0;
    cm_cell_t *tcellp;
    cm_serverRef_t *serverRefp;
    cm_server_t *serverp;
    long i;
    char *cp;
    char *tp;
    char *basep;

    tp = ioctlp->inDatap;

    memcpy((char *)&whichCell, tp, sizeof(long));
    tp += sizeof(long);

    /* see if more than one long passed in, ignoring the null pathname (the -1) */
    if (ioctlp->inCopied-1 > sizeof(afs_uint32)) {
        memcpy((char *)&magic, tp, sizeof(afs_uint32));
    }

    lock_ObtainRead(&cm_cellLock);
    for (tcellp = cm_data.allCellsp; tcellp; tcellp = tcellp->allNextp) {
        if (whichCell == 0)
            break;
        whichCell--;
    }
    lock_ReleaseRead(&cm_cellLock);
    if (tcellp) {
        int max = 8;
        clientchar_t * cellnamep;

        cp = ioctlp->outDatap;

        if (magic == 0x12345678) {
            memcpy(cp, (char *)&magic, sizeof(long));
            max = 13;
        }
        memset(cp, 0, max * sizeof(long));
        basep = cp;
        lock_ObtainRead(&cm_serverLock);	/* for going down server list */
        for (i=0, serverRefp = tcellp->vlServersp;
             serverRefp && i<max;
             i++, serverRefp = serverRefp->next) {
            serverp = serverRefp->server;
            memcpy(cp, &serverp->addr.sin_addr.s_addr, sizeof(long));
            cp += sizeof(long);
        }
        lock_ReleaseRead(&cm_serverLock);
        ioctlp->outDatap = basep + max * sizeof(afs_int32);

        cellnamep = cm_FsStringToClientStringAlloc(tcellp->name, -1, NULL);
        if (cellnamep) {
            cm_UnparseIoctlString(ioctlp, NULL, cellnamep, -1);
            free(cellnamep);
        } else {
            tcellp = NULL;
        }
    }

    if (tcellp)
        return 0;
    else
        return CM_ERROR_NOMORETOKENS;	/* mapped to EDOM */
}


/*
 * VIOCNEWCELL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlNewCell(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    /*
     * All that needs to be done is to refresh server information for all cells that
     * are already loaded.

     * cell list will be cm_CellLock and cm_ServerLock will be held for write.
     */

    cm_cell_t *cp;
    cm_cell_rock_t rock;

    lock_ObtainWrite(&cm_cellLock);

    for (cp = cm_data.allCellsp; cp; cp=cp->allNextp)
    {
        afs_int32 code;

        /* delete all previous server lists - cm_FreeServerList will ask for write on cm_ServerLock*/
        cm_FreeServerList(&cp->vlServersp, CM_FREESERVERLIST_DELETE);
        lock_ReleaseWrite(&cm_cellLock);

        rock.cellp = cp;
        rock.flags = 0;
        code = cm_SearchCellRegistry(1, cp->name, cp->name, cp->linkedName, cm_AddCellProc, &rock);
        if (code && code != CM_ERROR_FORCE_DNS_LOOKUP)
            code = cm_SearchCellFileEx(cp->name, cp->name, cp->linkedName, cm_AddCellProc, &rock);
        if (code) {
            if (cm_dnsEnabled) {
                int ttl;
                code = cm_SearchCellByDNS(cp->name, cp->name, &ttl, cm_AddCellProc, &rock);
                if ( code == 0 ) { /* got cell from DNS */
                    lock_ObtainMutex(&cp->mx);
                    _InterlockedOr(&cp->flags, CM_CELLFLAG_DNS);
                    _InterlockedAnd(&cp->flags, ~CM_CELLFLAG_VLSERVER_INVALID);
                    cp->timeout = time(0) + ttl;
                    lock_ReleaseMutex(&cp->mx);
                }
            }
        }
        else {
            lock_ObtainMutex(&cp->mx);
            _InterlockedAnd(&cp->flags, ~CM_CELLFLAG_DNS);
            lock_ReleaseMutex(&cp->mx);
        }
        if (code) {
            lock_ObtainMutex(&cp->mx);
            _InterlockedOr(&cp->flags, CM_CELLFLAG_VLSERVER_INVALID);
            lock_ReleaseMutex(&cp->mx);
            lock_ObtainWrite(&cm_cellLock);
        }
        else {
            lock_ObtainMutex(&cp->mx);
            _InterlockedAnd(&cp->flags, ~CM_CELLFLAG_VLSERVER_INVALID);
            lock_ReleaseMutex(&cp->mx);
            lock_ObtainWrite(&cm_cellLock);
            cm_RandomizeServer(&cp->vlServersp);
        }
    }
    lock_ReleaseWrite(&cm_cellLock);
    return 0;
}

/*
 * VIOCNEWCELL2 internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 *
 * The pioctl data buffer consists of the following structure:
 *
 *  afs_uint32 flags
 *  afs_uint32 alternative fs port
 *  afs_uint32 alternative vl port
 *  afs_uint32 count of vldb servers
 *  char[]     cellname
 *  char[]     linkedcell
 *  n * char[] hostnames
 */
afs_int32
cm_IoctlNewCell2(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_uint32  code = 0;
    afs_uint32  flags = 0;
    afs_uint32  fsport = 0;
    afs_uint32  vlport = 0;
    afs_uint32  i, host_count = 0;
    char *      cellname = NULL;
    char *      linked_cellname = NULL;
    char *tp;
    size_t tplen;
    afs_uint32 *lp;
    char * hostname[AFS_MAXHOSTS];
    size_t len;

    memset(hostname, 0, sizeof(hostname));

    tp = ioctlp->inDatap;
    tplen = ioctlp->inCopied;
    lp = (afs_uint32 *)tp;

    if (tplen >= 4 * sizeof(afs_uint32)) {
        flags = *lp++;
        fsport = *lp++;
        vlport = *lp++;
        host_count = *lp++;
        tp = (char *)lp;
        tplen -= 4 * sizeof(afs_uint32);
    }

    if ( FAILED(StringCbLength(tp, tplen, &len)) ||
         len + 1 > CELL_MAXNAMELEN)
        return CM_ERROR_INVAL;
    cellname = tp;
    tp += len + 1;
    tplen -= (len + 1);

    if ( FAILED(StringCbLength(tp, tplen, &len)) ||
         len + 1 > CELL_MAXNAMELEN)
        return CM_ERROR_INVAL;
    linked_cellname = tp;
    tp += len + 1;
    tplen -= (len + 1);

    if (!(flags & VIOC_NEWCELL2_FLAG_USEDNS)) {
        for ( i=0; i<host_count; i++) {
            if ( FAILED(StringCbLength(tp, tplen, &len)) )
                return CM_ERROR_INVAL;
            hostname[i] = tp;
            tp += len + 1;
            tplen -= (len + 1);
        }
    }

    code = cm_CreateCellWithInfo( cellname, linked_cellname,
                                  vlport, host_count,
                                  hostname,
                                  (flags & VIOC_NEWCELL2_FLAG_USEDNS) ? CM_CELLFLAG_DNS : 0);

    if (code == 0 && (flags & VIOC_NEWCELL2_FLAG_USEREG)) {
        cm_AddCellToRegistry( cellname, linked_cellname,
                              vlport, host_count,
                              hostname,
                              (flags & VIOC_NEWCELL2_FLAG_USEDNS) ? CM_CELLFLAG_DNS : 0);
    }
    return code;
}

/*
 * VIOC_GET_WS_CELL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetWsCell(cm_ioctl_t *ioctlp, cm_user_t *userp)
{
    afs_int32 code = 0;

    if (cm_freelanceEnabled) {
        if (cm_GetRootCellName(ioctlp->outDatap))
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), "Freelance.Local.Root");
        ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
    } else if (cm_data.rootCellp) {
        clientchar_t * cellnamep = cm_FsStringToClientStringAlloc(cm_data.rootCellp->name, -1, NULL);
        /* return the default cellname to the caller */
        if (cellnamep) {
            cm_UnparseIoctlString(ioctlp, NULL, cellnamep, -1);
            free(cellnamep);
        } else {
            code = CM_ERROR_NOSUCHCELL;
        }
    } else {
        /* if we don't know our default cell, return failure */
        code = CM_ERROR_NOSUCHCELL;
    }

    return code;
}

/*
 * VIOC_AFS_SYSNAME internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlSysName(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_uint32 setSysName;
    char *cp, *cp2;
    clientchar_t *inname = NULL;
    int t;
    unsigned int count;

    memcpy(&setSysName, ioctlp->inDatap, sizeof(afs_uint32));
    ioctlp->inDatap += sizeof(afs_uint32);

    if (setSysName) {
        /* check my args */
        if ( setSysName < 0 || setSysName > MAXNUMSYSNAMES )
            return EINVAL;
        cp2 = ioctlp->inDatap;
        for ( cp=ioctlp->inDatap, count = 0; count < setSysName; count++ ) {
            /* won't go past end of ioctlp->inDatap since
               maxsysname*num < ioctlp->inDatap length */
            t = (int)strlen(cp);
            if (t >= MAXSYSNAME || t <= 0)
                return EINVAL;
            /* check for names that can shoot us in the foot */
            if (*cp == '.' && (cp[1] == 0 || (cp[1] == '.' && cp[2] == 0)))
                return EINVAL;
            cp += t + 1;
        }
        /* args ok */

        /* inname gets first entry in case we're being a translator */
        /* (we are never a translator) */
        inname = cm_ParseIoctlStringAlloc(ioctlp, NULL);
    }

    /* Not xlating, so local case */
    if (!cm_sysName)
        osi_panic("cm_IoctlSysName: !cm_sysName\n", __FILE__, __LINE__);

    if (setSysName) {
        /* Local guy; only root can change sysname */
        /* clear @sys entries from the dnlc, once afs_lookup can
         * do lookups of @sys entries and thinks it can trust them */
        /* privs ok, store the entry, ... */

        cm_ClientStrCpy(cm_sysName, lengthof(cm_sysName), inname);
        cm_ClientStrCpy(cm_sysNameList[0], MAXSYSNAME, inname);

        if (setSysName > 1) {       /* ... or list */
            for (count = 1; count < setSysName; ++count) {
                clientchar_t * newsysname;

                if (!cm_sysNameList[count])
                    osi_panic("cm_IoctlSysName: no cm_sysNameList entry to write\n",
                              __FILE__, __LINE__);

                newsysname = cm_ParseIoctlStringAlloc(ioctlp, NULL);
                cm_ClientStrCpy(cm_sysNameList[count], MAXSYSNAME, newsysname);
                free(newsysname);
            }
        }
        cm_sysNameCount = setSysName;
    } else {
        afs_uint32 i32;

        /* return the sysname to the caller */
        i32 = cm_sysNameCount;
        memcpy(ioctlp->outDatap, &i32, sizeof(afs_int32));
        ioctlp->outDatap += sizeof(afs_int32);	/* skip found flag */

        if (cm_sysNameCount) {
            for ( count=0; count < cm_sysNameCount ; ++count) {   /* ... or list */
                if ( !cm_sysNameList[count] || *cm_sysNameList[count] == _C('\0'))
                    osi_panic("cm_IoctlSysName: no cm_sysNameList entry to read\n",
                              __FILE__, __LINE__);
                cm_UnparseIoctlString(ioctlp, NULL, cm_sysNameList[count], -1);
            }
        }
    }

    if (inname) {
        free(inname);
        inname = NULL;
    }

    /* done: success */
    return 0;
}

/*
 * VIOC_GETCELLSTATUS internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetCellStatus(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_uint32 temp;
    cm_cell_t *cellp;
    clientchar_t * cellnamep;
    fschar_t     * fscellnamep;

    cellnamep = cm_ParseIoctlStringAlloc(ioctlp, NULL);
    fscellnamep = cm_ClientStringToFsStringAlloc(cellnamep, -1, NULL);
    cellp = cm_GetCell(fscellnamep, 0);
    free(fscellnamep);
    free(cellnamep);

    if (!cellp)
        return CM_ERROR_NOSUCHCELL;

    temp = 0;
    lock_ObtainMutex(&cellp->mx);
    if (cellp->flags & CM_CELLFLAG_SUID)
        temp |= CM_SETCELLFLAG_SUID;
    lock_ReleaseMutex(&cellp->mx);

    /* now copy out parm */
    memcpy(ioctlp->outDatap, &temp, sizeof(afs_uint32));
    ioctlp->outDatap += sizeof(afs_uint32);

    return 0;
}

/*
 * VIOC_SETCELLSTATUS internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlSetCellStatus(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_uint32 flags;
    cm_cell_t *cellp;
    clientchar_t *temp;
    fschar_t * cellnamep;

    temp = cm_ParseIoctlStringAlloc(ioctlp, ioctlp->inDatap + 2*sizeof(afs_uint32));
    cellnamep = cm_ClientStringToFsStringAlloc(temp, -1, NULL);
    cellp = cm_GetCell(cellnamep, 0);
    free(temp);
    free(cellnamep);

    if (!cellp)
        return CM_ERROR_NOSUCHCELL;

    memcpy((char *)&flags, ioctlp->inDatap, sizeof(afs_uint32));

    lock_ObtainMutex(&cellp->mx);
    if (flags & CM_SETCELLFLAG_SUID)
        _InterlockedOr(&cellp->flags, CM_CELLFLAG_SUID);
    else
        _InterlockedAnd(&cellp->flags, ~CM_CELLFLAG_SUID);
    lock_ReleaseMutex(&cellp->mx);

    return 0;
}

/*
 * VIOC_SETSPREFS internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlSetSPrefs(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    cm_SSetPref_t     *spin;    /* input */
    cm_SPref_t        *srvin;   /* one input component */
    cm_server_t       *tsp;
    int 	       i, vlonly, noServers, type;
    struct sockaddr_in tmp;
    unsigned short     rank;

    spin	   = (cm_SSetPref_t *)ioctlp->inDatap;
    noServers  = spin->num_servers;
    vlonly     = spin->flags;
    if ( vlonly )
        type = CM_SERVER_VLDB;
    else
        type = CM_SERVER_FILE;

    for ( i=0; i < noServers; i++)
    {
        srvin          = &(spin->servers[i]);
        rank           = srvin->rank + (rand() & 0x000f);
        tmp.sin_addr   = srvin->host;
        switch (type) {
        case CM_SERVER_VLDB:
            tmp.sin_port = htons(7003);
            break;
        case CM_SERVER_FILE:
            tmp.sin_port = htons(7000);
            break;
        }
        tmp.sin_family = AF_INET;

        tsp = cm_FindServer(&tmp, type, FALSE);
        if ( tsp )		/* an existing server - ref count increased */
        {
            lock_ObtainMutex(&tsp->mx);
            tsp->ipRank = rank;
            _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PREF_SET);
	    tsp->adminRank = tsp->ipRank;
            lock_ReleaseMutex(&tsp->mx);

            switch (type) {
            case CM_SERVER_FILE:
                /*
                 * find volumes which might have RO copy
                 * on server and change the ordering of
                 * their RO list
                 */
                cm_ChangeRankVolume(tsp);
                break;
            case CM_SERVER_VLDB:
                /* set preferences for an existing vlserver */
                cm_ChangeRankCellVLServer(tsp);
                break;
            }
        }
        else	/* add a new server without a cell */
        {
            tsp = cm_NewServer(&tmp, type, NULL, NULL, CM_FLAG_NOPROBE); /* refcount = 1 */
            lock_ObtainMutex(&tsp->mx);
            tsp->ipRank = rank;
            _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PREF_SET);
	    tsp->adminRank = tsp->ipRank;
            lock_ReleaseMutex(&tsp->mx);
            tsp->ipRank = rank;
        }
	cm_PutServer(tsp);  /* decrease refcount */
    }
    return 0;
}

/*
 * VIOC_GETSPREFS internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetSPrefs(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    cm_SPrefRequest_t *spin; /* input */
    cm_SPrefInfo_t    *spout;   /* output */
    cm_SPref_t        *srvout;   /* one output component */
    cm_server_t       *tsp;
    int 		  i, vlonly, noServers;

    spin      = (cm_SPrefRequest_t *)ioctlp->inDatap;
    spout     = (cm_SPrefInfo_t *) ioctlp->outDatap;
    srvout    = spout->servers;
    noServers = spin->num_servers;
    vlonly    = spin->flags & CM_SPREF_VLONLY;
    spout->num_servers = 0;

    lock_ObtainRead(&cm_serverLock); /* get server lock */

    for (tsp=cm_allServersp, i=0; tsp && noServers; tsp=tsp->allNextp,i++){
        if (spin->offset > i) {
            continue;    /* catch up to where we left off */
        }

        if ( vlonly && (tsp->type != CM_SERVER_VLDB) )
            continue;   /* ignore fileserver for -vlserver option*/
        if ( !vlonly && (tsp->type != CM_SERVER_FILE) )
            continue;   /* ignore vlservers */

        srvout->host = tsp->addr.sin_addr;
        srvout->rank = tsp->ipRank;
        srvout++;
        spout->num_servers++;
        noServers--;
    }
    lock_ReleaseRead(&cm_serverLock); /* release server lock */

    if ( tsp ) 	/* we ran out of space in the output buffer */
        spout->next_offset = i;
    else
        spout->next_offset = 0;
    ioctlp->outDatap += sizeof(cm_SPrefInfo_t) +
        (spout->num_servers -1 ) * sizeof(cm_SPref_t) ;
    return 0;
}


/*
 * VIOC_AFS_CREATE_MT_PT internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * dscp is held but not locked.
 */
afs_int32
cm_IoctlCreateMountPoint(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *dscp, cm_req_t *reqp, clientchar_t *leaf)
{
    afs_int32 code;
    cm_attr_t tattr;
    clientchar_t *cp;
    fschar_t mpInfo[512];           /* mount point string */
    fschar_t fullCell[CELL_MAXNAMELEN];
    fschar_t *fscell = NULL;
    fschar_t *fsvolume = NULL;
    clientchar_t volume[VL_MAXNAMELEN];
    clientchar_t *mpp = NULL;
    clientchar_t *cell = NULL;
    cm_volume_t *volp = NULL;
    cm_cell_t *cellp = NULL;
    size_t len;

   /*
     * The fs command allows the user to specify partial cell names on NT.  These must
     * be expanded to the full cell name for mount points so that the mount points will
     * work on UNIX clients.
     */

    /* Extract the possibly partial cell name */
    mpp = cm_ParseIoctlStringAlloc(ioctlp, NULL);
    cell = cm_ClientCharNext(mpp);
    if (cp = cm_ClientStrChr(cell, ':')) {

        /* Extract the volume name */
        *cp = 0;
        cm_ClientStrCpy(volume, lengthof(volume), cm_ClientCharNext(cp));

        fscell = cm_ClientStringToFsStringAlloc(cell, -1, NULL);
        fsvolume = cm_ClientStringToFsStringAlloc(volume, -1, NULL);

        /* Get the full name for this cell */
        cellp = cm_GetCell_Gen(fscell, fullCell, CM_FLAG_NOPROBE);
        if (!cellp) {
            code = CM_ERROR_NOSUCHCELL;
            goto done;
        }

        StringCbPrintfA(mpInfo, sizeof(mpInfo), "%c%s:%s.", (char) *mpp,
                        fullCell, fsvolume);

    } else {
        /* No cell name specified, so cell points at the volume instead. */
        fsvolume = cm_ClientStringToFsStringAlloc(cell, -1, NULL);
        cm_ClientStringToFsString(mpp, -1, mpInfo, lengthof(mpInfo));
        cellp = cm_FindCellByID(dscp->fid.cell, CM_FLAG_NOPROBE);
    }

    /* remove the trailing dot if it is present */
    len = strlen(fsvolume);
    if (len > 1 && fsvolume[len-1] == '.')
        fsvolume[len-1] = '\0';

    /* validate the target info */
    if (cm_VolNameIsID(fsvolume)) {
        code = cm_FindVolumeByID(cellp, atoi(fsvolume), userp, reqp,
                                CM_GETVOL_FLAG_CREATE, &volp);
    } else {
        code = cm_FindVolumeByName(cellp, fsvolume, userp, reqp,
                                  CM_GETVOL_FLAG_CREATE, &volp);
    }
    if (code)
        goto done;

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* we are adding the mount point to the root dir, so call
         * the freelance code to do the add. */
        fschar_t * fsleaf = cm_ClientStringToFsStringAlloc(leaf, -1, NULL);
        osi_Log0(afsd_logp,"IoctlCreateMountPoint within Freelance root dir");
        code = cm_FreelanceAddMount(fsleaf, fullCell, fsvolume, *mpInfo == '%', NULL);
        free(fsleaf);
    } else
#endif
    {
        /* create the symlink with mode 644.  The lack of X bits tells
         * us that it is a mount point.
         */
        tattr.mask = CM_ATTRMASK_UNIXMODEBITS | CM_ATTRMASK_CLIENTMODTIME;
        tattr.unixModeBits = 0644;
        tattr.clientModTime = time(NULL);

        code = cm_SymLink(dscp, leaf, mpInfo, 0, &tattr, userp, reqp, NULL);
    }

    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_ADDED,
                         FILE_NOTIFY_CHANGE_DIR_NAME,
                         dscp, leaf, NULL, TRUE);

  done:
    if (volp)
        cm_PutVolume(volp);
    if (mpp)
        free(mpp);
    if (fscell)
        free(fscell);
    if (fsvolume)
        free(fsvolume);

    return code;
}

/*
 * VIOC_SYMLINK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * dscp is held but not locked.
 */
afs_int32
cm_IoctlSymlink(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *dscp, cm_req_t *reqp, clientchar_t *leaf)
{
    afs_int32 code;
    cm_attr_t tattr;
    char *cp;

    if (!(ioctlp->flags & CM_IOCTLFLAG_USEUTF8)) {
        /* Translate chars for the linked to name */
        TranslateExtendedChars(ioctlp->inDatap);
    }

    cp = ioctlp->inDatap;		/* contents of link */

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* we are adding the symlink to the root dir., so call
         * the freelance code to do the add. */
        fschar_t *fsleaf;

        if (cp[0] == cp[1] && cp[1] == '\\' &&
            !_strnicmp(cm_NetbiosName,cp+2,strlen(cm_NetbiosName)))
        {
            /* skip \\AFS\ or \\AFS\all\ */
            char * p;
            p = cp + 2 + strlen(cm_NetbiosName) + 1;
            if ( !_strnicmp("all", p, 3) )
                p += 4;
            cp = p;
        }

        osi_Log0(afsd_logp,"IoctlSymlink within Freelance root dir");
        fsleaf = cm_ClientStringToFsStringAlloc(leaf, -1, NULL);
        code = cm_FreelanceAddSymlink(fsleaf, cp, NULL);
        free(fsleaf);
    } else
#endif
    {
        /* Create symlink with mode 0755. */
        tattr.mask = CM_ATTRMASK_UNIXMODEBITS;
        tattr.unixModeBits = 0755;

        code = cm_SymLink(dscp, leaf, cp, 0, &tattr, userp, reqp, NULL);
    }

    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_ADDED,
                          FILE_NOTIFY_CHANGE_FILE_NAME
                          | FILE_NOTIFY_CHANGE_DIR_NAME,
                          dscp, leaf, NULL, TRUE);
    return code;
}


/*
 * VIOC_LISTSYMLINK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * dscp is held but not locked.
 */
afs_int32
cm_IoctlListlink(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *dscp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_scache_t *scp;
    char *cp;
    cm_space_t *spacep;
    cm_scache_t *newRootScp;
    clientchar_t *clientp;

    if (!(ioctlp->flags & CM_IOCTLFLAG_USEUTF8)) {
        /* Translate chars for the link name */
        TranslateExtendedChars(ioctlp->inDatap);
    }
    cp = ioctlp->inDatap;

    clientp = cm_Utf8ToClientStringAlloc(cp, -1, NULL);
    code = cm_Lookup(dscp, clientp[0] ? clientp : L".", CM_FLAG_NOMOUNTCHASE, userp, reqp, &scp);
    free(clientp);
    if (code)
        return code;

    /* Check that it's a real symlink */
    if (scp->fileType != CM_SCACHETYPE_SYMLINK &&
        scp->fileType != CM_SCACHETYPE_DFSLINK &&
        scp->fileType != CM_SCACHETYPE_INVALID) {
        cm_ReleaseSCache(scp);
        return CM_ERROR_INVAL;
    }

    code = cm_AssembleLink(scp, "", &newRootScp, &spacep, userp, reqp);
    cm_ReleaseSCache(scp);
    if (code == 0) {
        char * linkstr;
        cp = ioctlp->outDatap;
        if (newRootScp != NULL) {
            StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), cm_mountRoot);
            StringCbCatA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), "/");
            cp += strlen(cp);
        }

        linkstr = cm_ClientStringToFsStringAlloc(spacep->wdata, -1, NULL);
        StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), linkstr);
        cp += strlen(cp) + 1;
        ioctlp->outDatap = cp;
        cm_FreeSpace(spacep);
        free(linkstr);
        if (newRootScp != NULL)
            cm_ReleaseSCache(newRootScp);
        code = 0;
    } else if (code == CM_ERROR_PATH_NOT_COVERED &&
                scp->fileType == CM_SCACHETYPE_DFSLINK ||
               code == CM_ERROR_NOSUCHPATH &&
                scp->fileType == CM_SCACHETYPE_INVALID) {

        cp = ioctlp->outDatap;
        StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), scp->mountPointStringp);
        cp += strlen(cp) + 1;
        ioctlp->outDatap = cp;
        code = 0;
    }

    return code;
}

/*
 * VIOC_ISSYMLINK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * dscp is held but not locked.
 */
afs_int32
cm_IoctlIslink(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *dscp, cm_req_t *reqp)
{/*CHECK FOR VALID SYMLINK*/
    afs_int32 code;
    cm_scache_t *scp;
    char *cp;
    clientchar_t *clientp;

    if (!(ioctlp->flags & CM_IOCTLFLAG_USEUTF8)) {
        /* Translate chars for the link name */
        TranslateExtendedChars(ioctlp->inDatap);
    }
    cp = ioctlp->inDatap;

    clientp = cm_Utf8ToClientStringAlloc(cp, -1, NULL);
    code = cm_Lookup(dscp, clientp[0] ? clientp : L".", CM_FLAG_NOMOUNTCHASE, userp, reqp, &scp);
    free(clientp);
    if (code)
        return code;

    /* Check that it's a real symlink */
    if (scp->fileType != CM_SCACHETYPE_SYMLINK &&
        scp->fileType != CM_SCACHETYPE_DFSLINK &&
        scp->fileType != CM_SCACHETYPE_INVALID)
        code = CM_ERROR_INVAL;
    cm_ReleaseSCache(scp);
    return code;
}

/*
 * VIOC_DELSYMLINK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * dscp is held but not locked.
 */
afs_int32
cm_IoctlDeletelink(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *dscp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_scache_t *scp;
    char *cp;
    char * originalName = NULL;
    cm_dirOp_t dirop;
    clientchar_t *clientp;

    if (!(ioctlp->flags & CM_IOCTLFLAG_USEUTF8)) {
        /* Translate chars for the link name */
        TranslateExtendedChars(ioctlp->inDatap);
    }
    cp = ioctlp->inDatap;

    clientp = cm_Utf8ToClientStringAlloc(cp, -1, NULL);
    code = cm_Lookup(dscp, clientp[0] ? clientp : L".", CM_FLAG_NOMOUNTCHASE, userp, reqp, &scp);

    /* if something went wrong, bail out now */
    if (code)
        goto done3;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code)
        goto done2;

    /* now check that this is a real symlink */
    if (scp->fileType != CM_SCACHETYPE_SYMLINK &&
        scp->fileType != CM_SCACHETYPE_DFSLINK &&
        scp->fileType != CM_SCACHETYPE_INVALID) {
        code = CM_ERROR_INVAL;
        goto done1;
    }

    /* time to make the RPC, so drop the lock */
    lock_ReleaseWrite(&scp->rw);

#ifdef USE_BPLUS
    code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_READ,
                         CM_DIROP_FLAG_NONE, &dirop);
    if (code == 0) {
        code = cm_BPlusDirLookupOriginalName(&dirop, clientp, &originalName);
        /* cm_Dir*() functions can't be used to lookup the original
           name since those functions only know of the original
           name. */
        cm_EndDirOp(&dirop);
    }
#endif

    /* If this name doesn't have a non-normalized name associated with
       it, we assume that what we had is what is actually present on
       the file server. */

    if (originalName == NULL)
        originalName = cp;

    /* cp is a normalized name.  originalName is the actual name we
       saw on the fileserver. */


#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && dscp == cm_data.rootSCachep) {
        /* we are adding the mount point to the root dir., so call
         * the freelance code to do the add. */
        osi_Log0(afsd_logp,"IoctlDeletelink from Freelance root dir");
        code = cm_FreelanceRemoveSymlink(originalName);
    } else
#endif
    {
        /* easier to do it this way */
        code = cm_Unlink(dscp, originalName, clientp, userp, reqp);
    }
    if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
        smb_NotifyChange(FILE_ACTION_REMOVED,
                          FILE_NOTIFY_CHANGE_FILE_NAME
                          | FILE_NOTIFY_CHANGE_DIR_NAME,
                          dscp, clientp, NULL, TRUE);

    if (originalName != NULL && originalName != cp) {
        free(originalName);
        originalName = NULL;
    }

    lock_ObtainWrite(&scp->rw);
  done1:
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

  done2:
    cm_DiscardSCache(scp);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

  done3:
    free(clientp);

    return code;
}

#ifdef QUERY_AFSID
/* Utility function.  Not currently used.
 * This function performs a PTS lookup which has traditionally
 * not been performed by the cache manager.
 */
afs_int32
cm_UsernameToId(char *uname, cm_ucell_t * ucellp, afs_uint32* uid)
{
    afs_int32 code;
    namelist lnames;
    idlist lids;
    static struct afsconf_cell info;
    struct rx_connection *serverconns[MAXSERVERS];
    struct rx_securityClass *sc[3];
    afs_int32 scIndex = 2;	/* authenticated - we have a token */
    struct ubik_client *pruclient = NULL;
    struct afsconf_dir *tdir;
    int i;
    char * p, * r;

    memset(&info, 0, sizeof(info));
    tdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    code = afsconf_GetCellInfo(tdir, ucellp->cellp->name, "afsprot", &info);
    afsconf_Close(tdir);

    sc[0] = 0;
    sc[1] = 0;
    sc[2] = 0;

    /* we have the token that was given to us in the settoken
     * call.   we just have to use it.
     */
    scIndex = 2;	/* kerberos ticket */
    sc[2] = rxkad_NewClientSecurityObject(rxkad_clear, &ucellp->sessionKey,
					  ucellp->kvno, ucellp->ticketLen,
					  ucellp->ticketp);

    memset(serverconns, 0, sizeof(serverconns));	/* terminate list!!! */
    for (i = 0; i < info.numServers; i++)
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, PRSRV, sc[scIndex],
			     scIndex);

    code = ubik_ClientInit(serverconns, &pruclient);
    if (code) {
        if (info.linkedCell)
            free(info.linkedCell);
	return code;
    }

    code = rxs_Release(sc[scIndex]);

    lids.idlist_len = 0;
    lids.idlist_val = 0;
    lnames.namelist_len = 1;
    lnames.namelist_val = (prname *) malloc(PR_MAXNAMELEN);
    strncpy(lnames.namelist_val[0], uname, PR_MAXNAMELEN);
    lnames.namelist_val[0][PR_MAXNAMELEN-1] = '\0';
    for ( p=lnames.namelist_val[0], r=NULL; *p; p++ ) {
	if (isupper(*p))
	    *p = tolower(*p);
	if (*p == '@')
	    r = p;
    }
    if (r && !cm_stricmp_utf8(r+1,ucellp->cellp->name))
	*r = '\0';

    code = ubik_PR_NameToID(pruclient, 0, &lnames, &lids);
    if (lids.idlist_val) {
	*uid = *lids.idlist_val;
	free(lids.idlist_val);
    }
    if (lnames.namelist_val)
	free(lnames.namelist_val);

    if ( pruclient ) {
	ubik_ClientDestroy(pruclient);
	pruclient = NULL;
    }

    if (info.linkedCell)
        free(info.linkedCell);
    return 0;
}
#endif /* QUERY_AFSID */

#if 0
/* This has been copied to smb_IoctlSetToken in its entirety.
 * An equivalent version will need to be produced for the
 * redirector and some extensive refactoring might be required.
 */
afs_int32
cm_IoctlSetToken(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    char *saveDataPtr;
    char *tp;
    int ticketLen;
    char *ticket;
    int ctSize;
    struct ClearToken ct;
    cm_cell_t *cellp;
    cm_ucell_t *ucellp;
    char *uname = NULL;
    afs_uuid_t uuid;
    int flags;
    char sessionKey[8];
    char *smbname;
    int release_userp = 0;
    char * wdir = NULL;

    saveDataPtr = ioctlp->inDatap;

    cm_SkipIoctlPath(ioctlp);

    tp = ioctlp->inDatap;

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
    if (ioctlp->inCopied > tp - saveDataPtr) {
        /* flags:  logon flag */
        memcpy(&flags, tp, sizeof(int));
        tp += sizeof(int);

        /* cell name */
        cellp = cm_GetCell(tp, CM_FLAG_CREATE | CM_FLAG_NOPROBE);
        if (!cellp)
            return CM_ERROR_NOSUCHCELL;
        tp += strlen(tp) + 1;

        /* user name */
        uname = tp;
        tp += strlen(tp) + 1;

        if (flags & PIOCTL_LOGON) {
            /* SMB user name with which to associate tokens */
            smbname = tp;
            osi_Log2(smb_logp,"cm_IoctlSetToken for user [%s] smbname [%s]",
                     osi_LogSaveString(smb_logp,uname), osi_LogSaveString(smb_logp,smbname));
            fprintf(stderr, "SMB name = %s\n", smbname);
            tp += strlen(tp) + 1;
        } else {
            osi_Log1(smb_logp,"cm_IoctlSetToken for user [%s]",
                     osi_LogSaveString(smb_logp, uname));
        }

		/* uuid */
        memcpy(&uuid, tp, sizeof(uuid));
        if (!cm_FindTokenEvent(uuid, sessionKey))
            return CM_ERROR_INVAL;
    } else {
        cellp = cm_data.rootCellp;
        osi_Log0(smb_logp,"cm_IoctlSetToken - no name specified");
    }

    if (flags & PIOCTL_LOGON) {
        userp = smb_FindCMUserByName(smbname, ioctlp->fidp->vcp->rname,
				     SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
	release_userp = 1;
    }

    /* store the token */
    lock_ObtainMutex(&userp->mx);
    ucellp = cm_GetUCell(userp, cellp);
    osi_Log1(smb_logp,"cm_IoctlSetToken ucellp %lx", ucellp);
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
        StringCbCopyA(ucellp->userName, MAXKTCNAMELEN, uname);
#ifdef QUERY_AFSID
	cm_UsernameToId(uname, ucellp, &ucellp->uid);
#endif
    }
    _InterlockedOr(&ucellp->flags, CM_UCELLFLAG_RXKAD);
    lock_ReleaseMutex(&userp->mx);

    if (flags & PIOCTL_LOGON) {
        ioctlp->flags |= CM_IOCTLFLAG_LOGON;
    }

    cm_ResetACLCache(cellp, userp);

    if (release_userp)
	cm_ReleaseUser(userp);

    return 0;
}
#endif

/*
 * VIOC_GETTOK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetTokenIter(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    char *tp, *cp;
    int iterator;
    int temp;
    cm_ucell_t *ucellp;
    struct ClearToken ct;

    tp = ioctlp->inDatap;
    cp = ioctlp->outDatap;

    /* iterator */
    memcpy(&iterator, tp, sizeof(iterator));
    tp += sizeof(iterator);

    lock_ObtainMutex(&userp->mx);

    /* look for token */
    for (;;iterator++) {
        ucellp = cm_FindUCell(userp, iterator);
        if (!ucellp) {
            lock_ReleaseMutex(&userp->mx);
            return CM_ERROR_NOMORETOKENS;
        }
        if (ucellp->flags & CM_UCELLFLAG_RXKAD)
            break;
    }

    /* new iterator */
    temp = ucellp->iterator + 1;
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* ticket length */
    memcpy(cp, &ucellp->ticketLen, sizeof(ucellp->ticketLen));
    cp += sizeof(ucellp->ticketLen);

    /* ticket */
    memcpy(cp, ucellp->ticketp, ucellp->ticketLen);
    cp += ucellp->ticketLen;

    /* clear token size */
    temp = sizeof(ct);
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* clear token */
    ct.AuthHandle = ucellp->kvno;

    /*
     * This field is supposed to hold the session key
     * but we don't want to make it easier for someone
     * to attack the cache.  The user gave us the session
     * key in the first place.
     */
    memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
    ct.ViceId = 37;			/* XXX */
    ct.BeginTimestamp = 0;		/* XXX */
    ct.EndTimestamp = ucellp->expirationTime;
    memcpy(cp, &ct, sizeof(ct));
    cp += sizeof(ct);

    /* Primary flag (unused) */
    temp = 0;
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* cell name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->cellp->name);
    cp += strlen(cp) + 1;

    /* user name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->userName);
    cp += strlen(cp) + 1;

    ioctlp->outDatap = cp;

    lock_ReleaseMutex(&userp->mx);

    return 0;
}

/*
 * VIOC_NEWGETTOK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetToken(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    char *cp;
    int temp;
    cm_cell_t *cellp;
    cm_ucell_t *ucellp;
    struct ClearToken ct;
    char *tp;
    afs_uuid_t uuid;

    tp = ioctlp->inDatap;

    cp = ioctlp->outDatap;

    /* cell name is right here */
    cellp = cm_GetCell(tp, 0);
    if (!cellp)
        return CM_ERROR_NOSUCHCELL;
    tp += strlen(tp) + 1;

    /* uuid */
    memcpy(&uuid, tp, sizeof(uuid));

    lock_ObtainMutex(&userp->mx);

    ucellp = cm_GetUCell(userp, cellp);
    if (!ucellp || !(ucellp->flags & CM_UCELLFLAG_RXKAD)) {
        lock_ReleaseMutex(&userp->mx);
        return CM_ERROR_NOMORETOKENS;
    }

    /* ticket length */
    memcpy(cp, &ucellp->ticketLen, sizeof(ucellp->ticketLen));
    cp += sizeof(ucellp->ticketLen);

    /* ticket */
    memcpy(cp, ucellp->ticketp, ucellp->ticketLen);
    cp += ucellp->ticketLen;

    /* clear token size */
    temp = sizeof(ct);
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* clear token */
    ct.AuthHandle = ucellp->kvno;

    /* do not give out the session key */
    memset(ct.HandShakeKey, 0, sizeof(ct.HandShakeKey));
    ct.ViceId = 37;			/* XXX */
    ct.BeginTimestamp = 0;		/* XXX */
    ct.EndTimestamp = ucellp->expirationTime;
    memcpy(cp, &ct, sizeof(ct));
    cp += sizeof(ct);

    /* Primary flag (unused) */
    temp = 0;
    memcpy(cp, &temp, sizeof(temp));
    cp += sizeof(temp);

    /* cell name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->cellp->name);
    cp += strlen(cp) + 1;

    /* user name */
    StringCbCopyA(cp, SMB_IOCTL_MAXDATA - (cp - ioctlp->outAllocp), ucellp->userName);
    cp += strlen(cp) + 1;

    ioctlp->outDatap = cp;

    lock_ReleaseMutex(&userp->mx);

    cm_RegisterNewTokenEvent(uuid, ucellp->sessionKey.data, NULL);

    return 0;
}

/*
 * VIOCDELTOK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlDelToken(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    char *cp;
    cm_cell_t *cellp;
    cm_ucell_t *ucellp;

    cp = ioctlp->outDatap;

    /* cell name is right here */
    cellp = cm_GetCell(ioctlp->inDatap, 0);
    if (!cellp)
        return CM_ERROR_NOSUCHCELL;

    lock_ObtainMutex(&userp->mx);

    ucellp = cm_GetUCell(userp, cellp);
    if (!ucellp) {
        lock_ReleaseMutex(&userp->mx);
        return CM_ERROR_NOMORETOKENS;
    }

    osi_Log1(smb_logp,"cm_IoctlDelToken ucellp %lx", ucellp);

    if (ucellp->ticketp) {
        free(ucellp->ticketp);
        ucellp->ticketp = NULL;
    }
    ucellp->ticketLen = 0;
    memset(ucellp->sessionKey.data, 0, 8);
    ucellp->kvno = 0;
    ucellp->expirationTime = 0;
    ucellp->userName[0] = '\0';
    _InterlockedAnd(&ucellp->flags, ~CM_UCELLFLAG_RXKAD);
    ucellp->gen++;

    lock_ReleaseMutex(&userp->mx);

    cm_ResetACLCache(cellp, userp);

    return 0;
}

/*
 * VIOCDELALLTOK internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlDelAllToken(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    cm_ucell_t *ucellp;

    lock_ObtainMutex(&userp->mx);

    for (ucellp = userp->cellInfop; ucellp; ucellp = ucellp->nextp) {
        osi_Log1(smb_logp,"cm_IoctlDelAllToken ucellp %lx", ucellp);

	if (ucellp->ticketp) {
	    free(ucellp->ticketp);
	    ucellp->ticketp = NULL;
	}
	ucellp->ticketLen = 0;
	memset(ucellp->sessionKey.data, 0, 8);
	ucellp->kvno = 0;
	ucellp->expirationTime = 0;
	ucellp->userName[0] = '\0';
        _InterlockedAnd(&ucellp->flags, ~CM_UCELLFLAG_RXKAD);
        ucellp->gen++;
    }

    lock_ReleaseMutex(&userp->mx);

    cm_ResetACLCache(NULL, userp);

    return 0;
}

/*
 * VIOC_MAKESUBMOUNT internals.  (This function should be deprecated)
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlMakeSubmount(cm_ioctl_t *ioctlp, cm_user_t *userp)
{
    char afspath[MAX_PATH];
    char *submountreqp;
    int nextAutoSubmount;
    HKEY hkSubmounts;
    DWORD dwType, dwSize;
    DWORD status;
    DWORD dwIndex;
    DWORD dwSubmounts;

    /* Serialize this one, to prevent simultaneous mods
     * to afsdsbmt.ini
     */
    lock_ObtainMutex(&cm_Afsdsbmt_Lock);

    /* Parse the input parameters--first the required afs path,
     * then the requested submount name (which may be "").
     */
    cm_NormalizeAfsPathAscii(afspath, sizeof(afspath), ioctlp->inDatap);
    submountreqp = ioctlp->inDatap + (strlen(ioctlp->inDatap)+1);

    /* If the caller supplied a suggested submount name, see if
     * that submount name is in use... if so, the submount's path
     * has to match our path.
     */

    RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                    AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                    0,
                    "AFS",
                    REG_OPTION_NON_VOLATILE,
                    KEY_READ|KEY_WRITE|KEY_QUERY_VALUE,
                    NULL,
                    &hkSubmounts,
                    NULL );

    if (submountreqp && *submountreqp) {
        char submountPath[MAX_PATH];

        dwSize = sizeof(submountPath);
        status = RegQueryValueEx( hkSubmounts, submountreqp, 0,
                                  &dwType, submountPath, &dwSize);

        if (status != ERROR_SUCCESS) {

            /* The suggested submount name isn't in use now--
             * so we can safely map the requested submount name
             * to the supplied path. Remember not to write the
             * leading "/afs" when writing out the submount.
             */
            RegSetValueEx( hkSubmounts, submountreqp, 0,
                           REG_EXPAND_SZ,
                           (strlen(&afspath[strlen(cm_mountRoot)])) ?
                           &afspath[strlen(cm_mountRoot)]:"/",
                           (strlen(&afspath[strlen(cm_mountRoot)])) ?
                           (DWORD)strlen(&afspath[strlen(cm_mountRoot)])+1:2);

            RegCloseKey( hkSubmounts );
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), submountreqp);
            ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
            lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
            return 0;
        }

        /* The suggested submount name is already in use--if the
         * supplied path matches the submount's path, we can still
         * use the suggested submount name.
         */
        if (!strcmp (submountPath, afspath)) {
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), submountreqp);
            ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
            RegCloseKey( hkSubmounts );
            lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
            return 0;
        }
    }

    RegQueryInfoKey( hkSubmounts,
                     NULL,  /* lpClass */
                     NULL,  /* lpcClass */
                     NULL,  /* lpReserved */
                     NULL,  /* lpcSubKeys */
                     NULL,  /* lpcMaxSubKeyLen */
                     NULL,  /* lpcMaxClassLen */
                     &dwSubmounts, /* lpcValues */
                     NULL,  /* lpcMaxValueNameLen */
                     NULL,  /* lpcMaxValueLen */
                     NULL,  /* lpcbSecurityDescriptor */
                     NULL   /* lpftLastWriteTime */
                     );


    /* Having obtained a list of all available submounts, start
     * searching that list for a path which matches the requested
     * AFS path. We'll also keep track of the highest "auto15"/"auto47"
     * submount, in case we need to add a new one later.
     */

    nextAutoSubmount = 1;

    for ( dwIndex = 0; dwIndex < dwSubmounts; dwIndex ++ ) {
        char submountPath[MAX_PATH] = "";
        DWORD submountPathLen = sizeof(submountPath);
        char submountName[MAX_PATH];
        DWORD submountNameLen = sizeof(submountName);

        dwType = 0;
        RegEnumValue( hkSubmounts, dwIndex, submountName, &submountNameLen, NULL,
                      &dwType, submountPath, &submountPathLen);
        if (dwType == REG_EXPAND_SZ) {
            char buf[MAX_PATH];
            StringCbCopyA(buf, MAX_PATH, submountPath);
            submountPathLen = ExpandEnvironmentStrings(buf, submountPath, MAX_PATH);
            if (submountPathLen > MAX_PATH)
                continue;
        }

        /* If this is an Auto### submount, remember its ### value */
        if ((!strnicmp (submountName, "auto", 4)) &&
             (isdigit (submountName[strlen("auto")]))) {
            int thisAutoSubmount;
            thisAutoSubmount = atoi (&submountName[strlen("auto")]);
            nextAutoSubmount = max (nextAutoSubmount,
                                     thisAutoSubmount+1);
        }

        if ((submountPathLen == 0) ||
             (submountPathLen == sizeof(submountPath) - 1)) {
            continue;
        }

        /* See if the path for this submount matches the path
         * that our caller specified. If so, we can return
         * this submount.
         */
        if (!strcmp (submountPath, afspath)) {
            StringCbCopyA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), submountName);
            ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
            RegCloseKey(hkSubmounts);
            lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
            return 0;

        }
    }

    /* We've been through the entire list of existing submounts, and
     * didn't find any which matched the specified path. So, we'll
     * just have to add one. Remember not to write the leading "/afs"
     * when writing out the submount.
     */

    StringCbPrintfA(ioctlp->outDatap, SMB_IOCTL_MAXDATA - (ioctlp->outDatap - ioctlp->outAllocp), "auto%ld", nextAutoSubmount);

    RegSetValueEx( hkSubmounts,
                   ioctlp->outDatap,
                   0,
                   REG_EXPAND_SZ,
                   (strlen(&afspath[strlen(cm_mountRoot)])) ?
                   &afspath[strlen(cm_mountRoot)]:"/",
                   (strlen(&afspath[strlen(cm_mountRoot)])) ?
                   (DWORD)strlen(&afspath[strlen(cm_mountRoot)])+1:2);

    ioctlp->outDatap += strlen(ioctlp->outDatap) +1;
    RegCloseKey(hkSubmounts);
    lock_ReleaseMutex(&cm_Afsdsbmt_Lock);
    return 0;
}

/*
 * VIOC_GETRXKCRYPT internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlGetRxkcrypt(cm_ioctl_t *ioctlp, cm_user_t *userp)
{
    memcpy(ioctlp->outDatap, &cryptall, sizeof(cryptall));
    ioctlp->outDatap += sizeof(cryptall);

    return 0;
}

/*
 * VIOC_SETRXKCRYPT internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlSetRxkcrypt(cm_ioctl_t *ioctlp, cm_user_t *userp)
{
    afs_int32 c = cryptall;

    memcpy(&cryptall, ioctlp->inDatap, sizeof(cryptall));

    if (c != cryptall) {
	if (cryptall == 1)
            LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_ON);
	else if (cryptall == 2)
            LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_AUTH);
	else
            LogEvent(EVENTLOG_INFORMATION_TYPE, MSG_CRYPT_OFF);
    }
    return 0;
}

/*
 * VIOC_RXSTAT_PROC internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlRxStatProcess(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_int32 flags;
    int code = 0;

    memcpy((char *)&flags, ioctlp->inDatap, sizeof(afs_int32));
    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK)) {
        return -1;
    }
    if (flags & AFSCALL_RXSTATS_ENABLE) {
        rx_enableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
        rx_disableProcessRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
        rx_clearProcessRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
    return 0;
}

/*
 * VIOC_RXSTAT_PEER internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlRxStatPeer(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_int32 flags;
    int code = 0;

    memcpy((char *)&flags, ioctlp->inDatap, sizeof(afs_int32));
    if (!(flags & AFSCALL_RXSTATS_MASK) || (flags & ~AFSCALL_RXSTATS_MASK)) {
	return -1;
    }
    if (flags & AFSCALL_RXSTATS_ENABLE) {
        rx_enablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_DISABLE) {
        rx_disablePeerRPCStats();
    }
    if (flags & AFSCALL_RXSTATS_CLEAR) {
        rx_clearPeerRPCStats(AFS_RX_STATS_CLEAR_ALL);
    }
    return 0;
}

/*
 * VIOC_UNICODECTL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlUnicodeControl(struct cm_ioctl *ioctlp, struct cm_user * userp)
{
    afs_int32 result = 0;
#ifdef SMB_UNICODE
    afs_uint32 cmd;

    memcpy(&cmd, ioctlp->inDatap, sizeof(afs_uint32));

    if (cmd & 2) {
        /* Setting the Unicode flag */
        LONG newflag;

        newflag = ((cmd & 1) == 1);

        InterlockedExchange(&smb_UseUnicode, newflag);
    }

    result = smb_UseUnicode;
#else
    result = 2;
#endif

    memcpy(ioctlp->outDatap, &result, sizeof(result));
    ioctlp->outDatap += sizeof(result);

    return 0;
}

/*
 * VIOC_UUIDCTL internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlUUIDControl(struct cm_ioctl * ioctlp, struct cm_user *userp)
{
    afs_uint32 cmd;
    afsUUID uuid;

    memcpy(&cmd, ioctlp->inDatap, sizeof(afs_uint32));

    if (cmd) {             /* generate a new UUID */
        UuidCreate((UUID *) &uuid);
        cm_data.Uuid = uuid;
	cm_ForceNewConnectionsAllServers();
    }

    memcpy(ioctlp->outDatap, &cm_data.Uuid, sizeof(cm_data.Uuid));
    ioctlp->outDatap += sizeof(cm_data.Uuid);

    return 0;
}



/*
 * functions to dump contents of various structures.
 * In debug build (linked with crt debug library) will dump allocated but not freed memory
 */
extern int cm_DumpSCache(FILE *outputFile, char *cookie, int lock);
extern int cm_DumpBufHashTable(FILE *outputFile, char *cookie, int lock);

/*
 * VIOC_TRACEMEMDUMP internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * dscp is held but not locked.
 */
afs_int32
cm_IoctlMemoryDump(struct cm_ioctl *ioctlp, struct cm_user *userp)
{
    afs_int32 inValue = 0;
    HANDLE hLogFile;
    char logfileName[MAX_PATH+1];
    char *cookie;
    DWORD dwSize;

#ifdef _DEBUG
    static _CrtMemState memstate;
#endif

    memcpy(&inValue, ioctlp->inDatap, sizeof(afs_int32));

    dwSize = GetEnvironmentVariable("TEMP", logfileName, sizeof(logfileName));
    if ( dwSize == 0 || dwSize > sizeof(logfileName) )
    {
        GetWindowsDirectory(logfileName, sizeof(logfileName));
    }
    strncat(logfileName, "\\afsd_alloc.log", sizeof(logfileName));

    hLogFile = CreateFile(logfileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (!hLogFile)
    {
      /* error */
      inValue = -1;
      memcpy(ioctlp->outDatap, &inValue, sizeof(afs_int32));
      ioctlp->outDatap += sizeof(afs_int32);

      return 0;
    }

    SetFilePointer(hLogFile, 0, NULL, FILE_END);

    cookie = inValue ? "b" : "e";

#ifdef _DEBUG

    if (inValue)
    {
      _CrtMemCheckpoint(&memstate);
    }
    else
    {
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, hLogFile);
        _CrtMemDumpAllObjectsSince(&memstate);
    }
#endif

    /* dump all interesting data */
    cm_MemDumpDirStats(hLogFile, cookie, 1);
    cm_MemDumpBPlusStats(hLogFile, cookie, 1);
    cm_DumpCells(hLogFile, cookie, 1);
    cm_DumpVolumes(hLogFile, cookie, 1);
    cm_DumpSCache(hLogFile, cookie, 1);
    cm_DumpBufHashTable(hLogFile, cookie, 1);
    cm_DumpServers(hLogFile, cookie, 1);
    smb_DumpVCP(hLogFile, cookie, 1);
    rx_DumpCalls(hLogFile, cookie);
    rx_DumpPackets(hLogFile, cookie);

    CloseHandle(hLogFile);

    inValue = 0;	/* success */
    memcpy(ioctlp->outDatap, &inValue, sizeof(long));
    ioctlp->outDatap += sizeof(long);

    return 0;
}

/* Utility functon.  Not currently used. */
static afs_int32
cm_CheckServersStatus(cm_serverRef_t *serversp)
{
    afs_int32 code = 0;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    int someBusy = 0, someOffline = 0, allOffline = 1, allBusy = 1, allDown = 1;

    if (serversp == NULL) {
	osi_Log1(afsd_logp, "cm_CheckServersStatus returning 0x%x", CM_ERROR_ALLDOWN);
	return CM_ERROR_ALLDOWN;
    }

    lock_ObtainRead(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (tsrp->status == srv_deleted)
            continue;
        if (tsp = tsrp->server) {
            cm_GetServerNoLock(tsp);
            if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
                allDown = 0;
                if (tsrp->status == srv_busy) {
                    allOffline = 0;
                    someBusy = 1;
                } else if (tsrp->status == srv_offline) {
                    allBusy = 0;
                    someOffline = 1;
                } else {
                    allOffline = 0;
                    allBusy = 0;
                    cm_PutServerNoLock(tsp);
                    goto done;
                }
            }
            cm_PutServerNoLock(tsp);
        }
    }
    lock_ReleaseRead(&cm_serverLock);

    if (allDown)
        code = CM_ERROR_ALLDOWN;
    else if (allBusy)
        code = CM_ERROR_ALLBUSY;
    else if (allOffline || (someBusy && someOffline))
        code = CM_ERROR_ALLOFFLINE;

  done:
    osi_Log1(afsd_logp, "cm_CheckServersStatus returning 0x%x", code);
    return code;
}

/*
 * VIOC_PATH_AVAILABILITY internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlPathAvailability(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_cell_t *cellp;
    cm_volume_t *tvp;
    cm_vol_state_t *statep;
    afs_uint32 volume;

#ifdef AFS_FREELANCE_CLIENT
    if ( scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID ) {
	code = 0;
    } else
#endif
    {
        volume = scp->fid.volume;

        cellp = cm_FindCellByID(scp->fid.cell, 0);

        if (!cellp)
            return CM_ERROR_NOSUCHCELL;

        code = cm_FindVolumeByID(cellp, volume, userp, reqp, CM_GETVOL_FLAG_CREATE, &tvp);
        if (code)
            return code;

        statep = cm_VolumeStateByID(tvp, volume);
        switch (statep->state) {
        case vl_online:
        case vl_unknown:
            code = 0;
            break;
        case vl_busy:
            code = CM_ERROR_ALLBUSY;
            break;
        case vl_offline:
            code = CM_ERROR_ALLOFFLINE;
            break;
        case vl_alldown:
            code = CM_ERROR_ALLDOWN;
            break;
        }
        cm_PutVolume(tvp);
    }
    return code;
}

/*
 * VIOC_VOLSTAT_TEST internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 */
afs_int32
cm_IoctlVolStatTest(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_req_t *reqp)
{
    afs_int32 code;
    cm_cell_t *cellp = NULL;
    cm_volume_t *volp;
    cm_vol_state_t *statep;
    struct VolStatTest * testp;
    afs_uint32 n;

    testp = (struct VolStatTest *)ioctlp->inDatap;

#ifdef AFS_FREELANCE_CLIENT
    if (testp->fid.cell == -1)
        return CM_ERROR_NOACCESS;
#endif

    if (testp->flags & VOLSTAT_TEST_CHECK_VOLUME) {
        cm_CheckOfflineVolumes();
        return 0;
    }

    if (testp->flags & VOLSTAT_TEST_NETWORK_UP) {
        cm_VolStatus_Network_Started(cm_NetbiosName
#ifdef _WIN64
                                  , cm_NetbiosName
#endif
                                  );
        return 0;
    }

    if (testp->flags & VOLSTAT_TEST_NETWORK_DOWN) {
        cm_VolStatus_Network_Stopped(cm_NetbiosName
#ifdef _WIN64
                                  , cm_NetbiosName
#endif
                                  );
        return 0;
    }

    if (testp->cellname[0]) {
        n = atoi(testp->cellname);
        if (n)
            testp->fid.cell = n;
        else
            cellp = cm_GetCell(testp->cellname, 0);
    }

    if (testp->fid.cell > 0) {
        cellp = cm_FindCellByID(testp->fid.cell, 0);
    }

    if (!cellp)
        return CM_ERROR_NOSUCHCELL;

    if (testp->volname[0]) {
        n = atoi(testp->volname);
        if (n)
            testp->fid.volume = n;
        else
            code = cm_FindVolumeByName(cellp, testp->volname, userp, reqp, CM_GETVOL_FLAG_NO_LRU_UPDATE, &volp);
    }

    if (testp->fid.volume > 0)
        code = cm_FindVolumeByID(cellp, testp->fid.volume, userp, reqp, CM_GETVOL_FLAG_NO_LRU_UPDATE, &volp);

    if (code)
        return code;

    if (testp->fid.volume)
        statep = cm_VolumeStateByID(volp, testp->fid.volume);
    else
        statep = cm_VolumeStateByName(volp, testp->volname);

    if (statep) {
        statep->state = testp->state;
        code = cm_VolStatus_Change_Notification(cellp->cellID, statep->ID, testp->state);
    }

    cm_PutVolume(volp);

    return code;
}

/*
 * VIOC_GETUNIXMODE internals.
 *
 * Assumes that pioctl path has been parsed or skipped.
 * scp is held but not locked.
 */
afs_int32
cm_IoctlGetUnixMode(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    char *cp;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    if (code == 0) {
        /* Copy all this junk into msg->im_data, keeping track of the lengths. */
        cp = ioctlp->outDatap;
        memcpy(cp, (char *)&scp->unixModeBits, sizeof(afs_uint32));
        cp += sizeof(afs_uint32);

        /* return new size */
        ioctlp->outDatap = cp;
    }
    return code;
}


/*
 * VIOC_SETUNIXMODE internals.
 *
 * Assumes that pioctl path has been parsed or skipped
 * and that cm_ioctlQueryOptions_t have been parsed and skipped.
 *
 * scp is held but not locked.
 */
afs_int32
cm_IoctlSetUnixMode(struct cm_ioctl *ioctlp, struct cm_user *userp, cm_scache_t *scp, cm_req_t *reqp)
{
    afs_int32 code = 0;
    char *cp;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    if (code == 0) {
        afs_uint32 unixModeBits;
        cm_attr_t attr;

        memset(&attr, 0, sizeof(attr));

        cp = ioctlp->inDatap;
        memcpy((char *)&unixModeBits, cp, sizeof(afs_uint32));

        attr.mask = CM_ATTRMASK_UNIXMODEBITS;
        attr.unixModeBits = unixModeBits;

        code = cm_SetAttr(scp, &attr, userp, reqp);
    }
    return code;
}
