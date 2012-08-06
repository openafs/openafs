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

#ifndef _AFS_DEFINES_H
#define _AFS_DEFINES_H
//
// File: AFSDefines.h
//

//
// Registry names
//

#define AFS_REG_DEBUG_FLAGS              L"DebugFlags"
#define AFS_REG_TRACE_LEVEL              L"TraceLevel"
#define AFS_REG_TRACE_SUBSYSTEM          L"TraceSubsystem"
#define AFS_REG_TRACE_BUFFER_LENGTH      L"TraceBufferSize"  // in KB
#define AFS_REG_MAX_DIRTY                L"MaxDirtyMb"
#define AFS_REG_MAX_IO                   L"MaxIOMb"
#define AFS_NETBIOS_NAME                 L"NetbiosName"
#define AFS_REG_SHUTDOWN_STATUS          L"ShutdownStatus"
#define AFS_REG_REQUIRE_CLEAN_SHUTDOWN   L"RequireCleanShutdown"

//
// Control Device name
//

#define AFS_CONTROL_DEVICE_NAME     L"\\Device\\AFSControlDevice"
#define AFS_SYMLINK_NAME            L"\\??\\AFSRedirector"

//
// How big to make the runs
//
#define AFS_MAX_STACK_IO_RUNS              5

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif

#define QuadAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 7) & 0xfffffff8) \
    )

#define CRC32_POLYNOMIAL     0xEDB88320L;

//
// Define one second in terms of 100 nS units
//

#define AFS_ONE_SECOND          10000000

#define AFS_SERVER_FLUSH_DELAY  30
#define AFS_SERVER_PURGE_DELAY  60
//
// PURGE_SLEEP is the number of PURGE_DELAYS we wait before we will unilaterally
// give back extents.
//
// If the Service asks us, we will start at PURGE_SLEEP of delays and then work back
//
#define AFS_SERVER_PURGE_SLEEP  6

//
// Read ahead granularity
//

#define READ_AHEAD_GRANULARITY      0x10000     // 64KB

#define AFS_DIR_ENUM_BUFFER_LEN   (16 * 1024)

//
// IS_BYTE_OFFSET_WRITE_TO_EOF
// liOffset - should be from Irp.StackLocation.Parameters.Write.ByteOffset
// this macro checks to see if the Offset Large_Integer points to the
// special constant value which denotes to start the write at EndOfFile
//
#define IS_BYTE_OFFSET_WRITE_TO_EOF(liOffset) \
    (((liOffset).LowPart == FILE_WRITE_TO_END_OF_FILE) \
      && ((liOffset).HighPart == 0xFFFFFFFF))

//
// Ccb Directory enum flags
//

#define CCB_FLAG_DIR_OF_DIRS_ONLY           0x00000001
#define CCB_FLAG_FULL_DIRECTORY_QUERY       0x00000002
#define CCB_FLAG_MASK_CONTAINS_WILD_CARDS   0x00000004
#define CCB_FLAG_FREE_FULL_PATHNAME         0x00000008
#define CCB_FLAG_RETURN_RELATIVE_ENTRIES    0x00000010
#define CCB_FLAGS_DIRECTORY_QUERY_MAPPED    0x00000020

//
// DirEntry flags
//

#define AFS_DIR_RELEASE_NAME_BUFFER             0x00000001
#define AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD 0x00000004
#define AFS_DIR_ENTRY_NOT_IN_PARENT_TREE        0x00000008
// Use this entry
#define AFS_DIR_ENTRY_FAKE                      0x00000020
#define AFS_DIR_RELEASE_TARGET_NAME_BUFFER      0x00000040
#define AFS_DIR_ENTRY_VALID                     0x00000080
#define AFS_DIR_ENTRY_PENDING_DELETE            0x00000100
#define AFS_DIR_ENTRY_DELETED                   0x00000200
#define AFS_DIR_ENTRY_SERVER_SERVICE            0x00000400
#define AFS_DIR_ENTRY_WORKSTATION_SERVICE       0x00000800
#define AFS_DIR_ENTRY_IPC                       0x00001000

//
// Network provider errors
//

#define WN_SUCCESS                              0L
#define WN_ALREADY_CONNECTED                    85L
#define WN_OUT_OF_MEMORY                        8L
#define WN_NOT_CONNECTED                        2250L
#define WN_BAD_NETNAME                          67L

#define RESOURCE_CONNECTED      0x00000001
#define RESOURCE_GLOBALNET      0x00000002
#define RESOURCE_REMEMBERED     0x00000003
#define RESOURCE_RECENT         0x00000004
#define RESOURCE_CONTEXT        0x00000005

#define RESOURCETYPE_ANY        0x00000000
#define RESOURCETYPE_DISK       0x00000001
#define RESOURCETYPE_PRINT      0x00000002
#define RESOURCETYPE_RESERVED   0x00000008
#define RESOURCETYPE_UNKNOWN    0xFFFFFFFF

#define RESOURCEUSAGE_CONNECTABLE   0x00000001
#define RESOURCEUSAGE_CONTAINER     0x00000002
#define RESOURCEUSAGE_NOLOCALDEVICE 0x00000004
#define RESOURCEUSAGE_SIBLING       0x00000008
#define RESOURCEUSAGE_ATTACHED      0x00000010
#define RESOURCEUSAGE_ALL           (RESOURCEUSAGE_CONNECTABLE | RESOURCEUSAGE_CONTAINER | RESOURCEUSAGE_ATTACHED)
#define RESOURCEUSAGE_RESERVED      0x80000000

#define RESOURCEDISPLAYTYPE_GENERIC        0x00000000
#define RESOURCEDISPLAYTYPE_DOMAIN         0x00000001
#define RESOURCEDISPLAYTYPE_SERVER         0x00000002
#define RESOURCEDISPLAYTYPE_SHARE          0x00000003
#define RESOURCEDISPLAYTYPE_FILE           0x00000004
#define RESOURCEDISPLAYTYPE_GROUP          0x00000005
#define RESOURCEDISPLAYTYPE_NETWORK        0x00000006
#define RESOURCEDISPLAYTYPE_ROOT           0x00000007
#define RESOURCEDISPLAYTYPE_SHAREADMIN     0x00000008
#define RESOURCEDISPLAYTYPE_DIRECTORY      0x00000009
#define RESOURCEDISPLAYTYPE_TREE           0x0000000A
#define RESOURCEDISPLAYTYPE_NDSCONTAINER   0x0000000B

//
// Method for determining the different control device open requests
//

#define AFS_CONTROL_INSTANCE            0x00000001
#define AFS_REDIRECTOR_INSTANCE         0x00000002

//
// Extent flags
//

#define AFS_EXTENT_DIRTY                0x00000001

//
// Extent skip list sizes
//
#define AFS_NUM_EXTENT_LISTS    3

//
// Extents skip lists
//
// We use constant sizes.
//
#define AFS_EXTENT_SIZE         (4*1024)
#define AFS_EXTENTS_LIST        0
//
// A max of 64 extents in ther first skip list
#define AFS_EXTENT_SKIP1_BITS   6

//
// Then 128 bits in the second skip list
#define AFS_EXTENT_SKIP2_BITS   7

//
// This means that the top list skips in steps of 2^25 (=12+6+7) which
// is 32 Mb.  It is to be expected that files which are massively
// larger that this will not be fully mapped.
//
#define AFS_EXTENT_SKIP1_SIZE (AFS_EXTENT_SIZE << AFS_EXTENT_SKIP1_BITS)
#define AFS_EXTENT_SKIP2_SIZE (AFS_EXTENT_SKIP1_SIZE << AFS_EXTENT_SKIP2_BITS)

#define AFS_EXTENTS_MASKS { (AFS_EXTENT_SIZE-1),       \
                            (AFS_EXTENT_SKIP1_SIZE-1), \
                            (AFS_EXTENT_SKIP2_SIZE-1) }

//
// Maximum count to release at a time
//

#define AFS_MAXIMUM_EXTENT_RELEASE_COUNT        100

// {41966169-3FD7-4392-AFE4-E6A9D0A92C72}  - generated using guidgen.exe
DEFINE_GUID (GUID_SD_AFS_REDIRECTOR_CONTROL_OBJECT,
        0x41966169, 0x3fd7, 0x4392, 0xaf, 0xe4, 0xe6, 0xa9, 0xd0, 0xa9, 0x2c, 0x72);

//
// Debug log length
//

#define AFS_DBG_LOG_LENGTH              256

//
// Debug log flags
//

#define AFS_DBG_LOG_WRAPPED             0x00000001

//
// Connection flags
//

#define AFS_CONNECTION_FLAG_GLOBAL_SHARE        0x00000001

//
// Process CB flags
//

#define AFS_PROCESS_FLAG_IS_64BIT           0x00000001
#define AFS_PROCESS_FLAG_ACE_SET            0x00000002
#define AFS_PROCESS_LOCAL_SYSTEM_AUTH       0x00000004

//
// Auth group flags
//

#define AFS_AUTHGROUP_ACTIVE_SESSION        0x00000001

//
// Maximum number of special share names
//

#define AFS_SPECIAL_SHARE_NAME_COUNT_MAX    10

//
// Reparse tag information
//

//
//  Tag allocated to OpenAFS for DFS by Microsoft
//  GUID: EF21A155-5C92-4470-AB3B-370403D96369
//

#ifndef IO_REPARSE_TAG_OPENAFS_DFS
#define IO_REPARSE_TAG_OPENAFS_DFS              0x00000037L
#endif

//  {EF21A155-5C92-4470-AB3B-370403D96369}
DEFINE_GUID (GUID_AFS_REPARSE_GUID,
        0xEF21A155, 0x5C92, 0x4470, 0xAB, 0x3B, 0x37, 0x04, 0x03, 0xD9, 0x63, 0x69);

//
// Enumeration constants
//

#define AFS_DIR_ENTRY_INITIAL_DIR_INDEX   (ULONG)-3
#define AFS_DIR_ENTRY_INITIAL_ROOT_INDEX  (ULONG)-1

#define AFS_DIR_ENTRY_DOT_INDEX           (ULONG)-2
#define AFS_DIR_ENTRY_DOT_DOT_INDEX       (ULONG)-1

//
// Library flags
//

#define AFS_LIBRARY_LOADED                  0x00000001
#define AFS_LIBRARY_QUEUE_CANCELLED         0x00000002

//
// Custom ACE Information
//

// {7E5D0E2F-7500-45df-857A-C0C8A2CC6BE8}
static const GUID AFSSidGuid = { 0x7f5d0e2f, 0x7500, 0x45df, { 0x85, 0x7a, 0xc0, 0xc8, 0xa2, 0xcc, 0x6b, 0xe8 } };

// size of our special DACL SID...  S-1-8-GuidDword1-GuidDword2-GuidDword3-GuidDword4-AuthGroupGUIDDword1-AuthGroupGUIDDword2-AuthGroupGUIDDword3-AuthGroupGUIDDword4
// Revision (1) + SubAuthorityCount (1) + IdentifierAuthority (6) + GUID (16) + AuthGruopGUID( 16)
#define AFS_DACL_SID_LENGTH 40

typedef
NTSTATUS
(*PAFSSetInformationToken) (
    __in HANDLE TokenHandle,
    __in TOKEN_INFORMATION_CLASS TokenInformationClass,
    __in_bcount(TokenInformationLength) PVOID TokenInformation,
    __in ULONG TokenInformationLength
    );

#endif /* _AFS_DEFINES_H */
