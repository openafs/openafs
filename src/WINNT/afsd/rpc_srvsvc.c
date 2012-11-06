/*
 * Copyright (c) 2009 Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include<windows.h>
#include <wchar.h>
#define _CRT_RAND_S
#include <stdlib.h>
#include <strsafe.h>
#include <svrapi.h>
#include "ms-srvsvc.h"
#include "msrpc.h"
#include "afsd.h"
#include "smb.h"
#include <WINNT/afsreg.h>
#define AFS_VERSION_STRINGS
#include "AFS_component_version_number.h"

#pragma warning( disable: 4027 )  /* func w/o formal parameter list */

/* Do not pull in lmserver.h */
//
// The platform ID indicates the levels to use for platform-specific
// information.
//
#define SV_PLATFORM_ID_DOS 300
#define SV_PLATFORM_ID_OS2 400
#define SV_PLATFORM_ID_NT  500
#define SV_PLATFORM_ID_OSF 600
#define SV_PLATFORM_ID_VMS 700
#define SV_PLATFORM_ID_AFS 800
//
//      Bit-mapped values for svX_type fields. X = 1, 2 or 3.
//
#define SV_TYPE_WORKSTATION         0x00000001
#define SV_TYPE_SERVER              0x00000002
#define SV_TYPE_SQLSERVER           0x00000004
#define SV_TYPE_DOMAIN_CTRL         0x00000008
#define SV_TYPE_DOMAIN_BAKCTRL      0x00000010
#define SV_TYPE_TIME_SOURCE         0x00000020
#define SV_TYPE_AFP                 0x00000040
#define SV_TYPE_NOVELL              0x00000080
#define SV_TYPE_DOMAIN_MEMBER       0x00000100
#define SV_TYPE_PRINTQ_SERVER       0x00000200
#define SV_TYPE_DIALIN_SERVER       0x00000400
#define SV_TYPE_XENIX_SERVER        0x00000800
#define SV_TYPE_SERVER_UNIX         SV_TYPE_XENIX_SERVER
#define SV_TYPE_NT                  0x00001000
#define SV_TYPE_WFW                 0x00002000
#define SV_TYPE_SERVER_MFPN         0x00004000
#define SV_TYPE_SERVER_NT           0x00008000
#define SV_TYPE_POTENTIAL_BROWSER   0x00010000
#define SV_TYPE_BACKUP_BROWSER      0x00020000
#define SV_TYPE_MASTER_BROWSER      0x00040000
#define SV_TYPE_DOMAIN_MASTER       0x00080000
#define SV_TYPE_SERVER_OSF          0x00100000
#define SV_TYPE_SERVER_VMS          0x00200000
#define SV_TYPE_WINDOWS             0x00400000  /* Windows95 and above */
#define SV_TYPE_DFS                 0x00800000  /* Root of a DFS tree */
#define SV_TYPE_CLUSTER_NT          0x01000000  /* NT Cluster */
#define SV_TYPE_TERMINALSERVER      0x02000000  /* Terminal Server(Hydra) */
#define SV_TYPE_CLUSTER_VS_NT       0x04000000  /* NT Cluster Virtual Server Name */
#define SV_TYPE_DCE                 0x10000000  /* IBM DSS (Directory and Security Services) or equivalent */
#define SV_TYPE_ALTERNATE_XPORT     0x20000000  /* return list for alternate transport */
#define SV_TYPE_LOCAL_LIST_ONLY     0x40000000  /* Return local list only */
#define SV_TYPE_DOMAIN_ENUM         0x80000000
#define SV_TYPE_ALL                 0xFFFFFFFF  /* handy for NetServerEnum2 */
//
//      Values of svX_hidden field. X = 2 or 3.
//
#define SV_HIDDEN       1
#define SV_VISIBLE      0

NET_API_STATUS NetrConnectionEnum(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][string][in] */ WCHAR *Qualifier,
    /* [out][in] */ LPCONNECT_ENUM_STRUCT InfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ DWORD *TotalEntries,
    /* [unique][out][in] */ DWORD *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrConnectionEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrFileEnum(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][string][in] */ WCHAR *BasePath,
    /* [unique][string][in] */ WCHAR *UserName,
    /* [out][in] */ PFILE_ENUM_STRUCT InfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ DWORD *TotalEntries,
    /* [unique][out][in] */ DWORD *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrFileEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrFileGetInfo(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD FileId,
    /* [in] */ DWORD Level,
    /* [switch_is][out] */ LPFILE_INFO InfoStruct)
{
    osi_Log0(afsd_logp, "NetrFileGetInfo not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrFileClose(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD FileId)
{
    osi_Log0(afsd_logp, "NetrFileClose not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrSessionEnum(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][string][in] */ WCHAR *ClientName,
    /* [unique][string][in] */ WCHAR *UserName,
    /* [out][in] */ PSESSION_ENUM_STRUCT InfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ DWORD *TotalEntries,
    /* [unique][out][in] */ DWORD *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrSessionEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrSessionDel(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][string][in] */ WCHAR *ClientName,
    /* [unique][string][in] */ WCHAR *UserName)
{
    osi_Log0(afsd_logp, "NetrSessionDel not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareAdd(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPSHARE_INFO InfoStruct,
    /* [unique][out][in] */ DWORD *ParmErr)
{
    osi_Log0(afsd_logp, "NetrShareAdd not supported");
    return ERROR_NOT_SUPPORTED;
}

static wchar_t *
NetrIntGenerateShareRemark(cm_scache_t *scp, cm_fid_t *fidp)
{
    wchar_t remark[1536], *s, *wcellname, *wmp;
    HRESULT hr;
    cm_cell_t *cellp;
    afs_int32 b;

    cellp = cm_FindCellByID(fidp->cell, CM_FLAG_NOPROBE);

    if (cellp)
        wcellname = cm_FsStringToClientStringAlloc(cellp->name, -1, NULL);
    else
        wcellname = L"";

    if (scp) {
        switch (scp->fileType) {
        case 1:
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS File (%s:%u.%u.%u)",
                                   wcellname,
                                   scp->fid.volume,
                                   scp->fid.vnode, scp->fid.unique);
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS File");
            break;
        case 2:
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS Directory (%s:%u.%u.%u)",
                                   wcellname,
                                   scp->fid.volume,
                                   scp->fid.vnode, scp->fid.unique);
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS Directory");
            break;
        case 3:
            wmp = cm_FsStringToClientStringAlloc(scp->mountPointStringp, -1, NULL);
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS Symlink to %s",
                                   wmp ? wmp : L"");
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS Symlink");
            if (wmp)
                free(wmp);
            break;
        case 4:
            b = (strchr(scp->mountPointStringp, ':') != NULL);
            wmp = cm_FsStringToClientStringAlloc(scp->mountPointStringp, -1, NULL);
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS MountPoint to %s%s%s%s",
                                   wmp ? wmp : L"",
                                   b ? L"" : L" (",
                                   b ? L"" : wcellname,
                                   b ? L"" : L")");
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS MountPoint");
            if (wmp)
                free(wmp);
            break;
        case 5:
            wmp = cm_FsStringToClientStringAlloc(scp->mountPointStringp, -1, NULL);
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS Dfslink to %s",
                                   wmp ? wmp : L"");
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS DfsLink");
            if (wmp)
                free(wmp);
            break;
        default:
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS Object (%s:%u.%u.%u)",
                                   wcellname,
                                   scp->fid.volume,
                                   scp->fid.vnode, scp->fid.unique);
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS Object");
        }
    } else {
        if (fidp->vnode & 1) {
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS Directory (%s:%u.%u.%u)",
                                   wcellname,
                                   fidp->volume,
                                   fidp->vnode, fidp->unique);
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS Directory");
        } else {
            hr = StringCchPrintfW( remark,
                                   sizeof(remark)/sizeof(remark[0]),
                                   L"AFS Object (%s:%u.%u.%u)",
                                   wcellname,
                                   fidp->volume,
                                   fidp->vnode, fidp->unique);
            if (hr == S_OK)
                s = wcsdup(remark);
            else
                s = wcsdup(L"AFS Object");
        }
    }
    if (cellp && wcellname)
        free(wcellname);
    return s;
}

static wchar_t *
NetrIntGenerateSharePath(wchar_t *ServerName, cm_fid_t *fidp)
{
    wchar_t remark[1536], *s, *wcellname;
    HRESULT hr;
    cm_cell_t *cellp;

    cellp = cm_FindCellByID(fidp->cell, CM_FLAG_NOPROBE);

    if (cellp)
        wcellname = cm_FsStringToClientStringAlloc(cellp->name, -1, NULL);
    else
        wcellname = L"";

    for ( s=ServerName; *s == '\\' || *s == '/'; s++);
    hr = StringCchPrintfW( remark,
                           sizeof(remark)/sizeof(remark[0]),
                           L"\\\\%s\\%s\\%u.%u.%u",
                           s, wcellname,
                           fidp->volume,
                           fidp->vnode, fidp->unique);
    if (hr == S_OK)
        s = wcsdup(remark);
    else
        s = wcsdup(L"");

    if (cellp && wcellname)
        free(wcellname);
    return s;
}

typedef struct netr_share_enum {
    osi_queue_t   q;
    time_t        cleanup_time;
    DWORD         handle;
    cm_direnum_t *direnump;
} netr_share_enum_t;

static osi_queue_t * shareEnumQ = NULL;
static osi_mutex_t   shareEnum_mx;
static DWORD         shareEnum_next_handle=1;

void
RPC_SRVSVC_Init(void)
{
#if _MSC_VER >= 1400
    int hasRand_s = -1;
    OSVERSIONINFO osInfo;

    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
    if (GetVersionEx(&osInfo)) {
        if ((osInfo.dwMajorVersion > 5) ||
             (osInfo.dwMajorVersion == 5) && (osInfo.dwMinorVersion >= 1))
            hasRand_s = 1;
        else
            hasRand_s = 0;
    }
#endif

    lock_InitializeMutex(&shareEnum_mx, "NetrShareEnum", 0);
    shareEnumQ = NULL;
#if _MSC_VER >= 1400
    if (hasRand_s > 0) {
        rand_s(&shareEnum_next_handle);
    } else
#endif
    {
        srand((unsigned) time( NULL ));
        shareEnum_next_handle = rand();
    }
}

void
RPC_SRVSVC_Shutdown(void)
{
    netr_share_enum_t *enump;

    lock_ObtainMutex(&shareEnum_mx);
    while (shareEnumQ) {
        enump = (netr_share_enum_t *)shareEnumQ;
        cm_BPlusDirFreeEnumeration(enump->direnump);
        osi_QRemove(&shareEnumQ, (osi_queue_t *)enump);
        free(enump);
    }
    lock_FinalizeMutex(&shareEnum_mx);
}

static void
RPC_SRVSVC_ShareEnumAgeCheck(void) {
    netr_share_enum_t *enump, *enumnp;
    time_t            now;

    lock_ObtainMutex(&shareEnum_mx);
    now = time(NULL);
    for (enump = (netr_share_enum_t *)shareEnumQ;
         enump; enump = enumnp) {
        enumnp = (netr_share_enum_t *) osi_QNext(&(enump->q));
        if (now > enump->cleanup_time)
            osi_QRemove(&shareEnumQ, (osi_queue_t *)enump);
    }
    lock_ReleaseMutex(&shareEnum_mx);
}

static cm_direnum_t *
RPC_SRVSVC_ShareEnumFind(DWORD ResumeHandle) {
    netr_share_enum_t *enump;

    lock_ObtainMutex(&shareEnum_mx);
    for (enump = (netr_share_enum_t *)shareEnumQ;
         enump;
         enump = (netr_share_enum_t *) osi_QNext(&enump->q)) {
        if (ResumeHandle == enump->handle)
            break;
    }
    lock_ReleaseMutex(&shareEnum_mx);

    return enump ? enump->direnump : NULL;
}

static DWORD
RPC_SRVSVC_ShareEnumSave(cm_direnum_t *direnump) {
    netr_share_enum_t *enump = (netr_share_enum_t *)malloc(sizeof(netr_share_enum_t));

    if (enump == NULL)
        return 0xFFFFFFFF;

    lock_ObtainMutex(&shareEnum_mx);
    enump->cleanup_time = time(NULL) + 300;
    enump->handle = shareEnum_next_handle++;
    if (shareEnum_next_handle == 0xFFFFFFFF)
        shareEnum_next_handle = 1;
    enump->direnump = direnump;
    osi_QAdd(&shareEnumQ, (osi_queue_t *)enump);
    lock_ReleaseMutex(&shareEnum_mx);

    return enump->handle;
}

static void
RPC_SRVSVC_ShareEnumRemove(cm_direnum_t *direnump) {
    netr_share_enum_t *enump;

    lock_ObtainMutex(&shareEnum_mx);
    for (enump = (netr_share_enum_t *)shareEnumQ;
         enump;
         enump = (netr_share_enum_t *) osi_QNext(&enump->q)) {
        if (direnump == enump->direnump) {
            osi_QRemove(&shareEnumQ, (osi_queue_t *)enump);
            break;
        }
    }
    lock_ReleaseMutex(&shareEnum_mx);
}

NET_API_STATUS NetrShareEnum(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [out][in] */ LPSHARE_ENUM_STRUCT InfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ DWORD *TotalEntries,
    /* [unique][out][in] */ DWORD *ResumeHandle)
{
    cm_direnum_t *enump = NULL;
    cm_direnum_entry_t * entryp = NULL;
    cm_scache_t * dscp;
    cm_user_t *userp = MSRPC_GetCmUser();
    cm_req_t      req;
    int           stopnow = 0;
    afs_uint32    count = 0;
    afs_uint32    code;
    afs_uint32    old_enum = 0;
    size_t        space_limited = 0;
    size_t        space_available = 0;
    afs_uint32    first_entry = 1;

    osi_Log1(afsd_logp, "NetrShareEnum level %u", InfoStruct->Level);

    cm_InitReq(&req);

    dscp = cm_RootSCachep(userp, &req);

    RPC_SRVSVC_ShareEnumAgeCheck();

    /* We only support one server name so ignore 'ServerName'. */

    /* Test for unsupported level early */
    switch (InfoStruct->Level) {
    case 2:
    case 1:
    case 0:
        break;
    default:
        return ERROR_INVALID_LEVEL;
    }

    if (ResumeHandle && *ResumeHandle == 0xFFFFFFFF) {
        /* No More Entries */
        InfoStruct->ShareInfo.Level0->EntriesRead = 0;
        return ERROR_NO_MORE_FILES;
    }

    /* get the directory size */
    cm_HoldSCache(dscp);
    lock_ObtainWrite(&dscp->rw);
    code = cm_SyncOp(dscp, NULL, userp, &req, PRSFS_LOOKUP,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        lock_ReleaseWrite(&dscp->rw);
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "NetShareEnum cm_SyncOp failure code=0x%x", code);
        return ERROR_BUSY;
    }

    cm_SyncOpDone(dscp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&dscp->rw);

    if (dscp->fileType != CM_SCACHETYPE_DIRECTORY) {
        cm_ReleaseSCache(dscp);
        osi_Log1(afsd_logp, "NetShareEnum Not a Directory dscp=0x%p", dscp);
        return ERROR_DIRECTORY;
    }

    /*
     * If there is no enumeration handle, then this is a new query
     * and we must perform an enumeration for the specified object
     */
    if (ResumeHandle == NULL || *ResumeHandle == 0) {
        cm_dirOp_t    dirop;

        code = cm_BeginDirOp(dscp, userp, &req, CM_DIRLOCK_READ,
                             CM_DIROP_FLAG_NONE, &dirop);
        if (code == 0) {
            code = cm_BPlusDirEnumerate(dscp, userp, &req, TRUE, NULL, TRUE, &enump);
            if (code) {
                osi_Log1(afsd_logp, "NetShareEnum cm_BPlusDirEnumerate failure code=0x%x",
                          code);
            }
            cm_EndDirOp(&dirop);
        } else {
            osi_Log1(afsd_logp, "NetShareEnum cm_BeginDirOp failure code=0x%x",
                      code);
        }
        old_enum = 0;
    } else {
        enump = RPC_SRVSVC_ShareEnumFind(*ResumeHandle);
        old_enum = 1;
    }

    if (enump == NULL) {
        cm_ReleaseSCache(dscp);
        osi_Log0(afsd_logp, "NetShareEnum No Enumeration Present");
        *TotalEntries = 0;
        return (code ? ERROR_BUSY : ERROR_SUCCESS);
    }

    /*
     * Do not exceed PreferredMaximumLength in any response.
     * 0xFFFFFFFF means no limit.
     *
     * *TotalEntries is a hint of the total entries in the
     * enumeration.  It does not have to be exact.  It is
     * the same for each response in an enum.
     *
     * If *ResumeHandle == 0, this is an initial call
     *
     * The algorithm will be to enumerate the list of share
     * names based upon the contents of the root.afs root
     * directory.  We will ignore SMB submount names.
     * The enumeration will be valid for five minutes.  If
     * the enumeration is not destroyed within that time period
     * it will be automatically garbage collected.
     */

    /*
     * How much space do we need and do we have that much room?
     *
     * If a preferred maximum length is specified, then we must
     * manage the number of bytes we return in order to restrict
     * it to a value smaller than then the preferred.  This includes
     * not only the InfoStruct but also the allocated SHARE_INFO
     * buffer and the various C strings.  We are not permitted to
     * send incomplete values so we will send a value that is close.
     */
    if (PreferedMaximumLength != 0xFFFFFFFF) {
        space_limited = 1;
        space_available =  PreferedMaximumLength;
    }

    switch (InfoStruct->Level) {
    case 2:
        if (space_limited)
            space_available -= sizeof(InfoStruct->ShareInfo.Level2);
        InfoStruct->ShareInfo.Level2->Buffer = MIDL_user_allocate((enump->count - enump->next) * sizeof(SHARE_INFO_2));
        break;
    case 1:
        if (space_limited)
            space_available -= sizeof(InfoStruct->ShareInfo.Level1);
        InfoStruct->ShareInfo.Level1->Buffer = MIDL_user_allocate((enump->count - enump->next) * sizeof(SHARE_INFO_1));
        break;
    case 0:
        if (space_limited)
            space_available -= sizeof(InfoStruct->ShareInfo.Level0);
        InfoStruct->ShareInfo.Level0->Buffer = MIDL_user_allocate((enump->count - enump->next) * sizeof(SHARE_INFO_0));
        break;
    }

    if (InfoStruct->ShareInfo.Level0->Buffer == NULL) {
        cm_ReleaseSCache(dscp);
        cm_BPlusDirFreeEnumeration(enump);
        osi_Log0(afsd_logp, "NetShareEnum No Enumeration Present");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    while (!stopnow) {
        cm_scache_t *scp;

        if (space_limited)
            code = cm_BPlusDirPeekNextEnumEntry(enump, &entryp);
        else
            code = cm_BPlusDirNextEnumEntry(enump, &entryp);
        if (code != 0 && code != CM_ERROR_STOPNOW || entryp == NULL)
            break;

        stopnow = (code == CM_ERROR_STOPNOW);

        if ( !wcscmp(L".", entryp->name) || !wcscmp(L"..", entryp->name) ) {
            osi_Log0(afsd_logp, "NetShareEnum skipping . or ..");
            if (space_limited)
                cm_BPlusDirNextEnumEntry(enump, &entryp);
            continue;
        }

        cm_GetSCache(&entryp->fid, &scp, userp, &req);

        switch (InfoStruct->Level) {
        case 2:
            /* for share level security */
            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_permissions = 0;
            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_max_uses = -1;
            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_current_uses = 0;
            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_path =
                NetrIntGenerateSharePath(ServerName, &entryp->fid);
            /* must be the empty string */
            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_passwd = wcsdup(L"");

            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_type = STYPE_DISKTREE;
            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_remark =
                NetrIntGenerateShareRemark(scp, &entryp->fid);
            InfoStruct->ShareInfo.Level2->Buffer[count].shi2_netname = wcsdup(entryp->name);
            break;
        case 1:
            InfoStruct->ShareInfo.Level1->Buffer[count].shi1_type = STYPE_DISKTREE;
            InfoStruct->ShareInfo.Level1->Buffer[count].shi1_remark =
                NetrIntGenerateShareRemark(scp, &entryp->fid);
            InfoStruct->ShareInfo.Level1->Buffer[count].shi1_netname = wcsdup(entryp->name);
            break;
        case 0:
            InfoStruct->ShareInfo.Level0->Buffer[count].shi0_netname = wcsdup(entryp->name);
        }

        if (scp)
            cm_ReleaseSCache(scp);

        if (space_limited) {
            size_t space_used;
            /*
             * If space is limited, we need to determine if there is room
             * for the next entry but always send at least one.
             */
            switch (InfoStruct->Level) {
            case 2:
                space_used = sizeof(InfoStruct->ShareInfo.Level2->Buffer[count]) +
                    (wcslen(InfoStruct->ShareInfo.Level2->Buffer[count].shi2_path) + 1) * sizeof(wchar_t) +
                    sizeof(wchar_t) +  /* passwd */
                    (wcslen(InfoStruct->ShareInfo.Level2->Buffer[count].shi2_remark) + 1) * sizeof(wchar_t) +
                    (wcslen(InfoStruct->ShareInfo.Level2->Buffer[count].shi2_netname) + 1) * sizeof(wchar_t);
                break;
            case 1:
                space_used = sizeof(InfoStruct->ShareInfo.Level1->Buffer[count]) +
                    (wcslen(InfoStruct->ShareInfo.Level1->Buffer[count].shi1_remark) + 1) * sizeof(wchar_t) +
                    (wcslen(InfoStruct->ShareInfo.Level1->Buffer[count].shi1_netname) + 1) * sizeof(wchar_t);
                break;
            case 0:
                space_used = sizeof(InfoStruct->ShareInfo.Level0->Buffer[count]) +
                    (wcslen(InfoStruct->ShareInfo.Level0->Buffer[count].shi0_netname) + 1) * sizeof(wchar_t);
                break;
            }

            if (!first_entry && space_used > space_available)
                break;

            /* Now consume the entry */
            cm_BPlusDirNextEnumEntry(enump, &entryp);
            space_available -= space_used;
        }
        count++;
        first_entry = 0;
    }
    InfoStruct->ShareInfo.Level0->EntriesRead = count;
    *TotalEntries = enump->count;
    if (dscp)
        cm_ReleaseSCache(dscp);

    if (code || enump->next == enump->count || ResumeHandle == NULL) {
        if (old_enum)
            RPC_SRVSVC_ShareEnumRemove(enump);
        cm_BPlusDirFreeEnumeration(enump);
        if (ResumeHandle)
            *ResumeHandle = 0xFFFFFFFF;
        return (code || enump->next == enump->count) ? 0 : ERROR_MORE_DATA;
    } else {
        *ResumeHandle = RPC_SRVSVC_ShareEnumSave(enump);
        return ERROR_MORE_DATA;
    }
}

NET_API_STATUS NetrShareGetInfo(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *NetName,
    /* [in] */ DWORD Level,
    /* [switch_is][out] */ LPSHARE_INFO InfoStruct)
{
    cm_scache_t * dscp;
    cm_scache_t * scp = NULL;
    cm_user_t *userp = MSRPC_GetCmUser();
    cm_req_t      req;
    afs_uint32    code = 0;
    NET_API_STATUS status = 0;
    HKEY parmKey;
    /* make room for '/@vol:' + mountchar + NULL terminator*/
    clientchar_t pathstr[CELL_MAXNAMELEN + VL_MAXNAMELEN + 1 + CM_PREFIX_VOL_CCH];
    DWORD  cblen;

    osi_Log1(afsd_logp, "NetrShareGetInfo level %u", Level);

    cm_InitReq(&req);

    dscp = cm_RootSCachep(userp, &req);

    /* Allocate the memory for the response */
    switch (Level) {
    case 2:
        InfoStruct->ShareInfo2 = MIDL_user_allocate(sizeof(SHARE_INFO_2));
        break;
    case 1:
        InfoStruct->ShareInfo1 = MIDL_user_allocate(sizeof(SHARE_INFO_1));
        break;
    case 0:
        InfoStruct->ShareInfo0 = MIDL_user_allocate(sizeof(SHARE_INFO_0));
        break;
    }

    if (InfoStruct->ShareInfo0 == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    /*
     * NetName will be:
     *  1.a an entry in the root.afs volume root directory
     *  1.b an entry in the Freelance volume root directory
     *  2. A volume reference: <cell><type><volume>
     *  3. an SMB Submount name
     *  4. a cell name that we do not yet have an entry
     *     for in the Freelance volume root directory
     */

    /*
     * To obtain the needed information for the response we
     * need to obtain the cm_scache_t entry.  First check to
     * see if the NetName is a volume reference, then check
     * if the NetName is an entry in the root volume root
     * directory, finally we can check the SMB Submounts.
     * Volume references and SMB submounts are not advertised
     * as part of the Share enumerations but we should provide
     * Share info for them nevertheless.  Same for the secret
     * "all" share.
     */

    /* Check for volume references
     *
     * They look like <cell>{%,#}<volume>
     */
    if (cm_ClientStrChr(NetName, '%') != NULL ||
        cm_ClientStrChr(NetName, '#') != NULL)
    {
        osi_Log1(afsd_logp, "NetrShareGetInfo found volume reference [%S]",
                 osi_LogSaveClientString(afsd_logp, NetName));

        cm_ClientStrPrintfN(pathstr, lengthof(pathstr),
                            _C(CM_PREFIX_VOL) _C("%s"), NetName);
        code = cm_Lookup(dscp, pathstr, 0, userp, &req, &scp);
    } else if (cm_ClientStrCmpI(NetName, L"ALL") == 0) {
        DWORD allSubmount = 1;

        /* if allSubmounts == 0, only return the //mountRoot/all share
         * if in fact it has been been created in the subMounts table.
         * This is to allow sites that want to restrict access to the
         * world to do so.
         */
        code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                             0, KEY_QUERY_VALUE, &parmKey);
        if (code == ERROR_SUCCESS) {
            cblen = sizeof(allSubmount);
            code = RegQueryValueEx(parmKey, "AllSubmount", NULL, NULL,
                                    (BYTE *) &allSubmount, &cblen);
            if (code != ERROR_SUCCESS) {
                allSubmount = 1;
            }
            RegCloseKey (parmKey);
        }

        if (allSubmount) {
            scp = dscp;
            cm_HoldSCache(scp);
        }
    } else {
        /*
         * Could be a Submount, a directory entry, or a cell name we
         * have yet to create an entry for.
         */

        /* Try a directory entry first */
        code = cm_Lookup(dscp, NetName, CM_FLAG_NOMOUNTCHASE,
                          userp, &req, &scp);
        if (code && code != CM_ERROR_NOACCESS)
            code = cm_Lookup(dscp, NetName, CM_FLAG_CASEFOLD | CM_FLAG_NOMOUNTCHASE,
                              userp, &req, &scp);

        if (scp == NULL && code != CM_ERROR_NOACCESS) {  /* Try a submount */
            code = RegOpenKeyExW(HKEY_LOCAL_MACHINE,  L"Software\\OpenAFS\\Client\\Submounts",
                                  0, KEY_QUERY_VALUE, &parmKey);
            if (code == ERROR_SUCCESS) {
                cblen = sizeof(pathstr);
                code = RegQueryValueExW(parmKey, NetName, NULL, NULL,
                                         (BYTE *) pathstr, &cblen);
                if (code != ERROR_SUCCESS)
                    cblen = 0;
                RegCloseKey (parmKey);
            } else {
                cblen = 0;
            }

            if (cblen) {
                code = cm_NameI(dscp, pathstr, CM_FLAG_FOLLOW, userp, NULL, &req, &scp);
                if (code == CM_ERROR_NOSUCHFILE ||
                    code == CM_ERROR_NOSUCHPATH ||
                    code == CM_ERROR_BPLUS_NOMATCH)
                    code = cm_NameI(dscp, pathstr, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW,
                                    userp, NULL, &req, &scp);
            }
        }
    }

    if (scp) {
        switch (Level) {
        case 2:
            /* for share level security */
            InfoStruct->ShareInfo2->shi2_permissions = 0;
            InfoStruct->ShareInfo2->shi2_max_uses = -1;
            InfoStruct->ShareInfo2->shi2_current_uses = 0;
            InfoStruct->ShareInfo2->shi2_path =
                NetrIntGenerateSharePath(ServerName, &scp->fid);
            /* must be the empty string */
            InfoStruct->ShareInfo2->shi2_passwd = wcsdup(L"");
            /* fall-through */
        case 1:
            InfoStruct->ShareInfo1->shi1_type = STYPE_DISKTREE;
            InfoStruct->ShareInfo1->shi1_remark =
                NetrIntGenerateShareRemark(scp, &scp->fid);
            /* fall-through */
        case 0:
            /* Canonicalized version of NetName parameter */
            InfoStruct->ShareInfo0->shi0_netname = wcsdup(NetName);
            break;
        case 501:
        case 502:
        case 503:
        case 1004:
        case 1005:
        case 1006:
        default:
            status = ERROR_INVALID_LEVEL;
        }
        cm_ReleaseSCache(scp);
    } else {
        /*
         * The requested object does not exist.
         * Return the correct NERR or Win32 Error.
         */
        smb_MapWin32Error(code, &status);
        switch (status) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_BAD_NETPATH:
            status = NERR_NetNameNotFound;
            break;
        }
    }
    return status;
}

NET_API_STATUS NetrShareSetInfo(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *NetName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPSHARE_INFO ShareInfo,
    /* [unique][out][in] */ DWORD *ParmErr)
{
    osi_Log0(afsd_logp, "NetrShareSetInfo not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareDel(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *NetName,
    /* [in] */ DWORD Reserved)
{
    osi_Log0(afsd_logp, "NetrShareDel not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareDelSticky(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *NetName,
    /* [in] */ DWORD Reserved)
{
    osi_Log0(afsd_logp, "NetrShareDelSticky not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareCheck(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *Device,
    /* [out] */ DWORD *Type)
{
    osi_Log0(afsd_logp, "NetrShareCheck not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerGetInfo(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][out] */ LPSERVER_INFO InfoStruct)
{
    wchar_t *s;

    osi_Log1(afsd_logp, "NetrServerGetInfo level %u", Level);
    /*
     * How much space do we need and do we have that much room?
     * For now, just assume we can return everything in one shot
     * because the reality is that in this function call we do
     * not know the max size of the RPC response.
     */
    switch (Level) {
    case 103:
        InfoStruct->ServerInfo103 = MIDL_user_allocate(sizeof(SERVER_INFO_103));
        break;
    case 102:
        InfoStruct->ServerInfo102 = MIDL_user_allocate(sizeof(SERVER_INFO_102));
        break;
    case 101:
        InfoStruct->ServerInfo101 = MIDL_user_allocate(sizeof(SERVER_INFO_101));
        break;
    case 100:
        InfoStruct->ServerInfo100 = MIDL_user_allocate(sizeof(SERVER_INFO_100));
        break;
    }

    if (InfoStruct->ServerInfo100 == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    /*
     * Remove any leading slashes since they are not part of the
     * server name.
     */
    for ( s=ServerName; *s == '\\' || *s == '/'; s++);

    switch (Level) {
    case 103:
        InfoStruct->ServerInfo103->sv103_capabilities = 0;
        /* fall-through */
    case 102:
        InfoStruct->ServerInfo102->sv102_users = 0xFFFFFFFF;
        InfoStruct->ServerInfo102->sv102_disc = SV_NODISC;
        InfoStruct->ServerInfo102->sv102_hidden = SV_HIDDEN;
        InfoStruct->ServerInfo102->sv102_announce = 65535;
        InfoStruct->ServerInfo102->sv102_anndelta = 0;
        InfoStruct->ServerInfo102->sv102_licenses = 0;
        InfoStruct->ServerInfo102->sv102_userpath = wcsdup(L"C:\\");
        /* fall-through */
    case 101:
        InfoStruct->ServerInfo101->sv101_version_major = AFSPRODUCT_VERSION_MAJOR;
        InfoStruct->ServerInfo101->sv101_version_minor = AFSPRODUCT_VERSION_MINOR;
        InfoStruct->ServerInfo101->sv101_type = SV_TYPE_WORKSTATION | SV_TYPE_SERVER | SV_TYPE_SERVER_UNIX;
        InfoStruct->ServerInfo101->sv101_comment = wcsdup(wAFSVersion);
        /* fall-through */
    case 100:
        InfoStruct->ServerInfo100->sv100_platform_id = SV_PLATFORM_ID_AFS;
        /* The Netbios Name */
        InfoStruct->ServerInfo100->sv100_name = _wcsupr(wcsdup(s));
        return 0;
    case 502:
    case 503:
    case 599:
    case 1005:
    case 1107:
    case 1010:
    case 1016:
    case 1017:
    case 1018:
    case 1501:
    case 1502:
    case 1503:
    case 1506:
    case 1510:
    case 1511:
    case 1512:
    case 1513:
    case 1514:
    case 1515:
    case 1516:
    case 1518:
    case 1523:
    case 1528:
    case 1529:
    case 1530:
    case 1533:
    case 1534:
    case 1535:
    case 1536:
    case 1538:
    case 1539:
    case 1540:
    case 1541:
    case 1542:
    case 1543:
    case 1544:
    case 1545:
    case 1546:
    case 1547:
    case 1548:
    case 1549:
    case 1550:
    case 1552:
    case 1553:
    case 1554:
    case 1555:
    case 1556:
    default:
        return ERROR_INVALID_LEVEL;
    }
    return 0;
}

NET_API_STATUS NetrServerSetInfo(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPSERVER_INFO ServerInfo,
    /* [unique][out][in] */ DWORD *ParmErr)
{
    osi_Log0(afsd_logp, "NetrServerSetInfo not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerDiskEnum(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [out][in] */ DISK_ENUM_CONTAINER *DiskInfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ DWORD *TotalEntries,
    /* [unique][out][in] */ DWORD *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrServerDiskEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerStatisticsGet(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][string][in] */ WCHAR *Service,
    /* [in] */ DWORD Level,
    /* [in] */ DWORD Options,
    /* [out] */ LPSTAT_SERVER_0 *InfoStruct)
{
    osi_Log0(afsd_logp, "NetrServerStatisticsGet not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerTransportAdd(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [in] */ LPSERVER_TRANSPORT_INFO_0 Buffer)
{
    osi_Log0(afsd_logp, "NetrServerTransportAdd not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerTransportEnum(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [out][in] */ LPSERVER_XPORT_ENUM_STRUCT InfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ DWORD *TotalEntries,
    /* [unique][out][in] */ DWORD *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrServerTransportEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerTransportDel(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [in] */ LPSERVER_TRANSPORT_INFO_0 Buffer)
{
    osi_Log0(afsd_logp, "NetrServerTransportDel not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrRemoteTOD(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [out] */ LPTIME_OF_DAY_INFO *BufferPtr)
{
    osi_Log0(afsd_logp, "NetrRemoteTOD not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetprPathType(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *PathName,
    /* [out] */ DWORD *PathType,
    /* [in] */ DWORD Flags)
{
    osi_Log0(afsd_logp, "NetprPathType not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetprPathCanonicalize(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *PathName,
    /* [size_is][out] */ unsigned char *Outbuf,
    /* [range][in] */ DWORD OutbufLen,
    /* [string][in] */ WCHAR *Prefix,
    /* [out][in] */ DWORD *PathType,
    /* [in] */ DWORD Flags)
{
    osi_Log0(afsd_logp, "NetprPathCanonicalize not supported");
    return ERROR_NOT_SUPPORTED;
}

long NetprPathCompare(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *PathName1,
    /* [string][in] */ WCHAR *PathName2,
    /* [in] */ DWORD PathType,
    /* [in] */ DWORD Flags)
{
    osi_Log0(afsd_logp, "NetprPathCompare not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetprNameValidate(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *Name,
    /* [in] */ DWORD NameType,
    /* [in] */ DWORD Flags)
{
    osi_Log0(afsd_logp, "NetprNameValidate not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetprNameCanonicalize(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *Name,
    /* [size_is][out] */ WCHAR *Outbuf,
    /* [range][in] */ DWORD OutbufLen,
    /* [in] */ DWORD NameType,
    /* [in] */ DWORD Flags)
{
    osi_Log0(afsd_logp, "NetprNameCanonicalize not supported");
    return ERROR_NOT_SUPPORTED;
}

long NetprNameCompare(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *Name1,
    /* [string][in] */ WCHAR *Name2,
    /* [in] */ DWORD NameType,
    /* [in] */ DWORD Flags)
{
    osi_Log0(afsd_logp, "NetprNameCompare not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareEnumSticky(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [out][in] */ LPSHARE_ENUM_STRUCT InfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ DWORD *TotalEntries,
    /* [unique][out][in] */ DWORD *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrShareEnumSticky not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareDelStart(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *NetName,
    /* [in] */ DWORD Reserved,
    /* [out] */ PSHARE_DEL_HANDLE ContextHandle)
{
    osi_Log0(afsd_logp, "NetrShareDelStart not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareDelCommit(
    /* [out][in] */ PSHARE_DEL_HANDLE ContextHandle)
{
    osi_Log0(afsd_logp, "NetrShareDelCommit not supported");
    return ERROR_NOT_SUPPORTED;
}

DWORD NetrpGetFileSecurity(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][string][in] */ WCHAR *ShareName,
    /* [string][in] */ WCHAR *lpFileName,
    /* [in] */ SECURITY_INFORMATION RequestedInformation,
    /* [out] */ PADT_SECURITY_DESCRIPTOR *SecurityDescriptor)
{
    osi_Log0(afsd_logp, "NetprGetFileSecurity not supported");
    return ERROR_NOT_SUPPORTED;
}

DWORD NetrpSetFileSecurity(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][string][in] */ WCHAR *ShareName,
    /* [string][in] */ WCHAR *lpFileName,
    /* [in] */ SECURITY_INFORMATION SecurityInformation,
    /* [in] */ PADT_SECURITY_DESCRIPTOR SecurityDescriptor)
{
    osi_Log0(afsd_logp, "NetprSetFileSecurity not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerTransportAddEx(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPTRANSPORT_INFO Buffer)
{
    osi_Log0(afsd_logp, "NetrServerTransportAddEx not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsGetVersion(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [out] */ DWORD *Version)
{
    osi_Log0(afsd_logp, "NetrDfsGetVersion not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsCreateLocalPartition(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *ShareName,
    /* [in] */ GUID *EntryUid,
    /* [string][in] */ WCHAR *EntryPrefix,
    /* [string][in] */ WCHAR *ShortName,
    /* [in] */ LPNET_DFS_ENTRY_ID_CONTAINER RelationInfo,
    /* [in] */ int Force)
{
    osi_Log0(afsd_logp, "NetrDfsCreateLocalPartition not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsDeleteLocalPartition(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ GUID *Uid,
    /* [string][in] */ WCHAR *Prefix)
{
    osi_Log0(afsd_logp, "NetrDfsDeleteLocalPartition not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsSetLocalVolumeState(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ GUID *Uid,
    /* [string][in] */ WCHAR *Prefix,
    /* [in] */ unsigned long State)
{
    osi_Log0(afsd_logp, "NetrDfsSetLocalVolumeState not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsCreateExitPoint(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ GUID *Uid,
    /* [string][in] */ WCHAR *Prefix,
    /* [in] */ unsigned long Type,
    /* [range][in] */ DWORD ShortPrefixLen,
    /* [size_is][string][out] */ WCHAR *ShortPrefix)
{
    osi_Log0(afsd_logp, "NetrDfsCreateExitPoint not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsDeleteExitPoint(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ GUID *Uid,
    /* [string][in] */ WCHAR *Prefix,
    /* [in] */ unsigned long Type)
{
    osi_Log0(afsd_logp, "NetrDfsDeleteExitPoint not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsModifyPrefix(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ GUID *Uid,
    /* [string][in] */ WCHAR *Prefix)
{
    osi_Log0(afsd_logp, "NetrDfsModifyPrefix not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsFixLocalVolume(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [string][in] */ WCHAR *VolumeName,
    /* [in] */ unsigned long EntryType,
    /* [in] */ unsigned long ServiceType,
    /* [string][in] */ WCHAR *StgId,
    /* [in] */ GUID *EntryUid,
    /* [string][in] */ WCHAR *EntryPrefix,
    /* [in] */ LPNET_DFS_ENTRY_ID_CONTAINER RelationInfo,
    /* [in] */ unsigned long CreateDisposition)
{
    osi_Log0(afsd_logp, "NetrDfsFixLocalVolume not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrDfsManagerReportSiteInfo(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [unique][out][in] */ LPDFS_SITELIST_INFO *ppSiteInfo)
{
    osi_Log0(afsd_logp, "NetrDfsManagerReportSiteInfo not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerTransportDelEx(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPTRANSPORT_INFO Buffer)
{
    osi_Log0(afsd_logp, "NetrServerTransportDelEx not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerAliasAdd(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPSERVER_ALIAS_INFO InfoStruct)
{
    osi_Log0(afsd_logp, "NetrServerAliasAdd not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerAliasEnum(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [out][in] */ LPSERVER_ALIAS_ENUM_STRUCT InfoStruct,
    /* [in] */ DWORD PreferedMaximumLength,
    /* [out] */ LPDWORD TotalEntries,
    /* [unique][out][in] */ LPDWORD ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrServerAliasEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrServerAliasDel(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPSERVER_ALIAS_INFO InfoStruct)
{
    osi_Log0(afsd_logp, "NetrServerAliasDel not supported");
    return ERROR_NOT_SUPPORTED;
}

NET_API_STATUS NetrShareDelEx(
    /* [unique][string][in] */ SRVSVC_HANDLE ServerName,
    /* [in] */ DWORD Level,
    /* [switch_is][in] */ LPSHARE_INFO ShareInfo)
{
    osi_Log0(afsd_logp, "NetrShareDelEx not supported");
    return ERROR_NOT_SUPPORTED;
}


void __RPC_USER SHARE_DEL_HANDLE_rundown( SHARE_DEL_HANDLE h)
{
}

/* [nocode] */ void Opnum0NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum1NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum2NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

#if 0
/* [nocode] */ void Opnum3NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum4NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}
#endif

/* [nocode] */ void Opnum5NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum6NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum7NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum29NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum42NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum47NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

