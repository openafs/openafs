#include <windows.h>
#include "afsd.h"
#include "cm_memmap.h"

extern void afsi_log(char *pattern, ...);
extern DWORD cm_ValidateCache;

DWORD
GranularityAdjustment(DWORD size)
{
    SYSTEM_INFO sysInfo;
    static DWORD dwGranularity = 0;

    if ( !dwGranularity ) {
        GetSystemInfo(&sysInfo);
        afsi_log("Granularity - %lX", sysInfo.dwAllocationGranularity);
        dwGranularity = sysInfo.dwAllocationGranularity;
    }

    size = (size + (dwGranularity - 1)) & ~(dwGranularity - 1);
    return size;
}

DWORD 
ComputeSizeOfConfigData(void)
{
    DWORD size;
    size = sizeof(cm_config_data_t);
    return size;
}

DWORD
ComputeSizeOfVolumes(DWORD maxvols)
{
    DWORD size;
    size = maxvols * sizeof(cm_volume_t);
    return size;
}

DWORD
ComputeSizeOfCells(DWORD maxcells)
{
    DWORD size;
    size = maxcells * sizeof(cm_cell_t);
    return size;
}

DWORD 
ComputeSizeOfACLCache(DWORD stats)
{
    DWORD size;
    size = 2 * (stats + 10) * sizeof(cm_aclent_t);
    return size;
}

DWORD 
ComputeSizeOfSCache(DWORD stats)
{
    DWORD size;
    size = (stats + 10) * sizeof(cm_scache_t);
    return size;
}

DWORD 
ComputeSizeOfSCacheHT(DWORD stats)
{
    DWORD size;
    size = (stats + 10) / 2 * sizeof(cm_scache_t *);;
    return size;
}

DWORD 
ComputeSizeOfDNLCache(void)
{
    DWORD size;
    size = NHSIZE * sizeof(cm_nc_t *) + NCSIZE * sizeof(cm_nc_t);
    return size;
}

DWORD 
ComputeSizeOfDataBuffers(DWORD cacheBlocks, DWORD blockSize)
{
    DWORD size;
    size = cacheBlocks * blockSize;
    return size;
}

DWORD 
ComputeSizeOfDataHT(void)
{
    DWORD size;
    size = osi_PrimeLessThan(CM_BUF_HASHSIZE) * sizeof(cm_buf_t *);
    return size;
}

DWORD 
ComputeSizeOfDataHeaders(DWORD cacheBlocks)
{
    DWORD size;
    size = cacheBlocks * sizeof(cm_buf_t);
    return size;
}

DWORD
ComputeSizeOfMappingFile(DWORD stats, DWORD chunkSize, DWORD cacheBlocks, DWORD blockSize)
{
    DWORD size;
    
    size       =  ComputeSizeOfConfigData()
               +  ComputeSizeOfVolumes(stats/2) 
               +  ComputeSizeOfCells(stats/4) 
               +  ComputeSizeOfACLCache(stats)
               +  ComputeSizeOfSCache(stats)
               +  ComputeSizeOfSCacheHT(stats)
               +  ComputeSizeOfDNLCache()
               +  ComputeSizeOfDataBuffers(cacheBlocks, blockSize) 
               +  2 * ComputeSizeOfDataHT() 
               +  ComputeSizeOfDataHeaders(cacheBlocks);
    return size;    
}

/* Create a security attribute structure suitable for use when the cache file
 * is created.  What we mainly want is that only the administrator should be
 * able to do anything with the file.  We create an ACL with only one entry,
 * an entry that grants all rights to the administrator.
 */
PSECURITY_ATTRIBUTES CreateCacheFileSA()
{
    PSECURITY_ATTRIBUTES psa;
    PSECURITY_DESCRIPTOR psd;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID AdminSID;
    DWORD AdminSIDlength;
    PACL AdminOnlyACL;
    DWORD ACLlength;

    /* Get Administrator SID */
    AllocateAndInitializeSid(&authority, 2,
                              SECURITY_BUILTIN_DOMAIN_RID,
                              DOMAIN_ALIAS_RID_ADMINS,
                              0, 0, 0, 0, 0, 0,
                              &AdminSID);

    /* Create Administrator-only ACL */
    AdminSIDlength = GetLengthSid(AdminSID);
    ACLlength = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE)
        + AdminSIDlength - sizeof(DWORD);
    AdminOnlyACL = GlobalAlloc(GMEM_FIXED, ACLlength);
    InitializeAcl(AdminOnlyACL, ACLlength, ACL_REVISION);
    AddAccessAllowedAce(AdminOnlyACL, ACL_REVISION,
                         STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
                         AdminSID);

    /* Create security descriptor */
    psd = GlobalAlloc(GMEM_FIXED, sizeof(SECURITY_DESCRIPTOR));
    InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(psd, TRUE, AdminOnlyACL, FALSE);

    /* Create security attributes structure */
    psa = GlobalAlloc(GMEM_FIXED, sizeof(SECURITY_ATTRIBUTES));
    psa->nLength = sizeof(SECURITY_ATTRIBUTES);
    psa->lpSecurityDescriptor = psd;
    psa->bInheritHandle = TRUE;

    return psa;
}       


/* Free a security attribute structure created by CreateCacheFileSA() */
VOID FreeCacheFileSA(PSECURITY_ATTRIBUTES psa)
{
    BOOL b1, b2;
    PACL pAcl;

    GetSecurityDescriptorDacl(psa->lpSecurityDescriptor, &b1, &pAcl, &b2);
    GlobalFree(pAcl);
    GlobalFree(psa->lpSecurityDescriptor);
    GlobalFree(psa);
}       

static HANDLE hMemoryMappedFile = NULL;

int
cm_IsCacheValid(void)
{
    int rc = 1;

    afsi_log("Validating Cache Contents");

    if (cm_ValidateACLCache()) {
        afsi_log("ACL Cache validation failure");
        rc = 0;
    } else if (cm_ValidateDCache()) {
        afsi_log("Data Cache validation failure");
        rc = 0;
    } else if (cm_ValidateVolume()) {
        afsi_log("Volume validation failure");
        rc = 0;
    } else if (cm_ValidateCell()) {
        afsi_log("Cell validation failure");
        rc = 0;
    } else if (cm_ValidateSCache()) {
        afsi_log("Stat Cache validation failure");
        rc = 0;
    }

    return rc;
}

int
cm_ShutdownMappedMemory(void)
{
    cm_config_data_t * config_data_p = (cm_config_data_t *)cm_data.baseAddress;
    int dirty = 0;

    cm_ShutdownDCache();
    cm_ShutdownSCache();
    cm_ShutdownACLCache();
    cm_ShutdownCell();
    cm_ShutdownVolume();

    if (cm_ValidateCache == 2)
        dirty = !cm_IsCacheValid();

    *config_data_p = cm_data;
    config_data_p->dirty = dirty;
    UnmapViewOfFile(config_data_p);
    CloseHandle(hMemoryMappedFile);
    hMemoryMappedFile = NULL;

    afsi_log("Memory Mapped File has been closed");
}

int
cm_ValidateMappedMemory(char * cachePath)
{
    HANDLE hf = INVALID_HANDLE_VALUE, hm;
    PSECURITY_ATTRIBUTES psa;
    BY_HANDLE_FILE_INFORMATION fileInfo;
    int newFile = 1;
    DWORD mappingSize;
    char * baseAddress = NULL;
    cm_config_data_t * config_data_p;
        
    psa = CreateCacheFileSA();
    hf = CreateFile( cachePath,
                     GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     psa,
                     OPEN_EXISTING,
                     FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 
                     FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_FLAG_RANDOM_ACCESS,
                     NULL);
    FreeCacheFileSA(psa);

    if (hf == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error creating cache file \"%s\" error %d\n", 
                 cachePath, GetLastError());
        return CM_ERROR_INVAL;
    }

    /* The file is being re-used; check to see if the existing data can be reused */
    if ( !GetFileInformationByHandle(hf, &fileInfo) ) {
        CloseHandle(hf);
        fprintf(stderr, "Unable to obtain File Information\n");
        return CM_ERROR_INVAL;
    }

    afsi_log("Existing File Size: %08X:%08X",
              fileInfo.nFileSizeHigh,
              fileInfo.nFileSizeLow);
            
    hm = CreateFileMapping( hf,
                            NULL,
                            PAGE_READWRITE,
                            0, 
                            sizeof(cm_config_data_t),
                            NULL);
    if (hm == NULL) {
        if (GetLastError() == ERROR_DISK_FULL) {
            fprintf(stderr, "Error creating file mapping for \"%s\": disk full (%lX)\n",
                     cachePath, sizeof(cm_config_data_t));

            hm = CreateFileMapping( hf,
                                    NULL,
                                    PAGE_READWRITE,
                                    0, 
                                    fileInfo.nFileSizeLow,
                                    NULL);
            if (hm == NULL) {
                if (GetLastError() == ERROR_DISK_FULL) {
                    CloseHandle(hf);
                    return CM_ERROR_TOOMANYBUFS;
                } else {
                    fprintf(stderr,"Error creating file mapping for \"%s\": %d\n",
                              cachePath, GetLastError());
                    CloseHandle(hf);
                    return CM_ERROR_INVAL;
                }
            } else {
                fprintf(stderr, "Retry with file size (%lX) succeeds", 
                         fileInfo.nFileSizeLow);
            }
        } else {
            afsi_log("Error creating file mapping for \"%s\": %d",
                      cachePath, GetLastError());
            CloseHandle(hf);
            return CM_ERROR_INVAL;
        }
    }

    config_data_p = MapViewOfFile( hm,
                                   FILE_MAP_READ,
                                   0, 0,   
                                   sizeof(cm_config_data_t));
    if ( config_data_p == NULL ) {
        fprintf(stderr, "Unable to MapViewOfFile\n");
        if (hf != INVALID_HANDLE_VALUE)
            CloseHandle(hf);
        CloseHandle(hm);
        return CM_ERROR_INVAL;
    }

    if ( config_data_p->dirty ) {
        fprintf(stderr, "Previous session terminated prematurely\n");
        UnmapViewOfFile(config_data_p);
        CloseHandle(hm);               
        CloseHandle(hf);
        return CM_ERROR_INVAL;
    }

    mappingSize = config_data_p->bufferSize;
    baseAddress = config_data_p->baseAddress;
    UnmapViewOfFile(config_data_p);
    CloseHandle(hm);

    hm = CreateFileMapping( hf,
                            NULL,
                            PAGE_READWRITE,
                            0, mappingSize,
                            NULL);
    if (hm == NULL) {
        if (GetLastError() == ERROR_DISK_FULL) {
            fprintf(stderr, "Error creating file mapping for \"%s\": disk full [2]\n",
                  cachePath);
            CloseHandle(hf);
            return CM_ERROR_TOOMANYBUFS;
        }
        fprintf(stderr, "Error creating file mapping for \"%s\": %d\n",
                cachePath, GetLastError());
        CloseHandle(hf);
        return CM_ERROR_INVAL;
    }
    
    baseAddress = MapViewOfFileEx( hm,
                                   FILE_MAP_ALL_ACCESS,
                                   0, 0,   
                                   mappingSize,
                                   baseAddress );
    if (baseAddress == NULL) {
        fprintf(stderr, "Error mapping view of file: %d\n", GetLastError());
        baseAddress = MapViewOfFile( hm,
                                     FILE_MAP_ALL_ACCESS,
                                     0, 0,   
                                     mappingSize );
        if (baseAddress == NULL) {
            CloseHandle(hm);
            if (hf != INVALID_HANDLE_VALUE)
                CloseHandle(hf);
            return CM_ERROR_INVAL;
        }
        fprintf(stderr, "Unable to re-load cache file at base address\n");
        CloseHandle(hm);
        if (hf != INVALID_HANDLE_VALUE)
            CloseHandle(hf);
        return CM_ERROR_INVAL;
    }
    CloseHandle(hm);

    config_data_p = (cm_config_data_t *) baseAddress;

    fprintf(stderr,"AFS Cache data:\n");
    fprintf(stderr,"  Base Address   = %lX\n",baseAddress);
    fprintf(stderr,"  stats          = %d\n", config_data_p->stats);
    fprintf(stderr,"  chunkSize      = %d\n", config_data_p->chunkSize);
    fprintf(stderr,"  blockSize      = %d\n", config_data_p->blockSize);
    fprintf(stderr,"  bufferSize     = %d\n", config_data_p->bufferSize);
    fprintf(stderr,"  cacheType      = %d\n", config_data_p->cacheType);
    fprintf(stderr,"  currentVolumes = %d\n", config_data_p->currentVolumes);
    fprintf(stderr,"  maxVolumes     = %d\n", config_data_p->maxVolumes);
    fprintf(stderr,"  currentCells   = %d\n", config_data_p->currentCells);
    fprintf(stderr,"  maxCells       = %d\n", config_data_p->maxCells);
    fprintf(stderr,"  hashTableSize  = %d\n", config_data_p->hashTableSize );
    fprintf(stderr,"  currentSCaches = %d\n", config_data_p->currentSCaches);
    fprintf(stderr,"  maxSCaches     = %d\n", config_data_p->maxSCaches);
    cm_data = *config_data_p;      

    // perform validation of persisted data structures
    // if there is a failure, start from scratch
    if (!cm_IsCacheValid()) {
        fprintf(stderr,"Cache file fails validation test\n");
        UnmapViewOfFile(config_data_p);
        CloseHandle(hm);
        return CM_ERROR_INVAL;
    }

    fprintf(stderr,"Cache passes validation test\n");
    UnmapViewOfFile(config_data_p);
    CloseHandle(hm);
    return 0;
}

int
cm_InitMappedMemory(DWORD virtualCache, char * cachePath, DWORD stats, DWORD chunkSize, DWORD cacheBlocks)
{
    HANDLE hf = INVALID_HANDLE_VALUE, hm;
    PSECURITY_ATTRIBUTES psa;
    int newFile = 1;
    DWORD mappingSize;
    char * baseAddress = NULL;
    cm_config_data_t * config_data_p;
    char * p;

    mappingSize = ComputeSizeOfMappingFile(stats, chunkSize, cacheBlocks, CM_CONFIGDEFAULT_BLOCKSIZE);

    if ( !virtualCache ) {
        psa = CreateCacheFileSA();
        hf = CreateFile( cachePath,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         psa,
                         OPEN_ALWAYS,
                         FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 
                         FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_FLAG_RANDOM_ACCESS,
                         NULL);
        FreeCacheFileSA(psa);

        if (hf == INVALID_HANDLE_VALUE) {
            afsi_log("Error creating cache file \"%s\" error %d", 
                      cachePath, GetLastError());
            return CM_ERROR_INVAL;
        }
        
        if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
            BY_HANDLE_FILE_INFORMATION fileInfo;

            /* The file is being re-used; check to see if the existing data can be reused */
            afsi_log("Cache File \"%s\" already exists", cachePath);

            if ( GetFileInformationByHandle(hf, &fileInfo) ) {
                afsi_log("Existing File Size: %08X:%08X",
                          fileInfo.nFileSizeHigh,
                          fileInfo.nFileSizeLow);
                if (fileInfo.nFileSizeLow > GranularityAdjustment(mappingSize)) {
                    psa = CreateCacheFileSA();
                    hf = CreateFile( cachePath,
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     psa,
                                     TRUNCATE_EXISTING,
                                     FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 
                                     FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_FLAG_RANDOM_ACCESS,
                                     NULL);
                    FreeCacheFileSA(psa);

                    if (hf == INVALID_HANDLE_VALUE) {
                        afsi_log("Error creating cache file \"%s\" error %d", 
                                  cachePath, GetLastError());
                        return CM_ERROR_INVAL;
                    }

                    GetFileInformationByHandle(hf, &fileInfo);
                    afsi_log("     New File Size: %08X:%08X",
                              fileInfo.nFileSizeHigh,
                              fileInfo.nFileSizeLow);
                }

            }

            hm = CreateFileMapping( hf,
                                    NULL,
                                    PAGE_READWRITE,
                                    0, 
                                    sizeof(cm_config_data_t),
                                    NULL);
            if (hm == NULL) {
                if (GetLastError() == ERROR_DISK_FULL) {
                    afsi_log("Error creating file mapping for \"%s\": disk full (%lX)",
                              cachePath, sizeof(cm_config_data_t));

                    hm = CreateFileMapping( hf,
                                            NULL,
                                            PAGE_READWRITE,
                                            0, 
                                            mappingSize,
                                            NULL);
                    if (hm == NULL) {
                        if (GetLastError() == ERROR_DISK_FULL) {
                            CloseHandle(hf);
                            return CM_ERROR_TOOMANYBUFS;
                        } else {
                            afsi_log("Error creating file mapping for \"%s\": %d",
                                      cachePath, GetLastError());
                            CloseHandle(hf);
                            return CM_ERROR_INVAL;
                        }
                    } else {
                        afsi_log("Retry with mapping size (%lX) succeeds", mappingSize);
                    }
                } else {
                    afsi_log("Error creating file mapping for \"%s\": %d",
                              cachePath, GetLastError());
                    CloseHandle(hf);
                    return CM_ERROR_INVAL;
                }
            }

            config_data_p = MapViewOfFile( hm,
                                           FILE_MAP_READ,
                                           0, 0,   
                                           sizeof(cm_config_data_t));
            if ( config_data_p == NULL ) {
                if (hf != INVALID_HANDLE_VALUE)
                    CloseHandle(hf);
                CloseHandle(hm);
                return CM_ERROR_INVAL;
            }

            if ( config_data_p->size == sizeof(cm_config_data_t) &&
                 config_data_p->magic == CM_CONFIG_DATA_MAGIC &&
                 config_data_p->stats == stats &&
                 config_data_p->chunkSize == chunkSize &&
                 config_data_p->buf_nbuffers == cacheBlocks &&
                 config_data_p->blockSize == CM_CONFIGDEFAULT_BLOCKSIZE &&
                 config_data_p->bufferSize == mappingSize)
            {
                if ( config_data_p->dirty ) {
                    afsi_log("Previous session terminated prematurely");
                } else {
                    baseAddress = config_data_p->baseAddress;
                    newFile = 0;
                }
            } else {
                afsi_log("Configuration changed or Not a persistent cache file");
            }
            UnmapViewOfFile(config_data_p);
            CloseHandle(hm);
        }
    }

    hm = CreateFileMapping( hf,
                            NULL,
                            PAGE_READWRITE,
                            0, mappingSize,
                            NULL);
    if (hm == NULL) {
        if (GetLastError() == ERROR_DISK_FULL) {
            afsi_log("Error creating file mapping for \"%s\": disk full [2]",
                      cachePath);
            return CM_ERROR_TOOMANYBUFS;
        }
        afsi_log("Error creating file mapping for \"%s\": %d",
                  cachePath, GetLastError());
        return CM_ERROR_INVAL;
    }
    baseAddress = MapViewOfFileEx( hm,
                                   FILE_MAP_ALL_ACCESS,
                                   0, 0,   
                                   mappingSize,
                                   baseAddress );
    if (baseAddress == NULL) {
        afsi_log("Error mapping view of file: %d", GetLastError());
        baseAddress = MapViewOfFile( hm,
                                     FILE_MAP_ALL_ACCESS,
                                     0, 0,   
                                     mappingSize );
        if (baseAddress == NULL) {
            if (hf != INVALID_HANDLE_VALUE)
                CloseHandle(hf);
            CloseHandle(hm);
            return CM_ERROR_INVAL;
        }
        newFile = 1;
    }
    CloseHandle(hm);

    config_data_p = (cm_config_data_t *) baseAddress;

    if (!newFile) {
        afsi_log("Reusing existing AFS Cache data: Base Address = %lX",baseAddress);
        cm_data = *config_data_p;      

        // perform validation of persisted data structures
        // if there is a failure, start from scratch
        if (cm_ValidateCache && !cm_IsCacheValid()) {
            newFile = 1;
        }
    }

    if ( newFile ) {
        afsi_log("Building AFS Cache from scratch");
        cm_data.size = sizeof(cm_config_data_t);
        cm_data.magic = CM_CONFIG_DATA_MAGIC;
        cm_data.baseAddress = baseAddress;
        cm_data.stats = stats;
        cm_data.chunkSize = chunkSize;
        cm_data.blockSize = CM_CONFIGDEFAULT_BLOCKSIZE;
        cm_data.bufferSize = mappingSize;
        cm_data.hashTableSize = osi_PrimeLessThan(stats / 2 + 1);
        if (virtualCache) {
            cm_data.cacheType = CM_BUF_CACHETYPE_VIRTUAL;
        } else {
            cm_data.cacheType = CM_BUF_CACHETYPE_FILE;
        }

        cm_data.buf_nbuffers = cacheBlocks;
        cm_data.buf_nOrigBuffers = 0;
        cm_data.buf_blockSize = CM_BUF_BLOCKSIZE;
        cm_data.buf_hashSize = CM_BUF_HASHSIZE;

        cm_data.mountRootGen = time(NULL);

        baseAddress += ComputeSizeOfConfigData();
        cm_data.volumeBaseAddress = (cm_volume_t *) baseAddress;
        baseAddress += ComputeSizeOfVolumes(stats/2);
        cm_data.cellBaseAddress = (cm_cell_t *) baseAddress;
        baseAddress += ComputeSizeOfCells(stats/4);
        cm_data.aclBaseAddress = (cm_aclent_t *) baseAddress;
        baseAddress += ComputeSizeOfACLCache(stats);
        cm_data.scacheBaseAddress = (cm_scache_t *) baseAddress;
        baseAddress += ComputeSizeOfSCache(stats);
        cm_data.hashTablep = (cm_scache_t **) baseAddress;
        baseAddress += ComputeSizeOfSCacheHT(stats);
        cm_data.dnlcBaseAddress = (cm_nc_t *) baseAddress;
        baseAddress += ComputeSizeOfDNLCache();
        cm_data.buf_hashTablepp = (cm_buf_t **) baseAddress;
        baseAddress += ComputeSizeOfDataHT();
        cm_data.buf_fileHashTablepp = (cm_buf_t **) baseAddress;
        baseAddress += ComputeSizeOfDataHT();
        cm_data.bufHeaderBaseAddress = (cm_buf_t *) baseAddress;
        baseAddress += ComputeSizeOfDataHeaders(cacheBlocks);
        cm_data.bufDataBaseAddress = (char *) baseAddress;
        baseAddress += ComputeSizeOfDataBuffers(cacheBlocks, CM_CONFIGDEFAULT_BLOCKSIZE);
        cm_data.bufEndOfData = (char *) baseAddress;

        cm_data.fakeDirVersion = 0x8;

        UuidCreate((UUID *)&cm_data.Uuid);
    }

    UuidToString((UUID *)&cm_data.Uuid, &p);
    afsi_log("Initializing Uuid to %s",p);
    RpcStringFree(&p);

    afsi_log("Initializing Volume Data");
    cm_InitVolume(newFile, stats/2);

    afsi_log("Initializing Cell Data");
    cm_InitCell(newFile, stats/4);

    afsi_log("Initializing ACL Data");
    cm_InitACLCache(newFile, 2*stats);

    afsi_log("Initializing Stat Data");
    cm_InitSCache(newFile, stats);
        
    afsi_log("Initializing Data Buffers");
    cm_InitDCache(newFile, 0, cacheBlocks);

    *config_data_p = cm_data;
    config_data_p->dirty = 1;
    
    hMemoryMappedFile = hf;
    afsi_log("Cache Initialization Complete");
    return 0;
}

