/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Dynamic /afs volume support.
 *
 * Implements:
 * afs_IsDynrootFid
 * afs_GetDynrootFid
 * afs_IsDynroot
 * afs_RefreshDynroot
 * afs_GetDynroot
 * afs_PutDynroot
 * afs_DynrootNewVnode
 * afs_SetDynrootEnable
 * afs_GetDynrootEnable
 *
 */

#include <afsconfig.h>
#include "../afs/param.h"

#include "../afs/stds.h"
#include "../afs/sysincludes.h" /* Standard vendor system headers */
#include "../afs/afsincludes.h"
#include "../afs/afs_osi.h"
#include "../afsint/afsint.h"
#include "../afs/lock.h"

#include "../afs/prs_fs.h"
#include "../afs/dir.h"

#define AFS_DYNROOT_CELL	1
#define AFS_DYNROOT_VOLUME	1
#define AFS_DYNROOT_VNODE	1
#define AFS_DYNROOT_UNIQUE	1

#define VNUM2CIDX(vnum)		((vnum) >> 2)
#define VNUM2RW(vnum)		(((vnum) >> 1) & 1)
#define CIDXRW2VNUM(cidx, rw)	(((cidx) << 2) | ((rw) << 1))

static int afs_dynrootEnable = 0;

static afs_rwlock_t afs_dynrootDirLock;
/* Start of variables protected by afs_dynrootDirLock */
static char *afs_dynrootDir = NULL;
static int afs_dynrootDirLen;
static int afs_dynrootDirLinkcnt;
static int afs_dynrootCellCount;
static int afs_dynrootVersion = 1;
static int afs_dynrootVersionHigh = 1;
/* End of variables protected by afs_dynrootDirLock */

extern afs_int32 afs_cellindex;
extern afs_rwlock_t afs_xvcache;

/*
 * Returns non-zero iff fid corresponds to the top of the dynroot volume.
 */
int
afs_IsDynrootFid(struct VenusFid *fid)
{
    return
	(afs_dynrootEnable &&
	 fid->Cell       == AFS_DYNROOT_CELL   &&
	 fid->Fid.Volume == AFS_DYNROOT_VOLUME &&
	 fid->Fid.Vnode  == AFS_DYNROOT_VNODE  &&
	 fid->Fid.Unique == AFS_DYNROOT_UNIQUE);
}

/*
 * Obtain the magic dynroot volume Fid.
 */
void
afs_GetDynrootFid(struct VenusFid *fid) 
{
    fid->Cell       = AFS_DYNROOT_CELL;
    fid->Fid.Volume = AFS_DYNROOT_VOLUME;
    fid->Fid.Vnode  = AFS_DYNROOT_VNODE;
    fid->Fid.Unique = AFS_DYNROOT_UNIQUE;
}

/*
 * Returns non-zero iff avc is a pointer to the dynroot /afs vnode.
 */
int
afs_IsDynroot(avc)
    struct vcache *avc;
{
    return afs_IsDynrootFid(&avc->fid);
}

/*
 * Add directory entry by given name to a directory.  Assumes the
 * caller has allocated the directory to be large enough to hold
 * the necessary entry.
 */
static void
afs_dynroot_addDirEnt(dirHeader, curPageP, curChunkP, name, vnode)
    struct DirHeader *dirHeader;
    int *curPageP;
    int *curChunkP;
    char *name;
    int vnode;
{
    char *dirBase = (char *) dirHeader;
    struct PageHeader *pageHeader;
    struct DirEntry *dirEntry;
    int sizeOfEntry, i, t1, t2;
    int curPage = *curPageP;
    int curChunk = *curChunkP;
    int didNewPage = 0;

    /*
     * Check if we need to flip pages..  If so, init the new page.
     */
    sizeOfEntry = afs_dir_NameBlobs(name);
    if (curChunk + sizeOfEntry > EPP) {
	curPage++;
	curChunk = 1;
	didNewPage = 1;
    }

    pageHeader = (struct PageHeader *) (dirBase + curPage * AFS_PAGESIZE);
    if (didNewPage) {
	pageHeader->pgcount = 0;
	pageHeader->tag = htons(1234);
	pageHeader->freecount = 0;
	pageHeader->freebitmap[0] = 0x01;
	for (i = 1; i < EPP/8; i++)
	    pageHeader->freebitmap[i] = 0;

	dirHeader->alloMap[curPage] = EPP - 1;
    }

    dirEntry = (struct DirEntry *) (pageHeader + curChunk);
    dirEntry->flag        = 1;
    dirEntry->length      = 0;
    dirEntry->next        = 0;
    dirEntry->fid.vnode   = htonl(vnode);
    dirEntry->fid.vunique = htonl(1);
    strcpy(dirEntry->name, name);

    for (i = curChunk; i < curChunk + sizeOfEntry; i++) {
	t1 = i / 8;
	t2 = i % 8;
	pageHeader->freebitmap[t1] |= (1 << t2);
    }

    /*
     * Add the new entry to the correct hash chain.
     */
    i = DirHash(name);
    dirEntry->next = dirHeader->hashTable[i];
    dirHeader->hashTable[i] = htons(curPage * EPP + curChunk);

    curChunk += sizeOfEntry;
    dirHeader->alloMap[curPage] -= sizeOfEntry;

    *curPageP = curPage;
    *curChunkP = curChunk;
}

/*
 * Regenerates the dynroot contents from the current list of
 * cells.  Useful when the list of cells has changed due to
 * an AFSDB lookup, for instance.
 */
void
afs_RefreshDynroot()
{
    int cellidx, maxcellidx, i;
    struct cell *c;
    int curChunk, curPage;
    int dirSize;
    char *newDir, *dotCell;
    struct DirHeader *dirHeader;
    struct PageHeader *pageHeader;
    struct DirEntry *dirEntry;
    int doFlush = 0;
    int linkCount = 0;

    /*
     * Save afs_cellindex here, in case it changes between the
     * two loops.
     */
    maxcellidx = afs_cellindex;

    /*
     * Compute the amount of space we need for the fake dir
     */
    curChunk = 13;
    curPage = 0;

    for (cellidx = 0; cellidx < maxcellidx; cellidx++) {
	int sizeOfCurEntry;

	c = afs_GetCellByIndex(cellidx, READ_LOCK);
	if (!c) continue;

	sizeOfCurEntry = afs_dir_NameBlobs(c->cellName);
	if (curChunk + sizeOfCurEntry > EPP) {
	    curPage++;
	    curChunk = 1;
	}
	curChunk += sizeOfCurEntry;

	dotCell = afs_osi_Alloc(strlen(c->cellName) + 2);
	strcpy(dotCell, ".");
	strcat(dotCell, c->cellName);
	sizeOfCurEntry = afs_dir_NameBlobs(dotCell);
	if (curChunk + sizeOfCurEntry > EPP) {
	    curPage++;
	    curChunk = 1;
	}
	curChunk += sizeOfCurEntry;

	afs_PutCell(c, READ_LOCK);
    }

    dirSize = (curPage + 1) * AFS_PAGESIZE;
    newDir = afs_osi_Alloc(dirSize);

    /*
     * Now actually construct the directory.
     */
    curChunk = 13;
    curPage = 0;
    dirHeader = (struct DirHeader *) newDir;

    dirHeader->header.pgcount = 0;
    dirHeader->header.tag = htons(1234);
    dirHeader->header.freecount = 0;

    dirHeader->header.freebitmap[0] = 0xff;
    dirHeader->header.freebitmap[1] = 0x1f;
    for (i = 2; i < EPP/8; i++)
	dirHeader->header.freebitmap[i] = 0;
    dirHeader->alloMap[0] = EPP - DHE - 1;
    for (i = 1; i < MAXPAGES; i++)
	dirHeader->alloMap[i] = EPP;
    for (i = 0; i < NHASHENT; i++)
	dirHeader->hashTable[i] = 0;

    /* Install "." and ".." */
    afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, ".", 1);
    afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, "..", 1);
    linkCount += 2;

    for (cellidx = 0; cellidx < maxcellidx; cellidx++) {
	c = afs_GetCellByIndex(cellidx, READ_LOCK);
	afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk,
			      c->cellName, CIDXRW2VNUM(cellidx, 0));

	dotCell = afs_osi_Alloc(strlen(c->cellName) + 2);
	strcpy(dotCell, ".");
	strcat(dotCell, c->cellName);
	afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk,
			      dotCell, CIDXRW2VNUM(cellidx, 1));

	linkCount += 2;

	afs_PutCell(c, READ_LOCK);
    }

    ObtainWriteLock(&afs_dynrootDirLock, 549);
    if (afs_dynrootDir) afs_osi_Free(afs_dynrootDir, afs_dynrootDirLen);
    afs_dynrootDir = newDir;
    afs_dynrootDirLen = dirSize;
    afs_dynrootDirLinkcnt = linkCount;
    if (afs_dynrootCellCount != maxcellidx) {
	/*
	 * New cells added -- bump data version, invalidate vcache.
	 */
	afs_dynrootCellCount = maxcellidx;
	afs_dynrootVersion++;
	afs_dynrootVersionHigh = osi_Time();
	doFlush = 1;
    }
    ReleaseWriteLock(&afs_dynrootDirLock);

    if (doFlush) {
	afs_int32 retry;
	struct vcache *tvc;
	struct VenusFid tfid;

	afs_GetDynrootFid(&tfid);
	do {
	    retry = 0;
	    ObtainReadLock(&afs_xvcache);
	    tvc = afs_FindVCache(&tfid, 0, 0, &retry, 0);
	    ReleaseReadLock(&afs_xvcache);
	} while (retry);
	if (tvc) {
	    tvc->states &= ~(CStatd | CUnique);
	    osi_dnlc_purgedp(tvc);
	    afs_PutVCache(tvc);
	}
    }
}

/*
 * Returns a pointer to the base of the dynroot directory in memory,
 * length thereof, and a FetchStatus.
 */
void
afs_GetDynroot(dynrootDir, dynrootLen, status)
    char **dynrootDir;
    int *dynrootLen;
    struct AFSFetchStatus *status;
{
    ObtainReadLock(&afs_dynrootDirLock);
    if (!afs_dynrootDir) {
	ReleaseReadLock(&afs_dynrootDirLock);
	afs_RefreshDynroot();
	ObtainReadLock(&afs_dynrootDirLock);
    }

    if (dynrootDir) *dynrootDir = afs_dynrootDir;
    if (dynrootLen) *dynrootLen = afs_dynrootDirLen;

    if (status) {
	memset(status, 0, sizeof(struct AFSFetchStatus));
	status->FileType        = Directory;
	status->LinkCount       = afs_dynrootDirLinkcnt;
	status->Length          = afs_dynrootDirLen;
	status->DataVersion     = afs_dynrootVersion;
	status->CallerAccess    = PRSFS_LOOKUP | PRSFS_READ;
	status->AnonymousAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->UnixModeBits    = 0755;
	status->ParentVnode     = 1;
	status->ParentUnique    = 1;
	status->dataVersionHigh = afs_dynrootVersionHigh;
    }
}

/*
 * Puts back the dynroot read lock.
 */
void
afs_PutDynroot()
{
    ReleaseReadLock(&afs_dynrootDirLock);
}

/*
 * Inform dynroot that a new vnode is being created.  Return value
 * is non-zero if this vnode is handled by dynroot, in which case
 * FetchStatus will be filled in.
 */
int
afs_DynrootNewVnode(avc, status)
    struct vcache *avc;
    struct AFSFetchStatus *status;
{
    if (!afs_dynrootEnable) return 0;

    if (afs_IsDynroot(avc)) {
	afs_GetDynroot(0, 0, status);
	afs_PutDynroot();
	return 1;
    }

    /*
     * Check if this is an entry under /afs, e.g. /afs/cellname.
     */
    if (avc->fid.Cell       == AFS_DYNROOT_CELL &&
	avc->fid.Fid.Volume == AFS_DYNROOT_VOLUME) {

	struct cell *c;
	int namelen, linklen, cellidx, rw;

	cellidx = VNUM2CIDX(avc->fid.Fid.Vnode);
	rw = VNUM2RW(avc->fid.Fid.Vnode);

	c = afs_GetCellByIndex(cellidx, READ_LOCK);
	if (!c) {
	    afs_warn("dynroot vnode inconsistency, can't find cell %d\n",
		     cellidx);
	    return 0;
	}

	memset(status, 0, sizeof(struct AFSFetchStatus));

	if (c->states & CAlias) {
	    /*
	     * linkData needs to contain the name of the cell
	     * we're aliasing for.
	     */
	    struct cell *tca = c->alias;

	    if (!tca) {
		afs_warn("dynroot: alias %s missing cell alias pointer\n",
			 c->cellName);
		linklen = 7;
		avc->linkData = afs_osi_Alloc(linklen + 1);
		strcpy(avc->linkData, "unknown");
	    } else {
		int namelen = strlen(tca->cellName);
		linklen = rw + namelen;
		avc->linkData = afs_osi_Alloc(linklen + 1);
		strcpy(avc->linkData, rw ? "." : "");
		strcat(avc->linkData, tca->cellName);
	    }

	    status->UnixModeBits = 0755;
	} else {
	    /*
	     * linkData needs to contain "#cell:root.cell" or "%cell:root.cell"
	     */
	    namelen = strlen(c->cellName);
	    linklen = 1 + namelen + 10;
	    avc->linkData = afs_osi_Alloc(linklen + 1);
	    strcpy(avc->linkData, rw ? "%" : "#");
	    strcat(avc->linkData, c->cellName);
	    strcat(avc->linkData, ":root.cell");

	    status->UnixModeBits = 0644;
	}

	status->FileType        = SymbolicLink;
	status->LinkCount       = 1;
	status->Length          = linklen;
	status->DataVersion     = 1;
	status->CallerAccess    = PRSFS_LOOKUP | PRSFS_READ;
	status->AnonymousAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->ParentVnode     = 1;
	status->ParentUnique    = 1;

	afs_PutCell(c, READ_LOCK);
	return 1;
    }

    return 0;
}

/*
 * Enable or disable dynroot.  Returns 0 if successful.
 */
int
afs_SetDynrootEnable(enable)
    int enable;
{
    afs_dynrootEnable = enable;
    return 0;
}

/*
 * Check if dynroot support is enabled.
 */
int
afs_GetDynrootEnable()
{
    return afs_dynrootEnable;
}
