/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011 Your File System, Inc.
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
 * - Neither the names of Kernel Drivers, LLC and Your File System, Inc.
 *   nor the names of their contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission from Kernel Drivers, LLC and Your File System, Inc.
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

#ifndef _AFS_EXTERN_H
#define _AFS_EXTERN_H
//
// File: AFSExtern.h
//
//

extern "C" {

extern PDRIVER_OBJECT      AFSLibraryDriverObject;

extern PDEVICE_OBJECT      AFSLibraryDeviceObject;

extern PDEVICE_OBJECT      AFSControlDeviceObject;

extern PDEVICE_OBJECT      AFSRDRDeviceObject;

extern FAST_IO_DISPATCH    AFSFastIoDispatch;

extern UNICODE_STRING      AFSRegistryPath;

extern ULONG               AFSDebugFlags;

extern CACHE_MANAGER_CALLBACKS *AFSLibCacheManagerCallbacks;

extern HANDLE              AFSSysProcess;

extern UNICODE_STRING      AFSServerName;

extern AFSVolumeCB        *AFSGlobalRoot;

extern UNICODE_STRING      AFSPIOCtlName;

extern UNICODE_STRING      AFSGlobalRootName;

extern AFSDirectoryCB     *AFSSpecialShareNames;

extern AFSDirectoryCB     *AFSGlobalDotDirEntry;

extern AFSDirectoryCB     *AFSGlobalDotDotDirEntry;

extern PAFSProcessRequest  AFSProcessRequest;

extern PAFSDbgLogMsg       AFSDbgLogMsg;

extern PAFSAddConnectionEx AFSAddConnectionEx;

extern PAFSExAllocatePoolWithTag   AFSExAllocatePoolWithTag;

extern PAFSExFreePoolWithTag      AFSExFreePoolWithTag;

extern PAFSRetrieveAuthGroup AFSRetrieveAuthGroupFnc;

extern ULONG               AFSLibControlFlags;

extern void               *AFSLibCacheBaseAddress;

extern LARGE_INTEGER       AFSLibCacheLength;

extern PAFSRtlSetSaclSecurityDescriptor AFSRtlSetSaclSecurityDescriptor;

extern SECURITY_DESCRIPTOR *AFSDefaultSD;

extern PAFSRtlSetGroupSecurityDescriptor AFSRtlSetGroupSecurityDescriptor;

extern SID_IDENTIFIER_AUTHORITY SeWorldSidAuthority;

}

#endif /* _AFS_EXTERN_H */
