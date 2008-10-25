/*
 * Copyright (c) 2008 Kernel Drivers, LLC.
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
 *   specific prior written permission from Kernel Drivers, LLC.
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

extern PDRIVER_OBJECT      AFSDriverObject;

extern PDEVICE_OBJECT      AFSDeviceObject;

extern PDEVICE_OBJECT      AFSRDRDeviceObject;

extern FAST_IO_DISPATCH    AFSFastIoDispatch;

extern unsigned long       AFSCRCTable[];

extern UNICODE_STRING      AFSRegistryPath;

extern ULONG               AFSDebugFlags;

extern ULONG               AFSDebugLevel;

extern ULONG               AFSMaxDirectIo;

extern ULONG               AFSMaxDirtyFile;

extern CACHE_MANAGER_CALLBACKS AFSCacheManagerCallbacks;

extern HANDLE              AFSSysProcess;

extern HANDLE              AFSMUPHandle;

extern UNICODE_STRING      AFSServerName;

extern ERESOURCE           AFSProviderListLock;

extern AFSProviderConnectionCB   *AFSProviderConnectionList;

extern AFSProviderConnectionCB   *AFSProviderEnumerationList;

extern AFSFcb             *AFSGlobalRoot;

extern UNICODE_STRING      AFSPIOCtlName;

extern ULONG               AFSAllocationMemoryLevel;

extern UNICODE_STRING      AFSGlobalRootName;

#ifdef AFS_DEBUG_LOG

extern ERESOURCE           AFSDbgLogLock;

extern ULONG               AFSDbgLogRemainingLength;

extern char               *AFSDbgCurrentBuffer;

extern char               *AFSDbgBuffer;

extern ULONG               AFSDbgLogCounter;

#endif

}

#endif /* _AFS_EXTERN_H */
