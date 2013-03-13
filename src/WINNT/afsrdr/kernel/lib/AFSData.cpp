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

PDRIVER_OBJECT      AFSLibraryDriverObject = NULL;

PDEVICE_OBJECT      AFSLibraryDeviceObject = NULL;

PDEVICE_OBJECT      AFSControlDeviceObject = NULL;

PDEVICE_OBJECT      AFSRDRDeviceObject = NULL;

UNICODE_STRING      AFSRegistryPath;

HANDLE              AFSSysProcess = NULL;

UNICODE_STRING      AFSServerName;

UNICODE_STRING      AFSMountRootName;

AFSVolumeCB        *AFSGlobalRoot = NULL;

UNICODE_STRING      AFSPIOCtlName;

UNICODE_STRING      AFSGlobalRootName;

ULONG               AFSDebugFlags = 0;

CACHE_MANAGER_CALLBACKS *AFSLibCacheManagerCallbacks = NULL;

ULONG               AFSLibControlFlags = 0;

void               *AFSLibCacheBaseAddress = NULL;

LARGE_INTEGER       AFSLibCacheLength = {0,0};

PAFSDbgLogMsg       AFSDebugTraceFnc = NULL;

//
// List of 'special' share names we need to handle
//

AFSDirectoryCB     *AFSSpecialShareNames = NULL;

//
// Global relative entries for enumerations
//

AFSDirectoryCB     *AFSGlobalDotDirEntry = NULL;

AFSDirectoryCB     *AFSGlobalDotDotDirEntry = NULL;

//
// Callbacks in the framework
//

PAFSProcessRequest  AFSProcessRequest = NULL;

PAFSDbgLogMsg       AFSDbgLogMsg = AFSDefaultLogMsg;

PAFSAddConnectionEx AFSAddConnectionEx = NULL;

PAFSExAllocatePoolWithTag   AFSExAllocatePoolWithTag = NULL;

PAFSExFreePoolWithTag      AFSExFreePoolWithTag = NULL;

PAFSDumpTraceFiles  AFSDumpTraceFilesFnc = AFSDumpTraceFiles_Default;

PAFSRetrieveAuthGroup AFSRetrieveAuthGroupFnc = NULL;

//
// Security descriptor routine
//

PAFSRtlSetSaclSecurityDescriptor AFSRtlSetSaclSecurityDescriptor = NULL;

SECURITY_DESCRIPTOR *AFSDefaultSD = NULL;

PAFSRtlSetGroupSecurityDescriptor AFSRtlSetGroupSecurityDescriptor = NULL;

SID_IDENTIFIER_AUTHORITY SeWorldSidAuthority = {SECURITY_WORLD_SID_AUTHORITY};

}
