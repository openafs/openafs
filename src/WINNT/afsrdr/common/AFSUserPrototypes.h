/*
 * Copyright (c) 2008-2011 Kernel Drivers, LLC.
 * Copyright (c) 2009-2011 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice,
 *   this list of conditions and the following disclaimer in the
 *   documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Kernel Drivers, LLC nor the names of its
 *   contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Kernel Drivers, LLC
 *   and Your File System, Inc.
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

extern DWORD RDR_RequestExtentRelease(cm_fid_t *fidp, LARGE_INTEGER numOfHeldExtents,
                                       DWORD numOfExtents, AFSFileExtentCB *extentList);

extern DWORD RDR_Initialize( void);

extern DWORD RDR_ShutdownNotify( void);

extern DWORD RDR_ShutdownFinal( void);

extern DWORD RDR_NetworkStatus( IN BOOLEAN status);

extern DWORD RDR_VolumeStatus( IN ULONG cellID, IN ULONG volID, IN BOOLEAN online);

extern DWORD RDR_NetworkAddrChange(void);

extern DWORD RDR_InvalidateVolume( IN ULONG cellID, IN ULONG volID, IN ULONG reason);

extern DWORD RDR_SetFileStatus( IN cm_fid_t *pFileId, IN GUID *pAuthGroup, IN DWORD dwStatus);

extern DWORD
RDR_InvalidateObject( IN ULONG cellID, IN ULONG volID, IN ULONG vnode,
                      IN ULONG uniq, IN ULONG hash, IN ULONG filetype, IN ULONG reason);

extern DWORD
RDR_SysName(ULONG Architecture, ULONG Count, WCHAR **NameList);

extern afs_int32
RDR_BkgFetch(cm_scache_t *scp, void *rockp, cm_user_t *userp, cm_req_t *reqp);

extern VOID RDR_Suspend( void);

extern VOID RDR_Resume( void);
