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

#ifdef __cplusplus
extern "C" {
#endif

#include <ntsecapi.h>

// The following are forward declarations of structures
// which are referenced in the RDR code only by pointer.
typedef struct cm_user cm_user_t;
typedef struct cm_req cm_req_t;
typedef struct cm_fid cm_fid_t;
typedef struct cm_scache cm_scache_t;

// Function Declarations
#include <../common/AFSUserPrototypes.h>

void
RDR_InitReq( IN OUT cm_req_t *reqp, BOOL bWow64);

DWORD
RDR_SetInitParams( OUT AFSRedirectorInitInfo **ppRedirInitInfo,
                   OUT DWORD * pRedirInitInfoLen );

DWORD
WINAPI
RDR_RequestWorkerThread( LPVOID lpParameter);

DWORD
RDR_ProcessWorkerThreads( IN DWORD);

void
RDR_ProcessRequest( AFSCommRequest *RequestBuffer);

void
RDR_EnumerateDirectory( IN cm_user_t *userp,
                        IN AFSFileID ParentID,
                        IN AFSDirQueryCB *QueryCB,
                        IN BOOL bWow64,
                        IN BOOL bQueryStatus,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);

void
RDR_EvaluateNodeByName( IN cm_user_t *userp,
                        IN AFSFileID ParentID,
                        IN WCHAR     *Name,
                        IN DWORD     NameLength,
                        IN BOOL      CaseSensitive,
                        IN BOOL      bWow64,
                        IN BOOL      bQueryStatus,
                        IN BOOL      bHoldFid,
                        IN DWORD     ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);

void
RDR_EvaluateNodeByID( IN cm_user_t *userp,
                      IN AFSFileID ParentID,
                      IN AFSFileID SourceID,
                      IN BOOL bWow64,
                      IN BOOL bQueryStatus,
                      IN BOOL      bHoldFid,
                      IN DWORD    ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB);

void
RDR_CreateFileEntry( IN cm_user_t *userp,
                     IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN AFSFileCreateCB *CreateCB,
                     IN BOOL bWow64,
                     IN BOOL bHoldFid,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_UpdateFileEntry( IN cm_user_t *userp,
                     IN AFSFileID FileId,
                     IN AFSFileUpdateCB *UpdateCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_DeleteFileEntry( IN cm_user_t *userp,
                     IN AFSFileID ParentId,
                     IN ULONGLONG ProcessId,
                     IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN BOOL bWow64,
                     IN BOOL bCheckOnly,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_RenameFileEntry( IN cm_user_t *userp,
                     IN WCHAR    *SourceFileName,
                     IN DWORD     SourceFileNameLength,
                     IN AFSFileID SourceFileId,
                     IN AFSFileRenameCB *RenameCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_HardLinkFileEntry( IN cm_user_t *userp,
                       IN WCHAR    *SourceFileName,
                       IN DWORD     SourceFileNameLength,
                       IN AFSFileID SourceFileId,
                       IN AFSFileHardLinkCB *HardLinkCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB);

void
RDR_FlushFileEntry( IN cm_user_t *userp,
                    IN AFSFileID FileId,
                    IN BOOL bWow64,
                    IN DWORD ResultBufferLength,
                    IN OUT AFSCommResult **ResultCB);

void
RDR_OpenFileEntry( IN cm_user_t *userp,
                   IN AFSFileID FileId,
                   IN AFSFileOpenCB *OpenCB,
                   IN BOOL bWow64,
                   IN BOOL bHoldFid,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB);

void
RDR_ReleaseFileAccess( IN cm_user_t *userp,
                       IN AFSFileID FileId,
                       IN AFSFileAccessReleaseCB *ReleaseFileCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB);

void
RDR_CleanupFileEntry( IN cm_user_t *userp,
                      IN AFSFileID FileId,
                      IN WCHAR *FileName,
                      IN DWORD FileNameLength,
                      IN AFSFileCleanupCB *CleanupCB,
                      IN BOOL bWow64,
                      IN BOOL bFlushFile,
                      IN BOOL bDeleteFile,
                      IN BOOL bUnlockFile,
                      IN DWORD ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB);

BOOL
RDR_RequestFileExtentsAsync( IN cm_user_t *userp,
                             IN AFSFileID FileId,
                             IN AFSRequestExtentsCB *RequestExtentsCB,
                             IN BOOL bWow64,
                             IN OUT DWORD * ResultBufferLength,
                             IN OUT AFSSetFileExtentsCB **ResultCB);

void
RDR_ReleaseFileExtents( IN cm_user_t *userp,
                        IN AFSFileID FileId,
                        IN AFSReleaseExtentsCB *ReleaseExtentsCB,
                        IN BOOL bWow64,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);

DWORD
RDR_RequestExtentRelease( IN cm_fid_t *fidp,
                          IN LARGE_INTEGER numOfHeldExtents,
                          IN DWORD numOfExtents,
                          IN AFSFileExtentCB *extentList);

DWORD
RDR_ProcessReleaseFileExtentsResult( IN AFSReleaseFileExtentsResultCB *ReleaseFileExtentsResultCB,
                                     IN DWORD ResultBufferLength);

DWORD
RDR_ReleaseFailedSetFileExtents( IN cm_user_t *userp,
                                 IN AFSSetFileExtentsCB *SetFileExtentsResultCB,
                                 IN DWORD ResultBufferLength);

DWORD
RDR_SetFileExtents( IN AFSSetFileExtentsCB *pSetFileExtentsResultCB,
                    IN DWORD dwResultBufferLength);
void
RDR_PioctlOpen( IN cm_user_t *userp,
                IN AFSFileID  ParentId,
                IN AFSPIOCtlOpenCloseRequestCB *pPioctlCB,
                IN BOOL bWow64,
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB);

void
RDR_PioctlClose( IN cm_user_t *userp,
                 IN AFSFileID  ParentId,
                 IN AFSPIOCtlOpenCloseRequestCB *pPioctlCB,
                 IN BOOL bWow64,
                 IN DWORD ResultBufferLength,
                 IN OUT AFSCommResult **ResultCB);

void
RDR_PioctlWrite( IN cm_user_t *userp,
                 IN AFSFileID  ParentId,
                 IN AFSPIOCtlIORequestCB *pPioctlCB,
                 IN BOOL bWow64,
                 IN DWORD ResultBufferLength,
                 IN OUT AFSCommResult **ResultCB);

void
RDR_PioctlRead( IN cm_user_t *userp,
                IN AFSFileID  ParentId,
                IN AFSPIOCtlIORequestCB *pPioctlCB,
                IN BOOL bWow64,
                IN BOOL bIsLocalSystem,
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB);

void
RDR_ByteRangeLockSync( IN cm_user_t     *userp,
                       IN AFSFileID     FileId,
                       IN AFSByteRangeLockRequestCB *pBRLRequestCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB);

void
RDR_ByteRangeUnlock( IN cm_user_t     *userp,
                     IN AFSFileID     FileId,
                     IN AFSByteRangeUnlockRequestCB *pBRURequestCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_ByteRangeUnlockAll( IN cm_user_t     *userp,
                        IN AFSFileID     FileId,
                        IN AFSByteRangeUnlockRequestCB *pBRURequestCB,
                        IN BOOL bWow64,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);

void
RDR_GetVolumeInfo( IN cm_user_t     *userp,
                   IN AFSFileID     FileId,
                   IN BOOL bWow64,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB);

void
RDR_GetVolumeSizeInfo( IN cm_user_t     *userp,
                       IN AFSFileID     FileId,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB);

void
RDR_HoldFid( IN cm_user_t     *userp,
             IN AFSHoldFidRequestCB * pHoldFidCB,
             IN BOOL bFast,
             IN DWORD ResultBufferLength,
             IN OUT AFSCommResult **ResultCB);

void
RDR_ReleaseFid( IN cm_user_t     *userp,
                IN AFSReleaseFidRequestCB * pReleaseFidCB,
                IN BOOL bFast,
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB);

void
RDR_InitPipe(void);

void
RDR_ShutdownPipe(void);

void
RDR_PipeOpen( IN cm_user_t *userp,
              IN AFSFileID  ParentId,
              IN WCHAR     *Name,
              IN DWORD      NameLength,
              IN AFSPipeOpenCloseRequestCB *pPipeCB,
              IN BOOL bWow64,
              IN DWORD ResultBufferLength,
              IN OUT AFSCommResult **ResultCB);

void
RDR_PipeClose( IN cm_user_t *userp,
               IN AFSFileID  ParentId,
               IN AFSPipeOpenCloseRequestCB *pPipeCB,
               IN BOOL bWow64,
               IN DWORD ResultBufferLength,
               IN OUT AFSCommResult **ResultCB);

void
RDR_PipeWrite( IN cm_user_t *userp,
               IN AFSFileID  ParentId,
               IN AFSPipeIORequestCB *pPipeCB,
               IN BYTE *pPipeData,
               IN BOOL bWow64,
               IN DWORD ResultBufferLength,
               IN OUT AFSCommResult **ResultCB);

void
RDR_PipeRead( IN cm_user_t *userp,
              IN AFSFileID  ParentId,
              IN AFSPipeIORequestCB *pPipeCB,
              IN BOOL bWow64,
              IN DWORD ResultBufferLength,
              IN OUT AFSCommResult **ResultCB);

void
RDR_PipeSetInfo( IN cm_user_t *userp,
                 IN AFSFileID  ParentId,
                 IN AFSPipeInfoRequestCB *pPipeCB,
                 IN BYTE *pPipeData,
                 IN BOOL bWow64,
                 IN DWORD ResultBufferLength,
                 IN OUT AFSCommResult **ResultCB);

void
RDR_PipeQueryInfo( IN cm_user_t *userp,
                   IN AFSFileID  ParentId,
                   IN AFSPipeInfoRequestCB *pPipeCB,
                   IN BOOL bWow64,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB);

void
RDR_PipeTransceive( IN cm_user_t     *userp,
                    IN AFSFileID  ParentId,
                    IN AFSPipeIORequestCB *pPipeCB,
                    IN BYTE *pPipeData,
                    IN BOOL bWow64,
                    IN DWORD          ResultBufferLength,
                    IN OUT AFSCommResult **ResultCB);

cm_user_t *
RDR_UserFromCommRequest( IN AFSCommRequest * pRequest);

cm_user_t *
RDR_UserFromAuthGroup( IN GUID *pGuid);

void
RDR_ReleaseUser( IN cm_user_t *userp);

void
RDR_fid2FID( IN cm_fid_t *fid,
             IN AFSFileID *FileId);

void
RDR_FID2fid( IN AFSFileID *FileId,
             IN cm_fid_t *fid);

void
RDR_InitIoctl(void);

void
RDR_ShutdownIoctl(void);

#ifdef __cplusplus
}
#endif

