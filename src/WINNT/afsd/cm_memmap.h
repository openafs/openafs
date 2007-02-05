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
    afs_uint32          size;
    afs_uint32          magic;
    CHAR *              baseAddress;

    afs_uint32          stats;
    afs_uint32          chunkSize;
    afs_uint32          blockSize;
    afs_uint64          bufferSize;
    afs_uint32          cacheType;
    afs_uint32          dirty;

    cm_volume_t *       volumeBaseAddress;
    cm_cell_t   *       cellBaseAddress; 
    cm_aclent_t *       aclBaseAddress;
    cm_scache_t *       scacheBaseAddress;
    cm_nc_t     *       dnlcBaseAddress;
    cm_buf_t    *       bufHeaderBaseAddress;
    char *              bufDataBaseAddress;
    char *              bufEndOfData;

    cm_volume_t	*       allVolumesp;
    afs_uint32          currentVolumes;
    afs_uint32          maxVolumes;

    cm_cell_t	*       allCellsp;
    afs_uint32          currentCells;
    afs_uint32          maxCells;

    cm_volume_t	*       rootVolumep;
    cm_cell_t   *       rootCellp;
    cm_fid_t            rootFid;
    cm_scache_t *       rootSCachep;
    cm_scache_t         fakeSCache;
    afs_uint32          fakeDirVersion;

    cm_aclent_t *       aclLRUp;
    cm_aclent_t	*       aclLRUEndp;

    cm_scache_t	**      hashTablep;
    afs_uint32		hashTableSize;

    afs_uint32		currentSCaches;
    afs_uint32          maxSCaches;
    cm_scache_t *       scacheLRUFirstp;
    cm_scache_t *       scacheLRULastp;

    cm_nc_t 	*       ncfreelist;
    cm_nc_t 	*       nameCache;
    cm_nc_t 	**      nameHash; 

    cm_buf_t	*       buf_freeListp;
    cm_buf_t    *       buf_freeListEndp;
    cm_buf_t	*       buf_dirtyListp;
    cm_buf_t    *       buf_dirtyListEndp;
    cm_buf_t	**      buf_hashTablepp;
    cm_buf_t	**      buf_fileHashTablepp;
    cm_buf_t	*       buf_allp;
    afs_uint64		buf_nbuffers;
    afs_uint32		buf_blockSize;
    afs_uint32		buf_hashSize;
    afs_uint64		buf_nOrigBuffers;
    afs_uint64          buf_reservedBufs;
    afs_uint64          buf_maxReservedBufs;
    afs_uint64          buf_reserveWaiting;

    time_t              mountRootGen;
    afsUUID             Uuid;
    DWORD 		volSerialNumber;
    CHAR 		Sid[6 * sizeof(DWORD)];
} cm_config_data_t;

extern cm_config_data_t cm_data;

afs_uint64 GranularityAdjustment(afs_uint64 size);
afs_uint64 ComputeSizeOfConfigData(void);
afs_uint64 ComputeSizeOfVolumes(DWORD maxvols);
afs_uint64 ComputeSizeOfCells(DWORD maxcells);
afs_uint64 ComputeSizeOfACLCache(DWORD stats);
afs_uint64 ComputeSizeOfSCache(DWORD stats);
afs_uint64 ComputeSizeOfSCacheHT(DWORD stats);
afs_uint64 ComputeSizeOfDNLCache(void);
afs_uint64 ComputeSizeOfDataBuffers(afs_uint64 cacheBlocks, DWORD blockSize);
afs_uint64 ComputeSizeOfDataHT(afs_uint64 cacheBlocks);
afs_uint64 ComputeSizeOfDataHeaders(afs_uint64 cacheBlocks);
afs_uint64 ComputeSizeOfMappingFile(DWORD stats, DWORD maxVols, DWORD maxCells, DWORD chunkSize, afs_uint64 cacheBlocks, DWORD blockSize);
PSECURITY_ATTRIBUTES CreateCacheFileSA();
VOID  FreeCacheFileSA(PSECURITY_ATTRIBUTES psa);
int   cm_ShutdownMappedMemory(void);
int   cm_ValidateMappedMemory(char * cachePath);
int   cm_InitMappedMemory(DWORD virtualCache, char * cachePath, DWORD stats, DWORD chunkSize, afs_uint64 cacheBlocks );
#endif /* CM_MEMMAP_H */