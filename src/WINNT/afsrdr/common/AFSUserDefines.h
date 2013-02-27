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

#ifndef _AFS_USER_DEFINE_H
#define _AFS_USER_DEFINE_H

//
// Symbolic link name
//

#define AFS_SYMLINK                    "\\\\.\\AFSRedirector"
#define AFS_SYMLINK_W                 L"\\\\.\\AFSRedirector"

#define AFS_PIOCTL_FILE_INTERFACE_NAME  L"_._AFS_IOCTL_._"
#define AFS_GLOBAL_ROOT_SHARE_NAME      L"ALL"

//
// Payload buffer length
//

#define AFS_PAYLOAD_BUFFER_SIZE       (16 * 1024)


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
#define AFS_REQUEST_TYPE_BYTE_RANGE_LOCK         0x00000010  // Takes AFSByteRangeLockRequestCB as INPUT
#define AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK       0x00000011  // Takes AFSByteRangeUnlockRequestCB as INPUT
#define AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL   0x00000012  // Takes AFSByteRangeUnlockRequestCB as INPUT
#define AFS_REQUEST_TYPE_GET_VOLUME_INFO         0x00000013
#define AFS_REQUEST_TYPE_HOLD_FID                0x00000014
#define AFS_REQUEST_TYPE_RELEASE_FID             0x00000015
#define AFS_REQUEST_TYPE_PIPE_TRANSCEIVE         0x00000016
#define AFS_REQUEST_TYPE_PIPE_READ               0x00000017
#define AFS_REQUEST_TYPE_PIPE_WRITE              0x00000018
#define AFS_REQUEST_TYPE_PIPE_OPEN               0x00000019
#define AFS_REQUEST_TYPE_PIPE_CLOSE              0x0000001A
#define AFS_REQUEST_TYPE_PIPE_QUERY_INFO         0x0000001B
#define AFS_REQUEST_TYPE_PIPE_SET_INFO           0x0000001C
#define AFS_REQUEST_TYPE_CLEANUP_PROCESSING      0x0000001D
#define AFS_REQUEST_TYPE_CREATE_LINK             0x0000001E
#define AFS_REQUEST_TYPE_CREATE_MOUNTPOINT       0x0000001F
#define AFS_REQUEST_TYPE_CREATE_SYMLINK          0x00000020
#define AFS_REQUEST_TYPE_RELEASE_FILE_ACCESS     0x00000021
#define AFS_REQUEST_TYPE_GET_VOLUME_SIZE_INFO    0x00000022
#define AFS_REQUEST_TYPE_HARDLINK_FILE           0x00000023
#define AFS_REQUEST_TYPE_PROCESS_READ_FILE       0x00000024
#define AFS_REQUEST_TYPE_PROCESS_WRITE_FILE      0x00000025

//
// Request Flags, these are passed up from the file system
//

#define AFS_REQUEST_FLAG_SYNCHRONOUS             0x00000001 // The service must call back through the
                                                            // IOCTL_AFS_PROCESS_IRP_RESULT IOCtl to ack
                                                            // the request with a response. The absense of
                                                            // this flag indicates no call should be made to
                                                            // the IOCTL_AFS_PROCESS_IRP_RESULT IOCtl and if a
                                                            // response is required for the call it is to be made
                                                            // through an IOCtl call

#define AFS_REQUEST_FLAG_CASE_SENSITIVE          0x00000002

#define AFS_REQUEST_FLAG_WOW64                   0x00000004 // On 64-bit systems, set if the request
                                                            // originated from a WOW64 process

#define AFS_REQUEST_FLAG_FAST_REQUEST            0x00000008 // if this flag is set, the cache manager
                                                            // responds to the request using a minimum
                                                            // of file server interaction

#define AFS_REQUEST_FLAG_HOLD_FID                0x00000010 // if this flag is set, the cache manager
                                                            // maintains a reference count on the
                                                            // evaluated file object just as if
                                                            // AFS_REQUEST_TYPE_HOLD_FID was issued.
                                                            // The reference count must be released
                                                            // using AFS_REQUEST_TYPE_RELEASE_FID.
                                                            // This flag is only valid on
                                                            // AFS_REQUEST_TYPE_EVALUATE_BY_NAME,
                                                            // AFS_REQUEST_TYPE_EVALUATE_BY_ID,
                                                            // AFS_REQUEST_TYPE_CREATE_FILE, and
                                                            // AFS_REQUEST_TYPE_OPEN_FILE.

#define AFS_REQUEST_FLAG_FLUSH_FILE              0x00000020 // Passed as a flag to the AFS_REQUEST_TYPE_CLEANUP_PROCESSING
                                                            // request when the last handle is closed.  This flag tells the
                                                            // to flush all dirty data before returning.

#define AFS_REQUEST_FLAG_FILE_DELETED            0x00000040 // Passed as a flag to the AFS_REQUEST_TYPE_CLEANUP_PROCESSING
                                                            // request to indicate the file has been marked for deletion.

#define AFS_REQUEST_FLAG_BYTE_RANGE_UNLOCK_ALL   0x00000080 // Passed as a flag to the AFS_REQUEST_TYPE_CLEANUP_PROCESSING
                                                            // request to indicate to release all BR locks on the file for the
                                                            // given process.

#define AFS_REQUEST_FLAG_CHECK_ONLY              0x00000100 // Do not perform the action, just check if the action is possible
                                                            // Only used with AFS_REQUEST_TYPE_DELETE_FILE.

#define AFS_REQUEST_LOCAL_SYSTEM_PAG             0x00000200 // Indicates that the caller is or was at some point a system
                                                            // process

#define AFS_REQUEST_FLAG_CACHE_BYPASS            0x00000400 // Indicates that the AFS_REQUEST_TYPE_PROCESS_READ_FILE or
                                                            // AFS_REQUEST_TYPE_PROCESS_WRITE_FILE is to be performed directly
                                                            // against the file server bypassing the AFS Cache file.

#define AFS_REQUEST_FLAG_LAST_COMPONENT          0x00000800 // During an AFS_REQUEST_TYPE_TARGET_BY_NAME the provided name
                                                            // is the last component in the path.

//
// Request Flags, these are passed down from the sevice
//

#define AFS_REQUEST_RELEASE_THREAD                 0x00000001 // Set on threads which are dedicated extent release threads

//
// Status codes that can returned for various requests
//

#if !defined(AFS_KERNEL_MODE) && !defined(STATUS_SUCCESS)

#define STATUS_SUCCESS                   0x00000000
#define STATUS_MORE_ENTRIES              0x00000105
#define STATUS_NO_MORE_FILES             0x80000006

#endif

//
// Trace Levels
//

#define AFS_TRACE_LEVEL_ERROR               0x00000001
#define AFS_TRACE_LEVEL_WARNING             0x00000002
#define AFS_TRACE_LEVEL_VERBOSE             0x00000003
#define AFS_TRACE_LEVEL_VERBOSE_2           0x00000004
#define AFS_TRACE_LEVEL_MAXIMUM             0x00000004

//
// Trace Subsystem Classes
//

#define AFS_SUBSYSTEM_IO_PROCESSING         0x00000001  // Includes IO subsystem
#define AFS_SUBSYSTEM_FILE_PROCESSING       0x00000002  // Includes Fcb and name processing
#define AFS_SUBSYSTEM_LOCK_PROCESSING       0x00000004  // All lock processing, level must be set to VERBOSE
#define AFS_SUBSYSTEM_EXTENT_PROCESSING     0x00000008  // Specific extent processing
#define AFS_SUBSYSTEM_WORKER_PROCESSING     0x00000010  // All worker processing
#define AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING 0x00000020  // Reference counting directory entries
#define AFS_SUBSYSTEM_OBJECT_REF_COUNTING   0x00000040  // Reference counting objects
#define AFS_SUBSYSTEM_VOLUME_REF_COUNTING   0x00000080  // Reference counting volumes
#define AFS_SUBSYSTEM_FCB_REF_COUNTING      0x00000100  // Reference counting fcbs
#define AFS_SUBSYSTEM_CLEANUP_PROCESSING    0x00000200  // Garbage collection of objects, dir entries, fcbs, etc.
#define AFS_SUBSYSTEM_PIPE_PROCESSING       0x00000400  // Pipe and share processing
#define AFS_SUBSYSTEM_DIR_NOTIF_PROCESSING  0x00000800  // Directory notification interface
#define AFS_SUBSYSTEM_NETWORK_PROVIDER      0x00001000  // Network provier interactions
#define AFS_SUBSYSTEM_DIR_NODE_COUNT        0x00002000  // Dir node count processing
#define AFS_SUBSYSTEM_PIOCTL_PROCESSING     0x00004000  // PIOCtl processing
#define AFS_SUBSYSTEM_AUTHGROUP_PROCESSING  0x00008000  // Auth group creation/assignment
#define AFS_SUBSYSTEM_LOAD_LIBRARY          0x00010000  // Library load and unload, request queuing
#define AFS_SUBSYSTEM_PROCESS_PROCESSING    0x00020000  // Process creation and destruction
#define AFS_SUBSYSTEM_EXTENT_ACTIVE_COUNTING 0x00040000 // Extent Active Counts
#define AFS_SUBSYSTEM_INIT_PROCESSING       0x00080000  // Redirector Initialization
#define AFS_SUBSYSTEM_NAME_ARRAY_PROCESSING 0x00100000  // Name Array Processing
#define AFS_SUBSYSTEM_NAME_ARRAY_REF_COUNTING 0x00200000  // Name Array DirectoryCB refcnts
#define AFS_SUBSYSTEM_FCB_ALLOCATION        0x01000000  // AFSFileCB Allocation
#define AFS_SUBSYSTEM_DIRENTRY_ALLOCATION   0x02000000  // AFSDirectoryCB Allocation
#define AFS_SUBSYSTEM_OBJINFO_ALLOCATION    0x04000000  // AFSObjectInformationCB Allocation

//
// Invalidation Reasons
//

#define AFS_INVALIDATE_EXPIRED          1  /* Set RE_VALIDATE */
#define AFS_INVALIDATE_FLUSHED          2  /* Set RE-VALIDATE */
#define AFS_INVALIDATE_CALLBACK         3  /* Set VERIFY Reset dir enumeration */
#define AFS_INVALIDATE_SMB              4  /* Set VERIFY Reset dir enumeration */
#define AFS_INVALIDATE_CREDS            5  /* Set VERIFY - User credentials changed */
#define AFS_INVALIDATE_DATA_VERSION     6  /* Set VERIFY */
#define AFS_INVALIDATE_DELETED          7  /* Requires top level locks */

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

#define AFS_EXTENT_FLAG_CLEAN   4   // The presence of this flag during a AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS
                                    // call from the file system indicates to the server that the file system
                                    // is going to completely overwrite the contents of the extent and the
                                    // service should therefore not bother to obtain the current version
                                    // from the file server.

#define AFS_EXTENT_FLAG_FLUSH   8   // The presence of this flag indicates that the service should perform
                                    // the equivalent of a FLUSH ioctl on the file after processing the
                                    // extents.

#define AFS_EXTENT_FLAG_IN_USE  0x10 // The extent is currenty in use by the fs and cannot be released

#define AFS_EXTENT_FLAG_UNKNOWN 0x20 // The extent is unknown to the fs

#define AFS_EXTENT_FLAG_MD5_SET 0x40 // The extent MD5 field has been set

//
// Volume Information Characteristics
//

#ifndef AFS_KERNEL_MODE

#define FILE_REMOVABLE_MEDIA            0x00000001
#define FILE_READ_ONLY_DEVICE           0x00000002
#define FILE_REMOTE_DEVICE              0x00000010

//
// File attributes
//

#define FILE_ATTRIBUTE_READONLY             0x00000001  // winnt
#define FILE_ATTRIBUTE_HIDDEN               0x00000002  // winnt
#define FILE_ATTRIBUTE_SYSTEM               0x00000004  // winnt

#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  // winnt
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020  // winnt
#define FILE_ATTRIBUTE_DEVICE               0x00000040  // winnt
#define FILE_ATTRIBUTE_NORMAL               0x00000080  // winnt

//
// Filesystem attributes
//

#define FILE_CASE_PRESERVED_NAMES       0x00000002  // winnt
#define FILE_UNICODE_ON_DISK            0x00000004  // winnt
#define FILE_PERSISTENT_ACLS            0x00000008  // winnt
#define FILE_VOLUME_QUOTAS              0x00000020  // winnt
#define FILE_SUPPORTS_REPARSE_POINTS    0x00000080  // winnt
#define FILE_SUPPORTS_OBJECT_IDS        0x00010000  // winnt
#define FILE_SUPPORTS_HARD_LINKS        0x00400000  // winnt

#endif

//
// AFS File Types
//

#define AFS_FILE_TYPE_UNKNOWN            0    /* an unknown object */
#define AFS_FILE_TYPE_FILE               1    /* a file */
#define AFS_FILE_TYPE_DIRECTORY          2    /* a dir */
#define AFS_FILE_TYPE_SYMLINK            3    /* a symbolic link */
#define AFS_FILE_TYPE_MOUNTPOINT         4    /* a mount point */
#define AFS_FILE_TYPE_DFSLINK            5    /* a Microsoft Dfs link */
#define AFS_FILE_TYPE_INVALID            99   /* an invalid link */

//
// AFS File types specific to Windows
//

#define AFS_FILE_TYPE_SPECIAL_SHARE_NAME    -1
#define AFS_FILE_TYPE_PIOCTL                -2
#define AFS_FILE_TYPE_PIPE                  -3

//
// AFS SysName Constants
//

#define AFS_MAX_SYSNAME_LENGTH 128
#define AFS_SYSNAME_ARCH_32BIT 0
#define AFS_SYSNAME_ARCH_64BIT 1

//
// Server file access granted to callers on open
//

#define AFS_FILE_ACCESS_NOLOCK          0x00000000
#define AFS_FILE_ACCESS_EXCLUSIVE       0x00000001
#define AFS_FILE_ACCESS_SHARED          0x00000002

//
// Reparse Point processing policy
//

#define AFS_REPARSE_POINT_POLICY_RESET       0x00000000 // This will reset the policy to default
							// behavior which is to process the
							// "open as reparse point" flag during
							// Create per normal operation.

#define AFS_REPARSE_POINT_TO_FILE_AS_FILE    0x00000001 // If indicated then ignore any attempt to
							// "open as reparse point" when the target is
							// a file.

#define AFS_REPARSE_POINT_VALID_POLICY_FLAGS 0x00000001

//
// Reparse Point policy scope
//

#define AFS_REPARSE_POINT_POLICY_GLOBAL      0x00000000

#define AFS_REPARSE_POINT_POLICY_AUTHGROUP   0x00000001

#endif /* _AFS_USER_DEFINE_H */
