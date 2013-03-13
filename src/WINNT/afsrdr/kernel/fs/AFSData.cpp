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

//
// File: AFSData.cpp
//

#define NO_EXTERN

#include "AFSCommon.h"

extern "C" {

PDRIVER_OBJECT      AFSDriverObject = NULL;

PDEVICE_OBJECT      AFSDeviceObject = NULL;

PDEVICE_OBJECT      AFSRDRDeviceObject = NULL;

FAST_IO_DISPATCH    AFSFastIoDispatch;

UNICODE_STRING      AFSRegistryPath;

ULONG               AFSDebugFlags = 0;

ULONG               AFSTraceLevel = 0;

ULONG               AFSTraceComponent = 0;

HANDLE              AFSSysProcess = NULL;

HANDLE              AFSMUPHandle = NULL;

UNICODE_STRING      AFSServerName;

UNICODE_STRING      AFSMountRootName;

UNICODE_STRING      AFSGlobalRootName;

CACHE_MANAGER_CALLBACKS AFSCacheManagerCallbacks;

//
// Max Length IO (Mb)
//
ULONG               AFSMaxDirectIo = 0;

//
// Maximum dirtiness that a file can get
//
ULONG               AFSMaxDirtyFile = 0;

//
// Dbg log information
//

ERESOURCE           AFSDbgLogLock;

ULONG               AFSDbgLogRemainingLength = 0;

char               *AFSDbgCurrentBuffer = NULL;

char               *AFSDbgBuffer = NULL;

ULONG               AFSDbgLogCounter = 0;

ULONG               AFSDbgBufferLength = 0;

ULONG               AFSDbgLogFlags = 0;

PAFSDumpTraceFiles  AFSDumpTraceFilesFnc = AFSDumpTraceFiles;

UNICODE_STRING      AFSDumpFileLocation;

KEVENT              AFSDumpFileEvent;

UNICODE_STRING      AFSDumpFileName;

void               *AFSDumpBuffer = NULL;

ULONG               AFSDumpBufferLength = 0;

//
// Authentication group information
//

ULONG               AFSAuthGroupFlags = 0;

GUID                AFSActiveAuthGroup;

GUID                AFSNoPAGAuthGroup;

PAFSSetInformationToken AFSSetInformationToken = NULL;

PAFSDbgLogMsg       AFSDebugTraceFnc = NULL;

}
