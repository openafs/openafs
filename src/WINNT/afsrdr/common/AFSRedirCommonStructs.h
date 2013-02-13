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

#ifndef _AFS_REDIR_COMMON_STRUCTS_H
#define _AFS_REDIR_COMMON_STRUCTS_H

//
// BTree Entry
//

typedef struct AFS_BTREE_ENTRY
{

    void    *parentLink;

    void    *leftLink;

    void    *rightLink;

    ULONGLONG HashIndex;

} AFSBTreeEntry, *PAFSBTreeEntry;

typedef struct AFS_LIST_ENTRY
{

    void    *fLink;

    void    *bLink;

} AFSListEntry;

typedef struct _AFS_TREE_HDR
{

    AFSBTreeEntry *TreeHead;

    ERESOURCE     *TreeLock;

} AFSTreeHdr;

//
// Sys Name Information CB
//

typedef struct _AFS_SYS_NAME_CB
{

    struct _AFS_SYS_NAME_CB     *fLink;

    UNICODE_STRING      SysName;

} AFSSysNameCB;

//
// Communication service control structures
//

typedef struct _AFSFSD_SRVC_CONTROL_BLOCK
{

    ULONG                       IrpPoolRequestIndex;

    struct _POOL_ENTRY          *RequestPoolHead;

    struct _POOL_ENTRY          *RequestPoolTail;

    ERESOURCE                   IrpPoolLock;

    struct _POOL_ENTRY          *ResultPoolHead;

    struct _POOL_ENTRY          *ResultPoolTail;

    ERESOURCE                   ResultPoolLock;

    KEVENT                      IrpPoolHasEntries;

    KEVENT                      IrpPoolHasReleaseEntries;

    ULONG                       IrpPoolControlFlag;

    LONG                        QueueCount;

} AFSCommSrvcCB, *PAFSCommSrvcCB;

//
// Irp Pool entry
//

typedef struct _POOL_ENTRY
{

    struct _POOL_ENTRY    *fLink;

    struct _POOL_ENTRY    *bLink;

    KEVENT      Event;

    GUID        AuthGroup;

    AFSFileID    FileId;

    UNICODE_STRING  FileName;

    ULONG       RequestType;

    ULONG       RequestIndex;

    ULONG       RequestFlags;

    ULONG       DataLength;

    void       *Data;

    ULONG       ResultStatus;

    void       *ResultBuffer;

    ULONG      *ResultBufferLength;

} AFSPoolEntry;

//
// The first portion is the non-paged section of the Fcb
//

typedef struct _AFS_NONPAGED_FCB
{

    USHORT          Size;
    USHORT          Type;

    //
    // Ranking - File Resource, Paging Resource,
    //           then SectionObject Resource
    //

    ERESOURCE       Resource;

    ERESOURCE       PagingResource;

    ERESOURCE       SectionObjectResource;

    //
    // The section object pointer
    //

    SECTION_OBJECT_POINTERS        SectionObjectPointers;

    FAST_MUTEX      AdvancedHdrMutex;

    ERESOURCE       CcbListLock;

    union
    {

        struct
        {

            ERESOURCE       ExtentsResource;

            //
            // This is set when an Extents Request completes.  Do not wait for this
            // with the Extents resource held!
            //
            KEVENT          ExtentsRequestComplete;

            NTSTATUS        ExtentsRequestStatus;

            GUID            ExtentsRequestAuthGroup;

            struct _AFS_FSD_EXTENT  *DirtyListHead;

            struct _AFS_FSD_EXTENT  *DirtyListTail;

            ERESOURCE       DirtyExtentsListLock;

            KEVENT          FlushEvent;

            //
            // Queued Flush event. This event is set when the queued flush count
            // is zero, cleared otherwise.

            KEVENT          QueuedFlushEvent;

        } File;

        struct
        {

            LONG            DirectoryEnumCount;

        } Directory;

    } Specific;

} AFSNonPagedFcb, *PAFSNonPagedFcb;

typedef struct _AFS_FSD_EXTENT
{
    //
    // Linked list first - the extents and then the skip list
    //

    LIST_ENTRY          Lists[AFS_NUM_EXTENT_LISTS];

    AFSListEntry        DirtyList;

    //
    // And the extent itself
    //

    LARGE_INTEGER       FileOffset;

    LARGE_INTEGER       CacheOffset;

    ULONG               Size;

    ULONG               Flags;

    LONG                ActiveCount;

#if GEN_MD5
    UCHAR               MD5[16];
#endif

} AFSExtent, *PAFSExtent;

typedef struct AFS_FCB
{

    FSRTL_ADVANCED_FCB_HEADER Header;

    //
    // This is the linked list of nodes processed asynchronously by the respective worker thread
    //

    AFSListEntry        ListEntry;

    //
    // The NP portion of the Fcb
    //

    AFSNonPagedFcb    *NPFcb;

    //
    // Fcb flags
    //

    ULONG               Flags;

    //
    // Share access mapping
    //

    SHARE_ACCESS        ShareAccess;

    //
    // Open pointer count on this file
    //

    LONG                OpenReferenceCount;

    //
    // Open handle count on this file
    //

    LONG                OpenHandleCount;

    //
    // Object info block
    //

    struct _AFS_OBJECT_INFORMATION_CB   *ObjectInformation;

    //
    // Ccb list pointers
    //

    struct _AFS_CCB    *CcbListHead;

    struct _AFS_CCB    *CcbListTail;

    //
    // Union for node type specific information
    //

    union
    {

        struct
        {
            //
            // We set this when a flush has been sent to the
            // server sucessfully.  We use this to influence when we
            // write the flush.
            //
            LARGE_INTEGER       LastServerFlush;

            //
            // We set this when the extent ref count goes to zero.
            // we use this to influence which files to purge
            //
            LARGE_INTEGER       LastExtentAccess;

            //
            // If there has been a RELEASE_FILE_EXTENTS - this is
            // where we stopped last time this stops us from
            // constantly refreeing the same extents and then grabbing
            // them again.
            //
            LARGE_INTEGER       LastPurgePoint;

            //
            // File lock
            //

            FILE_LOCK           FileLock;

            //
            // The extents
            //

            LIST_ENTRY          ExtentsLists[AFS_NUM_EXTENT_LISTS];

            //
            // There is only ever one request active, so we embed it
            //

            AFSRequestExtentsCB ExtentsRequest;

            //
            // Last PID that requested extents, NOT the system process
            //

            ULONGLONG           ExtentRequestProcessId;

            //
            // Dirty extent count
            //

            LONG                ExtentsDirtyCount;

            //
            // Extent count for this file
            //

            LONG                ExtentCount;

            //
            // Current count of queued flush items for the file
            //

            LONG                QueuedFlushCount;

            //
            // Cache space currently held by extents for the file
            //

            LONG                ExtentLength; // in KBs

        } File;

        struct
        {

            ULONG       Reserved;

        } Directory;

    } Specific;

} AFSFcb, *PAFSFcb;

typedef struct _AFS_DEVICE_EXTENSION
{

    //
    // Self pointer
    //

    PDEVICE_OBJECT  Self;

    //
    // List of device isntances
    //

    struct _AFS_DEVICE_EXTENSION *DeviceLink;

    //
    // Device flags
    //

    ULONG           DeviceFlags;

    AFSFcb*         Fcb;

    union
    {

        struct
        {

            //
            // Volume worker tracking information
            //

            KEVENT           VolumeWorkerCloseEvent;

            LONG             VolumeWorkerThreadCount;

            //
            // Fcb lifetime & flush time tickcount. This is calculated
            // in DriverEntry() for the control device.
            //

            LARGE_INTEGER           ObjectLifeTimeCount;
            LARGE_INTEGER           FcbFlushTimeCount;
            LARGE_INTEGER           FcbPurgeTimeCount;
            LARGE_INTEGER           ExtentRequestTimeCount;

            //
            // Comm interface
            //

            AFSCommSrvcCB    CommServiceCB;

            //
            // Extent Release Interface
            //

            ERESOURCE        ExtentReleaseResource;

            KEVENT           ExtentReleaseEvent;

            ULONG            ExtentReleaseSequence;

            PKPROCESS        ServiceProcess;

            //
            // SysName information control block
            //

            ERESOURCE       SysName32ListLock;

            AFSSysNameCB    *SysName32ListHead;

            AFSSysNameCB    *SysName32ListTail;

            ERESOURCE       SysName64ListLock;

            AFSSysNameCB    *SysName64ListHead;

            AFSSysNameCB    *SysName64ListTail;

            //
            // Our process tree information
            //

            AFSTreeHdr          ProcessTree;

            ERESOURCE           ProcessTreeLock;

            //
            // SID Entry tree
            //

            AFSTreeHdr          AuthGroupTree;

            ERESOURCE           AuthGroupTreeLock;

            //
            // Notificaiton information. This is used for change notification
            //

            LIST_ENTRY          DirNotifyList;

            PNOTIFY_SYNC        NotifySync;

            //
            // Library load information
            //

            KEVENT              LoadLibraryEvent;

            ULONG               LibraryState;

            ERESOURCE           LibraryStateLock;

            LONG                InflightLibraryRequests;

            KEVENT              InflightLibraryEvent;

            ERESOURCE           LibraryQueueLock;

            struct _AFS_LIBRARY_QUEUE_REQUEST_CB    *LibraryQueueHead;

            struct _AFS_LIBRARY_QUEUE_REQUEST_CB    *LibraryQueueTail;

            UNICODE_STRING      LibraryServicePath;

            DEVICE_OBJECT      *LibraryDeviceObject;

            FILE_OBJECT        *LibraryFileObject;

            //
            // Extent processing information within the library
            //

            LONG                ExtentCount;

            LONG                ExtentsHeldLength;

            KEVENT              ExtentsHeldEvent;

            //
            // Outstanding service request information
            //

            LONG                OutstandingServiceRequestCount;

            KEVENT              OutstandingServiceRequestEvent;

            //
            // Out of memory signalling
            //

            LONG                WaitingForMemoryCount;

            KEVENT              MemoryAvailableEvent;

        } Control;

        struct
        {

            //
            // Cache file information
            //

            HANDLE              CacheFileHandle;

            PFILE_OBJECT        CacheFileObject;

            ULONG               CacheBlockSize;

            UNICODE_STRING      CacheFile;

            LARGE_INTEGER       CacheBlockCount; // Total number of cache blocks in the cache file

            void               *CacheBaseAddress;

            LARGE_INTEGER       CacheLength;

            PMDL                CacheMdl;

            //
            // Throttles on behavior
            //
            LARGE_INTEGER       MaxIo;

            LARGE_INTEGER       MaxDirty;

            //
            // Maximum RPC length that is issued by the service. We should limit our
            // data requests such as for extents to this length
            //

            ULONG               MaximumRPCLength;

            //
            // Volume tree
            //

            AFSTreeHdr          VolumeTree;

            ERESOURCE           VolumeTreeLock;

            struct _AFS_VOLUME_CB        *VolumeListHead;

            struct _AFS_VOLUME_CB        *VolumeListTail;

            ERESOURCE           VolumeListLock;

            //
            // Queued extent release count and event
            //

            LONG                QueuedReleaseExtentCount;

            KEVENT              QueuedReleaseExtentEvent;

            //
            // Name array related information
            //

            ULONG               NameArrayLength;

            ULONG               MaxLinkCount;

            //
            // Our root cell tree
            //

            AFSTreeHdr          RootCellTree;

            ERESOURCE           RootCellTreeLock;

            //
            // Cache file object access
            //

            ERESOURCE           CacheFileLock;

            //
            // NP Connection list information
            //

            ERESOURCE                 ProviderListLock;

            struct _AFSFSD_PROVIDER_CONNECTION_CB   *ProviderConnectionList;

            struct _AFSFSD_PROVIDER_CONNECTION_CB   *ProviderEnumerationList;

        } RDR;

        struct
        {

            //
            // Worker pool information
            //

            ULONG            WorkerCount;

            struct _AFS_WORKER_QUEUE_HDR *PoolHead;

            ERESOURCE        QueueLock;

            struct _AFS_WORK_ITEM     *QueueHead;

            struct _AFS_WORK_ITEM     *QueueTail;

            KEVENT           WorkerQueueHasItems;

            LONG             QueueItemCount;

            //
            // IO Worker queue
            //

            ULONG            IOWorkerCount;

            struct _AFS_WORKER_QUEUE_HDR *IOPoolHead;

            ERESOURCE        IOQueueLock;

            struct _AFS_WORK_ITEM     *IOQueueHead;

            struct _AFS_WORK_ITEM     *IOQueueTail;

            KEVENT           IOWorkerQueueHasItems;

            LONG             IOQueueItemCount;

        } Library;

    } Specific;

} AFSDeviceExt, *PAFSDeviceExt;

//
// Network provider connection cb
//
#pragma pack(push, 1)
typedef struct _AFSFSD_PROVIDER_CONNECTION_CB
{

    struct _AFSFSD_PROVIDER_CONNECTION_CB *fLink;

    struct _AFSFSD_PROVIDER_CONNECTION_CB *EnumerationList;

    ULONG       Flags;

    ULONG       Type;

    ULONG       Scope;

    ULONG       DisplayType;

    ULONG       Usage;

    LARGE_INTEGER AuthenticationId;

    WCHAR       LocalName;

    UNICODE_STRING RemoteName;

    UNICODE_STRING ComponentName;

    UNICODE_STRING Comment;

} AFSProviderConnectionCB;
#pragma pack(pop)

//
// Callbacks defined in the framework
//

typedef
NTSTATUS
(*PAFSProcessRequest)( IN ULONG RequestType,
                       IN ULONG RequestFlags,
                       IN GUID *AuthGroup,
                       IN PUNICODE_STRING FileName,
                       IN AFSFileID *FileId,
                       IN WCHAR * Cell,
                       IN ULONG   CellLength,
                       IN void  *Data,
                       IN ULONG DataLength,
                       IN OUT void *ResultBuffer,
                       IN OUT PULONG ResultBufferLength);

typedef
NTSTATUS
(*PAFSDbgLogMsg)( IN ULONG Subsystem,
                  IN ULONG Level,
                  IN PCCH Format,
                  ...);

typedef
NTSTATUS
(*PAFSAddConnectionEx)( IN UNICODE_STRING *RemoteName,
                        IN ULONG DisplayType,
                        IN ULONG Flags);

typedef
void *
(*PAFSExAllocatePoolWithTag)( IN POOL_TYPE  PoolType,
                              IN SIZE_T  NumberOfBytes,
                              IN ULONG  Tag);

typedef
void
(*PAFSExFreePoolWithTag)( IN void *Pointer, IN ULONG Tag);

typedef
void
(*PAFSRetrieveAuthGroup)( IN ULONGLONG ProcessId,
                          IN ULONGLONG ThreadId,
                          OUT GUID *AuthGroup);

typedef struct _AFS_LIBRARY_INIT_CB
{

    PDEVICE_OBJECT      AFSControlDeviceObject;

    PDEVICE_OBJECT      AFSRDRDeviceObject;

    UNICODE_STRING      AFSServerName;

    UNICODE_STRING      AFSMountRootName;

    ULONG               AFSDebugFlags;

    AFSFileID           GlobalRootFid;

    CACHE_MANAGER_CALLBACKS *AFSCacheManagerCallbacks;

    void               *AFSCacheBaseAddress;

    LARGE_INTEGER       AFSCacheLength;

    //
    // Callbacks in the framework
    //

    PAFSProcessRequest  AFSProcessRequest;

    PAFSDbgLogMsg       AFSDbgLogMsg;

    PAFSAddConnectionEx AFSAddConnectionEx;

    PAFSExAllocatePoolWithTag   AFSExAllocatePoolWithTag;

    PAFSExFreePoolWithTag      AFSExFreePoolWithTag;

    PAFSDumpTraceFiles  AFSDumpTraceFiles;

    PAFSRetrieveAuthGroup AFSRetrieveAuthGroup;

} AFSLibraryInitCB;


#endif

