#ifndef _KD_COMMON_H
#define _KD_COMMON_H


//
// Common defines for shared code
//

typedef struct _KD_FILE_ID 
{

    ULONG       Hash;

    ULONG	Cell;

    ULONG	Volume;
   
    ULONG	Vnode;
    
    ULONG	Unique;

} KDFileID;

//
// This control structure is the request block passed to the filter. The filter will populate the structure
// when it requires a request to be handled by the service.
//

typedef struct _KD_COMM_REQUEST_BLOCK
{

    ULONG           ProcessId;          /* Session Identifier */

    KDFileID	    FileId;		/* Initialize unused elements to 0 */

    ULONG           RequestType;

    ULONG           RequestIndex;	/* Must return to caller */

    ULONG           RequestFlags;	/* KDFSD_REQUEST_FLAG_xxxx */

    ULONG           NameLength;  	// Length of the name in bytes

    ULONG           DataOffset;   	// This offset is from the end of the structure, including the name

    ULONG           DataLength;

    ULONG           SIDOffset;      // From the end of the structure

    ULONG           SIDLength;      // The length of the wchar representation of the SID

    ULONG           ResultBufferLength;	/* Do not exceed this length in response */

    WCHAR           Name[ 1];

} KDFsdCommRequest;

//
// This is the result block passed back to the redirector after a request has been handled
//

typedef struct _KD_COMM_RESULT_BLOCK
{

    ULONG           RequestIndex;	/* Must match the KDFsdCommRequest value */

    ULONG           ResultStatus;	/* NTSTATUS_xxx */

    ULONG           ResultBufferLength;	/* Not to exceed KDFsdCommRequest ResultBufferLength */

    ULONG           Reserved;           /* To ease Quad Alignment */

    char            ResultData[ 1];

} KDFsdCommResult;

//
// Symbolic link name
//

#define KDFSD_SYMLINK                   "\\\\.\\KDFSD"
#define KDFSD_SYMLINK_W                 L"\\\\.\\KDFSD"

//
// Payload buffer length
//

#define KDFSD_PAYLOAD_BUFFER_SIZE       (16 * 1024)

//
// IOCtl definitions
//

#define IOCTL_KDFSD_INITIALIZE_CONTROL_DEVICE   CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1001, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_KDFSD_INITIALIZE_REDIRECTOR_DEVICE CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1002, METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_KDFSD_PROCESS_IRP_REQUEST         CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1003, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_KDFSD_PROCESS_IRP_RESULT          CTL_CODE( FILE_DEVICE_DISK_FILE_SYSTEM, 0x1004, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// Request types
//

#define KD_FSD_REQUEST_TYPE_DIR_ENUM                0x00000001
#define KD_FSD_REQUEST_TYPE_CREATE_FILE             0x00000002
#define KD_FSD_REQUEST_TYPE_REQUEST_FILE_EXTENTS    0x00000003
#define KD_FSD_REQUEST_TYPE_RELEASE_FILE_EXTENTS    0x00000004
#define KD_FSD_REQUEST_TYPE_UPDATE_FILE             0x00000005
#define KD_FSD_REQUEST_TYPE_DELETE_FILE             0x00000006
#define KD_FSD_REQUEST_TYPE_RENAME_FILE             0x00000007
#define KD_FSD_REQUEST_TYPE_FLUSH_FILE	            0x00000008
#define KD_FSD_REQUEST_TYPE_OPEN_FILE 	            0x00000009
#define KD_FSD_REQUEST_TYPE_EVAL_TARGET_BY_ID       0x0000000A
#define KD_FSD_REQUEST_TYPE_EVAL_TARGET_BY_NAME     0x0000000B

//
// Request Flags
//

#define KDFSD_REQUEST_FLAG_RESPONSE_REQUIRED          0x00000001
#define KDFSD_REQUEST_FLAG_CASE_SENSITIVE             0x00000002

//
// Ststua codes that can returned for various requests
//

#ifndef KD_FSD_KERNEL_MODE

#define STATUS_SUCCESS			         0x00000000
#define STATUS_MORE_ENTRIES              0x00000105
#define STATUS_NO_MORE_FILES             0x80000006

#endif

//
// Directory query CB
//

typedef struct _KD_DIR_QUERY_CB
{

    ULONG_PTR        EnumHandle;  // If this is 0 then it is a new query,
                                  // otherwise it is the FileIndex of the last 
                                  // entry processed.

} KDFsdDirQueryCB;

//
// Directory enumeration control block
// Entries are aligned on a QuadWord boundary
// 

typedef struct _KD_DIR_ENUM_ENTRY
{

    KDFileID	    FileId;

    ULONG           FileIndex;		/* Incremented  */

    LARGE_INTEGER   Expiration;		/* FILETIME */

    ULONG           DataVersion;

    ULONG           FileType;		/* File, Dir, MountPoint, Symlink */
    
    LARGE_INTEGER   CreationTime;	/* FILETIME */

    LARGE_INTEGER   LastAccessTime;	/* FILETIME */

    LARGE_INTEGER   LastWriteTime;	/* FILETIME */

    LARGE_INTEGER   ChangeTime;		/* FILETIME */

    LARGE_INTEGER   EndOfFile;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;	/* NTFS FILE_ATTRIBUTE_xxxx see below */

    ULONG           FileNameLength;

    ULONG           EaSize;

    ULONG           Links;

    ULONG           FileNameOffset;		    /* From beginning of this structure */

    ULONG           TargetNameOffset;       /* From beginning of this structure */

    ULONG           TargetNameLength;

    KDFileID	    TargetFileId;          /* Target fid for mp's and symlinks */

    CCHAR           ShortNameLength;

    WCHAR           ShortName[12];

    /* Long Filename and Target (Symlink and MountPoint only) to follow without NULs */

} KDFsdDirEnumEntry;


typedef struct _KD_DIR_ENUM_RESP {

    ULONG_PTR       EnumHandle;

    KDFsdDirEnumEntry Entry[ 1];     /* Each entry is Quad aligned */

} KDFsdDirEnumResp;

// AFS File Types

#define KD_AFS_FILE_TYPE_UNKNOWN		0	/* an unknown object */
#define KD_AFS_FILE_TYPE_FILE			1	/* a file */
#define KD_AFS_FILE_TYPE_DIRECTORY		2	/* a dir */
#define KD_AFS_FILE_TYPE_SYMLINK		3	/* a symbolic link */
#define KD_AFS_FILE_TYPE_MOUNTPOINT		4	/* a mount point */
#define KD_AFS_FILE_TYPE_DFSLINK           	5       /* a Microsoft Dfs link */
#define KD_AFS_FILE_TYPE_INVALID           	99      /* an invalid link */

//
// File attributes
//

#ifndef KD_FSD_KERNEL_MODE

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

typedef struct _KD_VOLUME_INFORMATION 
{

    LARGE_INTEGER   TotalAllocationUnits;	/* Partition Max Blocks */

    LARGE_INTEGER   AvailableAllocationUnits;	/* Partition Blocks Avail */

    LARGE_INTEGER   VolumeCreationTime;		/* AFS Last Update - Not Creation */

    ULONG           Characteristics;		/* FILE_READ_ONLY_DEVICE (if readonly) 
                                             * FILE_REMOTE_DEVICE (always)
						                     * FILE_DEVICE_IS_MOUNTED (if file server is up)
						                     */

    ULONG           SectorsPerAllocationUnit;	/* = 1 */

    ULONG           BytesPerSector;		/* = 1024 */

    ULONG           CellID;			/* AFS Cell ID */

    ULONG           VolumeID;			/* AFS Volume ID */

    ULONG           VolumeLabelLength;

    WCHAR           VolumeLabel[1];		/* Volume:Cell */

} KDFsdVolumeInfoCB;

//
// Possible characteristics
//

#ifndef KD_FSD_KERNEL_MODE

#define FILE_REMOVABLE_MEDIA            0x00000001
#define FILE_READ_ONLY_DEVICE           0x00000002
#define FILE_REMOTE_DEVICE              0x00000010

#endif

//
// File create CB
//

typedef struct _KD_FILE_CREATE_CB
{

    KDFileID	    ParentId;

    LARGE_INTEGER   CreationTime;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

    LARGE_INTEGER   EndOfFile;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;

    ULONG           EaSize;

} KDFsdFileCreateCB;

typedef struct _KD_FILE_CREATE_RESULT_CB
{

    KDFileID 	    FileId;

    ULONG	        ParentDataVersion;

    CCHAR           ShortNameLength;

    WCHAR           ShortName[12];

} KDFsdFileCreateResultCB;

//
// File open CB
//

typedef struct _KD_FILE_OPEN_CB
{

    KDFileID	    ParentId;

    ULONG           DesiredAccess;

    ULONG           ShareAccess;

} KDFsdFileOpenCB;

typedef struct _KD_FILE_OPEN_RESULT_CB
{

    ULONG           GrantedAccess;

} KDFsdFileOpenResultCB;

//
// IO Interace control blocks
//

typedef struct _KD_FILE_EXTENT_CB
{

    ULONG               Flags;

    ULONG               Length;

    LARGE_INTEGER       FileOffset;

    LARGE_INTEGER       CacheOffset;

} KDFsdFileExtentCB;

typedef struct _KD_FILE_REQUEST_EXTENTS_CB
{

    ULONG 		        Flags;

    LARGE_INTEGER       ByteOffset;

    ULONG               Length;

} KDFsdFileRequestExtentsCB;

typedef struct _KD_FILE_REQUEST_EXTENTS_RESULT_CB
{

    ULONG       ExtentCount;

    KDFsdFileExtentCB     FileExtents[ 1];

} KDFsdFileRequestExtentsResultCB;

typedef struct _KD_FILE_RELEASE_EXTENTS_CB
{

    ULONG 		        Flags;

    ULONG               ExtentCount;

    KDFsdFileExtentCB     FileExtents[ 1];

} KDFsdFileReleaseExtentsCB;

//
// File updatee CB
//

typedef struct _KD_FILE_UPDATE_CB
{
    LARGE_INTEGER   CreationTime;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

    LARGE_INTEGER   EndOfFile;

    LARGE_INTEGER   AllocationSize;

    ULONG           FileAttributes;

    ULONG           EaSize;

} KDFsdFileUpdateCB;

//
// File delete CB
//

typedef struct _KD_FILE_DELETE_CB
{

    KDFileID	    ParentId;		/* Must be directory */

    /* File Name and FileID in Common Request Block */

} KDFsdFileDeleteCB;

typedef struct _KD_FILE_DELETE_RESULT_CB
{
    ULONG	    ParentDataVersion;

} KDFsdFileDeleteResultCB;


//
// File rename CB
//

typedef struct _KD_FILE_RENAME_CB
{

    KDFileID	    SourceParentId;		/* Must be directory */

    KDFileID        TargetParentId;		/* Must be directory */

    /* Source Name and FileID in Common Request Block */

    USHORT          TargetNameLength;

    WCHAR           TargetName[ 1];

} KDFsdFileRenameCB;

typedef struct _KD_FILE_RENAME_RESULT_CB
{
    ULONG	    SourceParentDataVersion;
    
    ULONG	    TargetParentDataVersion;

} KDFsdFileRenameResultCB;

//
// Control structures for KD_FSD_REQUEST_TYPE_EVAL_TARGET_BY_ID
// and KD_FSD_REQUEST_TYPE_EVAL_TARGET_BY_NAME
//

typedef struct _KD_FILE_EVAL_TARGET_CB
{

    KDFileID        ParentId;

} KDFsdEvalTargetCB;

//
// The response to these requests are a KDFsdDirEnumEntry
//



#endif
