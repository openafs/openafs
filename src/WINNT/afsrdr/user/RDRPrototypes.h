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
RDR_ProcessRequest( KDFsdCommRequest *RequestBuffer);

void
RDR_EnumerateDirectory( IN KDFileID ParentID,
                        IN KDFsdDirQueryCB *QueryCB,
                        IN DWORD ResultBufferLength,
                        IN OUT KDFsdCommResult **ResultCB);
    
void
RDR_EvaluateNodeByName( IN     KDFileID ParentID,
                        IN     WCHAR   *Name,
                        IN     DWORD    NameLength,
                        IN     DWORD    CaseSensitive,
                        IN     DWORD    ResultBufferLength,
                        IN OUT KDFsdCommResult **ResultCB);
    
void
RDR_EvaluateNodeByID( IN     KDFileID ParentID,
                      IN     KDFileID SourceID,
                      IN     DWORD    ResultBufferLength,
                      IN OUT KDFsdCommResult **ResultCB);
    
void
RDR_CreateFileEntry( IN WCHAR *FileName,
                     IN DWORD FileNameLength,
                     IN KDFsdFileCreateCB *CreateCB,
                     IN DWORD ResultBufferLength,
                     IN OUT KDFsdCommResult **ResultCB);

void
RDR_UpdateFileEntry( IN KDFileID FileId,
                     IN KDFsdFileUpdateCB *UpdateCB,
                     IN OUT KDFsdCommResult **ResultCB);

void
RDR_DeleteFileEntry( IN KDFileID FileId,
                     IN OUT KDFsdCommResult **ResultCB);

void
RDR_RenameFileEntry( IN KDFileID FileId,
                     IN KDFsdFileRenameCB *RenameCB,
                     IN DWORD ResultBufferLength,
                     IN OUT KDFsdCommResult **ResultCB);

#ifdef __cplusplus
}
#endif

