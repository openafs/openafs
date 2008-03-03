/* 
 * Copyright (c) 2007 Secure Endpoints Inc.
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Neither the name of the Secure Endpoints Inc. nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This source file provides the declarations 
 * which specify the AFS Cache Manager Volume Status Event
 * Notification API
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <nb30.h>
#include <string.h>
#include <malloc.h>
#include "afsd.h"
#include <WINNT/afsreg.h>

HMODULE hVolStatus = NULL;
dll_VolStatus_Funcs_t dll_funcs;
cm_VolStatus_Funcs_t cm_funcs;

static char volstat_NetbiosName[64] = "";

afs_uint32
cm_VolStatus_Active(void)
{
    return (hVolStatus != NULL);
}

/* This function is used to load any Volume Status Handlers 
 * and their associated function pointers.  
 */
long 
cm_VolStatus_Initialization(void)
{
    long (__fastcall * dll_VolStatus_Initialization)(dll_VolStatus_Funcs_t * dll_funcs, cm_VolStatus_Funcs_t *cm_funcs) = NULL;
    long code = 0;
    HKEY parmKey;
    DWORD dummyLen;
    char wd[MAX_PATH+1] = "";

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(wd);
        code = RegQueryValueEx(parmKey, "VolStatusHandler", NULL, NULL,
                                (BYTE *) &wd, &dummyLen);

        if (code == 0) {
            dummyLen = sizeof(volstat_NetbiosName);
            code = RegQueryValueEx(parmKey, "NetbiosName", NULL, NULL, 
                                   (BYTE *)volstat_NetbiosName, &dummyLen);
        }
        RegCloseKey (parmKey);
    }

    if (code == ERROR_SUCCESS && wd[0])
        hVolStatus = LoadLibrary(wd);
    if (hVolStatus) {
        (FARPROC) dll_VolStatus_Initialization = GetProcAddress(hVolStatus, "@VolStatus_Initialization@8");
        if (dll_VolStatus_Initialization) {
            cm_funcs.version = CM_VOLSTATUS_FUNCS_VERSION;
            cm_funcs.cm_VolStatus_Path_To_ID = cm_VolStatus_Path_To_ID;
            cm_funcs.cm_VolStatus_Path_To_DFSlink = cm_VolStatus_Path_To_DFSlink;

            dll_funcs.version = DLL_VOLSTATUS_FUNCS_VERSION;
            code = dll_VolStatus_Initialization(&dll_funcs, &cm_funcs);
        } 

        if (dll_VolStatus_Initialization == NULL || code != 0 || 
            dll_funcs.version != DLL_VOLSTATUS_FUNCS_VERSION) {
            FreeLibrary(hVolStatus);
            hVolStatus = NULL;
            code = -1;
        }
    }

    osi_Log1(afsd_logp,"cm_VolStatus_Initialization 0x%x", code);

    return code;
}

/* This function is used to unload any Volume Status Handlers
 * that were loaded during initialization.
 */
long 
cm_VolStatus_Finalize(void)
{
    osi_Log1(afsd_logp,"cm_VolStatus_Finalize handle 0x%x", hVolStatus);

    if (hVolStatus == NULL)
        return 0;

    FreeLibrary(hVolStatus);
    hVolStatus = NULL;
    return 0;
}

/* This function notifies the Volume Status Handlers that the
 * AFS client service has started.  If the network is started
 * at this point we call cm_VolStatus_Network_Started().
 */
long 
cm_VolStatus_Service_Started(void)
{
    long code = 0;

    osi_Log1(afsd_logp,"cm_VolStatus_Service_Started handle 0x%x", hVolStatus);

    if (hVolStatus == NULL)
        return 0;
   
    code = dll_funcs.dll_VolStatus_Service_Started();
    if (code == 0 && smb_IsNetworkStarted())
        code = dll_funcs.dll_VolStatus_Network_Started(cm_NetbiosName, cm_NetbiosName);

    return code;
}

/* This function notifies the Volume Status Handlers that the
 * AFS client service is stopping.
 */
long 
cm_VolStatus_Service_Stopped(void)
{
    long code = 0;

    osi_Log1(afsd_logp,"cm_VolStatus_Service_Stopped handle 0x%x", hVolStatus);

    if (hVolStatus == NULL)
        return 0;
   
    code = dll_funcs.dll_VolStatus_Service_Stopped();

    return code;
}


/* This function notifies the Volume Status Handlers that the
 * AFS client service is accepting network requests using the 
 * specified netbios names.
 */
long
#ifdef _WIN64
cm_VolStatus_Network_Started(const char * netbios32, const char * netbios64)
#else /* _WIN64 */
cm_VolStatus_Network_Started(const char * netbios)
#endif /* _WIN64 */
{
    long code = 0;

    if (hVolStatus == NULL)
        return 0;

#ifdef _WIN64
    code = dll_funcs.dll_VolStatus_Network_Started(netbios32, netbios64);
#else
    code = dll_funcs.dll_VolStatus_Network_Started(netbios, netbios);
#endif

    return code;
}

/* This function notifies the Volume Status Handlers that the
 * AFS client service is no longer accepting network requests 
 * using the specified netbios names 
 */
long
#ifdef _WIN64
cm_VolStatus_Network_Stopped(const char * netbios32, const char * netbios64)
#else /* _WIN64 */
cm_VolStatus_Network_Stopped(const char * netbios)
#endif /* _WIN64 */
{
    long code = 0;

    if (hVolStatus == NULL)
        return 0;

#ifdef _WIN64
    code = dll_funcs.dll_VolStatus_Network_Stopped(netbios32, netbios64);
#else
    code = dll_funcs.dll_VolStatus_Network_Stopped(netbios, netbios);
#endif

    return code;
}

/* This function is called when the IP address list changes.
 * Volume Status Handlers can use this notification as a hint 
 * that it might be possible to determine volume IDs for paths 
 * that previously were not accessible.  
 */
long 
cm_VolStatus_Network_Addr_Change(void)
{
    long code = 0;

    if (hVolStatus == NULL)
        return 0;

    code = dll_funcs.dll_VolStatus_Network_Addr_Change();

    return code;
}

/* This function notifies the Volume Status Handlers that the 
 * state of the specified cell.volume has changed.
 */
long 
cm_VolStatus_Change_Notification(afs_uint32 cellID, afs_uint32 volID, enum volstatus status)
{
    long code = 0;

    if (hVolStatus == NULL)
        return 0;

    code = dll_funcs.dll_VolStatus_Change_Notification(cellID, volID, status);

    return code;
}



long
cm_VolStatus_Notify_DFS_Mapping(cm_scache_t *scp, char *tidPathp, char *pathp)
{
    long code = 0;
    char src[1024], *p;
    size_t len;

    if (hVolStatus == NULL || dll_funcs.version < 2)
        return 0;

    snprintf(src,sizeof(src), "\\\\%s%s", volstat_NetbiosName, tidPathp);
    len = strlen(src);
    if ((src[len-1] == '\\' || src[len-1] == '/') &&
        (pathp[0] == '\\' || pathp[0] == '/'))
        strncat(src, &pathp[1], sizeof(src));
    else
        strncat(src, pathp, sizeof(src));

    for ( p=src; *p; p++ ) {
        if (*p == '/')
            *p = '\\';
    }

    code = dll_funcs.dll_VolStatus_Notify_DFS_Mapping(scp->fid.cell, scp->fid.volume, scp->fid.vnode, scp->fid.unique,
                                                      src, scp->mountPointStringp);

    return code;
}

long
cm_VolStatus_Invalidate_DFS_Mapping(cm_scache_t *scp)
{
    long code = 0;

    if (hVolStatus == NULL || dll_funcs.version < 2)
        return 0;

    code = dll_funcs.dll_VolStatus_Invalidate_DFS_Mapping(scp->fid.cell, scp->fid.volume, scp->fid.vnode, scp->fid.unique);

    return code;
}


long __fastcall
cm_VolStatus_Path_To_ID(const char * share, const char * path, afs_uint32 * cellID, afs_uint32 * volID, enum volstatus *pstatus)
{
    afs_uint32  code = 0;
    cm_req_t    req;
    cm_scache_t *scp;

    if (cellID == NULL || volID == NULL)
        return CM_ERROR_INVAL;

    osi_Log2(afsd_logp,"cm_VolStatus_Path_To_ID share %s path %s", 
              osi_LogSaveString(afsd_logp, (char *)share), osi_LogSaveString(afsd_logp, (char *)path));

    cm_InitReq(&req);

    code = cm_NameI(cm_data.rootSCachep, (char *)path, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW, cm_rootUserp, (char *)share, &req, &scp);
    if (code)
        goto done;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL,cm_rootUserp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        goto done;
    }
        
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    *cellID = scp->fid.cell;
    *volID  = scp->fid.volume;
    *pstatus = cm_GetVolumeStatus(scp->volp, scp->fid.volume);

    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

  done:
    osi_Log1(afsd_logp,"cm_VolStatus_Path_To_ID code 0x%x",code); 
    return code;
}

long __fastcall
cm_VolStatus_Path_To_DFSlink(const char * share, const char * path, afs_uint32 *pBufSize, char *pBuffer)
{
    afs_uint32  code = 0;
    cm_req_t    req;
    cm_scache_t *scp;
    size_t      len;

    if (pBufSize == NULL || (pBuffer == NULL && *pBufSize != 0))
        return CM_ERROR_INVAL;

    osi_Log2(afsd_logp,"cm_VolStatus_Path_To_DFSlink share %s path %s", 
              osi_LogSaveString(afsd_logp, (char *)share), osi_LogSaveString(afsd_logp, (char *)path));

    cm_InitReq(&req);

    code = cm_NameI(cm_data.rootSCachep, (char *)path, CM_FLAG_CASEFOLD | CM_FLAG_FOLLOW, 
                    cm_rootUserp, (char *)share, &req, &scp);
    if (code)
        goto done;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, cm_rootUserp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        lock_ReleaseWrite(&scp->rw);
        cm_ReleaseSCache(scp);
        goto done;
    }
        
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    if (scp->fileType != CM_SCACHETYPE_DFSLINK) {
        code = CM_ERROR_NOT_A_DFSLINK;
        goto done;
    }

    len = strlen(scp->mountPointStringp) + 1;
    if (pBuffer == NULL)
        *pBufSize = len;
    else if (*pBufSize >= len) {
        strcpy(pBuffer, scp->mountPointStringp);
        *pBufSize = len;
    } else {
        code = CM_ERROR_TOOBIG;
        goto done;
    }

    lock_ReleaseWrite(&scp->rw);
    cm_ReleaseSCache(scp);

  done:
    osi_Log1(afsd_logp,"cm_VolStatus_Path_To_DFSlink code 0x%x",code); 
    return code;
}
