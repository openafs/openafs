/*
 * Copyright (c) 2008 Secure Endpoints, Inc.
 * Copyright (c) 2009-2014 Your File System, Inc.
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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS
#define INITGUID        /* define AFS_AUTH_GUID_NO_PAG */

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>

#include <roken.h>

#include <afs/stds.h>

#include <ntsecapi.h>
#include <sddl.h>
#pragma warning(push)
#pragma warning(disable: 4005)

#include <devioctl.h>

#include "..\\Common\\AFSUserDefines.h"
#include "..\\Common\\AFSUserStructs.h"

#pragma warning(pop)

#include <tchar.h>
#include <wchar.h>
#include <winbase.h>
#include <winreg.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strsafe.h>

#include "afsd.h"
#include "smb.h"
#include "cm_btree.h"
#include "msrpc.h"
#include <RDRPrototypes.h>
#include <RDRIoctl.h>
#include <RDRPipe.h>

static CHAR * RDR_extentBaseAddress = NULL;

void
RDR_InitReq(cm_req_t *reqp, BOOL bWow64)
{
    cm_InitReq(reqp);
    reqp->flags |= CM_REQ_SOURCE_REDIR;
    if (bWow64)
        reqp->flags |= CM_REQ_WOW64;
}

void
RDR_fid2FID( cm_fid_t *fid, AFSFileID *FileId)
{
    FileId->Cell = fid->cell;
    FileId->Volume = fid->volume;
    FileId->Vnode = fid->vnode;
    FileId->Unique = fid->unique;
    FileId->Hash = fid->hash;
}

void
RDR_FID2fid( AFSFileID *FileId, cm_fid_t *fid)
{
    fid->cell = FileId->Cell;
    fid->volume = FileId->Volume;
    fid->vnode = FileId->Vnode;
    fid->unique = FileId->Unique;
    fid->hash = FileId->Hash;
}

unsigned long
RDR_ExtAttributes(cm_scache_t *scp)
{
    unsigned long attrs;

    if (scp->fileType == CM_SCACHETYPE_DIRECTORY ||
        scp->fid.vnode & 0x1)
    {
        attrs = SMB_ATTR_DIRECTORY;
#ifdef SPECIAL_FOLDERS
        attrs |= SMB_ATTR_SYSTEM;		/* FILE_ATTRIBUTE_SYSTEM */
#endif /* SPECIAL_FOLDERS */
    } else if ( scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
                scp->fileType == CM_SCACHETYPE_DFSLINK ||
                scp->fileType == CM_SCACHETYPE_INVALID)
    {
        attrs = SMB_ATTR_DIRECTORY | SMB_ATTR_REPARSE_POINT;
    } else if ( scp->fileType == CM_SCACHETYPE_SYMLINK) {
        attrs = SMB_ATTR_REPARSE_POINT;
    } else {
        attrs = 0;
    }

    if ((scp->unixModeBits & 0200) == 0)
        attrs |= SMB_ATTR_READONLY;		/* Read-only */

    if (attrs == 0)
        attrs = SMB_ATTR_NORMAL;		/* FILE_ATTRIBUTE_NORMAL */

    return attrs;
}

DWORD
RDR_SetInitParams( OUT AFSRedirectorInitInfo **ppRedirInitInfo, OUT DWORD * pRedirInitInfoLen )
{
    extern char cm_CachePath[];
    extern cm_config_data_t cm_data;
    extern int smb_hideDotFiles;
    size_t CachePathLen;
    DWORD TempPathLen;
    size_t err;
    MEMORYSTATUSEX memStatus;
    DWORD maxMemoryCacheSize;
    char FullCachePath[MAX_PATH];
    char TempPath[MAX_PATH];
    char FullTempPath[MAX_PATH];

    /*
     * The %TEMP% environment variable may be relative instead
     * of absolute which can result in the redirector referring
     * to a different directory than the service.  The full path
     * must therefore be obtained first.
     */

    CachePathLen = GetFullPathNameA(cm_CachePath, MAX_PATH, FullCachePath, NULL);
    if (CachePathLen == 0) {
        osi_Log0(afsd_logp, "RDR_SetInitParams Unable to obtain Full Cache Path");
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    TempPathLen = ExpandEnvironmentStringsA("%TEMP%", TempPath, MAX_PATH);
    if (TempPathLen == 0) {
        osi_Log0(afsd_logp, "RDR_SetInitParams Unable to expand %%TEMP%%");
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    TempPathLen = GetFullPathNameA(TempPath, MAX_PATH, FullTempPath, NULL);
    if (TempPathLen == 0) {
        osi_Log0(afsd_logp, "RDR_SetInitParams Unable to obtain Full Temp Path");
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        /*
         * Use the memory extent interface in the afs redirector
         * whenever the cache size is less than equal to 10% of
         * physical memory.  Do not use too much because this memory
         * will be locked by the redirector so it can't be swapped
         * out.
         */
        maxMemoryCacheSize = (DWORD)(memStatus.ullTotalPhys / 1024 / 10);
    } else {
        /*
         * If we can't determine the amount of physical memory
         * in the system, be conservative and limit the use of
         * memory extent interface to 64MB data caches.
         */
        maxMemoryCacheSize = 65536;
    }

    *pRedirInitInfoLen = (DWORD) (sizeof(AFSRedirectorInitInfo) + (CachePathLen + TempPathLen) * sizeof(WCHAR));
    *ppRedirInitInfo = (AFSRedirectorInitInfo *)malloc(*pRedirInitInfoLen);
    (*ppRedirInitInfo)->Flags = smb_hideDotFiles ? AFS_REDIR_INIT_FLAG_HIDE_DOT_FILES : 0;
    (*ppRedirInitInfo)->Flags |= cm_shortNames ? 0 : AFS_REDIR_INIT_FLAG_DISABLE_SHORTNAMES;
    (*ppRedirInitInfo)->Flags |= cm_directIO ? AFS_REDIR_INIT_PERFORM_SERVICE_IO : 0;
    (*ppRedirInitInfo)->MaximumChunkLength = cm_data.chunkSize;
    (*ppRedirInitInfo)->GlobalFileId.Cell   = cm_data.rootFid.cell;
    (*ppRedirInitInfo)->GlobalFileId.Volume = cm_data.rootFid.volume;
    (*ppRedirInitInfo)->GlobalFileId.Vnode  = cm_data.rootFid.vnode;
    (*ppRedirInitInfo)->GlobalFileId.Unique = cm_data.rootFid.unique;
    (*ppRedirInitInfo)->GlobalFileId.Hash   = cm_data.rootFid.hash;
    (*ppRedirInitInfo)->ExtentCount.QuadPart = cm_data.buf_nbuffers;
    (*ppRedirInitInfo)->CacheBlockSize = cm_data.blockSize;
    (*ppRedirInitInfo)->MaxPathLinkCount = MAX_FID_COUNT;
    (*ppRedirInitInfo)->NameArrayLength = MAX_FID_COUNT;
    (*ppRedirInitInfo)->GlobalReparsePointPolicy = rdr_ReparsePointPolicy;
    if (cm_virtualCache || cm_data.bufferSize <= maxMemoryCacheSize) {
        osi_Log0(afsd_logp, "RDR_SetInitParams Initializing Memory Extent Interface");
        (*ppRedirInitInfo)->MemoryCacheOffset.QuadPart = (LONGLONG)cm_data.bufDataBaseAddress;
        (*ppRedirInitInfo)->MemoryCacheLength.QuadPart = cm_data.bufEndOfData - cm_data.bufDataBaseAddress;
        (*ppRedirInitInfo)->CacheFileNameLength = 0;
        RDR_extentBaseAddress = cm_data.bufDataBaseAddress;
    } else {
        (*ppRedirInitInfo)->MemoryCacheOffset.QuadPart = 0;
        (*ppRedirInitInfo)->MemoryCacheLength.QuadPart = 0;
        (*ppRedirInitInfo)->CacheFileNameLength = (ULONG) (CachePathLen * sizeof(WCHAR));
        err = mbstowcs((*ppRedirInitInfo)->CacheFileName, FullCachePath, (CachePathLen + 1) *sizeof(WCHAR));
        if (err == -1) {
            free(*ppRedirInitInfo);
            osi_Log0(afsd_logp, "RDR_SetInitParams Invalid Object Name");
            return STATUS_OBJECT_NAME_INVALID;
        }
        RDR_extentBaseAddress = cm_data.baseAddress;
    }
    (*ppRedirInitInfo)->DumpFileLocationOffset = FIELD_OFFSET(AFSRedirectorInitInfo, CacheFileName) + (*ppRedirInitInfo)->CacheFileNameLength;
    (*ppRedirInitInfo)->DumpFileLocationLength = (TempPathLen - 1) * sizeof(WCHAR);

    err = mbstowcs((((PBYTE)(*ppRedirInitInfo)) + (*ppRedirInitInfo)->DumpFileLocationOffset),
                   FullTempPath, (TempPathLen + 1) *sizeof(WCHAR));
    if (err == -1) {
        free(*ppRedirInitInfo);
        osi_Log0(afsd_logp, "RDR_SetInitParams Invalid Object Name");
        return STATUS_OBJECT_NAME_INVALID;
    }

    osi_Log0(afsd_logp,"RDR_SetInitParams Success");
    return 0;
}

static wchar_t cname[MAX_COMPUTERNAME_LENGTH+1] = L"";

cm_user_t *
RDR_GetLocalSystemUser( void)
{
    smb_username_t *unp;
    cm_user_t *userp = NULL;

    if ( cname[0] == '\0') {
        int len = MAX_COMPUTERNAME_LENGTH+1;
        GetComputerNameW(cname, &len);
        _wcsupr(cname);
    }
    unp = smb_FindUserByName(NTSID_LOCAL_SYSTEM, cname, SMB_FLAG_CREATE);
    lock_ObtainMutex(&unp->mx);
    if (!unp->userp)
        unp->userp = cm_NewUser();
    unp->flags |= SMB_USERNAMEFLAG_SID;
    lock_ReleaseMutex(&unp->mx);
    userp = unp->userp;
    cm_HoldUser(userp);
    smb_ReleaseUsername(unp);

    if (!userp) {
        userp = cm_rootUserp;
        cm_HoldUser(userp);
    }

    return userp;
}

cm_user_t *
RDR_UserFromCommRequest( IN AFSCommRequest *RequestBuffer)
{

    return RDR_UserFromAuthGroup( &RequestBuffer->AuthGroup);
}

cm_user_t *
RDR_UserFromAuthGroup( IN GUID *pGuid)
{
    smb_username_t *unp;
    cm_user_t * userp = NULL;
    RPC_WSTR UuidString = NULL;

    if (UuidToStringW((UUID *)pGuid, &UuidString) != RPC_S_OK)
        goto done;

    if ( cname[0] == '\0') {
        int len = MAX_COMPUTERNAME_LENGTH+1;
        GetComputerNameW(cname, &len);
        _wcsupr(cname);
    }

    unp = smb_FindUserByName(UuidString, cname, SMB_FLAG_CREATE);
    lock_ObtainMutex(&unp->mx);
    if (!unp->userp) {
        unp->userp = cm_NewUser();
        memcpy(&unp->userp->authgroup, pGuid, sizeof(GUID));
    }
    unp->flags |= SMB_USERNAMEFLAG_SID;
    lock_ReleaseMutex(&unp->mx);
    userp = unp->userp;
    cm_HoldUser(userp);
    smb_ReleaseUsername(unp);

  done:
    if (!userp) {
        userp = cm_rootUserp;
        cm_HoldUser(userp);
    }

    osi_Log2(afsd_logp, "RDR_UserFromCommRequest Guid %S userp = 0x%p",
             osi_LogSaveStringW(afsd_logp, UuidString),
             userp);

    if (UuidString)
        RpcStringFreeW(&UuidString);

    return userp;
}

void
RDR_ReleaseUser( IN cm_user_t *userp )
{
    osi_Log1(afsd_logp, "RDR_ReleaseUser userp = 0x%p", userp);
    cm_ReleaseUser(userp);
}


/*
 * RDR_FlagScpInUse flags the scp with CM_SCACHEFLAG_RDR_IN_USE
 */
static void
RDR_FlagScpInUse( IN cm_scache_t *scp, IN BOOL bLocked )
{
    if (!bLocked)
        lock_ObtainWrite(&scp->rw);

    lock_AssertWrite(&scp->rw);
    scp->flags |= CM_SCACHEFLAG_RDR_IN_USE;

    if (!bLocked)
        lock_ReleaseWrite(&scp->rw);
}

/*
 * Obtain the status information for the specified object using
 * an inline bulk status rpc.  cm_BPlusDirEnumBulkStatOne() will
 * obtain current status for the directory object, the object
 * which is the focus of the inquiry and as many other objects
 * in the directory for which there are not callbacks registered
 * since we are likely to be asked for other objects in the directory.
 */
static afs_uint32
RDR_BulkStatLookup( cm_scache_t *dscp,
                    cm_scache_t *scp,
                    cm_user_t   *userp,
                    cm_req_t    *reqp)
{
    cm_direnum_t *      enump = NULL;
    afs_uint32  code = 0;
    cm_dirOp_t    dirop;

    code = cm_BeginDirOp(dscp, userp, reqp, CM_DIRLOCK_READ, CM_DIROP_FLAG_NONE, &dirop);
    if (code == 0) {
        code = cm_BPlusDirEnumerate(dscp, userp, reqp, TRUE, NULL, TRUE, &enump);
        if (code) {
            osi_Log1(afsd_logp, "RDR_BulkStatLookup cm_BPlusDirEnumerate failure code=0x%x",
                      code);
        }
        cm_EndDirOp(&dirop);
    } else {
        osi_Log1(afsd_logp, "RDR_BulkStatLookup cm_BeginDirOp failure code=0x%x",
                  code);
    }

    if (enump)
    {
        code = cm_BPlusDirEnumBulkStatOne(enump, scp);
        if (code) {
            osi_Log1(afsd_logp, "RDR_BulkStatLookup cm_BPlusDirEnumBulkStatOne failure code=0x%x",
                      code);
        }
        cm_BPlusDirFreeEnumeration(enump);
    }

    return code;
}


#define RDR_POP_FOLLOW_MOUNTPOINTS 0x01
#define RDR_POP_EVALUATE_SYMLINKS  0x02
#define RDR_POP_WOW64              0x04
#define RDR_POP_NO_GETSTATUS       0x08

static afs_uint32
RDR_PopulateCurrentEntry( IN  AFSDirEnumEntry * pCurrentEntry,
                          IN  DWORD             dwMaxEntryLength,
                          IN  cm_scache_t     * dscp,
                          IN  cm_scache_t     * scp,
                          IN  cm_user_t       * userp,
                          IN  cm_req_t        * reqp,
                          IN  wchar_t         * name,
                          IN  wchar_t         * shortName,
                          IN  DWORD             dwFlags,
                          IN  afs_uint32        cmError,
                          OUT AFSDirEnumEntry **ppNextEntry,
                          OUT DWORD           * pdwRemainingLength)
{
    FILETIME ft;
    WCHAR *  wname, *wtarget;
    size_t   len;
    DWORD      dwEntryLength;
    afs_uint32 code = 0, code2 = 0;
    BOOL          bMustFake = FALSE;

    osi_Log5(afsd_logp, "RDR_PopulateCurrentEntry dscp=0x%p scp=0x%p name=%S short=%S flags=0x%x",
             dscp, scp, osi_LogSaveStringW(afsd_logp, name),
             osi_LogSaveStringW(afsd_logp, shortName), dwFlags);
    osi_Log1(afsd_logp, "... maxLength=%d", dwMaxEntryLength);

    if (dwMaxEntryLength < sizeof(AFSDirEnumEntry) + (MAX_PATH + MOUNTPOINTLEN) * sizeof(wchar_t)) {
        if (ppNextEntry)
            *ppNextEntry = pCurrentEntry;
        if (pdwRemainingLength)
            *pdwRemainingLength = dwMaxEntryLength;
        osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry Not Enough Room for Entry %d < %d",
                 dwMaxEntryLength, sizeof(AFSDirEnumEntry) + (MAX_PATH + MOUNTPOINTLEN) * sizeof(wchar_t));
        return CM_ERROR_TOOBIG;
    }

    if (!name)
        name = L"";
    if (!shortName)
        shortName = L"";

    dwEntryLength = sizeof(AFSDirEnumEntry);

    lock_ObtainWrite(&scp->rw);
    if (dwFlags & RDR_POP_NO_GETSTATUS) {
        if (!cm_HaveCallback(scp))
            bMustFake = TRUE;
    } else {
#ifdef AFS_FREELANCE_CLIENT
        if (scp->fid.cell == AFS_FAKE_ROOT_CELL_ID && scp->fid.volume == AFS_FAKE_ROOT_VOL_ID) {
            /*
             * If the FID is from the Freelance Local Root always perform
             * a single item status check.
             */
            code = cm_SyncOp( scp, NULL, userp, reqp, 0,
                              CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
            if (code) {
                lock_ReleaseWrite(&scp->rw);
                osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_SyncOp failed for scp=0x%p code=0x%x",
                         scp, code);
                return code;
            }
        } else
#endif
        {
            /*
             * For non-Freelance objects, check to see if we have current
             * status information.  If not, perform a bulk status lookup of multiple
             * entries in order to reduce the number of RPCs issued to the file server.
             */
            if (cm_EAccesFindEntry(userp, &scp->fid))
                bMustFake = TRUE;
            else if (!cm_HaveCallback(scp)) {
                lock_ReleaseWrite(&scp->rw);
                code = RDR_BulkStatLookup(dscp, scp, userp, reqp);
                if (code) {
                    osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry RDR_BulkStatLookup failed for scp=0x%p code=0x%x",
                             scp, code);
		    if (code != CM_ERROR_NOACCESS)
			return code;
                }
                lock_ObtainWrite(&scp->rw);
                /*
                 * RDR_BulkStatLookup can succeed but it may be the case that there
                 * still is not valid status info.  If we get this far, generate fake
                 * status info.
                 */
                if (!cm_HaveCallback(scp))
                    bMustFake = TRUE;
            }
        }
    }

    /* Populate the error code */
    smb_MapNTError(cmError, &pCurrentEntry->NTStatus, TRUE);

    /* Populate the real or fake data */
    pCurrentEntry->FileId.Cell = scp->fid.cell;
    pCurrentEntry->FileId.Volume = scp->fid.volume;
    pCurrentEntry->FileId.Vnode = scp->fid.vnode;
    pCurrentEntry->FileId.Unique = scp->fid.unique;
    pCurrentEntry->FileId.Hash = scp->fid.hash;

    pCurrentEntry->FileType = scp->fileType;

    pCurrentEntry->DataVersion.QuadPart = scp->dataVersion;

    if (scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
        scp->fid.volume==AFS_FAKE_ROOT_VOL_ID) {
        cm_LargeSearchTimeFromUnixTime(&ft, MAX_AFS_UINT32);
    } else {
        cm_LargeSearchTimeFromUnixTime(&ft, scp->cbExpires);
    }
    pCurrentEntry->Expiration.LowPart = ft.dwLowDateTime;
    pCurrentEntry->Expiration.HighPart = ft.dwHighDateTime;

    if (bMustFake) {
        /* 1969-12-31 23:59:59 +00 */
        ft.dwHighDateTime = 0x19DB200;
        ft.dwLowDateTime = 0x5BB78980;
    } else
        cm_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
    pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
    pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
    pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
    pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
    pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

    pCurrentEntry->EndOfFile = scp->length;
    pCurrentEntry->AllocationSize.QuadPart =
	((scp->length.QuadPart/1024)+1)*1024;

    if (bMustFake) {
        switch (scp->fileType) {
        case CM_SCACHETYPE_DIRECTORY:
            pCurrentEntry->FileAttributes = SMB_ATTR_DIRECTORY;
            break;
        case CM_SCACHETYPE_MOUNTPOINT:
        case CM_SCACHETYPE_INVALID:
        case CM_SCACHETYPE_DFSLINK:
            pCurrentEntry->FileAttributes = SMB_ATTR_DIRECTORY | SMB_ATTR_REPARSE_POINT;
            break;
        case CM_SCACHETYPE_SYMLINK:
            if (cm_TargetPerceivedAsDirectory(scp->mountPointStringp))
                pCurrentEntry->FileAttributes = SMB_ATTR_DIRECTORY | SMB_ATTR_REPARSE_POINT;
            else
                pCurrentEntry->FileAttributes = SMB_ATTR_REPARSE_POINT;
            break;
        default:
            /* if we get here we either have a normal file
            * or we have a file for which we have never
            * received status info.  In this case, we can
            * check the even/odd value of the entry's vnode.
            * odd means it is to be treated as a directory
            * and even means it is to be treated as a file.
            */
            if (scp->fid.vnode & 0x1)
                pCurrentEntry->FileAttributes = SMB_ATTR_DIRECTORY;
            else
                pCurrentEntry->FileAttributes = SMB_ATTR_NORMAL;
        }
    } else
        pCurrentEntry->FileAttributes = RDR_ExtAttributes(scp);
    pCurrentEntry->EaSize = 0;
    pCurrentEntry->Links = scp->linkCount;

    len = wcslen(shortName);
    wcsncpy(pCurrentEntry->ShortName, shortName, len);
    pCurrentEntry->ShortNameLength = (CCHAR)(len * sizeof(WCHAR));

    pCurrentEntry->FileNameOffset = sizeof(AFSDirEnumEntry);
    len = wcslen(name);
    wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);
    wcsncpy(wname, name, len);
    pCurrentEntry->FileNameLength = (ULONG)(sizeof(WCHAR) * len);

    osi_Log3(afsd_logp, "RDR_PopulateCurrentEntry scp=0x%p fileType=%d dv=%u",
              scp, scp->fileType, (afs_uint32)scp->dataVersion);

    if (!(dwFlags & RDR_POP_NO_GETSTATUS))
        cm_SyncOpDone( scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    pCurrentEntry->TargetNameOffset = 0;
    pCurrentEntry->TargetNameLength = 0;
    if (!(dwFlags & RDR_POP_NO_GETSTATUS) && cm_HaveCallback(scp)) {
    switch (scp->fileType) {
    case CM_SCACHETYPE_MOUNTPOINT:
	{
            if ((code2 = cm_ReadMountPoint(scp, userp, reqp)) == 0) {
                cm_scache_t *targetScp = NULL;

                pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                len = strlen(scp->mountPointStringp);
                wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                cch = MultiByteToWideChar( CP_UTF8, 0, scp->mountPointStringp,
                                           len * sizeof(char),
                                           wtarget,
                                           len * sizeof(WCHAR));
#else
                mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                pCurrentEntry->TargetNameLength = (ULONG)(sizeof(WCHAR) * len);

		if (dwFlags & RDR_POP_FOLLOW_MOUNTPOINTS) {
		    code2 = cm_FollowMountPoint(scp, dscp, userp, reqp, &targetScp);
		    if (code2 == 0) {
			pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
			pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
			pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
			pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
			pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

			osi_Log4(afsd_logp, "RDR_PopulateCurrentEntry target FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
				  pCurrentEntry->TargetFileId.Cell, pCurrentEntry->TargetFileId.Volume,
				  pCurrentEntry->TargetFileId.Vnode, pCurrentEntry->TargetFileId.Unique);

			cm_ReleaseSCache(targetScp);
		    } else {
			osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_FollowMountPoint failed scp=0x%p code=0x%x",
				  scp, code2);
			if (code2 == CM_ERROR_TOO_MANY_SYMLINKS)
			    code = CM_ERROR_TOO_MANY_SYMLINKS;
		    }
                }
            } else {
                osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_ReadMountPoint failed scp=0x%p code=0x%x",
                          scp, code2);
            }
        }
        break;
    case CM_SCACHETYPE_SYMLINK:
    case CM_SCACHETYPE_DFSLINK:
        {
            pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
            wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

            if (dwFlags & RDR_POP_EVALUATE_SYMLINKS) {

                code2 = cm_HandleLink(scp, userp, reqp);
                if (code2 == 0) {
                    if (scp->mountPointStringp[0]) {
                        char * mp;
                        char * s;
                        size_t offset = 0;
			size_t wtarget_len = 0;

                        len = strlen(scp->mountPointStringp) + 1;
                        mp = strdup(scp->mountPointStringp);

                        for (s=mp; *s; s++) {
                            if (*s == '/')
                                *s = '\\';
                        }

                        if (strncmp("msdfs:", mp, 6) == 0) {
                            offset = 6;
                        }


                        if ( mp[offset + 1] == ':' && mp[offset] != '\\') {
                            /* Local drive letter.  Must return <drive>:\<path> */
                            pCurrentEntry->FileType = CM_SCACHETYPE_DFSLINK;
                            wtarget_len = len - offset;
#ifdef UNICODE
                            cch = MultiByteToWideChar( CP_UTF8, 0, &mp[offset],
                                                       wtarget_len * sizeof(char),
                                                       wtarget,
                                                       wtarget_len * sizeof(WCHAR));
#else
                            mbstowcs(wtarget, &mp[offset], wtarget_len);
#endif
                        } else if (mp[offset] == '\\') {
                            size_t nbNameLen = strlen(cm_NetbiosName);

                            if ( strnicmp(&mp[offset + 1], cm_NetbiosName, nbNameLen) == 0 &&
                                 mp[offset + nbNameLen + 1] == '\\')
                            {
                                /* an AFS symlink */
                                pCurrentEntry->FileType = CM_SCACHETYPE_SYMLINK;
                                wtarget_len = len - offset;
#ifdef UNICODE
                                cch = MultiByteToWideChar( CP_UTF8, 0, &mp[offset],
                                                           wtarget_len * sizeof(char),
                                                           wtarget,
                                                           wtarget_len * sizeof(WCHAR));
#else
                                mbstowcs(wtarget, &mp[offset], wtarget_len);
#endif
                            } else if ( mp[offset + 1] == '\\' &&
                                        strnicmp(&mp[offset + 2], cm_NetbiosName, strlen(cm_NetbiosName)) == 0 &&
                                        mp[offset + nbNameLen + 2] == '\\')
                            {
                                /* an AFS symlink */
                                pCurrentEntry->FileType = CM_SCACHETYPE_SYMLINK;
                                wtarget_len = len - offset - 1;
#ifdef UNICODE
                                cch = MultiByteToWideChar( CP_UTF8, 0, &mp[offset + 1],
                                                           wtarget_len * sizeof(char),
                                                           wtarget,
                                                           wtarget_len * sizeof(WCHAR));
#else
                                mbstowcs(wtarget, &mp[offset + 1], wtarget_len);
#endif
                            } else {
                                /*
                                 * treat as a UNC path. Needs to be \<server>\<share\<path>
                                 */
                                pCurrentEntry->FileType = CM_SCACHETYPE_DFSLINK;

                                if ( mp[offset] == '\\' && mp[offset + 1] == '\\')
                                     offset++;

                                wtarget_len = len - offset;
#ifdef UNICODE
                                cch = MultiByteToWideChar( CP_UTF8, 0, &mp[offset],
                                                           wtarget_len * sizeof(char),
                                                           wtarget,
                                                           wtarget_len * sizeof(WCHAR));
#else
                                mbstowcs(wtarget, &mp[offset], wtarget_len);
#endif
                            }
                        } else {
                            /* Relative AFS Symlink */
                            pCurrentEntry->FileType = CM_SCACHETYPE_SYMLINK;
                            wtarget_len = len - offset;
#ifdef UNICODE
                            cch = MultiByteToWideChar( CP_UTF8, 0, &mp[offset],
                                                       wtarget_len * sizeof(char),
                                                       wtarget,
                                                       wtarget_len * sizeof(WCHAR));
#else
                            mbstowcs(wtarget, &mp[offset], wtarget_len);
#endif
                        }

                        free(mp);

			pCurrentEntry->TargetNameLength = (ULONG)(sizeof(WCHAR) * (wtarget_len - 1));
		    }
                } else {
                    osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_HandleLink failed scp=0x%p code=0x%x",
                             scp, code2);
                }
            }

        }
        break;

    default:
        pCurrentEntry->TargetNameOffset = 0;
        pCurrentEntry->TargetNameLength = 0;
    }
    }
    lock_ReleaseWrite(&scp->rw);

    dwEntryLength += pCurrentEntry->FileNameLength + pCurrentEntry->TargetNameLength;
    dwEntryLength += (dwEntryLength % 8) ? 8 - (dwEntryLength % 8) : 0;   /* quad align */
    if (ppNextEntry)
        *ppNextEntry = (AFSDirEnumEntry *)((PBYTE)pCurrentEntry + dwEntryLength);
    if (pdwRemainingLength)
        *pdwRemainingLength = dwMaxEntryLength - dwEntryLength;

    osi_Log3(afsd_logp, "RDR_PopulateCurrentEntry Success FileNameLength=%d TargetNameLength=%d RemainingLength=%d",
              pCurrentEntry->FileNameLength, pCurrentEntry->TargetNameLength, *pdwRemainingLength);

    return code;
}

static afs_uint32
RDR_PopulateCurrentEntryNoScp( IN  AFSDirEnumEntry * pCurrentEntry,
                               IN  DWORD             dwMaxEntryLength,
                               IN  cm_scache_t     * dscp,
                               IN  cm_fid_t        * fidp,
                               IN  cm_user_t       * userp,
                               IN  cm_req_t        * reqp,
                               IN  wchar_t         * name,
                               IN  wchar_t         * shortName,
                               IN  DWORD             dwFlags,
                               IN  afs_uint32        cmError,
                               OUT AFSDirEnumEntry **ppNextEntry,
                               OUT DWORD           * pdwRemainingLength)
{
    FILETIME ft;
    WCHAR *  wname;
    size_t   len;
    DWORD      dwEntryLength;
    afs_uint32 code = 0, code2 = 0;

    osi_Log4(afsd_logp, "RDR_PopulateCurrentEntryNoEntry dscp=0x%p name=%S short=%S flags=0x%x",
             dscp, osi_LogSaveStringW(afsd_logp, name),
             osi_LogSaveStringW(afsd_logp, shortName), dwFlags);
    osi_Log1(afsd_logp, "... maxLength=%d", dwMaxEntryLength);

    if (dwMaxEntryLength < sizeof(AFSDirEnumEntry) + (MAX_PATH + MOUNTPOINTLEN) * sizeof(wchar_t)) {
        if (ppNextEntry)
            *ppNextEntry = pCurrentEntry;
        if (pdwRemainingLength)
            *pdwRemainingLength = dwMaxEntryLength;
        osi_Log2(afsd_logp, "RDR_PopulateCurrentEntryNoEntry Not Enough Room for Entry %d < %d",
                 dwMaxEntryLength, sizeof(AFSDirEnumEntry) + (MAX_PATH + MOUNTPOINTLEN) * sizeof(wchar_t));
        return CM_ERROR_TOOBIG;
    }

    if (!name)
        name = L"";
    if (!shortName)
        shortName = L"";

    dwEntryLength = sizeof(AFSDirEnumEntry);

    /* Populate the error code */
    smb_MapNTError(cmError, &pCurrentEntry->NTStatus, TRUE);

    /* Populate the fake data */
    pCurrentEntry->FileId.Cell = fidp->cell;
    pCurrentEntry->FileId.Volume = fidp->volume;
    pCurrentEntry->FileId.Vnode = fidp->vnode;
    pCurrentEntry->FileId.Unique = fidp->unique;
    pCurrentEntry->FileId.Hash = fidp->hash;

    pCurrentEntry->DataVersion.QuadPart = CM_SCACHE_VERSION_BAD;

    cm_LargeSearchTimeFromUnixTime(&ft, 0);
    pCurrentEntry->Expiration.LowPart = ft.dwLowDateTime;
    pCurrentEntry->Expiration.HighPart = ft.dwHighDateTime;

    cm_LargeSearchTimeFromUnixTime(&ft, 0);
    pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
    pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
    pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
    pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
    pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

    pCurrentEntry->EndOfFile.QuadPart = 0;
    pCurrentEntry->AllocationSize.QuadPart = 0;
    if (fidp->vnode & 0x1) {
        pCurrentEntry->FileType = CM_SCACHETYPE_DIRECTORY;
        pCurrentEntry->FileAttributes = SMB_ATTR_DIRECTORY;
    } else {
        pCurrentEntry->FileType = CM_SCACHETYPE_UNKNOWN;
        pCurrentEntry->FileAttributes = SMB_ATTR_NORMAL;
    pCurrentEntry->EaSize = 0;
    }
    pCurrentEntry->Links = 0;

    len = wcslen(shortName);
    wcsncpy(pCurrentEntry->ShortName, shortName, len);
    pCurrentEntry->ShortNameLength = (CCHAR)(len * sizeof(WCHAR));

    pCurrentEntry->FileNameOffset = sizeof(AFSDirEnumEntry);
    len = wcslen(name);
    wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);
    wcsncpy(wname, name, len);
    pCurrentEntry->FileNameLength = (ULONG)(sizeof(WCHAR) * len);

    pCurrentEntry->TargetNameOffset = 0;
    pCurrentEntry->TargetNameLength = 0;

    dwEntryLength += pCurrentEntry->FileNameLength + pCurrentEntry->TargetNameLength;
    dwEntryLength += (dwEntryLength % 8) ? 8 - (dwEntryLength % 8) : 0;   /* quad align */
    if (ppNextEntry)
        *ppNextEntry = (AFSDirEnumEntry *)((PBYTE)pCurrentEntry + dwEntryLength);
    if (pdwRemainingLength)
        *pdwRemainingLength = dwMaxEntryLength - dwEntryLength;

    osi_Log3(afsd_logp, "RDR_PopulateCurrentEntryNoScp Success FileNameLength=%d TargetNameLength=%d RemainingLength=%d",
              pCurrentEntry->FileNameLength, pCurrentEntry->TargetNameLength, *pdwRemainingLength);

    return code;
}

void
RDR_EnumerateDirectory( IN cm_user_t *userp,
                        IN AFSFileID DirID,
                        IN AFSDirQueryCB *QueryCB,
                        IN BOOL bWow64,
                        IN BOOL bSkipStatus,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    DWORD status;
    cm_direnum_t *      enump = NULL;
    AFSDirEnumResp  * pDirEnumResp;
    AFSDirEnumEntry * pCurrentEntry;
    size_t size = ResultBufferLength ? sizeof(AFSCommResult) + ResultBufferLength - 1 : sizeof(AFSCommResult);
    DWORD             dwMaxEntryLength;
    afs_uint32  code = 0;
    cm_fid_t      fid;
    cm_scache_t * dscp = NULL;
    cm_req_t      req;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_EnumerateDirectory FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
             DirID.Cell, DirID.Volume, DirID.Vnode, DirID.Unique);

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_EnumerateDirectory Out of Memory");
	return;
    }

    memset(*ResultCB, 0, size);

    if (QueryCB->EnumHandle == (ULONG_PTR)-1) {
        osi_Log0(afsd_logp, "RDR_EnumerateDirectory No More Entries");
        (*ResultCB)->ResultStatus = STATUS_NO_MORE_ENTRIES;
        (*ResultCB)->ResultBufferLength = 0;
        return;
    }

    (*ResultCB)->ResultBufferLength = dwMaxEntryLength = ResultBufferLength;
    if (ResultBufferLength) {
        pDirEnumResp = (AFSDirEnumResp *)&(*ResultCB)->ResultData;
        pCurrentEntry = (AFSDirEnumEntry *)&pDirEnumResp->Entry;
        dwMaxEntryLength -= FIELD_OFFSET( AFSDirEnumResp, Entry);      /* AFSDirEnumResp */
    }

    if (DirID.Cell != 0) {
        fid.cell   = DirID.Cell;
        fid.volume = DirID.Volume;
        fid.vnode  = DirID.Vnode;
        fid.unique = DirID.Unique;
        fid.hash   = DirID.Hash;

        code = cm_GetSCache(&fid, NULL, &dscp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EnumerateDirectory cm_GetSCache failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        osi_Log0(afsd_logp, "RDR_EnumerateDirectory Object Name Invalid - Cell = 0");
        return;
    }

    /* get the directory size */
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, PRSFS_LOOKUP,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        osi_Log2(afsd_logp, "RDR_EnumerateDirectory cm_SyncOp failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "RDR_EnumerateDirectory Not a Directory dscp=0x%p",
                 dscp);
        return;
    }

    osi_Log1(afsd_logp, "RDR_EnumerateDirectory dv=%u", (afs_uint32)dscp->dataVersion);

    /*
     * If there is no enumeration handle, then this is a new query
     * and we must perform an enumeration for the specified object.
     */
    if (QueryCB->EnumHandle == (ULONG_PTR)NULL) {
        cm_dirOp_t    dirop;

        code = cm_BeginDirOp(dscp, userp, &req, CM_DIRLOCK_READ, CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
            code = cm_BPlusDirEnumerate(dscp, userp, &req,
                                        TRUE /* dir locked */, NULL /* no mask */,
                                        TRUE /* fetch status? */, &enump);
            if (code) {
                osi_Log1(afsd_logp, "RDR_EnumerateDirectory cm_BPlusDirEnumerate failure code=0x%x",
                          code);
            }
            cm_EndDirOp(&dirop);
        } else {
            osi_Log1(afsd_logp, "RDR_EnumerateDirectory cm_BeginDirOp failure code=0x%x",
                      code);
        }
    } else {
        enump = (cm_direnum_t *)QueryCB->EnumHandle;
    }

    if (enump) {
        if (ResultBufferLength == 0) {
            code = cm_BPlusDirEnumBulkStat(enump);
            if (code) {
                osi_Log1(afsd_logp, "RDR_EnumerateDirectory cm_BPlusDirEnumBulkStat failure code=0x%x",
                          code);
            }
        } else {
            cm_direnum_entry_t * entryp = NULL;

            pDirEnumResp->SnapshotDataVersion.QuadPart = enump->dataVersion;

          getnextentry:
            if (dwMaxEntryLength < sizeof(AFSDirEnumEntry) + (MAX_PATH + MOUNTPOINTLEN) * sizeof(wchar_t)) {
                osi_Log0(afsd_logp, "RDR_EnumerateDirectory out of space, returning");
                goto outofspace;
            }

            code = cm_BPlusDirNextEnumEntry(enump, &entryp);

            if ((code == 0 || code == CM_ERROR_STOPNOW) && entryp) {
                cm_scache_t *scp = NULL;
                int stopnow = (code == CM_ERROR_STOPNOW);

                if ( !wcscmp(L".", entryp->name) || !wcscmp(L"..", entryp->name) ) {
                    osi_Log0(afsd_logp, "RDR_EnumerateDirectory skipping . or ..");
                    if (stopnow)
                        goto outofspace;
                    goto getnextentry;
                }

                if (bSkipStatus) {
                    code = cm_GetSCache(&entryp->fid, &dscp->fid, &scp, userp, &req);
                    if (code) {
                        osi_Log5(afsd_logp, "RDR_EnumerateDirectory cm_GetSCache failure cell %u vol %u vnode %u uniq %u code=0x%x",
                                 entryp->fid.cell, entryp->fid.volume, entryp->fid.vnode, entryp->fid.unique, code);
                    }
                } else {
                    code = entryp->errorCode;
                    scp = code ? NULL : cm_FindSCache(&entryp->fid);
                }

                if (scp) {
                    code = RDR_PopulateCurrentEntry( pCurrentEntry, dwMaxEntryLength,
                                                     dscp, scp, userp, &req,
                                                     entryp->name,
                                                     cm_shortNames && cm_Is8Dot3(entryp->name) ? NULL : entryp->shortName,
                                                     (bWow64 ? RDR_POP_WOW64 : 0) |
						     (bSkipStatus ? RDR_POP_NO_GETSTATUS : 0) |
						     RDR_POP_EVALUATE_SYMLINKS,
                                                     code,
                                                     &pCurrentEntry, &dwMaxEntryLength);
                    cm_ReleaseSCache(scp);
                } else {
                    code = RDR_PopulateCurrentEntryNoScp( pCurrentEntry, dwMaxEntryLength,
                                                          dscp, &entryp->fid, userp, &req,
                                                          entryp->name,
                                                          cm_shortNames && cm_Is8Dot3(entryp->name) ? NULL : entryp->shortName,
                                                          (bWow64 ? RDR_POP_WOW64 : 0),
                                                          code,
                                                          &pCurrentEntry, &dwMaxEntryLength);
                }
                if (stopnow)
                    goto outofspace;
                goto getnextentry;
            }
        }
    }

  outofspace:

    if (code || enump->next == enump->count || ResultBufferLength == 0) {
        cm_BPlusDirFreeEnumeration(enump);
        enump = (cm_direnum_t *)(ULONG_PTR)-1;
    }

    if (code == 0 || code == CM_ERROR_STOPNOW) {
        (*ResultCB)->ResultStatus = STATUS_SUCCESS;
        osi_Log0(afsd_logp, "RDR_EnumerateDirectory SUCCESS");
    } else {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_EnumerateDirectory Failure code=0x%x status=0x%x",
                  code, status);
    }

    if (ResultBufferLength) {
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwMaxEntryLength;

        pDirEnumResp->EnumHandle = (ULONG_PTR) enump;
        pDirEnumResp->CurrentDataVersion.QuadPart = dscp->dataVersion;
    }

    if (dscp)
        cm_ReleaseSCache(dscp);

    return;
}

void
RDR_EvaluateNodeByName( IN cm_user_t *userp,
                        IN AFSFileID ParentID,
                        IN WCHAR   *FileNameCounted,
                        IN DWORD    FileNameLength,
                        IN BOOL     CaseSensitive,
                        IN BOOL     LastComponent,
                        IN BOOL     bWow64,
                        IN BOOL     bNoFollow,
                        IN BOOL     bHoldFid,
                        IN DWORD    ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    AFSFileEvalResultCB *pEvalResultCB = NULL;
    AFSDirEnumEntry * pCurrentEntry;
    size_t size = ResultBufferLength ? sizeof(AFSCommResult) + ResultBufferLength - 1 : sizeof(AFSCommResult);
    afs_uint32  code = 0;
    cm_scache_t * scp = NULL;
    cm_scache_t * dscp = NULL;
    cm_req_t      req;
    cm_fid_t      parentFid;
    DWORD         status;
    DWORD         dwRemaining;
    WCHAR       * wszName = NULL;
    size_t        cbName;
    BOOL          bVol = FALSE;
    wchar_t       FileName[260];
    afs_uint32    lookupFlags;

    StringCchCopyNW(FileName, 260, FileNameCounted, FileNameLength / sizeof(WCHAR));

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_EvaluateNodeByName parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
             ParentID.Cell, ParentID.Volume, ParentID.Vnode, ParentID.Unique);

    /* Allocate enough room to add a volume prefix if necessary */
    cbName = FileNameLength + (CM_PREFIX_VOL_CCH + 64) * sizeof(WCHAR);
    wszName = malloc(cbName);
    if (!wszName) {
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByName Out of Memory");
	return;
    }
    StringCbCopyNW(wszName, cbName, FileName, FileNameLength);
    osi_Log1(afsd_logp, "... name=%S", osi_LogSaveStringW(afsd_logp, wszName));

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByName Out of Memory");
        free(wszName);
        return;
    }

    memset(*ResultCB, 0, size);
    (*ResultCB)->ResultBufferLength = 0;
    dwRemaining = ResultBufferLength;
    if (ResultBufferLength >= sizeof( AFSFileEvalResultCB)) {
        pEvalResultCB = (AFSFileEvalResultCB *)&(*ResultCB)->ResultData;
        pCurrentEntry = &pEvalResultCB->DirEnum;
        dwRemaining -= (sizeof( AFSFileEvalResultCB) - sizeof( AFSDirEnumEntry));
    }

    if (ParentID.Cell != 0) {
        parentFid.cell   = ParentID.Cell;
        parentFid.volume = ParentID.Volume;
        parentFid.vnode  = ParentID.Vnode;
        parentFid.unique = ParentID.Unique;
        parentFid.hash   = ParentID.Hash;

        code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            if ( status == STATUS_INVALID_HANDLE)
                status = STATUS_OBJECT_PATH_INVALID;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByName cm_GetSCache parentFID failure code=0x%x status=0x%x",
                      code, status);
            free(wszName);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByName Object Name Invalid - Cell = 0");
        return;
    }

    /* get the directory size */
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        osi_Log3(afsd_logp, "RDR_EvaluateNodeByName cm_SyncOp failure dscp=0x%p code=0x%x status=0x%x",
                 dscp, code, status);
        free(wszName);
        return;
    }
    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "RDR_EvaluateNodeByName Not a Directory dscp=0x%p",
                 dscp);
        free(wszName);
        return;
    }

    lookupFlags = CM_FLAG_NOMOUNTCHASE;

    if ( !LastComponent )
        lookupFlags |= CM_FLAG_CHECKPATH;
    code = cm_Lookup(dscp, wszName, lookupFlags, userp, &req, &scp);

    if (!CaseSensitive &&
        (code == CM_ERROR_NOSUCHPATH || code == CM_ERROR_NOSUCHFILE || code == CM_ERROR_BPLUS_NOMATCH)) {
        lookupFlags |= CM_FLAG_CASEFOLD;
        code = cm_Lookup(dscp, wszName, lookupFlags, userp, &req, &scp);
    }

    if ((code == CM_ERROR_NOSUCHPATH || code == CM_ERROR_NOSUCHFILE || code == CM_ERROR_BPLUS_NOMATCH) &&
         dscp == cm_data.rootSCachep) {

        if (wcschr(wszName, '%') != NULL || wcschr(wszName, '#') != NULL) {
            /*
             * A volume reference:  <cell>{%,#}<volume> -> @vol:<cell>{%,#}<volume>
             */
            StringCchCopyNW(wszName, cbName, _C(CM_PREFIX_VOL), CM_PREFIX_VOL_CCH);
            StringCbCatNW(wszName, cbName, FileName, FileNameLength);
            bVol = TRUE;

            code = cm_EvaluateVolumeReference(wszName, CM_FLAG_CHECKPATH, userp, &req, &scp);
        }
#ifdef AFS_FREELANCE_CLIENT
        else if (dscp->fid.cell == AFS_FAKE_ROOT_CELL_ID && dscp->fid.volume == AFS_FAKE_ROOT_VOL_ID &&
                 dscp->fid.vnode == 1 && dscp->fid.unique == 1) {
            /*
             * If this is the Freelance volume root directory then treat unrecognized
             * names as cell names and attempt to find the appropriate "root.cell".
             */
            StringCchCopyNW(wszName, cbName, _C(CM_PREFIX_VOL), CM_PREFIX_VOL_CCH);
            if (FileName[0] == L'.') {
                StringCbCatNW(wszName, cbName, &FileName[1], FileNameLength);
                StringCbCatNW(wszName, cbName, L"%", sizeof(WCHAR));
            } else {
                StringCbCatNW(wszName, cbName, FileName, FileNameLength);
                StringCbCatNW(wszName, cbName, L"#", sizeof(WCHAR));
            }
            StringCbCatNW(wszName, cbName, L"root.cell", 9 * sizeof(WCHAR));
            bVol = TRUE;

            code = cm_EvaluateVolumeReference(wszName, CM_FLAG_CHECKPATH, userp, &req, &scp);
        }
#endif
    }

    if (code == 0 && scp) {
        wchar_t shortName[13]=L"";

        if (!cm_shortNames) {
            shortName[0] = L'\0';
        } else if (bVol) {
            cm_Gen8Dot3VolNameW(scp->fid.cell, scp->fid.volume, shortName, NULL);
        } else if (!cm_Is8Dot3(wszName)) {
            cm_dirFid_t dfid;

            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            cm_Gen8Dot3NameIntW(FileName, &dfid, shortName, NULL);
        } else {
            shortName[0] = L'\0';
        }

        code = RDR_PopulateCurrentEntry(pCurrentEntry, dwRemaining,
                                        dscp, scp, userp, &req,
                                        FileName, shortName,
                                        (bWow64 ? RDR_POP_WOW64 : 0) |
					(bNoFollow ? 0 : RDR_POP_FOLLOW_MOUNTPOINTS) |
					RDR_POP_EVALUATE_SYMLINKS,
                                        0, NULL, &dwRemaining);
        if (bHoldFid)
            RDR_FlagScpInUse( scp, FALSE );
        cm_ReleaseSCache(scp);

        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByName FAILURE code=0x%x status=0x%x",
                      code, status);
        } else {
            pEvalResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;
            (*ResultCB)->ResultStatus = STATUS_SUCCESS;
            (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
            osi_Log0(afsd_logp, "RDR_EvaluateNodeByName SUCCESS");
        }
    } else if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_EvaluateNodeByName FAILURE code=0x%x status=0x%x",
                 code, status);
    } else {
        (*ResultCB)->ResultStatus = STATUS_NO_SUCH_FILE;
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByName No Such File");
    }
    cm_ReleaseSCache(dscp);
    free(wszName);

    return;
}

void
RDR_EvaluateNodeByID( IN cm_user_t *userp,
                      IN AFSFileID ParentID,            /* not used */
                      IN AFSFileID SourceID,
                      IN BOOL      bWow64,
                      IN BOOL      bNoFollow,
                      IN BOOL      bHoldFid,
                      IN DWORD     ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB)
{
    AFSFileEvalResultCB *pEvalResultCB = NULL;
    AFSDirEnumEntry * pCurrentEntry = NULL;
    size_t size = ResultBufferLength ? sizeof(AFSCommResult) + ResultBufferLength - 1 : sizeof(AFSCommResult);
    afs_uint32  code = 0;
    cm_scache_t * scp = NULL;
    cm_scache_t * dscp = NULL;
    cm_req_t      req;
    cm_fid_t      Fid;
    cm_fid_t      parentFid;
    DWORD         status;
    DWORD         dwRemaining;

    osi_Log4(afsd_logp, "RDR_EvaluateNodeByID source FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              SourceID.Cell, SourceID.Volume, SourceID.Vnode, SourceID.Unique);
    osi_Log4(afsd_logp, "... parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              ParentID.Cell, ParentID.Volume, ParentID.Vnode, ParentID.Unique);

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByID Out of Memory");
	return;
    }

    memset(*ResultCB, 0, size);
    (*ResultCB)->ResultBufferLength = 0;
    dwRemaining = ResultBufferLength;
    if (ResultBufferLength >= sizeof( AFSFileEvalResultCB)) {
        pEvalResultCB = (AFSFileEvalResultCB *)&(*ResultCB)->ResultData;
        pCurrentEntry = &pEvalResultCB->DirEnum;
        dwRemaining -= (sizeof( AFSFileEvalResultCB) - sizeof( AFSDirEnumEntry));
    }

    RDR_InitReq(&req, bWow64);

    if (SourceID.Cell != 0) {
        cm_SetFid(&Fid, SourceID.Cell, SourceID.Volume, SourceID.Vnode, SourceID.Unique);
        code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByID cm_GetSCache SourceFID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByID Object Name Invalid - Cell = 0");
        return;
    }

    if (ParentID.Cell != 0) {
        cm_SetFid(&parentFid, ParentID.Cell, ParentID.Volume, ParentID.Vnode, ParentID.Unique);
        code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
        if (code) {
            cm_ReleaseSCache(scp);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            if ( status == STATUS_INVALID_HANDLE)
                status = STATUS_OBJECT_PATH_INVALID;
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByID cm_GetSCache parentFID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else if (SourceID.Vnode == 1) {
        dscp = scp;
        cm_HoldSCache(dscp);
    } else if (scp->parentVnode) {
        cm_SetFid(&parentFid, SourceID.Cell, SourceID.Volume, scp->parentVnode, scp->parentUnique);
        code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
        if (code) {
            cm_ReleaseSCache(scp);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            if ( status == STATUS_INVALID_HANDLE)
                status = STATUS_OBJECT_PATH_INVALID;
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByID cm_GetSCache parentFID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByID Object Path Invalid - Unknown Parent");
        return;
    }

    /* Make sure the directory is current */
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_EvaluateNodeByID cm_SyncOp failure dscp=0x%p code=0x%x status=0x%x",
                 dscp, code, status);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        osi_Log1(afsd_logp, "RDR_EvaluateNodeByID Not a Directory dscp=0x%p", dscp);
        return;
    }

    code = RDR_PopulateCurrentEntry(pCurrentEntry, dwRemaining,
                                    dscp, scp, userp, &req, NULL, NULL,
                                    (bWow64 ? RDR_POP_WOW64 : 0) |
				    (bNoFollow ? 0 : RDR_POP_FOLLOW_MOUNTPOINTS) |
				    RDR_POP_EVALUATE_SYMLINKS,
                                    0, NULL, &dwRemaining);

    if (bHoldFid)
        RDR_FlagScpInUse( scp, FALSE );
    cm_ReleaseSCache(scp);
    cm_ReleaseSCache(dscp);

    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_EvaluateNodeByID FAILURE code=0x%x status=0x%x",
                 code, status);
    } else {
        pEvalResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;

        (*ResultCB)->ResultStatus = STATUS_SUCCESS;
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByID SUCCESS");
    }
    return;
}

void
RDR_CreateFileEntry( IN cm_user_t *userp,
                     IN WCHAR *FileNameCounted,
                     IN DWORD FileNameLength,
                     IN AFSFileCreateCB *CreateCB,
                     IN BOOL bWow64,
                     IN BOOL bHoldFid,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{
    AFSFileCreateResultCB *pResultCB = NULL;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    cm_fid_t            parentFid;
    afs_uint32          code;
    cm_scache_t *       dscp = NULL;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_scache_t *       scp = NULL;
    cm_req_t            req;
    DWORD               status;
    wchar_t             FileName[260];

    StringCchCopyNW(FileName, 260, FileNameCounted, FileNameLength / sizeof(WCHAR));

    osi_Log4(afsd_logp, "RDR_CreateFileEntry parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              CreateCB->ParentId.Cell, CreateCB->ParentId.Volume,
              CreateCB->ParentId.Vnode, CreateCB->ParentId.Unique);
    osi_Log1(afsd_logp, "... name=%S", osi_LogSaveStringW(afsd_logp, FileName));

    RDR_InitReq(&req, bWow64);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_CreateFileEntry out of memory");
	return;
    }

    memset( *ResultCB,
            '\0',
            size);

    parentFid.cell   = CreateCB->ParentId.Cell;
    parentFid.volume = CreateCB->ParentId.Volume;
    parentFid.vnode  = CreateCB->ParentId.Vnode;
    parentFid.unique = CreateCB->ParentId.Unique;
    parentFid.hash   = CreateCB->ParentId.Hash;

    code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        if ( status == STATUS_INVALID_HANDLE)
            status = STATUS_OBJECT_PATH_INVALID;
        osi_Log2(afsd_logp, "RDR_CreateFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        osi_Log3(afsd_logp, "RDR_CreateFileEntry cm_SyncOp failure (1) dscp=0x%p code=0x%x status=0x%x",
                 dscp, code, status);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "RDR_CreateFileEntry Not a Directory dscp=0x%p",
                 dscp);
        return;
    }

    /* Use current time */
    setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
    setAttr.clientModTime = time(NULL);

    if (CreateCB->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (smb_unixModeDefaultDir) {
            setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
            setAttr.unixModeBits = smb_unixModeDefaultDir;
            if (CreateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)
                setAttr.unixModeBits &= ~0222;          /* disable the write bits */
        }

        code = cm_MakeDir(dscp, FileName, flags, &setAttr, userp, &req, &scp);
    } else {
        if (smb_unixModeDefaultFile) {
            setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
            setAttr.unixModeBits = smb_unixModeDefaultFile;
            if (CreateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)
                setAttr.unixModeBits &= ~0222;          /* disable the write bits */
        }

        setAttr.mask |= CM_ATTRMASK_LENGTH;
        setAttr.length.LowPart = CreateCB->AllocationSize.LowPart;
        setAttr.length.HighPart = CreateCB->AllocationSize.HighPart;
        code = cm_Create(dscp, FileName, flags, &setAttr, &scp, userp, &req);
    }
    if (code == 0) {
        wchar_t shortName[13]=L"";
        cm_dirFid_t dfid;
        DWORD dwRemaining;

        (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

        (*ResultCB)->ResultBufferLength = sizeof( AFSFileCreateResultCB);

        pResultCB = (AFSFileCreateResultCB *)(*ResultCB)->ResultData;

        dwRemaining = ResultBufferLength - sizeof( AFSFileCreateResultCB) + sizeof( AFSDirEnumEntry);

        lock_ObtainWrite(&dscp->rw);
        code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&dscp->rw);
            cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            osi_Log3(afsd_logp, "RDR_CreateFileEntry cm_SyncOp failure (2) dscp=0x%p code=0x%x status=0x%x",
                      dscp, code, status);
            return;
        }

        pResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;

        cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&dscp->rw);

        if (cm_shortNames) {
            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            if (!cm_Is8Dot3(FileName))
                cm_Gen8Dot3NameIntW(FileName, &dfid, shortName, NULL);
            else
                shortName[0] = '\0';
        }

        code = RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining,
                                        dscp, scp, userp, &req, FileName, shortName,
                                        RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                        0, NULL, &dwRemaining);

        if (bHoldFid)
            RDR_FlagScpInUse( scp, FALSE );
        cm_ReleaseSCache(scp);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        osi_Log0(afsd_logp, "RDR_CreateFileEntry SUCCESS");
    } else {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_CreateFileEntry FAILURE code=0x%x status=0x%x",
                  code, status);
    }

    cm_ReleaseSCache(dscp);

    return;
}

void
RDR_UpdateFileEntry( IN cm_user_t *userp,
                     IN AFSFileID FileId,
                     IN AFSFileUpdateCB *UpdateCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{
    AFSFileUpdateResultCB *pResultCB = NULL;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    cm_fid_t            Fid;
    cm_fid_t            parentFid;
    afs_uint32          code;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_scache_t *       scp = NULL;
    cm_scache_t *       dscp = NULL;
    cm_req_t            req;
    time_t              clientModTime;
    FILETIME            ft;
    DWORD               status;
    BOOL                bScpLocked = FALSE;

    RDR_InitReq(&req, bWow64);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    osi_Log4(afsd_logp, "RDR_UpdateFileEntry parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              UpdateCB->ParentId.Cell, UpdateCB->ParentId.Volume,
              UpdateCB->ParentId.Vnode, UpdateCB->ParentId.Unique);
    osi_Log4(afsd_logp, "... object FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);

    *ResultCB = (AFSCommResult *)malloc( size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_UpdateFileEntry Out of Memory");
	return;
    }

    memset( *ResultCB,
            '\0',
            size);

    parentFid.cell   = UpdateCB->ParentId.Cell;
    parentFid.volume = UpdateCB->ParentId.Volume;
    parentFid.vnode  = UpdateCB->ParentId.Vnode;
    parentFid.unique = UpdateCB->ParentId.Unique;
    parentFid.hash   = UpdateCB->ParentId.Hash;

    code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        if ( status == STATUS_INVALID_HANDLE)
            status = STATUS_OBJECT_PATH_INVALID;
        osi_Log2(afsd_logp, "RDR_UpdateFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        osi_Log3(afsd_logp, "RDR_UpdateFileEntry cm_SyncOp failure dscp=0x%p code=0x%x status=0x%x",
                 dscp, code, status);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "RDR_UpdateFileEntry Not a Directory dscp=0x%p",
                 dscp);
        return;
    }

    Fid.cell   = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode  = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash   = FileId.Hash;

    code = cm_GetSCache(&Fid, &dscp->fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        cm_ReleaseSCache(dscp);
        osi_Log2(afsd_logp, "RDR_UpdateFileEntry cm_GetSCache object FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    bScpLocked = TRUE;
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_UpdateFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    if (UpdateCB->ChangeTime.QuadPart) {

        if (scp->fileType == CM_SCACHETYPE_FILE) {
            /* Do not set length and other attributes at the same time */
            if (scp->length.QuadPart != UpdateCB->AllocationSize.QuadPart) {
                osi_Log2(afsd_logp, "RDR_UpdateFileEntry Length Change 0x%x -> 0x%x",
                          (afs_uint32)scp->length.QuadPart, (afs_uint32)UpdateCB->AllocationSize.QuadPart);
                setAttr.mask |= CM_ATTRMASK_LENGTH;
                setAttr.length.LowPart = UpdateCB->AllocationSize.LowPart;
                setAttr.length.HighPart = UpdateCB->AllocationSize.HighPart;
                lock_ReleaseWrite(&scp->rw);
                bScpLocked = FALSE;
                code = cm_SetAttr(scp, &setAttr, userp, &req);
                if (code)
                    goto on_error;
                setAttr.mask = 0;
            }
        }

        if (!bScpLocked) {
            lock_ObtainWrite(&scp->rw);
            bScpLocked = TRUE;
        }
        if ((scp->unixModeBits & 0200) && (UpdateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
            setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
            setAttr.unixModeBits = scp->unixModeBits & ~0222;
        } else if (!(scp->unixModeBits & 0200) && !(UpdateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
            setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
            setAttr.unixModeBits = scp->unixModeBits | 0222;
        }
    }

    if (UpdateCB->LastWriteTime.QuadPart) {
        ft.dwLowDateTime = UpdateCB->LastWriteTime.LowPart;
        ft.dwHighDateTime = UpdateCB->LastWriteTime.HighPart;

        cm_UnixTimeFromLargeSearchTime(& clientModTime, &ft);

        if (!bScpLocked) {
            lock_ObtainWrite(&scp->rw);
            bScpLocked = TRUE;
        }
        if (scp->clientModTime != clientModTime) {
            setAttr.mask |= CM_ATTRMASK_CLIENTMODTIME;
            setAttr.clientModTime = clientModTime;
        }

        /* call setattr */
        if (setAttr.mask) {
            lock_ReleaseWrite(&scp->rw);
            bScpLocked = FALSE;
            code = cm_SetAttr(scp, &setAttr, userp, &req);
        } else
            code = 0;
    }

  on_error:
    if (bScpLocked) {
        lock_ReleaseWrite(&scp->rw);
    }

    if (code == 0) {
        DWORD dwRemaining = ResultBufferLength - sizeof( AFSFileUpdateResultCB) + sizeof( AFSDirEnumEntry);

        pResultCB = (AFSFileUpdateResultCB *)(*ResultCB)->ResultData;

        pResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;

        code = RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining,
                                        dscp, scp, userp, &req, NULL, NULL,
                                        RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                        0, NULL, &dwRemaining);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        osi_Log0(afsd_logp, "RDR_UpdateFileEntry SUCCESS");
    } else {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_UpdateFileEntry FAILURE code=0x%x status=0x%x",
                  code, status);
    }
    cm_ReleaseSCache(scp);
    cm_ReleaseSCache(dscp);

    return;
}

void
RDR_CleanupFileEntry( IN cm_user_t *userp,
                      IN AFSFileID FileId,
                      IN WCHAR *FileNameCounted,
                      IN DWORD FileNameLength,
                      IN AFSFileCleanupCB *CleanupCB,
                      IN BOOL bWow64,
                      IN BOOL bLastHandle,
                      IN BOOL bDeleteFile,
                      IN BOOL bUnlockFile,
                      IN DWORD ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB)
{
    AFSFileCleanupResultCB *pResultCB = NULL;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    cm_fid_t            Fid;
    cm_fid_t            parentFid;
    afs_uint32          code = 0;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_scache_t *       scp = NULL;
    cm_scache_t *       dscp = NULL;
    cm_req_t            req;
    time_t              clientModTime;
    FILETIME            ft;
    DWORD               status;
    BOOL                bScpLocked = FALSE;
    BOOL                bDscpLocked = FALSE;
    BOOL                bFlushFile = FALSE;
    cm_key_t            key;

    RDR_InitReq(&req, bWow64);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    osi_Log4(afsd_logp, "RDR_CleanupFileEntry parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              CleanupCB->ParentId.Cell, CleanupCB->ParentId.Volume,
              CleanupCB->ParentId.Vnode, CleanupCB->ParentId.Unique);
    osi_Log4(afsd_logp, "... object FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);

    *ResultCB = (AFSCommResult *)malloc( size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_CleanupFileEntry Out of Memory");
	return;
    }

    memset( *ResultCB,
            '\0',
            size);

    parentFid.cell   = CleanupCB->ParentId.Cell;
    parentFid.volume = CleanupCB->ParentId.Volume;
    parentFid.vnode  = CleanupCB->ParentId.Vnode;
    parentFid.unique = CleanupCB->ParentId.Unique;
    parentFid.hash   = CleanupCB->ParentId.Hash;

    if (parentFid.cell) {
        code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            if ( status == STATUS_INVALID_HANDLE)
                status = STATUS_OBJECT_PATH_INVALID;
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_CleanupFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                     code, status);
            return;
        }

        lock_ObtainWrite(&dscp->rw);
        bDscpLocked = TRUE;
        code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                         CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            osi_Log2(afsd_logp, "RDR_CleanupFileEntry cm_SyncOp failure dscp=0x%p code=0x%x",
                    dscp, code);
            if (code)
                goto on_error;
        }

        cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&dscp->rw);
        bDscpLocked = FALSE;

        if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
            (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
            cm_ReleaseSCache(dscp);
            osi_Log1(afsd_logp, "RDR_CleanupFileEntry Not a Directory dscp=0x%p",
                     dscp);
            if (code)
                goto on_error;
        }
    }

    Fid.cell   = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode  = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash   = FileId.Hash;

    code = cm_GetSCache(&Fid, dscp ? &dscp->fid : NULL, &scp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_CleanupFileEntry cm_GetSCache object FID failure code=0x%x",
                 code);
        goto on_error;
    }

    lock_ObtainWrite(&scp->rw);
    bScpLocked = TRUE;
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    if (code) {
        osi_Log2(afsd_logp, "RDR_CleanupFileEntry cm_SyncOp failure scp=0x%p code=0x%x",
                 scp, code);
        goto on_error;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    if (bLastHandle && (scp->fileType == CM_SCACHETYPE_FILE) &&
        scp->redirBufCount > 0)
    {
        LARGE_INTEGER heldExtents;
        AFSFileExtentCB extentList[1024];
        DWORD extentCount = 0;
        cm_buf_t *srbp;
        time_t now;

        time(&now);
        heldExtents.QuadPart = 0;

        for ( srbp = redirq_to_cm_buf_t(scp->redirQueueT);
              srbp;
              srbp = redirq_to_cm_buf_t(osi_QPrev(&srbp->redirq)))
        {
            extentList[extentCount].Flags = 0;
            extentList[extentCount].Length = cm_data.blockSize;
            extentList[extentCount].FileOffset.QuadPart = srbp->offset.QuadPart;
            extentList[extentCount].CacheOffset.QuadPart = srbp->datap - RDR_extentBaseAddress;
            lock_ObtainWrite(&buf_globalLock);
            srbp->redirReleaseRequested = now;
            lock_ReleaseWrite(&buf_globalLock);
            extentCount++;

            if (extentCount == 1024) {
                lock_ReleaseWrite(&scp->rw);
                code = RDR_RequestExtentRelease(&scp->fid, heldExtents, extentCount, extentList);
                if (code) {
                    if (code == CM_ERROR_RETRY) {
                        /*
                         * The redirector either is not holding the extents or cannot let them
                         * go because they are otherwise in use.  At the moment, do nothing.
                         */
                    } else
                        break;
                }
                extentCount = 0;
                bFlushFile = TRUE;
                lock_ObtainWrite(&scp->rw);
            }
        }

        if (code == 0 && extentCount > 0) {
            if (bScpLocked) {
                lock_ReleaseWrite(&scp->rw);
                bScpLocked = FALSE;
            }
            code = RDR_RequestExtentRelease(&scp->fid, heldExtents, extentCount, extentList);
            bFlushFile = TRUE;
        }
    }

    /* No longer in use by redirector */
    if (!bScpLocked) {
        lock_ObtainWrite(&scp->rw);
        bScpLocked = TRUE;
    }

    if (bLastHandle) {
        lock_AssertWrite(&scp->rw);
        scp->flags &= ~CM_SCACHEFLAG_RDR_IN_USE;
    }

    /* If not a readonly object, flush dirty data and update metadata */
    if (!(scp->flags & CM_SCACHEFLAG_RO)) {
        if ((scp->fileType == CM_SCACHETYPE_FILE) && (bLastHandle || bFlushFile)) {
            /* Serialize with any outstanding AsyncStore operation */
            code = cm_SyncOp(scp, NULL, userp, &req, 0, CM_SCACHESYNC_ASYNCSTORE);
            if (code == 0) {
                cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_ASYNCSTORE);

                code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_WRITE,
                                 CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
                /*
                 * If we only have 'i' bits, then we should still be able to
                 * set flush the file.
                 */
                if (code == CM_ERROR_NOACCESS && scp->creator == userp) {
                    code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_INSERT,
                                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
                }
                if (code == 0) {
                    if (bScpLocked) {
                        lock_ReleaseWrite(&scp->rw);
                        bScpLocked = FALSE;
                    }

                    code = cm_FSync(scp, userp, &req, bScpLocked);
                }
            }
            if (bLastHandle && code)
                goto unlock;
        }

        if (CleanupCB->ChangeTime.QuadPart) {

            if (scp->fileType == CM_SCACHETYPE_FILE) {
                /* Do not set length and other attributes at the same time */
                if (scp->length.QuadPart != CleanupCB->AllocationSize.QuadPart) {
                    osi_Log2(afsd_logp, "RDR_CleanupFileEntry Length Change 0x%x -> 0x%x",
                             (afs_uint32)scp->length.QuadPart, (afs_uint32)CleanupCB->AllocationSize.QuadPart);
                    setAttr.mask |= CM_ATTRMASK_LENGTH;
                    setAttr.length.LowPart = CleanupCB->AllocationSize.LowPart;
                    setAttr.length.HighPart = CleanupCB->AllocationSize.HighPart;

                    if (bScpLocked) {
                        lock_ReleaseWrite(&scp->rw);
                        bScpLocked = FALSE;
                    }
                    code = cm_SetAttr(scp, &setAttr, userp, &req);
                    if (code)
                        goto unlock;
                    setAttr.mask = 0;
                }
            }

            if (!bScpLocked) {
                lock_ObtainWrite(&scp->rw);
                bScpLocked = TRUE;
            }

            if ((scp->unixModeBits & 0200) && (CleanupCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
                setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
                setAttr.unixModeBits = scp->unixModeBits & ~0222;
            } else if (!(scp->unixModeBits & 0200) && !(CleanupCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
                setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
                setAttr.unixModeBits = scp->unixModeBits | 0222;
            }
        }

        if (CleanupCB->LastWriteTime.QuadPart) {
            ft.dwLowDateTime = CleanupCB->LastWriteTime.LowPart;
            ft.dwHighDateTime = CleanupCB->LastWriteTime.HighPart;

            cm_UnixTimeFromLargeSearchTime(&clientModTime, &ft);
            if (scp->clientModTime != clientModTime) {
                setAttr.mask |= CM_ATTRMASK_CLIENTMODTIME;
                setAttr.clientModTime = clientModTime;
            }
        }

        /* call setattr */
        if (setAttr.mask) {
            if (bScpLocked) {
                lock_ReleaseWrite(&scp->rw);
                bScpLocked = FALSE;
            }
            code = cm_SetAttr(scp, &setAttr, userp, &req);
        } else
            code = 0;
    }

  unlock:
    /* Now drop the lock enforcing the share access */
    if ( CleanupCB->FileAccess != AFS_FILE_ACCESS_NOLOCK) {
        unsigned int sLockType;
        LARGE_INTEGER LOffset, LLength;

        if (CleanupCB->FileAccess == AFS_FILE_ACCESS_SHARED)
            sLockType = LOCKING_ANDX_SHARED_LOCK;
        else
            sLockType = 0;

        key = cm_GenerateKey(CM_SESSION_IFS, SMB_FID_QLOCK_PID, CleanupCB->Identifier);

        LOffset.HighPart = SMB_FID_QLOCK_HIGH;
        LOffset.LowPart = SMB_FID_QLOCK_LOW;
        LLength.HighPart = 0;
        LLength.LowPart = SMB_FID_QLOCK_LENGTH;

        if (!bScpLocked) {
            lock_ObtainWrite(&scp->rw);
            bScpLocked = TRUE;
        }

        code = cm_SyncOp(scp, NULL, userp, &req, 0, CM_SCACHESYNC_LOCK);
        if (code == 0)
        {
            code = cm_Unlock(scp, sLockType, LOffset, LLength, key, 0, userp, &req);

            cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

            if (code == CM_ERROR_RANGE_NOT_LOCKED)
            {
                osi_Log3(afsd_logp, "RDR_CleanupFileEntry Range Not Locked -- FileAccess 0x%x ProcessId 0x%x HandleId 0x%x",
                         CleanupCB->FileAccess, CleanupCB->ProcessId, CleanupCB->Identifier);

            }
        }
    }

    if (bUnlockFile || bDeleteFile) {
        if (!bScpLocked) {
            lock_ObtainWrite(&scp->rw);
            bScpLocked = TRUE;
        }
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
			 CM_SCACHESYNC_LOCK);
        if (code) {
            osi_Log2(afsd_logp, "RDR_CleanupFileEntry cm_SyncOp (2) failure scp=0x%p code=0x%x",
                     scp, code);
            goto on_error;
        }

        key = cm_GenerateKey(CM_SESSION_IFS, CleanupCB->ProcessId, 0);

        /* the scp is now locked and current */
        code = cm_UnlockByKey(scp, key,
                              bDeleteFile ? CM_UNLOCK_FLAG_BY_FID : 0,
                              userp, &req);

	cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

        if (code)
            goto on_error;
    }

  on_error:
    if (bDscpLocked)
        lock_ReleaseWrite(&dscp->rw);
    if (bScpLocked)
        lock_ReleaseWrite(&scp->rw);

    if (code == 0 && dscp && bDeleteFile) {
        WCHAR FileName[260];

        StringCchCopyNW(FileName, 260, FileNameCounted, FileNameLength / sizeof(WCHAR));

        if (scp->fileType == CM_SCACHETYPE_DIRECTORY)
            code = cm_RemoveDir(dscp, NULL, FileName, userp, &req);
        else
            code = cm_Unlink(dscp, NULL, FileName, userp, &req);
    }

    if (code == 0) {
        if ( ResultBufferLength >=  sizeof( AFSFileCleanupResultCB))
        {
            (*ResultCB)->ResultBufferLength = sizeof( AFSFileCleanupResultCB);
            pResultCB = (AFSFileCleanupResultCB *)&(*ResultCB)->ResultData;
            pResultCB->ParentDataVersion.QuadPart = dscp ? dscp->dataVersion : 0;
        } else {
            (*ResultCB)->ResultBufferLength = 0;
        }

        (*ResultCB)->ResultStatus = 0;
        osi_Log0(afsd_logp, "RDR_CleanupFileEntry SUCCESS");
    } else {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_CleanupFileEntry FAILURE code=0x%x status=0x%x",
                  code, status);
    }

    if (scp)
        cm_ReleaseSCache(scp);
    if (dscp)
        cm_ReleaseSCache(dscp);

    return;
}

void
RDR_DeleteFileEntry( IN cm_user_t *userp,
                     IN AFSFileID ParentId,
                     IN ULONGLONG ProcessId,
                     IN WCHAR *FileNameCounted,
                     IN DWORD FileNameLength,
                     IN BOOL bWow64,
                     IN BOOL bCheckOnly,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{

    AFSFileDeleteResultCB *pResultCB = NULL;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    cm_fid_t            parentFid;
    afs_uint32          code;
    cm_scache_t *       dscp = NULL;
    cm_scache_t *       scp = NULL;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_req_t            req;
    DWORD               status;
    wchar_t             FileName[260];
    cm_key_t            key;

    StringCchCopyNW(FileName, 260, FileNameCounted, FileNameLength / sizeof(WCHAR));

    osi_Log4(afsd_logp, "RDR_DeleteFileEntry parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              ParentId.Cell,  ParentId.Volume,
              ParentId.Vnode, ParentId.Unique);
    osi_Log2(afsd_logp, "... name=%S checkOnly=%x",
             osi_LogSaveStringW(afsd_logp, FileName),
             bCheckOnly);

    RDR_InitReq(&req, bWow64);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    *ResultCB = (AFSCommResult *)malloc( size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_DeleteFileEntry out of memory");
	return;
    }

    memset( *ResultCB,
            '\0',
            size);

    parentFid.cell   = ParentId.Cell;
    parentFid.volume = ParentId.Volume;
    parentFid.vnode  = ParentId.Vnode;
    parentFid.unique = ParentId.Unique;
    parentFid.hash   = ParentId.Hash;

    code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        if ( status == STATUS_INVALID_HANDLE)
            status = STATUS_OBJECT_PATH_INVALID;
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_DeleteFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&dscp->rw);

    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        osi_Log3(afsd_logp, "RDR_DeleteFileEntry cm_SyncOp failure dscp=0x%p code=0x%x status=0x%x",
                 dscp, code, status);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "RDR_DeleteFileEntry Not a Directory dscp=0x%p",
                 dscp);
        return;
    }

    code = cm_Lookup(dscp, FileName, 0, userp, &req, &scp);
    if (code && code != CM_ERROR_INEXACT_MATCH) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        cm_ReleaseSCache(dscp);
        osi_Log2(afsd_logp, "RDR_DeleteFileEntry cm_Lookup failure code=0x%x status=0x%x",
                 code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_DELETE,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        cm_ReleaseSCache(dscp);
        osi_Log3(afsd_logp, "RDR_DeleteFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    if (scp->fileType == CM_SCACHETYPE_DIRECTORY) {
        cm_dirOp_t dirop;

        lock_ReleaseWrite(&scp->rw);

        code = cm_BeginDirOp(scp, userp, &req, CM_DIRLOCK_READ,
                             CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
            /* is the directory empty? if not, CM_ERROR_NOTEMPTY */
            afs_uint32 bEmpty;

            code = cm_BPlusDirIsEmpty(&dirop, &bEmpty);
            if (code == 0 && !bEmpty)
                code = CM_ERROR_NOTEMPTY;

            cm_EndDirOp(&dirop);
        }

        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            (*ResultCB)->ResultBufferLength = 0;
            cm_ReleaseSCache(scp);
            cm_ReleaseSCache(dscp);
            osi_Log3(afsd_logp, "RDR_DeleteFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                     scp, code, status);
            return;
        }
        lock_ObtainWrite(&scp->rw);
    }

    if (!bCheckOnly) {
        /* Drop all locks since the file is being deleted */
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
			 CM_SCACHESYNC_LOCK);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            (*ResultCB)->ResultBufferLength = 0;
            lock_ReleaseWrite(&scp->rw);
            cm_ReleaseSCache(scp);
            cm_ReleaseSCache(dscp);
            osi_Log3(afsd_logp, "RDR_DeleteFileEntry cm_SyncOp Lock failure scp=0x%p code=0x%x status=0x%x",
                     scp, code, status);
        }

        /* the scp is now locked and current */
        key = cm_GenerateKey(CM_SESSION_IFS, ProcessId, 0);

        code = cm_UnlockByKey(scp, key,
                              CM_UNLOCK_FLAG_BY_FID,
                              userp, &req);

	cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);
        lock_ReleaseWrite(&scp->rw);

        if (scp->fileType == CM_SCACHETYPE_DIRECTORY)
            code = cm_RemoveDir(dscp, NULL, FileName, userp, &req);
        else
            code = cm_Unlink(dscp, NULL, FileName, userp, &req);
    } else {
        lock_ReleaseWrite(&scp->rw);
    }

    if (code == 0) {
        (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

        (*ResultCB)->ResultBufferLength = sizeof( AFSFileDeleteResultCB);

        pResultCB = (AFSFileDeleteResultCB *)(*ResultCB)->ResultData;

        pResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;
        osi_Log0(afsd_logp, "RDR_DeleteFileEntry SUCCESS");
    } else {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_DeleteFileEntry FAILURE code=0x%x status=0x%x",
                  code, status);
    }

    cm_ReleaseSCache(dscp);
    cm_ReleaseSCache(scp);

    return;
}

void
RDR_RenameFileEntry( IN cm_user_t *userp,
                     IN WCHAR    *SourceFileNameCounted,
                     IN DWORD     SourceFileNameLength,
                     IN AFSFileID SourceFileId,
                     IN AFSFileRenameCB *pRenameCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{

    AFSFileRenameResultCB *pResultCB = NULL;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    AFSFileID              SourceParentId   = pRenameCB->SourceParentId;
    AFSFileID              TargetParentId   = pRenameCB->TargetParentId;
    WCHAR *                TargetFileNameCounted = pRenameCB->TargetName;
    DWORD                  TargetFileNameLength = pRenameCB->TargetNameLength;
    cm_fid_t               SourceParentFid;
    cm_fid_t               TargetParentFid;
    cm_fid_t		   SourceFid;
    cm_fid_t		   OrigTargetFid = {0,0,0,0,0};
    cm_fid_t		   TargetFid;
    cm_scache_t *          oldDscp;
    cm_scache_t *          newDscp;
    cm_dirOp_t dirop;
    wchar_t                shortName[13];
    wchar_t                SourceFileName[260];
    wchar_t                TargetFileName[260];
    cm_dirFid_t            dfid;
    cm_req_t               req;
    afs_uint32             code;
    DWORD                  status;

    RDR_InitReq(&req, bWow64);

    StringCchCopyNW(SourceFileName, 260, SourceFileNameCounted, SourceFileNameLength / sizeof(WCHAR));
    StringCchCopyNW(TargetFileName, 260, TargetFileNameCounted, TargetFileNameLength / sizeof(WCHAR));

    osi_Log4(afsd_logp, "RDR_RenameFileEntry Source Parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              SourceParentId.Cell,  SourceParentId.Volume,
              SourceParentId.Vnode, SourceParentId.Unique);
    osi_Log2(afsd_logp, "... Source Name=%S Length %u", osi_LogSaveStringW(afsd_logp, SourceFileName), SourceFileNameLength);
    osi_Log4(afsd_logp, "... Target Parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              TargetParentId.Cell,  TargetParentId.Volume,
              TargetParentId.Vnode, TargetParentId.Unique);
    osi_Log2(afsd_logp, "... Target Name=%S Length %u", osi_LogSaveStringW(afsd_logp, TargetFileName), TargetFileNameLength);

    *ResultCB = (AFSCommResult *)malloc( size);
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            size);

    pResultCB = (AFSFileRenameResultCB *)(*ResultCB)->ResultData;

    if (SourceFileNameLength == 0 || TargetFileNameLength == 0)
    {
        osi_Log2(afsd_logp, "RDR_RenameFileEntry Invalid Name Length: src %u target %u",
                 SourceFileNameLength, TargetFileNameLength);
        (*ResultCB)->ResultStatus = STATUS_INVALID_PARAMETER;
        return;
    }

    SourceParentFid.cell   = SourceParentId.Cell;
    SourceParentFid.volume = SourceParentId.Volume;
    SourceParentFid.vnode  = SourceParentId.Vnode;
    SourceParentFid.unique = SourceParentId.Unique;
    SourceParentFid.hash   = SourceParentId.Hash;

    TargetParentFid.cell   = TargetParentId.Cell;
    TargetParentFid.volume = TargetParentId.Volume;
    TargetParentFid.vnode  = TargetParentId.Vnode;
    TargetParentFid.unique = TargetParentId.Unique;
    TargetParentFid.hash   = TargetParentId.Hash;

    code = cm_GetSCache(&SourceParentFid, NULL, &oldDscp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_GetSCache source parent failed code 0x%x", code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        if ( status == STATUS_INVALID_HANDLE)
            status = STATUS_OBJECT_PATH_INVALID;
        (*ResultCB)->ResultStatus = status;
        return;
    }

    lock_ObtainWrite(&oldDscp->rw);
    code = cm_SyncOp(oldDscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        osi_Log2(afsd_logp, "RDR_RenameFileEntry cm_SyncOp oldDscp 0x%p failed code 0x%x", oldDscp, code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        if ( status == STATUS_INVALID_HANDLE)
            status = STATUS_OBJECT_PATH_INVALID;
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&oldDscp->rw);
        cm_ReleaseSCache(oldDscp);
        return;
    }

    cm_SyncOpDone(oldDscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&oldDscp->rw);


    if (oldDscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        osi_Log1(afsd_logp, "RDR_RenameFileEntry oldDscp 0x%p not a directory", oldDscp);
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(oldDscp);
        return;
    }

    code = cm_GetSCache(&TargetParentFid, NULL, &newDscp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_GetSCache target parent failed code 0x%x", code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        cm_ReleaseSCache(oldDscp);
        return;
    }

    lock_ObtainWrite(&newDscp->rw);
    code = cm_SyncOp(newDscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        osi_Log2(afsd_logp, "RDR_RenameFileEntry cm_SyncOp newDscp 0x%p failed code 0x%x", newDscp, code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&newDscp->rw);
        cm_ReleaseSCache(oldDscp);
        cm_ReleaseSCache(newDscp);
        return;
    }

    cm_SyncOpDone(newDscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&newDscp->rw);


    if (newDscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        osi_Log1(afsd_logp, "RDR_RenameFileEntry newDscp 0x%p not a directory", newDscp);
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(oldDscp);
        cm_ReleaseSCache(newDscp);
        return;
    }

    /* Obtain the original FID just for debugging purposes */
    code = cm_BeginDirOp( oldDscp, userp, &req, CM_DIRLOCK_READ, CM_DIROP_FLAG_NONE, &dirop);
    if (code == 0) {
        code = cm_BPlusDirLookup(&dirop, SourceFileName, &SourceFid);
        code = cm_BPlusDirLookup(&dirop, TargetFileName, &OrigTargetFid);
        cm_EndDirOp(&dirop);
    }

    code = cm_Rename( oldDscp, NULL, SourceFileName,
                      newDscp, TargetFileName, userp, &req);
    if (code == 0) {
        cm_scache_t *scp = 0;
        DWORD dwRemaining;

        (*ResultCB)->ResultBufferLength = ResultBufferLength;
        dwRemaining = ResultBufferLength - sizeof( AFSFileRenameResultCB) + sizeof( AFSDirEnumEntry);
        (*ResultCB)->ResultStatus = 0;

        pResultCB->SourceParentDataVersion.QuadPart = oldDscp->dataVersion;
        pResultCB->TargetParentDataVersion.QuadPart = newDscp->dataVersion;

        osi_Log2(afsd_logp, "RDR_RenameFileEntry cm_Rename oldDscp 0x%p newDscp 0x%p SUCCESS",
                 oldDscp, newDscp);

        code = cm_BeginDirOp( newDscp, userp, &req, CM_DIRLOCK_READ, CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
            code = cm_BPlusDirLookup(&dirop, TargetFileName, &TargetFid);
            cm_EndDirOp(&dirop);
        }

	if (code != 0 && code != CM_ERROR_INEXACT_MATCH) {
            osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_BPlusDirLookup failed code 0x%x",
                     code);
            (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
            cm_ReleaseSCache(oldDscp);
            cm_ReleaseSCache(newDscp);
            return;
        }

        osi_Log4(afsd_logp, "RDR_RenameFileEntry Target FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
                  TargetFid.cell,  TargetFid.volume,
                  TargetFid.vnode, TargetFid.unique);

        code = cm_GetSCache(&TargetFid, &newDscp->fid, &scp, userp, &req);
        if (code) {
            osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_GetSCache target failed code 0x%x", code);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            cm_ReleaseSCache(oldDscp);
            cm_ReleaseSCache(newDscp);
            return;
        }

        /* Make sure the source vnode is current */
        lock_ObtainWrite(&scp->rw);
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            osi_Log2(afsd_logp, "RDR_RenameFileEntry cm_SyncOp scp 0x%p failed code 0x%x", scp, code);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&scp->rw);
            cm_ReleaseSCache(oldDscp);
            cm_ReleaseSCache(newDscp);
            cm_ReleaseSCache(scp);
            return;
        }

        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&scp->rw);

        if (cm_shortNames) {
            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            if (!cm_Is8Dot3(TargetFileName))
                cm_Gen8Dot3NameIntW(TargetFileName, &dfid, shortName, NULL);
            else
                shortName[0] = '\0';
        }

        RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining,
                                 newDscp, scp, userp, &req, TargetFileName, shortName,
                                 RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                 0, NULL, &dwRemaining);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        cm_ReleaseSCache(scp);

        osi_Log0(afsd_logp, "RDR_RenameFileEntry SUCCESS");
    } else {
        osi_Log3(afsd_logp, "RDR_RenameFileEntry cm_Rename oldDscp 0x%p newDscp 0x%p failed code 0x%x",
                 oldDscp, newDscp, code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
    }

    cm_ReleaseSCache(oldDscp);
    cm_ReleaseSCache(newDscp);
    return;
}

/*
 * AFS does not support cross-directory hard links but RDR_HardLinkFileEntry
 * is written as if AFS does.  The check for cross-directory links is
 * implemented in cm_Link().
 *
 * Windows supports optional ReplaceIfExists functionality.  The AFS file
 * server does not.  If the target name already exists and bReplaceIfExists
 * is true, check to see if the user has insert permission before calling
 * cm_Unlink() on the existing object.  If the user does not have insert
 * permission return STATUS_ACCESS_DENIED.
 */

void
RDR_HardLinkFileEntry( IN cm_user_t *userp,
                       IN WCHAR    *SourceFileNameCounted,
                       IN DWORD     SourceFileNameLength,
                       IN AFSFileID SourceFileId,
                       IN AFSFileHardLinkCB *pHardLinkCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB)
{

    AFSFileHardLinkResultCB *pResultCB = NULL;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    AFSFileID              SourceParentId   = pHardLinkCB->SourceParentId;
    AFSFileID              TargetParentId   = pHardLinkCB->TargetParentId;
    WCHAR *                TargetFileNameCounted = pHardLinkCB->TargetName;
    DWORD                  TargetFileNameLength = pHardLinkCB->TargetNameLength;
    cm_fid_t               SourceParentFid;
    cm_fid_t               TargetParentFid;
    cm_fid_t		   SourceFid;
    cm_fid_t		   OrigTargetFid = {0,0,0,0,0};
    cm_scache_t *          srcDscp = NULL;
    cm_scache_t *          targetDscp = NULL;
    cm_scache_t *          srcScp = NULL;
    cm_dirOp_t             dirop;
    wchar_t                shortName[13];
    wchar_t                SourceFileName[260];
    wchar_t                TargetFileName[260];
    cm_dirFid_t            dfid;
    cm_req_t               req;
    afs_uint32             code;
    DWORD                  status;

    RDR_InitReq(&req, bWow64);

    StringCchCopyNW(SourceFileName, 260, SourceFileNameCounted, SourceFileNameLength / sizeof(WCHAR));
    StringCchCopyNW(TargetFileName, 260, TargetFileNameCounted, TargetFileNameLength / sizeof(WCHAR));

    osi_Log4(afsd_logp, "RDR_HardLinkFileEntry Source Parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              SourceParentId.Cell,  SourceParentId.Volume,
              SourceParentId.Vnode, SourceParentId.Unique);
    osi_Log2(afsd_logp, "... Source Name=%S Length %u", osi_LogSaveStringW(afsd_logp, SourceFileName), SourceFileNameLength);
    osi_Log4(afsd_logp, "... Target Parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              TargetParentId.Cell,  TargetParentId.Volume,
              TargetParentId.Vnode, TargetParentId.Unique);
    osi_Log2(afsd_logp, "... Target Name=%S Length %u", osi_LogSaveStringW(afsd_logp, TargetFileName), TargetFileNameLength);

    *ResultCB = (AFSCommResult *)malloc( size);
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            size);

    pResultCB = (AFSFileHardLinkResultCB *)(*ResultCB)->ResultData;

    if (SourceFileNameLength == 0 || TargetFileNameLength == 0)
    {
        osi_Log2(afsd_logp, "RDR_HardLinkFileEntry Invalid Name Length: src %u target %u",
                 SourceFileNameLength, TargetFileNameLength);
        (*ResultCB)->ResultStatus = STATUS_INVALID_PARAMETER;
        return;
    }

    SourceFid.cell   = SourceFileId.Cell;
    SourceFid.volume = SourceFileId.Volume;
    SourceFid.vnode  = SourceFileId.Vnode;
    SourceFid.unique = SourceFileId.Unique;
    SourceFid.hash   = SourceFileId.Hash;

    SourceParentFid.cell   = SourceParentId.Cell;
    SourceParentFid.volume = SourceParentId.Volume;
    SourceParentFid.vnode  = SourceParentId.Vnode;
    SourceParentFid.unique = SourceParentId.Unique;
    SourceParentFid.hash   = SourceParentId.Hash;

    TargetParentFid.cell   = TargetParentId.Cell;
    TargetParentFid.volume = TargetParentId.Volume;
    TargetParentFid.vnode  = TargetParentId.Vnode;
    TargetParentFid.unique = TargetParentId.Unique;
    TargetParentFid.hash   = TargetParentId.Hash;

    code = cm_GetSCache(&SourceFid, NULL, &srcScp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_HardLinkFileEntry cm_GetSCache source failed code 0x%x", code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    code = cm_GetSCache(&TargetParentFid, NULL, &targetDscp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_HardLinkFileEntry cm_GetSCache target parent failed code 0x%x", code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        cm_ReleaseSCache(srcScp);
        return;
    }

    lock_ObtainWrite(&targetDscp->rw);
    code = cm_SyncOp(targetDscp, NULL, userp, &req, PRSFS_INSERT,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        osi_Log2(afsd_logp, "RDR_HardLinkFileEntry cm_SyncOp targetDscp 0x%p failed code 0x%x", targetDscp, code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&targetDscp->rw);
        cm_ReleaseSCache(srcScp);
        cm_ReleaseSCache(targetDscp);
        return;
    }

    cm_SyncOpDone(targetDscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&targetDscp->rw);

    if (targetDscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        osi_Log1(afsd_logp, "RDR_HardLinkFileEntry targetDscp 0x%p not a directory", targetDscp);
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(srcScp);
        cm_ReleaseSCache(targetDscp);
        return;
    }

    if ( cm_FidCmp(&SourceParentFid, &TargetParentFid) ) {
        code = cm_GetSCache(&SourceParentFid, NULL, &srcDscp, userp, &req);
        if (code) {
            osi_Log1(afsd_logp, "RDR_HardLinkFileEntry cm_GetSCache source parent failed code 0x%x", code);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            if ( status == STATUS_INVALID_HANDLE)
                status = STATUS_OBJECT_PATH_INVALID;
            (*ResultCB)->ResultStatus = status;
            cm_ReleaseSCache(srcScp);
            cm_ReleaseSCache(targetDscp);
            return;
        }

        lock_ObtainWrite(&srcDscp->rw);
        code = cm_SyncOp(srcDscp, NULL, userp, &req, 0,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            osi_Log2(afsd_logp, "RDR_HardLinkFileEntry cm_SyncOp srcDscp 0x%p failed code 0x%x", srcDscp, code);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            if ( status == STATUS_INVALID_HANDLE)
                status = STATUS_OBJECT_PATH_INVALID;
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&srcDscp->rw);
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(targetDscp);
            cm_ReleaseSCache(srcScp);
            return;
        }

        cm_SyncOpDone(srcDscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&srcDscp->rw);

        if (srcDscp->fileType != CM_SCACHETYPE_DIRECTORY) {
            osi_Log1(afsd_logp, "RDR_HardLinkFileEntry srcDscp 0x%p not a directory", srcDscp);
            (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(targetDscp);
            cm_ReleaseSCache(srcScp);
            return;
        }
    } else {
        srcDscp = targetDscp;
    }

    /* Obtain the target FID if it exists */
    code = cm_BeginDirOp( targetDscp, userp, &req, CM_DIRLOCK_READ, CM_DIROP_FLAG_NONE, &dirop);
    if (code == 0) {
        code = cm_BPlusDirLookup(&dirop, TargetFileName, &OrigTargetFid);
        cm_EndDirOp(&dirop);
    }

    if (OrigTargetFid.vnode) {

        /* An object exists with the target name */
        if (!pHardLinkCB->bReplaceIfExists) {
            osi_Log0(afsd_logp, "RDR_HardLinkFileEntry target name collision and !ReplaceIfExists");
            (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_COLLISION;
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(targetDscp);
            cm_ReleaseSCache(srcScp);
            return;
        }

        lock_ObtainWrite(&targetDscp->rw);
        code = cm_SyncOp(targetDscp, NULL, userp, &req, PRSFS_INSERT | PRSFS_DELETE,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&srcDscp->rw);
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(targetDscp);
            cm_ReleaseSCache(srcScp);
            return;
        }
        cm_SyncOpDone(targetDscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&targetDscp->rw);

        code = cm_Unlink(targetDscp, NULL, TargetFileName, userp, &req);
        if (code) {
            osi_Log1(afsd_logp, "RDR_HardLinkFileEntry cm_Unlink code 0x%x", code);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&srcDscp->rw);
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(targetDscp);
            cm_ReleaseSCache(srcScp);
            return;
        }
    }

    code = cm_Link( targetDscp, TargetFileName, srcScp, 0, userp, &req);
    if (code == 0) {
        cm_fid_t TargetFid;
        cm_scache_t *targetScp = 0;
        DWORD dwRemaining;

        (*ResultCB)->ResultBufferLength = ResultBufferLength;
        dwRemaining = ResultBufferLength - sizeof( AFSFileHardLinkResultCB) + sizeof( AFSDirEnumEntry);
        (*ResultCB)->ResultStatus = 0;

        pResultCB->SourceParentDataVersion.QuadPart = srcDscp->dataVersion;
        pResultCB->TargetParentDataVersion.QuadPart = targetDscp->dataVersion;

        osi_Log2(afsd_logp, "RDR_HardLinkFileEntry cm_Link srcDscp 0x%p targetDscp 0x%p SUCCESS",
                 srcDscp, targetDscp);

        code = cm_BeginDirOp( targetDscp, userp, &req, CM_DIRLOCK_READ, CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
            code = cm_BPlusDirLookup(&dirop, TargetFileName, &TargetFid);
            cm_EndDirOp(&dirop);
        }

	if (code != 0 && code != CM_ERROR_INEXACT_MATCH) {
            osi_Log1(afsd_logp, "RDR_HardLinkFileEntry cm_BPlusDirLookup failed code 0x%x",
                     code);
            (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(srcScp);
            cm_ReleaseSCache(targetDscp);
            return;
        }

        osi_Log4(afsd_logp, "RDR_HardLinkFileEntry Target FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
                  TargetFid.cell,  TargetFid.volume,
                  TargetFid.vnode, TargetFid.unique);

        code = cm_GetSCache(&TargetFid, &targetDscp->fid, &targetScp, userp, &req);
        if (code) {
            osi_Log1(afsd_logp, "RDR_HardLinkFileEntry cm_GetSCache target failed code 0x%x", code);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(srcScp);
            cm_ReleaseSCache(targetDscp);
            return;
        }

        /* Make sure the source vnode is current */
        lock_ObtainWrite(&targetScp->rw);
        code = cm_SyncOp(targetScp, NULL, userp, &req, 0,
                         CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            osi_Log2(afsd_logp, "RDR_HardLinkFileEntry cm_SyncOp scp 0x%p failed code 0x%x",
                     targetScp, code);
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&targetScp->rw);
            cm_ReleaseSCache(targetScp);
            if (srcDscp != targetDscp)
                cm_ReleaseSCache(srcDscp);
            cm_ReleaseSCache(srcScp);
            cm_ReleaseSCache(targetDscp);
            return;
        }

        cm_SyncOpDone(targetScp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&targetScp->rw);

        if (cm_shortNames) {
            dfid.vnode = htonl(targetScp->fid.vnode);
            dfid.unique = htonl(targetScp->fid.unique);

            if (!cm_Is8Dot3(TargetFileName))
                cm_Gen8Dot3NameIntW(TargetFileName, &dfid, shortName, NULL);
            else
                shortName[0] = '\0';
        }

        RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining,
                                 targetDscp, targetScp, userp, &req, TargetFileName, shortName,
                                 RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                 0, NULL, &dwRemaining);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        cm_ReleaseSCache(targetScp);

        osi_Log0(afsd_logp, "RDR_HardLinkFileEntry SUCCESS");
    } else {
        osi_Log3(afsd_logp, "RDR_HardLinkFileEntry cm_Link srcDscp 0x%p targetDscp 0x%p failed code 0x%x",
                 srcDscp, targetDscp, code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
    }

    cm_ReleaseSCache(srcScp);
    if (srcDscp != targetDscp)
        cm_ReleaseSCache(srcDscp);
    cm_ReleaseSCache(targetDscp);
    return;
}


void
RDR_CreateSymlinkEntry( IN cm_user_t *userp,
                        IN AFSFileID FileId,
                        IN WCHAR *FileNameCounted,
                        IN DWORD FileNameLength,
                        IN AFSCreateSymlinkCB *SymlinkCB,
                        IN BOOL bWow64,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    AFSCreateSymlinkResultCB *pResultCB = NULL;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    cm_fid_t            parentFid;
    cm_fid_t            Fid;
    afs_uint32          code;
    cm_scache_t *       dscp = NULL;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_scache_t *       scp = NULL;
    cm_req_t            req;
    DWORD               status;
    wchar_t             FileName[260];
    char               *TargetPath = NULL;

    StringCchCopyNW(FileName, 260, FileNameCounted, FileNameLength / sizeof(WCHAR));
    TargetPath = cm_Utf16ToUtf8Alloc( SymlinkCB->TargetName,  SymlinkCB->TargetNameLength / sizeof(WCHAR), NULL);

    osi_Log4( afsd_logp, "RDR_CreateSymlinkEntry parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              SymlinkCB->ParentId.Cell, SymlinkCB->ParentId.Volume,
              SymlinkCB->ParentId.Vnode, SymlinkCB->ParentId.Unique);
    osi_Log1(afsd_logp, "... name=%S", osi_LogSaveStringW(afsd_logp, FileName));

    RDR_InitReq(&req, bWow64);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_CreateSymlinkEntry out of memory");
        free(TargetPath);
	return;
    }

    memset( *ResultCB,
            '\0',
            size);

    parentFid.cell   = SymlinkCB->ParentId.Cell;
    parentFid.volume = SymlinkCB->ParentId.Volume;
    parentFid.vnode  = SymlinkCB->ParentId.Vnode;
    parentFid.unique = SymlinkCB->ParentId.Unique;
    parentFid.hash   = SymlinkCB->ParentId.Hash;

    code = cm_GetSCache(&parentFid, NULL, &dscp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        if ( status == STATUS_INVALID_HANDLE)
            status = STATUS_OBJECT_PATH_INVALID;
        osi_Log2(afsd_logp, "RDR_CreateSymlinkEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        free(TargetPath);
        return;
    }

    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        osi_Log3(afsd_logp, "RDR_CreateSymlinkEntry cm_SyncOp failure (1) dscp=0x%p code=0x%x status=0x%x",
                 dscp, code, status);
        free(TargetPath);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "RDR_CreateSymlinkEntry Not a Directory dscp=0x%p",
                 dscp);
        free(TargetPath);
        return;
    }

    Fid.cell   = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode  = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash   = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        if ( status == STATUS_INVALID_HANDLE)
            status = STATUS_OBJECT_PATH_INVALID;
        osi_Log2(afsd_logp, "RDR_CreateSymlinkEntry cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        free(TargetPath);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_CreateSymlinkEntry cm_SyncOp failure (1) scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        free(TargetPath);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    /* Remove the temporary object */
    if (scp->fileType == CM_SCACHETYPE_DIRECTORY)
        code = cm_RemoveDir(dscp, NULL, FileName, userp, &req);
    else
        code = cm_Unlink(dscp, NULL, FileName, userp, &req);
    cm_ReleaseSCache(scp);
    scp = NULL;
    if (code && code != CM_ERROR_NOSUCHFILE) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        cm_ReleaseSCache(dscp);
        osi_Log2(afsd_logp, "RDR_CreateSymlinkEntry Unable to delete file dscp=0x%p code=0x%x",
                 dscp, code);
        free(TargetPath);
        return;
    }

    /*
     * The target path is going to be provided by the redirector in one of the following forms:
     *
     * 1. Relative path.
     * 2. Absolute path prefaced as \??\UNC\<server>\<share>\<path>
     * 3. Absolute path prefaced as \??\<drive-letter>:\<path>
     *
     * Relative paths can be used with just slash conversion.  Absolute paths must be converted.
     * UNC paths with a server name that matches cm_NetbiosName then the path is an AFS path and
     * it must be converted to /<server>/<share>/<path>.  Other UNC paths must be converted to
     * msdfs:\\<server>\<share>\<path>.  Local disk paths should be converted to
     * msdfs:<drive-letter>:<path>.
     */

    if ( TargetPath[0] == '\\' ) {
        size_t nbNameLen = strlen(cm_NetbiosName);
        size_t len;
        char  *s;

        if ( strncmp(TargetPath, "\\??\\UNC\\", 8) == 0) {

            if (strncmp(&TargetPath[8], cm_NetbiosName, nbNameLen) == 0 &&
                TargetPath[8 + nbNameLen] == '\\')
            {
                /* AFS path */
                s = strdup(&TargetPath[8 + nbNameLen]);
                free(TargetPath);
                TargetPath = s;
                for (; *s; s++) {
                    if (*s == '\\')
                        *s = '/';
                }
            } else {
                /*
                 * non-AFS UNC path (msdfs:\\server\share\path)
                 * strlen("msdfs:\\") == 7 + 1 for the NUL
                 */
                len = 8 + strlen(&TargetPath[7]);
                s = malloc(8 + strlen(&TargetPath[7]));
                StringCbCopy(s, len, "msdfs:\\");
                StringCbCat(s, len, &TargetPath[7]);
                free(TargetPath);
                TargetPath = s;
            }
        } else {
            /* non-UNC path (msdfs:<drive>:\<path> */
            s = strdup(&TargetPath[4]);
            free(TargetPath);
            TargetPath = s;
        }

    } else {
        /* relative paths require slash conversion */
        char *s = TargetPath;
        for (; *s; s++) {
            if (*s == '\\')
                *s = '/';
        }
    }

    /* Use current time */
    setAttr.mask = CM_ATTRMASK_UNIXMODEBITS | CM_ATTRMASK_CLIENTMODTIME;
    setAttr.unixModeBits = 0755;
    setAttr.clientModTime = time(NULL);

    code = cm_SymLink(dscp, FileName, TargetPath, flags, &setAttr, userp, &req, &scp);
    free(TargetPath);

    if (code == 0) {
        wchar_t shortName[13]=L"";
        cm_dirFid_t dfid;
        DWORD dwRemaining;

        if (dscp->flags & CM_SCACHEFLAG_ANYWATCH) {
            smb_NotifyChange(FILE_ACTION_ADDED,
                             FILE_NOTIFY_CHANGE_DIR_NAME,
                             dscp, FileName, NULL, TRUE);
        }

        (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

        (*ResultCB)->ResultBufferLength = sizeof( AFSCreateSymlinkResultCB);

        pResultCB = (AFSCreateSymlinkResultCB *)(*ResultCB)->ResultData;

        dwRemaining = ResultBufferLength - sizeof( AFSCreateSymlinkResultCB) + sizeof( AFSDirEnumEntry);

        lock_ObtainWrite(&dscp->rw);
        code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&dscp->rw);
            cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            osi_Log3(afsd_logp, "RDR_CreateSymlinkEntry cm_SyncOp failure (2) dscp=0x%p code=0x%x status=0x%x",
                      dscp, code, status);
            return;
        }

        pResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;

        cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&dscp->rw);

        if (cm_shortNames) {
            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            if (!cm_Is8Dot3(FileName))
                cm_Gen8Dot3NameIntW(FileName, &dfid, shortName, NULL);
            else
                shortName[0] = '\0';
        }

        code = RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining,
                                        dscp, scp, userp, &req, FileName, shortName,
                                        RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                        0, NULL, &dwRemaining);
        cm_ReleaseSCache(scp);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        osi_Log0(afsd_logp, "RDR_CreateSymlinkEntry SUCCESS");
    } else {
        (*ResultCB)->ResultStatus = STATUS_FILE_DELETED;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_CreateSymlinkEntry FAILURE code=0x%x status=0x%x",
                  code, STATUS_FILE_DELETED);
    }

    cm_ReleaseSCache(dscp);

    return;
}


void
RDR_FlushFileEntry( IN cm_user_t *userp,
                    IN AFSFileID FileId,
                    IN BOOL bWow64,
                    IN DWORD ResultBufferLength,
                    IN OUT AFSCommResult **ResultCB)
{
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;
#ifdef ODS_DEBUG
    char        dbgstr[1024];
#endif

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_FlushFileEntry File FID cell 0x%x vol 0x%x vno 0x%x uniq 0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);
#ifdef ODS_DEBUG
    snprintf( dbgstr, 1024,
              "RDR_FlushFileEntry File FID cell 0x%x vol 0x%x vno 0x%x uniq 0x%x\n",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);
    OutputDebugStringA( dbgstr);
#endif

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_FlushFileEntry out of memory");
	return;
    }

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Process the release */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_FlushFileEntry cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    if (scp->flags & CM_SCACHEFLAG_DELETED) {
        lock_ReleaseWrite(&scp->rw);
        (*ResultCB)->ResultStatus = STATUS_INVALID_HANDLE;
        return;
    }

    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_FlushFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    code = cm_FSync(scp, userp, &req, FALSE);
    cm_ReleaseSCache(scp);

    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_FlushFileEntry FAILURE code=0x%x status=0x%x",
                  code, status);
    } else {
        (*ResultCB)->ResultStatus = 0;
        osi_Log0(afsd_logp, "RDR_FlushFileEntry SUCCESS");
    }
    (*ResultCB)->ResultBufferLength = 0;

    return;
}

afs_uint32
RDR_CheckAccess( IN cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp,
                 ULONG access,
                 ULONG *granted)
{
    ULONG afs_acc, afs_gr;
    BOOLEAN file, dir;
    afs_uint32 code = 0;

    file = (scp->fileType == CM_SCACHETYPE_FILE);
    dir = !file;

    /* access definitions from prs_fs.h */
    afs_acc = 0;
    if (access & FILE_READ_DATA)
	afs_acc |= PRSFS_READ;
    if (access & FILE_READ_EA || access & FILE_READ_ATTRIBUTES)
	afs_acc |= PRSFS_READ;
    if (file && ((access & FILE_WRITE_DATA) || (access & FILE_APPEND_DATA)))
	afs_acc |= PRSFS_WRITE;
    if (access & FILE_WRITE_EA || access & FILE_WRITE_ATTRIBUTES)
	afs_acc |= PRSFS_WRITE;
    if (dir && ((access & FILE_ADD_FILE) || (access & FILE_ADD_SUBDIRECTORY)))
	afs_acc |= PRSFS_INSERT;
    if (dir && (access & FILE_LIST_DIRECTORY))
	afs_acc |= PRSFS_LOOKUP;
    if (file && (access & FILE_EXECUTE))
	afs_acc |= PRSFS_WRITE;
    if (dir && (access & FILE_TRAVERSE))
	afs_acc |= PRSFS_READ;
    if (dir && (access & FILE_DELETE_CHILD))
	afs_acc |= PRSFS_DELETE;
    if ((access & DELETE))
	afs_acc |= PRSFS_DELETE;

    /* check ACL with server */
    lock_ObtainWrite(&scp->rw);
    while (1)
    {
	if (cm_HaveAccessRights(scp, userp, reqp, afs_acc, &afs_gr))
        {
            break;
        }
	else
        {
            /* we don't know the required access rights */
            code = cm_GetAccessRights(scp, userp, reqp);
            if (code)
                break;
            continue;
        }
    }
    lock_ReleaseWrite(&(scp->rw));

    if (code == 0) {
        *granted = 0;
        if (afs_gr & PRSFS_READ)
            *granted |= FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES | FILE_EXECUTE;
        if (afs_gr & PRSFS_WRITE)
            *granted |= FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_EXECUTE;
        if (afs_gr & PRSFS_INSERT)
            *granted |= (dir ? FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY : 0) | (file ? FILE_ADD_SUBDIRECTORY : 0);
        if (afs_gr & PRSFS_LOOKUP)
            *granted |= (dir ? FILE_LIST_DIRECTORY : 0);
        if (afs_gr & PRSFS_DELETE)
            *granted |= FILE_DELETE_CHILD | DELETE;
        if (afs_gr & PRSFS_LOCK)
            *granted |= 0;
        if (afs_gr & PRSFS_ADMINISTER)
            *granted |= 0;

        *granted |= SYNCHRONIZE | READ_CONTROL;

        /* don't give more access than what was requested */
        *granted &= access;
        osi_Log3(afsd_logp, "RDR_CheckAccess SUCCESS scp=0x%p requested=0x%x granted=0x%x", scp, access, *granted);
    } else
        osi_Log2(afsd_logp, "RDR_CheckAccess FAILURE scp=0x%p code=0x%x",
                 scp, code);

    return code;
}

void
RDR_OpenFileEntry( IN cm_user_t *userp,
                   IN AFSFileID FileId,
                   IN AFSFileOpenCB *OpenCB,
                   IN BOOL bWow64,
                   IN BOOL bHoldFid,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB)
{
    AFSFileOpenResultCB *pResultCB = NULL;
    cm_scache_t *scp = NULL;
    cm_user_t   *sysUserp = NULL;
    cm_fid_t    Fid;
    cm_lock_data_t      *ldp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_OpenFileEntry File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof( AFSFileOpenResultCB));
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_OpenFileEntry out of memory");
	return;
    }

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof( AFSFileOpenResultCB));

    pResultCB = (AFSFileOpenResultCB *)(*ResultCB)->ResultData;

    /* Process the release */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_OpenFileEntry cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_OpenFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    sysUserp = RDR_GetLocalSystemUser();

    /*
     * Skip the open check if the request is coming from the local system account.
     * The local system has no tokens and therefore any requests sent to a file
     * server will fail.  Unfortunately, there are special system processes that
     * perform actions on files and directories in preparation for memory mapping
     * executables.  If the open check fails, the real request from the user process
     * will never be issued.
     *
     * Permitting the file system to allow subsequent operations to proceed does
     * not compromise security.  All requests to obtain file data or directory
     * enumerations will subsequently fail if they are not submitted under the
     * context of a process for that have access to the necessary credentials.
     */

    if ( userp == sysUserp)
    {
        osi_Log1(afsd_logp, "RDR_OpenFileEntry LOCAL_SYSTEM access check skipped scp=0x%p",
                 scp);
        pResultCB->GrantedAccess = OpenCB->DesiredAccess;
        pResultCB->FileAccess = AFS_FILE_ACCESS_NOLOCK;
        code = 0;
    }
    else
    {
        int count = 0;

        do {
            if (count++ > 0) {
                Sleep(350);
                osi_Log3(afsd_logp,
                         "RDR_OpenFileEntry repeating open check scp=0x%p userp=0x%p code=0x%x",
                         scp, userp, code);
            }
            code = cm_CheckNTOpen(scp, OpenCB->DesiredAccess, OpenCB->ShareAccess,
                                  OPEN_ALWAYS,
                                  OpenCB->ProcessId, OpenCB->Identifier,
                                  userp, &req, &ldp);
            if (code == 0)
                code = RDR_CheckAccess(scp, userp, &req, OpenCB->DesiredAccess, &pResultCB->GrantedAccess);


            cm_CheckNTOpenDone(scp, userp, &req, &ldp);
        } while (count < 100 && (code == CM_ERROR_RETRY || code == CM_ERROR_WOULDBLOCK));
    }

    /*
     * If we are restricting sharing, we should do so with a suitable
     * share lock.
     */
    if (code == 0 && scp->fileType == CM_SCACHETYPE_FILE && !(OpenCB->ShareAccess & FILE_SHARE_WRITE)) {
        cm_key_t key;
        LARGE_INTEGER LOffset, LLength;
        int sLockType;

        LOffset.HighPart = SMB_FID_QLOCK_HIGH;
        LOffset.LowPart = SMB_FID_QLOCK_LOW;
        LLength.HighPart = 0;
        LLength.LowPart = SMB_FID_QLOCK_LENGTH;

        /*
         * If we are not opening the file for writing, then we don't
         * try to get an exclusive lock.  No one else should be able to
         * get an exclusive lock on the file anyway, although someone
         * else can get a shared lock.
         */
        if ((OpenCB->ShareAccess & FILE_SHARE_READ) || !(OpenCB->DesiredAccess & AFS_ACCESS_WRITE))
        {
            sLockType = LOCKING_ANDX_SHARED_LOCK;
        } else {
            sLockType = 0;
        }

        key = cm_GenerateKey(CM_SESSION_IFS, SMB_FID_QLOCK_PID, OpenCB->Identifier);

        lock_ObtainWrite(&scp->rw);
        code = cm_Lock(scp, sLockType, LOffset, LLength, key, 0, userp, &req, NULL);
        lock_ReleaseWrite(&scp->rw);

        if (code) {
            code = CM_ERROR_SHARING_VIOLATION;
            pResultCB->FileAccess = AFS_FILE_ACCESS_NOLOCK;
        } else {
            if (sLockType == LOCKING_ANDX_SHARED_LOCK)
                pResultCB->FileAccess = AFS_FILE_ACCESS_SHARED;
            else
                pResultCB->FileAccess = AFS_FILE_ACCESS_EXCLUSIVE;
        }
    } else {
        pResultCB->FileAccess = AFS_FILE_ACCESS_NOLOCK;
    }

    cm_ReleaseUser(sysUserp);
    if (code == 0 && bHoldFid)
        RDR_FlagScpInUse( scp, FALSE );
    cm_ReleaseSCache(scp);

    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_OpenFileEntry FAILURE code=0x%x status=0x%x",
                  code, status);
    } else {
        (*ResultCB)->ResultStatus = 0;
        (*ResultCB)->ResultBufferLength = sizeof( AFSFileOpenResultCB);
        osi_Log0(afsd_logp, "RDR_OpenFileEntry SUCCESS");
    }
    return;
}

void
RDR_ReleaseFileAccess( IN cm_user_t *userp,
                       IN AFSFileID FileId,
                       IN AFSFileAccessReleaseCB *ReleaseFileCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB)
{
    cm_key_t key;
    unsigned int sLockType;
    LARGE_INTEGER LOffset, LLength;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_ReleaseFileAccess File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_ReleaseFileAccess out of memory");
	return;
    }

    memset( *ResultCB, '\0', sizeof( AFSCommResult));

    if (ReleaseFileCB->FileAccess == AFS_FILE_ACCESS_NOLOCK)
        return;

    /* Process the release */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_ReleaseFileAccess cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    if (ReleaseFileCB->FileAccess == AFS_FILE_ACCESS_SHARED)
        sLockType = LOCKING_ANDX_SHARED_LOCK;
    else
        sLockType = 0;

    key = cm_GenerateKey(CM_SESSION_IFS, SMB_FID_QLOCK_PID, ReleaseFileCB->Identifier);

    LOffset.HighPart = SMB_FID_QLOCK_HIGH;
    LOffset.LowPart = SMB_FID_QLOCK_LOW;
    LLength.HighPart = 0;
    LLength.LowPart = SMB_FID_QLOCK_LENGTH;

    lock_ObtainWrite(&scp->rw);

    code = cm_SyncOp(scp, NULL, userp, &req, 0, CM_SCACHESYNC_LOCK);
    if (code == 0)
    {
        code = cm_Unlock(scp, sLockType, LOffset, LLength, key, 0, userp, &req);

        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

        if (code == CM_ERROR_RANGE_NOT_LOCKED)
        {
            osi_Log3(afsd_logp, "RDR_ReleaseFileAccess Range Not Locked -- FileAccess 0x%x ProcessId 0x%x HandleId 0x%x",
                     ReleaseFileCB->FileAccess, ReleaseFileCB->ProcessId, ReleaseFileCB->Identifier);
        }
    }

    lock_ReleaseWrite(&scp->rw);

    osi_Log0(afsd_logp, "RDR_ReleaseFileAccessEntry SUCCESS");
}

static const char *
HexCheckSum(unsigned char * buf, int buflen, unsigned char * md5cksum)
{
    int i, k;
    static char tr[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

    if (buflen < 33)
        return "buffer length too small to HexCheckSum";

    for (i=0;i<16;i++) {
        k = md5cksum[i];

        buf[i*2] = tr[k / 16];
        buf[i*2+1] = tr[k % 16];
    }
    buf[32] = '\0';

    return buf;
}

/*
 * Extent requests from the file system are triggered when a file
 * page is not resident in the Windows cache.  The file system is
 * responsible for loading the page but cannot block the request
 * while doing so.  The AFS Redirector forwards the requests to
 * the AFS cache manager while indicating to Windows that the page
 * is not yet available.  A polling operation will then ensue with
 * the AFS Redirector issuing a RDR_RequestFileExtentsXXX call for
 * each poll attempt.  As each request is received and processed
 * by a separate worker thread in the service, this can lead to
 * contention by multiple threads attempting to claim the same
 * cm_buf_t objects.  Therefore, it is important that
 *
 *  (a) the service avoid processing more than one overlapping
 *      extent request at a time
 *  (b) background daemon processing be used to avoid blocking
 *      of ioctl threads
 *
 * Beginning with the 20091122 build of the redirector, the redirector
 * will not issue an additional RDR_RequestFileExtentsXXX call for
 * each poll request.  Instead, afsd_service is required to track
 * the requests and return them to the redirector or fail the
 * portions of the request that cannot be satisfied.
 *
 * The request processing returns any extents that can be returned
 * immediately to the redirector.  The rest of the requested range(s)
 * are queued as background operations using RDR_BkgFetch().
 */

/* do the background fetch. */
afs_int32
RDR_BkgFetch(cm_scache_t *scp, void *rockp, cm_user_t *userp, cm_req_t *reqp)
{
    osi_hyper_t length;
    osi_hyper_t base;
    osi_hyper_t offset;
    osi_hyper_t end;
    osi_hyper_t fetched;
    osi_hyper_t tblocksize;
    afs_int32 code;
    int rwheld = 0;
    cm_buf_t *bufp = NULL;
    DWORD dwResultBufferLength;
    AFSSetFileExtentsCB *pResultCB;
    DWORD status;
    afs_uint32 count=0;
    AFSFileID FileId;
    int reportErrorToRedir = 0;
    int force_retry = 0;

    FileId.Cell = scp->fid.cell;
    FileId.Volume = scp->fid.volume;
    FileId.Vnode = scp->fid.vnode;
    FileId.Unique = scp->fid.unique;
    FileId.Hash = scp->fid.hash;

    fetched.LowPart = 0;
    fetched.HighPart = 0;
    tblocksize = ConvertLongToLargeInteger(cm_data.buf_blockSize);
    base = ((rock_BkgFetch_t *)rockp)->base;
    length = ((rock_BkgFetch_t *)rockp)->length;
    end = LargeIntegerAdd(base, length);

    osi_Log5(afsd_logp, "Starting BKG Fetch scp 0x%p offset 0x%x:%x length 0x%x:%x",
             scp, base.HighPart, base.LowPart, length.HighPart, length.LowPart);

    /*
     * Make sure we have a callback.
     * This is necessary so that we can return access denied
     * if a callback cannot be granted.
     */
    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, PRSFS_READ,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        osi_Log2(afsd_logp, "RDR_BkgFetch cm_SyncOp failure scp=0x%p code=0x%x",
                 scp, code);
        smb_MapNTError(cm_MapRPCError(code, reqp), &status, TRUE);
        RDR_SetFileStatus( &scp->fid, &userp->authgroup, status);
        return code;
    }
    lock_ReleaseWrite(&scp->rw);

    dwResultBufferLength = (DWORD)(sizeof( AFSSetFileExtentsCB) + sizeof( AFSFileExtentCB) * (length.QuadPart / cm_data.blockSize + 1));
    pResultCB = (AFSSetFileExtentsCB *)malloc( dwResultBufferLength );
    if (!pResultCB)
        return CM_ERROR_RETRY;

    memset( pResultCB, '\0', dwResultBufferLength );
    pResultCB->FileId = FileId;

    for ( code = 0, offset = base;
          code == 0 && LargeIntegerLessThan(offset, end);
          offset = LargeIntegerAdd(offset, tblocksize) )
    {
        int bBufRelease = TRUE;

        if (rwheld) {
            lock_ReleaseWrite(&scp->rw);
            rwheld = 0;
        }

        code = buf_Get(scp, &offset, reqp, 0, &bufp);
        if (code) {
            /*
             * any error from buf_Get() is non-fatal.
             * we need to re-queue this extent fetch.
             */
            force_retry = 1;
            break;
        }

        if (!rwheld) {
            lock_ObtainWrite(&scp->rw);
            rwheld = 1;
        }

        code = cm_GetBuffer(scp, bufp, NULL, userp, reqp);
        if (code == 0) {
            if (!(bufp->qFlags & CM_BUF_QREDIR)) {
#ifdef VALIDATE_CHECK_SUM
#ifdef ODS_DEBUG
                char md5dbg[33];
                char dbgstr[1024];
#endif
#endif
                if (bufp->flags & CM_BUF_DIRTY)
                    cm_BufWrite(scp, &bufp->offset, cm_data.buf_blockSize, CM_BUF_WRITE_SCP_LOCKED, userp, reqp);

                lock_ObtainWrite(&buf_globalLock);
                if (!(bufp->flags & CM_BUF_DIRTY) &&
                    bufp->cmFlags == 0 &&
                    !(bufp->qFlags & CM_BUF_QREDIR)) {
                    buf_InsertToRedirQueue(scp, bufp);
                    lock_ReleaseWrite(&buf_globalLock);

#ifdef VALIDATE_CHECK_SUM
                    buf_ComputeCheckSum(bufp);
#endif
                    pResultCB->FileExtents[count].Flags = 0;
                    pResultCB->FileExtents[count].FileOffset.QuadPart = bufp->offset.QuadPart;
                    pResultCB->FileExtents[count].CacheOffset.QuadPart = bufp->datap - RDR_extentBaseAddress;
                    pResultCB->FileExtents[count].Length = cm_data.blockSize;
                    count++;
                    fetched = LargeIntegerAdd(fetched, tblocksize);
                    bBufRelease = FALSE;

#ifdef VALIDATE_CHECK_SUM
#ifdef ODS_DEBUG
                    HexCheckSum(md5dbg, sizeof(md5dbg), bufp->md5cksum);
                    snprintf( dbgstr, 1024,
                              "RDR_BkgFetch md5 %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                              md5dbg,
                              scp->fid.volume, scp->fid.vnode, scp->fid.unique,
                              pResultCB->FileExtents[count].FileOffset.HighPart,
                              pResultCB->FileExtents[count].FileOffset.LowPart,
                              pResultCB->FileExtents[count].CacheOffset.HighPart,
                              pResultCB->FileExtents[count].CacheOffset.LowPart);
                    OutputDebugStringA( dbgstr);
#endif
#endif
                    osi_Log4(afsd_logp, "RDR_BkgFetch Extent2FS bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                              bufp, bufp->offset.QuadPart, bufp->datap - RDR_extentBaseAddress, cm_data.blockSize);
                } else {
                    lock_ReleaseWrite(&buf_globalLock);
                    if ((bufp->cmFlags != 0) || (bufp->flags & CM_BUF_DIRTY)) {
                        /* An I/O operation is already in progress */
                        force_retry = 1;
                        osi_Log4(afsd_logp, "RDR_BkgFetch Extent2FS Not delivering to Redirector Dirty or Busy bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                                  bufp, bufp->offset.QuadPart, bufp->datap - RDR_extentBaseAddress, cm_data.blockSize);
                    } else {
                        osi_Log4(afsd_logp, "RDR_BkgFetch Extent2FS Already held by Redirector bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                                  bufp, bufp->offset.QuadPart, bufp->datap - RDR_extentBaseAddress, cm_data.blockSize);
                    }
                }
            } else {
                osi_Log4(afsd_logp, "RDR_BkgFetch Extent2FS Already held by Redirector bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                          bufp, bufp->offset.QuadPart, bufp->datap - RDR_extentBaseAddress, cm_data.blockSize);
            }
        } else {
            /*
             * depending on what the error from cm_GetBuffer is
             * it may or may not be fatal.  Only return fatal errors.
             * Re-queue a request for others.
             */
            osi_Log5(afsd_logp, "RDR_BkgFetch Extent2FS FAILURE bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x code 0x%x",
                      bufp, offset.QuadPart, bufp->datap - RDR_extentBaseAddress, cm_data.blockSize, code);
            switch (code) {
            case CM_ERROR_NOACCESS:
            case CM_ERROR_NOSUCHFILE:
            case CM_ERROR_NOSUCHPATH:
            case CM_ERROR_NOSUCHVOLUME:
            case CM_ERROR_NOSUCHCELL:
            case CM_ERROR_INVAL:
            case CM_ERROR_BADFD:
            case CM_ERROR_CLOCKSKEW:
            case RXKADNOAUTH:
            case CM_ERROR_QUOTA:
            case CM_ERROR_LOCK_CONFLICT:
            case EIO:
            case CM_ERROR_INVAL_NET_RESP:
            case CM_ERROR_UNKNOWN:
                /*
                 * these are fatal errors.  deliver what we can
                 * and halt.
                 */
                reportErrorToRedir = 1;
                break;
            default:
                /*
                 * non-fatal errors.  re-queue the exent
                 */
                code = CM_ERROR_RETRY;
                force_retry = 1;
            }
        }

        if (bBufRelease)
            buf_Release(bufp);
    }

    if (!rwheld) {
        lock_ObtainWrite(&scp->rw);
        rwheld = 1;
    }

    /* wakeup anyone who is waiting */
    if (scp->flags & CM_SCACHEFLAG_WAITING) {
        osi_Log1(afsd_logp, "RDR Bkg Fetch Waking scp 0x%p", scp);
        osi_Wakeup((LONG_PTR) &scp->flags);
    }
    lock_ReleaseWrite(&scp->rw);

    if (count > 0) {
        pResultCB->ExtentCount = count;
        RDR_SetFileExtents( pResultCB, dwResultBufferLength);
    }
    free(pResultCB);

    if (reportErrorToRedir) {
        smb_MapNTError(cm_MapRPCError(code, reqp), &status, TRUE);
        RDR_SetFileStatus( &scp->fid, &userp->authgroup, status);
    }

    osi_Log4(afsd_logp, "Ending BKG Fetch scp 0x%p code 0x%x fetched 0x%x:%x",
             scp, code, fetched.HighPart, fetched.LowPart);

    return force_retry ? CM_ERROR_RETRY : code;
}


BOOL
RDR_RequestFileExtentsAsync( IN cm_user_t *userp,
                             IN AFSFileID FileId,
                             IN AFSRequestExtentsCB *RequestExtentsCB,
                             IN BOOL bWow64,
                             IN OUT DWORD * ResultBufferLength,
                             IN OUT AFSSetFileExtentsCB **ResultCB)
{
    AFSSetFileExtentsCB *pResultCB = NULL;
    DWORD Length;
    DWORD count;
    DWORD status;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    cm_buf_t    *bufp;
    afs_uint32  code = 0;
    osi_hyper_t thyper;
    LARGE_INTEGER ByteOffset, BeginOffset, EndOffset, QueueOffset;
    afs_uint32  QueueLength;
    cm_req_t    req;
    BOOLEAN     bBufRelease = TRUE;

    RDR_InitReq(&req, bWow64);
    req.flags |= CM_REQ_NORETRY;

    osi_Log4(afsd_logp, "RDR_RequestFileExtentsAsync File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);
    osi_Log4(afsd_logp, "... Flags 0x%x ByteOffset 0x%x:%x Length 0x%x",
             RequestExtentsCB->Flags,
             RequestExtentsCB->ByteOffset.HighPart, RequestExtentsCB->ByteOffset.LowPart,
             RequestExtentsCB->Length);
    Length = sizeof( AFSSetFileExtentsCB) + sizeof( AFSFileExtentCB) * (RequestExtentsCB->Length / cm_data.blockSize + 1);

    pResultCB = *ResultCB = (AFSSetFileExtentsCB *)malloc( Length );
    if (*ResultCB == NULL) {
        *ResultBufferLength = 0;
        return FALSE;
    }
    *ResultBufferLength = Length;

    memset( pResultCB, '\0', Length );
    pResultCB->FileId = FileId;

    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_RequestFileExtentsAsync cm_GetSCache FID failure code=0x%x",
                  code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        return FALSE;
    }

    /*
     * Make sure we have a callback.
     * This is necessary so that we can return access denied
     * if a callback cannot be granted.
     */
    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_READ,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);
    if (code) {
        cm_ReleaseSCache(scp);
        osi_Log2(afsd_logp, "RDR_RequestFileExtentsAsync cm_SyncOp failure scp=0x%p code=0x%x",
                 scp, code);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        RDR_SetFileStatus( &scp->fid, &userp->authgroup, status);
        return FALSE;
    }

    /* Allocate the extents from the buffer package */
    for ( count = 0,
          ByteOffset = BeginOffset = RequestExtentsCB->ByteOffset,
          EndOffset.QuadPart = ByteOffset.QuadPart + RequestExtentsCB->Length;
          code == 0 && ByteOffset.QuadPart < EndOffset.QuadPart;
          ByteOffset.QuadPart += cm_data.blockSize)
    {
        BOOL bHaveBuffer = FALSE;

        QueueLength = 0;
        thyper.QuadPart = ByteOffset.QuadPart;

        code = buf_Get(scp, &thyper, &req, 0,  &bufp);
        if (code == 0) {
            lock_ObtainMutex(&bufp->mx);
            bBufRelease = TRUE;

            if (bufp->qFlags & CM_BUF_QREDIR) {
                bHaveBuffer = TRUE;
            } else if (bufp->flags & CM_BUF_DIRTY) {
                bHaveBuffer = FALSE;
            } else {
                osi_hyper_t minLength;  /* effective end of file */

                lock_ObtainRead(&scp->rw);
                bHaveBuffer = cm_HaveBuffer(scp, bufp, TRUE);

                if (LargeIntegerGreaterThan(scp->length, scp->serverLength))
                    minLength = scp->serverLength;
                else
                    minLength = scp->length;

                if (LargeIntegerGreaterThanOrEqualTo(bufp->offset, minLength)) {
                    if (!bHaveBuffer) {
                        memset(bufp->datap, 0, cm_data.buf_blockSize);
                        bufp->dataVersion = scp->dataVersion;
                        bHaveBuffer = TRUE;
                    }
                    else if (bufp->dataVersion == CM_BUF_VERSION_BAD) {
                        bufp->dataVersion = scp->dataVersion;
                    }
                }
                else if ((RequestExtentsCB->Flags & AFS_EXTENT_FLAG_CLEAN) &&
                         ByteOffset.QuadPart <= bufp->offset.QuadPart &&
                         EndOffset.QuadPart >= bufp->offset.QuadPart + cm_data.blockSize)
                {
                    memset(bufp->datap, 0, cm_data.blockSize);
                    bufp->dataVersion = scp->dataVersion;
                    buf_SetDirty(bufp, &req, 0, cm_data.blockSize, userp);
                    bHaveBuffer = TRUE;
                }
                lock_ReleaseRead(&scp->rw);
            }

            /*
             * if this buffer is already up to date, skip it.
             */
            if (bHaveBuffer) {
                if (ByteOffset.QuadPart == BeginOffset.QuadPart) {
                    BeginOffset.QuadPart += cm_data.blockSize;
                } else {
                    QueueLength = (afs_uint32)(ByteOffset.QuadPart - BeginOffset.QuadPart);
                    QueueOffset = BeginOffset;
                    BeginOffset = ByteOffset;
                }

                if (!(bufp->qFlags & CM_BUF_QREDIR)) {
#ifdef VALIDATE_CHECK_SUM
#ifdef ODS_DEBUG
                    char md5dbg[33];
                    char dbgstr[1024];
#endif
#endif
                    lock_ObtainWrite(&buf_globalLock);
                    if (!(bufp->qFlags & CM_BUF_QREDIR)) {
                        buf_InsertToRedirQueue(scp, bufp);
                        lock_ReleaseWrite(&buf_globalLock);

#ifdef VALIDATE_CHECK_SUM
                        buf_ComputeCheckSum(bufp);
#endif
                        /* we already have the buffer, return it now */
                        pResultCB->FileExtents[count].Flags = 0;
                        pResultCB->FileExtents[count].FileOffset = ByteOffset;
                        pResultCB->FileExtents[count].CacheOffset.QuadPart = bufp->datap - RDR_extentBaseAddress;
                        pResultCB->FileExtents[count].Length = cm_data.blockSize;
                        count++;

                        bBufRelease = FALSE;

#ifdef VALIDATE_CHECK_SUM
#ifdef ODS_DEBUG
                        HexCheckSum(md5dbg, sizeof(md5dbg), bufp->md5cksum);
                        snprintf( dbgstr, 1024,
                                  "RDR_RequestFileExtentsAsync md5 %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                  md5dbg,
                                  scp->fid.volume, scp->fid.vnode, scp->fid.unique,
                                  pResultCB->FileExtents[count].FileOffset.HighPart,
                                  pResultCB->FileExtents[count].FileOffset.LowPart,
                                  pResultCB->FileExtents[count].CacheOffset.HighPart,
                                  pResultCB->FileExtents[count].CacheOffset.LowPart);
                        OutputDebugStringA( dbgstr);
#endif
#endif
                        osi_Log4(afsd_logp, "RDR_RequestFileExtentsAsync Extent2FS bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                                 bufp, ByteOffset.QuadPart, bufp->datap - RDR_extentBaseAddress, cm_data.blockSize);
                    } else {
                        lock_ReleaseWrite(&buf_globalLock);
                    }
                } else {
                    if (bBufRelease) {
                        /*
                         * The service is not handing off the extent to the redirector in this pass.
                         * However, we know the buffer is in recent use so move the buffer to the
                         * front of the queue
                         */
                        lock_ObtainWrite(&buf_globalLock);
                        buf_MoveToHeadOfRedirQueue(scp, bufp);
                        lock_ReleaseWrite(&buf_globalLock);

                        osi_Log4(afsd_logp, "RDR_RequestFileExtentsAsync Extent2FS Already held by Redirector bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                                 bufp, ByteOffset.QuadPart, bufp->datap - RDR_extentBaseAddress, cm_data.blockSize);
                    }
                }
            }
            lock_ReleaseMutex(&bufp->mx);
            if (bBufRelease)
                buf_Release(bufp);

            if (QueueLength) {
                rock_BkgFetch_t * rockp = malloc(sizeof(*rockp));

                if (rockp) {
                    req.flags &= ~CM_REQ_NORETRY;
                    rockp->base = QueueOffset;
                    rockp->length.LowPart = QueueLength;
                    rockp->length.HighPart = 0;

                    cm_QueueBKGRequest(scp, RDR_BkgFetch, rockp, userp, &req);
                    osi_Log3(afsd_logp, "RDR_RequestFileExtentsAsync Queued a Background Fetch offset 0x%x:%x length 0x%x",
                              QueueOffset.HighPart, QueueOffset.LowPart, QueueLength);
                    req.flags |= CM_REQ_NORETRY;
                } else {
                    code = ENOMEM;
                }
            }
        } else {
            /* No error from buf_Get() can be fatal */
            osi_Log3(afsd_logp, "RDR_RequestFileExtentsAsync buf_Get FAILURE offset 0x%x:%x code 0x%x",
                     BeginOffset.HighPart, BeginOffset.LowPart, code);
        }
    }

    if (BeginOffset.QuadPart != EndOffset.QuadPart) {
        afs_uint32 length = (afs_uint32)(EndOffset.QuadPart - BeginOffset.QuadPart);
        rock_BkgFetch_t * rockp = malloc(sizeof(*rockp));

        if (rockp) {
            req.flags &= ~CM_REQ_NORETRY;
            rockp->base = BeginOffset;
            rockp->length.LowPart = length;
            rockp->length.HighPart = 0;

            cm_QueueBKGRequest(scp, RDR_BkgFetch, rockp, userp, &req);
            osi_Log3(afsd_logp, "RDR_RequestFileExtentsAsync Queued a Background Fetch offset 0x%x:%x length 0x%x",
                     BeginOffset.HighPart, BeginOffset.LowPart, length);
        } else {
            code = ENOMEM;
        }
    }
    cm_ReleaseSCache(scp);

    (*ResultCB)->ExtentCount = count;
    osi_Log1(afsd_logp, "RDR_RequestFileExtentsAsync replying with 0x%x extent records", count);
    return FALSE;
}

/*
 * When processing an extent release the extents must be accepted back by
 * the service even if there is an error condition returned to the redirector.
 * For example, there may no longer be a callback present or the file may
 * have been deleted on the file server.  Regardless, the extents must be
 * put back into the pool.
 */
void
RDR_ReleaseFileExtents( IN cm_user_t *userp,
                        IN AFSFileID FileId,
                        IN AFSReleaseExtentsCB *ReleaseExtentsCB,
                        IN BOOL bWow64,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    DWORD count;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    cm_buf_t    *bufp;
    afs_uint32  code;
    osi_hyper_t thyper;
    cm_req_t    req;
    int         dirty = 0;
    int         released = 0;
    int         deleted = 0;
    DWORD       status;
    rock_BkgStore_t *rockp;
#ifdef ODS_DEBUG
#ifdef VALIDATE_CHECK_SUM
    char md5dbg[33], md5dbg2[33], md5dbg3[33];
#endif
    char dbgstr[1024];
#endif

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_ReleaseFileExtents File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Process the release */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_ReleaseFileExtents cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
    }

    deleted = scp && (scp->flags & CM_SCACHEFLAG_DELETED);

    /*
     * We do not stop processing as a result of being unable to find the cm_scache object.
     * If this occurs something really bad has happened since the cm_scache object must have
     * been recycled while extents were held by the redirector.  However, we will be resilient
     * and carry on without it.
     *
     * If the file is known to be deleted, there is no point attempting to ask the
     * file server about it or update the attributes.
     */
    if (scp && ReleaseExtentsCB->AllocationSize.QuadPart != scp->length.QuadPart &&
        !deleted)
    {
        cm_attr_t setAttr;

        memset(&setAttr, 0, sizeof(cm_attr_t));
        lock_ObtainWrite(&scp->rw);
        if (ReleaseExtentsCB->AllocationSize.QuadPart != scp->length.QuadPart) {

            osi_Log4(afsd_logp, "RDR_ReleaseFileExtents new length fid vol 0x%x vno 0x%x length 0x%x:%x",
                      scp->fid.volume, scp->fid.vnode,
                      ReleaseExtentsCB->AllocationSize.HighPart,
                      ReleaseExtentsCB->AllocationSize.LowPart);

            setAttr.mask |= CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = ReleaseExtentsCB->AllocationSize.LowPart;
            setAttr.length.HighPart = ReleaseExtentsCB->AllocationSize.HighPart;
        }
        lock_ReleaseWrite(&scp->rw);
        if (setAttr.mask)
            code = cm_SetAttr(scp, &setAttr, userp, &req);
    }

    for ( count = 0; count < ReleaseExtentsCB->ExtentCount; count++) {
        AFSFileExtentCB * pExtent = &ReleaseExtentsCB->FileExtents[count];

        thyper.QuadPart = pExtent->FileOffset.QuadPart;

        bufp = buf_Find(&Fid, &thyper);
        if (bufp) {
            if (pExtent->Flags & AFS_EXTENT_FLAG_UNKNOWN) {
                if (!(bufp->qFlags & CM_BUF_QREDIR)) {
                    osi_Log4(afsd_logp, "RDR_ReleaseFileExtents extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                              Fid.volume, Fid.vnode,
                              pExtent->FileOffset.HighPart,
                              pExtent->FileOffset.LowPart);
                    osi_Log2(afsd_logp, "... coffset 0x%x:%x UNKNOWN to redirector; previously released",
                              pExtent->CacheOffset.HighPart,
                              pExtent->CacheOffset.LowPart);
                } else {
                    osi_Log4(afsd_logp, "RDR_ReleaseFileExtents extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                              Fid.volume, Fid.vnode,
                              pExtent->FileOffset.HighPart,
                              pExtent->FileOffset.LowPart);
                    osi_Log2(afsd_logp, "... coffset 0x%x:%x UNKNOWN to redirector; owned by redirector",
                              pExtent->CacheOffset.HighPart,
                              pExtent->CacheOffset.LowPart);
                }
                buf_Release(bufp);
                continue;
            }

            if (pExtent->Flags & AFS_EXTENT_FLAG_IN_USE) {
                osi_Log4(afsd_logp, "RDR_ReleaseFileExtents extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                          Fid.volume, Fid.vnode,
                          pExtent->FileOffset.HighPart,
                          pExtent->FileOffset.LowPart);
                osi_Log2(afsd_logp, "... coffset 0x%x:%x IN_USE by file system",
                          pExtent->CacheOffset.HighPart,
                          pExtent->CacheOffset.LowPart);

                /* Move the buffer to the front of the queue */
                lock_ObtainWrite(&buf_globalLock);
                buf_MoveToHeadOfRedirQueue(scp, bufp);
                lock_ReleaseWrite(&buf_globalLock);
                buf_Release(bufp);
                continue;
            }

            if (bufp->datap - RDR_extentBaseAddress == pExtent->CacheOffset.QuadPart) {
                if (!(bufp->qFlags & CM_BUF_QREDIR)) {
                    osi_Log4(afsd_logp, "RDR_ReleaseFileExtents extent vol 0x%x vno 0x%x foffset 0x%x:%x not held by file system",
                             Fid.volume, Fid.vnode, pExtent->FileOffset.HighPart,
                             pExtent->FileOffset.LowPart);
                    osi_Log2(afsd_logp, "... coffset 0x%x:%x",
                             pExtent->CacheOffset.HighPart,
                             pExtent->CacheOffset.LowPart);
                } else {
                    osi_Log4(afsd_logp, "RDR_ReleaseFileExtents bufp 0x%p vno 0x%x foffset 0x%x:%x",
                              bufp, bufp->fid.vnode, pExtent->FileOffset.HighPart,
                              pExtent->FileOffset.LowPart);
                    osi_Log2(afsd_logp, "... coffset 0x%x:%x",
                             pExtent->CacheOffset.HighPart,
                             pExtent->CacheOffset.LowPart);

                    if (pExtent->Flags || ReleaseExtentsCB->Flags) {
                        lock_ObtainMutex(&bufp->mx);
                        if ( (ReleaseExtentsCB->Flags & AFS_EXTENT_FLAG_RELEASE) ||
                             (pExtent->Flags & AFS_EXTENT_FLAG_RELEASE) )
                        {
                            if (bufp->qFlags & CM_BUF_QREDIR) {
                                lock_ObtainWrite(&buf_globalLock);
                                if (bufp->qFlags & CM_BUF_QREDIR) {
                                    buf_RemoveFromRedirQueue(scp, bufp);
                                    buf_ReleaseLocked(bufp, TRUE);
                                }
                                lock_ReleaseWrite(&buf_globalLock);
                            }
#ifdef ODS_DEBUG
                            snprintf( dbgstr, 1024,
                                      "RDR_ReleaseFileExtents releasing: vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                      Fid.volume, Fid.vnode, Fid.unique,
                                      pExtent->FileOffset.HighPart,
                                      pExtent->FileOffset.LowPart,
                                      pExtent->CacheOffset.HighPart,
                                      pExtent->CacheOffset.LowPart);
                            OutputDebugStringA( dbgstr);
#endif
                            released++;
                        } else {
#ifdef ODS_DEBUG
                            snprintf( dbgstr, 1024,
                                      "RDR_ReleaseFileExtents not releasing: vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                      Fid.volume, Fid.vnode, Fid.unique,
                                      pExtent->FileOffset.HighPart,
                                      pExtent->FileOffset.LowPart,
                                      pExtent->CacheOffset.HighPart,
                                      pExtent->CacheOffset.LowPart);
                            OutputDebugStringA( dbgstr);
#endif
                            osi_Log4( afsd_logp, "RDR_ReleaseFileExtents not releasing vol 0x%x vno 0x%x foffset 0x%x:%x",
                                      Fid.volume, Fid.vnode,
                                      pExtent->FileOffset.HighPart,
                                      pExtent->FileOffset.LowPart);
                            osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                                      pExtent->CacheOffset.HighPart,
                                      pExtent->CacheOffset.LowPart);
                        }

                        if ( (ReleaseExtentsCB->Flags & AFS_EXTENT_FLAG_DIRTY) ||
                             (pExtent->Flags & AFS_EXTENT_FLAG_DIRTY) )
                        {
#ifdef VALIDATE_CHECK_SUM
#ifdef ODS_DEBUG
                            HexCheckSum(md5dbg, sizeof(md5dbg), bufp->md5cksum);
#endif

                            /*
                             * if the saved checksum matches the checksum of the current state of the buffer
                             * then the buffer is the same as what was given to the kernel.
                             */
                            if ( buf_ValidateCheckSum(bufp) ) {
                                buf_ComputeCheckSum(bufp);

                                if (pExtent->Flags & AFS_EXTENT_FLAG_MD5_SET)
                                {
#ifdef ODS_DEBUG
                                    HexCheckSum(md5dbg2, sizeof(md5dbg2), pExtent->MD5);
                                    HexCheckSum(md5dbg3, sizeof(md5dbg3), bufp->md5cksum);
#endif
                                    if (memcmp(bufp->md5cksum, pExtent->MD5, 16))
                                    {
#ifdef ODS_DEBUG
                                        snprintf( dbgstr, 1024,
                                                  "RDR_ReleaseFileExtents dirty flag set but not dirty and user != kernel: old %s kernel %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                                  md5dbg, md5dbg2,md5dbg3,
                                                  Fid.volume, Fid.vnode, Fid.unique,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart,
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                        OutputDebugStringA( dbgstr);
#endif
                                        osi_Log4( afsd_logp, "RDR_ReleaseFileExtents dirty flag set and checksums do not match! vol 0x%x vno 0x%x foffset 0x%x:%x",
                                                  Fid.volume, Fid.vnode,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart);
                                        osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                        buf_SetDirty(bufp, &req, pExtent->DirtyOffset, pExtent->DirtyLength, userp);
                                        dirty++;
                                    } else {
#ifdef ODS_DEBUG
                                        snprintf( dbgstr, 1024,
                                                  "RDR_ReleaseFileExtents dirty flag set but not dirty and user == kernel: old %s kernel %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                                  md5dbg, md5dbg2, md5dbg3,
                                                  Fid.volume, Fid.vnode, Fid.unique,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart,
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                        OutputDebugStringA( dbgstr);
#endif
                                        osi_Log4( afsd_logp, "RDR_ReleaseFileExtents dirty flag set but extent has not changed vol 0x%x vno 0x%x foffset 0x%x:%x",
                                                  Fid.volume, Fid.vnode,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart);
                                        osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                    }
                                } else {
#ifdef ODS_DEBUG
                                        snprintf( dbgstr, 1024,
                                                  "RDR_ReleaseFileExtents dirty flag set but not dirty: vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                                  Fid.volume, Fid.vnode, Fid.unique,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart,
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                        OutputDebugStringA( dbgstr);
#endif
                                        osi_Log4( afsd_logp, "RDR_ReleaseFileExtents dirty flag set but extent has not changed vol 0x%x vno 0x%x foffset 0x%x:%x",
                                                  Fid.volume, Fid.vnode,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart);
                                        osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                }
                            } else {
                                buf_ComputeCheckSum(bufp);
#ifdef ODS_DEBUG
                                if (pExtent->Flags & AFS_EXTENT_FLAG_MD5_SET)
                                {
                                    HexCheckSum(md5dbg3, sizeof(md5dbg3), bufp->md5cksum);
                                    if (memcmp(bufp->md5cksum, pExtent->MD5, 16))
                                    {
                                        snprintf( dbgstr, 1024,
                                                  "RDR_ReleaseFileExtents dirty flag set and dirty and user != kernel: old %s kernel %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                                  md5dbg, md5dbg2,md5dbg3,
                                                  Fid.volume, Fid.vnode, Fid.unique,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart,
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                        OutputDebugStringA( dbgstr);
                                    } else {
                                        snprintf( dbgstr, 1024,
                                                  "RDR_ReleaseFileExtents dirty flag set and dirty and user == kernel: old %s kernel %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                                  md5dbg, md5dbg2,md5dbg3,
                                                  Fid.volume, Fid.vnode, Fid.unique,
                                                  pExtent->FileOffset.HighPart,
                                                  pExtent->FileOffset.LowPart,
                                                  pExtent->CacheOffset.HighPart,
                                                  pExtent->CacheOffset.LowPart);
                                        OutputDebugStringA( dbgstr);
                                    }
                                } else {
                                    snprintf( dbgstr, 1024,
                                              "RDR_ReleaseFileExtents dirty flag set: vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                              Fid.volume, Fid.vnode, Fid.unique,
                                              pExtent->FileOffset.HighPart,
                                              pExtent->FileOffset.LowPart,
                                              pExtent->CacheOffset.HighPart,
                                              pExtent->CacheOffset.LowPart);
                                    OutputDebugStringA( dbgstr);
                                }
#endif
                                buf_SetDirty(bufp, &req, pExtent->DirtyOffset, pExtent->DirtyLength, userp);
                                dirty++;
                            }
#else /* !VALIDATE_CHECK_SUM */
                            buf_SetDirty(bufp, &req, pExtent->DirtyOffset, pExtent->DirtyLength, userp);
                            dirty++;
#endif /* VALIDATE_CHECK_SUM */
                        }
#ifdef VALIDATE_CHECK_SUM
                        else {
#ifdef ODS_DEBUG
                            HexCheckSum(md5dbg, sizeof(md5dbg), bufp->md5cksum);
#endif
                            if ( !buf_ValidateCheckSum(bufp) ) {
                                buf_ComputeCheckSum(bufp);
#ifdef ODS_DEBUG
                                HexCheckSum(md5dbg3, sizeof(md5dbg2), bufp->md5cksum);
                                snprintf( dbgstr, 1024,
                                          "RDR_ReleaseFileExtents dirty flag not set but dirty! old %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                          md5dbg, md5dbg3,
                                          Fid.volume, Fid.vnode, Fid.unique,
                                          pExtent->FileOffset.HighPart,
                                          pExtent->FileOffset.LowPart,
                                          pExtent->CacheOffset.HighPart,
                                          pExtent->CacheOffset.LowPart);
                                OutputDebugStringA( dbgstr);
#endif
                                osi_Log4( afsd_logp, "RDR_ReleaseFileExtents dirty flag not set but extent has changed vol 0x%x vno 0x%x foffset 0x%x:%x",
                                          Fid.volume, Fid.vnode,
                                          pExtent->FileOffset.HighPart,
                                          pExtent->FileOffset.LowPart);
                                osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                                          pExtent->CacheOffset.HighPart,
                                          pExtent->CacheOffset.LowPart);
                                buf_SetDirty(bufp, &req, pExtent->DirtyOffset, pExtent->DirtyLength, userp);
                                dirty++;
                            } else {
                                buf_ComputeCheckSum(bufp);
#ifdef ODS_DEBUG
                                HexCheckSum(md5dbg3, sizeof(md5dbg2), bufp->md5cksum);
                                snprintf( dbgstr, 1024,
                                          "RDR_ReleaseFileExtents dirty flag not set: vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                          Fid.volume, Fid.vnode, Fid.unique,
                                          pExtent->FileOffset.HighPart,
                                          pExtent->FileOffset.LowPart,
                                          pExtent->CacheOffset.HighPart,
                                          pExtent->CacheOffset.LowPart);
                                OutputDebugStringA( dbgstr);
#endif
                                osi_Log4( afsd_logp, "RDR_ReleaseFileExtents dirty flag not set: vol 0x%x vno 0x%x foffset 0x%x:%x",
                                          Fid.volume, Fid.vnode,
                                          pExtent->FileOffset.HighPart,
                                          pExtent->FileOffset.LowPart);
                                osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                                          pExtent->CacheOffset.HighPart,
                                          pExtent->CacheOffset.LowPart);
                            }
                        }
#endif /* VALIDATE_CHECK_SUM */
                        lock_ReleaseMutex(&bufp->mx);
                    }
                }
            }
            else {
                char * datap = RDR_extentBaseAddress + pExtent->CacheOffset.QuadPart;
                cm_buf_t *wbp;

                for (wbp = cm_data.buf_allp; wbp; wbp = wbp->allp) {
                    if (wbp->datap == datap)
                        break;
                }

#ifdef ODS_DEBUG
                snprintf( dbgstr, 1024,
                          "RDR_ReleaseFileExtents non-matching extent vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                          Fid.volume, Fid.vnode, Fid.unique,
                          pExtent->FileOffset.HighPart,
                          pExtent->FileOffset.LowPart,
                          pExtent->CacheOffset.HighPart,
                          pExtent->CacheOffset.LowPart);
                OutputDebugStringA( dbgstr);
#endif
                osi_Log4( afsd_logp, "RDR_ReleaseFileExtents non-matching extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                          Fid.volume, Fid.vnode,
                          pExtent->FileOffset.HighPart,
                          pExtent->FileOffset.LowPart);
                osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                          pExtent->CacheOffset.HighPart,
                          pExtent->CacheOffset.LowPart);
                osi_Log5( afsd_logp, "... belongs to bp 0x%p vol 0x%x vno 0x%x foffset 0x%x:%x",
                          wbp, wbp->fid.volume, wbp->fid.vnode, wbp->offset.HighPart, wbp->offset.LowPart);
            }
            buf_Release(bufp);
        }
        else {
            char * datap = RDR_extentBaseAddress + pExtent->CacheOffset.QuadPart;
            cm_buf_t *wbp;

            for (wbp = cm_data.buf_allp; wbp; wbp = wbp->allp) {
                if (wbp->datap == datap)
                    break;
            }

#ifdef ODS_DEBUG
            snprintf( dbgstr, 1024,
                      "RDR_ReleaseFileExtents unknown extent vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                      Fid.volume, Fid.vnode, Fid.unique,
                      pExtent->FileOffset.HighPart,
                      pExtent->FileOffset.LowPart,
                      pExtent->CacheOffset.HighPart,
                      pExtent->CacheOffset.LowPart);
            OutputDebugStringA( dbgstr);
#endif
            osi_Log4( afsd_logp, "RDR_ReleaseFileExtents unknown extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                      Fid.volume, Fid.vnode,
                      pExtent->FileOffset.HighPart,
                      pExtent->FileOffset.LowPart);
            osi_Log2( afsd_logp, "... coffset 0x%x:%x",
                      pExtent->CacheOffset.HighPart,
                      pExtent->CacheOffset.LowPart);
            osi_Log5( afsd_logp, "... belongs to bp 0x%p vol 0x%x vno 0x%x foffset 0x%x:%x",
                      wbp, wbp->fid.volume, wbp->fid.vnode, wbp->offset.HighPart, wbp->offset.LowPart);
        }
    }

    if (scp) {
        if (deleted) {
            code = 0;
        } else if (ReleaseExtentsCB->Flags & AFS_EXTENT_FLAG_FLUSH) {
            lock_ObtainWrite(&scp->rw);
            code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_WRITE,
                             CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
            if (code == CM_ERROR_NOACCESS && scp->creator == userp) {
                code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_INSERT,
                                 CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
            }
            lock_ReleaseWrite(&scp->rw);
            if (code == 0)
                code = cm_FSync(scp, userp, &req, FALSE);
        }
        else if (dirty) {
            osi_hyper_t offset = {0,0};
            afs_uint32  length = 0;
            afs_uint32  rights = 0;

            lock_ObtainWrite(&scp->rw);
            code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_WRITE,
                             CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
            if (code == CM_ERROR_NOACCESS && scp->creator == userp) {
                code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_INSERT,
                                  CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
            }
            lock_ReleaseWrite(&scp->rw);
            if (code == 0) {
                /*
                 * there is at least one dirty extent on this file.  queue up background store
                 * requests for contiguous blocks
                 */
                for ( count = 0; count < ReleaseExtentsCB->ExtentCount; count++) {
                    if (ReleaseExtentsCB->FileExtents[count].FileOffset.QuadPart == offset.QuadPart + length &&
                         length + cm_data.buf_blockSize <= cm_chunkSize)
                    {
                        length += cm_data.buf_blockSize;
                    } else {
                        if (!(offset.QuadPart == 0 && length == 0)) {
                            rockp = malloc(sizeof(*rockp));
                            if (rockp) {
                                rockp->length = length;
                                rockp->offset = offset;

                                cm_QueueBKGRequest(scp, cm_BkgStore, rockp, userp, &req);

                                /* rock is freed by cm_BkgStore */
                            }
                        }
                        offset.QuadPart = ReleaseExtentsCB->FileExtents[count].FileOffset.QuadPart;
                        length = cm_data.buf_blockSize;
                    }
                }

                /* Store whatever is left */
                rockp = malloc(sizeof(*rockp));
                if (rockp) {
                    rockp->length = length;
                    rockp->offset = offset;

                    cm_QueueBKGRequest(scp, cm_BkgStore, rockp, userp, &req);

                    /* rock is freed by cm_BkgStore */
                }
            }
        }
        cm_ReleaseSCache(scp);
    }

    osi_Log5(afsd_logp, "RDR_ReleaseFileExtents File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x Released %d",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique, released);
    if (code && code != CM_ERROR_WOULDBLOCK) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_ReleaseFileExtents FAILURE code=0x%x status=0x%x",
                  code, status);
    } else {
        (*ResultCB)->ResultStatus = 0;
        osi_Log0(afsd_logp, "RDR_ReleaseFileExtents SUCCESS");
    }
    (*ResultCB)->ResultBufferLength = 0;

    return;
}

DWORD
RDR_ProcessReleaseFileExtentsResult( IN AFSReleaseFileExtentsResultCB *ReleaseFileExtentsResultCB,
                                     IN DWORD ResultBufferLength)
{
    afs_uint32  code = 0;
    cm_req_t    req;
    osi_hyper_t thyper;
    cm_buf_t    *bufp;
    unsigned int fileno, extentno, total_extents = 0;
    AFSReleaseFileExtentsResultFileCB *pNextFileCB;
    rock_BkgStore_t *rockp;
#ifdef ODS_DEBUG
#ifdef VALIDATE_CHECK_SUM
    char md5dbg[33], md5dbg2[33], md5dbg3[33];
#endif
    char dbgstr[1024];
#endif
    RDR_InitReq(&req, FALSE);

    for ( fileno = 0, pNextFileCB = &ReleaseFileExtentsResultCB->Files[0];
          fileno < ReleaseFileExtentsResultCB->FileCount;
          fileno++ ) {
        AFSReleaseFileExtentsResultFileCB *pFileCB = pNextFileCB;
        cm_user_t       *userp = NULL;
        cm_fid_t         Fid;
        cm_scache_t *    scp = NULL;
        int              dirty = 0;
        int              released = 0;
        int              deleted = 0;
        char * p;

        userp = RDR_UserFromAuthGroup( &pFileCB->AuthGroup);

        osi_Log4(afsd_logp, "RDR_ProcessReleaseFileExtentsResult %d.%d.%d.%d",
                  pFileCB->FileId.Cell, pFileCB->FileId.Volume,
                  pFileCB->FileId.Vnode, pFileCB->FileId.Unique);

        /* Process the release */
        Fid.cell = pFileCB->FileId.Cell;
        Fid.volume = pFileCB->FileId.Volume;
        Fid.vnode = pFileCB->FileId.Vnode;
        Fid.unique = pFileCB->FileId.Unique;
        Fid.hash = pFileCB->FileId.Hash;

        if (Fid.cell == 0) {
            osi_Log4(afsd_logp, "RDR_ProcessReleaseFileExtentsResult Invalid FID %d.%d.%d.%d",
                     Fid.cell, Fid.volume, Fid.vnode, Fid.unique);
            code = CM_ERROR_INVAL;
            goto cleanup_file;
        }

        code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
        if (code) {
            osi_Log1(afsd_logp, "RDR_ProcessReleaseFileExtentsResult cm_GetSCache FID failure code=0x%x",
                     code);
            /*
             * A failure to find the cm_scache object cannot prevent the service
             * from accepting the extents back from the redirector.
             */
        }

        deleted = scp && (scp->flags & CM_SCACHEFLAG_DELETED);

        /* if the scp was not found, do not perform the length check */
        if (scp && (pFileCB->AllocationSize.QuadPart != scp->length.QuadPart)) {
            cm_attr_t setAttr;

            memset(&setAttr, 0, sizeof(cm_attr_t));
            lock_ObtainWrite(&scp->rw);
            if (pFileCB->AllocationSize.QuadPart != scp->length.QuadPart) {
                osi_Log4(afsd_logp, "RDR_ReleaseFileExtentsResult length change vol 0x%x vno 0x%x length 0x%x:%x",
                          scp->fid.volume, scp->fid.vnode,
                          pFileCB->AllocationSize.HighPart,
                          pFileCB->AllocationSize.LowPart);
                setAttr.mask |= CM_ATTRMASK_LENGTH;
                setAttr.length.LowPart = pFileCB->AllocationSize.LowPart;
                setAttr.length.HighPart = pFileCB->AllocationSize.HighPart;
            }
            lock_ReleaseWrite(&scp->rw);
            if (setAttr.mask)
                code = cm_SetAttr(scp, &setAttr, userp, &req);
        }

        for ( extentno = 0; extentno < pFileCB->ExtentCount; total_extents++, extentno++ ) {
            AFSFileExtentCB *pExtent = &pFileCB->FileExtents[extentno];

            thyper.QuadPart = pExtent->FileOffset.QuadPart;

            bufp = buf_Find(&Fid, &thyper);
            if (bufp) {
                if (pExtent->Flags & AFS_EXTENT_FLAG_UNKNOWN) {
                    if (!(bufp->qFlags & CM_BUF_QREDIR)) {
                        osi_Log4(afsd_logp, "RDR_ReleaseFileExtentsResult extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                                 Fid.volume, Fid.vnode,
                                 pExtent->FileOffset.HighPart,
                                 pExtent->FileOffset.LowPart);
                        osi_Log2(afsd_logp, "... coffset 0x%x:%x UNKNOWN to redirector; previously released",
                                 pExtent->CacheOffset.HighPart,
                                 pExtent->CacheOffset.LowPart);
                    } else {
                        osi_Log4(afsd_logp, "RDR_ReleaseFileExtentsResult extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                                 Fid.volume, Fid.vnode,
                                 pExtent->FileOffset.HighPart,
                                 pExtent->FileOffset.LowPart);
                        osi_Log2(afsd_logp, "... coffset 0x%x:%x UNKNOWN to redirector; owned by redirector",
                                 pExtent->CacheOffset.HighPart,
                                 pExtent->CacheOffset.LowPart);
                    }
                    buf_Release(bufp);
                    continue;
                }

                if (pExtent->Flags & AFS_EXTENT_FLAG_IN_USE) {
                    osi_Log4(afsd_logp, "RDR_ReleaseFileExtentsResult extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                              Fid.volume, Fid.vnode,
                              pExtent->FileOffset.HighPart,
                              pExtent->FileOffset.LowPart);
                    osi_Log2(afsd_logp, "... coffset 0x%x:%x IN_USE by file system",
                              pExtent->CacheOffset.HighPart,
                              pExtent->CacheOffset.LowPart);

                    /* Move the buffer to the front of the queue */
                    lock_ObtainWrite(&buf_globalLock);
                    buf_MoveToHeadOfRedirQueue(scp, bufp);
                    lock_ReleaseWrite(&buf_globalLock);
                    buf_Release(bufp);
                    continue;
                }

                if (bufp->datap - RDR_extentBaseAddress == pExtent->CacheOffset.QuadPart) {
                    if (!(bufp->qFlags & CM_BUF_QREDIR)) {
                        osi_Log4(afsd_logp, "RDR_ReleaseFileExtentsResult extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                                 Fid.volume, Fid.vnode,
                                 pExtent->FileOffset.HighPart,
                                 pExtent->FileOffset.LowPart);
                        osi_Log2(afsd_logp, "... coffset 0x%x:%x not held by file system",
                                 pExtent->CacheOffset.HighPart,
                                 pExtent->CacheOffset.LowPart);
#ifdef ODS_DEBUG
                        snprintf(dbgstr, 1024,
                                  "RDR_ProcessReleaseFileExtentsResult not held by redirector! flags 0x%x:%x vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                  ReleaseFileExtentsResultCB->Flags, pExtent->Flags,
                                  Fid.volume, Fid.vnode, Fid.unique,
                                  pExtent->FileOffset.HighPart,
                                  pExtent->FileOffset.LowPart,
                                  pExtent->CacheOffset.HighPart,
                                  pExtent->CacheOffset.LowPart);
                        OutputDebugStringA( dbgstr);
#endif
                    } else {
                        osi_Log5(afsd_logp, "RDR_ProcessReleaseFileExtentsResult bufp 0x%p foffset 0x%x:%x coffset 0x%x:%x",
                                 bufp, pExtent->FileOffset.HighPart, pExtent->FileOffset.LowPart,
                                 pExtent->CacheOffset.HighPart, pExtent->CacheOffset.LowPart);

                        if (pExtent->Flags || ReleaseFileExtentsResultCB->Flags) {
                            lock_ObtainMutex(&bufp->mx);
                            if ( (ReleaseFileExtentsResultCB->Flags & AFS_EXTENT_FLAG_RELEASE) ||
                                 (pExtent->Flags & AFS_EXTENT_FLAG_RELEASE) )
                            {
                                if (bufp->qFlags & CM_BUF_QREDIR) {
                                    lock_ObtainWrite(&buf_globalLock);
                                    if (bufp->qFlags & CM_BUF_QREDIR) {
                                        buf_RemoveFromRedirQueue(scp, bufp);
                                        buf_ReleaseLocked(bufp, TRUE);
                                    }
                                    lock_ReleaseWrite(&buf_globalLock);
                                }

#ifdef ODS_DEBUG
                                snprintf(dbgstr, 1024,
                                          "RDR_ProcessReleaseFileExtentsResult extent released: vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                          Fid.volume, Fid.vnode, Fid.unique,
                                          pExtent->FileOffset.HighPart,
                                          pExtent->FileOffset.LowPart,
                                          pExtent->CacheOffset.HighPart,
                                          pExtent->CacheOffset.LowPart);
                                OutputDebugStringA( dbgstr);
#endif

                                released++;
                            } else {
                                osi_Log4(afsd_logp, "RDR_ProcessReleaseFileExtentsResult not releasing vol 0x%x vno 0x%x foffset 0x%x:%x",
                                         Fid.volume, Fid.vnode,
                                         pExtent->FileOffset.HighPart,
                                         pExtent->FileOffset.LowPart);
                                osi_Log2(afsd_logp, "... coffset 0x%x:%x",
                                         pExtent->CacheOffset.HighPart,
                                         pExtent->CacheOffset.LowPart);
#ifdef ODS_DEBUG
                                snprintf(dbgstr, 1024,
                                          "RDR_ProcessReleaseFileExtentsResult not released! vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                          Fid.volume, Fid.vnode, Fid.unique,
                                          pExtent->FileOffset.HighPart,
                                          pExtent->FileOffset.LowPart,
                                          pExtent->CacheOffset.HighPart,
                                          pExtent->CacheOffset.LowPart);
                                OutputDebugStringA( dbgstr);
#endif
                            }

                            if ((ReleaseFileExtentsResultCB->Flags & AFS_EXTENT_FLAG_DIRTY) ||
                                (pExtent->Flags & AFS_EXTENT_FLAG_DIRTY))
                            {
#ifdef VALIDATE_CHECK_SUM
                                if ( buf_ValidateCheckSum(bufp) ) {
#ifdef ODS_DEBUG
                                    HexCheckSum(md5dbg, sizeof(md5dbg), bufp->md5cksum);
                                    if (ReleaseFileExtentsResultCB->Flags & AFS_EXTENT_FLAG_MD5_SET)
                                        HexCheckSum(md5dbg2, sizeof(md5dbg2), pExtent->MD5);
#endif
                                    buf_ComputeCheckSum(bufp);
#ifdef ODS_DEBUG
                                    HexCheckSum(md5dbg3, sizeof(md5dbg), bufp->md5cksum);
#endif
                                    if (ReleaseFileExtentsResultCB->Flags & AFS_EXTENT_FLAG_MD5_SET)
                                    {
                                        if (memcmp(bufp->md5cksum, pExtent->MD5, 16))
                                        {
#ifdef ODS_DEBUG
                                            snprintf(dbgstr, 1024,
                                                      "RDR_ProcessReleaseFileExtentsResult dirty flag set and checksums do not match! user %s kernel %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                                      md5dbg3, md5dbg2,
                                                      Fid.volume, Fid.vnode, Fid.unique,
                                                      pExtent->FileOffset.HighPart,
                                                      pExtent->FileOffset.LowPart,
                                                      pExtent->CacheOffset.HighPart,
                                                      pExtent->CacheOffset.LowPart);
                                            OutputDebugStringA( dbgstr);
#endif
                                            osi_Log4(afsd_logp,
                                                      "RDR_ProcessReleaseFileExtentsResult dirty flag set and checksums do not match! vol 0x%x vno 0x%x foffset 0x%x:%x",
                                                      Fid.volume, Fid.vnode,
                                                      pExtent->FileOffset.HighPart,
                                                      pExtent->FileOffset.LowPart);
                                            osi_Log2(afsd_logp,
                                                      "... coffset 0x%x:%x",
                                                      pExtent->CacheOffset.HighPart,
                                                      pExtent->CacheOffset.LowPart);

                                            if (!deleted) {
                                                buf_SetDirty(bufp, &req, pExtent->DirtyOffset, pExtent->DirtyLength, userp);
                                                dirty++;
                                            }
                                        } else {
#ifdef ODS_DEBUG
                                            snprintf(dbgstr, 1024,
                                                      "RDR_ProcessReleaseFileExtentsResult dirty flag set but extent has not changed! old %s kernel %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                                      md5dbg, md5dbg2, md5dbg3,
                                                      Fid.volume, Fid.vnode, Fid.unique,
                                                      pExtent->FileOffset.HighPart,
                                                      pExtent->FileOffset.LowPart,
                                                      pExtent->CacheOffset.HighPart,
                                                      pExtent->CacheOffset.LowPart);
                                            OutputDebugStringA( dbgstr);
#endif
                                            osi_Log4(afsd_logp,
                                                      "RDR_ProcessReleaseFileExtentsResult dirty flag set but extent has not changed vol 0x%x vno 0x%x foffset 0x%x:%x",
                                                      Fid.volume, Fid.vnode,
                                                      pExtent->FileOffset.HighPart,
                                                      pExtent->FileOffset.LowPart);
                                            osi_Log2(afsd_logp,
                                                      "... coffset 0x%x:%x",
                                                      pExtent->CacheOffset.HighPart,
                                                      pExtent->CacheOffset.LowPart);
                                        }
                                    }
                                }
#else /* !VALIDATE_CHECK_SUM */
                                if (!deleted) {
                                    buf_SetDirty(bufp, &req, pExtent->DirtyOffset, pExtent->DirtyLength, userp);
                                    dirty++;
                                }
#ifdef ODS_DEBUG
                                snprintf(dbgstr, 1024,
                                          "RDR_ProcessReleaseFileExtentsResult dirty! vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                          Fid.volume, Fid.vnode, Fid.unique,
                                          pExtent->FileOffset.HighPart,
                                          pExtent->FileOffset.LowPart,
                                          pExtent->CacheOffset.HighPart,
                                          pExtent->CacheOffset.LowPart);
                                OutputDebugStringA( dbgstr);
#endif
#endif /* VALIDATE_CHECK_SUM */
                            }
#ifdef VALIDATE_CHECK_SUM
                            else {
#ifdef ODS_DEBUG
                                HexCheckSum(md5dbg, sizeof(md5dbg), bufp->md5cksum);
#endif
                                if (!buf_ValidateCheckSum(bufp) ) {
                                    buf_ComputeCheckSum(bufp);
#ifdef ODS_DEBUG
                                    HexCheckSum(md5dbg3, sizeof(md5dbg2), bufp->md5cksum);
                                    snprintf(dbgstr, 1024,
                                             "RDR_ProcessReleaseFileExtentsResult dirty flag not set but dirty! old %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                             md5dbg, md5dbg3,
                                             Fid.volume, Fid.vnode, Fid.unique,
                                             pExtent->FileOffset.HighPart,
                                             pExtent->FileOffset.LowPart,
                                             pExtent->CacheOffset.HighPart,
                                             pExtent->CacheOffset.LowPart);
                                    OutputDebugStringA( dbgstr);
#endif
                                    osi_Log4(afsd_logp, "RDR_ProcessReleaseFileExtentsResult dirty flag NOT set but extent has changed! vol 0x%x vno 0x%x foffset 0x%x:%x",
                                             Fid.volume, Fid.vnode,
                                             pExtent->FileOffset.HighPart,
                                             pExtent->FileOffset.LowPart);
                                    osi_Log2(afsd_logp, "... coffset 0x%x:%x",
                                             pExtent->CacheOffset.HighPart,
                                             pExtent->CacheOffset.LowPart);

				    if (!deleted) {
                                        buf_SetDirty(bufp, &req, pExtent->DirtyOffset, pExtent->DirtyLength, userp);
                                        dirty++;
                                    }
                                } else {
                                    buf_ComputeCheckSum(bufp);
#ifdef ODS_DEBUG
                                    HexCheckSum(md5dbg3, sizeof(md5dbg2), bufp->md5cksum);
                                    snprintf(dbgstr, 1024,
                                             "RDR_ProcessReleaseFileExtentsResult dirty flag not set and not dirty! old %s new %s vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                                             md5dbg, md5dbg3,
                                             Fid.volume, Fid.vnode, Fid.unique,
                                             pExtent->FileOffset.HighPart,
                                             pExtent->FileOffset.LowPart,
                                             pExtent->CacheOffset.HighPart,
                                             pExtent->CacheOffset.LowPart);
                                    OutputDebugStringA( dbgstr);
#endif
                                }
                            }
#endif /* VALIDATE_CHECK_SUM */
                            lock_ReleaseMutex(&bufp->mx);
                        }
                    }
                } else {
                    /* CacheOffset doesn't match bufp->datap */
                    char * datap = RDR_extentBaseAddress + pExtent->CacheOffset.QuadPart;
                    cm_buf_t *wbp;

                    for (wbp = cm_data.buf_allp; wbp; wbp = wbp->allp) {
                        if (wbp->datap == datap)
                            break;
                    }

#ifdef ODS_DEBUG
                    snprintf(dbgstr, 1024,
                             "RDR_ProcessReleaseFileExtentsResult non-matching extent vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x flags 0x%x\n",
                             Fid.volume, Fid.vnode, Fid.unique,
                             pExtent->FileOffset.HighPart,
                             pExtent->FileOffset.LowPart,
                             pExtent->CacheOffset.HighPart,
                             pExtent->CacheOffset.LowPart,
                             pExtent->Flags);
                    OutputDebugStringA( dbgstr);
#endif
                    osi_Log4(afsd_logp, "RDR_ProcessReleaseFileExtentsResult non-matching extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                             Fid.volume, Fid.vnode,
                             pExtent->FileOffset.HighPart,
                             pExtent->FileOffset.LowPart);
                    osi_Log3(afsd_logp, "... coffset 0x%x:%x flags 0x%x",
                             pExtent->CacheOffset.HighPart,
                             pExtent->CacheOffset.LowPart,
                             pExtent->Flags);
                    if (wbp)
                        osi_Log5(afsd_logp, "... coffset belongs to bp 0x%p vol 0x%x vno 0x%x foffset 0x%x:%x",
                                 wbp, wbp->fid.volume, wbp->fid.vnode, wbp->offset.HighPart, wbp->offset.LowPart);
                    else
                        osi_Log0(afsd_logp, "... coffset cannot be found");
                }
                buf_Release(bufp);
            } else {
                if (pExtent->Flags & AFS_EXTENT_FLAG_UNKNOWN) {
                    osi_Log4(afsd_logp, "RDR_ReleaseFileExtentsResult extent vol 0x%x vno 0x%x foffset 0x%x:%x",
                             Fid.volume, Fid.vnode, pExtent->FileOffset.HighPart,
                             pExtent->FileOffset.LowPart);
                    osi_Log2(afsd_logp, "... coffset 0x%x:%x UNKNOWN to redirector; cm_buf not found -- recycled?",
                             pExtent->CacheOffset.HighPart,
                             pExtent->CacheOffset.LowPart);

                    continue;
                }

#ifdef ODS_DEBUG
                snprintf(dbgstr, 1024,
                         "RDR_ProcessReleaseFileExtentsResult buf not found vol 0x%x vno 0x%x uniq 0x%x foffset 0x%x:%x coffset 0x%x:%x\n",
                         Fid.volume, Fid.vnode, Fid.unique,
                         pExtent->FileOffset.HighPart,
                         pExtent->FileOffset.LowPart,
                         pExtent->CacheOffset.HighPart,
                         pExtent->CacheOffset.LowPart);
                OutputDebugStringA( dbgstr);
#endif
                osi_Log4(afsd_logp, "RDR_ProcessReleaseFileExtentsResult buf not found vol 0x%x vno 0x%x foffset 0x%x:%x",
                         Fid.volume, Fid.vnode,
                         pExtent->FileOffset.HighPart,
                         pExtent->FileOffset.LowPart);
                osi_Log2(afsd_logp, "... coffset 0x%x:%x",
                         pExtent->CacheOffset.HighPart,
                         pExtent->CacheOffset.LowPart);
            }
        }

        if (scp && dirty) {
            osi_hyper_t offset = {0,0};
            afs_uint32  length = 0;

            /*
             * there is at least one dirty extent on this file.  queue up background store
             * requests for contiguous blocks
             */
            for ( extentno = 0; extentno < pFileCB->ExtentCount; extentno++ ) {
                AFSFileExtentCB *pExtent = &pFileCB->FileExtents[extentno];
                if (pExtent->FileOffset.QuadPart == offset.QuadPart + length &&
                     length < cm_chunkSize) {
                    length += cm_data.buf_blockSize;
                } else {
                    if (!(offset.QuadPart == 0 && length == 0)) {
                        rockp = malloc(sizeof(*rockp));
                        if (rockp) {
                            rockp->offset = offset;
                            rockp->length = length;

                            cm_QueueBKGRequest(scp, cm_BkgStore, rockp, userp, &req);
                        } else {
                            code = ENOMEM;
                        }
                    }
                    offset.QuadPart = pExtent->FileOffset.QuadPart;
                    length = cm_data.buf_blockSize;
                }
            }

            /* Background store the rest */
            rockp = malloc(sizeof(*rockp));
            if (rockp) {
                rockp->offset = offset;
                rockp->length = length;

                cm_QueueBKGRequest(scp, cm_BkgStore, rockp, userp, &req);
            } else {
                code = ENOMEM;
            }
        }

        osi_Log5(afsd_logp, "RDR_ProcessReleaseFileExtentsResult File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x Released %d",
                  Fid.cell, Fid.volume, Fid.vnode, Fid.unique, released);

      cleanup_file:
        if (userp)
            cm_ReleaseUser(userp);
        if (scp)
            cm_ReleaseSCache(scp);

        p = (char *)pFileCB;
        p += sizeof(AFSReleaseFileExtentsResultFileCB);
        p += sizeof(AFSFileExtentCB) * (pFileCB->ExtentCount - 1);
        pNextFileCB = (AFSReleaseFileExtentsResultFileCB *)p;
    }

    if (total_extents == 0) {
        osi_Log0(afsd_logp, "RDR_ProcessReleaseFileExtentsResult is empty");
        code = CM_ERROR_RETRY;
    }

    if (code)
        osi_Log1(afsd_logp, "RDR_ProcessReleaseFileExtentsResult FAILURE code=0x%x", code);
    else
        osi_Log1(afsd_logp, "RDR_ProcessReleaseFileExtentsResult DONE code=0x%x", code);

    return code;
}

DWORD
RDR_ReleaseFailedSetFileExtents( IN cm_user_t *userp,
                                 IN AFSSetFileExtentsCB *SetFileExtentsResultCB,
                                 IN DWORD ResultBufferLength)
{
    afs_uint32  code = 0;
    cm_req_t    req;
    unsigned int extentno;
    cm_fid_t         Fid;
    cm_scache_t *    scp = NULL;
    int              dirty = 0;

    RDR_InitReq(&req, FALSE);

    osi_Log4(afsd_logp, "RDR_ReleaseFailedSetFileExtents %d.%d.%d.%d",
              SetFileExtentsResultCB->FileId.Cell, SetFileExtentsResultCB->FileId.Volume,
              SetFileExtentsResultCB->FileId.Vnode, SetFileExtentsResultCB->FileId.Unique);

    /* Process the release */
    Fid.cell = SetFileExtentsResultCB->FileId.Cell;
    Fid.volume = SetFileExtentsResultCB->FileId.Volume;
    Fid.vnode = SetFileExtentsResultCB->FileId.Vnode;
    Fid.unique = SetFileExtentsResultCB->FileId.Unique;
    Fid.hash = SetFileExtentsResultCB->FileId.Hash;

    if (Fid.cell == 0) {
        osi_Log4(afsd_logp, "RDR_ReleaseFailedSetFile Invalid FID %d.%d.%d.%d",
                  Fid.cell, Fid.volume, Fid.vnode, Fid.unique);
        code = CM_ERROR_INVAL;
        goto cleanup_file;
    }

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_ReleaseFailedSetFileExtents cm_GetSCache FID failure code=0x%x",
                  code);
        /* Failure to find the cm_scache object cannot block return of the extents */
    }

    for ( extentno = 0; extentno < SetFileExtentsResultCB->ExtentCount; extentno++ ) {
        osi_hyper_t thyper;
        cm_buf_t    *bufp;
        AFSFileExtentCB *pExtent = &SetFileExtentsResultCB->FileExtents[extentno];

        thyper.QuadPart = pExtent->FileOffset.QuadPart;

        bufp = buf_Find(&Fid, &thyper);
        if (bufp) {
            osi_Log5(afsd_logp, "RDR_ReleaseFailedSetFileExtents bufp 0x%p foffset 0x%x:%x coffset 0x%x:%x",
                      bufp, pExtent->FileOffset.HighPart, pExtent->FileOffset.LowPart,
                      pExtent->CacheOffset.HighPart, pExtent->CacheOffset.LowPart);

            lock_ObtainMutex(&bufp->mx);
            if (bufp->qFlags & CM_BUF_QREDIR) {
                lock_ObtainWrite(&buf_globalLock);
                if (bufp->qFlags & CM_BUF_QREDIR) {
                    buf_RemoveFromRedirQueue(scp, bufp);
                    buf_ReleaseLocked(bufp, TRUE);
                }
                lock_ReleaseWrite(&buf_globalLock);
            }
            lock_ReleaseMutex(&bufp->mx);
            buf_Release(bufp);
        }
    }

  cleanup_file:
    if (userp)
        cm_ReleaseUser(userp);
    if (scp)
        cm_ReleaseSCache(scp);

    osi_Log1(afsd_logp, "RDR_ReleaseFailedSetFileExtents DONE code=0x%x", code);
    return code;
}

void
RDR_PioctlOpen( IN cm_user_t *userp,
                IN AFSFileID  ParentId,
                IN AFSPIOCtlOpenCloseRequestCB *pPioctlCB,
                IN BOOL bWow64,
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB)
{
    cm_fid_t    ParentFid;
    cm_fid_t    RootFid;
    cm_req_t    req;

    cm_InitReq(&req);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Get the active directory */
    ParentFid.cell = ParentId.Cell;
    ParentFid.volume = ParentId.Volume;
    ParentFid.vnode = ParentId.Vnode;
    ParentFid.unique = ParentId.Unique;
    ParentFid.hash = ParentId.Hash;

    /* Get the root directory */
    RootFid.cell = pPioctlCB->RootId.Cell;
    RootFid.volume = pPioctlCB->RootId.Volume;
    RootFid.vnode = pPioctlCB->RootId.Vnode;
    RootFid.unique = pPioctlCB->RootId.Unique;
    RootFid.hash = pPioctlCB->RootId.Hash;

    /* Create the pioctl index */
    RDR_SetupIoctl(pPioctlCB->RequestId, &ParentFid, &RootFid, userp, &req);

    return;
}


void
RDR_PioctlClose( IN cm_user_t *userp,
                 IN AFSFileID  ParentId,
                 IN AFSPIOCtlOpenCloseRequestCB *pPioctlCB,
                 IN BOOL bWow64,
                 IN DWORD ResultBufferLength,
                 IN OUT AFSCommResult **ResultCB)
{
    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Cleanup the pioctl index */
    RDR_CleanupIoctl(pPioctlCB->RequestId);

    return;
}


void
RDR_PioctlWrite( IN cm_user_t *userp,
                 IN AFSFileID  ParentId,
                 IN AFSPIOCtlIORequestCB *pPioctlCB,
                 IN BOOL bWow64,
                 IN DWORD ResultBufferLength,
                 IN OUT AFSCommResult **ResultCB)
{
    AFSPIOCtlIOResultCB *pResultCB;
    cm_scache_t *dscp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    cm_InitReq(&req);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));

    pResultCB = (AFSPIOCtlIOResultCB *)(*ResultCB)->ResultData;

    code = RDR_IoctlWrite(userp, pPioctlCB->RequestId, pPioctlCB->BufferLength, pPioctlCB->MappedBuffer);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    pResultCB->BytesProcessed = pPioctlCB->BufferLength;
    (*ResultCB)->ResultBufferLength = sizeof( AFSPIOCtlIOResultCB);
}

void
RDR_PioctlRead( IN cm_user_t *userp,
                IN AFSFileID  ParentId,
                IN AFSPIOCtlIORequestCB *pPioctlCB,
                IN BOOL bWow64,
                IN BOOL bIsLocalSystem,
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB)
{
    AFSPIOCtlIOResultCB *pResultCB;
    cm_scache_t *dscp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;
    afs_uint32  pflags = (bIsLocalSystem ? AFSCALL_FLAG_LOCAL_SYSTEM : 0);

    cm_InitReq(&req);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));

    pResultCB = (AFSPIOCtlIOResultCB *)(*ResultCB)->ResultData;

    code = RDR_IoctlRead(userp, pPioctlCB->RequestId, pPioctlCB->BufferLength, pPioctlCB->MappedBuffer,
                         &pResultCB->BytesProcessed, pflags);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    (*ResultCB)->ResultBufferLength = sizeof( AFSPIOCtlIOResultCB);
}

void
RDR_ByteRangeLockSync( IN cm_user_t     *userp,
                       IN AFSFileID     FileId,
                       IN AFSByteRangeLockRequestCB *pBRLRequestCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB)
{
    AFSByteRangeLockResultCB *pResultCB = NULL;
    LARGE_INTEGER ProcessId;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    cm_key_t    key;
    DWORD       i;
    DWORD       status;

    ProcessId.QuadPart = pBRLRequestCB->ProcessId;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_ByteRangeLockSync File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);
    osi_Log2(afsd_logp, "... ProcessId 0x%x:%x",
             ProcessId.HighPart, ProcessId.LowPart);

    Length = sizeof( AFSByteRangeLockResultCB) + ((pBRLRequestCB->Count - 1) * sizeof(AFSByteRangeLockResult));
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult));
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }

    *ResultCB = (AFSCommResult *)malloc( Length + sizeof( AFSCommResult) );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length + sizeof( AFSCommResult) );
    (*ResultCB)->ResultBufferLength = Length;

    pResultCB = (AFSByteRangeLockResultCB *)(*ResultCB)->ResultData;
    pResultCB->FileId = FileId;
    pResultCB->Count = pBRLRequestCB->Count;

    /* Allocate the extents from the buffer package */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_ByteRangeLockSync cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
		     CM_SCACHESYNC_LOCK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_ByteRangeLockSync cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* the scp is now locked and current */
    key = cm_GenerateKey(CM_SESSION_IFS, ProcessId.QuadPart, 0);

    for ( i=0; i<pBRLRequestCB->Count; i++ ) {
        pResultCB->Result[i].LockType = pBRLRequestCB->Request[i].LockType;
        pResultCB->Result[i].Offset = pBRLRequestCB->Request[i].Offset;
        pResultCB->Result[i].Length = pBRLRequestCB->Request[i].Length;

        code = cm_Lock(scp,
                       pBRLRequestCB->Request[i].LockType == AFS_BYTE_RANGE_LOCK_TYPE_SHARED,
                       pBRLRequestCB->Request[i].Offset,
                       pBRLRequestCB->Request[i].Length,
                       key, 0, userp, &req, NULL);

        if (code) {
            osi_Log4(afsd_logp, "RDR_ByteRangeLockSync FAILURE code 0x%x type 0x%u offset 0x%x:%x",
                     code,
                     pBRLRequestCB->Request[i].LockType,
                     pBRLRequestCB->Request[i].Offset.HighPart,
                     pBRLRequestCB->Request[i].Offset.LowPart);
            osi_Log2(afsd_logp, "... length 0x%x:%x",
                     pBRLRequestCB->Request[i].Length.HighPart,
                     pBRLRequestCB->Request[i].Length.LowPart);
        }

        switch (code) {
        case 0:
            pResultCB->Result[i].Status = 0;
            break;
        case CM_ERROR_WOULDBLOCK:
            pResultCB->Result[i].Status = STATUS_FILE_LOCK_CONFLICT;
            break;
        default:
            pResultCB->Result[i].Status = STATUS_LOCK_NOT_GRANTED;
        }
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    (*ResultCB)->ResultStatus = 0;
    osi_Log0(afsd_logp, "RDR_ByteRangeLockSync SUCCESS");
    return;
}

void
RDR_ByteRangeUnlock( IN cm_user_t     *userp,
                     IN AFSFileID     FileId,
                     IN AFSByteRangeUnlockRequestCB *pBRURequestCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{
    AFSByteRangeUnlockResultCB *pResultCB = NULL;
    LARGE_INTEGER ProcessId;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    cm_key_t    key;
    DWORD       i;
    DWORD       status;

    ProcessId.QuadPart = pBRURequestCB->ProcessId;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_ByteRangeUnlock File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);
    osi_Log2(afsd_logp, "... ProcessId 0x%x:%x",
             ProcessId.HighPart, ProcessId.LowPart);

    Length = sizeof( AFSByteRangeUnlockResultCB) + ((pBRURequestCB->Count - 1) * sizeof(AFSByteRangeLockResult));
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult));
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }

    *ResultCB = (AFSCommResult *)malloc( Length + sizeof( AFSCommResult) );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length + sizeof( AFSCommResult) );
    (*ResultCB)->ResultBufferLength = Length;

    pResultCB = (AFSByteRangeUnlockResultCB *)(*ResultCB)->ResultData;
    pResultCB->Count = pBRURequestCB->Count;

    /* Allocate the extents from the buffer package */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_ByteRangeUnlock cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
		     CM_SCACHESYNC_LOCK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_ByteRangeUnlock cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* the scp is now locked and current */
    key = cm_GenerateKey(CM_SESSION_IFS, ProcessId.QuadPart, 0);

    for ( i=0; i<pBRURequestCB->Count; i++ ) {
        pResultCB->Result[i].LockType = pBRURequestCB->Request[i].LockType;
        pResultCB->Result[i].Offset = pBRURequestCB->Request[i].Offset;
        pResultCB->Result[i].Length = pBRURequestCB->Request[i].Length;

        code = cm_Unlock(scp,
                         pBRURequestCB->Request[i].LockType == AFS_BYTE_RANGE_LOCK_TYPE_SHARED,
                         pBRURequestCB->Request[i].Offset,
                         pBRURequestCB->Request[i].Length,
                         key, CM_UNLOCK_FLAG_MATCH_RANGE, userp, &req);

        if (code) {
            osi_Log4(afsd_logp, "RDR_ByteRangeUnlock FAILURE code 0x%x type 0x%u offset 0x%x:%x",
                     code, pBRURequestCB->Request[i].LockType,
                     pBRURequestCB->Request[i].Offset.HighPart,
                     pBRURequestCB->Request[i].Offset.LowPart);
            osi_Log2(afsd_logp, "... length 0x%x:%x",
                     pBRURequestCB->Request[i].Length.HighPart,
                     pBRURequestCB->Request[i].Length.LowPart);
        }
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        pResultCB->Result[i].Status = status;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    (*ResultCB)->ResultStatus = 0;
    osi_Log0(afsd_logp, "RDR_ByteRangeUnlock SUCCESS");
    return;
}

void
RDR_ByteRangeUnlockAll( IN cm_user_t     *userp,
                        IN AFSFileID     FileId,
                        IN AFSByteRangeUnlockRequestCB *pBRURequestCB,
                        IN BOOL bWow64,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    AFSByteRangeUnlockResultCB *pResultCB = NULL;
    LARGE_INTEGER ProcessId;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    cm_key_t    key;
    DWORD       status;

    ProcessId.QuadPart = pBRURequestCB->ProcessId;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_ByteRangeUnlockAll File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume,
              FileId.Vnode, FileId.Unique);
    osi_Log2(afsd_logp, "... ProcessId 0x%x:%x",
             ProcessId.HighPart, ProcessId.LowPart);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', sizeof( AFSCommResult));
    (*ResultCB)->ResultBufferLength = 0;

    /* Allocate the extents from the buffer package */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_ByteRangeUnlockAll cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
		     CM_SCACHESYNC_LOCK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_ByteRangeUnlockAll cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* the scp is now locked and current */
    key = cm_GenerateKey(CM_SESSION_IFS, ProcessId.QuadPart, 0);

    code = cm_UnlockByKey(scp, key, 0, userp, &req);

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
    (*ResultCB)->ResultStatus = status;

    if (code)
        osi_Log1(afsd_logp, "RDR_ByteRangeUnlockAll FAILURE code 0x%x", code);
    else
        osi_Log0(afsd_logp, "RDR_ByteRangeUnlockAll SUCCESS");
    return;

}

void
RDR_GetVolumeInfo( IN cm_user_t     *userp,
                   IN AFSFileID     FileId,
                   IN BOOL bWow64,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB)
{
    AFSVolumeInfoCB *pResultCB = NULL;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_volume_t *volp = NULL;
    afs_uint32   volType;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;
    FILETIME ft = {0x832cf000, 0x01abfcc4}; /* October 1, 1982 00:00:00 +0600 */

    char volName[32]="(unknown)";
    char offLineMsg[256]="server temporarily inaccessible";
    char motd[256]="server temporarily inaccessible";
    cm_conn_t *connp;
    AFSFetchVolumeStatus volStat;
    char *Name;
    char *OfflineMsg;
    char *MOTD;
    struct rx_connection * rxconnp;
    int scp_locked = 0;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_GetVolumeInfo File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
             FileId.Cell, FileId.Volume,
             FileId.Vnode, FileId.Unique);

    Length = sizeof( AFSCommResult) + sizeof(AFSVolumeInfoCB);
    if (sizeof(AFSVolumeInfoCB) > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult) );
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }

    *ResultCB = (AFSCommResult *)malloc( Length );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );
    (*ResultCB)->ResultBufferLength = sizeof(AFSVolumeInfoCB);
    pResultCB = (AFSVolumeInfoCB *)(*ResultCB)->ResultData;

    if (FileId.Cell != 0) {
        cm_SetFid(&Fid, FileId.Cell, FileId.Volume, 1, 1);
        code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            (*ResultCB)->ResultBufferLength = 0;
            osi_Log2(afsd_logp, "RDR_GetVolumeInfo cm_GetSCache FID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        osi_Log0(afsd_logp, "RDR_GetVolumeInfo Object Name Invalid - Cell = 0");
        return;
    }
    lock_ObtainWrite(&scp->rw);
    scp_locked = 1;

    pResultCB->SectorsPerAllocationUnit = 1;
    pResultCB->BytesPerSector = 1024;

    pResultCB->CellID = scp->fid.cell;
    pResultCB->VolumeID = scp->fid.volume;
    pResultCB->Characteristics = FILE_REMOTE_DEVICE;
    pResultCB->FileSystemAttributes = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES |
        FILE_UNICODE_ON_DISK | FILE_SUPPORTS_HARD_LINKS | FILE_SUPPORTS_REPARSE_POINTS;

    if (scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
         scp->fid.volume==AFS_FAKE_ROOT_VOL_ID)
    {
        pResultCB->TotalAllocationUnits.QuadPart = 100;
        memcpy(&pResultCB->VolumeCreationTime, &ft, sizeof(ft));

        pResultCB->AvailableAllocationUnits.QuadPart = 0;
        if (cm_volumeInfoReadOnlyFlag)
            pResultCB->FileSystemAttributes |= FILE_READ_ONLY_VOLUME;

        pResultCB->VolumeLabelLength = cm_Utf8ToUtf16( "Freelance.Local.Root", -1, pResultCB->VolumeLabel,
                                                       (sizeof(pResultCB->VolumeLabel) / sizeof(WCHAR)) + 1);
        if ( pResultCB->VolumeLabelLength )
            pResultCB->VolumeLabelLength--;

        pResultCB->CellLength = cm_Utf8ToUtf16( "Freelance.Local", -1, pResultCB->Cell,
                                                (sizeof(pResultCB->Cell) / sizeof(WCHAR)) + 1);
        if ( pResultCB->CellLength )
            pResultCB->CellLength--;
    } else {
	volp = cm_FindVolumeByFID(&scp->fid, userp, &req);
	if (!volp) {
            code = CM_ERROR_NOSUCHVOLUME;
            goto _done;
        }

        volType = cm_VolumeType(volp, scp->fid.volume);

        if (cm_volumeInfoReadOnlyFlag && (volType == ROVOL || volType == BACKVOL))
            pResultCB->FileSystemAttributes |= FILE_READ_ONLY_VOLUME;

        code = -1;

        if ( volType == ROVOL &&
             (volp->flags & CM_VOLUMEFLAG_RO_SIZE_VALID))
        {
            lock_ObtainRead(&volp->rw);
            if (volp->flags & CM_VOLUMEFLAG_RO_SIZE_VALID) {
                volStat.BlocksInUse = volp->volumeSizeRO / 1024;
                code = 0;
            }
            lock_ReleaseRead(&volp->rw);
        }
        
        if (code == -1)
        {
	    Name = volName;
	    OfflineMsg = offLineMsg;
	    MOTD = motd;
	    lock_ReleaseWrite(&scp->rw);
	    scp_locked = 0;

	    do {
		code = cm_ConnFromFID(&scp->fid, userp, &req, &connp);
		if (code) continue;

		rxconnp = cm_GetRxConn(connp);
		code = RXAFS_GetVolumeStatus(rxconnp, scp->fid.volume,
					     &volStat, &Name, &OfflineMsg, &MOTD);
		rx_PutConnection(rxconnp);

	    } while (cm_Analyze(connp, userp, &req, &scp->fid, NULL, 0, NULL, NULL, NULL, NULL, code));
	    code = cm_MapRPCError(code, &req);

	    if (code == 0 && volType == ROVOL)
	    {
		lock_ObtainWrite(&volp->rw);
		volp->volumeSizeRO = volStat.BlocksInUse * 1024;
		_InterlockedOr(&volp->flags, CM_VOLUMEFLAG_RO_SIZE_VALID);
		lock_ReleaseWrite(&volp->rw);
            }
        }

        if ( scp->volumeCreationDate )
            cm_LargeSearchTimeFromUnixTime(&ft, scp->volumeCreationDate);
        memcpy(&pResultCB->VolumeCreationTime, &ft, sizeof(ft));

        if (code == 0) {
            if (volType == ROVOL || volType == BACKVOL) {
                pResultCB->TotalAllocationUnits.QuadPart = volStat.BlocksInUse;
                pResultCB->AvailableAllocationUnits.QuadPart = 0;
            } else {
                if (volStat.MaxQuota)
                {
                    pResultCB->TotalAllocationUnits.QuadPart = volStat.MaxQuota;
                    pResultCB->AvailableAllocationUnits.QuadPart =
                        min(volStat.MaxQuota - volStat.BlocksInUse, volStat.PartBlocksAvail);
                }
                else
                {
                    pResultCB->TotalAllocationUnits.QuadPart = volStat.PartMaxBlocks;
                    pResultCB->AvailableAllocationUnits.QuadPart = volStat.PartBlocksAvail;
                }
            }
	} else if ( code != CM_ERROR_ALLBUSY &&
		    code != CM_ERROR_ALLOFFLINE &&
		    code != CM_ERROR_ALLDOWN)
	{
            /*
             * Lie about the available space.  Out of quota errors will need
             * detected when the file server rejects the store data.
             */
            pResultCB->TotalAllocationUnits.QuadPart = 0x7FFFFFFF;
            pResultCB->AvailableAllocationUnits.QuadPart = (volType == ROVOL || volType == BACKVOL) ? 0 : 0x3F000000;
            code = 0;
        }

        pResultCB->VolumeLabelLength = cm_Utf8ToUtf16( volp->namep, -1, pResultCB->VolumeLabel,
                                                       (sizeof(pResultCB->VolumeLabel) / sizeof(WCHAR)) + 1);

        if ( pResultCB->VolumeLabelLength) {

            /* add .readonly and .backup if appropriate */
            switch ( volType) {
            case ROVOL:
                pResultCB->VolumeLabelLength--;
                pResultCB->VolumeLabelLength += cm_Utf8ToUtf16( ".readonly", -1,
                                                                &pResultCB->VolumeLabel[ pResultCB->VolumeLabelLength],
                                                                (sizeof(pResultCB->VolumeLabel) / sizeof(WCHAR)) - pResultCB->VolumeLabelLength + 1);
                break;

            case BACKVOL:
                pResultCB->VolumeLabelLength--;
                pResultCB->VolumeLabelLength += cm_Utf8ToUtf16( ".backup", -1,
                                                                &pResultCB->VolumeLabel[ pResultCB->VolumeLabelLength],
                                                                (sizeof(pResultCB->VolumeLabel) / sizeof(WCHAR)) - pResultCB->VolumeLabelLength + 1);
                break;
            }
        }

        /* do not include the trailing nul */
        if ( pResultCB->VolumeLabelLength )
            pResultCB->VolumeLabelLength--;

        pResultCB->CellLength = cm_Utf8ToUtf16( volp->cellp->name, -1, pResultCB->Cell,
                                                (sizeof(pResultCB->Cell) / sizeof(WCHAR)) + 1);

        /* do not include the trailing nul */
        if ( pResultCB->CellLength )
            pResultCB->CellLength--;
    }
    pResultCB->VolumeLabelLength *= sizeof(WCHAR);  /* convert to bytes from chars */
    pResultCB->CellLength *= sizeof(WCHAR);         /* convert to bytes from chars */

  _done:
    if (scp_locked)
        lock_ReleaseWrite(&scp->rw);
    if (volp)
       cm_PutVolume(volp);
    cm_ReleaseSCache(scp);

    smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
    (*ResultCB)->ResultStatus = status;
    osi_Log0(afsd_logp, "RDR_GetVolumeInfo SUCCESS");
    return;
}

void
RDR_GetVolumeSizeInfo( IN cm_user_t     *userp,
                   IN AFSFileID     FileId,
                   IN BOOL bWow64,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB)
{
    AFSVolumeSizeInfoCB *pResultCB = NULL;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_volume_t *volp = NULL;
    afs_uint32   volType;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    char volName[32]="(unknown)";
    char offLineMsg[256]="server temporarily inaccessible";
    char motd[256]="server temporarily inaccessible";
    cm_conn_t *connp;
    AFSFetchVolumeStatus volStat;
    char *Name;
    char *OfflineMsg;
    char *MOTD;
    struct rx_connection * rxconnp;
    int scp_locked = 0;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_GetVolumeSizeInfo File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
             FileId.Cell, FileId.Volume,
             FileId.Vnode, FileId.Unique);

    Length = sizeof( AFSCommResult) + sizeof(AFSVolumeSizeInfoCB);
    if (sizeof(AFSVolumeSizeInfoCB) > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult) );
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }

    *ResultCB = (AFSCommResult *)malloc( Length );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );
    (*ResultCB)->ResultBufferLength = sizeof(AFSVolumeSizeInfoCB);
    pResultCB = (AFSVolumeSizeInfoCB *)(*ResultCB)->ResultData;

    if (FileId.Cell != 0) {
        cm_SetFid(&Fid, FileId.Cell, FileId.Volume, 1, 1);
        code = cm_GetSCache(&Fid, NULL, &scp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            (*ResultCB)->ResultBufferLength = 0;
            osi_Log2(afsd_logp, "RDR_GetVolumeSizeInfo cm_GetSCache FID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        osi_Log0(afsd_logp, "RDR_GetVolumeSizeInfo Object Name Invalid - Cell = 0");
        return;
    }
    lock_ObtainWrite(&scp->rw);
    scp_locked = 1;

    pResultCB->SectorsPerAllocationUnit = 1;
    pResultCB->BytesPerSector = 1024;

    if (scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
        scp->fid.volume==AFS_FAKE_ROOT_VOL_ID)
    {
        pResultCB->TotalAllocationUnits.QuadPart = 100;
        pResultCB->AvailableAllocationUnits.QuadPart = 0;
    } else {
	volp = cm_FindVolumeByFID(&scp->fid, userp, &req);
	if (!volp) {
            code = CM_ERROR_NOSUCHVOLUME;
            goto _done;
        }

        volType = cm_VolumeType(volp, scp->fid.volume);

        code = -1;

        if ( volType == ROVOL &&
             (volp->flags & CM_VOLUMEFLAG_RO_SIZE_VALID))
        {
            lock_ObtainRead(&volp->rw);
            if (volp->flags & CM_VOLUMEFLAG_RO_SIZE_VALID) {
                volStat.BlocksInUse = volp->volumeSizeRO / 1024;
                code = 0;
            }
            lock_ReleaseRead(&volp->rw);
        }
        
        if (code == -1)
        {
	    Name = volName;
	    OfflineMsg = offLineMsg;
	    MOTD = motd;
	    lock_ReleaseWrite(&scp->rw);
	    scp_locked = 0;

	    do {
		code = cm_ConnFromFID(&scp->fid, userp, &req, &connp);
		if (code) continue;

		rxconnp = cm_GetRxConn(connp);
		code = RXAFS_GetVolumeStatus(rxconnp, scp->fid.volume,
					     &volStat, &Name, &OfflineMsg, &MOTD);
		rx_PutConnection(rxconnp);

	    } while (cm_Analyze(connp, userp, &req, &scp->fid, NULL, 0, NULL, NULL, NULL, NULL, code));
	    code = cm_MapRPCError(code, &req);

	    if (code == 0 && volType == ROVOL)
	    {
		lock_ObtainWrite(&volp->rw);
		volp->volumeSizeRO = volStat.BlocksInUse * 1024;
		_InterlockedOr(&volp->flags, CM_VOLUMEFLAG_RO_SIZE_VALID);
		lock_ReleaseWrite(&volp->rw);
            }
        }

        if (code == 0) {
            if (volType == ROVOL || volType == BACKVOL) {
                pResultCB->TotalAllocationUnits.QuadPart = volStat.BlocksInUse;
                pResultCB->AvailableAllocationUnits.QuadPart = 0;
            } else {
                if (volStat.MaxQuota)
                {
                    pResultCB->TotalAllocationUnits.QuadPart = volStat.MaxQuota;
                    pResultCB->AvailableAllocationUnits.QuadPart =
                        min(volStat.MaxQuota - volStat.BlocksInUse, volStat.PartBlocksAvail);
                }
                else
                {
                    pResultCB->TotalAllocationUnits.QuadPart = volStat.PartMaxBlocks;
                    pResultCB->AvailableAllocationUnits.QuadPart = volStat.PartBlocksAvail;
                }
            }
        } else {
            /*
             * Lie about the available space.  Out of quota errors will need
             * detected when the file server rejects the store data.
             */
            pResultCB->TotalAllocationUnits.QuadPart = 0x7FFFFFFF;
            pResultCB->AvailableAllocationUnits.QuadPart = (volType == ROVOL || volType == BACKVOL) ? 0 : 0x3F000000;
            code = 0;
        }
    }

  _done:
    if (scp_locked)
        lock_ReleaseWrite(&scp->rw);
    if (volp)
       cm_PutVolume(volp);
    cm_ReleaseSCache(scp);

    smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
    (*ResultCB)->ResultStatus = status;
    osi_Log0(afsd_logp, "RDR_GetVolumeSizeInfo SUCCESS");
    return;
}

void
RDR_HoldFid( IN cm_user_t     *userp,
             IN AFSHoldFidRequestCB * pHoldFidCB,
             IN BOOL bFast,
             IN DWORD ResultBufferLength,
             IN OUT AFSCommResult **ResultCB)
{
    AFSHoldFidResultCB *pResultCB = NULL;
    DWORD       index;
    DWORD       Length;
    cm_req_t    req;

    RDR_InitReq(&req, FALSE);

    osi_Log1(afsd_logp, "RDR_HoldFid Count=%u", pHoldFidCB->Count);

    Length = sizeof(AFSHoldFidResultCB) + (pHoldFidCB->Count-1) * sizeof(AFSFidResult);
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult) );
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }
    *ResultCB = (AFSCommResult *)malloc( Length + sizeof( AFSCommResult) );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );
    (*ResultCB)->ResultBufferLength = Length;
    pResultCB = (AFSHoldFidResultCB *)(*ResultCB)->ResultData;

    for ( index = 0; index < pHoldFidCB->Count; index++ )
    {
        cm_scache_t *scp = NULL;
        cm_fid_t    Fid;

        Fid.cell   = pResultCB->Result[index].FileID.Cell   = pHoldFidCB->FileID[index].Cell;
        Fid.volume = pResultCB->Result[index].FileID.Volume = pHoldFidCB->FileID[index].Volume;
        Fid.vnode  = pResultCB->Result[index].FileID.Vnode  = pHoldFidCB->FileID[index].Vnode;
        Fid.unique = pResultCB->Result[index].FileID.Unique = pHoldFidCB->FileID[index].Unique;
        Fid.hash   = pResultCB->Result[index].FileID.Hash   = pHoldFidCB->FileID[index].Hash;

        osi_Log4( afsd_logp,
                  "RDR_HoldFid File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
                  Fid.cell, Fid.volume, Fid.vnode, Fid.unique);

        scp = cm_FindSCache(&Fid);
        if (scp) {
            RDR_FlagScpInUse( scp, FALSE );
            cm_ReleaseSCache(scp);
        }
        pResultCB->Result[index].Status = 0;
    }

    (*ResultCB)->ResultStatus = 0;
    osi_Log0(afsd_logp, "RDR_HoldFid SUCCESS");
    return;
}

void
RDR_ReleaseFid( IN cm_user_t     *userp,
                IN AFSReleaseFidRequestCB * pReleaseFidCB,
                IN BOOL bFast,
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB)
{
    AFSReleaseFidResultCB *pResultCB = NULL;
    DWORD       index;
    DWORD       Length;
    cm_req_t    req;

    RDR_InitReq(&req, FALSE);

    osi_Log1(afsd_logp, "RDR_ReleaseFid Count=%u", pReleaseFidCB->Count);

    Length = sizeof(AFSReleaseFidResultCB) + (pReleaseFidCB->Count ? pReleaseFidCB->Count-1 : 0) * sizeof(AFSFidResult);
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult) );
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }
    *ResultCB = (AFSCommResult *)malloc( Length + sizeof( AFSCommResult) );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );
    (*ResultCB)->ResultBufferLength = Length;
    pResultCB = (AFSReleaseFidResultCB *)(*ResultCB)->ResultData;

    for ( index = 0; index < pReleaseFidCB->Count; index++ )
    {
        cm_scache_t *scp = NULL;
        cm_fid_t    Fid;

        Fid.cell   = pResultCB->Result[index].FileID.Cell   = pReleaseFidCB->FileID[index].Cell;
        Fid.volume = pResultCB->Result[index].FileID.Volume = pReleaseFidCB->FileID[index].Volume;
        Fid.vnode  = pResultCB->Result[index].FileID.Vnode  = pReleaseFidCB->FileID[index].Vnode;
        Fid.unique = pResultCB->Result[index].FileID.Unique = pReleaseFidCB->FileID[index].Unique;
        Fid.hash   = pResultCB->Result[index].FileID.Hash   = pReleaseFidCB->FileID[index].Hash;

        osi_Log4( afsd_logp,
                  "RDR_ReleaseFid File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
                  Fid.cell, Fid.volume, Fid.vnode, Fid.unique);

        scp = cm_FindSCache(&Fid);
        if (scp) {
            lock_ObtainWrite(&scp->rw);
            scp->flags &= ~CM_SCACHEFLAG_RDR_IN_USE;
            lock_ReleaseWrite(&scp->rw);

            cm_ReleaseSCache(scp);
        }
        pResultCB->Result[index].Status = 0;
    }
    pResultCB->Count = pReleaseFidCB->Count;

    (*ResultCB)->ResultStatus = 0;
    osi_Log0(afsd_logp, "RDR_ReleaseFid SUCCESS");
    return;
}

/*
 * The redirector makes several assumptions regarding the
 * SRVSVC and WKSSVC pipes transactions.  First, the interface
 * versions are those indicated below.  Secondly, the encoding
 * will be performed using NDR version 2.  These assumptions
 * may not hold in the future and end-to-end MSRPC Bind
 * negotiations may need to be supported.  Of course, these
 * are the only interface versions that are supported by the
 * service.
 */
#define MSRPC_PIPE_PREFIX L".\\"

static const UUID MSRPC_SRVSVC_UUID = {0x4B324FC8, 0x1670, 0x01D3,
                                       {0x12, 0x78, 0x5A, 0x47, 0xBF, 0x6E, 0xE1, 0x88}};
#define MSRPC_SRVSVC_NAME L"PIPE\\SRVSVC"
#define MSRPC_SRVSVC_VERS 3

static const UUID MSRPC_WKSSVC_UUID = {0x6BFFD098, 0xA112, 0x3610,
                                       {0x98, 0x33, 0x46, 0xC3, 0xF8, 0x7E, 0x34, 0x5A}};
#define MSRPC_WKSSVC_NAME L"PIPE\\WKSSVC"
#define MSRPC_WKSSVC_VERS 1

static const UUID MSRPC_NDR_UUID = {0x8A885D04, 0x1CEB, 0x11C9,
                                    {0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60}};
#define MSRPC_NDR_NAME    L"NDR"
#define MSRPC_NDR_VERS    2

extern RPC_IF_HANDLE srvsvc_v3_0_s_ifspec;
extern RPC_IF_HANDLE wkssvc_v1_0_s_ifspec;

void
RDR_PipeOpen( IN cm_user_t *userp,
              IN AFSFileID  ParentId,
              IN WCHAR     *Name,
              IN DWORD      NameLength,
              IN AFSPipeOpenCloseRequestCB *pPipe_CB,
              IN BOOL bWow64,
              IN DWORD ResultBufferLength,
              IN OUT AFSCommResult **ResultCB)
{
    cm_fid_t    ParentFid;
    cm_fid_t    RootFid;

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Get the active directory */
    ParentFid.cell = ParentId.Cell;
    ParentFid.volume = ParentId.Volume;
    ParentFid.vnode = ParentId.Vnode;
    ParentFid.unique = ParentId.Unique;
    ParentFid.hash = ParentId.Hash;

    /* Get the root directory */
    RootFid.cell = pPipe_CB->RootId.Cell;
    RootFid.volume = pPipe_CB->RootId.Volume;
    RootFid.vnode = pPipe_CB->RootId.Vnode;
    RootFid.unique = pPipe_CB->RootId.Unique;
    RootFid.hash = pPipe_CB->RootId.Hash;

    /* Create the pipe index */
    (*ResultCB)->ResultStatus =
      RDR_SetupPipe( pPipe_CB->RequestId, &ParentFid, &RootFid,
                     Name, NameLength, userp);
    return;
}


void
RDR_PipeClose( IN cm_user_t *userp,
               IN AFSFileID  ParentId,
               IN AFSPipeOpenCloseRequestCB *pPipe_CB,
               IN BOOL bWow64,
               IN DWORD ResultBufferLength,
               IN OUT AFSCommResult **ResultCB)
{
    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Cleanup the pipe index */
    RDR_CleanupPipe(pPipe_CB->RequestId);

    return;
}


void
RDR_PipeWrite( IN cm_user_t *userp,
               IN AFSFileID  ParentId,
               IN AFSPipeIORequestCB *pPipe_CB,
               IN BYTE *pPipe_Data,
               IN BOOL bWow64,
               IN DWORD ResultBufferLength,
               IN OUT AFSCommResult **ResultCB)
{
    AFSPipeIOResultCB *pResultCB;
    cm_scache_t *dscp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req, bWow64);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof(AFSPipeIOResultCB));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof(AFSPipeIOResultCB));

    pResultCB = (AFSPipeIOResultCB *)(*ResultCB)->ResultData;

    code = RDR_Pipe_Write( pPipe_CB->RequestId, pPipe_CB->BufferLength, pPipe_Data, &req, userp);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    pResultCB->BytesProcessed = pPipe_CB->BufferLength;
    (*ResultCB)->ResultBufferLength = sizeof( AFSPipeIOResultCB);
}


void
RDR_PipeRead( IN cm_user_t *userp,
              IN AFSFileID  ParentId,
              IN AFSPipeIORequestCB *pPipe_CB,
              IN BOOL bWow64,
              IN DWORD ResultBufferLength,
              IN OUT AFSCommResult **ResultCB)
{
    BYTE *pPipe_Data;
    cm_scache_t *dscp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req, bWow64);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + ResultBufferLength);
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    pPipe_Data = (BYTE *)(*ResultCB)->ResultData;

    code = RDR_Pipe_Read( pPipe_CB->RequestId, ResultBufferLength, pPipe_Data,
                          &(*ResultCB)->ResultBufferLength, &req, userp);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        return;
    }
}


void
RDR_PipeSetInfo( IN cm_user_t *userp,
                 IN AFSFileID  ParentId,
                 IN AFSPipeInfoRequestCB *pPipeInfo_CB,
                 IN BYTE *pPipe_Data,
                 IN BOOL bWow64,
                 IN DWORD ResultBufferLength,
                 IN OUT AFSCommResult **ResultCB)
{
    cm_scache_t *dscp = NULL;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req, bWow64);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    status = RDR_Pipe_SetInfo( pPipeInfo_CB->RequestId, pPipeInfo_CB->InformationClass,
                               pPipeInfo_CB->BufferLength, pPipe_Data, &req, userp);

    (*ResultCB)->ResultStatus = status;
}


void
RDR_PipeQueryInfo( IN cm_user_t *userp,
                   IN AFSFileID  ParentId,
                   IN AFSPipeInfoRequestCB *pPipeInfo_CB,
                   IN BOOL bWow64,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB)
{
    BYTE *pPipe_Data;
    cm_scache_t *dscp = NULL;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req, bWow64);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + ResultBufferLength);
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof(AFSPipeIOResultCB));

    pPipe_Data = (BYTE *)(*ResultCB)->ResultData;

    status = RDR_Pipe_QueryInfo( pPipeInfo_CB->RequestId, pPipeInfo_CB->InformationClass,
                                 ResultBufferLength, pPipe_Data,
                                 &(*ResultCB)->ResultBufferLength, &req, userp);

    (*ResultCB)->ResultStatus = status;
}

void
RDR_PipeTransceive( IN cm_user_t     *userp,
                    IN AFSFileID  ParentId,
                    IN AFSPipeIORequestCB *pPipe_CB,
                    IN BYTE *pPipe_InData,
                    IN BOOL bWow64,
                    IN DWORD ResultBufferLength,
                    IN OUT AFSCommResult **ResultCB)
{
    /*
     * This function processes a Pipe Service request
     * that would normally be sent to a LAN Manager server
     * across an authenticated SMB-PIPE/MSRPC/SVC request
     * stack.  The request is being sent here because the
     * application (e.g., Explorer Shell or Common Control File
     * dialog) believes that because the UNC path it is
     * processing has specified a server name that is not
     * "." and that the Server is remote and that the Share
     * list cannot be obtained using the Network Provider
     * interface.
     *
     * The file system driver is faking the Bind-Ack response
     * to the MSRPC Bind request but cannot decode the NDR
     * encoded Pipe Service requests.  For that we will use
     * the service's MSRPC module.  However, unlike the SMB
     * server usage we must fake the MSRPC Bind exchange and
     * map the PipeName to an interface instead of using the
     * GUID specified in the MSRPC Bind request.
     *
     * None of the requests that are being processed by the
     * service require authentication.  As a result the userp
     * parameter will be ignored.
     *
     * Although there are dozens of Pipe Services, the only
     * ones that we are implementing are WKSSVC and SRVSVC.
     * These support NetShareEnum, NetShareGetInfo,
     * NetServerGetInfo, and NetWorkstaGetInfo which are
     * commonly queried by NET VIEW, the Explorer Shell,
     * and the Common Control File dialog.
     */
    BYTE *pPipe_OutData;
    cm_scache_t *dscp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;
    DWORD Length = ResultBufferLength + sizeof( AFSCommResult);

    RDR_InitReq(&req, bWow64);

    *ResultCB = (AFSCommResult *)malloc( Length);
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );

    code = RDR_Pipe_Write( pPipe_CB->RequestId, pPipe_CB->BufferLength, pPipe_InData, &req, userp);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        osi_Log2( afsd_logp, "RDR_Pipe_Transceive Write FAILURE code=0x%x status=0x%x",
                  code, status);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    pPipe_OutData = (BYTE *)(*ResultCB)->ResultData;
    code = RDR_Pipe_Read( pPipe_CB->RequestId, ResultBufferLength, pPipe_OutData,
                          &(*ResultCB)->ResultBufferLength, &req, userp);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        osi_Log2( afsd_logp, "RDR_Pipe_Transceive Read FAILURE code=0x%x status=0x%x",
                  code, status);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    (*ResultCB)->ResultStatus = 0;
    osi_Log0(afsd_logp, "RDR_Pipe_Transceive SUCCESS");
}

void
RDR_ReadFile( IN cm_user_t     *userp,
              IN AFSFileID      FileID,
              IN LARGE_INTEGER *Offset,
              IN ULONG          BytesToRead,
              IN PVOID          Buffer,
              IN BOOL           bWow64,
              IN BOOL           bCacheBypass,
              IN DWORD          ResultBufferLength,
              IN OUT AFSCommResult **ResultCB)
{
    AFSFileIOResultCB * pFileIOResultCB;
    DWORD         status;
    ULONG         Length;
    ULONG         ulBytesRead = 0;
    afs_uint32    code = 0;
    cm_fid_t      fid;
    cm_scache_t * scp = NULL;
    cm_req_t      req;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_ReadFile FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
             FileID.Cell, FileID.Volume, FileID.Vnode, FileID.Unique);

    Length = sizeof(AFSFileIOResultCB);
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult) );
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }
    *ResultCB = (AFSCommResult *)malloc( Length + sizeof( AFSCommResult) );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );
    (*ResultCB)->ResultBufferLength = Length;
    pFileIOResultCB = (AFSFileIOResultCB *)(*ResultCB)->ResultData;

    if ( Buffer == NULL) {
        (*ResultCB)->ResultStatus = STATUS_INVALID_PARAMETER;
        osi_Log0(afsd_logp, "RDR_ReadFile Null IOctl Buffer");
        return;
    }

    if (FileID.Cell != 0) {
        fid.cell   = FileID.Cell;
        fid.volume = FileID.Volume;
        fid.vnode  = FileID.Vnode;
        fid.unique = FileID.Unique;
        fid.hash   = FileID.Hash;

        code = cm_GetSCache(&fid, NULL, &scp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_ReadFile cm_GetSCache failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        osi_Log0(afsd_logp, "RDR_ReadFile Object Name Invalid - Cell = 0");
        return;
    }

    /* Ensure that the caller can access this file */
    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_READ,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log2(afsd_logp, "RDR_ReadFile cm_SyncOp failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    if (scp->fileType == CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_FILE_IS_A_DIRECTORY;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log1(afsd_logp, "RDR_ReadFile File is a Directory scp=0x%p",
                 scp);
        return;
    }

    if (scp->fileType != CM_SCACHETYPE_FILE) {
        (*ResultCB)->ResultStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log1(afsd_logp, "RDR_ReadFile File is a MountPoint or Link scp=0x%p",
                 scp);
        return;
    }

    if ( bCacheBypass) {
        //
        // Read the file directly into the buffer bypassing the AFS Cache
        //
        code = cm_GetData( scp, Offset, Buffer, BytesToRead, &ulBytesRead, userp, &req);
    } else {
        //
        // Read the file via the AFS Cache
        //
        code = raw_ReadData( scp, Offset, BytesToRead, Buffer, &ulBytesRead, userp, &req);
    }

    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_ReadFile failure code=0x%x status=0x%x",
                 code, status);
    } else {
        (*ResultCB)->ResultStatus = STATUS_SUCCESS;
        pFileIOResultCB->Length = ulBytesRead;
        pFileIOResultCB->DataVersion.QuadPart = scp->dataVersion;
        pFileIOResultCB->Expiration.QuadPart = scp->cbExpires;
    }

    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);
    return;
}

void
RDR_WriteFile( IN cm_user_t     *userp,
               IN AFSFileID      FileID,
               IN AFSFileIOCB   *FileIOCB,
               IN LARGE_INTEGER *Offset,
               IN ULONG          BytesToWrite,
               IN PVOID          Buffer,
               IN BOOL           bWow64,
               IN BOOL           bCacheBypass,
               IN DWORD          ResultBufferLength,
               IN OUT AFSCommResult **ResultCB)
{
    AFSFileIOResultCB * pFileIOResultCB;
    DWORD         status;
    ULONG         Length;
    ULONG         ulBytesWritten = 0;
    afs_uint32    code = 0;
    cm_fid_t      fid;
    cm_scache_t * scp = NULL;
    cm_req_t      req;

    RDR_InitReq(&req, bWow64);

    osi_Log4(afsd_logp, "RDR_WriteFile FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
             FileID.Cell, FileID.Volume, FileID.Vnode, FileID.Unique);

    Length = sizeof(AFSFileIOResultCB);
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult) );
        if (!(*ResultCB))
            return;
        memset( *ResultCB, 0, sizeof(AFSCommResult));
        (*ResultCB)->ResultStatus = STATUS_BUFFER_OVERFLOW;
        return;
    }
    *ResultCB = (AFSCommResult *)malloc( Length + sizeof( AFSCommResult) );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );
    (*ResultCB)->ResultBufferLength = Length;
    pFileIOResultCB = (AFSFileIOResultCB *)(*ResultCB)->ResultData;

    if ( Buffer == NULL) {
        (*ResultCB)->ResultStatus = STATUS_INVALID_PARAMETER;
        osi_Log0(afsd_logp, "RDR_WriteFile Null IOctl Buffer");
        return;
    }

    if (FileID.Cell != 0) {
        fid.cell   = FileID.Cell;
        fid.volume = FileID.Volume;
        fid.vnode  = FileID.Vnode;
        fid.unique = FileID.Unique;
        fid.hash   = FileID.Hash;

        code = cm_GetSCache(&fid, NULL, &scp, userp, &req);
        if (code) {
            smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_WriteFile cm_GetSCache failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        osi_Log0(afsd_logp, "RDR_WriteFile Object Name Invalid - Cell = 0");
        return;
    }

    /* Ensure that the caller can access this file */
    lock_ObtainWrite(&scp->rw);
    /*
     * Request PRSFS_WRITE | PRSFS_LOCK in order to bypass the unix mode
     * check in cm_HaveAccessRights().   By the time RDR_WriteFile is called
     * it is already too late to deny the write due to the readonly attribute.
     * The Windows cache may have already accepted the data.  Only if the
     * user does not have real write permission should the write be denied.
     */
    code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_WRITE | PRSFS_LOCK,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code == CM_ERROR_NOACCESS && scp->creator == userp) {
        code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_INSERT,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    }
    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log2(afsd_logp, "RDR_WriteFile cm_SyncOp failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    if (scp->fileType == CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_FILE_IS_A_DIRECTORY;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log1(afsd_logp, "RDR_WriteFile File is a Directory scp=0x%p",
                 scp);
        return;
    }

    if (scp->fileType != CM_SCACHETYPE_FILE) {
        (*ResultCB)->ResultStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log1(afsd_logp, "RDR_WriteFile File is a MountPoint or Link scp=0x%p",
                 scp);
        return;
    }

    if (FileIOCB->EndOfFile.QuadPart != scp->length.QuadPart)
    {
        cm_attr_t setAttr;

        memset(&setAttr, 0, sizeof(cm_attr_t));
        if (FileIOCB->EndOfFile.QuadPart != scp->length.QuadPart) {
            osi_Log4(afsd_logp, "RDR_WriteFile new length fid vol 0x%x vno 0x%x length 0x%x:%x",
                     scp->fid.volume, scp->fid.vnode,
                     FileIOCB->EndOfFile.HighPart,
                     FileIOCB->EndOfFile.LowPart);

            setAttr.mask |= CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = FileIOCB->EndOfFile.LowPart;
            setAttr.length.HighPart = FileIOCB->EndOfFile.HighPart;
            lock_ReleaseWrite(&scp->rw);
            code = cm_SetAttr(scp, &setAttr, userp, &req);
            osi_Log2(afsd_logp, "RDR_WriteFile cm_SetAttr failure scp=0x%p code 0x%x",
                     scp, code);
            code = 0;       /* ignore failure */
            lock_ObtainWrite(&scp->rw);
        }
    }

    /*
     * The input buffer may contain data beyond the end of the file.
     * Such data must be discarded.
     */
    if ( Offset->QuadPart + BytesToWrite > scp->length.QuadPart)
    {
        if ( Offset->QuadPart > scp->length.QuadPart) {
            (*ResultCB)->ResultStatus = STATUS_SUCCESS;
            lock_ReleaseWrite(&scp->rw);
            cm_ReleaseSCache(scp);
            osi_Log1(afsd_logp, "RDR_WriteFile Nothing to do scp=0x%p",
                     scp);
            return;
        }

        BytesToWrite -= (afs_uint32)(Offset->QuadPart + BytesToWrite - scp->length.QuadPart);
    }

    if (bCacheBypass) {
        code = cm_DirectWrite( scp, Offset, BytesToWrite,
                               CM_DIRECT_SCP_LOCKED,
                               userp, &req, Buffer, &ulBytesWritten);
    } else {
        code = raw_WriteData( scp, Offset, BytesToWrite, Buffer, userp, &req, &ulBytesWritten);
    }

    if (code) {
        smb_MapNTError(cm_MapRPCError(code, &req), &status, TRUE);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_WriteFile failure code=0x%x status=0x%x",
                 code, status);
    } else {
        (*ResultCB)->ResultStatus = STATUS_SUCCESS;
        pFileIOResultCB->Length = ulBytesWritten;
        pFileIOResultCB->DataVersion.QuadPart = scp->dataVersion;
        pFileIOResultCB->Expiration.QuadPart = scp->cbExpires;
    }

    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);
    return;
}
