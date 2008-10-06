/*
 * Copyright (c) 2008 Secure Endpoints Inc.
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
 * - Neither the name of Secure Endpoints Inc nor the names of its contributors may be 
 *   used to endorse or promote products derived from this software without 
 *   specific prior written permission from Secure Endpoints Inc.
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
 *
 */

#ifndef CM_RDR_H
#define CM_RDR_H

/* From RDR Prototypes.h */
extern DWORD RDR_RequestExtentRelease(DWORD numOfExtents, LARGE_INTEGER numOfHeldExtents);

extern DWORD RDR_Initialize( void);

extern DWORD RDR_Shutdown( void);

extern DWORD RDR_NetworkStatus( IN BOOLEAN status);

extern DWORD RDR_VolumeStatus( IN ULONG cellID, IN ULONG volID, IN BOOLEAN online);

extern DWORD RDR_NetworkAddrChange(void);

extern DWORD RDR_InvalidateVolume( IN ULONG cellID, IN ULONG volID, IN ULONG reason);

extern DWORD 
RDR_InvalidateObject( IN ULONG cellID, IN ULONG volID, IN ULONG vnode, 
                      IN ULONG uniq, IN ULONG hash, IN ULONG filetype, IN ULONG reason);

extern DWORD 
RDR_SysName(ULONG Architecture, ULONG Count, WCHAR **NameList);
#define AFS_MAX_SYSNAME_LENGTH 128
#define AFS_SYSNAME_ARCH_32BIT 0
#define AFS_SYSNAME_ARCH_64BIT 1

#endif /* CM_RDR_H */

