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

#ifndef _AFS_STRUCTS_H
#define _AFS_STRUCTS_H

//
// File: AFSStructs.h
//

//
// Library queue request cb
//

typedef struct _AFS_LIBRARY_QUEUE_REQUEST_CB
{

    struct _AFS_LIBRARY_QUEUE_REQUEST_CB    *fLink;

    PIRP        Irp;

} AFSLibraryQueueRequestCB;

typedef struct AFS_PROCESS_CB
{

    AFSBTreeEntry       TreeEntry;              // HashIndex = ProcessId

    ERESOURCE           Lock;

    ULONG               Flags;

    ULONGLONG           ParentProcessId;

    ULONGLONG           CreatingThread;

    GUID               *ActiveAuthGroup;

    struct _AFS_PROCESS_AUTH_GROUP_CB *AuthGroupList;

    struct AFS_THREAD_CB    *ThreadList;

} AFSProcessCB;

typedef struct AFS_THREAD_CB
{

    struct AFS_THREAD_CB *Next;

    ULONGLONG           ThreadId;

    ULONG               Flags;

    GUID               *ActiveAuthGroup;

} AFSThreadCB;

typedef struct _AFS_SID_ENTRY_CB
{

    AFSBTreeEntry       TreeEntry;

    ULONG               Flags;

    GUID                AuthGroup;

} AFSSIDEntryCB;

typedef struct _AFS_PROCESS_AUTH_GROUP_CB
{

    struct _AFS_PROCESS_AUTH_GROUP_CB *Next;

    ULONG               Flags;

    ULONGLONG           AuthGroupHash;

    GUID                AuthGroup;

} AFSProcessAuthGroupCB;

typedef struct _AFS_SET_DACL_CB
{

    AFSProcessCB       *ProcessCB;

    NTSTATUS            RequestStatus;

    KEVENT              Event;

} AFSSetDaclRequestCB;

typedef struct _AFS_SRVTABLE_ENTRY
{

    PVOID          *ServiceTable;

    ULONG           LowCall;

    ULONG           HiCall;

    PVOID          *ArgTable;

} AFSSrvcTableEntry;

#endif /* _AFS_STRUCTS_H */
