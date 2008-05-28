
#define _WIN32_WINNT 0x0500
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <windows.h>
#pragma warning(push)
#pragma warning(disable: 4005)
#include <ntstatus.h>

#include <devioctl.h>

#include "..\\Common\\AFSUserCommon.h"
#include <RDRPrototypes.h>


#pragma warning(pop)

#include <tchar.h>
#include <wchar.h>
#include <winbase.h>
#include <winreg.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>


#include "afsd.h"
#include "cm_btree.h"

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


DWORD
RDR_SetInitParams( OUT AFSCacheFileInfo **ppCacheFileInfo, OUT DWORD * pCacheFileInfoLen )
{
    extern char cm_CachePath[];
    extern cm_config_data_t cm_data;
    size_t cm_CachePathLen = strlen(cm_CachePath);
    size_t err;

    *pCacheFileInfoLen = sizeof(AFSCacheFileInfo) + (cm_CachePathLen) * sizeof(WCHAR);
    *ppCacheFileInfo = (AFSCacheFileInfo *)malloc(*pCacheFileInfoLen);
    (*ppCacheFileInfo)->CacheBlockSize = cm_data.blockSize;
    (*ppCacheFileInfo)->CacheFileNameLength = cm_CachePathLen * sizeof(WCHAR);
    err = mbstowcs((*ppCacheFileInfo)->CacheFileName, cm_CachePath, (cm_CachePathLen + 1) *sizeof(WCHAR));
    if (err == -1) {
        free(*ppCacheFileInfo);
        return STATUS_OBJECT_NAME_INVALID;
    }

    return 0;
}

cm_user_t *
RDR_UserFromCommRequest( IN AFSCommRequest *RequestBuffer)
{
    cm_user_t *userp = cm_rootUserp;
    cm_HoldUser(userp);
    return userp;
}

void
RDR_ReleaseUser( IN cm_user_t *userp )
{
    cm_ReleaseUser(userp);
}

void
RDR_EnumerateDirectory( IN cm_user_t *userp,
                        IN AFSFileID ParentID,
                        IN AFSDirQueryCB *QueryCB,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    cm_direnum_t *      enump;
    AFSDirEnumResp  * pDirEnumResp;
    AFSDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    afs_uint32  code = 0;
    cm_fid_t      fid;
    cm_scache_t * dscp = NULL;
    cm_req_t      req;

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB))
	return;

    cm_InitReq(&req);
    memset(*ResultCB, 0, size);

    (*ResultCB)->ResultBufferLength = ResultBufferLength;

    pDirEnumResp = (AFSDirEnumResp *)&(*ResultCB)->ResultData;
    pCurrentEntry = (AFSDirEnumEntry *)QuadAlign(&pDirEnumResp->Entry);

    if (ParentID.Cell != 0) {
        fid.cell   = ParentID.Cell;
        fid.volume = ParentID.Volume;
        fid.vnode  = ParentID.Vnode;
        fid.unique = ParentID.Unique;
        fid.hash   = ParentID.Hash;

        dscp = cm_FindSCache(&fid);
        if (dscp == NULL) {
            (*ResultCB)->ResultStatus = STATUS_NO_SUCH_FILE;
            return;
        }
    } else {
        fid = cm_data.rootFid;
        dscp = cm_data.rootSCachep;
        cm_HoldSCache(dscp);
    }

    /* get the directory size */
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        DWORD status;
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        return;
    }

    /*
     * If there is no enumeration handle, then this is a new query
     * and we must perform an enumeration for the specified object 
     */
    if (QueryCB->EnumHandle == (ULONG_PTR)NULL) {
        cm_dirOp_t    dirop;
        LARGE_INTEGER thyper;

        thyper.HighPart = thyper.LowPart = 0;

        code = cm_BeginDirOp(dscp, userp, &req, CM_DIRLOCK_READ, &dirop);
        if (code == 0) {
            code = cm_BPlusDirEnumerate(dscp, TRUE, NULL, &enump);

            if (code == 0) {
                lock_ObtainWrite(&dscp->rw);
                code = cm_TryBulkStat(dscp, &thyper, userp, &req);
                lock_ReleaseWrite(&dscp->rw);
            }

            cm_EndDirOp(&dirop);
        }
    } else {
        enump = (cm_direnum_t *)QueryCB->EnumHandle;
    }

    if (enump) {
        cm_direnum_entry_t    * entryp = NULL;

      getnextentry:
        code = cm_BPlusDirNextEnumEntry(enump, &entryp);

        if (code == 0 && entryp) {
            cm_scache_t *scp;

            if ( !strcmp(".", entryp->name) || !strcmp("..", entryp->name) )
                goto getnextentry;

            scp = cm_FindSCache(&entryp->fid);
            if (scp) {
                FILETIME ft;
                WCHAR *  wname, *wtarget;
                size_t   len;

                pCurrentEntry->FileId.Cell = scp->fid.cell;
                pCurrentEntry->FileId.Volume = scp->fid.volume;
                pCurrentEntry->FileId.Vnode = scp->fid.vnode;
                pCurrentEntry->FileId.Unique = scp->fid.unique;
                pCurrentEntry->FileId.Hash = scp->fid.hash;

                pCurrentEntry->DataVersion.QuadPart = scp->dataVersion;
                pCurrentEntry->FileType = scp->fileType;

                smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
                pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
                pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
                pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
                pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
                pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

                pCurrentEntry->EndOfFile = scp->length;
                pCurrentEntry->AllocationSize = scp->length;
                pCurrentEntry->FileAttributes = smb_ExtAttributes(scp);
                pCurrentEntry->EaSize = 0;
                pCurrentEntry->Links = scp->linkCount;


                len = strlen(entryp->shortName);
#ifdef UNICODE
                cch = MultiByteToWideChar(CP_UTF8, 0, entryp->shortname, 
                                          len * sizeof(char),
                                          pCurrentEntry->ShortName, 
                                          len * sizeof(WCHAR));
#else
                mbstowcs(pCurrentEntry->ShortName, entryp->shortName, len);
#endif
                pCurrentEntry->ShortNameLength = len * sizeof(WCHAR);

                pCurrentEntry->FileNameOffset = sizeof(AFSDirEnumEntry);
                len = strlen(entryp->name);
                wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);


#ifdef UNICODE
                cch = MultiByteToWideChar(CP_UTF8, 0, entryp->name, 
                                          len * sizeof(char),
                                          wname, 
                                          len * sizeof(WCHAR));
#else
                mbstowcs(wname, entryp->name, len);
#endif
                pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;

                switch (scp->fileType) {
                case CM_SCACHETYPE_MOUNTPOINT:
                    lock_ObtainWrite(&scp->rw);
                    if (cm_ReadMountPoint(scp, userp, &req) == 0) {
                        afs_uint32 code2;
                        cm_scache_t *targetScp = NULL;

                        pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                        len = strlen(scp->mountPointStringp);
                        wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                        cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                          len * sizeof(char),
                                          wtarget, 
                                          len * sizeof(WCHAR));
#else
                        mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                        pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                        code2 = cm_FollowMountPoint(scp, dscp, userp, &req, &targetScp);

                        if (code2 == 0) {
                            pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                            pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                            pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                            pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                            pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                            cm_ReleaseSCache(targetScp);
                        }
                    }
                    lock_ReleaseWrite(&scp->rw);
                    break;
                case CM_SCACHETYPE_SYMLINK:
                case CM_SCACHETYPE_DFSLINK:
                    {
                        afs_uint32 code2;
                        cm_scache_t *targetScp = NULL;

                        code2 = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                        if (code2 == 0) {
                            pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                            len = strlen(scp->mountPointStringp);
                            wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                            cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                                       len * sizeof(char),
                                                       wtarget, 
                                                       len * sizeof(WCHAR));
#else
                            mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                            pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                            pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                            pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                            pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                            pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                            pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                            cm_ReleaseSCache(targetScp);
                        }
                    }
                    break;
                default:
                    pCurrentEntry->TargetNameOffset = 0;
                    pCurrentEntry->TargetNameLength = 0;
                }

                cm_ReleaseSCache(scp);
            } else {
                code = STATUS_NO_SUCH_FILE;
            }
        }
    }

    if (code != 0 || enump->next == enump->count) {
        cm_BPlusDirFreeEnumeration(enump);
        enump = NULL;
        if (code != 0)
            (*ResultCB)->ResultStatus = STATUS_NO_MORE_ENTRIES;
        else
            (*ResultCB)->ResultStatus = STATUS_SUCCESS;
    } else 
        (*ResultCB)->ResultStatus = STATUS_MORE_ENTRIES;

    pDirEnumResp->EnumHandle = (ULONG_PTR) enump;

    if (dscp)
        cm_ReleaseSCache(dscp);

    return;
}

void
RDR_EvaluateNodeByName( IN cm_user_t *userp,
                        IN AFSFileID ParentID,
                        IN WCHAR   *Name,
                        IN DWORD    NameLength,
                        IN DWORD    CaseSensitive,
                        IN DWORD    ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{
    AFSDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    afs_uint32  code = 0;
    char aname[1025];
    cm_scache_t * scp = NULL;
    cm_scache_t * dscp = NULL;
    cm_req_t      req;
    cm_fid_t      parentFid;
    cm_dirOp_t    dirop;

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB))
	return;

    memset(*ResultCB, 0, size);
    (*ResultCB)->ResultBufferLength = ResultBufferLength;

    {
        wchar_t wname[1025];
        size_t err;

        wmemcpy(wname, Name, min(NameLength, 1024));
        wname[min(NameLength, 1024)] = '\0';

        err = wcstombs(aname, wname, sizeof(aname));
        if (err == -1) {
            (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
            return;
        }
    }

    pCurrentEntry = (AFSDirEnumEntry *)&(*ResultCB)->ResultData;


    cm_InitReq(&req);

    if (ParentID.Cell != 0) {
        parentFid.cell   = ParentID.Cell;
        parentFid.volume = ParentID.Volume;
        parentFid.vnode  = ParentID.Vnode;
        parentFid.unique = ParentID.Unique;
        parentFid.hash   = ParentID.Hash;

        dscp = cm_FindSCache(&parentFid);
        if (dscp == NULL) {
            (*ResultCB)->ResultStatus = STATUS_NO_SUCH_FILE;
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
        DWORD status;
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        return;
    }

    code = cm_BeginDirOp(dscp, userp, &req, CM_DIRLOCK_READ, &dirop);
    if (code == 0) {
        cm_fid_t fid;

        code = cm_BPlusDirLookup(&dirop, aname, &fid);
        cm_EndDirOp(&dirop);
        if (code == 0 || (code == CM_ERROR_INEXACT_MATCH && !CaseSensitive))
            scp = cm_FindSCache(&fid);
    }

    if (scp) {
        FILETIME ft;
        WCHAR *  wname, *wtarget;
        size_t   len;

        pCurrentEntry->FileId.Cell = scp->fid.cell;
        pCurrentEntry->FileId.Volume = scp->fid.volume;
        pCurrentEntry->FileId.Vnode = scp->fid.vnode;
        pCurrentEntry->FileId.Unique = scp->fid.unique;
        pCurrentEntry->FileId.Hash = scp->fid.hash;

        pCurrentEntry->DataVersion.QuadPart = scp->dataVersion;
        pCurrentEntry->FileType = scp->fileType;

        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
        pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
        pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
        pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
        pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

        pCurrentEntry->EndOfFile = scp->length;
        pCurrentEntry->AllocationSize = scp->length;
        pCurrentEntry->FileAttributes = smb_ExtAttributes(scp);
        pCurrentEntry->EaSize = 0;
        pCurrentEntry->Links = scp->linkCount;

        {
            char shortName[13];
            cm_dirFid_t dfid;

            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            cm_Gen8Dot3NameInt(aname, &dfid, shortName, NULL);
            len = strlen(shortName);
#ifdef UNICODE
            cch = MultiByteToWideChar(CP_UTF8, 0, shortName, 
                                       len * sizeof(char),
                                       pCurrentEntry->ShortName, 
                                       len * sizeof(WCHAR));
#else
            mbstowcs(pCurrentEntry->ShortName, shortName, len);
#endif
            pCurrentEntry->ShortNameLength = len * sizeof(WCHAR);
        }

        pCurrentEntry->FileNameOffset = sizeof(AFSDirEnumEntry);
        len = strlen(aname);
        wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);

#ifdef UNICODE
        cch = MultiByteToWideChar(CP_UTF8, 0, aname, 
                                   len * sizeof(char),
                                   wname, 
                                   len * sizeof(WCHAR));
#else
        mbstowcs(wname, aname, len);
#endif
        pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;

        switch (scp->fileType) {
        case CM_SCACHETYPE_MOUNTPOINT:
            lock_ObtainWrite(&scp->rw);
            if (cm_ReadMountPoint(scp, userp, &req) == 0) {
                afs_uint32 code2;
                cm_scache_t *targetScp = NULL;

                pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                len = strlen(scp->mountPointStringp);
                wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                           len * sizeof(char),
                                           wtarget, 
                                           len * sizeof(WCHAR));
#else
                mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                code2 = cm_FollowMountPoint(scp, dscp, userp, &req, &targetScp);

                if (code2 == 0) {
                    pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                    pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                    pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                    pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                    pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                    cm_ReleaseSCache(targetScp);
                }
            }
            lock_ReleaseWrite(&scp->rw);
            break;
        case CM_SCACHETYPE_SYMLINK:
        case CM_SCACHETYPE_DFSLINK:
            {
                afs_uint32 code2;
                cm_scache_t *targetScp = NULL;

                code2 = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                if (code2 == 0) {
                    pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                    len = strlen(scp->mountPointStringp);
                    wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                    cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                              len * sizeof(char),
                                              wtarget, 
                                              len * sizeof(WCHAR));
#else
                    mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                    pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                    pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                    pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                    pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                    pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                    pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                    cm_ReleaseSCache(targetScp);
                }
            }
            break;
        default:
            pCurrentEntry->TargetNameOffset = 0;
            pCurrentEntry->TargetNameLength = 0;
        }

        cm_ReleaseSCache(scp);
        (*ResultCB)->ResultStatus = STATUS_SUCCESS;
    } else if (code) {
        DWORD status;
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
    } else {
        (*ResultCB)->ResultStatus = STATUS_NO_SUCH_FILE;
    }
    cm_ReleaseSCache(dscp);

    return;
}

void
RDR_EvaluateNodeByID( IN cm_user_t *userp,
                      IN AFSFileID ParentID, 
                      IN AFSFileID SourceID,
                      IN DWORD    ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB)
{
    AFSDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(AFSCommResult) + ResultBufferLength - 1;
    afs_uint32  code = 0;
    char aname[1025] = "";
    cm_scache_t * scp = NULL;
    cm_scache_t * dscp = NULL;
    cm_req_t      req;
    cm_fid_t      Fid;
    cm_fid_t      parentFid;
    FILETIME ft;
    WCHAR *  wname, *wtarget;
    size_t   len;

    *ResultCB = (AFSCommResult *)malloc(size);
    if (!(*ResultCB))
	return;

    memset(*ResultCB, 0, size);
    (*ResultCB)->ResultBufferLength = ResultBufferLength;

    pCurrentEntry = (AFSDirEnumEntry *)&(*ResultCB)->ResultData;

    cm_InitReq(&req);

    if (ParentID.Cell != 0) {
        parentFid.cell   = ParentID.Cell;
        parentFid.volume = ParentID.Volume;
        parentFid.vnode  = ParentID.Vnode;
        parentFid.unique = ParentID.Unique;
        parentFid.hash   = ParentID.Hash;

        dscp = cm_FindSCache(&parentFid);
        if (dscp == NULL) {
            (*ResultCB)->ResultStatus = STATUS_NO_SUCH_FILE;
            return;
        }
    } else {
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

        scp = cm_FindSCache(&Fid);
        if (scp == NULL) {
            (*ResultCB)->ResultStatus = STATUS_NO_SUCH_FILE;
            cm_ReleaseSCache(dscp);
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
        DWORD status;
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        return;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        return;
    }

    /* Make sure the source vnode is current */
    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {     
        DWORD status;
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        return;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&scp->rw);

    pCurrentEntry->FileId.Cell = scp->fid.cell;
    pCurrentEntry->FileId.Volume = scp->fid.volume;
    pCurrentEntry->FileId.Vnode = scp->fid.vnode;
    pCurrentEntry->FileId.Unique = scp->fid.unique;
    pCurrentEntry->FileId.Hash = scp->fid.hash;

    pCurrentEntry->DataVersion.QuadPart = scp->dataVersion;
    pCurrentEntry->FileType = scp->fileType;

    smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
    pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
    pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
    pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
    pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
    pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

    pCurrentEntry->EndOfFile = scp->length;
    pCurrentEntry->AllocationSize = scp->length;
    pCurrentEntry->FileAttributes = smb_ExtAttributes(scp);
    pCurrentEntry->EaSize = 0;
    pCurrentEntry->Links = scp->linkCount;

    {
        char shortName[13]="";
        cm_dirFid_t dfid;

        dfid.vnode = htonl(scp->fid.vnode);
        dfid.unique = htonl(scp->fid.unique);

        cm_Gen8Dot3NameInt(aname, &dfid, shortName, NULL);
        len = strlen(shortName);
#ifdef UNICODE
        cch = MultiByteToWideChar(CP_UTF8, 0, shortName, 
                                   len * sizeof(char),
                                   pCurrentEntry->ShortName, 
                                   len * sizeof(WCHAR));
#else
        mbstowcs(pCurrentEntry->ShortName, shortName, len);
#endif
        pCurrentEntry->ShortNameLength = len * sizeof(WCHAR);
    }

    pCurrentEntry->FileNameOffset = sizeof(AFSDirEnumEntry);
    len = strlen(aname);
    wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);

#ifdef UNICODE
    cch = MultiByteToWideChar(CP_UTF8, 0, aname, 
                               len * sizeof(char),
                               wname
                               len * sizeof(WCHAR));
#else
    mbstowcs(wname, aname, len);
#endif
    pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;

    switch (scp->fileType) {
    case CM_SCACHETYPE_MOUNTPOINT:
        lock_ObtainWrite(&scp->rw);
        if (cm_ReadMountPoint(scp, userp, &req) == 0) {
            afs_uint32 code2;
            cm_scache_t *targetScp = NULL;

            pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
            len = strlen(scp->mountPointStringp);
            wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
            cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                       len * sizeof(char),
                                       wtarget, 
                                       len * sizeof(WCHAR));
#else
            mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
            pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

            code2 = cm_FollowMountPoint(scp, dscp, userp, &req, &targetScp);

            if (code2 == 0) {
                pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                cm_ReleaseSCache(targetScp);
            }
        }
        lock_ReleaseWrite(&scp->rw);
        break;
    case CM_SCACHETYPE_SYMLINK:
    case CM_SCACHETYPE_DFSLINK:
        {
            afs_uint32 code2;
            cm_scache_t *targetScp = NULL;

            code2 = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
            if (code2 == 0) {
                pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                len = strlen(scp->mountPointStringp);
                wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                           len * sizeof(char),
                                           wtarget, 
                                           len * sizeof(WCHAR));
#else
                mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                cm_ReleaseSCache(targetScp);
            }
        }
        break;
    default:
        pCurrentEntry->TargetNameOffset = 0;
        pCurrentEntry->TargetNameLength = 0;
    }

    cm_ReleaseSCache(scp);
    (*ResultCB)->ResultStatus = STATUS_SUCCESS;
    return;
}

void
RDR_CreateFileEntry( IN cm_user_t *userp,
                     IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN AFSFileCreateCB *CreateCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{
    AFSFileCreateResultCB *pResultCB = NULL;
    cm_fid_t            parentFid;
    afs_uint32          code;
    cm_scache_t *       dscp = NULL;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_scache_t *       scp = NULL;
    cm_req_t            req;
    char                utf8_name[1025];
    FILETIME            ft;
    WCHAR               *wname, *wtarget;
    size_t              len;

    cm_InitReq(&req);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof( AFSFileCreateResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof( AFSFileCreateResultCB));

    code = !WideCharToMultiByte(CP_UTF8, 0, FileName, FileNameLength, utf8_name, sizeof(utf8_name), NULL, NULL);
    if (code) {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        return;
    }

    parentFid.cell   = CreateCB->ParentId.Cell;
    parentFid.volume = CreateCB->ParentId.Volume;
    parentFid.vnode  = CreateCB->ParentId.Vnode;
    parentFid.unique = CreateCB->ParentId.Unique;
    parentFid.hash   = CreateCB->ParentId.Hash;

    dscp = cm_FindSCache(&parentFid);
    if (dscp == NULL) {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
        return;
    }

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        return;
    }

    setAttr.mask = CM_ATTRMASK_LENGTH;
    setAttr.length.LowPart = CreateCB->AllocationSize.LowPart;
    setAttr.length.HighPart = CreateCB->AllocationSize.HighPart;
    if (CreateCB->FileAttributes & FILE_ATTRIBUTE_READONLY) {
        setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
        setAttr.unixModeBits = 0222;
    }

    code = cm_Create(dscp, utf8_name, flags, &setAttr, &scp, userp, &req);

    if (code == 0) {
        AFSDirEnumEntry * pCurrentEntry;

        (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

        (*ResultCB)->ResultBufferLength = sizeof( AFSFileCreateResultCB);

        pResultCB = (AFSFileCreateResultCB *)(*ResultCB)->ResultData;

        pResultCB->FileId.Cell = scp->fid.cell;
        pResultCB->FileId.Volume = scp->fid.volume;
        pResultCB->FileId.Vnode = scp->fid.vnode;
        pResultCB->FileId.Unique = scp->fid.unique;
        pResultCB->FileId.Hash = scp->fid.hash;

        pResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;

        pCurrentEntry = &pResultCB->DirEnum;
        pCurrentEntry->FileId.Cell = scp->fid.cell;
        pCurrentEntry->FileId.Volume = scp->fid.volume;
        pCurrentEntry->FileId.Vnode = scp->fid.vnode;
        pCurrentEntry->FileId.Unique = scp->fid.unique;
        pCurrentEntry->FileId.Hash = scp->fid.hash;

        pCurrentEntry->DataVersion.QuadPart = scp->dataVersion;
        pCurrentEntry->FileType = scp->fileType;

        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
        pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
        pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
        pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
        pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

        pCurrentEntry->EndOfFile = scp->length;
        pCurrentEntry->AllocationSize = scp->length;
        pCurrentEntry->FileAttributes = smb_ExtAttributes(scp);
        pCurrentEntry->EaSize = 0;
        pCurrentEntry->Links = scp->linkCount;

        {
            char shortName[13];
            cm_dirFid_t dfid;

            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            cm_Gen8Dot3NameInt(utf8_name, &dfid, shortName, NULL);
            len = strlen(shortName);
#ifdef UNICODE
            cch = MultiByteToWideChar(CP_UTF8, 0, shortName, 
                                       len * sizeof(char),
                                       pCurrentEntry->ShortName, 
                                       len * sizeof(WCHAR));
#else
            mbstowcs(pCurrentEntry->ShortName, shortName, len);
#endif
            pCurrentEntry->ShortNameLength = len * sizeof(WCHAR);
        }

        pCurrentEntry->FileNameOffset = sizeof(AFSDirEnumEntry);
        len = strlen(utf8_name);
        wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);

#ifdef UNICODE
        cch = MultiByteToWideChar(CP_UTF8, 0, utf8_name, 
                                   len * sizeof(char),
                                   wname
                                   len * sizeof(WCHAR));
#else
        mbstowcs(wname, utf8_name, len);
#endif
        pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;

        switch (scp->fileType) {
        case CM_SCACHETYPE_MOUNTPOINT:
            lock_ObtainWrite(&scp->rw);
            if (cm_ReadMountPoint(scp, userp, &req) == 0) {
                afs_uint32 code2;
                cm_scache_t *targetScp = NULL;

                pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                len = strlen(scp->mountPointStringp);
                wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                           len * sizeof(char),
                                           wtarget, 
                                           len * sizeof(WCHAR));
#else
                mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                code2 = cm_FollowMountPoint(scp, dscp, userp, &req, &targetScp);

                if (code2 == 0) {
                    pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                    pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                    pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                    pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                    pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                    cm_ReleaseSCache(targetScp);
                }
            }
            lock_ReleaseWrite(&scp->rw);
            break;
        case CM_SCACHETYPE_SYMLINK:
        case CM_SCACHETYPE_DFSLINK:
            {
                afs_uint32 code2;
                cm_scache_t *targetScp = NULL;

                code2 = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                if (code2 == 0) {
                    pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                    len = strlen(scp->mountPointStringp);
                    wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                    cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                               len * sizeof(char),
                                               wtarget, 
                                               len * sizeof(WCHAR));
#else
                    mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                    pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                    pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                    pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                    pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                    pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                    pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                    cm_ReleaseSCache(targetScp);
                }
            }
            break;
        default:
            pCurrentEntry->TargetNameOffset = 0;
            pCurrentEntry->TargetNameLength = 0;
        }


        cm_ReleaseSCache(scp);
    } else {
        DWORD               NTStatus;
        smb_MapNTError(code, &NTStatus);
        (*ResultCB)->ResultStatus = NTStatus;
        (*ResultCB)->ResultBufferLength = 0;
    }

    cm_ReleaseSCache(dscp);

    return;
}

void
RDR_UpdateFileEntry( IN cm_user_t *userp,
                     IN AFSFileID FileId,
                     IN AFSFileUpdateCB *UpdateCB,
                     IN OUT AFSCommResult **ResultCB)
{
    AFSFileUpdateResultCB *pResultCB = NULL;
    cm_fid_t            Fid;
    cm_fid_t            parentFid;
    afs_uint32          code;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_scache_t *       scp = NULL;
    cm_scache_t *       dscp = NULL;
    cm_req_t            req;
    char                aname[1025];
    FILETIME            ft;
    WCHAR               *wname, *wtarget;
    size_t              len;
    time_t              clientModTime;

    cm_InitReq(&req);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof( AFSFileUpdateResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof( AFSFileUpdateResultCB));

    parentFid.cell   = UpdateCB->ParentId.Cell;
    parentFid.volume = UpdateCB->ParentId.Volume;
    parentFid.vnode  = UpdateCB->ParentId.Vnode;
    parentFid.unique = UpdateCB->ParentId.Unique;
    parentFid.hash   = UpdateCB->ParentId.Hash;

    dscp = cm_FindSCache(&parentFid);
    if (dscp == NULL) {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
        return;
    }

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        return;
    }

    Fid.cell   = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode  = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash   = FileId.Hash;

    scp = cm_FindSCache(&Fid);
    if (scp == NULL) {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
        cm_ReleaseSCache(dscp);
        return;
    }

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    if (code) {
        DWORD               NTStatus;
        lock_ReleaseWrite(&scp->rw);
        smb_MapNTError(code, &NTStatus);
        (*ResultCB)->ResultStatus = NTStatus;
        (*ResultCB)->ResultBufferLength = 0;
        cm_ReleaseSCache(dscp);
        cm_ReleaseSCache(scp);
        return;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);


    /* TODO - Deal with Truncation properly */
    if (scp->length.QuadPart != UpdateCB->AllocationSize.QuadPart) {
        setAttr.mask |= CM_ATTRMASK_LENGTH;
        setAttr.length.LowPart = UpdateCB->AllocationSize.LowPart;
        setAttr.length.HighPart = UpdateCB->AllocationSize.HighPart;
    }

    if ((scp->unixModeBits & 0222) && !(UpdateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
        setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
        setAttr.unixModeBits &= ~0222;
    } else if (!(scp->unixModeBits & 0222) && (UpdateCB->FileAttributes & FILE_ATTRIBUTE_READONLY)) {
        setAttr.mask |= CM_ATTRMASK_UNIXMODEBITS;
        setAttr.unixModeBits |= 0222;
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

    if (code == 0) {
        AFSDirEnumEntry * pCurrentEntry;

        (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

        (*ResultCB)->ResultBufferLength = sizeof( AFSFileUpdateResultCB);

        pResultCB = (AFSFileUpdateResultCB *)(*ResultCB)->ResultData;

        pCurrentEntry = &pResultCB->DirEnum;
        pCurrentEntry->FileId.Cell = scp->fid.cell;
        pCurrentEntry->FileId.Volume = scp->fid.volume;
        pCurrentEntry->FileId.Vnode = scp->fid.vnode;
        pCurrentEntry->FileId.Unique = scp->fid.unique;
        pCurrentEntry->FileId.Hash = scp->fid.hash;

        pCurrentEntry->DataVersion.QuadPart = scp->dataVersion;
        pCurrentEntry->FileType = scp->fileType;

        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        pCurrentEntry->CreationTime.LowPart = ft.dwLowDateTime;
        pCurrentEntry->CreationTime.HighPart = ft.dwHighDateTime;
        pCurrentEntry->LastAccessTime = pCurrentEntry->CreationTime;
        pCurrentEntry->LastWriteTime = pCurrentEntry->CreationTime;
        pCurrentEntry->ChangeTime = pCurrentEntry->CreationTime;

        pCurrentEntry->EndOfFile = scp->length;
        pCurrentEntry->AllocationSize = scp->length;
        pCurrentEntry->FileAttributes = smb_ExtAttributes(scp);
        pCurrentEntry->EaSize = 0;
        pCurrentEntry->Links = scp->linkCount;

        {
            char shortName[13];
            cm_dirFid_t dfid;

            dfid.vnode = htonl(scp->fid.vnode);
            dfid.unique = htonl(scp->fid.unique);

            cm_Gen8Dot3NameInt(aname, &dfid, shortName, NULL);
            len = strlen(shortName);
#ifdef UNICODE
            cch = MultiByteToWideChar(CP_UTF8, 0, shortName, 
                                       len * sizeof(char),
                                       pCurrentEntry->ShortName, 
                                       len * sizeof(WCHAR));
#else
            mbstowcs(pCurrentEntry->ShortName, shortName, len);
#endif
            pCurrentEntry->ShortNameLength = len * sizeof(WCHAR);
        }

        pCurrentEntry->FileNameOffset = sizeof(AFSDirEnumEntry);
        len = strlen(aname);
        wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);

#ifdef UNICODE
        cch = MultiByteToWideChar(CP_UTF8, 0, aname, 
                                   len * sizeof(char),
                                   wname
                                   len * sizeof(WCHAR));
#else
        mbstowcs(wname, aname, len);
#endif
        pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;

        switch (scp->fileType) {
        case CM_SCACHETYPE_MOUNTPOINT:
            lock_ObtainWrite(&scp->rw);
            if (cm_ReadMountPoint(scp, userp, &req) == 0) {
                afs_uint32 code2;
                cm_scache_t *targetScp = NULL;

                pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                len = strlen(scp->mountPointStringp);
                wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                           len * sizeof(char),
                                           wtarget, 
                                           len * sizeof(WCHAR));
#else
                mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                code2 = cm_FollowMountPoint(scp, dscp, userp, &req, &targetScp);

                if (code2 == 0) {
                    pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                    pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                    pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                    pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                    pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                    cm_ReleaseSCache(targetScp);
                }
            }
            lock_ReleaseWrite(&scp->rw);
            break;
        case CM_SCACHETYPE_SYMLINK:
        case CM_SCACHETYPE_DFSLINK:
            {
                afs_uint32 code2;
                cm_scache_t *targetScp = NULL;

                code2 = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                if (code2 == 0) {
                    pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                    len = strlen(scp->mountPointStringp);
                    wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);

#ifdef UNICODE
                    cch = MultiByteToWideChar(CP_UTF8, 0, scp->mountPointStringp, 
                                               len * sizeof(char),
                                               wtarget, 
                                               len * sizeof(WCHAR));
#else
                    mbstowcs(wtarget, scp->mountPointStringp, len);
#endif
                    pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;

                    pCurrentEntry->TargetFileId.Cell = targetScp->fid.cell;
                    pCurrentEntry->TargetFileId.Volume = targetScp->fid.volume;
                    pCurrentEntry->TargetFileId.Vnode = targetScp->fid.vnode;
                    pCurrentEntry->TargetFileId.Unique = targetScp->fid.unique;
                    pCurrentEntry->TargetFileId.Hash = targetScp->fid.hash;

                    cm_ReleaseSCache(targetScp);
                }
            }
            break;
        default:
            pCurrentEntry->TargetNameOffset = 0;
            pCurrentEntry->TargetNameLength = 0;
        }
    } else {
        DWORD               NTStatus;
        smb_MapNTError(code, &NTStatus);
        (*ResultCB)->ResultStatus = NTStatus;
        (*ResultCB)->ResultBufferLength = 0;
    }
    cm_ReleaseSCache(scp);
    cm_ReleaseSCache(dscp);

    return;
}

void
RDR_DeleteFileEntry( IN cm_user_t *userp,
                     IN AFSFileID ParentId,
                     IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN OUT AFSCommResult **ResultCB)
{

    AFSFileDeleteResultCB *pResultCB = NULL;
    cm_fid_t            parentFid;
    afs_uint32          code;
    cm_scache_t *       dscp = NULL;
    afs_uint32          flags = 0;
    cm_attr_t           setAttr;
    cm_req_t            req;
    char                utf8_norm[1025];
    char                utf8_name[1025];

    cm_InitReq(&req);
    memset(&setAttr, 0, sizeof(cm_attr_t));

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof( AFSFileDeleteResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof( AFSFileDeleteResultCB));

    code = !WideCharToMultiByte(CP_UTF8, 0, FileName, FileNameLength, utf8_name, sizeof(utf8_name), NULL, NULL);
    if (code) {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        return;
    }

    code = !cm_NormalizeUtf16StringToUtf8(FileName, FileNameLength, utf8_norm, sizeof(utf8_norm));
    if (code) {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_NAME_INVALID;
        return;
    }

    parentFid.cell   = ParentId.Cell;
    parentFid.volume = ParentId.Volume;
    parentFid.vnode  = ParentId.Vnode;
    parentFid.unique = ParentId.Unique;
    parentFid.hash   = ParentId.Hash;

    dscp = cm_FindSCache(&parentFid);
    if (dscp == NULL) {
        (*ResultCB)->ResultStatus = STATUS_OBJECT_PATH_INVALID;
        return;
    }

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        (*ResultCB)->ResultStatus = STATUS_NOT_A_DIRECTORY;
        cm_ReleaseSCache(dscp);
        return;
    }

    code = cm_Unlink(dscp, utf8_name, utf8_norm, userp, &req);

    if (code == 0) {
        (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

        (*ResultCB)->ResultBufferLength = sizeof( AFSFileDeleteResultCB);

        pResultCB = (AFSFileDeleteResultCB *)(*ResultCB)->ResultData;

        pResultCB->ParentDataVersion.QuadPart = dscp->dataVersion;

    } else {
        DWORD               NTStatus;
        smb_MapNTError(code, &NTStatus);
        (*ResultCB)->ResultStatus = NTStatus;
        (*ResultCB)->ResultBufferLength = 0;
    }

    cm_ReleaseSCache(dscp);

    return;
}

void
RDR_RenameFileEntry( IN cm_user_t *userp,
                     IN AFSFileID FileId,
                     IN AFSFileRenameCB *RenameCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB)
{

    AFSFileRenameResultCB *pResultCB = NULL;

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof( AFSFileRenameResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof( AFSFileRenameResultCB));

    pResultCB = (AFSFileRenameResultCB *)(*ResultCB)->ResultData;
    
    (*ResultCB)->ResultStatus = 0;

    (*ResultCB)->ResultBufferLength = sizeof( AFSFileRenameResultCB);


    return;
}

void
RDR_FlushFileEntry( IN cm_user_t *userp,
                    IN AFSFileID FileId,
                    IN DWORD ResultBufferLength,
                    IN OUT AFSCommResult **ResultCB)
{

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Flush the dirty buffers associated with FID 
     * What do we do about extents that are currently held by the redirector?
     * How do I know what to flush?
     */
    
    (*ResultCB)->ResultStatus = 0;

    (*ResultCB)->ResultBufferLength = 0;


    return;
}

void
RDR_OpenFileEntry( IN cm_user_t *userp,
                   IN AFSFileID FileId,
                   IN AFSFileOpenCB *OpenCB,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB)
{

    AFSFileOpenResultCB *pResultCB = NULL;

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof( AFSFileOpenResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof( AFSFileOpenResultCB));

    pResultCB = (AFSFileOpenResultCB *)(*ResultCB)->ResultData;

    /* The following specifies the access that has actually been granted 
     * for the specified file.  This may be different from what was 
     * requested in OpenCB->DesiredAccess.
     *
     * TODO - compute the actual access based upon the user's permissions
     * for OpenCB->ParentId and FileId.
     */
    pResultCB->GrantedAccess = OpenCB->DesiredAccess;
    
    (*ResultCB)->ResultStatus = 0;

    (*ResultCB)->ResultBufferLength = sizeof( AFSFileOpenResultCB);


    return;
}

void
RDR_RequestFileExtentsSync( IN cm_user_t *userp,
                        IN AFSFileID FileId,
                        IN AFSRequestExtentsCB *RequestExtentsCB,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB)
{

    AFSRequestExtentsResultCB *pResultCB = NULL;

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult) + sizeof( AFSRequestExtentsResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult) + sizeof( AFSRequestExtentsResultCB));

    /* Allocate the extents from the buffer package */

    pResultCB = (AFSRequestExtentsResultCB *)(*ResultCB)->ResultData;
    
    (*ResultCB)->ResultStatus = 0;

    (*ResultCB)->ResultBufferLength = sizeof( AFSRequestExtentsResultCB);


    return;
}

void
RDR_RequestFileExtentsAsync( IN cm_user_t *userp,
                             IN AFSFileID FileId,
                             IN AFSRequestExtentsCB *RequestExtentsCB,
                             IN OUT DWORD * ResultBufferLength,
                             IN OUT AFSSetFileExtentsCB **ResultCB)
{
    AFSSetFileExtentsCB *pResultCB = NULL;
    DWORD Length;
    DWORD count;
    cm_scache_t *scp = NULL;
    cm_fid_t    Fid;
    cm_buf_t    *bufp;
    afs_uint32  code;
    osi_hyper_t thyper;
    LARGE_INTEGER ByteOffset, EndOffset;
    cm_req_t    req;

    cm_InitReq(&req);

    Length = sizeof( AFSSetFileExtentsCB) + sizeof( AFSRequestExtentsResultCB) * (RequestExtentsCB->Length / cm_data.blockSize + 1);

    *ResultCB = (AFSSetFileExtentsCB *)malloc( Length );
    if (*ResultCB == NULL) {
        *ResultBufferLength = 0;
        return;
    }
    *ResultBufferLength = Length;

    memset( *ResultCB, '\0', Length );

    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    scp = cm_FindSCache(&Fid);
    if (scp == NULL) {
        free(*ResultCB);
        *ResultCB = NULL;
        *ResultBufferLength = 0;
        return;
    }

    lock_ObtainWrite(&scp->rw);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        free(*ResultCB);
        *ResultCB = NULL;
        *ResultBufferLength = 0;
        cm_ReleaseSCache(scp);
        return;
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* the scp is now locked and current */

    (*ResultCB)->FileID = FileId;

    /* Allocate the extents from the buffer package */
    for ( count = 0, ByteOffset = RequestExtentsCB->ByteOffset, EndOffset.QuadPart = ByteOffset.QuadPart + Length; 
          ByteOffset.QuadPart < EndOffset.QuadPart; 
          ByteOffset.QuadPart += cm_data.blockSize)
    {
        thyper.QuadPart = ByteOffset.QuadPart;

        lock_ReleaseWrite(&scp->rw);
        code = buf_Get(scp, &thyper, &bufp);
        lock_ObtainWrite(&scp->rw);
        
        if (code == 0) {
            lock_ObtainMutex(&bufp->mx);
            bufp->flags |= CM_BUF_REDIR;
            lock_ReleaseMutex(&bufp->mx);

            /* now get the data in the cache */
            if (ByteOffset.QuadPart < scp->length.QuadPart) {
                while (1) {
                    code = cm_SyncOp(scp, bufp, userp, &req, 0,
                                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);
                    if (code) 
                        break;

                    cm_SyncOpDone(scp, bufp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);

                    if (cm_HaveBuffer(scp, bufp, 0)) 
                        break;

                    /* otherwise, load the buffer and try again */
                    code = cm_GetBuffer(scp, bufp, NULL, userp, &req);
                    if (code) 
                        break;
                }
            } else {
                memset(bufp->datap, 0, cm_data.blockSize);
            }

            (*ResultCB)->FileExtents[count].Flags = 0;
            (*ResultCB)->FileExtents[count].FileOffset = ByteOffset;
            (*ResultCB)->FileExtents[count].CacheOffset.QuadPart = bufp->datap - cm_data.baseAddress;
            (*ResultCB)->FileExtents[count].Length = cm_data.blockSize;
            count++;
            buf_Release(bufp);
        }
    }
    (*ResultCB)->ExtentCount = count;
    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

    return;
}



void
RDR_ReleaseFileExtents( IN cm_user_t *userp,
                        IN AFSFileID FileId,
                        IN AFSReleaseExtentsCB *ReleaseExtentsCB,
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

    cm_InitReq(&req);

    *ResultCB = (AFSCommResult *)malloc( sizeof( AFSCommResult));

    memset( *ResultCB,
            '\0',
            sizeof( AFSCommResult));

    /* Process the release */
    Fid.cell = FileId.Cell;
    Fid.volume = FileId.Volume;
    Fid.vnode = FileId.Vnode;
    Fid.unique = FileId.Unique;
    Fid.hash = FileId.Hash;

    scp = cm_FindSCache(&Fid);
    if (scp == NULL) {
        free(*ResultCB);
        *ResultCB = NULL;
        return;
    }

    for ( count = 0; count < ReleaseExtentsCB->ExtentCount; count++) {
        thyper.QuadPart = ReleaseExtentsCB->FileExtents[count].FileOffset.QuadPart;

        code = buf_Get(scp, &thyper, &bufp);
        if (code == 0) {
            if (ReleaseExtentsCB->FileExtents[count].Flags) {
                lock_ObtainMutex(&bufp->mx);
                if ( ReleaseExtentsCB->FileExtents[count].Flags & AFS_EXTENT_FLAG_RELEASE )
                    bufp->flags &= ~CM_BUF_REDIR;
                if ( ReleaseExtentsCB->FileExtents[count].Flags & AFS_EXTENT_FLAG_DIRTY ) {
                    bufp->flags |= CM_BUF_DIRTY;
                    dirty = 1;
                }
                lock_ReleaseMutex(&bufp->mx);
            }
            buf_Release(bufp);
        }
    }

    if (dirty)
        code = cm_FlushFile(scp, userp, &req);

    cm_ReleaseSCache(scp);

    if (code) {
        DWORD status;
        smb_MapNTError(code, &status);
        (*ResultCB)->ResultStatus = status;
    } else
        (*ResultCB)->ResultStatus = 0;

    (*ResultCB)->ResultBufferLength = 0;

    return;
}

