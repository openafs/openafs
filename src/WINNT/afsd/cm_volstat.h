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

/* This header file provides the definitions and prototypes 
 * which specify the AFS Cache Manager Volume Status Event
 * Notification API
 */

enum volstatus {vl_online, vl_busy, vl_offline, vl_alldown, vl_unknown};

extern long cm_VolStatus_Initialization(void);

extern long cm_VolStatus_Finalize(void);

extern long cm_VolStatus_Service_Started(void);

extern long cm_VolStatus_Service_Stopped(void);

#ifdef _WIN64
extern long cm_VolStatus_Network_Started(const char * netbios32, const char * netbios64);

extern long cm_VolStatus_Network_Stopped(const char * netbios32, const char * netbios64);
#else /* _WIN64 */
extern long cm_VolStatus_Network_Started(const char * netbios);

extern long cm_VolStatus_Network_Stopped(const char * netbios);
#endif /* _WIN64 */

extern long cm_VolStatus_Network_Addr_Change(void);

extern long cm_VolStatus_Change_Notification(afs_uint32 cellID, afs_uint32 volID, enum volstatus status);

extern long __fastcall cm_VolStatus_Path_To_ID(const char * share, const char * path, afs_uint32 * cellID, afs_uint32 * volID);

extern long __fastcall cm_VolStatus_Path_To_DFSlink(const char * share, const char * path, afs_uint32 *pBufSize, char *pBuffer);

#define DLL_VOLSTATUS_FUNCS_VERSION 1
typedef struct dll_VolStatus_Funcs {
    afs_uint32          version;
    long (__fastcall * dll_VolStatus_Service_Started)(void);
    long (__fastcall * dll_VolStatus_Service_Stopped)(void);
    long (__fastcall * dll_VolStatus_Network_Started)(const char *netbios32, const char *netbios64);
    long (__fastcall * dll_VolStatus_Network_Stopped)(const char *netbios32, const char *netbios64);
    long (__fastcall * dll_VolStatus_Network_Addr_Change)(void);
    long (__fastcall * dll_VolStatus_Change_Notification)(afs_uint32 cellID, afs_uint32 volID, enum volstatus status);
} dll_VolStatus_Funcs_t;

#define CM_VOLSTATUS_FUNCS_VERSION 1
typedef struct cm_VolStatus_Funcs {
    afs_uint32          version;
    long (__fastcall * cm_VolStatus_Path_To_ID)(const char * share, const char * path, afs_uint32 * cellID, afs_uint32 * volID);
    long (__fastcall * cm_VolStatus_Path_To_DFSlink)(const char * share, const char * path, afs_uint32 *pBufSize, char *pBuffer);
} cm_VolStatus_Funcs_t;

