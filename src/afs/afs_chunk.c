/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/afs_chunk.c,v 1.6 2003/07/15 23:14:11 shadow Exp $");

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"

/*
 * Chunk module.
 */

afs_int32 afs_FirstCSize = AFS_DEFAULTCSIZE;
afs_int32 afs_OtherCSize = AFS_DEFAULTCSIZE;
afs_int32 afs_LogChunk = AFS_DEFAULTLSIZE;

#ifdef notdef
int
afs_ChunkOffset(offset)
     afs_int32 offset;
{

    AFS_STATCNT(afs_ChunkOffset);
    if (offset < afs_FirstCSize)
	return offset;
    else
	return ((offset - afs_FirstCSize) & (afs_OtherCSize - 1));
}


int
afs_Chunk(offset)
     afs_int32 offset;
{
    AFS_STATCNT(afs_Chunk);
    if (offset < afs_FirstCSize)
	return 0;
    else
	return (((offset - afs_FirstCSize) >> afs_LogChunk) + 1);
}


int
afs_ChunkBase(offset)
     int offset;
{
    AFS_STATCNT(afs_ChunkBase);
    if (offset < afs_FirstCSize)
	return 0;
    else
	return (((offset - afs_FirstCSize) & ~(afs_OtherCSize - 1)) +
		afs_FirstCSize);
}


int
afs_ChunkSize(offset)
     afs_int32 offset;
{
    AFS_STATCNT(afs_ChunkSize);
    if (offset < afs_FirstCSize)
	return afs_FirstCSize;
    else
	return afs_OtherCSize;
}


int
afs_ChunkToBase(chunk)
     afs_int32 chunk;
{
    AFS_STATCNT(afs_ChunkToBase);
    if (chunk == 0)
	return 0;
    else
	return (afs_FirstCSize + ((chunk - 1) << afs_LogChunk));
}


int
afs_ChunkToSize(chunk)
     afs_int32 chunk;
{
    AFS_STATCNT(afs_ChunkToSize);
    if (chunk == 0)
	return afs_FirstCSize;
    else
	return afs_OtherCSize;
}

/* sizes are a power of two */
int
afs_SetChunkSize(chunk)
     afs_int32 chunk;
{
    AFS_STATCNT(afs_SetChunkSize);
    afs_LogChunk = chunk;
    afs_FirstCSize = afs_OtherCSize = (1 << chunk);
}

#endif /* notdef */
