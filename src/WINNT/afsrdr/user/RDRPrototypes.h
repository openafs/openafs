#ifdef __cplusplus
extern "C" {
#endif

// The following are forward declarations of structures
// which are referenced in the RDR code only by pointer.
typedef struct cm_user cm_user_t;
typedef struct cm_req cm_req_t;

// Function Declarations
DWORD
RDR_Initialize( void);

DWORD
RDR_ShutdownNotify( void);

DWORD
RDR_ShutdownFinal( void);

DWORD 
RDR_NetworkStatus( IN BOOLEAN status);

DWORD 
RDR_VolumeStatus( IN ULONG cellID, IN ULONG volID, IN BOOLEAN online);

DWORD 
RDR_NetworkAddrChange(void);

DWORD 
RDR_InvalidateVolume( IN ULONG cellID, IN ULONG volID, IN ULONG reason);

DWORD 
RDR_InvalidateObject( IN ULONG cellID, IN ULONG volID, IN ULONG vnode, 
                      IN ULONG uniq, IN ULONG hash, IN ULONG filetype, IN ULONG reason);

void
RDR_InitReq( IN OUT cm_req_t *reqp );

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
                     IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN BOOL bWow64,
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
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB);

void
RDR_RequestFileExtentsSync( IN cm_user_t *userp,
                            IN AFSFileID FileId,
                            IN AFSRequestExtentsCB *RequestExtentsCB,
                            IN BOOL bWow64,
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
RDR_RequestExtentRelease(DWORD numOfExtents, LARGE_INTEGER numOfHeldExtents);

DWORD
RDR_ProcessReleaseFileExtentsResult( IN AFSReleaseFileExtentsResultCB *ReleaseFileExtentsResultCB,
                                     IN DWORD ResultBufferLength);

DWORD
RDR_ReleaseFailedSetFileExtents( IN cm_user_t *userp,
                                 IN AFSSetFileExtentsCB *SetFileExtentsResultCB,
                                 IN DWORD ResultBufferLength);

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
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB);

void
RDR_ByteRangeLockSync( IN cm_user_t     *userp,
                       IN ULARGE_INTEGER ProcessId,
                       IN AFSFileID     FileId,
                       IN AFSByteRangeLockRequestCB *pBRLRequestCB,
                       IN BOOL bWow64,
                       IN DWORD ResultBufferLength,
                       IN OUT AFSCommResult **ResultCB);

void
RDR_ByteRangeLockAsync( IN cm_user_t     *userp,
                        IN ULARGE_INTEGER ProcessId,
                        IN AFSFileID     FileId,
                        IN AFSAsyncByteRangeLockRequestCB *pABRLRequestCB,
                        IN BOOL bWow64,
                        OUT DWORD *ResultBufferLength,
                        IN OUT AFSSetByteRangeLockResultCB **ResultCB);

void
RDR_ByteRangeUnlock( IN cm_user_t     *userp,
                     IN ULARGE_INTEGER ProcessId,
                     IN AFSFileID     FileId,
                     IN AFSByteRangeUnlockRequestCB *pBRURequestCB,
                     IN BOOL bWow64,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_ByteRangeUnlockAll( IN cm_user_t     *userp,
                        IN ULARGE_INTEGER ProcessId,
                        IN AFSFileID     FileId,
                        IN BOOL bWow64,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);

void
RDR_GetVolumeInfo( IN cm_user_t     *userp,
                   IN ULARGE_INTEGER ProcessId,
                   IN AFSFileID     FileId,
                   IN BOOL bWow64,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB);

void
RDR_HoldFid( IN cm_user_t     *userp,
             IN ULARGE_INTEGER ProcessId,
             IN AFSHoldFidRequestCB * pHoldFidCB,
             IN BOOL bFast,
             IN DWORD ResultBufferLength,
             IN OUT AFSCommResult **ResultCB);

void
RDR_ReleaseFid( IN cm_user_t     *userp,
                IN ULARGE_INTEGER ProcessId,
                IN AFSReleaseFidRequestCB * pReleaseFidCB,
                IN BOOL bFast,
                IN DWORD ResultBufferLength,
                IN OUT AFSCommResult **ResultCB);

extern DWORD 
RDR_SysName(ULONG Architecture, ULONG Count, WCHAR **NameList);

cm_user_t *
RDR_UserFromCommRequest( IN AFSCommRequest * pRequest );

cm_user_t *
RDR_UserFromProcessId( IN ULARGE_INTEGER ProcessId);

void
RDR_ReleaseUser( IN cm_user_t *userp);

void 
RDR_InitIoctl(void);

#ifdef __cplusplus
}
#endif

