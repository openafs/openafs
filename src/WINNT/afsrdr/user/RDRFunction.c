
#define _WIN32_WINNT 0x0500
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <windows.h>
#pragma warning(push)
#pragma warning(disable: 4005)
#include <ntstatus.h>

#include <devioctl.h>

#include "..\\Common\\KDFsdUserCommon.h"
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



void
RDR_EnumerateDirectory( IN KDFileID ParentID,
                        IN KDFsdDirQueryCB *QueryCB,
                        IN DWORD ResultBufferLength,
                        IN OUT KDFsdCommResult **ResultCB)
{
    cm_direnum_t *      enump;
    KDFsdDirEnumResp  * pDirEnumResp;
    KDFsdDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(KDFsdCommResult) + ResultBufferLength - 1;
    afs_uint32  code = 0;
    cm_fid_t      fid;
    cm_scache_t * dscp = NULL;
    cm_user_t *   userp = cm_rootUserp;
    cm_req_t      req;

    *ResultCB = (KDFsdCommResult *)malloc(size);
    if (!(*ResultCB))
	return;

    cm_InitReq(&req);
    memset(*ResultCB, 0, size);

    (*ResultCB)->ResultBufferLength = ResultBufferLength;

    pDirEnumResp = (KDFsdDirEnumResp *)&(*ResultCB)->ResultData;
    pCurrentEntry = (KDFsdDirEnumEntry *)QuadAlign(&pDirEnumResp->Entry);

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

                pCurrentEntry->DataVersion = scp->dataVersion;
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

                pCurrentEntry->ShortNameLength = strlen(entryp->shortName);
                mbstowcs(pCurrentEntry->ShortName, entryp->shortName, pCurrentEntry->ShortNameLength);

                pCurrentEntry->FileNameOffset = sizeof(KDFsdDirEnumEntry);
                len = strlen(entryp->name);
                pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;
                wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);
                mbstowcs(wname, entryp->name, len);

                switch (scp->fileType) {
                case CM_SCACHETYPE_MOUNTPOINT:
                    lock_ObtainWrite(&scp->rw);
                    if (cm_ReadMountPoint(scp, userp, &req) == 0) {
                        afs_uint32 code2;
                        cm_scache_t *targetScp = NULL;

                        pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                        len = strlen(scp->mountPointStringp);
                        pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;
                        wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);
                        mbstowcs(wtarget, scp->mountPointStringp, len);

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
                            pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;
                            wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);
                            mbstowcs(wtarget, scp->mountPointStringp, len);

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
RDR_EvaluateNodeByName( IN KDFileID ParentID,
                        IN WCHAR   *Name,
                        IN DWORD    NameLength,
                        IN DWORD    CaseSensitive,
                        IN DWORD    ResultBufferLength,
                        IN OUT KDFsdCommResult **ResultCB)
{
    KDFsdDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(KDFsdCommResult) + ResultBufferLength - 1;
    afs_uint32  code = 0;
    char aname[1025];
    cm_scache_t * scp = NULL;
    cm_scache_t * dscp = NULL;
    cm_user_t *   userp = cm_rootUserp;
    cm_req_t      req;
    cm_fid_t      parentFid;
    cm_dirOp_t    dirop;

    *ResultCB = (KDFsdCommResult *)malloc(size);
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

    pCurrentEntry = (KDFsdDirEnumEntry *)&(*ResultCB)->ResultData;


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

        pCurrentEntry->DataVersion = scp->dataVersion;
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

            pCurrentEntry->ShortNameLength = strlen(shortName);
            mbstowcs(pCurrentEntry->ShortName, shortName, pCurrentEntry->ShortNameLength);
        }

        pCurrentEntry->FileNameOffset = sizeof(KDFsdDirEnumEntry);
        len = strlen(aname);
        pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;
        wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);
        mbstowcs(wname, aname, len);

        switch (scp->fileType) {
        case CM_SCACHETYPE_MOUNTPOINT:
            lock_ObtainWrite(&scp->rw);
            if (cm_ReadMountPoint(scp, userp, &req) == 0) {
                afs_uint32 code2;
                cm_scache_t *targetScp = NULL;

                pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
                len = strlen(scp->mountPointStringp);
                pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;
                wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);
                mbstowcs(wtarget, scp->mountPointStringp, len);

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
                    pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;
                    wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);
                    mbstowcs(wtarget, scp->mountPointStringp, len);

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
RDR_EvaluateNodeByID( IN KDFileID ParentID, 
                      IN KDFileID SourceID,
                      IN DWORD    ResultBufferLength,
                      IN OUT KDFsdCommResult **ResultCB)
{
    KDFsdDirEnumEntry * pCurrentEntry;
    size_t size = sizeof(KDFsdCommResult) + ResultBufferLength - 1;
    afs_uint32  code = 0;
    char aname[1025];
    cm_scache_t * scp = NULL;
    cm_scache_t * dscp = NULL;
    cm_user_t *   userp = cm_rootUserp;
    cm_req_t      req;
    cm_fid_t      Fid;
    cm_fid_t      parentFid;
    FILETIME ft;
    WCHAR *  wname, *wtarget;
    size_t   len;

    *ResultCB = (KDFsdCommResult *)malloc(size);
    if (!(*ResultCB))
	return;

    memset(*ResultCB, 0, size);
    (*ResultCB)->ResultBufferLength = ResultBufferLength;

    pCurrentEntry = (KDFsdDirEnumEntry *)&(*ResultCB)->ResultData;

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

    pCurrentEntry->DataVersion = scp->dataVersion;
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

        pCurrentEntry->ShortNameLength = strlen(shortName);
        mbstowcs(pCurrentEntry->ShortName, shortName, pCurrentEntry->ShortNameLength);
    }

    pCurrentEntry->FileNameOffset = sizeof(KDFsdDirEnumEntry);
    len = strlen(aname);
    pCurrentEntry->FileNameLength = sizeof(WCHAR) * len;
    wname = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->FileNameOffset);
    mbstowcs(wname, aname, len);

    switch (scp->fileType) {
    case CM_SCACHETYPE_MOUNTPOINT:
        lock_ObtainWrite(&scp->rw);
        if (cm_ReadMountPoint(scp, userp, &req) == 0) {
            afs_uint32 code2;
            cm_scache_t *targetScp = NULL;

            pCurrentEntry->TargetNameOffset = pCurrentEntry->FileNameOffset + pCurrentEntry->FileNameLength;
            len = strlen(scp->mountPointStringp);
            pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;
            wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);
            mbstowcs(wtarget, scp->mountPointStringp, len);

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
                pCurrentEntry->TargetNameLength = sizeof(WCHAR) * len;
                wtarget = (WCHAR *)((PBYTE)pCurrentEntry + pCurrentEntry->TargetNameOffset);
                mbstowcs(wtarget, scp->mountPointStringp, len);

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
RDR_CreateFileEntry( IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN KDFsdFileCreateCB *CreateCB,
                     IN DWORD ResultBufferLength,
                     IN OUT KDFsdCommResult **ResultCB)
{
    KDFsdFileCreateResultCB *pResultCB = NULL;

    *ResultCB = (KDFsdCommResult *)malloc( sizeof( KDFsdCommResult) + sizeof( KDFsdFileCreateResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( KDFsdCommResult) + sizeof( KDFsdFileCreateResultCB));

    (*ResultCB)->ResultStatus = 0;  // We will be able to fit all the data in here

    (*ResultCB)->ResultBufferLength = sizeof( KDFsdFileCreateResultCB);

    pResultCB = (KDFsdFileCreateResultCB *)(*ResultCB)->ResultData;


    return;
}

void
RDR_UpdateFileEntry( IN KDFileID FileId,
                     IN KDFsdFileUpdateCB *UpdateCB,
                     IN OUT KDFsdCommResult **ResultCB)
{

    *ResultCB = (KDFsdCommResult *)malloc( sizeof( KDFsdCommResult));

    memset( *ResultCB,
            '\0',
            sizeof( KDFsdCommResult));
    
    (*ResultCB)->ResultStatus = 0;


    return;
}

void
RDR_DeleteFileEntry( IN KDFileID FileId,
                     IN OUT KDFsdCommResult **ResultCB)
{

    *ResultCB = (KDFsdCommResult *)malloc( sizeof( KDFsdCommResult));

    memset( *ResultCB,
            '\0',
            sizeof( KDFsdCommResult));
    
    (*ResultCB)->ResultStatus = 0;


    return;
}

void
RDR_RenameFileEntry( IN KDFileID FileId,
                     IN KDFsdFileRenameCB *RenameCB,
                     IN DWORD ResultBufferLength,
                     IN OUT KDFsdCommResult **ResultCB)
{

    KDFsdFileRenameResultCB *pResultCB = NULL;

    *ResultCB = (KDFsdCommResult *)malloc( sizeof( KDFsdCommResult) + sizeof( KDFsdFileRenameResultCB));

    memset( *ResultCB,
            '\0',
            sizeof( KDFsdCommResult) + sizeof( KDFsdFileRenameResultCB));

    pResultCB = (KDFsdFileRenameResultCB *)(*ResultCB)->ResultData;
    
    (*ResultCB)->ResultStatus = 0;

    (*ResultCB)->ResultBufferLength = sizeof( KDFsdFileRenameResultCB);


    return;
}
