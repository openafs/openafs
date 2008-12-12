
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <windows.h>
#include <ntsecapi.h>
#include <sddl.h>
#pragma warning(push)
#pragma warning(disable: 4005)
#include <ntstatus.h>

#include <devioctl.h>

#include "..\\Common\\AFSUserCommon.h"

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

#include <RDRPrototypes.h>
#include <RDRIoctl.h>

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif

#define QuadAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 7) & 0xfffffff8) \
    )


void 
RDR_InitReq(cm_req_t *reqp)
{
    cm_InitReq(reqp);
    reqp->flags |= CM_REQ_SOURCE_REDIR;
}

DWORD
RDR_SetInitParams( OUT AFSRedirectorInitInfo **ppRedirInitInfo, OUT DWORD * pRedirInitInfoLen )
{
    extern char cm_CachePath[];
    extern cm_config_data_t cm_data;
    extern int smb_hideDotFiles;
    size_t cm_CachePathLen = strlen(cm_CachePath);
    size_t err;

    *pRedirInitInfoLen = (DWORD) (sizeof(AFSRedirectorInitInfo) + (cm_CachePathLen) * sizeof(WCHAR));
    *ppRedirInitInfo = (AFSRedirectorInitInfo *)malloc(*pRedirInitInfoLen);
    (*ppRedirInitInfo)->Flags = smb_hideDotFiles ? AFS_REDIR_INIT_FLAG_HIDE_DOT_FILES : 0;
    (*ppRedirInitInfo)->MaximumChunkLength = cm_data.chunkSize;
    (*ppRedirInitInfo)->ExtentCount.QuadPart = cm_data.buf_nbuffers;
    (*ppRedirInitInfo)->CacheBlockSize = cm_data.blockSize;
    (*ppRedirInitInfo)->CacheFileNameLength = (ULONG) (cm_CachePathLen * sizeof(WCHAR));
    err = mbstowcs((*ppRedirInitInfo)->CacheFileName, cm_CachePath, (cm_CachePathLen + 1) *sizeof(WCHAR));
    if (err == -1) {
        free(*ppRedirInitInfo);
        osi_Log0(afsd_logp, "RDR_SetInitParams Invalid Object Name");
        return STATUS_OBJECT_NAME_INVALID;
    }

    osi_Log0(afsd_logp,"RDR_SetInitParams Success");
    return 0;
}

cm_user_t *
RDR_UserFromProcessId( IN ULARGE_INTEGER ProcessId)
{
    cm_user_t *userp = NULL;
    HANDLE hProcess = 0, hToken = 0;
    PSID        pSid = 0;
    wchar_t      *secSidString = 0;
    wchar_t cname[MAX_COMPUTERNAME_LENGTH+1];
    int cnamelen = MAX_COMPUTERNAME_LENGTH+1;

    hProcess = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, (ULONG)ProcessId.QuadPart);
    if (hProcess == NULL)
        goto done;

    if (!OpenProcessToken( hProcess, TOKEN_READ, &hToken))
        goto done;

    if (!smb_GetUserSID( hToken, &pSid))
        goto done;

    if (!ConvertSidToStringSidW(pSid, &secSidString))
        goto done;

    GetComputerNameW(cname, &cnamelen);
    _wcsupr(cname);

    userp = smb_FindCMUserByName(secSidString, cname, SMB_FLAG_CREATE);

  done:
    if (!userp) {
        userp = cm_rootUserp;
        cm_HoldUser(userp);
    }

    osi_Log2(afsd_logp, "RDR_UserFromProcessId %S userp = 0x%p", 
             osi_LogSaveStringW(afsd_logp, secSidString), userp);

    if (secSidString)
        LocalFree(secSidString);
    if (pSid)
        smb_FreeSID(pSid);
    if (hToken)
        CloseHandle(hToken);
    if (hProcess)
        CloseHandle(hProcess);

    return userp;
}

cm_user_t *
RDR_UserFromCommRequest( IN AFSCommRequest *RequestBuffer) {
    return RDR_UserFromProcessId(RequestBuffer->ProcessId);
}

void
RDR_ReleaseUser( IN cm_user_t *userp )
{
    osi_Log1(afsd_logp, "RDR_ReleaseUser userp = 0x%p", userp);
    cm_ReleaseUser(userp);
}

#define RDR_POP_FOLLOW_MOUNTPOINTS 0x01
#define RDR_POP_EVALUATE_SYMLINKS  0x02
#define RDR_POP_WOW64              0x04
#define RDR_POP_NO_GETSTATUS       0x08

afs_uint32
RDR_PopulateCurrentEntry( IN  AFSDirEnumEntry * pCurrentEntry,
                          IN  DWORD             dwMaxEntryLength,
                          IN  cm_scache_t     * dscp,
                          IN  cm_scache_t     * scp,
                          IN  cm_user_t       * userp,
                          IN  cm_req_t        * reqp,
                          IN  wchar_t         * name,
                          IN  wchar_t         * shortName,
                          IN  DWORD             dwFlags,
                          OUT AFSDirEnumEntry **ppNextEntry,
                          OUT DWORD           * pdwRemainingLength)
{
    FILETIME ft;
    WCHAR *  wname, *wtarget;
    size_t   len;
    DWORD      dwEntryLength;
    afs_uint32 code = 0, code2 = 0;

    osi_Log4(afsd_logp, "RDR_PopulateCurrentEntry dscp=0x%p scp=0x%p name=%S short=%S", 
             dscp, scp, osi_LogSaveStringW(afsd_logp, name), 
             osi_LogSaveStringW(afsd_logp, shortName));
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
    if (!(dwFlags & RDR_POP_NO_GETSTATUS)) {
        code = cm_SyncOp( scp, NULL, userp, reqp, 0,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            lock_ReleaseWrite(&scp->rw);
            osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_SyncOp failed for scp=0x%p code=0x%x", 
                     scp, code);
            return code;
        }
    }

    pCurrentEntry->FileId.Cell = scp->fid.cell;
    pCurrentEntry->FileId.Volume = scp->fid.volume;
    pCurrentEntry->FileId.Vnode = scp->fid.vnode;
    pCurrentEntry->FileId.Unique = scp->fid.unique;
    pCurrentEntry->FileId.Hash = scp->fid.hash;

    pCurrentEntry->FileType = scp->fileType;

    pCurrentEntry->DataVersion.QuadPart = scp->dataVersion;

    smb_LargeSearchTimeFromUnixTime(&ft, scp->cbExpires);
    pCurrentEntry->Expiration.LowPart = ft.dwLowDateTime;
    pCurrentEntry->Expiration.HighPart = ft.dwHighDateTime;

    smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
    pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
    pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
    pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
    pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
    pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

    pCurrentEntry->EndOfFile = scp->length;
    pCurrentEntry->AllocationSize = scp->length;
    pCurrentEntry->FileAttributes = smb_ExtAttributes(scp);
    if (smb_hideDotFiles && smb_IsDotFile(name))
        pCurrentEntry->FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
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

    osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry scp=0x%p fileType=%d", 
              scp, scp->fileType);

    if (!(dwFlags & RDR_POP_NO_GETSTATUS))
        cm_SyncOpDone( scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    if ((dwFlags & RDR_POP_NO_GETSTATUS) || !cm_HaveCallback(scp)) {
        pCurrentEntry->TargetNameOffset = 0;
        pCurrentEntry->TargetNameLength = 0;
    } 
    else
    switch (scp->fileType) {
    case CM_SCACHETYPE_MOUNTPOINT:
        if ((code = cm_ReadMountPoint(scp, userp, reqp)) == 0) {
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
                code = cm_FollowMountPoint(scp, dscp, userp, reqp, &targetScp);

                if (code == 0) {
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
                              scp, code);
                }
            }
        } else {
            osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_ReadMountPoint failed scp=0x%p code=0x%x", 
                      scp, code);
        }
        break;
    case CM_SCACHETYPE_SYMLINK:
    case CM_SCACHETYPE_DFSLINK:
        {
            char * mp = scp->mountPointStringp;

            pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
            wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

            len = strlen(mp);
            if ( len != 0 ) {
                /* Strip off the msdfs: prefix from the target name for the file system */
                if (scp->fileType == CM_SCACHETYPE_DFSLINK) {
                    if (!strncmp("msdfs:", mp, 6)) {
                        mp += 6;
                        len -= 6;
                    }
                    /* only send one slash to the redirector */
                    if (mp[0] == '\\' && mp[1] == '\\') {
                        mp++;
                        len--;
                    }
                }
#ifdef UNICODE
                cch = MultiByteToWideChar( CP_UTF8, 0, mp, 
                                           len * sizeof(char),
                                           wtarget, 
                                           len * sizeof(WCHAR));
#else
                mbstowcs(wtarget, mp, len);
#endif
            }
            pCurrentEntry->TargetNameLength = (ULONG)(sizeof(WCHAR) * len);

            if (dwFlags & RDR_POP_EVALUATE_SYMLINKS) {
                cm_scache_t *targetScp = NULL;
                cm_scache_t *linkParentScp = NULL;
                cm_fid_t fid;

                cm_SetFid(&fid, scp->fid.cell, scp->fid.volume, scp->parentVnode, scp->parentUnique);

                linkParentScp = cm_FindSCache(&fid);
                if (linkParentScp) {
                    lock_ObtainWrite(&linkParentScp->rw);
                    code = cm_SyncOp( linkParentScp, NULL, userp, reqp, PRSFS_LOOKUP,
                                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
                    if (code == 0) {
                        cm_SyncOpDone( linkParentScp, NULL, 
                                       CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
                        lock_ReleaseWrite(&linkParentScp->rw);

                        lock_ReleaseWrite(&scp->rw);
                        code = cm_EvaluateSymLink(linkParentScp, scp, &targetScp, userp, reqp);
                        lock_ObtainWrite(&scp->rw);
                        if (code == 0) {
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
                            if (scp->fileType == CM_SCACHETYPE_DFSLINK) {
                                osi_Log0(afsd_logp, "RDR_PopulateCurrentEntry DFSLink Detected");
                                pCurrentEntry->FileType = scp->fileType;
                                code = 0;
                            } else {
                                osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_EvaluateSymLink failed scp=0x%p code=0x%x", 
                                         scp, code);
                            }
                        }
                    } else {
                        lock_ReleaseWrite(&linkParentScp->rw);
                        osi_Log2(afsd_logp, "RDR_PopulateCurrentEntry cm_SyncOp link parent failed scp=0x%p code=0x%x", 
                                 linkParentScp, code);
                    }
                    cm_ReleaseSCache(linkParentScp);
                } else {
                    osi_Log4(afsd_logp, "RDR_PopulateCurrentEntry Link Parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x not found",
                              fid.cell, fid.volume, fid.vnode, fid.unique);
                }
            }
        }
        break;
    default:
        pCurrentEntry->TargetNameOffset = 0;
        pCurrentEntry->TargetNameLength = 0;
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

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

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

        code = cm_GetSCache(&fid, &dscp, userp, &req);
        if (code) {
            smb_MapNTError(code, &status);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EnumerateDirectory cm_GetSCache failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        fid = cm_data.rootFid;
        dscp = cm_data.rootSCachep;
        cm_HoldSCache(dscp);
    }

    /* get the directory size */
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, PRSFS_LOOKUP,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        smb_MapNTError(code, &status);
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

    /*
     * If there is no enumeration handle, then this is a new query
     * and we must perform an enumeration for the specified object 
     */
    if (QueryCB->EnumHandle == (ULONG_PTR)NULL) {
        cm_dirOp_t    dirop;

        code = cm_BeginDirOp(dscp, userp, &req, CM_DIRLOCK_READ, &dirop);
        if (code == 0) {
            code = cm_BPlusDirEnumerate(dscp, TRUE, NULL, &enump);
            if (code == 0 && !bSkipStatus) {
                code = cm_BPlusDirEnumBulkStat(dscp, enump, userp, &req);
                if (code) {
                    osi_Log1(afsd_logp, "RDR_EnumerateDirectory cm_BPlusDirEnumBulkStat failure code=0x%x",
                              code);
                }
            } else {
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

    if (enump && ResultBufferLength) {
        cm_direnum_entry_t * entryp = NULL;

      getnextentry:
        if (dwMaxEntryLength < sizeof(AFSDirEnumEntry) + (MAX_PATH + MOUNTPOINTLEN) * sizeof(wchar_t)) {
            osi_Log0(afsd_logp, "RDR_EnumerateDirectory out of space, returning");
            goto outofspace;
        }

        code = cm_BPlusDirNextEnumEntry(enump, &entryp);

        if ((code == 0 || code == CM_ERROR_STOPNOW) && entryp) {
            cm_scache_t *scp;
            int stopnow = (code == CM_ERROR_STOPNOW);

            if ( !wcscmp(L".", entryp->name) || !wcscmp(L"..", entryp->name) ) {
                osi_Log0(afsd_logp, "RDR_EnumerateDirectory skipping . or ..");
                if (stopnow)
                    goto outofspace;
                goto getnextentry;
            }

            code = cm_GetSCache(&entryp->fid, &scp, userp, &req);
            if (!code) {
                code = RDR_PopulateCurrentEntry(pCurrentEntry, dwMaxEntryLength, 
                                                dscp, scp, userp, &req, 
                                                entryp->name, entryp->shortName,
                                                (bWow64 ? RDR_POP_WOW64 : 0) | 
                                                (bSkipStatus ? RDR_POP_NO_GETSTATUS : 0),
                                                &pCurrentEntry, &dwMaxEntryLength);
                cm_ReleaseSCache(scp);
                if (stopnow)
                    goto outofspace;
                goto getnextentry;
            } else {
                osi_Log2(afsd_logp, "RDR_EnumerateDirectory cm_GetSCache failure scp=0x%p code=0x%x",
                         scp, code);
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
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_EnumerateDirectory Failure code=0x%x status=0x%x",
                  code, status);
    }

    if (ResultBufferLength) {
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwMaxEntryLength;

        pDirEnumResp->EnumHandle = (ULONG_PTR) enump;
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
                        IN BOOL     bWow64,
                        IN BOOL     bSkipStatus,
                        IN DWORD    ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    AFSDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
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

    StringCchCopyNW(FileName, 260, FileNameCounted, FileNameLength / sizeof(WCHAR));

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    osi_Log4(afsd_logp, "RDR_EvaluateNodeByName parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
             ParentID.Cell, ParentID.Volume, ParentID.Vnode, ParentID.Unique);

    /* Allocate enough room to add a volume prefix if necessary */
    cbName = FileNameLength + (CM_PREFIX_VOL_CCH + 1) * sizeof(WCHAR);
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
    (*ResultCB)->ResultBufferLength = ResultBufferLength;
    if (ResultBufferLength)
        pCurrentEntry = (AFSDirEnumEntry *)&(*ResultCB)->ResultData;

    if (ParentID.Cell != 0) {
        parentFid.cell   = ParentID.Cell;
        parentFid.volume = ParentID.Volume;
        parentFid.vnode  = ParentID.Vnode;
        parentFid.unique = ParentID.Unique;
        parentFid.hash   = ParentID.Hash;

        code = cm_GetSCache(&parentFid, &dscp, userp, &req);
        if (code) {
            smb_MapNTError(code, &status);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByName cm_GetSCache parentFID failure code=0x%x status=0x%x",
                      code, status);
            free(wszName);
            return;
        }
    } else {
        parentFid = cm_data.rootFid;
        dscp = cm_data.rootSCachep;
        cm_HoldSCache(dscp);
    }

    /* get the directory size */
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
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

    code = cm_Lookup(dscp, wszName, CM_FLAG_CHECKPATH, userp, &req, &scp);

    if ((code == CM_ERROR_NOSUCHPATH || code == CM_ERROR_NOSUCHFILE) &&
         (wcschr(wszName, '%') != NULL || wcschr(wszName, '#') != NULL)) {
        /* 
         * A volume reference:  <cell>{%,#}<volume> -> @vol:<cell>{%,#}<volume>
         */
        StringCchCopyNW(wszName, cbName, _C(CM_PREFIX_VOL), CM_PREFIX_VOL_CCH);
        StringCbCatNW(wszName, cbName, FileName, FileNameLength);
        cm_strlwr_utf16(wszName);
        bVol = TRUE;

        code = cm_EvaluateVolumeReference(wszName, CM_FLAG_CHECKPATH, userp, &req, &scp);
    }

    if (code == 0 && scp) {
        wchar_t shortName[13]=L"";

        if (bVol) {
            cm_Gen8Dot3VolNameW(scp->fid.cell, scp->fid.volume, shortName, NULL);
        } else if (!cm_Is8Dot3(wszName)) {
            cm_dirFid_t dfid;

            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            cm_Gen8Dot3NameIntW(FileName, &dfid, shortName, NULL);
        } else {
            shortName[0] = '\0';
        }

        code = RDR_PopulateCurrentEntry(pCurrentEntry, ResultBufferLength, 
                                        dscp, scp, userp, &req, 
                                        FileName, shortName,
                                        (bWow64 ? RDR_POP_WOW64 : 0) | 
                                        (bSkipStatus ? RDR_POP_NO_GETSTATUS : 0) |
                                        RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                        NULL, &dwRemaining);
        cm_ReleaseSCache(scp);
        if (code) {
            smb_MapNTError(code, &status);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByName FAILURE code=0x%x status=0x%x",
                      code, status);
        } else {
            (*ResultCB)->ResultStatus = STATUS_SUCCESS;
            (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
            osi_Log0(afsd_logp, "RDR_EvaluateNodeByName SUCCESS");
        }
    } else if (code) {
        smb_MapNTError(code, &status);
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
                      IN AFSFileID ParentID, 
                      IN AFSFileID SourceID,
                      IN BOOL      bWow64,
                      IN BOOL      bSkipStatus,
                      IN DWORD     ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB)
{
    AFSDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    afs_uint32  code = 0;
    cm_scache_t * scp = NULL;
    cm_scache_t * dscp = NULL;
    cm_req_t      req;
    cm_fid_t      Fid;
    cm_fid_t      parentFid;
    DWORD         status;
    DWORD         dwRemaining;

    osi_Log4(afsd_logp, "RDR_EvaluateNodeByID parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              ParentID.Cell, ParentID.Volume, ParentID.Vnode, ParentID.Unique);
    osi_Log4(afsd_logp, "... source FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              SourceID.Cell, SourceID.Volume, SourceID.Vnode, SourceID.Unique);

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB)) {
        osi_Log0(afsd_logp, "RDR_EvaluateNodeByID Out of Memory");
	return;
    }

    memset(*ResultCB, 0, size);
    (*ResultCB)->ResultBufferLength = ResultBufferLength;
    dwRemaining = ResultBufferLength;
    if (ResultBufferLength)
        pCurrentEntry = (AFSDirEnumEntry *)&(*ResultCB)->ResultData;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    if (ParentID.Cell != 0) {
        parentFid.cell   = ParentID.Cell;
        parentFid.volume = ParentID.Volume;
        parentFid.vnode  = ParentID.Vnode;
        parentFid.unique = ParentID.Unique;
        parentFid.hash   = ParentID.Hash;

        code = cm_GetSCache(&parentFid, &dscp, userp, &req);
        if (code) {
            smb_MapNTError(code, &status);
            (*ResultCB)->ResultStatus = status;
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByID cm_GetSCache parentFID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        if (SourceID.Cell == 0) {
            (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
            osi_Log0(afsd_logp, "RDR_EvaluateNodeByID Object Name Invalid - Cell = 0");
            return;
        }

        /* If the ParentID.Cell == 0 then we are evaluating the root mount point */
        parentFid = cm_data.rootFid;
        dscp = cm_data.rootSCachep;
        cm_HoldSCache(dscp);
    }

    if (SourceID.Cell != 0) {
        Fid.cell   = SourceID.Cell;
        Fid.volume = SourceID.Volume;
        Fid.vnode  = SourceID.Vnode;
        Fid.unique = SourceID.Unique;
        Fid.hash   = SourceID.Hash;

        if (bSkipStatus) {
            scp = cm_FindSCache(&Fid);
            if (!scp)
                code = cm_GetSCache(&Fid, &scp, userp, &req);
        } else {
            code = cm_GetSCache(&Fid, &scp, userp, &req);
        }
        if (code) {
            smb_MapNTError(code, &status);
            (*ResultCB)->ResultStatus = status;
            cm_ReleaseSCache(dscp);
            osi_Log2(afsd_logp, "RDR_EvaluateNodeByID cm_GetSCache SourceFID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        /* If the SourceID.Cell == 0 then we are evaluating the root mount point */
        Fid = cm_data.rootFid;
        scp = cm_data.rootSCachep;
        cm_HoldSCache(scp);
    }

    /* Make sure the directory is current */
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
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

    code = RDR_PopulateCurrentEntry(pCurrentEntry, ResultBufferLength, 
                                    dscp, scp, userp, &req, NULL, NULL,
                                    (bWow64 ? RDR_POP_WOW64 : 0) | 
                                    (bSkipStatus ? RDR_POP_NO_GETSTATUS : 0) |
                                    RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                    NULL, &dwRemaining);

    cm_ReleaseSCache(scp);
    cm_ReleaseSCache(dscp);

    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_EvaluateNodeByID FAILURE code=0x%x status=0x%x",
                 code, status);
    } else {
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

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;
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

    code = cm_GetSCache(&parentFid, &dscp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_CreateFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
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

    if (CreateCB->FileAttributes & FILE_ATTRIBUTE_READONLY) {
        setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
        setAttr.unixModeBits = 0222;
    }

    if (CreateCB->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        code = cm_MakeDir(dscp, FileName, flags, &setAttr, userp, &req, &scp);
    } else {
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
            smb_MapNTError(code, &status);
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

        dfid.vnode = htonl(scp->fid.vnode);
        dfid.unique = htonl(scp->fid.unique);

        if (!cm_Is8Dot3(FileName))
            cm_Gen8Dot3NameIntW(FileName, &dfid, shortName, NULL);
        else
            shortName[0] = '\0';
        code = RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining, 
                                        dscp, scp, userp, &req, FileName, shortName,
                                        RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                        NULL, &dwRemaining);

        cm_ReleaseSCache(scp);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        osi_Log0(afsd_logp, "RDR_CreateFileEntry SUCCESS");
    } else {
        smb_MapNTError(code, &status);
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

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;
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

    code = cm_GetSCache(&parentFid, &dscp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_UpdateFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        cm_ReleaseSCache(dscp);
        osi_Log2(afsd_logp, "RDR_UpdateFileEntry cm_GetSCache object FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_UpdateFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* Do not set length and other attributes at the same time */
    if (scp->length.QuadPart != UpdateCB->AllocationSize.QuadPart) {
        osi_Log2(afsd_logp, "RDR_UpdateFileEntry Length Change 0x%x -> 0x%x", 
                 (afs_uint32)scp->length.QuadPart, (afs_uint32)UpdateCB->AllocationSize.QuadPart);
        setAttr.mask |= CM_ATTRMASK_LENGTH;
        setAttr.length.LowPart = UpdateCB->AllocationSize.LowPart;
        setAttr.length.HighPart = UpdateCB->AllocationSize.HighPart;
        lock_ReleaseWrite(&scp->rw);
        code = cm_SetAttr(scp, &setAttr, userp, &req);
        if (code)
            goto on_error;
        setAttr.mask = 0;
        lock_ObtainWrite(&scp->rw);
    }

    if ((scp->unixModeBits & 0222) && (UpdateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
        setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
        setAttr.unixModeBits = scp->unixModeBits & ~0222;
    } else if (!(scp->unixModeBits & 0222) && !(UpdateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
        setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
        setAttr.unixModeBits = scp->unixModeBits | 0222;
    }

    ft.dwLowDateTime = UpdateCB->LastWriteTime.LowPart;
    ft.dwHighDateTime = UpdateCB->LastWriteTime.HighPart;

    smb_UnixTimeFromLargeSearchTime(&clientModTime, &ft);
    if (scp->clientModTime != clientModTime) {
        setAttr.mask |= CM_ATTRMASK_CLIENTMODTIME;
        setAttr.clientModTime = clientModTime;
    }
    lock_ReleaseWrite(&scp->rw);

    /* call setattr */
    if (setAttr.mask)
        code = cm_SetAttr(scp, &setAttr, userp, &req);
    else
        code = 0;

  on_error:
    if (code == 0) {
        DWORD dwRemaining = ResultBufferLength - sizeof( AFSFileUpdateResultCB) + sizeof( AFSDirEnumEntry);
        
        pResultCB = (AFSFileUpdateResultCB *)(*ResultCB)->ResultData;

        code = RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining, 
                                        dscp, scp, userp, &req, NULL, NULL,
                                        RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                        NULL, &dwRemaining);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        osi_Log0(afsd_logp, "RDR_UpdateFileEntry SUCCESS");
    } else {
        smb_MapNTError(code, &status);
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
RDR_DeleteFileEntry( IN cm_user_t *userp,
                     IN AFSFileID ParentId,
                     IN WCHAR *FileNameCounted,
                     IN DWORD FileNameLength,
                     IN BOOL bWow64,
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

    StringCchCopyNW(FileName, 260, FileNameCounted, FileNameLength / sizeof(WCHAR));

    osi_Log4(afsd_logp, "RDR_DeleteFileEntry parent FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              ParentId.Cell,  ParentId.Volume, 
              ParentId.Vnode, ParentId.Unique);
    osi_Log1(afsd_logp, "... name=%S", osi_LogSaveStringW(afsd_logp, FileName));

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;
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

    code = cm_GetSCache(&parentFid, &dscp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_DeleteFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
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
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        cm_ReleaseSCache(dscp);
        osi_Log2(afsd_logp, "RDR_DeleteFileEntry cm_Lookup failure code=0x%x status=0x%x",
                 code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        cm_ReleaseSCache(dscp);
        osi_Log3(afsd_logp, "RDR_DeleteFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);
        
    if (scp->fileType == CM_SCACHETYPE_DIRECTORY)
        code = cm_RemoveDir(dscp, NULL, FileName, userp, &req);
    else
        code = cm_Unlink(dscp, NULL, FileName, userp, &req);

    if (code == 0) {
        (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

        (*ResultCB)->ResultBufferLength = sizeof( AFSFileDeleteResultCB);

        pResultCB = (AFSFileDeleteResultCB *)(*ResultCB)->ResultData;

        pResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;
        osi_Log0(afsd_logp, "RDR_DeleteFileEntry SUCCESS");
    } else {
        smb_MapNTError(code, &status);
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
    cm_scache_t *          oldDscp;
    cm_scache_t *          newDscp;
    wchar_t                shortName[13];
    wchar_t                SourceFileName[260];
    wchar_t                TargetFileName[260];
    cm_dirFid_t            dfid;
    cm_req_t               req;
    afs_uint32             code;
    DWORD                  status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

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

    code = cm_GetSCache(&SourceParentFid, &oldDscp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_GetSCache source parent failed code 0x%x", code);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    lock_ObtainWrite(&oldDscp->rw);
    code = cm_SyncOp(oldDscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) { 
        osi_Log2(afsd_logp, "RDR_RenameFileEntry cm_SyncOp oldDscp 0x%p failed code 0x%x", oldDscp, code);
        smb_MapNTError(code, &status);
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

    code = cm_GetSCache(&TargetParentFid, &newDscp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_GetSCache target parent failed code 0x%x", code);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        cm_ReleaseSCache(oldDscp);
        return;
    }

    lock_ObtainWrite(&newDscp->rw);
    code = cm_SyncOp(newDscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        osi_Log2(afsd_logp, "RDR_RenameFileEntry cm_SyncOp newDscp 0x%p failed code 0x%x", newDscp, code);
        smb_MapNTError(code, &status);
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

    code = cm_Rename( oldDscp, NULL, SourceFileName, 
                      newDscp, TargetFileName, userp, &req);
    if (code == 0) {
        cm_dirOp_t dirop;
        cm_fid_t   targetFid;
        cm_scache_t *scp = 0;
        DWORD dwRemaining;

        (*ResultCB)->ResultBufferLength = ResultBufferLength;
        dwRemaining = ResultBufferLength - sizeof( AFSFileRenameResultCB) + sizeof( AFSDirEnumEntry);
        (*ResultCB)->ResultStatus = 0;

        pResultCB->SourceParentDataVersion.QuadPart = oldDscp->dataVersion;
        pResultCB->TargetParentDataVersion.QuadPart = newDscp->dataVersion;

        osi_Log2(afsd_logp, "RDR_RenameFileEntry cm_Rename oldDscp 0x%p newDscp 0x%p SUCCESS", 
                 oldDscp, newDscp);

        code = cm_BeginDirOp( newDscp, userp, &req, CM_DIRLOCK_READ, &dirop);
        if (code == 0) {
            code = cm_BPlusDirLookup(&dirop, TargetFileName, &targetFid);
            cm_EndDirOp(&dirop);
        }

        if (code != 0) {
            osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_BPlusDirLookup failed code 0x%x", 
                     code);
            (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
            cm_ReleaseSCache(oldDscp);
            cm_ReleaseSCache(newDscp);
            return;
        } 

        osi_Log4(afsd_logp, "RDR_RenameFileEntry Target FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
                  targetFid.cell,  targetFid.volume, 
                  targetFid.vnode, targetFid.unique);

        code = cm_GetSCache(&targetFid, &scp, userp, &req);
        if (code) {
            osi_Log1(afsd_logp, "RDR_RenameFileEntry cm_GetSCache target failed code 0x%x", code);
            smb_MapNTError(code, &status);
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
            smb_MapNTError(code, &status);
            (*ResultCB)->ResultStatus = status;
            lock_ReleaseWrite(&scp->rw);
            cm_ReleaseSCache(oldDscp);
            cm_ReleaseSCache(newDscp);
            cm_ReleaseSCache(scp);
            return;
        }

        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseWrite(&scp->rw);

        dfid.vnode = htonl(scp->fid.vnode);
        dfid.unique = htonl(scp->fid.unique);

        if (!cm_Is8Dot3(TargetFileName))
            cm_Gen8Dot3NameIntW(TargetFileName, &dfid, shortName, NULL);
        else
            shortName[0] = '\0';
        RDR_PopulateCurrentEntry(&pResultCB->DirEnum, dwRemaining,
                                 newDscp, scp, userp, &req, TargetFileName, shortName,
                                 RDR_POP_FOLLOW_MOUNTPOINTS | RDR_POP_EVALUATE_SYMLINKS,
                                 NULL, &dwRemaining);
        (*ResultCB)->ResultBufferLength = ResultBufferLength - dwRemaining;
        cm_ReleaseSCache(scp);

        osi_Log0(afsd_logp, "RDR_RenameFileEntry SUCCESS");
    } else {
        osi_Log3(afsd_logp, "RDR_RenameFileEntry cm_Rename oldDscp 0x%p newDscp 0x%p failed code 0x%x", 
                 oldDscp, newDscp, code);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
    }

    cm_ReleaseSCache(oldDscp);
    cm_ReleaseSCache(newDscp);
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

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    osi_Log4(afsd_logp, "RDR_FlushFileEntry File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique);

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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_FlushFileEntry cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_FlushFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);
        
    code = cm_FSync(scp, userp, &req);
    cm_ReleaseSCache(scp);

    if (code) {
        smb_MapNTError(code, &status);
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
    if (file && ((access & FILE_WRITE_DATA) || (access & FILE_APPEND_DATA)))
	afs_acc |= PRSFS_WRITE;
    if (access & FILE_WRITE_EA || access & FILE_WRITE_ATTRIBUTES)
	afs_acc |= PRSFS_WRITE;
    if (dir && ((access & FILE_ADD_FILE) || (access & FILE_ADD_SUBDIRECTORY)))
	afs_acc |= PRSFS_INSERT;
    if (dir && (access & FILE_LIST_DIRECTORY))
	afs_acc |= PRSFS_LOOKUP;
    if (access & FILE_READ_EA || access & FILE_READ_ATTRIBUTES)
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
	if (cm_HaveAccessRights(scp, userp, afs_acc, &afs_gr))
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
            *granted |= FILE_READ_DATA | FILE_EXECUTE;
        if (afs_gr & PRSFS_WRITE)
            *granted |= FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_EXECUTE;
        if (afs_gr & PRSFS_INSERT)
            *granted |= (dir ? FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY : 0) | (file ? FILE_ADD_SUBDIRECTORY : 0);
        if (afs_gr & PRSFS_LOOKUP)
            *granted |= (dir ? FILE_LIST_DIRECTORY : 0) | FILE_READ_EA | FILE_READ_ATTRIBUTES;
        if (afs_gr & PRSFS_DELETE)
            *granted |= FILE_DELETE_CHILD | DELETE;
        if (afs_gr & PRSFS_LOCK)
            *granted |= 0;
        if (afs_gr & PRSFS_ADMINISTER)
            *granted |= 0;

        *granted |= SYNCHRONIZE | READ_CONTROL;

        /* don't give more access than what was requested */
        *granted &= access;
        osi_Log2(afsd_logp, "RDR_CheckAccess SUCCESS scp=0x%p granted=0x%x", scp, *granted);
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
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB)
{
    AFSFileOpenResultCB *pResultCB = NULL;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    cm_lock_data_t      *ldp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_OpenFileEntry cm_GetSCache ParentFID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log3(afsd_logp, "RDR_OpenFileEntry cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);
        
    code = cm_CheckNTOpen(scp, OpenCB->DesiredAccess, OPEN_ALWAYS, userp, &req, &ldp);
    if (code == 0)
        code = RDR_CheckAccess(scp, userp, &req, OpenCB->DesiredAccess, &pResultCB->GrantedAccess);
    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
    cm_ReleaseSCache(scp);

    if (code) {
        smb_MapNTError(code, &status);
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
RDR_RequestFileExtentsSync( IN cm_user_t *userp,
                            IN AFSFileID FileId,
                            IN AFSRequestExtentsCB *RequestExtentsCB,
                            IN BOOL bWow64,
                            IN DWORD ResultBufferLength,
                            IN OUT AFSCommResult **ResultCB)
{
    AFSRequestExtentsResultCB *pResultCB = NULL;
    DWORD Length;
    DWORD count = 0;
    cm_scache_t *scp = NULL;
    BOOLEAN     bScpLocked = FALSE;
    BOOLEAN     bBufLocked = FALSE;
    BOOLEAN     bBufRelease = FALSE;
    cm_fid_t    Fid;
    cm_buf_t    *bufp;
    afs_uint32  code;
    osi_hyper_t thyper;
    LARGE_INTEGER ByteOffset, EndOffset;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;
    req.flags |= CM_REQ_NORETRY;

    osi_Log4(afsd_logp, "RDR_RequestFileExtentsSync File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique);

    Length = sizeof( AFSCommResult) + sizeof( AFSRequestExtentsResultCB) * (RequestExtentsCB->Length / cm_data.blockSize + 1);
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult));
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
    (*ResultCB)->ResultBufferLength = Length;

    pResultCB = (AFSRequestExtentsResultCB *)(*ResultCB)->ResultData;

    /* Allocate the extents from the buffer package */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_RequestFileExtentsSync cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    bScpLocked = TRUE;

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_READ,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_BULKREAD);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_RequestFileExtentsSync cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* the scp is now locked and current */

    /* Allocate the extents from the buffer package */
    for ( count = 0, ByteOffset = RequestExtentsCB->ByteOffset, EndOffset.QuadPart = ByteOffset.QuadPart + RequestExtentsCB->Length; 
          code == 0 && ByteOffset.QuadPart < EndOffset.QuadPart; 
          ByteOffset.QuadPart += cm_data.blockSize)
    {
        thyper.QuadPart = ByteOffset.QuadPart;

        if (bScpLocked) {
            lock_ReleaseWrite(&scp->rw);
            bScpLocked = FALSE;
        }

        code = buf_Get(scp, &thyper, &req, &bufp);
        
        if (code == 0) {
            lock_ObtainMutex(&bufp->mx);

            if (bufp->flags & CM_BUF_REDIR) {
                lock_ReleaseMutex(&bufp->mx);
                bBufRelease = TRUE;
                goto release;
            }
            
            bBufLocked = TRUE;
            bufp->flags |= CM_BUF_REDIR;
            bBufRelease = FALSE;

            /* now get the data in the cache */
            if (ByteOffset.QuadPart < scp->length.QuadPart) {
                while (1) {
                    if (!bScpLocked) {
                        lock_ObtainWrite(&scp->rw);
                        bScpLocked = TRUE;
                    }
                    code = cm_SyncOp(scp, bufp, userp, &req, 0,
                        CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | (bBufLocked ? CM_SCACHESYNC_BUFLOCKED : 0));
                    if (code) 
                        break;

                    cm_SyncOpDone(scp, bufp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | (bBufLocked ? CM_SCACHESYNC_BUFLOCKED : 0));

                    /* 
                     * Prevent reading the buffer from the file server
                     * if the file system wants it clean 
                     */
                    if ((RequestExtentsCB->Flags & AFS_EXTENT_FLAG_CLEAN) &&
                        ByteOffset.QuadPart <= bufp->offset.QuadPart && 
                        EndOffset.QuadPart >= bufp->offset.QuadPart + cm_data.blockSize) {
                        memset(bufp->datap, 0, cm_data.blockSize);
                        bufp->dataVersion = scp->dataVersion;
                    }

                    if (cm_HaveBuffer(scp, bufp, bBufLocked)) 
                        break;
        
                    if (bBufLocked) {
                        lock_ReleaseMutex(&bufp->mx);
                        bBufLocked = FALSE;
                    }
                    /* 
                     * otherwise, load the buffer and try again.
                     * call with scp locked and bufp unlocked.
                     */
                    code = cm_GetBuffer(scp, bufp, NULL, userp, &req);
                    if (code) 
                        break;
                }
            } else {
                memset(bufp->datap, 0, cm_data.blockSize);
            }
            if (bBufLocked) {
                lock_ReleaseMutex(&bufp->mx);
                bBufLocked = FALSE;
            }

            /* if an error occurred, don't give the extent to the file system */
            if (code) {
                if (bScpLocked) {
                    lock_ReleaseWrite(&scp->rw);
                    bScpLocked = FALSE;
                }
                if (!bBufLocked)
                    lock_ObtainMutex(&bufp->mx);
                bufp->flags &= ~CM_BUF_REDIR;
                bBufRelease = TRUE;
                lock_ReleaseMutex(&bufp->mx);
            } else {
                pResultCB->FileExtents[count].Flags = 0;
                pResultCB->FileExtents[count].FileOffset = ByteOffset;
                pResultCB->FileExtents[count].CacheOffset.QuadPart = bufp->datap - cm_data.baseAddress;
                pResultCB->FileExtents[count].Length = cm_data.blockSize;
                osi_Log4(afsd_logp, "RDR_RequestFileExtentsSync Extent2FS bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                          bufp, ByteOffset.QuadPart, bufp->datap - cm_data.baseAddress, cm_data.blockSize);
                count++;
            }

          release:
            if (bBufRelease)
                buf_Release(bufp);
        } else if ( code == CM_ERROR_WOULDBLOCK ) {
            /* 
             * TODO: queue up this extent and all remaining extents in the request so that 
             * they can can be processed when extents become available.
             */
            DebugBreak();
            break;
        }
    }
    
    if (!bScpLocked) {
        lock_ObtainWrite(&scp->rw);
        bScpLocked = TRUE;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_BULKREAD);

    pResultCB->ExtentCount = count;
    if (bScpLocked) {
        lock_ReleaseWrite(&scp->rw);
        bScpLocked = FALSE;
    }
    cm_ReleaseSCache(scp);

    if (code && count == 0) {
        osi_Log2(afsd_logp, "RDR_RequestFileExtentsSync cm_SyncOp failure scp=0x%p code=0x%x",
                 scp, code);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
    } else {
        (*ResultCB)->ResultStatus = 0;
        osi_Log0(afsd_logp, "RDR_RequestFileExtentsSync SUCCESS");
    }
    return;
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
    LARGE_INTEGER ByteOffset, EndOffset;
    cm_req_t    req;
    BOOLEAN     bScpLocked = FALSE;
    BOOLEAN     bBufLocked = FALSE;
    BOOLEAN     bBufRelease = FALSE;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;
    req.flags |= CM_REQ_NORETRY;

    osi_Log4(afsd_logp, "RDR_RequestFileExtentsAsync File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique);
    Length = sizeof( AFSSetFileExtentsCB) + sizeof( AFSRequestExtentsResultCB) * (RequestExtentsCB->Length / cm_data.blockSize + 1);

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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_RequestFileExtentsAsync cm_GetSCache FID failure code=0x%x",
                  code);
        smb_MapNTError(code, &status);
        pResultCB->ResultStatus = status;
        return FALSE;
    }

    lock_ObtainWrite(&scp->rw);
    bScpLocked = TRUE;

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, PRSFS_READ,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_BULKREAD);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        osi_Log2(afsd_logp, "RDR_RequestFileExtentsAsync cm_SyncOp failure scp=0x%p code=0x%x",
                 scp, code);
        smb_MapNTError(code, &status);
        pResultCB->ResultStatus = status;
        return FALSE;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* the scp is now locked and current */

    /* Allocate the extents from the buffer package */
    for ( count = 0, ByteOffset = RequestExtentsCB->ByteOffset, EndOffset.QuadPart = ByteOffset.QuadPart + RequestExtentsCB->Length; 
          code == 0 && ByteOffset.QuadPart < EndOffset.QuadPart; 
          ByteOffset.QuadPart += cm_data.blockSize)
    {
      retry_loop:
        thyper.QuadPart = ByteOffset.QuadPart;

        if (bScpLocked) {
            lock_ReleaseWrite(&scp->rw);
            bScpLocked = FALSE;
        }
        code = buf_Get(scp, &thyper, &req, &bufp);
        
        if (code == 0) {
            lock_ObtainMutex(&bufp->mx);

            if (bufp->flags & CM_BUF_REDIR) {
                lock_ReleaseMutex(&bufp->mx);
                bBufRelease = TRUE;
                goto release;
            }
            
            bBufLocked = TRUE;
            bufp->flags |= CM_BUF_REDIR;
            bBufRelease = FALSE;

            /* now get the data in the cache */
            if (ByteOffset.QuadPart < scp->length.QuadPart) {
                while (1) {
                    if (!bScpLocked) {
                        lock_ObtainWrite(&scp->rw);
                        bScpLocked = TRUE;
                    }
                    code = cm_SyncOp(scp, bufp, userp, &req, 0,
                        CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | (bBufLocked ? CM_SCACHESYNC_BUFLOCKED : 0));
                    if (code) 
                        break;

                    cm_SyncOpDone(scp, bufp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ | (bBufLocked ? CM_SCACHESYNC_BUFLOCKED : 0));

                    if (cm_HaveBuffer(scp, bufp, bBufLocked)) 
                        break;

                    if (bBufLocked) {
                        lock_ReleaseMutex(&bufp->mx);
                        bBufLocked = FALSE;
                    }

                    /* 
                     * otherwise, load the buffer and try again 
                     * call with scp locked and bufp unlocked.
                     */
                    code = cm_GetBuffer(scp, bufp, NULL, userp, &req);
                    if (code) 
                        break;
                }
            } else {
                memset(bufp->datap, 0, cm_data.blockSize);
            }

            if (bBufLocked) {
                lock_ReleaseMutex(&bufp->mx);
                bBufLocked = FALSE;
            }

            /* if an error occurred, don't give the extent to the file system */
            if (code) {
                if (bScpLocked) {
                    lock_ReleaseWrite(&scp->rw);
                    bScpLocked = FALSE;
                }
                if (!bBufLocked)
                    lock_ObtainMutex(&bufp->mx);
                bufp->flags &= ~CM_BUF_REDIR;
                bBufRelease = TRUE;
                lock_ReleaseMutex(&bufp->mx);
            } else {
                pResultCB->FileExtents[count].Flags = 0;
                pResultCB->FileExtents[count].FileOffset = ByteOffset;
                pResultCB->FileExtents[count].CacheOffset.QuadPart = bufp->datap - cm_data.baseAddress;
                pResultCB->FileExtents[count].Length = cm_data.blockSize;
                osi_Log4(afsd_logp, "RDR_RequestFileExtentsAsync Extent2FS bufp 0x%p foffset 0x%p coffset 0x%p len 0x%x",
                          bufp, ByteOffset.QuadPart, bufp->datap - cm_data.baseAddress, cm_data.blockSize);
                count++;
            }
          release:
            if ( bBufRelease )
                buf_Release(bufp);
        } else if ( code == CM_ERROR_WOULDBLOCK ) {
            /* 
             * TODO: queue up this extent and all remaining extents in the request so that 
             * they can can be processed when extents become available.  Or we can return
             * what we have so far and have the request re-issued starting from where we
             * left off.
             */
            if ( count == 0 ) {
                Sleep(20);
                goto retry_loop;
            }

            RequestExtentsCB->ByteOffset.QuadPart += count * cm_data.blockSize;
            RequestExtentsCB->Length -= count * cm_data.blockSize;
            break;
        }
    }

    if (!bScpLocked) {
        lock_ObtainWrite(&scp->rw);
        bScpLocked = TRUE;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_BULKREAD);

    (*ResultCB)->ExtentCount = count;
    if (bScpLocked)
        lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    if (code == CM_ERROR_WOULDBLOCK) {
        osi_Log3(afsd_logp, "RDR_RequestFileExtentsAsync wouldblock scp=0x%p count=0x%x code=0x%x",
                 scp, count, code);
        return TRUE;
    } else if (code) {
        osi_Log2(afsd_logp, "RDR_RequestFileExtentsAsync failure scp=0x%p code=0x%x",
                 scp, code);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        return FALSE;
    } else {
        osi_Log0(afsd_logp, "RDR_RequestFileExtentsAsync SUCCESS");
        return FALSE;
    }
}



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
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        osi_Log2(afsd_logp, "RDR_ReleaseFileExtents cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    for ( count = 0; count < ReleaseExtentsCB->ExtentCount; count++) {
        thyper.QuadPart = ReleaseExtentsCB->FileExtents[count].FileOffset.QuadPart;

        bufp = buf_Find(scp, &thyper);
        if (bufp) {
            osi_Log4(afsd_logp, "RDR_ReleaseFileExtents bufp 0x%p vno 0x%x foffset 0x%p coffset 0x%p",
                     bufp, bufp->fid.vnode, ReleaseExtentsCB->FileExtents[count].FileOffset.QuadPart, 
                     ReleaseExtentsCB->FileExtents[count].CacheOffset.QuadPart);

            if (ReleaseExtentsCB->FileExtents[count].Flags) {
                lock_ObtainMutex(&bufp->mx);
                if ( ReleaseExtentsCB->Flags & AFS_EXTENT_FLAG_RELEASE ||
                     ReleaseExtentsCB->FileExtents[count].Flags & AFS_EXTENT_FLAG_RELEASE ) {
                    bufp->flags &= ~CM_BUF_REDIR;
                    buf_Release(bufp);
                    released++;
                }
                if ( ReleaseExtentsCB->FileExtents[count].Flags & AFS_EXTENT_FLAG_DIRTY ) {
                    buf_SetDirty(bufp, 0, cm_data.blockSize, userp);
                    dirty++;
                }
                lock_ReleaseMutex(&bufp->mx);
            }
            buf_Release(bufp);
        } else {
            osi_Log4(afsd_logp, "RDR_ReleaseFileExtents unknown extent vol 0x%x vno 0x%x foffset 0x%p coffset 0x%p",
                     scp->fid.volume, scp->fid.vnode, ReleaseExtentsCB->FileExtents[count].FileOffset.QuadPart, 
                     ReleaseExtentsCB->FileExtents[count].CacheOffset.QuadPart);
        }
    }

    if (dirty) {
        for ( count = 0; code == 0 && count < ReleaseExtentsCB->ExtentCount && dirty > 0; count++, dirty--) {
            thyper.QuadPart = ReleaseExtentsCB->FileExtents[count].FileOffset.QuadPart;

            bufp = buf_Find(scp, &thyper);
            if (bufp) {
                lock_ObtainMutex(&bufp->mx);
                if (bufp->flags & CM_BUF_DIRTY) {
                    code = buf_CleanAsyncLocked(bufp, &req, NULL);

                    /* 
                     * if we retry we are just going to block the worker
                     * thread for a longer period of time without necessarily
                     * making any progress.   We have marked the buffer dirty
                     * and will be able to write it to the file server the next
                     * time the file system releases it.
                     */
                    if ( code == CM_ERROR_WOULDBLOCK )
                        code = 0;
                }
                lock_ReleaseMutex(&bufp->mx);
                buf_Release(bufp);
            }
        }
    }

    cm_ReleaseSCache(scp);

    osi_Log5(afsd_logp, "RDR_ReleaseFileExtents File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x Released %d",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique, released);
    if (code) {
        smb_MapNTError(code, &status);
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
    unsigned int fileno, extentno;
    AFSReleaseFileExtentsResultFileCB *pNextFileCB;
    RDR_InitReq(&req);

    if ( ReleaseFileExtentsResultCB->FileCount == 0 ) {
        osi_Log0(afsd_logp, "RDR_ProcessReleaseFileExtentsResult is empty");
        return CM_ERROR_RETRY;
    }

    for ( fileno = 0, pNextFileCB = &ReleaseFileExtentsResultCB->Files[0]; 
          fileno < ReleaseFileExtentsResultCB->FileCount; fileno++ ) {
        AFSReleaseFileExtentsResultFileCB *pFileCB = pNextFileCB;
        cm_user_t       *userp = NULL;
        cm_fid_t         Fid;
        cm_scache_t *    scp = NULL;
        int              dirty = 0;
        int              released = 0;
        char * p;

        userp = RDR_UserFromProcessId( pFileCB->ProcessId);

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

        code = cm_GetSCache(&Fid, &scp, userp, &req);
        if (code) {
            osi_Log1(afsd_logp, "RDR_ProcessReleaseFileExtentsResult cm_GetSCache FID failure code=0x%x",
                      code);
            goto cleanup_file;
        }

        for ( extentno = 0; extentno < pFileCB->ExtentCount; extentno++ ) {
            osi_hyper_t thyper;
            cm_buf_t    *bufp;
            AFSFileExtentCB *pExtent = &pFileCB->FileExtents[extentno];

            thyper.QuadPart = pExtent->FileOffset.QuadPart;

            bufp = buf_Find(scp, &thyper);
            if (bufp) {
                osi_Log3(afsd_logp, "RDR_ProcessReleaseFileExtentsResult bufp 0x%p foffset 0x%p coffset 0x%p",
                          bufp, pFileCB->FileExtents[extentno].FileOffset.QuadPart, 
                          pFileCB->FileExtents[extentno].CacheOffset.QuadPart);

                if (pExtent->Flags) {
                    lock_ObtainMutex(&bufp->mx);
                    if ( ReleaseFileExtentsResultCB->Flags & AFS_EXTENT_FLAG_RELEASE ||
                         pExtent->Flags & AFS_EXTENT_FLAG_RELEASE ) {
                        bufp->flags &= ~CM_BUF_REDIR;
                        buf_Release(bufp);
                        released++;
                    }
                    if ( pExtent->Flags & AFS_EXTENT_FLAG_DIRTY ) {
                        buf_SetDirty(bufp, 0, cm_data.blockSize, userp);
                        dirty++;
                    }
                    lock_ReleaseMutex(&bufp->mx);
                }
                buf_Release(bufp);
            } else {
                osi_Log4(afsd_logp, "RDR_ProcessReleaseFileExtentsResult unknown extent vol 0x%x vno 0x%x foffset 0x%p coffset 0x%p",
                          scp->fid.volume, scp->fid.vnode, pFileCB->FileExtents[extentno].FileOffset.QuadPart, 
                          pFileCB->FileExtents[extentno].CacheOffset.QuadPart);
            }
        }

        if (dirty) {
            code = cm_FSync(scp, userp, &req);
            if (code) {
                osi_Log1(afsd_logp, "RDR_ProcessReleaseFileExtentsResult cm_FSync failure code=0x%x",
                          code);
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

    RDR_InitReq(&req);

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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        osi_Log1(afsd_logp, "RDR_ReleaseFailedSetFileExtents cm_GetSCache FID failure code=0x%x",
                  code);
        goto cleanup_file;
    }

    for ( extentno = 0; extentno < SetFileExtentsResultCB->ExtentCount; extentno++ ) {
        osi_hyper_t thyper;
        cm_buf_t    *bufp;
        AFSFileExtentCB *pExtent = &SetFileExtentsResultCB->FileExtents[extentno];

        thyper.QuadPart = pExtent->FileOffset.QuadPart;

        bufp = buf_Find(scp, &thyper);
        if (bufp) {
            osi_Log3(afsd_logp, "RDR_ReleaseFailedSetFileExtents bufp 0x%p foffset 0x%p coffset 0x%p",
                      bufp, pExtent->FileOffset.QuadPart, 
                      pExtent->CacheOffset.QuadPart);

            if (pExtent->Flags) {
                lock_ObtainMutex(&bufp->mx);
                bufp->flags &= ~CM_BUF_REDIR;
                lock_ReleaseMutex(&bufp->mx);
                buf_Release(bufp);
            }
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
    RDR_SetupIoctl(pPioctlCB->RequestId, &ParentFid, &RootFid, userp);

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

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));

    pResultCB = (AFSPIOCtlIOResultCB *)(*ResultCB)->ResultData;

    code = RDR_IoctlWrite(userp, pPioctlCB->RequestId, pPioctlCB->BufferLength, pPioctlCB->MappedBuffer, &req);
    if (code) {
        smb_MapNTError(code, &status);
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
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB)
{
    AFSPIOCtlIOResultCB *pResultCB;
    cm_scache_t *dscp = NULL;
    afs_uint32  code;
    cm_req_t    req;
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));
    if (!(*ResultCB))
	return;

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof(AFSPIOCtlIOResultCB));

    pResultCB = (AFSPIOCtlIOResultCB *)(*ResultCB)->ResultData;

    code = RDR_IoctlRead(userp, pPioctlCB->RequestId, pPioctlCB->BufferLength, pPioctlCB->MappedBuffer, 
                         &pResultCB->BytesProcessed, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        return;
    }

    (*ResultCB)->ResultBufferLength = sizeof( AFSPIOCtlIOResultCB);
}

void
RDR_ByteRangeLockSync( IN cm_user_t     *userp,
                       IN ULARGE_INTEGER ProcessId,
                       IN AFSFileID     FileId,
                       IN AFSByteRangeLockRequestCB *pBRLRequestCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB)
{
    AFSByteRangeLockResultCB *pResultCB = NULL;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    cm_key_t    key;
    DWORD       i;
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    osi_Log4(afsd_logp, "RDR_ByteRangeLockSync File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique);

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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_ByteRangeLockSync cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_ByteRangeLockSync cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* the scp is now locked and current */
    for ( i=0; i<pBRLRequestCB->Count; i++ ) {
        key = cm_GenerateKey(CM_SESSION_IFS, ProcessId.QuadPart, 0);

        pResultCB->Result[i].LockType = pBRLRequestCB->Request[i].LockType;
        pResultCB->Result[i].Offset = pBRLRequestCB->Request[i].Offset;
        pResultCB->Result[i].Length = pBRLRequestCB->Request[i].Length;

        code = cm_Lock(scp, pBRLRequestCB->Request[i].LockType == AFS_BYTE_RANGE_LOCK_TYPE_SHARED,
                       pBRLRequestCB->Request[i].Offset, 
                       pBRLRequestCB->Request[i].Length,
                       key, 0, userp, &req, NULL);
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

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    (*ResultCB)->ResultStatus = 0;
    osi_Log0(afsd_logp, "RDR_ByteRangeLockSync SUCCESS");
    return;
}

void
RDR_ByteRangeLockAsync( IN cm_user_t     *userp,
                        IN ULARGE_INTEGER ProcessId,
                        IN AFSFileID     FileId,
                        IN AFSAsyncByteRangeLockRequestCB *pABRLRequestCB,
                        IN BOOL bWow64,
                        OUT DWORD *ResultBufferLength,
                        IN OUT AFSSetByteRangeLockResultCB **ResultCB)
{
    AFSSetByteRangeLockResultCB *pResultCB = NULL;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    cm_key_t    key;
    DWORD       i;
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    osi_Log4(afsd_logp, "RDR_ByteRangeLockAsync File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique);

    Length = sizeof( AFSSetByteRangeLockResultCB) + ((pABRLRequestCB->Count - 1) * sizeof(AFSByteRangeLockResult));
    *ResultCB = (AFSSetByteRangeLockResultCB *)malloc( Length );
    if (!(*ResultCB))
	return;
    memset( *ResultCB, '\0', Length );
    *ResultBufferLength = Length;

    pResultCB = (AFSSetByteRangeLockResultCB *)(*ResultCB);
    pResultCB->SerialNumber = pABRLRequestCB->SerialNumber;
    pResultCB->FileId = FileId;
    pResultCB->Count = pABRLRequestCB->Count;

    /* Allocate the extents from the buffer package */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->Result[0].Status = status;
        (*ResultCB)->Count = 0;
        *ResultBufferLength = sizeof(AFSSetByteRangeLockResultCB);
        osi_Log2(afsd_logp, "RDR_ByteRangeLockAsync cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &status);
        (*ResultCB)->Result[0].Status = status;
        (*ResultCB)->Count = 0;
        *ResultBufferLength = sizeof(AFSSetByteRangeLockResultCB);
        osi_Log3(afsd_logp, "RDR_ByteRangeLockAsync cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* the scp is now locked and current */
    for ( i=0; i<pABRLRequestCB->Count; i++ ) {
        key = cm_GenerateKey(CM_SESSION_IFS, ProcessId.QuadPart, 0);

        pResultCB->Result[i].LockType = pABRLRequestCB->Request[i].LockType;
        pResultCB->Result[i].Offset = pABRLRequestCB->Request[i].Offset;
        pResultCB->Result[i].Length = pABRLRequestCB->Request[i].Length;

        code = cm_Lock(scp, pABRLRequestCB->Request[i].LockType == AFS_BYTE_RANGE_LOCK_TYPE_SHARED,
                       pABRLRequestCB->Request[i].Offset, 
                       pABRLRequestCB->Request[i].Length,
                       key, 0, userp, &req, NULL);
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

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);
    osi_Log0(afsd_logp, "RDR_ByteRangeLockAsync SUCCESS");
    return;
}

void
RDR_ByteRangeUnlock( IN cm_user_t     *userp,
                     IN ULARGE_INTEGER ProcessId,
                     IN AFSFileID     FileId,
                     IN AFSByteRangeUnlockRequestCB *pBRURequestCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{
    AFSByteRangeUnlockResultCB *pResultCB = NULL;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    cm_key_t    key;
    DWORD       i;
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    osi_Log4(afsd_logp, "RDR_ByteRangeUnlock File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique);

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

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_ByteRangeUnlock cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_ByteRangeUnlock cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* the scp is now locked and current */
    for ( i=0; i<pBRURequestCB->Count; i++ ) {
        key = cm_GenerateKey(CM_SESSION_IFS, ProcessId.QuadPart, 0);

        pResultCB->Result[i].LockType = pBRURequestCB->Request[i].LockType;
        pResultCB->Result[i].Offset = pBRURequestCB->Request[i].Offset;
        pResultCB->Result[i].Length = pBRURequestCB->Request[i].Length;

        code = cm_Unlock(scp, pBRURequestCB->Request[i].LockType == AFS_BYTE_RANGE_LOCK_TYPE_SHARED,
                       pBRURequestCB->Request[i].Offset, 
                       pBRURequestCB->Request[i].Length,
                       key, userp, &req);

        smb_MapNTError(code, &status);
        pResultCB->Result[i].Status = status;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    (*ResultCB)->ResultStatus = 0;
    osi_Log0(afsd_logp, "RDR_ByteRangeUnlock SUCCESS");
    return;
}

void
RDR_ByteRangeUnlockAll( IN cm_user_t     *userp,
                        IN ULARGE_INTEGER ProcessId,
                        IN AFSFileID     FileId,
                        IN BOOL bWow64,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    AFSByteRangeUnlockResultCB *pResultCB = NULL;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    afs_uint32  code;
    cm_req_t    req;
    cm_key_t    key;
    DWORD       status;

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

    osi_Log4(afsd_logp, "RDR_ByteRangeUnlockAll File FID cell=0x%x vol=0x%x vn=0x%x uniq=0x%x",
              FileId.Cell, FileId.Volume, 
              FileId.Vnode, FileId.Unique);

    Length = sizeof( AFSCommResult);
    if (Length > ResultBufferLength) {
        *ResultCB = (AFSCommResult *)malloc(sizeof(AFSCommResult));
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
    (*ResultCB)->ResultBufferLength = Length;

    /* Allocate the extents from the buffer package */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    code = cm_GetSCache(&Fid, &scp, userp, &req);
    if (code) {
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log2(afsd_logp, "RDR_ByteRangeUnlockAll cm_GetSCache FID failure code=0x%x status=0x%x",
                  code, status);
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_ByteRangeUnlockAll cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* the scp is now locked and current */
    key = cm_GenerateKey(CM_SESSION_IFS, ProcessId.QuadPart, 0);

    code = cm_UnlockByKey(scp, key, 0, userp, &req);

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_LOCK);
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    smb_MapNTError(code, &status);
    (*ResultCB)->ResultStatus = status;
    osi_Log0(afsd_logp, "RDR_ByteRangeUnlockAll SUCCESS");
    return;
        
}

void
RDR_GetVolumeInfo( IN cm_user_t     *userp,
                   IN ULARGE_INTEGER ProcessId,
                   IN AFSFileID     FileId,
                   IN BOOL bWow64,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB)
{       
    AFSVolumeInfoCB *pResultCB = NULL;
    DWORD       Length;
    cm_scache_t *scp = NULL;
    cm_volume_t *volp = NULL;
    cm_vol_state_t *volstatep = NULL;
    afs_uint32   volType;
    cm_cell_t   *cellp = NULL;
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

    RDR_InitReq(&req);
    if ( bWow64 )
        req.flags |= CM_REQ_WOW64;

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

    /* Allocate the extents from the buffer package */
    if (FileId.Cell != 0) {
        Fid.cell = FileId.Cell;
        Fid.volume = FileId.Volume;
        Fid.vnode = FileId.Vnode;
        Fid.unique = FileId.Unique;
        Fid.hash = FileId.Hash;

        code = cm_GetSCache(&Fid, &scp, userp, &req);
        if (code) {
            smb_MapNTError(code, &status);
            (*ResultCB)->ResultStatus = status;
            (*ResultCB)->ResultBufferLength = 0;
            osi_Log2(afsd_logp, "RDR_GetVolumeInfo cm_GetSCache FID failure code=0x%x status=0x%x",
                      code, status);
            return;
        }
    } else {
        /* If the SourceID.Cell == 0 then we are evaluating the root mount point */
        Fid = cm_data.rootFid;
        scp = cm_data.rootSCachep;
        cm_HoldSCache(scp);
    }
    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        (*ResultCB)->ResultBufferLength = 0;
        osi_Log3(afsd_logp, "RDR_GetVolumeInfo cm_SyncOp failure scp=0x%p code=0x%x status=0x%x",
                 scp, code, status);
        return;
    }

    /* Fake for now */
    pResultCB->SectorsPerAllocationUnit = 1;
    pResultCB->BytesPerSector = 1024;

    pResultCB->CellID = scp->fid.cell;
    pResultCB->VolumeID = scp->fid.volume;
    pResultCB->Characteristics = FILE_REMOTE_DEVICE;
    pResultCB->FileSystemAttributes = FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | 
        FILE_SUPPORTS_REPARSE_POINTS | FILE_VOLUME_QUOTAS;

    if (scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
         scp->fid.volume==AFS_FAKE_ROOT_VOL_ID) 
    {
        pResultCB->TotalAllocationUnits.QuadPart = 100;
        memcpy(&pResultCB->VolumeCreationTime, &ft, sizeof(ft));

        pResultCB->AvailableAllocationUnits.QuadPart = 0;
        pResultCB->Characteristics |= FILE_READ_ONLY_DEVICE;

        pResultCB->VolumeLabelLength = cm_Utf8ToUtf16( "Freelance.Local.Root", -1, pResultCB->VolumeLabel,
                                                       (sizeof(pResultCB->VolumeLabel) / sizeof(WCHAR)) + 1);
        if ( pResultCB->VolumeLabelLength )
            pResultCB->VolumeLabelLength--;
    } else {
        memcpy(&pResultCB->VolumeCreationTime, &ft, sizeof(ft));

        volp = cm_GetVolumeByFID(&scp->fid);
        if (!volp) {
            code = CM_ERROR_NOSUCHVOLUME;
            goto _done;
        }
        volstatep = cm_VolumeStateByID(volp, scp->fid.volume);
        volType = cm_VolumeType(volp, scp->fid.volume);

        pResultCB->Characteristics |= ((volType == ROVOL || volType == BACKVOL) ? FILE_READ_ONLY_DEVICE : 0);

        Name = volName;
	OfflineMsg = offLineMsg;
	MOTD = motd;
	do {
	    code = cm_ConnFromFID(&scp->fid, userp, &req, &connp);
	    if (code) continue;

	    rxconnp = cm_GetRxConn(connp);
	    code = RXAFS_GetVolumeStatus(rxconnp, scp->fid.volume,
					 &volStat, &Name, &OfflineMsg, &MOTD);
	    rx_PutConnection(rxconnp);

	} while (cm_Analyze(connp, userp, &req, &scp->fid, NULL, NULL, NULL, code));
	code = cm_MapRPCError(code, &req);
        if (code == 0) {
            pResultCB->TotalAllocationUnits.QuadPart = volStat.PartMaxBlocks;
            pResultCB->AvailableAllocationUnits.QuadPart = volStat.PartBlocksAvail;

            pResultCB->VolumeLabelLength = cm_Utf8ToUtf16( Name, -1, pResultCB->VolumeLabel, 
                                                           (sizeof(pResultCB->VolumeLabel) / sizeof(WCHAR)) + 1);
        } else {
            pResultCB->TotalAllocationUnits.QuadPart = 0x7FFFFFFF;
            pResultCB->AvailableAllocationUnits.QuadPart = (volType == ROVOL || volType == BACKVOL) ? 0 : 0x3F000000;

            pResultCB->VolumeLabelLength = cm_Utf8ToUtf16( volp->namep, -1, pResultCB->VolumeLabel, 
                                                           (sizeof(pResultCB->VolumeLabel) / sizeof(WCHAR)) + 1);
            code = 0;
        }
        if ( pResultCB->VolumeLabelLength )
            pResultCB->VolumeLabelLength--;
    }
    pResultCB->VolumeLabelLength *= sizeof(WCHAR);  /* convert to bytes from chars */
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

  _done:
    lock_ReleaseWrite(&scp->rw);
    if (volp)
       cm_PutVolume(volp);
    cm_ReleaseSCache(scp);

    smb_MapNTError(code, &status);
    (*ResultCB)->ResultStatus = status;
    osi_Log0(afsd_logp, "RDR_GetVolumeInfo SUCCESS");
    return;
}

