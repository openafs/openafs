/*
 * Copyright (c) 2008 Secure Endpoints, Inc.
 * Copyright (c) 2009-2011 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Secure Endpoints Inc. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software without
 *   specific prior written permission from Secure Endpoints, Inc. and
 *   Your File System, Inc.
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

extern void RDR_InitPipe(void);

extern DWORD RDR_SetupPipe( ULONG index, cm_fid_t *parentFid, cm_fid_t *rootFid,
                            WCHAR *Name, DWORD NameLength,
                            cm_user_t *userp);

extern void RDR_CleanupPipe(ULONG index);

extern DWORD RDR_Pipe_Read(ULONG index, ULONG BufferLength, void *MappedBuffer,
                           ULONG *pBytesProcessed, cm_req_t *reqp, cm_user_t *userp);

extern DWORD RDR_Pipe_Write(ULONG index, ULONG BufferLength, void *MappedBuffer,
                            cm_req_t *reqp, cm_user_t *userp);

extern DWORD RDR_Pipe_QueryInfo( ULONG index, ULONG InfoClass,
                                 ULONG BufferLength, void *MappedBuffer,
                                 ULONG *pBytesProcessed, cm_req_t *reqp, cm_user_t *userp);

extern DWORD RDR_Pipe_SetInfo( ULONG index, ULONG InfoClass,
                               ULONG BufferLength, void *MappedBuffer,
                               cm_req_t *reqp, cm_user_t *userp);

#ifdef RDR_PIPE_PRIVATE
typedef struct RDR_pipe {
    struct RDR_pipe *next, *prev;
    ULONG             index;
    wchar_t           name[MAX_PATH];
    cm_fid_t          parentFid;
    cm_fid_t          rootFid;
    cm_scache_t      *parentScp;
    afs_uint32        flags;
    afs_uint32        devstate;
    msrpc_conn        rpc_conn;

    /* input side */
    char *inDatap;                      /* current position
                                         * in input parameter block */
    char *inAllocp;                     /* allocated input parameter block */
    afs_uint32 inCopied;                /* # of input bytes copied in so far
                                         * by write calls */
    /* output side */
    char *outDatap;                     /* output results assembled so far */
    char *outAllocp;                    /* output results assembled so far */
    afs_uint32 outCopied;               /* # of output bytes copied back so far */
} RDR_pipe_t;

/* flags for smb_ioctl_t */
#define RDR_PIPEFLAG_DATAIN     1       /* reading data from client to server */
#define RDR_PIPEFLAG_LOGON      2       /* got tokens from integrated logon */
#define RDR_PIPEFLAG_USEUTF8    4       /* this request is using UTF-8 strings */
#define RDR_PIPEFLAG_DATAOUT    8       /* sending data from server to client */

#define RDR_PIPEFLAG_RPC                0x0010
#define RDR_PIPEFLAG_MESSAGE_MODE       0x0020
#define RDR_PIPEFLAG_BLOCKING           0x0040
#define RDR_PIPEFLAG_INCALL             0x0080

/* Device state constants */
#define RDR_DEVICESTATE_READASBYTESTREAM    0x0000
#define RDR_DEVICESTATE_READMSGFROMPIPE     0x0100
#define RDR_DEVICESTATE_BYTESTREAMPIPE      0x0000
#define RDR_DEVICESTATE_MESSAGEMODEPIPE     0x0400
#define RDR_DEVICESTATE_PIPECLIENTEND       0x0000
#define RDR_DEVICESTATE_PIPESERVEREND       0x4000
#define RDR_DEVICESTATE_BLOCKING            0x8000

#define RDR_PIPE_MAXDATA        65536

/* procedure implementing an pipe */
typedef long (RDR_pipeProc_t)(RDR_pipe_t *, struct cm_user *userp);

extern RDR_pipe_t *RDR_FindPipe(ULONG index, int locked);

/*
 * DDK Data Structures
 *
 * This is a userland module and does not include DDK headers.
 * Replicate the DDK Data Structures required for pipe handling
 * based on [MS-FSC]: File System Control Codes
 *   http://msdn.microsoft.com/en-us/library/cc231987%28v=PROT.13%29.aspx
 */
typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation         = 1,
    FileFullDirectoryInformation,   // 2
    FileBothDirectoryInformation,   // 3
    FileBasicInformation,           // 4
    FileStandardInformation,        // 5
    FileInternalInformation,        // 6
    FileEaInformation,              // 7
    FileAccessInformation,          // 8
    FileNameInformation,            // 9
    FileRenameInformation,          // 10
    FileLinkInformation,            // 11
    FileNamesInformation,           // 12
    FileDispositionInformation,     // 13
    FilePositionInformation,        // 14
    FileFullEaInformation,          // 15
    FileModeInformation,            // 16
    FileAlignmentInformation,       // 17
    FileAllInformation,             // 18
    FileAllocationInformation,      // 19
    FileEndOfFileInformation,       // 20
    FileAlternateNameInformation,   // 21
    FileStreamInformation,          // 22
    FilePipeInformation,            // 23
    FilePipeLocalInformation,       // 24
    FilePipeRemoteInformation,      // 25
    FileMailslotQueryInformation,   // 26
    FileMailslotSetInformation,     // 27
    FileCompressionInformation,     // 28
    FileObjectIdInformation,        // 29
    FileCompletionInformation,      // 30
    FileMoveClusterInformation,     // 31
    FileQuotaInformation,           // 32
    FileReparsePointInformation,    // 33
    FileNetworkOpenInformation,     // 34
    FileAttributeTagInformation,    // 35
    FileTrackingInformation,        // 36
    FileIdBothDirectoryInformation, // 37
    FileIdFullDirectoryInformation, // 38
    FileValidDataLengthInformation, // 39
    FileShortNameInformation,       // 40
    FileIoCompletionNotificationInformation, // 41
    FileIoStatusBlockRangeInformation,       // 42
    FileIoPriorityHintInformation,           // 43
    FileSfioReserveInformation,              // 44
    FileSfioVolumeInformation,               // 45
    FileHardLinkInformation,                 // 46
    FileProcessIdsUsingFileInformation,      // 47
    FileNormalizedNameInformation,           // 48
    FileNetworkPhysicalNameInformation,      // 49
    FileIdGlobalTxDirectoryInformation,      // 50
    FileIsRemoteDeviceInformation,           // 51
    FileAttributeCacheInformation,           // 52
    FileNumaNodeInformation,                 // 53
    FileStandardLinkInformation,             // 54
    FileRemoteProtocolInformation,           // 55
    FileMaximumInformation
} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef struct _FILE_BASIC_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef struct _FILE_STANDARD_INFORMATION {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG NumberOfLinks;
    BOOLEAN DeletePending;
    BOOLEAN IsDirectory;
} FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;

typedef struct _FILE_NAME_INFORMATION {
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

typedef struct _FILE_PIPE_INFORMATION {
     ULONG ReadMode;
     ULONG CompletionMode;
} FILE_PIPE_INFORMATION, *PFILE_PIPE_INFORMATION;

typedef struct _FILE_PIPE_LOCAL_INFORMATION {
     ULONG NamedPipeType;
     ULONG NamedPipeConfiguration;
     ULONG MaximumInstances;
     ULONG CurrentInstances;
     ULONG InboundQuota;
     ULONG ReadDataAvailable;
     ULONG OutboundQuota;
     ULONG WriteQuotaAvailable;
     ULONG NamedPipeState;
     ULONG NamedPipeEnd;
} FILE_PIPE_LOCAL_INFORMATION, *PFILE_PIPE_LOCAL_INFORMATION;

typedef struct _FILE_PIPE_REMOTE_INFORMATION {
     LARGE_INTEGER CollectDataTime;
     ULONG MaximumCollectionCount;
} FILE_PIPE_REMOTE_INFORMATION, *PFILE_PIPE_REMOTE_INFORMATION;

#define FILE_PIPE_BYTE_STREAM_TYPE          0x00000000
#define FILE_PIPE_MESSAGE_TYPE              0x00000001

#define FILE_PIPE_ACCEPT_REMOTE_CLIENTS     0x00000000
#define FILE_PIPE_REJECT_REMOTE_CLIENTS     0x00000002
#define FILE_PIPE_TYPE_VALID_MASK           0x00000003

#define FILE_PIPE_QUEUE_OPERATION           0x00000000
#define FILE_PIPE_COMPLETE_OPERATION        0x00000001

#define FILE_PIPE_BYTE_STREAM_MODE          0x00000000
#define FILE_PIPE_MESSAGE_MODE              0x00000001

#define FILE_PIPE_INBOUND                   0x00000000
#define FILE_PIPE_OUTBOUND                  0x00000001
#define FILE_PIPE_FULL_DUPLEX               0x00000002

#define FILE_PIPE_DISCONNECTED_STATE        0x00000001
#define FILE_PIPE_LISTENING_STATE           0x00000002
#define FILE_PIPE_CONNECTED_STATE           0x00000003
#define FILE_PIPE_CLOSING_STATE             0x00000004

#define FILE_PIPE_CLIENT_END                0x00000000
#define FILE_PIPE_SERVER_END                0x00000001

#define FILE_PIPE_READ_DATA                 0x00000000
#define FILE_PIPE_WRITE_SPACE               0x00000001
#endif

