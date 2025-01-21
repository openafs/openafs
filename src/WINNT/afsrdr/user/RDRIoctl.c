/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Copyright (c) 2008 Secure Endpoints, Inc.
 * Copyright (c) 2009-2011 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Secure Endpoints Inc. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software without
 *   specific prior written permission from Secure Endpoints, Inc. and
 *   Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

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

#include "cm_rpc.h"
#include "afs/afsrpc.h"
#include "afs/auth.h"

#include "smb_iocons.h"
#define RDR_IOCTL_PRIVATE 1
#include "RDRIoctl.h"
#include "smb.h"
#include "cm_nls.h"

static RDR_ioctlProc_t *RDR_ioctlProcsp[CM_IOCTL_MAXPROCS];

static RDR_ioctl_t * RDR_allIoctls = NULL, *RDR_allIoctlsLast = NULL;
static osi_rwlock_t  RDR_globalIoctlLock;

extern wchar_t       RDR_UNCName[];

static afs_int32
RDR_ParseIoctlPath(RDR_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                   cm_scache_t **scpp, afs_uint32 flags);

static afs_int32
RDR_ParseIoctlParent(RDR_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                     cm_scache_t **scpp, wchar_t *leafp);

void
RDR_InitIoctl(void)
{
    int i;

    lock_InitializeRWLock(&RDR_globalIoctlLock, "RDR global ioctl lock", LOCK_HIERARCHY_RDR_GLOBAL);

    for (i=0; i<CM_IOCTL_MAXPROCS; i++)
        RDR_ioctlProcsp[i] = NULL;

    RDR_ioctlProcsp[VIOCGETAL] = RDR_IoctlGetACL;
    RDR_ioctlProcsp[VIOC_FILE_CELL_NAME] = RDR_IoctlGetFileCellName;
    RDR_ioctlProcsp[VIOCSETAL] = RDR_IoctlSetACL;
    RDR_ioctlProcsp[VIOC_FLUSHVOLUME] = RDR_IoctlFlushVolume;
    RDR_ioctlProcsp[VIOCFLUSH] = RDR_IoctlFlushFile;
    RDR_ioctlProcsp[VIOCSETVOLSTAT] = RDR_IoctlSetVolumeStatus;
    RDR_ioctlProcsp[VIOCGETVOLSTAT] = RDR_IoctlGetVolumeStatus;
    RDR_ioctlProcsp[VIOCWHEREIS] = RDR_IoctlWhereIs;
    RDR_ioctlProcsp[VIOC_AFS_STAT_MT_PT] = RDR_IoctlStatMountPoint;
    RDR_ioctlProcsp[VIOC_AFS_DELETE_MT_PT] = RDR_IoctlDeleteMountPoint;
    RDR_ioctlProcsp[VIOCCKSERV] = RDR_IoctlCheckServers;
    RDR_ioctlProcsp[VIOC_GAG] = RDR_IoctlGag;
    RDR_ioctlProcsp[VIOCCKBACK] = RDR_IoctlCheckVolumes;
    RDR_ioctlProcsp[VIOCSETCACHESIZE] = RDR_IoctlSetCacheSize;
    RDR_ioctlProcsp[VIOCGETCACHEPARMS] = RDR_IoctlGetCacheParms;
    RDR_ioctlProcsp[VIOCGETCELL] = RDR_IoctlGetCell;
    RDR_ioctlProcsp[VIOCNEWCELL] = RDR_IoctlNewCell;
    RDR_ioctlProcsp[VIOC_GET_WS_CELL] = RDR_IoctlGetWsCell;
    RDR_ioctlProcsp[VIOC_AFS_SYSNAME] = RDR_IoctlSysName;
    RDR_ioctlProcsp[VIOC_GETCELLSTATUS] = RDR_IoctlGetCellStatus;
    RDR_ioctlProcsp[VIOC_SETCELLSTATUS] = RDR_IoctlSetCellStatus;
    RDR_ioctlProcsp[VIOC_SETSPREFS] = RDR_IoctlSetSPrefs;
    RDR_ioctlProcsp[VIOC_GETSPREFS] = RDR_IoctlGetSPrefs;
    RDR_ioctlProcsp[VIOC_STOREBEHIND] = RDR_IoctlStoreBehind;
    RDR_ioctlProcsp[VIOC_AFS_CREATE_MT_PT] = RDR_IoctlCreateMountPoint;
    RDR_ioctlProcsp[VIOC_TRACECTL] = RDR_IoctlTraceControl;
    RDR_ioctlProcsp[VIOCSETTOK] = RDR_IoctlSetToken;
    RDR_ioctlProcsp[VIOCGETTOK] = RDR_IoctlGetTokenIter;
    RDR_ioctlProcsp[VIOCNEWGETTOK] = RDR_IoctlGetToken;
    RDR_ioctlProcsp[VIOCDELTOK] = RDR_IoctlDelToken;
    RDR_ioctlProcsp[VIOCDELALLTOK] = RDR_IoctlDelAllToken;
    RDR_ioctlProcsp[VIOC_SYMLINK] = RDR_IoctlSymlink;
    RDR_ioctlProcsp[VIOC_LISTSYMLINK] = RDR_IoctlListlink;
    RDR_ioctlProcsp[VIOC_DELSYMLINK] = RDR_IoctlDeletelink;
    RDR_ioctlProcsp[VIOC_MAKESUBMOUNT] = RDR_IoctlMakeSubmount;
    RDR_ioctlProcsp[VIOC_GETRXKCRYPT] = RDR_IoctlGetRxkcrypt;
    RDR_ioctlProcsp[VIOC_SETRXKCRYPT] = RDR_IoctlSetRxkcrypt;
    RDR_ioctlProcsp[VIOC_ISSYMLINK] = RDR_IoctlIslink;
    RDR_ioctlProcsp[VIOC_TRACEMEMDUMP] = RDR_IoctlMemoryDump;
    RDR_ioctlProcsp[VIOC_ISSYMLINK] = RDR_IoctlIslink;
    RDR_ioctlProcsp[VIOC_FLUSHALL] = RDR_IoctlFlushAllVolumes;
    RDR_ioctlProcsp[VIOCGETFID] = RDR_IoctlGetFid;
    RDR_ioctlProcsp[VIOCGETOWNER] = RDR_IoctlGetOwner;
    RDR_ioctlProcsp[VIOC_RXSTAT_PROC] = RDR_IoctlRxStatProcess;
    RDR_ioctlProcsp[VIOC_RXSTAT_PEER] = RDR_IoctlRxStatPeer;
    RDR_ioctlProcsp[VIOC_UUIDCTL] = RDR_IoctlUUIDControl;
    RDR_ioctlProcsp[VIOC_PATH_AVAILABILITY] = RDR_IoctlPathAvailability;
    RDR_ioctlProcsp[VIOC_GETFILETYPE] = RDR_IoctlGetFileType;
    RDR_ioctlProcsp[VIOC_VOLSTAT_TEST] = RDR_IoctlVolStatTest;
    RDR_ioctlProcsp[VIOC_UNICODECTL] = RDR_IoctlUnicodeControl;
    RDR_ioctlProcsp[VIOC_SETOWNER] = RDR_IoctlSetOwner;
    RDR_ioctlProcsp[VIOC_SETGROUP] = RDR_IoctlSetGroup;
    RDR_ioctlProcsp[VIOCNEWCELL2] = RDR_IoctlNewCell2;
    RDR_ioctlProcsp[VIOC_GETUNIXMODE] = RDR_IoctlGetUnixMode;
    RDR_ioctlProcsp[VIOC_SETUNIXMODE] = RDR_IoctlSetUnixMode;
    RDR_ioctlProcsp[VIOC_GETVERIFYDATA] = RDR_IoctlGetVerifyData;
    RDR_ioctlProcsp[VIOC_SETVERIFYDATA] = RDR_IoctlSetVerifyData;
    RDR_ioctlProcsp[VIOC_GETCALLERACCESS] = RDR_IoctlGetCallerAccess;
}

void
RDR_ShutdownIoctl(void)
{
    lock_FinalizeRWLock(&RDR_globalIoctlLock);
}

/* called to make a fid structure into an IOCTL fid structure */
void
RDR_SetupIoctl(ULONG index, cm_fid_t *parentFid, cm_fid_t *rootFid, cm_user_t *userp, cm_req_t *reqp)
{
    RDR_ioctl_t *iop;

    lock_ObtainWrite(&RDR_globalIoctlLock);
    for ( iop=RDR_allIoctls; iop; iop=iop->next) {
        if (iop->index == index)
            break;
    }

    if (iop) {
        iop->flags = 0;
        iop->req = *reqp;

        /* we are reusing a previous ioctl */
        if (cm_FidCmp(&iop->parentFid, parentFid)) {
            iop->parentFid = *parentFid;
            if (iop->parentScp) {
                cm_ReleaseSCache(iop->parentScp);
                iop->parentScp = NULL;
            }
            cm_GetSCache(parentFid, NULL, &iop->parentScp, userp, reqp);
            iop->rootFid = *rootFid;
        }
    } else {
        /* need to allocate a new one */
        iop = malloc(sizeof(*iop));
        memset(iop, 0, sizeof(*iop));
        if (RDR_allIoctls == NULL)
            RDR_allIoctls = RDR_allIoctlsLast = iop;
        else {
            iop->prev = RDR_allIoctlsLast;
            RDR_allIoctlsLast->next = iop;
            RDR_allIoctlsLast = iop;
        }
        iop->index = index;
        iop->req = *reqp;
        if (parentFid->cell == 0) {
            iop->parentFid = cm_data.rootFid;
            iop->parentScp = cm_RootSCachep(userp, reqp);
            cm_HoldSCache(iop->parentScp);
        } else {
            iop->parentFid = *parentFid;
            cm_GetSCache(parentFid, NULL, &iop->parentScp, userp, reqp);
        }
        if (rootFid->cell == 0) {
            iop->rootFid = cm_data.rootFid;
        } else {
            iop->rootFid = *rootFid;
        }
    }
    lock_ReleaseWrite(&RDR_globalIoctlLock);
}

static void
RDR_DestroyIoctl(RDR_ioctl_t *iop)
{
    if (iop == NULL)
        return;

    if (iop->parentScp)
        cm_ReleaseSCache(iop->parentScp);

    if (iop->ioctl.inAllocp != NULL) {
	SecureZeroMemory(iop->ioctl.inAllocp, iop->ioctl.inCopied);
        free(iop->ioctl.inAllocp);
    }
    if (iop->ioctl.outAllocp != NULL) {
	SecureZeroMemory(iop->ioctl.outAllocp, iop->ioctl.outCopied);
        free(iop->ioctl.outAllocp);
    }

    if (RDR_allIoctls == RDR_allIoctlsLast)
        RDR_allIoctls = RDR_allIoctlsLast = NULL;
    else {
        if (iop->prev == NULL)
            RDR_allIoctls = iop->next;
        else
            iop->prev->next = iop->next;
        if (iop->next == NULL) {
            RDR_allIoctlsLast = iop->prev;
            iop->prev->next = NULL;
        } else
            iop->next->prev = iop->prev;
    }
    free(iop);
}

void
RDR_CleanupIoctl(ULONG index)
{
    RDR_ioctl_t *iop;

    lock_ObtainWrite(&RDR_globalIoctlLock);
    for ( iop=RDR_allIoctls; iop; iop=iop->next) {
        if (iop->index == index)
            break;
    }

    if (iop) {
        if (iop->refCount == 0) {
            RDR_DestroyIoctl(iop);
        }
        else
        {
            iop->flags |= RDR_IOCTL_FLAG_CLEANED;
        }
    }
    lock_ReleaseWrite(&RDR_globalIoctlLock);
}

/* called when we receive a write call.  If this is the first write call after
 * a series of reads (or the very first call), then we start a new call.
 * We also ensure that things are properly initialized for the start of a call.
 */
void
RDR_IoctlPrepareWrite(RDR_ioctl_t *ioctlp)
{
    /* make sure the buffer(s) are allocated */
    if (!ioctlp->ioctl.inAllocp)
        ioctlp->ioctl.inAllocp = malloc(CM_IOCTL_MAXDATA);
    if (!ioctlp->ioctl.outAllocp)
        ioctlp->ioctl.outAllocp = malloc(CM_IOCTL_MAXDATA);

    /* Fixes fs la problem.  We do a StrToOEM later and if this data isn't initialized we get memory issues. */
    (void) memset(ioctlp->ioctl.inAllocp, 0, CM_IOCTL_MAXDATA);
    (void) memset(ioctlp->ioctl.outAllocp, 0, CM_IOCTL_MAXDATA);

    /* and make sure that we've reset our state for the new incoming request */
    if (!(ioctlp->ioctl.flags & CM_IOCTLFLAG_DATAIN)) {
        ioctlp->ioctl.inCopied = 0;
        ioctlp->ioctl.outCopied = 0;
        ioctlp->ioctl.inDatap = ioctlp->ioctl.inAllocp;
        ioctlp->ioctl.outDatap = ioctlp->ioctl.outAllocp;
        ioctlp->ioctl.flags |= CM_IOCTLFLAG_DATAIN;
    }
}

/* called when we receive a read call, does the send of the received data if
 * this is the first read call.  This is the function that actually makes the
 * call to the ioctl code.
 */
afs_int32
RDR_IoctlPrepareRead(RDR_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    afs_int32 opcode;
    RDR_ioctlProc_t *procp = NULL;
    afs_int32 code;

    if (ioctlp->ioctl.flags & CM_IOCTLFLAG_DATAIN) {
        ioctlp->ioctl.flags &= ~CM_IOCTLFLAG_DATAIN;

        /* do the call now, or fail if we didn't get an opcode, or
         * enough of an opcode.
         */
        if (ioctlp->ioctl.inCopied < sizeof(afs_int32))
            return CM_ERROR_INVAL;

        memcpy(&opcode, ioctlp->ioctl.inDatap, sizeof(afs_int32));
        ioctlp->ioctl.inDatap += sizeof(afs_int32);

        osi_Log1(afsd_logp, "Ioctl opcode 0x%x", opcode);

        /* check for opcode out of bounds */
        if (opcode < 0 || opcode >= CM_IOCTL_MAXPROCS)
            return CM_ERROR_TOOBIG;

        /* check for no such proc */
	procp = RDR_ioctlProcsp[opcode];
        if (procp == NULL)
            return CM_ERROR_BADOP;

        /* otherwise, make the call */
        ioctlp->ioctl.outDatap += sizeof(afs_int32); /* reserve room for return code */
        code = (*procp)(ioctlp, userp, pflags);

        osi_Log1(afsd_logp, "Ioctl return code 0x%x", code);

        /* copy in return code */
        memcpy(ioctlp->ioctl.outAllocp, &code, sizeof(afs_int32));
    }
    return 0;
}

RDR_ioctl_t *
RDR_FindIoctl(ULONG index)
{
    RDR_ioctl_t *iop;

    lock_ObtainRead(&RDR_globalIoctlLock);
    for ( iop=RDR_allIoctls; iop; iop=iop->next) {
        if (iop->index == index &&
            !(iop->flags & RDR_IOCTL_FLAG_CLEANED)) {
            InterlockedIncrement(&iop->refCount);
            break;
        }
    }
    lock_ReleaseRead(&RDR_globalIoctlLock);
    return iop;
}

void
RDR_ReleaseIoctl(RDR_ioctl_t * iop)
{
    lock_ObtainWrite(&RDR_globalIoctlLock);
    InterlockedDecrement(&iop->refCount);

    if (iop->refCount == 0 &&
        (iop->flags & RDR_IOCTL_FLAG_CLEANED))
    {
        RDR_DestroyIoctl(iop);
    }
    lock_ReleaseWrite(&RDR_globalIoctlLock);
}

/* called from RDR_ReceiveCoreRead when we receive a read on the ioctl fid */
afs_int32
RDR_IoctlRead(cm_user_t *userp, ULONG RequestId, ULONG BufferLength, void *MappedBuffer, ULONG *pBytesProcessed, afs_uint32 pflags)
{
    RDR_ioctl_t *iop;
    afs_uint32 count;
    afs_int32 code = 0;

    iop = RDR_FindIoctl(RequestId);
    if (iop == NULL)
        return CM_ERROR_BADFD;

    /* turn the connection around, if required */
    code = RDR_IoctlPrepareRead(iop, userp, pflags);
    if (code == 0) {

        count = (afs_int32)((iop->ioctl.outDatap - iop->ioctl.outAllocp) - iop->ioctl.outCopied);
        if (BufferLength < count)
            count = BufferLength;

        /* now copy the data into the response packet */
        memcpy((char *)MappedBuffer, iop->ioctl.outCopied + iop->ioctl.outAllocp, count);

        /* and adjust the counters */
        iop->ioctl.outCopied += count;

        *pBytesProcessed = count;

        RDR_ReleaseIoctl(iop);
    }
    return code;
}

/* called from RDR_PioctWrite when we receive a write call on the IOCTL
 * file descriptor.
 */
afs_int32
RDR_IoctlWrite(cm_user_t *userp, ULONG RequestId, ULONG BufferLength, void *MappedBuffer)
{
    afs_int32 code = 0;
    RDR_ioctl_t *iop;

    iop = RDR_FindIoctl(RequestId);
    if (iop == NULL)
        return CM_ERROR_BADFD;

    RDR_IoctlPrepareWrite(iop);

    if (BufferLength + iop->ioctl.inCopied > CM_IOCTL_MAXDATA) {
        code = CM_ERROR_TOOBIG;
    }
    else
    {
        /* copy data */
        memcpy(iop->ioctl.inDatap + iop->ioctl.inCopied, (char *)MappedBuffer, BufferLength);

        /* adjust counts */
        iop->ioctl.inCopied += BufferLength;
    }

    RDR_ReleaseIoctl(iop);
    return code;
}


/* parse the passed-in file name and do a namei on it.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
#define CM_PARSE_FLAG_LITERAL 1

static afs_int32
RDR_ParseIoctlPath(RDR_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                   cm_scache_t **scpp, afs_uint32 flags)
{
    afs_int32 code;
    cm_scache_t *substRootp = NULL;
    cm_scache_t *iscp = NULL;
    char     *inPath;
    wchar_t     *relativePath = NULL;
    wchar_t     *lastComponent = NULL;
    afs_uint32 follow = (flags & CM_PARSE_FLAG_LITERAL ? CM_FLAG_NOMOUNTCHASE : CM_FLAG_FOLLOW);
    int free_path = FALSE;

    inPath = ioctlp->ioctl.inDatap;
    /* setup the next data value for the caller to use */
    ioctlp->ioctl.inDatap += (long)strlen(ioctlp->ioctl.inDatap) + 1;;

    osi_Log1(afsd_logp, "RDR_ParseIoctlPath inPath %s", osi_LogSaveString(afsd_logp,inPath));

    /* This is usually the file name, but for StatMountPoint it is the path. */
    /* ioctlp->inDatap can be either of the form:
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
        osi_Log1(afsd_logp, "RDR_ParseIoctlPath UTF8 relativePath %S",
                 osi_LogSaveStringW(afsd_logp,relativePath));
    } else {
        int cch;
        /* Not a UTF-8 string */
        /* TODO: If this is an OEM string, we should convert it to UTF-8. */
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
        osi_Log1(afsd_logp, "RDR_ParseIoctlPath ASCII relativePath %S",
                 osi_LogSaveStringW(afsd_logp,relativePath));
    }

    if (relativePath[0] == relativePath[1] &&
         relativePath[1] == '\\' &&
         !cm_ClientStrCmpNI(RDR_UNCName,relativePath+2,(int)wcslen(RDR_UNCName)))
    {
        wchar_t shareName[256];
        wchar_t *sharePath;
        int shareFound, i;
        wchar_t *p;

        /* We may have found a UNC path.
         * If the first component is the RDR UNC Name,
         * then throw out the second component (the submount)
         * since it had better expand into the value of ioctl->tidPathp
         */
        p = relativePath + 2 + wcslen(RDR_UNCName) + 1;			/* buffer overflow vuln.? */
        if ( !cm_ClientStrCmpNI(_C("all"), p, 3) )
            p += 4;

        for (i = 0; *p && *p != '\\'; i++,p++ ) {
            shareName[i] = *p;
        }
        p++;                    /* skip past trailing slash */
        shareName[i] = 0;       /* terminate string */

        shareFound = smb_FindShare(NULL, NULL, shareName, &sharePath);
        if ( shareFound ) {
            /* we found a sharename, therefore use the resulting path */
            code = cm_NameI(cm_RootSCachep(userp, reqp), sharePath,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, NULL, reqp, &substRootp);
            free(sharePath);
            if (code) {
		osi_Log1(afsd_logp,"RDR_ParseIoctlPath [1] code 0x%x", code);
                if (free_path)
                    free(relativePath);
                return code;
	    }

	    lastComponent = cm_ClientStrRChr(p, '\\');
	    if (lastComponent && (lastComponent - p) > 1 && wcslen(lastComponent) > 1) {
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
		osi_Log1(afsd_logp,"RDR_ParseIoctlPath [2] code 0x%x", code);
                if (free_path)
                    free(relativePath);
                return code;
	    }
        } else {
            /* otherwise, treat the name as a cellname mounted off the afs root.
             * This requires that we reconstruct the shareName string with
             * leading and trailing slashes.
             */
            p = relativePath + 2 + wcslen(RDR_UNCName) + 1;
            if ( !cm_ClientStrCmpNI(_C("all"), p, 3) )
                p += 4;

            shareName[0] = '/';
            for (i = 1; *p && *p != '\\'; i++,p++ ) {
                shareName[i] = *p;
            }
            p++;                    /* skip past trailing slash */
            shareName[i++] = '/';	/* add trailing slash */
            shareName[i] = 0;       /* terminate string */


            code = cm_NameI(cm_RootSCachep(userp, reqp), shareName,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, NULL, reqp, &substRootp);
            if (code) {
		osi_Log1(afsd_logp,"RDR_ParseIoctlPath [3] code 0x%x", code);
                if (free_path)
                    free(relativePath);
                return code;
	    }

	    lastComponent = cm_ClientStrRChr(p, '\\');
	    if (lastComponent && (lastComponent - p) > 1 && wcslen(lastComponent) > 1) {
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
		osi_Log1(afsd_logp,"RDR_ParseIoctlPath code [4] 0x%x", code);
                if (free_path)
                    free(relativePath);
                return code;
	    }
        }
    } else {
        code = cm_GetSCache(&ioctlp->parentFid, NULL, &substRootp, userp, reqp);
        if (code) {
	    osi_Log1(afsd_logp,"RDR_ParseIoctlPath [6] code 0x%x", code);
            if (free_path)
                free(relativePath);
            return code;
	}

	lastComponent = cm_ClientStrRChr(relativePath, L'\\');
	if (lastComponent && (lastComponent - relativePath) > 1) {
            if (wcslen(lastComponent) == 1) {
                *lastComponent = L'\0';
                lastComponent = L".";
            } else {
                *lastComponent = L'\0';
                lastComponent++;
            }

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
	    osi_Log1(afsd_logp,"RDR_ParseIoctlPath [7] code 0x%x", code);
            if (free_path)
                free(relativePath);
            return code;
	}
    }

    if (substRootp)
	cm_ReleaseSCache(substRootp);

    if (free_path)
        free(relativePath);

    /* Ensure that the status object is up to date */
    lock_ObtainWrite(&(*scpp)->rw);
    code = cm_SyncOp( *scpp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == 0)
        cm_SyncOpDone( *scpp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&(*scpp)->rw);

    /* and return success */
    osi_Log1(afsd_logp,"RDR_ParseIoctlPath [8] code 0x%x", code);
    return 0;
}

#define LEAF_SIZE 256
/* parse the passed-in file name and do a namei on its parent.  If we fail,
 * return an error code, otherwise return the vnode located in *scpp.
 */
static afs_int32
RDR_ParseIoctlParent(RDR_ioctl_t *ioctlp, cm_user_t *userp, cm_req_t *reqp,
                     cm_scache_t **scpp, wchar_t *leafp)
{
    afs_int32 code;
    clientchar_t tbuffer[1024];
    clientchar_t *tp, *jp;
    cm_scache_t *substRootp = NULL;
    clientchar_t *inpathp;
    char *inpathdatap;
    int free_path = FALSE;

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
        osi_Log1(afsd_logp, "RDR_ParseIoctlParent UTF8 inpathp %S",
                  osi_LogSaveStringW(afsd_logp, inpathp));
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
        osi_Log1(afsd_logp, "RDR_ParseIoctlParent ASCII inpathp %S",
                 osi_LogSaveStringW(afsd_logp, inpathp));
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

    if (free_path)
        free(inpathp);
    inpathp = NULL;             /* We don't need this from this point on */

    if (tbuffer[0] == tbuffer[1] &&
        tbuffer[1] == '\\' &&
        !cm_ClientStrCmpNI(RDR_UNCName,tbuffer+2, (int)wcslen(RDR_UNCName)))
    {
        wchar_t shareName[256];
        wchar_t *sharePath;
        int shareFound, i;

        /* We may have found a UNC path.
         * If the first component is the UNC Name,
         * then throw out the second component (the submount)
         * since it had better expand into the value of ioctl->tidPathp
         */
        clientchar_t * p;
        p = tbuffer + 2 + cm_ClientStrLen(RDR_UNCName) + 1;
        if ( !cm_ClientStrCmpNI(_C("all"), p, 3) )
            p += 4;

        for (i = 0; *p && *p != '\\'; i++,p++ ) {
            shareName[i] = *p;
        }
        p++;                    /* skip past trailing slash */
        shareName[i] = 0;       /* terminate string */

        shareFound = smb_FindShare(NULL, NULL, shareName, &sharePath);
        if ( shareFound ) {
            /* we found a sharename, therefore use the resulting path */
            code = cm_NameI(cm_RootSCachep(userp, reqp), sharePath,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, NULL, reqp, &substRootp);
            free(sharePath);
            if (code) {
		osi_Log1(afsd_logp,"RDR_ParseIoctlParent [1] code 0x%x", code);
                return code;
            }
            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, NULL, reqp, scpp);
	    cm_ReleaseSCache(substRootp);
            if (code) {
		osi_Log1(afsd_logp,"RDR_ParseIoctlParent [2] code 0x%x", code);
                return code;
            }
        } else {
            /* otherwise, treat the name as a cellname mounted off the afs root.
             * This requires that we reconstruct the shareName string with
             * leading and trailing slashes.
             */
            p = tbuffer + 2 + wcslen(RDR_UNCName) + 1;
            if ( !cm_ClientStrCmpNI(_C("all"), p, 3) )
                p += 4;

            shareName[0] = '/';
            for (i = 1; *p && *p != '\\'; i++,p++ ) {
                shareName[i] = *p;
            }
            p++;                    /* skip past trailing slash */
            shareName[i++] = '/';	/* add trailing slash */
            shareName[i] = 0;       /* terminate string */

            code = cm_NameI(cm_RootSCachep(userp, reqp), shareName,
                             CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                             userp, NULL, reqp, &substRootp);
            if (code) {
		osi_Log1(afsd_logp,"RDR_ParseIoctlParent [3] code 0x%x", code);
                return code;
            }
            code = cm_NameI(substRootp, p, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                            userp, NULL, reqp, scpp);
	    cm_ReleaseSCache(substRootp);
            if (code) {
		osi_Log1(afsd_logp,"RDR_ParseIoctlParent [4] code 0x%x", code);
                return code;
            }
        }
    } else {
        code = cm_GetSCache(&ioctlp->rootFid, NULL, &substRootp, userp, reqp);
        if (code) {
            osi_Log1(afsd_logp,"RDR_ParseIoctlParent [5] code 0x%x", code);
            return code;
        }
        code = cm_NameI(substRootp, tbuffer, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                        userp, NULL, reqp, scpp);
	cm_ReleaseSCache(substRootp);
        if (code) {
            osi_Log1(afsd_logp,"RDR_ParseIoctlParent [6] code 0x%x", code);
            return code;
        }
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
    osi_Log1(afsd_logp,"RDR_ParseIoctlParent [7] code 0x%x", code);
    return 0;
}

afs_int32
RDR_IoctlSetToken(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
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
    wchar_t *smbname = NULL;
    wchar_t *uname = NULL;
    afs_uint32 code = 0;
    int release_userp = 0;

    saveDataPtr = ioctlp->ioctl.inDatap;

    cm_SkipIoctlPath(&ioctlp->ioctl);

    tp = ioctlp->ioctl.inDatap;

    /* ticket length */
    memcpy(&ticketLen, tp, sizeof(ticketLen));
    tp += sizeof(ticketLen);
    if (ticketLen < MINKTCTICKETLEN || ticketLen > MAXKTCTICKETLEN) {
        code = CM_ERROR_INVAL;
        goto done;
    }

    /* remember ticket and skip over it for now */
    ticket = tp;
    tp += ticketLen;

    /* clear token size */
    memcpy(&ctSize, tp, sizeof(ctSize));
    tp += sizeof(ctSize);
    if (ctSize != sizeof(struct ClearToken)) {
        code = CM_ERROR_INVAL;
        goto done;
    }

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
        cellp = cm_GetCell(tp, CM_FLAG_CREATE | CM_FLAG_NOPROBE);
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
            tp += strlen(tp) + 1;
            osi_Log2(afsd_logp,"RDR_IoctlSetToken for user [%S] smbname [%S]",
                     osi_LogSaveStringW(afsd_logp,uname),
                     osi_LogSaveStringW(afsd_logp,smbname));
        } else {
            osi_Log1(afsd_logp,"RDR_IoctlSetToken for user [%S]",
                     osi_LogSaveStringW(afsd_logp, uname));
        }

        /* uuid */
        memcpy(&uuid, tp, sizeof(uuid));
        if (!cm_FindTokenEvent(uuid, sessionKey, NULL)) {
            code = CM_ERROR_INVAL;
            goto done;
        }

#if defined(NO_AUTH_GROUPS)
        /* after obtaining the session key check whether we can use it */
        if (!(pflags & AFSCALL_FLAG_LOCAL_SYSTEM) && (flags & PIOCTL_LOGON)) {
            code = CM_ERROR_NOACCESS;
            goto done;
        }
#endif
    } else {
        cellp = cm_data.rootCellp;
        osi_Log0(afsd_logp,"cm_IoctlSetToken - no name specified");
    }

#if defined(NO_AUTH_GROUPS)
    if ((pflags & AFSCALL_FLAG_LOCAL_SYSTEM) && (flags & PIOCTL_LOGON)) {
        PSID pSid = NULL;
        DWORD dwSize1, dwSize2;
        wchar_t *pszRefDomain = NULL;
        SID_NAME_USE snu = SidTypeGroup;
        clientchar_t * secSidString = NULL;
        DWORD gle;
        static wchar_t cname[MAX_COMPUTERNAME_LENGTH+1] = L"";

        if ( cname[0] == '\0') {
            int len = MAX_COMPUTERNAME_LENGTH+1;
            GetComputerNameW(cname, &len);
            _wcsupr(cname);
        }

        /*
         * The input name is probably not a SID for the user which is how
         * the user is now being identified as a result of the SMB
         * extended authentication.  See if we can obtain the SID for the
         * specified name.  If we can, use that instead of the name
         * provided.
         */

        dwSize1 = dwSize2 = 0;
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
            userp = smb_FindCMUserBySID( secSidString, cname,
                                         SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
            LocalFree(secSidString);
        } else {
            if (pSid) {
                LocalFree(pSid);
                pSid = NULL;
            }

            if (!ConvertStringSidToSidW( smbname, &pSid)) {
                userp = smb_FindCMUserBySID( smbname, cname,
                                             SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
            } else {
                userp = smb_FindCMUserByName( smbname, cname,
                                              SMB_FLAG_CREATE|SMB_FLAG_AFSLOGON);
            }
        }

        if (pSid)
            LocalFree(pSid);
        if (pszRefDomain)
            free(pszRefDomain);

	release_userp = 1;
    }
#endif

    /* store the token */
    lock_ObtainMutex(&userp->mx);
    ucellp = cm_GetUCell(userp, cellp);
    osi_Log1(afsd_logp,"cm_IoctlSetToken ucellp %lx", ucellp);
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
    ucellp->flags |= CM_UCELLFLAG_RXKAD;
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
RDR_IoctlGetACL(RDR_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    cm_scache_t *scp;
    afs_int32 code;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetACL(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
RDR_IoctlSetACL(RDR_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    cm_scache_t *scp;
    afs_int32 code;

    afs_uint32 flags = 0;

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    if (code)
        return code;

    code = cm_IoctlSetACL(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
RDR_IoctlGetFileCellName(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetFileCellName(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlFlushAllVolumes(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);	/* we don't care about the path */

    return cm_IoctlFlushAllVolumes(&ioctlp->ioctl, userp, &ioctlp->req);
}

afs_int32
RDR_IoctlFlushVolume(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlFlushVolume(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlFlushFile(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
	cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlFlushFile(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
RDR_IoctlSetVolumeStatus(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, 0);
    if (code) return code;

    code = cm_IoctlSetVolumeStatus(&ioctlp->ioctl, userp, scp, &ioctlp->req);
    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlGetVolumeStatus(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetVolumeStatus(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlGetFid(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t * optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    if (code)
        return code;

    code = cm_IoctlGetFid(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlGetFileType(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t * optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetFileType(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlGetOwner(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetOwner(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlWhereIs(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlWhereIs(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}


afs_int32
RDR_IoctlStatMountPoint(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &dscp, 0);
    if (code)
        return code;

    code = cm_IoctlStatMountPoint(&ioctlp->ioctl, userp, dscp, &ioctlp->req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
RDR_IoctlDeleteMountPoint(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &dscp, 0);
    if (code)
        return code;

    code = cm_IoctlDeleteMountPoint(&ioctlp->ioctl, userp, dscp, &ioctlp->req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
RDR_IoctlCheckServers(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);	/* we don't care about the path */

    return cm_IoctlCheckServers(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGag(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    /* we don't print anything superfluous, so we don't support the gag call */
    return CM_ERROR_INVAL;
}

afs_int32
RDR_IoctlCheckVolumes(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlCheckVolumes(&ioctlp->ioctl, userp);
}

afs_int32 RDR_IoctlSetCacheSize(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetCacheSize(&ioctlp->ioctl, userp);
}


afs_int32
RDR_IoctlTraceControl(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlTraceControl(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGetCacheParms(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetCacheParms(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGetCell(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetCell(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlNewCell(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlNewCell(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlNewCell2(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlNewCell2(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGetWsCell(RDR_ioctl_t *ioctlp, cm_user_t *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetWsCell(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlSysName(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSysName(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGetCellStatus(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetCellStatus(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlSetCellStatus(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetCellStatus(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlSetSPrefs(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetSPrefs(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGetSPrefs(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetSPrefs(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlStoreBehind(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    /* we ignore default asynchrony since we only have one way
     * of doing this today.
     */
    return 0;
}

afs_int32
RDR_IoctlCreateMountPoint(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    wchar_t leaf[LEAF_SIZE];

    code = RDR_ParseIoctlParent(ioctlp, userp, &ioctlp->req, &dscp, leaf);
    if (code)
        return code;

    code = cm_IoctlCreateMountPoint(&ioctlp->ioctl, userp, dscp, &ioctlp->req, leaf);

    cm_ReleaseSCache(dscp);
    return code;
}

afs_int32
RDR_IoctlSymlink(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;
    wchar_t leaf[LEAF_SIZE];

    code = RDR_ParseIoctlParent(ioctlp, userp, &ioctlp->req, &dscp, leaf);
    if (code) return code;

    code = cm_IoctlSymlink(&ioctlp->ioctl, userp, dscp, &ioctlp->req, leaf);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
RDR_IoctlListlink(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &dscp, 0);
    if (code) return code;

    code = cm_IoctlListlink(&ioctlp->ioctl, userp, dscp, &ioctlp->req);

    cm_ReleaseSCache(dscp);
    return code;
}

afs_int32
RDR_IoctlIslink(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{/*CHECK FOR VALID SYMLINK*/
    afs_int32 code;
    cm_scache_t *dscp;

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &dscp, 0);
    if (code) return code;

    code = cm_IoctlIslink(&ioctlp->ioctl, userp, dscp, &ioctlp->req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
RDR_IoctlDeletelink(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *dscp;

    code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &dscp, 0);
    if (code) return code;

    code = cm_IoctlDeletelink(&ioctlp->ioctl, userp, dscp, &ioctlp->req);

    cm_ReleaseSCache(dscp);

    return code;
}

afs_int32
RDR_IoctlGetTokenIter(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetTokenIter(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGetToken(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetToken(&ioctlp->ioctl, userp);
}


afs_int32
RDR_IoctlDelToken(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlDelToken(&ioctlp->ioctl, userp);
}


afs_int32
RDR_IoctlDelAllToken(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlDelAllToken(&ioctlp->ioctl, userp);
}


afs_int32
RDR_IoctlMakeSubmount(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlMakeSubmount(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlGetRxkcrypt(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetRxkcrypt(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlSetRxkcrypt(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetRxkcrypt(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlRxStatProcess(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlRxStatProcess(&ioctlp->ioctl, userp);
}


afs_int32
RDR_IoctlRxStatPeer(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlRxStatPeer(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlUnicodeControl(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlUnicodeControl(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlUUIDControl(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlUUIDControl(&ioctlp->ioctl, userp);
}


afs_int32
RDR_IoctlMemoryDump(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlMemoryDump(&ioctlp->ioctl, userp);
}

afs_int32
RDR_IoctlPathAvailability(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlPathAvailability(&ioctlp->ioctl, userp, scp, &ioctlp->req);
    cm_ReleaseSCache(scp);
    return code;
}

afs_int32
RDR_IoctlVolStatTest(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlVolStatTest(&ioctlp->ioctl, userp, &ioctlp->req);
}

/*
 * VIOC_SETOWNER
 *
 * This pioctl requires the use of the cm_ioctlQueryOptions_t structure.
 *
 */
afs_int32
RDR_IoctlSetOwner(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp) {
        if (CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
            flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

        if (CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
            cm_fid_t fid;
            cm_SkipIoctlPath(&ioctlp->ioctl);
            cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                       optionsp->fid.vnode, optionsp->fid.unique);
            code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
        } else {
            code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
        }
        if (code)
            return code;

        cm_IoctlSkipQueryOptions(&ioctlp->ioctl, userp);
    }

    code = cm_IoctlSetOwner(&ioctlp->ioctl, userp, scp, &ioctlp->req);

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
RDR_IoctlSetGroup(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp) {
        if (CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
            flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

        if (CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
            cm_fid_t fid;
            cm_SkipIoctlPath(&ioctlp->ioctl);
            cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                       optionsp->fid.vnode, optionsp->fid.unique);
            code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
        } else {
            code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
        }
        if (code)
            return code;

        cm_IoctlSkipQueryOptions(&ioctlp->ioctl, userp);
    }

    code = cm_IoctlSetGroup(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlGetUnixMode(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetUnixMode(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

/*
 * VIOC_SETUNIXMODE
 *
 * This pioctl requires the use of the cm_ioctlQueryOptions_t structure.
 *
 */
afs_int32
RDR_IoctlSetUnixMode(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t *optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp) {
        if (CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
            flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

        if (CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
            cm_fid_t fid;
            cm_SkipIoctlPath(&ioctlp->ioctl);
            cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                       optionsp->fid.vnode, optionsp->fid.unique);
            code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
        } else {
            code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
        }
        if (code)
            return code;

        cm_IoctlSkipQueryOptions(&ioctlp->ioctl, userp);
    }

    code = cm_IoctlSetUnixMode(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}

afs_int32
RDR_IoctlGetVerifyData(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlGetVerifyData(&ioctlp->ioctl);
}

afs_int32
RDR_IoctlSetVerifyData(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    cm_SkipIoctlPath(&ioctlp->ioctl);

    return cm_IoctlSetVerifyData(&ioctlp->ioctl);
}

afs_int32
RDR_IoctlGetCallerAccess(struct RDR_ioctl *ioctlp, struct cm_user *userp, afs_uint32 pflags)
{
    afs_int32 code;
    cm_scache_t *scp;
    cm_ioctlQueryOptions_t * optionsp;
    afs_uint32 flags = 0;

    optionsp = cm_IoctlGetQueryOptions(&ioctlp->ioctl, userp);
    if (optionsp && CM_IOCTL_QOPTS_HAVE_LITERAL(optionsp))
        flags |= (optionsp->literal ? CM_PARSE_FLAG_LITERAL : 0);

    if (optionsp && CM_IOCTL_QOPTS_HAVE_FID(optionsp)) {
        cm_fid_t fid;
        cm_SkipIoctlPath(&ioctlp->ioctl);
        cm_SetFid(&fid, optionsp->fid.cell, optionsp->fid.volume,
                  optionsp->fid.vnode, optionsp->fid.unique);
        code = cm_GetSCache(&fid, NULL, &scp, userp, &ioctlp->req);
    } else {
        code = RDR_ParseIoctlPath(ioctlp, userp, &ioctlp->req, &scp, flags);
    }
    if (code)
        return code;

    code = cm_IoctlGetCallerAccess(&ioctlp->ioctl, userp, scp, &ioctlp->req);

    cm_ReleaseSCache(scp);

    return code;
}
