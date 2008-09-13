#ifndef _AFS_STRUCTS_H
#define _AFS_STRUCTS_H

//
// File: AFSStructs.h
//

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

    ERESOURCE            *TreeLock;

} AFSTreeHdr;

typedef struct _AFS_DIR_HDR
{

    struct _AFS_DIR_NODE_CB *CaseSensitiveTreeHead;

    struct _AFS_DIR_NODE_CB *CaseInsensitiveTreeHead;

    ERESOURCE               *TreeLock;

    LONG                     ContentIndex;

} AFSDirHdr;

//
// Worker pool header
//

typedef struct _AFS_WORKER_QUEUE_HDR
{

    struct _AFS_WORKER_QUEUE_HDR    *fLink;

    KEVENT           WorkerThreadReady;
   
    void            *WorkerThreadObject;

    ULONG            State;

} AFSWorkQueueContext, *PAFSWorkQueueContext;

//
// The first portion is the non-paged section of the Fcb
//

typedef struct _AFS_NONPAGED_FCB
{

    USHORT          Size;
    USHORT          Type;
    
    //
    // Ranking - File Resource first, then Paging Resource
    //

    ERESOURCE       Resource;

    ERESOURCE       PagingResource;

    //
    // The section object pointer
    //

    SECTION_OBJECT_POINTERS        SectionObjectPointers;

    FAST_MUTEX      AdvancedHdrMutex;

    //
    // Notificaiton information. This is used for change notification
    //

    LIST_ENTRY          DirNotifyList;

    PNOTIFY_SYNC        NotifySync;

    union
    {

        struct
        {

            ERESOURCE       ExtentsResource; 

            //
            // This is set when it is OK for a thread to REMOVE extents from
            // the list - to preserve atomicity, setting it is protected by the
            // ExtentsResource
            //
            KEVENT          ExtentsNotBusy;

            //
            // This is set when an Extents Request completes.  Do not wait for this
            // with the Extents resource held!
            //
            KEVENT          ExtentsRequestComplete;

            //
            // Every activity which depends on the extents not being freed
            // increments this count.  When they are done they decrement it.
            // The 1->0 transition is protected by the ExtentsResource
            //
            LONG            ExtentsRefCount;

            NTSTATUS        ExtentsRequestStatus;

        } File;

        struct
        {

            ERESOURCE       DirectoryTreeLock;

        } Directory;

        struct
        {

            ERESOURCE       DirectoryTreeLock;

            ERESOURCE      FileIDTreeLock;

            ERESOURCE       FcbListLock;

        } VolumeRoot;

    } Specific;

} AFSNonPagedFcb, *PAFSNonPagedFcb;


typedef struct _AFS_FSD_EXTENT
{
    //
    // Linked list first - the extents and then the skip list
    //
    LIST_ENTRY          Lists[AFS_NUM_EXTENT_LISTS];

    //
    // And the extent itself
    //
    LARGE_INTEGER       FileOffset;

    LARGE_INTEGER       CacheOffset;

    ULONG               Size;

    ULONG               Flags;

} AFSExtent, *PAFSExtent;

typedef struct AFS_FCB
{

    FSRTL_ADVANCED_FCB_HEADER Header;

    //
    // Our tree entry.
    //

    AFSBTreeEntry       TreeEntry;

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
    // Last access time.
    //

    LARGE_INTEGER       LastAccessCount;

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
    // Back pointer to the parent Fcb
    //

    struct AFS_FCB  *ParentFcb;

    //
    // Pointer to this entries meta data
    //

    struct _AFS_DIR_NODE_CB  *DirEntry;

    //
    // Back pointer to the root Fcb for this entry
    //

    struct AFS_FCB *RootFcb;

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
            // Last PID that either modified the file, mapped the file or
            // opened it with write priv's
            //

            HANDLE              ModifyProcessId;

            //
            // Set if there is any dirty data. Set pessimistically
            //
            ULONGLONG           ExtentsDirtyCount;

        } File;

        struct
        {

            LONG                ChildOpenHandleCount;

            LONG                ChildOpenReferenceCount;

            LONG                ChildReferenceCount;      // This controls tear down of the Fcb, it references
                                                          // the number of allocated directory nodes in the 
                                                          // directory

            AFSDirHdr           DirectoryNodeHdr;

            struct _AFS_DIR_NODE_CB  *DirectoryNodeListHead;

            struct _AFS_DIR_NODE_CB  *DirectoryNodeListTail;

            struct _AFS_DIR_NODE_CB  *ShortNameTree;

            //
            // Index for the PIOCtl open count
            //

            LONG                PIOCtlIndex;

        } Directory;

        struct
        {

            //
            // Because a mount point contains the information in the root of the
            // volume it duplicates the Directory information as well as more
            // data. Do not change the fields to the PIOCtlIndex, inclusive.
            //

            LONG                ChildOpenHandleCount;

            LONG                ChildOpenReferenceCount;

            LONG                ChildReferenceCount;      // This controls tear down of the Fcb, it references
                                                          // the number of allocated directory nodes in the 
                                                          // directory

            AFSDirHdr           DirectoryNodeHdr;

            struct _AFS_DIR_NODE_CB  *DirectoryNodeListHead;

            struct _AFS_DIR_NODE_CB  *DirectoryNodeListTail;

            struct _AFS_DIR_NODE_CB  *ShortNameTree;

            //
            // Index for the PIOCtl open count
            //

            LONG                PIOCtlIndex;

            //
            // File ID and OFFLINE tree and list
            //

            AFSTreeHdr          FileIDTree;

            AFSTreeHdr          OfflineFileIDTree;

            //
            // Linked list of Fcb's for asynchronous processing
            //

            struct AFS_FCB     *FcbListHead;

            struct AFS_FCB     *FcbListTail;

            ERESOURCE          *FcbListLock;

            //
            // Volume work thread for this mount point
            //

            AFSWorkQueueContext VolumeWorkerContext;

        } VolumeRoot;

        struct
        {

            struct AFS_FCB  *TargetFcb;

        } MountPoint;

        struct
        {

            struct AFS_FCB  *TargetFcb;

        } SymbolicLink;

    } Specific;

} AFSFcb, *PAFSFcb;

//
// These are the context control blocks for the open instance
//

typedef struct _AFS_CCB
{

    USHORT        Size;
    USHORT        Type;

    ULONG         Flags;

    //
    // Directory enumeration informaiton
    //

    BOOLEAN     DirQueryMapped;

    UNICODE_STRING MaskName;

    ULONG       CurrentDirIndex;

    //
    // PIOCtl interface request id
    //

    ULONG       PIOCtlRequestID;

    //
    // Full path of how the instance was opened
    //

    UNICODE_STRING FullFileName;

} AFSCcb;

// Read and writes can fan out and so they are syncrhonized via one of
// these structures
//

typedef struct _AFS_GATHER_READWRITE
{
    KEVENT     Event;  

    LONG       Count;

    NTSTATUS   Status;

    PIRP       MasterIrp;

    BOOLEAN    Synchronous;
} AFSGatherIo;

typedef struct _AFS_IO_RUNS {

    LARGE_INTEGER CacheOffset;
    PIRP          ChildIrp;
    ULONG         ByteCount;
} AFSIoRun;

//
// Work item
//

typedef struct _AFS_WORK_ITEM
{

    struct _AFS_WORK_ITEM *next;          

    ULONG    RequestType;

    ULONG    RequestFlags;

    NTSTATUS Status;

    KEVENT   Event;

    ULONG    Size;

    union
    {
        struct
        {
            PIRP Irp;

        } ReleaseExtents;

        struct
        {
            AFSFcb *Fcb;

        } FlushFcb;

        struct
        {
            PIRP Irp;
            PDEVICE_OBJECT Device;

        } AsynchIo;

        struct 
        {
            char     Context[ 1];
        } Other;

    } Specific;

} AFSWorkItem, *PAFSWorkItem;

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

    ULONG                       IrpPoolControlFlag;

} AFSCommSrvcCB, *PAFSCommSrvcCB;

//
// Irp Pool entry
//

typedef struct _POOL_ENTRY
{

    struct _POOL_ENTRY    *fLink;

    struct _POOL_ENTRY    *bLink;

    KEVENT      Event;

    HANDLE      ProcessID;

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
// Process registration 
//

typedef struct AFS_PROCESS_CB
{

    PEPROCESS   Process;

    ULONG       RegistrationMask;

    struct AFS_PROCESS_CB *Next;

} AFSProcessCB;

typedef struct _AFS_NONPAGED_DIR_NODE
{

    ERESOURCE       Lock;

} AFSNonPagedDirNode, *PAFSNonPagedDirNode;

typedef struct _AFS_DIR_ENUM_ENTRY_CB
{

    AFSFileID       ParentId;

    AFSFileID	    FileId;

    AFSFileID       TargetFileId;

    ULONG           FileIndex;		/* Incremented  */

    LARGE_INTEGER   Expiration;		/* FILETIME */

    LARGE_INTEGER   DataVersion;

    ULONG           FileType;		/* File, Dir, MountPoint, Symlink */
    
    LARGE_INTEGER   CreationTime;	/* FILETIME */

    LARGE_INTEGER   LastAccessTime;	/* FILETIME */

    LARGE_INTEGER   LastWriteTime;	/* FILETIME */

    LARGE_INTEGER   ChangeTime;		/* FILETIME */

    LARGE_INTEGER   EndOfFile;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;	/* NTFS FILE_ATTRIBUTE_xxxx see below */

    ULONG           EaSize;

    ULONG           Links;

    CCHAR           ShortNameLength;

    WCHAR           ShortName[12];

    UNICODE_STRING  FileName;

    UNICODE_STRING  TargetName;

} AFSDirEnumEntryCB;

typedef struct _AFS_DIR_NODE_CB
{

    AFSBTreeEntry    CaseSensitiveTreeEntry;    // For entries in the NameEntry tree, the
                                                // Index is a CRC on the name. For Volume,
                                                // MP and SL nodes, the Index is the Cell, Volume
                                                // For all others it is the vnode, uniqueid

    AFSBTreeEntry    CaseInsensitiveTreeEntry;

    AFSListEntry     CaseInsensitiveList;

    ULONG            Flags;

    //
    // Directory entry information
    //

    AFSDirEnumEntryCB   DirectoryEntry;

    //
    // Nonpaged portion of the dir entry
    //
   
    AFSNonPagedDirNode    *NPDirNode;

    //
    // List entry for the directory enumeraiton list in a parent node
    //

    AFSListEntry     ListEntry;

    //
    // The Fcb for this entry if it has one
    //
    
    AFSFcb          *Fcb;

    //
    // Type specific information
    //

    union
    {

        struct
        {

            AFSBTreeEntry    ShortNameTreeEntry;

        } Data;

        struct
        {

            ULONG           Reserved;

        } MountPoint;

        struct
        {

            ULONG           Reserved;

        } SymLink;

        struct
        {

            AFSVolumeInfoCB        VolumeInformation;

        } Volume;

    } Type;

} AFSDirEntryCB;

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

    ULONG            Flags;

    union
    {

        struct
        {

            //
            // Worker pool information
            //

            ULONG            WorkerCount;

            //
            // Fcb lifetime & flush time tickcount. This is calculated
            // in DriverEntry() for the control device.
            //

            LARGE_INTEGER           FcbLifeTimeCount;
            LARGE_INTEGER           FcbFlushTimeCount;
            LARGE_INTEGER           FcbPurgeTimeCount;

            struct _AFS_WORKER_QUEUE_HDR *PoolHead;

            ERESOURCE        QueueLock;

            AFSWorkItem     *QueueHead;

            AFSWorkItem     *QueueTail;

            KEVENT           WorkerQueueHasItems;

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

            //
            // Throttles on behavior
            //
            LARGE_INTEGER       MaxIo;

            LARGE_INTEGER       MaxDirty;

            //
            // Volume tree
            //

            AFSTreeHdr          VolumeTree;

            ERESOURCE           VolumeTreeLock;

            AFSFcb             *VolumeListHead;

            AFSFcb             *VolumeListTail;

            ERESOURCE           VolumeListLock;

        } RDR;

    } Specific;

} AFSDeviceExt, *PAFSDeviceExt;

//
// Network provider connection cb
//

typedef struct _AFSFSD_PROVIDER_CONNECTION_CB
{

    struct _AFSFSD_PROVIDER_CONNECTION_CB *fLink;

    WCHAR   LocalName;

    UNICODE_STRING RemoteName;

} AFSProviderConnectionCB;

#endif /* _AFS_STRUCTS_H */
