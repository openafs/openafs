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

#ifndef _AFS_USER_STRUCT_H
#define _AFS_USER_STRUCT_H

typedef struct _AFS_FILE_ID
{

    ULONG    Hash;

    ULONG    Cell;

    ULONG    Volume;

    ULONG    Vnode;

    ULONG    Unique;

} AFSFileID;

//
// This control structure is the request block passed to the filter. The filter will populate the structure
// when it requires a request to be handled by the service.
//

typedef struct _AFS_COMM_REQUEST_BLOCK
{

    AFSFileID       FileId;          /* Initialize unused elements to 0 */

    ULONG           RequestType;

    ULONG           RequestIndex;    /* Must return to caller */

    ULONG           RequestFlags;    /* AFS_REQUEST_FLAG_xxxx */

    ULONG           NameLength;      /* Length of the name in bytes */

    ULONG           DataOffset;      /* This offset is from the end of the structure, including the name */

    ULONG           DataLength;

    GUID            AuthGroup;       /* Length: sizeof(GUID) */

    ULONG           ResultBufferLength;    /* Do not exceed this length in response */

    LONG            QueueCount;      /* Current outstanding requests in the queue */

    WCHAR           Name[ 1];

} AFSCommRequest;


//
// This is the result block passed back to the redirector after a request has been handled
//

typedef struct _AFS_COMM_RESULT_BLOCK
{

    ULONG           RequestIndex;        /* Must match the AFSCommRequest value */

    ULONG           ResultStatus;        /* NTSTATUS_xxx */

    ULONG           ResultBufferLength;  /* Not to exceed AFSCommRequest ResultBufferLength */

    ULONG           Authenticated;       /* Tokens or No? */

    char            ResultData[ 1];

} AFSCommResult;

//
// Control block passed to IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE
//

#define AFS_REDIR_INIT_FLAG_HIDE_DOT_FILES          0x00000001
#define AFS_REDIR_INIT_NO_PAGING_FILE               0x00000002
#define AFS_REDIR_INIT_FLAG_DISABLE_SHORTNAMES	    0x00000004

typedef struct _AFS_REDIR_INIT_INFO_CB
{

    ULONG       Flags;

    ULONG       MaximumChunkLength;     // Maximum RPC length issued so we should limit
                                        // requests for data to this length

    AFSFileID   GlobalFileId;           // AFS FID of the Global root.afs volume

    LARGE_INTEGER  ExtentCount;         // Number of extents in the current data cache

    ULONG       CacheBlockSize;         // Size, in bytes, of the current cache block

    ULONG       MaxPathLinkCount;       // Number of symlinks / mountpoints that may
                                        // be cross during the evaluation of any path

    ULONG       NameArrayLength;        // Number of components that should be allocated
                                        // in each name array chunk.  Name arrays are
                                        // dynamic and will be increased in size as
                                        // needed by this amount

    LARGE_INTEGER MemoryCacheOffset;    // The offset in the afsd_service process memory
                                        // space at which the extents are allocated
    LARGE_INTEGER MemoryCacheLength;    // and the length of the allocated region

    ULONG       DumpFileLocationOffset; // Offset from the beginning of this structure to
                                        // the start of the directory where dump files
                                        // are to be stored. The path must be fully
                                        // qualified such as C:\Windows\Temp

    ULONG       DumpFileLocationLength; // Length, in bytes, of the DumpFileLocation path

    ULONG       CacheFileNameLength;    // size, in bytes, of the cache file name

    WCHAR       CacheFileName[ 1];      // Fully qualified cache file name in the form
                                        // C:\OPenAFSDir\CacheFile.dat

} AFSRedirectorInitInfo;

//
// Directory query CB
//

typedef struct _AFS_DIR_QUERY_CB
{

    ULONG_PTR        EnumHandle;  // If this is 0 then it is a new query,
                                  // otherwise it is the FileIndex of the last
                                  // entry processed.

} AFSDirQueryCB;

//
// Directory enumeration control block
// Entries are aligned on a QuadWord boundary
//

typedef struct _AFS_DIR_ENUM_ENTRY
{

    AFSFileID       FileId;

    ULONG           FileIndex;          /* Incremented  */

    LARGE_INTEGER   Expiration;         /* FILETIME */

    LARGE_INTEGER   DataVersion;

    ULONG           FileType;           /* File, Dir, MountPoint, Symlink */

    LARGE_INTEGER   CreationTime;       /* FILETIME */

    LARGE_INTEGER   LastAccessTime;     /* FILETIME */

    LARGE_INTEGER   LastWriteTime;      /* FILETIME */

    LARGE_INTEGER   ChangeTime;         /* FILETIME */

    LARGE_INTEGER   EndOfFile;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;     /* NTFS FILE_ATTRIBUTE_xxxx see below */

    ULONG           FileNameLength;

    ULONG           EaSize;

    ULONG           Links;

    ULONG           FileNameOffset;     /* From beginning of this structure */

    ULONG           TargetNameOffset;   /* From beginning of this structure */

    ULONG           TargetNameLength;

    AFSFileID       TargetFileId;       /* Target fid for mp's and symlinks */

    CCHAR           ShortNameLength;

    WCHAR           ShortName[12];

    ULONG           NTStatus;           /* Error code during enumeration */

    /* Long Filename and Target (Symlink and MountPoint only) to follow without NULs */

} AFSDirEnumEntry;


typedef struct _AFS_DIR_ENUM_RESP
{

    ULONG_PTR       EnumHandle;

    LARGE_INTEGER   SnapshotDataVersion; /* DV at time Name/FID list was generated */

    LARGE_INTEGER   CurrentDataVersion;   /* DV at time this header was constructed */

    AFSDirEnumEntry Entry[ 1];     /* Each entry is Quad aligned */

} AFSDirEnumResp;

//
// Volume information CB passed in the create request
//

typedef struct _AFS_VOLUME_INFORMATION
{

    LARGE_INTEGER   TotalAllocationUnits;       /* Volume Max Blocks (Partition or Quota) */

    LARGE_INTEGER   AvailableAllocationUnits;   /* Volume Blocks Avail (Partition or Quota) */

    LARGE_INTEGER   VolumeCreationTime;         /* AFS Last Update - Not Creation */

    ULONG           Characteristics;            /* FILE_READ_ONLY_DEVICE (if readonly)
                                                 * FILE_REMOTE_DEVICE (always)
                                                 */

    ULONG           FileSystemAttributes;       /* FILE_CASE_PRESERVED_NAMES (always)
                                                   FILE_UNICODE_ON_DISK      (always) */

    ULONG           SectorsPerAllocationUnit;   /* = 1 */

    ULONG           BytesPerSector;             /* = 1024 */

    ULONG           CellID;                     /* AFS Cell ID */

    ULONG           VolumeID;                   /* AFS Volume ID */

    ULONG           VolumeLabelLength;

    WCHAR           VolumeLabel[128];           /* Volume */

    ULONG           CellLength;

    WCHAR           Cell[128];                  /* Cell */

} AFSVolumeInfoCB;


//
// Volume size information CB passed used to satisfy
// FileFsFullSizeInformation and FileFsSizeInformation
//

typedef struct _AFS_VOLUME_SIZE_INFORMATION
{

    LARGE_INTEGER   TotalAllocationUnits;       /* Max Blocks (Quota or Partition) */

    LARGE_INTEGER   AvailableAllocationUnits;   /* Blocks Avail (Quota or Partition) */

    ULONG           SectorsPerAllocationUnit;   /* = 1 */

    ULONG           BytesPerSector;             /* = 1024 */

} AFSVolumeSizeInfoCB;

//
// File create CB
//

typedef struct _AFS_FILE_CREATE_CB
{

    AFSFileID       ParentId;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;

    ULONG           EaSize;

    char            EaBuffer[ 1];

} AFSFileCreateCB;

typedef struct _AFS_FILE_CREATE_RESULT_CB
{

    LARGE_INTEGER   ParentDataVersion;

    AFSDirEnumEntry DirEnum;

} AFSFileCreateResultCB;

//
// File open CB
//

typedef struct _AFS_FILE_OPEN_CB
{

    AFSFileID       ParentId;

    ULONG           DesiredAccess;

    ULONG           ShareAccess;

    ULONGLONG       ProcessId;

    ULONGLONG       Identifier;

} AFSFileOpenCB;

typedef struct _AFS_FILE_OPEN_RESULT_CB
{

    ULONG           GrantedAccess;

    ULONG           FileAccess;

} AFSFileOpenResultCB;

typedef struct _AFS_FILE_ACCESS_RELEASE_CB
{

    ULONG           FileAccess;

    ULONGLONG       ProcessId;

    ULONGLONG       Identifier;

} AFSFileAccessReleaseCB;

//
// IO Interace control blocks for extent processing when performing
// queries via the AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS or synchronous
// results from the service
//

typedef struct _AFS_FILE_EXTENT_CB
{

    ULONG           Flags;

    ULONG           Length;

    LARGE_INTEGER   FileOffset;

    LARGE_INTEGER   CacheOffset;

    UCHAR           MD5[16];

    ULONG           DirtyOffset;

    ULONG           DirtyLength;

} AFSFileExtentCB;

typedef struct _AFS_REQUEST_EXTENTS_CB
{

    ULONG           Flags;

    LARGE_INTEGER   ByteOffset;

    ULONG           Length;

} AFSRequestExtentsCB;

//
// Extent processing when the file system calls the service to
// release extents through the AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS interface
//

typedef struct _AFS_RELEASE_EXTENTS_CB
{

    ULONG           Flags;

    ULONG           ExtentCount;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

    LARGE_INTEGER   CreateTime;

    LARGE_INTEGER   AllocationSize;

    AFSFileExtentCB FileExtents[ 1];

} AFSReleaseExtentsCB;

//
// This is the control structure used when the service passes the extent
// information via the IOCTL_AFS_SET_FILE_EXTENTS interface
//

typedef struct _AFS_SET_FILE_EXTENTS_CB
{

    AFSFileID       FileId;

    ULONG           ExtentCount;

    ULONG           ResultStatus;

    AFSFileExtentCB FileExtents[ 1];

} AFSSetFileExtentsCB;

//
// This is the control structure used when the service passes the extent
// information via the IOCTL_AFS_RELEASE_FILE_EXTENTS interface
//

#define AFS_RELEASE_EXTENTS_FLAGS_RELEASE_ALL       0x00000001

typedef struct _AFS_RELEASE_FILE_EXTENTS_CB
{

    ULONG           Flags;

    AFSFileID       FileId;

    ULONG           ExtentCount;

    LARGE_INTEGER   HeldExtentCount;

    AFSFileExtentCB FileExtents[ 1];

} AFSReleaseFileExtentsCB;

//
// These are the control structures that the filesystem returns from a
// IOCTL_AFS_RELEASE_FILE_EXTENTS
//

typedef struct _AFS_RELEASE_FILE_EXTENTS_RESULT_FILE_CB
{
    AFSFileID       FileId;

    ULONG           Flags;

    GUID            AuthGroup; /* Length: sizeof(GUID) */

    ULONG           ExtentCount;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

    LARGE_INTEGER   CreateTime;

    LARGE_INTEGER   AllocationSize;

    AFSFileExtentCB FileExtents[ 1];

} AFSReleaseFileExtentsResultFileCB;

typedef struct _AFS_RELEASE_FILE_EXTENTS_RESULT_CB
{
    ULONG           SerialNumber;

    ULONG           Flags;

    ULONG           FileCount;

    AFSReleaseFileExtentsResultFileCB Files[ 1];

} AFSReleaseFileExtentsResultCB;


typedef struct _AFS_EXTENT_FAILURE_CB
{

    AFSFileID       FileId;

    ULONG           FailureStatus;

    GUID            AuthGroup;      // Length: sizeof(GUID) */

} AFSExtentFailureCB;

//
// File update CB
//

typedef struct _AFS_FILE_UPDATE_CB
{

    AFSFileID       ParentId;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

    LARGE_INTEGER   CreateTime;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;

    ULONG           EaSize;

    char            EaBuffer[ 1];

} AFSFileUpdateCB;

//
// File update CB result
//

typedef struct _AFS_FILE_UPDATE_RESULT_CB
{

    LARGE_INTEGER   ParentDataVersion;

    AFSDirEnumEntry DirEnum;

} AFSFileUpdateResultCB;

//
// File delete CB
//

typedef struct _AFS_FILE_DELETE_CB
{

    AFSFileID       ParentId;        /* Must be directory */

    ULONGLONG       ProcessId;

                                     /* File Name and FileID in Common Request Block */

} AFSFileDeleteCB;

typedef struct _AFS_FILE_DELETE_RESULT_CB
{

    LARGE_INTEGER   ParentDataVersion;

} AFSFileDeleteResultCB;

//
// File rename CB
//

typedef struct _AFS_FILE_RENAME_CB
{

    AFSFileID       SourceParentId;        /* Must be directory */

    AFSFileID       TargetParentId;        /* Must be directory */

                                           /* Source Name and FileID in Common Request Block */

    USHORT          TargetNameLength;

    WCHAR           TargetName[ 1];

} AFSFileRenameCB;

typedef struct _AFS_FILE_RENAME_RESULT_CB
{

    LARGE_INTEGER   SourceParentDataVersion;

    LARGE_INTEGER   TargetParentDataVersion;

    AFSDirEnumEntry DirEnum;

} AFSFileRenameResultCB;


//
// File Hard Link CB
//

typedef struct _AFS_FILE_HARDLINK_CB
{

    AFSFileID       SourceParentId;        /* Must be directory */

    AFSFileID       TargetParentId;        /* Must be directory */

    BOOLEAN         bReplaceIfExists;

                                           /* Source Name and FileID in Common Request Block */

    USHORT          TargetNameLength;

    WCHAR           TargetName[ 1];

} AFSFileHardLinkCB;

typedef struct _AFS_FILE_HARDLINK_RESULT_CB
{

    LARGE_INTEGER   SourceParentDataVersion;

    LARGE_INTEGER   TargetParentDataVersion;

    AFSDirEnumEntry DirEnum;

} AFSFileHardLinkResultCB;


//
// Control structures for AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID
// and AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME
//
// The response to these requests is a AFSDirEnumEntry
//

typedef struct _AFS_FILE_EVAL_TARGET_CB
{

    AFSFileID       ParentId;

} AFSEvalTargetCB;


typedef struct _AFS_FILE_EVAL_RESULT_CB
{

    LARGE_INTEGER   ParentDataVersion;

    AFSDirEnumEntry DirEnum;

} AFSFileEvalResultCB;


//
// Control structure for read and write requests through the PIOCtl interface
//
// CommRequest FileId field contains the active directory

typedef struct _AFS_PIOCTL_IO_CB
{
    ULONG       RequestId;

    AFSFileID   RootId;

    ULONG       BufferLength;

    void        *MappedBuffer;

} AFSPIOCtlIORequestCB;

//
// The returned information for the IO Request
//

typedef struct _AFS_PIOCTL_IO_RESULT_CB
{

    ULONG       BytesProcessed;

} AFSPIOCtlIOResultCB;


//
// Control structure for open and close requests through the PIOCtl interface
//
// CommRequest FileId field contains the active directory
//
// There is no return structure.
//
typedef struct _AFS_PIOCTL_OPEN_CLOSE_CB
{

    ULONG       RequestId;

    AFSFileID   RootId;

} AFSPIOCtlOpenCloseRequestCB;

//
// Cache invalidation control block
//

typedef struct _AFS_INVALIDATE_CACHE_CB
{

    AFSFileID   FileID;

    ULONG       FileType;

    BOOLEAN     WholeVolume;

    ULONG       Reason;

} AFSInvalidateCacheCB;

//
// Network Status Control Block
//

typedef struct _AFS_NETWORK_STATUS_CB
{

    BOOLEAN     Online;

} AFSNetworkStatusCB;

//
// Volume Status Control Block
//

typedef struct _AFS_VOLUME_STATUS_CB
{

    AFSFileID   FileID;         // only cell and volume fields are set

    BOOLEAN     Online;

} AFSVolumeStatusCB;


typedef struct _AFS_SYSNAME
{

    ULONG       Length;         /* bytes */

    WCHAR       String[AFS_MAX_SYSNAME_LENGTH];

} AFSSysName;

//
// SysName Notification Control Block
//   Sent as the buffer with IOCTL_AFS_SYSNAME_NOTIFICATION
//   There is no response
//

typedef struct _AFS_SYSNAME_NOTIFICATION_CB
{

    ULONG       Architecture;

    ULONG       NumberOfNames;

    AFSSysName  SysNames[1];

} AFSSysNameNotificationCB;


//
// File System Status Query Control Block
//   Received as a response to IOCTL_AFS_STATUS_REQUEST
//
typedef struct _AFS_DRIVER_STATUS_RESPONSE_CB
{

    ULONG       Status;         // bit flags - see below

} AFSDriverStatusRespCB;

// Bit flags
#define AFS_DRIVER_STATUS_READY         0
#define AFS_DRIVER_STATUS_NOT_READY     1
#define AFS_DRIVER_STATUS_NO_SERVICE    2

//
// Byte Range Lock Request
//
typedef struct _AFS_BYTE_RANGE_LOCK_REQUEST
{
    ULONG               LockType;

    LARGE_INTEGER       Offset;

    LARGE_INTEGER       Length;

} AFSByteRangeLockRequest;

#define AFS_BYTE_RANGE_LOCK_TYPE_SHARED 0
#define AFS_BYTE_RANGE_LOCK_TYPE_EXCL   1


//
// Byte Range Lock Request Control Block
//
// Set ProcessId and FileId in the Comm Request Block
//
typedef struct _AFS_BYTE_RANGE_LOCK_REQUEST_CB
{

    ULONG                       Count;

    ULONGLONG                   ProcessId;

    AFSByteRangeLockRequest     Request[1];

} AFSByteRangeLockRequestCB;

//
// Byte Range Lock Result
//
typedef struct _AFS_BYTE_RANGE_LOCK_RESULT
{

    ULONG               LockType;

    LARGE_INTEGER       Offset;

    LARGE_INTEGER       Length;

    ULONG               Status;

} AFSByteRangeLockResult;

//
// Byte Range Lock Results Control Block
//

typedef struct _AFS_BYTE_RANGE_LOCK_RESULT_CB
{

    AFSFileID                   FileId;

    ULONG                       Count;

    AFSByteRangeLockResult      Result[1];

} AFSByteRangeLockResultCB;

//
// Set Byte Range Lock Results Control Block
//

typedef struct _AFS_SET_BYTE_RANGE_LOCK_RESULT_CB
{

    ULONG                       SerialNumber;

    AFSFileID                   FileId;

    ULONG                       Count;

    AFSByteRangeLockResult      Result[1];

} AFSSetByteRangeLockResultCB;


//
// Byte Range Unlock Request Control Block
//

typedef struct _AFS_BYTE_RANGE_UNLOCK_CB
{

    ULONG                       Count;

    ULONGLONG                   ProcessId;

    AFSByteRangeLockRequest     Request[1];

} AFSByteRangeUnlockRequestCB;


//
// Byte Range Unlock Request Control Block
//

typedef struct _AFS_BYTE_RANGE_UNLOCK_RESULT_CB
{

    ULONG                       Count;

    AFSByteRangeLockResult      Result[1];

} AFSByteRangeUnlockResultCB;


//
// Control structure for read and write requests through the PIPE interface
//
// CommRequest FileId field contains the active directory

typedef struct _AFS_PIPE_IO_CB
{
    ULONG       RequestId;

    AFSFileID   RootId;

    ULONG       BufferLength;

} AFSPipeIORequestCB;   // For read requests the buffer is mapped in the request cb block.
                        // For write requests, the buffer immediately follows this structure

//
// The returned information for the Pipe IO Request. Note that this is
// only returned in the write request. Read request info is returned in
// the request cb
//

typedef struct _AFS_PIPE_IO_RESULT_CB
{

    ULONG       BytesProcessed;

} AFSPipeIOResultCB;

//
// Control structure for set and query info requests through the PIPE interface
//

typedef struct _AFS_PIPE_INFO_CB
{

    ULONG       RequestId;

    AFSFileID   RootId;

    ULONG       InformationClass;

    ULONG       BufferLength;

} AFSPipeInfoRequestCB;   // For query info requests the buffer is mapped in the request cb block.
                          // For set info requests, the buffer immediately follows this structure

//
// Control structure for open and close requests through the Pipe interface
//
// CommRequest FileId field contains the active directory
//
// There is no return structure.
//
typedef struct _AFS_PIPE_OPEN_CLOSE_CB
{

    ULONG       RequestId;

    AFSFileID   RootId;

} AFSPipeOpenCloseRequestCB;


//
// Hold Fid Request Control Block
//

typedef struct _AFS_HOLD_FID_REQUEST_CB
{

    ULONG                       Count;

    AFSFileID                   FileID[ 1];

} AFSHoldFidRequestCB;


typedef struct _AFS_FID_RESULT
{

    AFSFileID                   FileID;

    ULONG                       Status;

} AFSFidResult;

typedef struct _AFS_HOLD_FID_RESULT_CB
{

    ULONG                       Count;

    AFSFidResult                Result[ 1];

} AFSHoldFidResultCB;


//
// Release Fid Request Control Block
//

typedef struct _AFS_RELEASE_FID_REQUEST_CB
{

    ULONG                       Count;

    AFSFileID                   FileID[ 1];

} AFSReleaseFidRequestCB;

typedef struct _AFS_RELEASE_FID_RESULT_CB
{

    ULONG                       Count;

    AFSFidResult                Result[ 1];

} AFSReleaseFidResultCB;


//
// File cleanup CB
//

typedef struct _AFS_FILE_CLEANUP_CB
{

    AFSFileID       ParentId;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

    LARGE_INTEGER   CreateTime;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;

    ULONGLONG       ProcessId;

    ULONG           FileAccess;

    ULONGLONG       Identifier;

} AFSFileCleanupCB;

typedef struct _AFS_FILE_CLEANUP_RESULT_CB
{

    LARGE_INTEGER   ParentDataVersion;

} AFSFileCleanupResultCB;


//
// Trace configuration cb
//

typedef struct _AFS_DEBUG_TRACE_CONFIG_CB
{

    ULONG       TraceLevel;

    ULONG       Subsystem;

    ULONG       TraceBufferLength;

    ULONG       DebugFlags;

} AFSTraceConfigCB;

//
// Object Status Information request
//

typedef struct _AFS_REDIR_GET_OBJECT_STATUS_CB
{

    AFSFileID       FileID;

    USHORT          FileNameLength;

    WCHAR           FileName[ 1];

} AFSGetStatusInfoCB;

typedef struct _AFS_REDIR_OBJECT_STATUS_CB
{

    AFSFileID	            FileId;

    AFSFileID               TargetFileId;

    LARGE_INTEGER           Expiration;		/* FILETIME */

    LARGE_INTEGER           DataVersion;

    ULONG                   FileType;		/* File, Dir, MountPoint, Symlink */

    ULONG                   ObjectFlags;

    LARGE_INTEGER           CreationTime;	/* FILETIME */

    LARGE_INTEGER           LastAccessTime;	/* FILETIME */

    LARGE_INTEGER           LastWriteTime;	/* FILETIME */

    LARGE_INTEGER           ChangeTime;		/* FILETIME */

    ULONG                   FileAttributes;	/* NTFS FILE_ATTRIBUTE_xxxx see below */

    LARGE_INTEGER           EndOfFile;

    LARGE_INTEGER           AllocationSize;

    ULONG                   EaSize;

    ULONG                   Links;

} AFSStatusInfoCB;

//
// Auth Group (Process and Thread) Processing
//
// afsredir.sys implements a set of generic Authentication Group
// operations that can be executed by processes.  The model supports
// one or more authentication groups per process.  A process may switch
// the active AuthGroup for any thread to any other AuthGroup the process
// is a member of.  However, processes cannot assign itself to an
// AuthGroup that it is not presently a member of.  A process can reset
// its AuthGroup to the SID-AuthGroup or can create a new AuthGroup that
// has not previously been used.
//
//  IOCTL_AFS_AUTHGROUP_CREATE_AND_SET
//	Creates a new AuthGroup and either activates it for
//  	the process or the current thread.  If set as the
//	new process AuthGroup, the prior AuthGroup list is
//	cleared.
//
//  IOCTL_AFS_AUTHGROUP_QUERY
//	Returns a list of the AuthGroup GUIDS associated
//	with the current process, the current process GUID,
//	and the current thread GUID.
//
//  IOCTL_AFS_AUTHGROUP_SET
//	Permits the current AuthGroup for the process or
//	thread to be set to the specified GUID.  The GUID
//	must be in the list of current values for the process.
//
//  IOCTL_AFS_AUTHGROUP_RESET
//	Resets the current AuthGroup for the process or
//	thread to the SID-AuthGroup
//
//  IOCTL_AFS_AUTHGROUP_SID_CREATE
//	Given a SID as input, assigns a new AuthGroup GUID.
//	(May only be executed by LOCAL_SYSTEM or the active SID)
//
//  IOCTL_AFS_AUTHGROUP_SID_QUERY
//	Given a SID as input, returns the associated AuthGroup GUID.
//
//  IOCTL_AFS_AUTHGROUP_LOGON_CREATE
//	Given a logon Session as input, assigns a new AuthGroup GUID.
//	(May only be executed by LOCAL_SYSTEM.)
//
// New processes inherit only the active AuthGroup at the time of process
// creation.  Either that of the active thread (if set) or the process.
// All of the other AuthGroups associated with a parent process are
// off-limits.
//

//
// Auth Group processing flags
//

#define AFS_PAG_FLAGS_SET_AS_ACTIVE         0x00000001 // If set, the newly created authgroup is set to the active group
#define AFS_PAG_FLAGS_THREAD_AUTH_GROUP     0x00000002 // If set, the request is targeted for the thread not the process

typedef struct _AFS_AUTH_GROUP_REQUEST
{

    USHORT              SIDLength; // If zero the SID of the caller is used

    ULONG               SessionId; // If -1 the session id of the caller is used

    ULONG               Flags;

    GUID                AuthGroup; // The auth group for certain requests

    WCHAR               SIDString[ 1];

} AFSAuthGroupRequestCB;

//
// Reparse tag AFS Specific information buffer
//

#define OPENAFS_SUBTAG_MOUNTPOINT 1
#define OPENAFS_SUBTAG_SYMLINK    2
#define OPENAFS_SUBTAG_UNC        3

#define OPENAFS_MOUNTPOINT_TYPE_NORMAL   L'#'
#define OPENAFS_MOUNTPOINT_TYPE_RW       L'%'

typedef struct _AFS_REPARSE_TAG_INFORMATION
{

    ULONG SubTag;

    union
    {
        struct
        {
            ULONG  Type;
            USHORT MountPointCellLength;
            USHORT MountPointVolumeLength;
            WCHAR  Buffer[1];
        } AFSMountPoint;

        struct
        {
            BOOLEAN RelativeLink;
            USHORT  SymLinkTargetLength;
            WCHAR   Buffer[1];
        } AFSSymLink;

        struct
        {
            USHORT UNCTargetLength;
            WCHAR  Buffer[1];
        } UNCReferral;
    };

} AFSReparseTagInfo;

#endif /* _AFS_USER_STRUCT_H */

