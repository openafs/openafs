/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef CM_DISKCACHE_H
#define CM_DISKCACHE_H

#include "queue95.h"

#define	DCHash(v, c)	((((v)->vnode + (v)->volume + (c))) & (afs_dhashsize-1))

#define CACHE_INFO_FILE "cacheInfo"

#define CACHE_INFO_MAGIC 0x34564321
#define CACHE_FILE_MAGIC 0x78931230
#define CACHE_FILES_PER_DIR 1000
#define CACHE_INFO_UPDATES_PER_WRITE 1

#define DPRINTF if (0) printf

/* kept on disk and in dcache entries */
struct fcache {
    cm_fid_t fid;	/* Fid for this file */
    int32 modTime;		/* last time this entry was modified */
  /*afs_hyper_t versionNo;	/* Associated data version number */
  int dataVersion;
    int chunk;		/* Relative chunk number */
    int chunkBytes;		/* Num bytes in this chunk */
    char states;		/* Has this chunk been modified? */
    int accessOrd;      /* change to 64 bit later */
#define DISK_CACHE_EMPTY 0
#define DISK_CACHE_USED 1
  int index;                /* absolute chunk number */
  int checksum;
  /*char pad[464];   /* pad up to 512 bytes */
};

/* in-memory chunk file control block */
typedef struct cm_diskcache {
  struct fcache f;

  /*osi_queue_t lruq;
    osi_queue_t hashq;*/
  QLink lruq;
  QLink openq;
  int openfd;      /* open file descriptor */
  struct cm_diskcache *hash_next;
  struct cm_diskcache *hash_prev;
  unsigned long refCount;
  osi_mutex_t mx;
} cm_diskcache_t;

typedef struct cm_cacheInfoHdr {
  int magic;
  int chunks;     /* total chunks in cache */
  int chunkSize;
  char pad[500];  /* pad up to 512 bytes */
} cm_cacheInfoHdr_t;

typedef struct cm_cacheFileHdr {
  int magic;
  int index;
} cm_cacheFileHdr_t;

/* external functions */

/* Initialize the disk cache */
int diskcache_Init();
/* Get chunk from the cache or allocate a new chunk */
int diskcache_Get(cm_fid_t *fid, osi_hyper_t *offset, char *buf, int size, int *dataVersion, int *dataCount, cm_diskcache_t **dcpRet);
/* Write out buffer to disk */
int diskcache_Update(cm_diskcache_t *dcp, char *buf, int size, int dataVersion);
/* we accessed this chunk, so move to MRU */
void diskcache_Touch(cm_diskcache_t *dcp);

#endif /* CM_DISKCACHE_H */
