/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This code is experimental persistent disk cache support for the
   Windows 95/DJGPP AFS client.  It uses synchronous I/O and assumes
   non-preemptible threads (which is the case in DJGPP), so it has
   no locking. */

   
#ifdef DISKCACHE95

#include <afs/param.h>
#include <afs/stds.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include "afsd.h"

cm_diskcache_t **diskHashTable;    /* pointers to hash chains */
Queue diskLRUList;
Queue openFileList;
cm_diskcache_t *diskCBBuf;

extern int afs_diskCacheChunks;
/*extern int cm_diskCacheChunkSize;*/
extern long buf_blockSize;
long cm_diskCacheChunkSize;
extern char cm_cachePath[];
extern int cm_cachePathLen;
extern int cm_diskCacheEnabled;

int cacheInfo_fd;
int accessOrd = 0;            /* monotonically increasing access ordinal */
int updates = 0;
int afs_dhashsize = 2048;
int openCacheFiles = 0;

char cacheInfoPath[128];
char cacheFileName[128];

extern int errno;

#define MAX_OPEN_FILES 22

/* internal functions */
void diskcache_WriteCacheInfo(cm_diskcache_t *dcp);
int diskcache_New();
cm_diskcache_t *diskcache_Find(cm_fid_t *fid, int chunk);
cm_diskcache_t *diskcache_Alloc(cm_fid_t *fid, int chunk, int dataVersion);
int diskcache_Read(cm_diskcache_t *dcp, char *buf, int size);
int diskcache_Write(cm_diskcache_t *dcp, char *buf, int size);

#define complain printf

#define OFFSET_TO_CHUNK(a) (LargeIntegerDivideByLong((a), cm_diskCacheChunkSize))
#define GEN_CACHE_DIR_NAME(name, path, i) \
           sprintf(name, "%s\\D%07d", cm_cachePath, (i) / CACHE_FILES_PER_DIR)
#define GEN_CACHE_FILE_NAME(name, path, i) \
           sprintf(name, "%s\\D%07d\\C%07d", path, (i) / CACHE_FILES_PER_DIR, \
                   (i) % CACHE_FILES_PER_DIR)

/* Initialize the disk cache */
int diskcache_Init()
{
  int i;
  int rc;
  int fd;
  int invalid;
  int index;
  char *chunkBuf;
  char tmpBuf[512];
  struct stat cacheInfoStat, chunkStat;
  cm_cacheInfoHdr_t hdr;
  cm_diskcache_t *dcp;
  int validCount = 0;

  if (!cm_diskCacheEnabled)
    return 0;
  
  cm_diskCacheChunkSize = buf_blockSize;
  if (cm_diskCacheChunkSize % buf_blockSize != 0)
  {
    complain("Error: disk cache chunk size %d not a multiple of buffer size %d\n",
             cm_diskCacheChunkSize, buf_blockSize);
    return CM_ERROR_INVAL;
  }
  
  /* alloc mem for chunk file control blocks */
  diskCBBuf = (cm_diskcache_t *) malloc(afs_diskCacheChunks * sizeof(cm_diskcache_t));
  if (diskCBBuf == NULL)
    return CM_ERROR_SPACE;
  memset(diskCBBuf, 0, afs_diskCacheChunks * sizeof(cm_diskcache_t));

  /* alloc mem for hash table pointers */
  diskHashTable = (cm_diskcache_t **) malloc(afs_dhashsize * sizeof(cm_diskcache_t *));
  if (diskHashTable == NULL)
    return CM_ERROR_SPACE;
  memset(diskHashTable, 0, afs_dhashsize*sizeof(cm_diskcache_t *));

  QInit(&diskLRUList);
  QInit(&openFileList);
  
  /*sprintf(cacheInfoPath, "%s\\%s", cm_cachePath, CACHE_INFO_FILE);*/
  memset(cacheInfoPath, 0, 128);
  DPRINTF("cm_cachePath=%s\n", cm_cachePath);
  strncpy(cacheInfoPath, cm_cachePath, 50);
  strcat(cacheInfoPath, "\\");
  strcat(cacheInfoPath, CACHE_INFO_FILE);
  DPRINTF("cacheInfoPath=%s\n", cacheInfoPath);
  
  cacheInfo_fd = open(cacheInfoPath, O_RDWR | O_BINARY);
  
  if (cacheInfo_fd < 0)
  {
    /* file not present */
    return diskcache_New();   /* initialize new empty disk cache */
  }

  /* get stat of cache info file */
  rc = fstat(cacheInfo_fd, &cacheInfoStat);

  /* Check for valid header in cache info file */
  rc = read(cacheInfo_fd, &hdr, sizeof(cm_cacheInfoHdr_t));
  if (rc < sizeof(cm_cacheInfoHdr_t) ||
      hdr.magic != CACHE_INFO_MAGIC)
  /*hdrp = (cm_cacheInfoHdr_t *) tmpBuf;*/
  {
    close(cacheInfo_fd);
    return diskcache_New();
  }
        
  if (hdr.chunks != afs_diskCacheChunks ||
      hdr.chunkSize != cm_diskCacheChunkSize)
  {
    /* print error message saying params don't match */
    return CM_ERROR_INVAL;
  }
  
  chunkBuf = (char *) malloc(cm_diskCacheChunkSize);
  if (chunkBuf == NULL)
    return CM_ERROR_SPACE;

  /* read metadata from cache info file into control blocks */
  /* reconstruct hash chains based on fid, chunk */
  for (i = 0; i < afs_diskCacheChunks; i++)
  {  /* for all cache chunks */
    if (i % 500 == 0)
    {
      printf("%d...", i);
      fflush(stdout);
    }
    dcp = &diskCBBuf[i];
    dcp->refCount = 0;
    rc = read(cacheInfo_fd, &dcp->f, sizeof(struct fcache));
    if (rc < sizeof(struct fcache))
    {
      /* print error message about reading cache info file */
      /* this isn't the right error code for a read error */
      return CM_ERROR_INVAL;
    }

    if (dcp->f.index != i)
      return CM_ERROR_INVAL;   /* index should match position in cache info file */

    /* Try to open cache file.  This chunk will be invalidated if we can't
       find the file, the file is newer than the cache info file, the file
       size doesn't match the cache info file, or the file's header is
       invalid. */
    GEN_CACHE_FILE_NAME(cacheFileName, cm_cachePath, i);
#if 1
    /*fd = open(cacheFileName, O_RDWR | O_BINARY);
    if (fd < 0) invalid = 1;
    else
    {
    rc = fstat(fd, &chunkStat);*/
    rc = stat(cacheFileName, &chunkStat);

      if (rc < 0) invalid = 1;
      /*else if (cacheInfoStat.st_mtime < chunkStat.st_mtime + 120) invalid = 1;*/
      else if (cacheInfoStat.st_mtime < chunkStat.st_mtime) invalid = 1;
      /*else if (cacheInfoStat.st_mtime < dcp->f.modTime + 120) invalid = 1;*/
      else if (cacheInfoStat.st_mtime < dcp->f.modTime) invalid = 1;
      else if (cm_diskCacheChunkSize != chunkStat.st_size ||
               dcp->f.chunkBytes != chunkStat.st_size) invalid = 1;
      /*else
        {*/
        /*rc = read(fd, chunkBuf, cm_diskCacheChunkSize);
          if (rc < 0) invalid = 1;*/

        /*else
        {
          cacheFileHdrP = (cm_cacheFileHdr_t *) chunkBuf;
          if (cacheFileHdrP->magic != CACHE_FILE_MAGIC ||
              cacheFileHdrP->index != i)
          {
            invalid = 1;
          }
        }*/
      /*}*/
      /*}*/
#else
    invalid = 0;
#endif
      
    if (invalid == 0)
    {
      /* Cache file seems to be valid */
    
      validCount++;
      DPRINTF("Found fid/chunk=%08x-%08x-%08x-%08x/%04d in slot=%d dcp=%x\n",
             dcp->f.fid.cell, dcp->f.fid.volume, dcp->f.fid.vnode,
             dcp->f.fid.unique, dcp->f.chunk, i, dcp);
      /* Put control block in hash table */
      index = DCHash(&dcp->f.fid, dcp->f.chunk);
      /*osi_QAdd(&diskHashTable[index], &dcp->f.hashq);*/

      /* add to head of hash list.  (we should probably look at
         ord here instead.  use queues?) */
      dcp->hash_next = diskHashTable[index];
      dcp->hash_prev = NULL;
      if (diskHashTable[index]) diskHashTable[index]->hash_prev = dcp;
      diskHashTable[index] = dcp;

      /* Add to LRU queue in access time order (lowest at tail) */
      QAddOrd(&diskLRUList, &dcp->lruq, dcp->f.accessOrd);

      close(fd);
    }
    else
    {
      /* Cache file is invalid */
      
      /* Create the cache file with correct size */
      memset(chunkBuf, 0, cm_diskCacheChunkSize);
      /*cacheFileHdrP->magic = CACHE_FILE_MAGIC;
        cacheFileHdrP->index = i;*/
      
      if (fd != 0) close(fd);
      /* Note that if the directory this file is supposed to be in doesn't
         exist, the creat call will fail and we will return an error. */
      /*fd = creat(cacheFileName, S_IRUSR|S_IWUSR);*/
      fd = open(cacheFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR);
      if (fd < 0) return CM_ERROR_INVAL;   /* couldn't create file */
      rc = write(fd, chunkBuf, cm_diskCacheChunkSize);
      if (rc < 0)   /* ran out of space? */
        return CM_ERROR_INVAL;
      close(fd);
      
      /* We consider an invalid chunk as empty, so we put it at tail of LRU */
      memset(dcp, 0, sizeof(cm_diskcache_t));
      dcp->f.accessOrd = 0;
      dcp->f.states = DISK_CACHE_EMPTY;
      dcp->f.index = i;
      dcp->f.chunkBytes = cm_diskCacheChunkSize;
      /*osi_QAdd(diskLRUList, &dcp->lruq);*/
      QAddOrd(&diskLRUList, &dcp->lruq, dcp->f.accessOrd);     /* head is LRU */

      /* write out cache info modifications */
      lseek(cacheInfo_fd, -sizeof(struct fcache), SEEK_CUR);
      write(cacheInfo_fd, &dcp->f, sizeof(struct fcache));
    }
  }  /* for all cache chunks */

  free(chunkBuf);
  /*close(cacheInfo_fd);*/
  fprintf(stderr, "\nFound %d of %d valid %d-byte blocks\n", validCount,
          afs_diskCacheChunks, cm_diskCacheChunkSize);

  return 0;
}

/* create empty disk cache files */
/* assumes tables have already been malloc'd by diskcache_Init */
int diskcache_New()
{
  int i;
  int rc;
  int fd;
  int invalid;
  int index;
  /*char cacheInfoPath[256];
    char cacheFileName[256];*/
  char dirName[256];
  char *chunkBuf;
  struct stat cacheInfoStat, chunkStat;
  cm_cacheInfoHdr_t hdr;
  cm_diskcache_t *dcp;
  
  sprintf(cacheInfoPath, "%s\\%s", cm_cachePath, CACHE_INFO_FILE);
  /*cacheInfo_fd = creat(cacheInfoPath, S_IRUSR | S_IWUSR);*/
  cacheInfo_fd = open(cacheInfoPath, O_RDWR | O_BINARY | O_CREAT | O_TRUNC,
                      S_IRUSR | S_IWUSR);
  if (cacheInfo_fd < 0)
  {
    complain("diskcache_New: Error creating cache info file in cache directory %s\n",
             cm_cachePath);
    return CM_ERROR_INVAL;
  }

  /* write valid header */
  hdr.magic = CACHE_INFO_MAGIC;
  hdr.chunks = afs_diskCacheChunks;
  hdr.chunkSize = cm_diskCacheChunkSize;
  rc = write(cacheInfo_fd, (char *) &hdr, sizeof(cm_cacheInfoHdr_t));
  if (rc < 0)
    return CM_ERROR_INVAL;
  
  chunkBuf = (char *) malloc(cm_diskCacheChunkSize);
  if (chunkBuf == NULL)
    return CM_ERROR_SPACE;
  memset(chunkBuf, 0, cm_diskCacheChunkSize);

  for (i = 0; i < afs_diskCacheChunks; i++)
  {  /* for all cache chunks */
    if (i % 500 == 0)
    {
      printf("%d...", i);
      fflush(stdout);
    }

    dcp = &diskCBBuf[i];

    dcp->refCount = 0;
    /* $$$: init mutex mx */
    memset(dcp, 0, sizeof(cm_diskcache_t));
    dcp->f.accessOrd = 0;
    dcp->f.index = i;
    dcp->f.states = DISK_CACHE_EMPTY;
    dcp->f.chunkBytes = cm_diskCacheChunkSize;
    QAddT(&diskLRUList, &dcp->lruq, dcp->f.accessOrd);     /* head is LRU */
    rc = write(cacheInfo_fd, &dcp->f, sizeof(struct fcache));

    if (i % CACHE_FILES_PER_DIR == 0)
    {
      GEN_CACHE_DIR_NAME(dirName, cm_cachePath, i);
      rc = mkdir(dirName, S_IRUSR | S_IWUSR);
      if (rc < 0 && errno != EEXIST)
      {
        complain("diskcache_New: Couldn't create cache directory %s\n", dirName);
        return CM_ERROR_INVAL;
      }
    }
    
    GEN_CACHE_FILE_NAME(cacheFileName, cm_cachePath, i);
    /*fd = creat(cacheFileName, S_IRUSR | S_IWUSR);*/
    fd = open(cacheFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC,
              S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
      if (errno == ENOSPC)
        complain("diskcache_New: Not enough space in cache path to create file %s\n",
                 cacheFileName);
      else
        complain("diskcache_New: Couldn't create cache file %s\n", cacheFileName);
      return CM_ERROR_INVAL;
    }

    /*fileHdr.magic = CACHE_FILE_MAGIC;
    fileHdr.index = i;
    rc = write(fd, &fileHdr, sizeof(cm_cacheFileHdr_t));  /* maybe we should write
                                                             a full block? */
    /*if (rc == 0)
      {*/
      rc = write(fd, chunkBuf, cm_diskCacheChunkSize);
      /*}*/

    if (rc < 0)
    {
      if (errno == ENOSPC)
        complain("diskcache_New: Not enough space in cache path to write to file %s\n",
                 cacheFileName);
      else
        complain("diskcache_New: Couldn't write to cache file %s\n",
                 cacheFileName);
      return CM_ERROR_INVAL;
    }

    close(fd);
  }  /* for all cache chunks */

  free(chunkBuf);
    
  /*close(cacheInfo_fd);*/

  return 0;
}

/* Get chunk from the cache or allocate a new chunk */
int diskcache_Get(cm_fid_t *fid, osi_hyper_t *offset, char *buf, int size, int *dataVersion, int *dataCount, cm_diskcache_t **dcpRet)
{
  cm_diskcache_t *dcp;
  int rc;
  int chunk;

  
  if (!cm_diskCacheEnabled)
  {
    *dcpRet = NULL;
    return 0;
  }

  chunk = OFFSET_TO_CHUNK(*offset);  /* chunk number */

  DPRINTF("diskcache_Get: fid/chunk=%08x-%08x-%08x-%08x/%04d\n",
           fid->cell, fid->volume, fid->vnode, fid->unique, chunk);
  
  dcp = diskcache_Find(fid, chunk);
  if (dcp != NULL)
  {
    rc = diskcache_Read(dcp, buf, size);
    *dataVersion = dcp->f.dataVersion;   /* update caller's data version */
    if (rc < 0)
      return -1;
    else
      *dataCount = rc;
  }
  else
  {
    dcp = diskcache_Alloc(fid, chunk, *dataVersion);
    if (dcp == NULL)
      return -1;
  }

  if (++updates >= CACHE_INFO_UPDATES_PER_WRITE)
  {
    updates = 0;
    diskcache_WriteCacheInfo(dcp);  /* update cache info for this slot */
  }

  *dcpRet = dcp;
  /*printf("diskcache_Get: returning dcp=%x\n", dcp);*/
  return 0;
}


/* Look for a file chunk in the cache */
cm_diskcache_t *diskcache_Find(cm_fid_t *fid, int chunk)
{
  int index;
  cm_diskcache_t *dcp;
  cm_diskcache_t *prev;

  index = DCHash(fid, chunk);
  dcp = diskHashTable[index];
  prev = NULL;

  while (dcp != NULL)
  {
    if (cm_FidCmp(&dcp->f.fid, fid) == 0 && chunk == dcp->f.chunk)
    {
      dcp->f.accessOrd = accessOrd++;
      /* Move it to the beginning of the list */
      if (diskHashTable[index] != dcp)
      {
        assert(dcp->hash_prev->hash_next == dcp);
        dcp->hash_prev->hash_next = dcp->hash_next;
        if (dcp->hash_next)
        {
          assert(dcp->hash_next->hash_prev == dcp);
          dcp->hash_next->hash_prev = dcp->hash_prev;
        }
        dcp->hash_next = diskHashTable[index];
        dcp->hash_prev = NULL;
        if (diskHashTable[index]) diskHashTable[index]->hash_prev = dcp;
        diskHashTable[index] = dcp;
      }
      break;
    }
    prev = dcp;
    dcp = dcp->hash_next;
  }
      
  if (dcp)
    DPRINTF("diskcache_Find: fid/chunk=%08x-%08x-%08x-%08x/%04d slot=%d hash=%d dcp=%x\n",
           fid->cell, fid->volume, fid->vnode, fid->unique, chunk, dcp->f.index, index, dcp);
  else
    DPRINTF("diskcache_Find: fid/chunk=%08x/%04d not found\n",
           fid->unique, chunk);
    
  return dcp;
}
    
int diskcache_Read(cm_diskcache_t *dcp, char *buf, int size)
{
  char cacheFileName[256];
  int fd;
  int rc;
  int opened = 0;

  GEN_CACHE_FILE_NAME(cacheFileName, cm_cachePath, dcp->f.index);

  DPRINTF("diskcache_Read: filename=%s dcp=%x\n", cacheFileName,
         dcp);
  
  /* For reads, we will use the fd if already open, but we won't leave
     the file open.  Note that if we use async I/O, we will need to
     do locking to prevent someone from closing the file while I/O
     is going on.  But for now, all I/O is synchronous, and threads
     are non-preemptible. */
  
  if (dcp->openfd == 0)
  {
    fd = open(cacheFileName, O_RDWR | O_BINARY);
    if (fd < 0)
    {
      complain("diskcache_Read: Couldn't open cache file %s\n", cacheFileName);
      return -1;
    }
    opened = 1;
  }
  else
    fd = dcp->openfd;

  if (fd < 0)
  {
    complain("diskcache_Read: Couldn't open cache file %s\n", cacheFileName);
    return -1;
  }

  rc = read(fd, buf, size);
  if (rc < 0)
  {
    complain("diskcache_Read: Couldn't read cache file %s\n", cacheFileName);
    close(fd); return -1;
  }
    
  if (opened)
    close(fd);   /* close it if we opened it */
  return rc;  /* bytes read */
}

/* Write out buffer to disk */
int diskcache_Update(cm_diskcache_t *dcp, char *buf, int size, int dataVersion)
{
  if (!cm_diskCacheEnabled)
    return 0;

  DPRINTF("diskcache_Update dcp=%x, dataVersion=%d\n", dcp, dataVersion);
  diskcache_Write(dcp, buf, size);
  /*diskcache_SetMRU(dcp);*/
  dcp->f.dataVersion = dataVersion;
  /*dcp->f.accessOrd = accessOrd++;*/
  /*QMoveToTail(&diskLRUList, &dcp->lruq, dcp->f.accessOrd);*/

  if (++updates >= CACHE_INFO_UPDATES_PER_WRITE)
  {
    updates = 0;
    diskcache_WriteCacheInfo(dcp);  /* update cache info */
  }
  return 0;
}

/* Allocate a new chunk file control block for this fid/chunk */
cm_diskcache_t *diskcache_Alloc(cm_fid_t *fid, int chunk, int dataVersion)
{
  cm_diskcache_t *dcp;
  QLink* q;
  int index;
  int stole=0, stolen_chunk, stolen_fid_unique;
  
  /* Remove LRU elt. (head) from free list */
  q = QServe(&diskLRUList);
  if (q == NULL)
    dcp = NULL;
  else
    dcp = (cm_diskcache_t *) MEM_TO_OBJ(cm_diskcache_t, lruq, q);
  if (dcp == NULL)
  {
    DPRINTF("diskcache_Alloc: fid/chunk=%08x/%04d allocation failed\n",
           fid->unique, chunk);
    return NULL;
  }

  /* Use this element for this fid/chunk */
  if (dcp->f.states == DISK_CACHE_USED)
  {
    /* Remove from old hash chain */
    if (dcp->hash_prev)
    {
      assert(dcp->hash_prev->hash_next == dcp);
      dcp->hash_prev->hash_next = dcp->hash_next;
    }
    else
    {
      index = DCHash(&dcp->f.fid, dcp->f.chunk);
      diskHashTable[index] = dcp->hash_next;
    }
    if (dcp->hash_next)
    {
      assert(dcp->hash_next->hash_prev == dcp);
      dcp->hash_next->hash_prev = dcp->hash_prev;
    }
    
    stole = 1;
    stolen_chunk = dcp->f.chunk;
    stolen_fid_unique = dcp->f.fid.unique;
  }
  
  memcpy(&dcp->f.fid, fid, sizeof(cm_fid_t));
  dcp->f.chunk = chunk;
  dcp->f.dataVersion = dataVersion;
  dcp->f.accessOrd = accessOrd++;
  dcp->f.states = DISK_CACHE_USED;
  
  /* allocate at head of new hash chain */
  index = DCHash(fid, chunk);
  /*osi_QAddH(&diskHashTable[index], &dcp->hashq);*/
  dcp->hash_next = diskHashTable[index];
  dcp->hash_prev = NULL;
  if (diskHashTable[index]) diskHashTable[index]->hash_prev = dcp;
  diskHashTable[index] = dcp;

  /* put at tail of queue */
  QAddT(&diskLRUList, &dcp->lruq, dcp->f.accessOrd);
  
  if (stole)
    DPRINTF("diskcache_Alloc: fid/chunk=%08x/%04d (recyc fid/chunk=%08x/%04d) "
           "slot=%d hash=%d dcp=%x\n",
           fid->unique, chunk, stolen_fid_unique, stolen_chunk,
           dcp->f.index, index, dcp);
  else
    DPRINTF("diskcache_Alloc: fid/chunk=%08x/%04d slot=%d hash=%d dcp=%x\n",
           fid->unique, chunk, dcp->f.index, index, dcp);
  return dcp;
}

/* Write this chunk to its disk file */
int diskcache_Write(cm_diskcache_t *dcp, /*int bufferNum,*/ char *buf, int size)
{
   char cacheFileName[256];
   int fd;
   int rc;
   int opened = 0;
   QLink *q;
   
   /*return 0;*/
   
   DPRINTF("diskcache_Write\n");
   
   /* write bytes of buf into chunk file */
   GEN_CACHE_FILE_NAME(cacheFileName, cm_cachePath, dcp->f.index);
   if (dcp->openfd == 0)
   {
     dcp->openfd = open(cacheFileName, O_RDWR | O_BINARY);
     if (dcp->openfd < 0)
     {
       dcp->openfd = 0;
       complain("diskcache_Write: Couldn't open cache file %s\n", cacheFileName);
       return -1;
     }
     opened = 1;
   }

   /*lseek(dcp->openfd, bufferNum * buf_blockSize, SEEK_SET);*/
   /* only write size bytes */
   rc = write(dcp->openfd, buf, size);
   if (rc < 0)
   {
      complain("diskcache_Write: Couldn't write cache file %s\n", cacheFileName);
      close(dcp->openfd); dcp->openfd = 0; return rc;
   }

   if (opened)
   {
     /* add to open file list */
     QAddT(&openFileList, &dcp->openq, 0);
     openCacheFiles++;
   }
   else
     QMoveToTail(&openFileList, &dcp->openq, 0);

   if (openCacheFiles >= MAX_OPEN_FILES)
   {
     /* close longest-open file */
     q = QServe(&openFileList);
     dcp = (cm_diskcache_t *) MEM_TO_OBJ(cm_diskcache_t, openq, q);
     assert(dcp != NULL);
     if (dcp->openfd > 0)
       close(dcp->openfd);
     dcp->openfd = 0;
     openCacheFiles--;
   }
     
   return 0;
}

/* we accessed this chunk (hit on buffer read), so move to MRU */
void diskcache_Touch(cm_diskcache_t *dcp)
{
  if (!cm_diskCacheEnabled || !dcp) return;
  dcp->f.accessOrd = accessOrd++;
  QMoveToTail(&diskLRUList, &dcp->lruq, dcp->f.accessOrd);  /* tail is MRU */
}

/* invalidate this disk cache entry */
int diskcache_Invalidate(cm_diskcache_t *dcp)
{
  /* We consider an invalid chunk as empty, so we put it at tail of LRU */
  QRemove(&diskLRUList, &dcp->lruq);

  dcp->f.accessOrd = 0;
  dcp->f.states = DISK_CACHE_EMPTY;
  dcp->f.chunk = 0;
  memset(&dcp->f.fid, sizeof(cm_fid_t));
  /*osi_QAdd(diskLRUList, &dcp->lruq);*/
  QAddH(&diskLRUList, &dcp->lruq, dcp->f.accessOrd);     /* head is LRU */
}

void diskcache_WriteCacheInfo(cm_diskcache_t *dcp)
{
  /*char cacheInfoPath[256];
    int cacheInfo_fd;*/
  int rc;
  
  /*return;   /* skip this for perf. testing */
  /*sprintf(cacheInfoPath, "%s\\%s", cm_cachePath, CACHE_INFO_FILE);
    cacheInfo_fd = open(cacheInfoPath, O_RDWR);*/

  DPRINTF("diskcache_WriteCacheInfo\n");

  lseek(cacheInfo_fd, dcp->f.index * sizeof(struct fcache) +
        sizeof(cm_cacheInfoHdr_t), SEEK_SET);

  rc = write(cacheInfo_fd, &dcp->f, sizeof(struct fcache));
  if (rc < 0)
    complain("diskcache_WriteCacheInfo: Couldn't write cache info file, error=%d\n", errno);
  /*fsync(cacheInfo_fd);*/

  /*close(cacheInfo_fd);*/
}

void diskcache_Shutdown()
{
  cm_diskcache_t *dcp;
  QLink *q;

  /* close cache info file */
  close (cacheInfo_fd);
  
  /* close all open cache files */
  q = QServe(&openFileList);
  while (q)
  {
    dcp = (cm_diskcache_t *) MEM_TO_OBJ(cm_diskcache_t, openq, q);
    if (dcp->openfd)
      close(dcp->openfd);
    q = QServe(&openFileList);
  }
}

#endif /* DISKCACHE95 */
