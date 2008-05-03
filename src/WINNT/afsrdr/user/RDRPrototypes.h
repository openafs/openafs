#ifdef __cplusplus
extern "C" {
#endif

DWORD
RDR_Initialize( void);

DWORD
RDR_Shutdown( void);

DWORD 
WINAPI 
RDR_RequestWorkerThread( LPVOID lpParameter);

DWORD
RDR_ProcessWorkerThreads(void);

void
RDR_ProcessRequest( AFSCommRequest *RequestBuffer);

void
RDR_EnumerateDirectory( IN AFSFileID ParentID,
                        IN AFSDirQueryCB *QueryCB,
                        IN DWORD ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);
    
void
RDR_EvaluateNodeByName( IN     AFSFileID ParentID,
                        IN     WCHAR   *Name,
                        IN     DWORD    NameLength,
                        IN     DWORD    CaseSensitive,
                        IN     DWORD    ResultBufferLength,
                        IN OUT AFSCommResult **ResultCB);
    
void
RDR_EvaluateNodeByID( IN     AFSFileID ParentID,
                      IN     AFSFileID SourceID,
                      IN     DWORD    ResultBufferLength,
                      IN OUT AFSCommResult **ResultCB);
    
void
RDR_CreateFileEntry( IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN AFSFileCreateCB *CreateCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_UpdateFileEntry( IN AFSFileID FileId,
                     IN AFSFileUpdateCB *UpdateCB,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_DeleteFileEntry( IN AFSFileID FileId,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_RenameFileEntry( IN AFSFileID FileId,
                     IN AFSFileRenameCB *RenameCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_FlushFileEntry( IN AFSFileID FileId,
                    IN DWORD ResultBufferLength,
                    IN OUT AFSCommResult **ResultCB);

void
RDR_OpenFileEntry( IN AFSFileID FileId,
                     IN AFSFileOpenCB *OpenCB,
                     IN DWORD ResultBufferLength,
                   IN OUT AFSCommResult **ResultCB);void

RDR_RequestFileExtents( IN AFSFileID FileId,
                     IN AFSFileRequestExtentsCB *RequestExtentsCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

void
RDR_ReleaseFileExtents( IN AFSFileID FileId,
                     IN AFSFileReleaseExtentsCB *ReleaseExtentsCB,
                     IN DWORD ResultBufferLength,
                     IN OUT AFSCommResult **ResultCB);

#ifdef __cplusplus
}
#endif

