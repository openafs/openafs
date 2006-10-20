/*
 * Copyright 2004, Secure Endpoints Inc.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CM_MEMMAP_H
#define CM_MEMMAP_H 1

#define CM_CONFIG_DATA_MAGIC            ('A' | 'F'<<8 | 'S'<<16 | 'D'<<24)

typedef struct cm_config_data {
    long                size;
    long                magic;
    CHAR *              baseAddress;

    long                stats;
    long                chunkSize;
    long                blockSize;
    long                bufferSize;
    long                cacheType;
    long                dirty;

    cm_volume_t *       volumeBaseAddress;
    cm_cell_t   *       cellBaseAddress; 
    cm_aclent_t *       aclBaseAddress;
    cm_scache_t *       scacheBaseAddress;
    cm_nc_t     *       dnlcBaseAddress;
    cm_buf_t    *       bufHeaderBaseAddress;
    char *              bufDataBaseAddress;
    char *              bufEndOfData;

    cm_volume_t	*       allVolumesp;
    long                currentVolumes;
    long                maxVolumes;

    cm_cell_t	*       allCellsp;
    long                currentCells;
    long                maxCells;

    cm_volume_t	*       rootVolumep;
    cm_cell_t   *       rootCellp;
    cm_fid_t            rootFid;
    cm_scache_t *       rootSCachep;
    cm_scache_t         fakeSCache;
    afs_uint32          fakeDirVersion;

    cm_aclent_t *       aclLRUp;
    cm_aclent_t	*       aclLRUEndp;

    cm_scache_t	**      hashTablep;
    long		hashTableSize;

    long		currentSCaches;
    long                maxSCaches;
    cm_scache_t *       scacheLRUFirstp;
    cm_scache_t *       scacheLRULastp;

    cm_nc_t 	*       ncfreelist;
    cm_nc_t 	*       nameCache;
    cm_nc_t 	**      nameHash; 

    cm_buf_t	*       buf_freeListp;
    cm_buf_t    *       buf_freeListEndp;
    cm_buf_t	**      buf_hashTablepp;
    cm_buf_t	**      buf_fileHashTablepp;
    cm_buf_t	*       buf_allp;
    long		buf_nbuffers;
    long		buf_blockSize;
    long		buf_hashSize;
    long		buf_nOrigBuffers;
    long                buf_reservedBufs;
    long                buf_maxReservedBufs;
    long                buf_reserveWaiting;

    time_t              mountRootGen;
    afsUUID             Uuid;
    DWORD 		volSerialNumber;
    CHAR 		Sid[6 * sizeof(DWORD)];
} cm_config_data_t;

extern cm_config_data_t cm_data;

DWORD GranularityAdjustment(DWORD size);
DWORD ComputeSizeOfConfigData(void);
DWORD ComputeSizeOfVolumes(DWORD maxvols);
DWORD ComputeSizeOfCells(DWORD maxcells);
DWORD ComputeSizeOfACLCache(DWORD stats);
DWORD ComputeSizeOfSCache(DWORD stats);
DWORD ComputeSizeOfSCacheHT(DWORD stats);
DWORD ComputeSizeOfDNLCache(void);
DWORD ComputeSizeOfDataBuffers(DWORD cacheBlocks, DWORD blockSize);
DWORD ComputeSizeOfDataHT(void);
DWORD ComputeSizeOfDataHeaders(DWORD cacheBlocks);
DWORD ComputeSizeOfMappingFile(DWORD stats, DWORD maxVols, DWORD maxCells, DWORD chunkSize, DWORD cacheBlocks, DWORD blockSize);
PSECURITY_ATTRIBUTES CreateCacheFileSA();
VOID  FreeCacheFileSA(PSECURITY_ATTRIBUTES psa);
int   cm_ShutdownMappedMemory(void);
int   cm_ValidateMappedMemory(char * cachePath);
int   cm_InitMappedMemory(DWORD virtualCache, char * cachePath, DWORD stats, DWORD chunkSize, DWORD cacheBlocks );
#endif /* CM_MEMMAP_H */