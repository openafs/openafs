#ifdef __cplusplus
extern "C" {
#endif

// The following are forward declarations of structures
// which are referenced in the RDR code only by pointer.
typedef struct cm_user cm_user_t;

// Function Declarations
DWORD
RDR_Initialize( void);

DWORD
RDR_Shutdown( void);

DWORD
RDR_SetInitParams( OUT AFSCacheFileInfo **ppCacheFileInfo, 
                   OUT DWORD * pCacheFileInfoLen );

DWORD 
WINAPI 
RDR_RequestWorkerThread( LPVOID lpParameter);

DWORD
RDR_ProcessWorkerThreads(void);

void
RDR_ProcessRequest( AFSCommRequest *RequestBuffer);

void
RDR_EnumerateDirectory( IN cm_user_t *userp,
                        IN AFSFileID ParentID,
                        IN AFSDirQueryCB *QueryCB,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);
    
void
RDR_EvaluateNodeByName( IN cm_user_t *userp,
                        IN     AFSFileID ParentID,
                        IN     WCHAR   *Name,
                        IN     DWORD    NameLength,
                        IN     DWORD    CaseSensitive,
                        IN     DWORD    ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);
    
void
RDR_EvaluateNodeByID( IN cm_user_t *userp,
                      IN     AFSFileID ParentID,
                      IN     AFSFileID SourceID,
                      IN     DWORD    ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB);
    
void
RDR_CreateFileEntry( IN cm_user_t *userp,
                     IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN AFSFileCreateCB *CreateCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_UpdateFileEntry( IN cm_user_t *userp,
                     IN AFSFileID FileId,
                     IN AFSFileUpdateCB *UpdateCB,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_DeleteFileEntry( IN cm_user_t *userp,
                     IN AFSFileID ParentId,
                     IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_RenameFileEntry( IN cm_user_t *userp,
                     IN AFSFileID FileId,
                     IN AFSFileRenameCB *RenameCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_FlushFileEntry( IN cm_user_t *userp,
                    IN AFSFileID FileId,
                    IN DWORD ResultBufferLength,
                    IN OUT AFSCommResult **ResultCB);

void
RDR_OpenFileEntry( IN cm_user_t *userp,
                   IN AFSFileID FileId,
                   IN AFSFileOpenCB *OpenCB,
                   IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB);

void
RDR_RequestFileExtentsSync( IN cm_user_t *userp,
                            IN AFSFileID FileId,
                            IN AFSRequestExtentsCB *RequestExtentsCB,
                            IN DWORD ResultBufferLength,
                            IN OUT AFSCommResult **ResultCB);

void
RDR_RequestFileExtentsAsync( IN cm_user_t *userp,
                             IN AFSFileID FileId,
                             IN AFSRequestExtentsCB *RequestExtentsCB,
                             IN OUT DWORD * ResultBufferLength,
                             IN OUT AFSSetFileExtentsCB **ResultCB);

void
RDR_ReleaseFileExtents( IN cm_user_t *userp,
                        IN AFSFileID FileId,
                        IN AFSReleaseExtentsCB *ReleaseExtentsCB,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);

cm_user_t *
RDR_UserFromCommRequest( IN AFSCommRequest * pRequest );

void
RDR_ReleaseUser( IN cm_user_t *userp);

#ifdef __cplusplus
}
#endif

