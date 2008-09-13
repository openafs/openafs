#ifndef _AFS_USER_COMMON_H
#define _AFS_USER_COMMON_H

//
// Common defines for shared code
//

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

    ULONG           ProcessId;          /* Session Identifier */

    AFSFileID       FileId;             /* Initialize unused elements to 0 */

    ULONG           RequestType;

    ULONG           RequestIndex;    /* Must return to caller */

    ULONG           RequestFlags;    /* AFS_REQUEST_FLAG_xxxx */

    ULONG           NameLength;      // Length of the name in bytes

    ULONG           DataOffset;       // This offset is from the end of the structure, including the name

    ULONG           DataLength;

    ULONG           SIDOffset;      // From the end of the structure

    ULONG           SIDLength;      // The length of the wchar representation of the SID

    ULONG           ResultBufferLength;    /* Do not exceed this length in response */

    WCHAR           Name[ 1];

} AFSCommRequest;

//
// This is the result block passed back to the redirector after a request has been handled
//

typedef struct _AFS_COMM_RESULT_BLOCK
{

    ULONG           RequestIndex;        /* Must match the AFSCommRequest value */

    ULONG           ResultStatus;        /* NTSTATUS_xxx */

    ULONG           ResultBufferLength;    /* Not to exceed AFSCommRequest ResultBufferLength */

    ULONG           Reserved;           /* To ease Quad Alignment */

    char            ResultData[ 1];

} AFSCommResult;

//
// Symbolic link name
//

#define AFS_SYMLINK                    "\\\\.\\AFSRedirector"
#define AFS_SYMLINK_W                 L"\\\\.\\AFSRedirector"

#define AFS_PIOCTL_FILE_INTERFACE_NAME  L"_._AFS_IOCTL_._"
#define AFS_PIOCTL_FILE_INTERFACE_NAME_ROOT  L"\\AFS\\ALL\\_._AFS_IOCTL_._"

//
// Payload buffer length
//

#define AFS_PAYLOAD_BUFFER_SIZE       (16 * 1024)

//
// IOCtl definitions
//

#define IOCTL_AFS_INITIALIZE_CONTROL_DEVICE     CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1001, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE  CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1002, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_PROCESS_IRP_REQUEST           CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1003, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_PROCESS_IRP_RESULT            CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1004, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_SET_FILE_EXTENTS              CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1005, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_RELEASE_FILE_EXTENTS          CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1006, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_INVALIDATE_CACHE              CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1007, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_RELEASE_FILE_EXTENTS_DONE     CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1008, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_NETWORK_STATUS                CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1009, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_VOLUME_STATUS                 CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x100A, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_AFS_SHUTDOWN                      CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x100B, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// Request types
//

#define AFS_REQUEST_TYPE_DIR_ENUM                0x00000001
#define AFS_REQUEST_TYPE_CREATE_FILE             0x00000002
#define AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS    0x00000003
#define AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS    0x00000004
#define AFS_REQUEST_TYPE_UPDATE_FILE             0x00000005
#define AFS_REQUEST_TYPE_DELETE_FILE             0x00000006
#define AFS_REQUEST_TYPE_RENAME_FILE             0x00000007
#define AFS_REQUEST_TYPE_FLUSH_FILE              0x00000008
#define AFS_REQUEST_TYPE_OPEN_FILE               0x00000009
#define AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID       0x0000000A
#define AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME     0x0000000B
#define AFS_REQUEST_TYPE_PIOCTL_READ             0x0000000C
#define AFS_REQUEST_TYPE_PIOCTL_WRITE            0x0000000D
#define AFS_REQUEST_TYPE_PIOCTL_OPEN             0x0000000E
#define AFS_REQUEST_TYPE_PIOCTL_CLOSE            0x0000000F

//
// Request Flags
//

#define AFS_REQUEST_FLAG_SYNCHRONOUS             0x00000001 // The service must call back through the
                                                            // IOCTL_AFS_PROCESS_IRP_RESULT IOCtl to ack
                                                            // the request with a response. The absense of 
                                                            // this flag indicates no call should be made to 
                                                            // the IOCTL_AFS_PROCESS_IRP_RESULT IOCtl and if a 
                                                            // response is required for the call it is to be made
                                                            // through an IOCtl call

#define AFS_REQUEST_FLAG_CASE_SENSITIVE          0x00000002

//
// Status codes that can returned for various requests
//

#if !defined(AFS_KERNEL_MODE) && !defined(STATUS_SUCCESS)

#define STATUS_SUCCESS                   0x00000000
#define STATUS_MORE_ENTRIES              0x00000105
#define STATUS_NO_MORE_FILES             0x80000006

#endif

//
// Control block passed to IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE
//

typedef struct _AFS_CACHE_FILE_INFO_CB
{
    LARGE_INTEGER  ExtentCount;         // Number of extents in the current data cache

    ULONG       CacheBlockSize;         // Size, in bytes, of the current cache block

    ULONG       CacheFileNameLength;    // size, in bytes, of the cache file name

    WCHAR       CacheFileName[ 1];      // Fully qualified cache file name in the form
                                        // \??\C:\OPenAFSDir\CacheFile.dat
} AFSCacheFileInfo;

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

    /* Long Filename and Target (Symlink and MountPoint only) to follow without NULs */

} AFSDirEnumEntry;


typedef struct _AFS_DIR_ENUM_RESP 
{

    ULONG_PTR       EnumHandle;

    AFSDirEnumEntry Entry[ 1];     /* Each entry is Quad aligned */

} AFSDirEnumResp;

// AFS File Types

#define AFS_FILE_TYPE_UNKNOWN            0    /* an unknown object */
#define AFS_FILE_TYPE_FILE               1    /* a file */
#define AFS_FILE_TYPE_DIRECTORY          2    /* a dir */
#define AFS_FILE_TYPE_SYMLINK            3    /* a symbolic link */
#define AFS_FILE_TYPE_MOUNTPOINT         4    /* a mount point */
#define AFS_FILE_TYPE_DFSLINK            5    /* a Microsoft Dfs link */
#define AFS_FILE_TYPE_INVALID            99   /* an invalid link */

//
// File attributes
//

#ifndef AFS_KERNEL_MODE

#define FILE_ATTRIBUTE_READONLY             0x00000001  // winnt
#define FILE_ATTRIBUTE_HIDDEN               0x00000002  // winnt
#define FILE_ATTRIBUTE_SYSTEM               0x00000004  // winnt

#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  // winnt
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020  // winnt
#define FILE_ATTRIBUTE_DEVICE               0x00000040  // winnt
#define FILE_ATTRIBUTE_NORMAL               0x00000080  // winnt

#endif

//
// Volume information CB passed in the create request
//

typedef struct _AFS_VOLUME_INFORMATION 
{

    LARGE_INTEGER   TotalAllocationUnits;       /* Partition Max Blocks */

    LARGE_INTEGER   AvailableAllocationUnits;   /* Partition Blocks Avail */

    LARGE_INTEGER   VolumeCreationTime;         /* AFS Last Update - Not Creation */

    ULONG           Characteristics;            /* FILE_READ_ONLY_DEVICE (if readonly) 
                                                 * FILE_REMOTE_DEVICE (always)
                                                 * FILE_DEVICE_IS_MOUNTED (if file server is up)
                                                 */

    ULONG           SectorsPerAllocationUnit;   /* = 1 */

    ULONG           BytesPerSector;             /* = 1024 */

    ULONG           CellID;                     /* AFS Cell ID */

    ULONG           VolumeID;                   /* AFS Volume ID */

    ULONG           VolumeLabelLength;

    WCHAR           VolumeLabel[20];            /* Volume:Cell */

} AFSVolumeInfoCB;

//
// Possible characteristics
//

#ifndef AFS_KERNEL_MODE

#define FILE_REMOVABLE_MEDIA            0x00000001
#define FILE_READ_ONLY_DEVICE           0x00000002
#define FILE_REMOTE_DEVICE              0x00000010

#endif

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

} AFSFileOpenCB;

typedef struct _AFS_FILE_OPEN_RESULT_CB
{

    ULONG           GrantedAccess;

} AFSFileOpenResultCB;

//
// Flags which can be specified for each extent in the AFSFileExtentCB structure
//

#define AFS_EXTENT_FLAG_DIRTY   1   // The specified extent requires flushing, this can be
                                    // specified by the file system during a release of the
                                    // extent

#define AFS_EXTENT_FLAG_RELEASE 2   // The presence of this flag during a AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS
                                    // call from the file system indicates to the service that the file system
                                    // no longer requires the extents and they can be completely released. The
                                    // absense of this flag tells the service that the extent should not be
                                    // dereferenced; this is usually the case when the file system tells the
                                    // service to flush a range of exents but do not release them

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

} AFSFileExtentCB;

typedef struct _AFS_REQUEST_EXTENTS_CB
{

    ULONG           Flags;

    LARGE_INTEGER   ByteOffset;

    ULONG           Length;

} AFSRequestExtentsCB;

typedef struct _AFS_REQUEST_EXTENTS_RESULT_CB
{

    ULONG           ExtentCount;

    AFSFileExtentCB FileExtents[ 1];

} AFSRequestExtentsResultCB;

//
// Extent processing when the file system calls the service to
// release extents through the AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS interface
//

typedef struct _AFS_RELEASE_EXTENTS_CB
{

    ULONG           Flags;

    ULONG           ExtentCount;

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

typedef struct _AFS_RELEASE_FILE_EXTENTS_CB
{

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

    ULONG           ProcessId;

    ULONG           ExtentCount;

    AFSFileExtentCB FileExtents[ 1];

} AFSReleaseFileExtentsResultFileCB;

typedef struct _AFS_RELEASE_FILE_EXTENTS_RESULT_CB
{
    ULONG           SerialNumber;

    ULONG           Flags;

    ULONG           FileCount;

    AFSReleaseFileExtentsResultFileCB Files[ 1];

} AFSReleaseFileExtentsResultCB;


typedef struct _AFS_RELEASE_FILE_EXTENTS_RESULT_DONE_CB
{
    ULONG           SerialNumber;

} AFSReleaseFileExtentsResultDoneCB;

//
// File update CB
//

typedef struct _AFS_FILE_UPDATE_CB
{

    AFSFileID       ParentId;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

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

    AFSDirEnumEntry DirEnum;

} AFSFileUpdateResultCB;

//
// File delete CB
//

typedef struct _AFS_FILE_DELETE_CB
{

    AFSFileID       ParentId;        /* Must be directory */

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
// Control structures for AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID
// and AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME
//
// The response to these requests is a AFSDirEnumEntry
//

typedef struct _AFS_FILE_EVAL_TARGET_CB
{

    AFSFileID       ParentId;

} AFSEvalTargetCB;


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

#define AFS_INVALIDATE_EXPIRED          1
#define AFS_INVALIDATE_FLUSHED          2
#define AFS_INVALIDATE_CALLBACK         3
#define AFS_INVALIDATE_SMB              4
#define AFS_INVALIDATE_CREDS            5
#define AFS_INVALIDATE_DATA_VERSION     6
#define AFS_INVALIDATE_DELETED          7

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

#endif
