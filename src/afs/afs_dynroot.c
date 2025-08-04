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
 * afs_IsDynrootMountFid
 * afs_IsDynrootAnyFid
 * afs_GetDynrootFid
 * afs_GetDynrootMountFid
 * afs_IsDynroot
 * afs_IsDynrootMount
 * afs_IsDynrootAny
 * afs_DynrootInvalidate
 * afs_GetDynroot
 * afs_PutDynroot
 * afs_DynrootNewVnode
 * afs_SetDynrootEnable
 * afs_GetDynrootEnable
 * afs_DynrootVOPRemove
 * afs_DynrootVOPSymlink
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"
#include "afs/afs_osi.h"
#include "afsint.h"
#include "afs/lock.h"

#include "afs/prs_fs.h"
#include "afs/dir.h"
#include "afs/afs_dynroot.h"

#define AFS_DYNROOT_CELLNAME	"dynroot"
#define AFS_DYNROOT_VOLUME	1
#define AFS_DYNROOT_VNODE	1
#define AFS_DYNROOT_MOUNT_VNODE 3
#define AFS_DYNROOT_UNIQUE	1

static int afs_dynrootInit = 0;
static int afs_dynrootEnable = 0;
static int afs_dynrootCell = 0;

afs_rwlock_t afs_dynrootDirLock;
/* Start of variables protected by afs_dynrootDirLock */
static char *afs_dynrootDir = NULL;
static int afs_dynrootDirLen;
static char *afs_dynrootMountDir = NULL;
static int afs_dynrootMountDirLen;
static int afs_dynrootDirLinkcnt;
static int afs_dynrootDirVersion;
static int afs_dynrootVersion = 1;
static int afs_dynrootVersionHigh = 1;
/* End of variables protected by afs_dynrootDirLock */

/* A dynamically-created symlink in a dynroot /afs */
struct afs_dynSymlink {
    struct afs_dynSymlink *next;
    int index;
    char *name;
    char *target;
};

afs_rwlock_t afs_dynSymlinkLock;
/* Start of variables protected by afs_dynSymlinkLock */
static struct afs_dynSymlink *afs_dynSymlinkBase = NULL;
static int afs_dynSymlinkIndex = 0;
/* End of variables protected by afs_dynSymlinkLock */

/*
 * Set up a cell for dynroot if it's not there yet.
 */
static int
afs_dynrootCellInit(void)
{
    if (!afs_dynrootCell) {
	afs_int32 cellHosts[AFS_MAXCELLHOSTS];
	struct cell *tc;
	int code;

	memset(cellHosts, 0, sizeof(cellHosts));
	code =
	    afs_NewCell(AFS_DYNROOT_CELLNAME, cellHosts, CNoSUID | CNoAFSDB,
			NULL, 0, 0, 0);
	if (code)
	    return code;
	tc = afs_GetCellByName(AFS_DYNROOT_CELLNAME, READ_LOCK);
	if (!tc)
	    return ENODEV;
	afs_dynrootCell = tc->cellNum;
	afs_PutCell(tc, READ_LOCK);
    }

    return 0;
}

/*!
 * Returns non-zero if the volume is the dynroot volume.
 */
int
afs_IsDynrootVolume(struct volume *v)
{
    return (afs_dynrootEnable
	    && v->cell == afs_dynrootCell
	    && v->volume == AFS_DYNROOT_VOLUME);
}

/*
 * Returns non-zero iff fid corresponds to the top of the dynroot volume.
 */
static int
_afs_IsDynrootFid(struct VenusFid *fid)
{
    return (fid->Cell == afs_dynrootCell
	    && fid->Fid.Volume == AFS_DYNROOT_VOLUME
	    && fid->Fid.Vnode == AFS_DYNROOT_VNODE
	    && fid->Fid.Unique == AFS_DYNROOT_UNIQUE);
}

int
afs_IsDynrootFid(struct VenusFid *fid)
{
    return (afs_dynrootEnable && _afs_IsDynrootFid(fid));
}

int
afs_IsDynrootMountFid(struct VenusFid *fid)
{
    return (fid->Cell == afs_dynrootCell
	    && fid->Fid.Volume == AFS_DYNROOT_VOLUME
	    && fid->Fid.Vnode == AFS_DYNROOT_MOUNT_VNODE
	    && fid->Fid.Unique == AFS_DYNROOT_UNIQUE);
}

int
afs_IsDynrootAnyFid(struct VenusFid *fid)
{
    return (fid->Cell == afs_dynrootCell
	    && fid->Fid.Volume == AFS_DYNROOT_VOLUME);
}

/*
 * Obtain the magic dynroot volume Fid.
 */
void
afs_GetDynrootFid(struct VenusFid *fid)
{
    fid->Cell = afs_dynrootCell;
    fid->Fid.Volume = AFS_DYNROOT_VOLUME;
    fid->Fid.Vnode = AFS_DYNROOT_VNODE;
    fid->Fid.Unique = AFS_DYNROOT_UNIQUE;
}

void
afs_GetDynrootMountFid(struct VenusFid *fid)
{
    fid->Cell = afs_dynrootCell;
    fid->Fid.Volume = AFS_DYNROOT_VOLUME;
    fid->Fid.Vnode = AFS_DYNROOT_MOUNT_VNODE;
    fid->Fid.Unique = AFS_DYNROOT_UNIQUE;
}

/*
 * Returns non-zero iff avc is a pointer to the dynroot /afs vnode.
 */
int
afs_IsDynroot(struct vcache *avc)
{
    return afs_IsDynrootFid(&avc->f.fid);
}

int
afs_IsDynrootMount(struct vcache *avc)
{
    return afs_IsDynrootMountFid(&avc->f.fid);
}

int
afs_IsDynrootAny(struct vcache *avc)
{
    return afs_IsDynrootAnyFid(&avc->f.fid);
}

/*
 * Given the current page and chunk pointers in a directory, adjust them
 * appropriately so that the given file name can be appended.  Used for
 * computing the size of a directory.
 */
static void
afs_dynroot_computeDirEnt(char *name, int *curPageP, int *curChunkP)
{
    int esize;

    esize = afs_dir_NameBlobs(name);
    if (*curChunkP + esize > EPP) {
	*curPageP += 1;
	*curChunkP = 1;
    }
    *curChunkP += esize;
}

/*
 * Add directory entry by given name to a directory.  Assumes the
 * caller has allocated the directory to be large enough to hold
 * the necessary entry.
 *
 * In order to make sure that memory isn't being accidentally stomped, receive
 * the size of the directory as an argument (dirSize) and use this info to
 * calculate the maximum number of bytes that can be occupied by the name
 * mentioned above.
 */
static void
afs_dynroot_addDirEnt(struct DirHeader *dirHeader, int *curPageP,
		      int *curChunkP, char *name, int vnode, size_t dirSize)
{
    char *dirBase = (char *)dirHeader;
    struct PageHeader *pageHeader;
    struct DirEntryFlex *dirEntry;
    int sizeOfEntry, i, t1, t2;
    int curPage = *curPageP;
    int curChunk = *curChunkP;
    int didNewPage = 0;
    size_t offset, limit;

    /*
     * Check if we need to flip pages..  If so, init the new page.
     */
    sizeOfEntry = afs_dir_NameBlobs(name);
    if (curChunk + sizeOfEntry > EPP) {
	curPage++;
	curChunk = 1;
	didNewPage = 1;
    }

    pageHeader = (struct PageHeader *)(dirBase + curPage * AFS_PAGESIZE);
    if (didNewPage) {
	pageHeader->pgcount = 0;
	pageHeader->tag = htons(1234);
	pageHeader->freecount = 0;
	pageHeader->freebitmap[0] = 0x01;
	for (i = 1; i < EPP / 8; i++)
	    pageHeader->freebitmap[i] = 0;

	dirHeader->alloMap[curPage] = EPP - 1;
    }

    dirEntry = (struct DirEntryFlex *)(pageHeader + curChunk);
    dirEntry->flag = 1;
    dirEntry->length = 0;
    dirEntry->next = 0;
    dirEntry->fid.vnode = htonl(vnode);
    dirEntry->fid.vunique = htonl(1);

    osi_Assert(dirEntry->name > dirBase);
    offset = dirEntry->name - dirBase;
    osi_Assert(dirSize > offset);
    limit = dirSize - offset;
    /*
     * The caller must have ensured that the directory is large enough, so
     * strlcpy() should never truncate the string.
     */
    strlcpy(dirEntry->name, name, limit);

    for (i = curChunk; i < curChunk + sizeOfEntry; i++) {
	t1 = i / 8;
	t2 = i % 8;
	pageHeader->freebitmap[t1] |= (1 << t2);
    }

    /*
     * Add the new entry to the correct hash chain.
     */
    i = afs_dir_DirHash(name);
    dirEntry->next = dirHeader->hashTable[i];
    dirHeader->hashTable[i] = htons(curPage * EPP + curChunk);

    curChunk += sizeOfEntry;
    dirHeader->alloMap[curPage] -= sizeOfEntry;

    *curPageP = curPage;
    *curChunkP = curChunk;
}

/*
 * Invalidate the /afs vnode for dynroot; called when the underlying
 * directory has changed and needs to be re-read.
 */
void
afs_DynrootInvalidate(void)
{
    struct vcache *tvc;
    struct VenusFid tfid;

    if (!afs_dynrootEnable)
	return;

    ObtainWriteLock(&afs_dynrootDirLock, 687);
    afs_dynrootVersion++;
    afs_dynrootVersionHigh = osi_Time();
    ReleaseWriteLock(&afs_dynrootDirLock);

    afs_GetDynrootFid(&tfid);
    ObtainReadLock(&afs_xvcache);
    tvc = afs_FindVCache(&tfid, 0);
    ReleaseReadLock(&afs_xvcache);
    if (tvc) {
	afs_StaleVCacheFlags(tvc, AFS_STALEVC_NOCB, CUnique);
	afs_PutVCache(tvc);
    }
}

/*
 * Regenerates the dynroot contents from the current list of
 * cells.  Useful when the list of cells has changed due to
 * an AFSDB lookup, for instance.
 */
static void
afs_RebuildDynroot(void)
{
    int cellidx, maxcellidx, i;
    int aliasidx, maxaliasidx;
    struct cell *c;
    struct cell_alias *ca;
    int curChunk, curPage;
    int dirSize, dotLen;
    char *newDir, *dotCell;
    struct DirHeader *dirHeader;
    int linkCount = 0;
    struct afs_dynSymlink *ts;
    int newVersion;

    ObtainReadLock(&afs_dynrootDirLock);
    newVersion = afs_dynrootVersion;
    ReleaseReadLock(&afs_dynrootDirLock);

    /*
     * Compute the amount of space we need for the fake dir
     */
    curChunk = 13;
    curPage = 0;

    /* Reserve space for "." and ".." */
    curChunk += 2;

    /* Reserve space for the dynamic-mount directory */
    afs_dynroot_computeDirEnt(AFS_DYNROOT_MOUNTNAME, &curPage, &curChunk);

    for (cellidx = 0;; cellidx++) {
	c = afs_GetCellByIndex(cellidx, READ_LOCK);
	if (!c)
	    break;
	if ((c->cellNum == afs_dynrootCell) || (c->states & CHush)) {
	    afs_PutCell(c, READ_LOCK);
	    continue;
	}
	dotLen = strlen(c->cellName) + 2;
	dotCell = afs_osi_Alloc(dotLen);
	osi_Assert(dotCell != NULL);
	osi_Assert(strlcpy(dotCell, ".", dotLen) < dotLen);
	osi_Assert(strlcat(dotCell, c->cellName, dotLen) < dotLen);

	afs_dynroot_computeDirEnt(c->cellName, &curPage, &curChunk);
	afs_dynroot_computeDirEnt(dotCell, &curPage, &curChunk);

	afs_osi_Free(dotCell, dotLen);
	afs_PutCell(c, READ_LOCK);
    }
    maxcellidx = cellidx;

    for (aliasidx = 0;; aliasidx++) {
	ca = afs_GetCellAlias(aliasidx);
	if (!ca)
	    break;

	dotLen = strlen(ca->alias) + 2;
	dotCell = afs_osi_Alloc(dotLen);
	osi_Assert(dotCell != NULL);
	osi_Assert(strlcpy(dotCell, ".", dotLen) < dotLen);
	osi_Assert(strlcat(dotCell, ca->alias, dotLen) < dotLen);

	afs_dynroot_computeDirEnt(ca->alias, &curPage, &curChunk);
	afs_dynroot_computeDirEnt(dotCell, &curPage, &curChunk);

	afs_osi_Free(dotCell, dotLen);
	afs_PutCellAlias(ca);
    }
    maxaliasidx = aliasidx;

    ObtainReadLock(&afs_dynSymlinkLock);
    ts = afs_dynSymlinkBase;
    while (ts) {
	afs_dynroot_computeDirEnt(ts->name, &curPage, &curChunk);
	ts = ts->next;
    }

    dirSize = (curPage + 1) * AFS_PAGESIZE;
    newDir = afs_osi_Alloc(dirSize);
    osi_Assert(newDir != NULL);

    /*
     * Now actually construct the directory.
     */
    curChunk = 13;
    curPage = 0;
    dirHeader = (struct DirHeader *)newDir;

    dirHeader->header.pgcount = 0;
    dirHeader->header.tag = htons(1234);
    dirHeader->header.freecount = 0;

    dirHeader->header.freebitmap[0] = 0xff;
    dirHeader->header.freebitmap[1] = 0x1f;
    for (i = 2; i < EPP / 8; i++)
	dirHeader->header.freebitmap[i] = 0;
    dirHeader->alloMap[0] = EPP - DHE - 1;
    for (i = 1; i < MAXPAGES; i++)
	dirHeader->alloMap[i] = EPP;
    memset(dirHeader->hashTable, 0, NHASHENT * sizeof(dirHeader->hashTable[0]));

    /* Install ".", "..", and the dynamic mount directory */
    afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, ".", 1, dirSize);
    afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, "..", 1, dirSize);
    afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk,
			  AFS_DYNROOT_MOUNTNAME, AFS_DYNROOT_MOUNT_VNODE,
			  dirSize);
    linkCount += 3;

    for (cellidx = 0; cellidx < maxcellidx; cellidx++) {
	c = afs_GetCellByIndex(cellidx, READ_LOCK);
	if (!c)
	    continue;
	if ((c->cellNum == afs_dynrootCell) || (c->states & CHush)) {
	    afs_PutCell(c, READ_LOCK);
	    continue;
	}

	dotLen = strlen(c->cellName) + 2;
	dotCell = afs_osi_Alloc(dotLen);
	osi_Assert(dotCell != NULL);
	osi_Assert(strlcpy(dotCell, ".", dotLen) < dotLen);
	osi_Assert(strlcat(dotCell, c->cellName, dotLen) < dotLen);
	afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, c->cellName,
			      VNUM_FROM_CIDX_RW(cellidx, 0), dirSize);
	afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, dotCell,
			      VNUM_FROM_CIDX_RW(cellidx, 1), dirSize);
	afs_osi_Free(dotCell, dotLen);

	linkCount += 2;
	afs_PutCell(c, READ_LOCK);
    }

    for (aliasidx = 0; aliasidx < maxaliasidx; aliasidx++) {
	ca = afs_GetCellAlias(aliasidx);
	if (!ca)
	    continue;

	dotLen = strlen(ca->alias) + 2;
	dotCell = afs_osi_Alloc(dotLen);
	osi_Assert(dotCell != NULL);
	osi_Assert(strlcpy(dotCell, ".", dotLen) < dotLen);
	osi_Assert(strlcat(dotCell, ca->alias, dotLen) < dotLen);
	afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, ca->alias,
			      VNUM_FROM_CAIDX_RW(aliasidx, 0), dirSize);
	afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, dotCell,
			      VNUM_FROM_CAIDX_RW(aliasidx, 1), dirSize);
	afs_osi_Free(dotCell, dotLen);
	afs_PutCellAlias(ca);
    }

    ts = afs_dynSymlinkBase;
    while (ts) {
	int vnum = VNUM_FROM_TYPEID(VN_TYPE_SYMLINK, ts->index);
	afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, ts->name, vnum,
			      dirSize);
	ts = ts->next;
    }

    ReleaseReadLock(&afs_dynSymlinkLock);

    ObtainWriteLock(&afs_dynrootDirLock, 549);
    if (afs_dynrootDir)
	afs_osi_Free(afs_dynrootDir, afs_dynrootDirLen);
    afs_dynrootDir = newDir;
    afs_dynrootDirLen = dirSize;
    afs_dynrootDirLinkcnt = linkCount;
    afs_dynrootDirVersion = newVersion;
    ReleaseWriteLock(&afs_dynrootDirLock);
}

static void
afs_RebuildDynrootMount(void)
{
    int i;
    int curChunk, curPage;
    char *newDir;
    struct DirHeader *dirHeader;
    size_t dirSize = AFS_PAGESIZE;

    newDir = afs_osi_Alloc(dirSize);
    osi_Assert(newDir != NULL);

    /*
     * Now actually construct the directory.
     */
    curChunk = 13;
    curPage = 0;
    dirHeader = (struct DirHeader *)newDir;

    dirHeader->header.pgcount = 0;
    dirHeader->header.tag = htons(1234);
    dirHeader->header.freecount = 0;

    dirHeader->header.freebitmap[0] = 0xff;
    dirHeader->header.freebitmap[1] = 0x1f;
    for (i = 2; i < EPP / 8; i++)
	dirHeader->header.freebitmap[i] = 0;
    dirHeader->alloMap[0] = EPP - DHE - 1;
    for (i = 1; i < MAXPAGES; i++)
	dirHeader->alloMap[i] = EPP;
    memset(dirHeader->hashTable, 0, NHASHENT * sizeof(dirHeader->hashTable[0]));

    /* Install "." and ".." */
    afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, ".", 1, dirSize);
    afs_dynroot_addDirEnt(dirHeader, &curPage, &curChunk, "..", 1, dirSize);

    ObtainWriteLock(&afs_dynrootDirLock, 549);
    if (afs_dynrootMountDir)
	afs_osi_Free(afs_dynrootMountDir, afs_dynrootMountDirLen);
    afs_dynrootMountDir = newDir;
    afs_dynrootMountDirLen = dirSize;
    ReleaseWriteLock(&afs_dynrootDirLock);
}

/*
 * Returns a pointer to the base of the dynroot directory in memory,
 * length thereof, and a FetchStatus.
 */
void
afs_GetDynroot(char **dynrootDir, int *dynrootLen,
	       struct AFSFetchStatus *status)
{
    ObtainReadLock(&afs_dynrootDirLock);
    if (!afs_dynrootDir || afs_dynrootDirVersion != afs_dynrootVersion) {
	ReleaseReadLock(&afs_dynrootDirLock);
	afs_RebuildDynroot();
	ObtainReadLock(&afs_dynrootDirLock);
    }

    if (dynrootDir)
	*dynrootDir = afs_dynrootDir;
    if (dynrootLen)
	*dynrootLen = afs_dynrootDirLen;

    if (status) {
	memset(status, 0, sizeof(struct AFSFetchStatus));
	status->FileType = Directory;
	status->LinkCount = afs_dynrootDirLinkcnt;
	status->Length = afs_dynrootDirLen;
	status->DataVersion = afs_dynrootVersion;
	status->CallerAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->AnonymousAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->UnixModeBits = 0555;
	status->ParentVnode = 1;
	status->ParentUnique = 1;
	status->dataVersionHigh = afs_dynrootVersionHigh;
    }
}

void
afs_GetDynrootMount(char **dynrootDir, int *dynrootLen,
		    struct AFSFetchStatus *status)
{
    ObtainReadLock(&afs_dynrootDirLock);
    if (!afs_dynrootMountDir) {
	ReleaseReadLock(&afs_dynrootDirLock);
	afs_RebuildDynrootMount();
	ObtainReadLock(&afs_dynrootDirLock);
    }

    if (dynrootDir)
	*dynrootDir = afs_dynrootMountDir;
    if (dynrootLen)
	*dynrootLen = afs_dynrootMountDirLen;

    if (status) {
	memset(status, 0, sizeof(struct AFSFetchStatus));
	status->FileType = Directory;
	status->LinkCount = 1;
	status->Length = afs_dynrootMountDirLen;
	status->DataVersion = 1;
	status->CallerAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->AnonymousAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->UnixModeBits = 0755;
	status->ParentVnode = 1;
	status->ParentUnique = 1;
	status->dataVersionHigh = 0;
    }
}

/*
 * Puts back the dynroot read lock.
 */
void
afs_PutDynroot(void)
{
    ReleaseReadLock(&afs_dynrootDirLock);
}

/*
 * Inform dynroot that a new vnode is being created.  Return value
 * is non-zero if this vnode is handled by dynroot, in which case
 * FetchStatus will be filled in.
 */
int
afs_DynrootNewVnode(struct vcache *avc, struct AFSFetchStatus *status)
{
    char *bp, tbuf[CVBS];

    if (_afs_IsDynrootFid(&avc->f.fid)) {
	if (!afs_dynrootEnable)
	    return 0;
	afs_GetDynroot(0, 0, status);
	afs_PutDynroot();
	goto succeed;
    }

    if (afs_IsDynrootMount(avc)) {
	afs_GetDynrootMount(0, 0, status);
	afs_PutDynroot();
	goto succeed;
    }

    /*
     * Check if this is an entry under /afs, e.g. /afs/cellname.
     */
    if (avc->f.fid.Cell == afs_dynrootCell
	&& avc->f.fid.Fid.Volume == AFS_DYNROOT_VOLUME) {

	struct cell *c;
	struct cell_alias *ca;
	int namelen, linklen, cellidx, rw;

	memset(status, 0, sizeof(struct AFSFetchStatus));

	status->FileType = SymbolicLink;
	status->LinkCount = 1;
	status->DataVersion = 1;
	status->CallerAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->AnonymousAccess = PRSFS_LOOKUP | PRSFS_READ;
	status->ParentVnode = 1;
	status->ParentUnique = 1;

	if (VNUM_TO_VNTYPE(avc->f.fid.Fid.Vnode) == VN_TYPE_SYMLINK) {
	    struct afs_dynSymlink *ts;
	    int index = VNUM_TO_VNID(avc->f.fid.Fid.Vnode);

	    ObtainReadLock(&afs_dynSymlinkLock);
	    ts = afs_dynSymlinkBase;
	    while (ts) {
		if (ts->index == index)
		    break;
		ts = ts->next;
	    }

	    if (ts) {
		linklen = strlen(ts->target);
		avc->linkData = afs_osi_Alloc(linklen + 1);
		osi_Assert(avc->linkData != NULL);
		osi_Assert(strlcpy(avc->linkData, ts->target, linklen + 1) <
				   linklen + 1);

		status->Length = linklen;
		status->UnixModeBits = 0755;
	    }
	    ReleaseReadLock(&afs_dynSymlinkLock);

	    if (ts)
		goto succeed;

	    return 0;
	}

	if (VNUM_TO_VNTYPE(avc->f.fid.Fid.Vnode) != VN_TYPE_CELL
	    && VNUM_TO_VNTYPE(avc->f.fid.Fid.Vnode) != VN_TYPE_ALIAS
	    && VNUM_TO_VNTYPE(avc->f.fid.Fid.Vnode) != VN_TYPE_MOUNT) {
	    afs_warn("dynroot vnode inconsistency, unknown VNTYPE %d\n",
		     VNUM_TO_VNTYPE(avc->f.fid.Fid.Vnode));
	    return 0;
	}

	cellidx = VNUM_TO_CIDX(avc->f.fid.Fid.Vnode);
	rw = VNUM_TO_RW(avc->f.fid.Fid.Vnode);

	if (VNUM_TO_VNTYPE(avc->f.fid.Fid.Vnode) == VN_TYPE_ALIAS) {
	    char *realName;

	    ca = afs_GetCellAlias(cellidx);
	    if (!ca) {
		afs_warn("dynroot vnode inconsistency, can't find alias %d\n",
			 cellidx);
		return 0;
	    }

	    /*
	     * linkData needs to contain the name of the cell
	     * we're aliasing for.
	     */
	    realName = ca->cell;
	    if (!realName) {
		afs_warn("dynroot: alias %s missing real cell name\n",
			 ca->alias);
		avc->linkData = afs_strdup("unknown");
		linklen = 7;
	    } else {
		int namelen = strlen(realName);
		char *prefix = rw ? "." : "";
		linklen = rw + namelen;
		avc->linkData = afs_osi_Alloc(linklen + 1);
		osi_Assert(avc->linkData != NULL);
		osi_Assert(strlcpy(avc->linkData, prefix, linklen + 1) < linklen + 1);
		osi_Assert(strlcat(avc->linkData, realName, linklen + 1) < linklen + 1);
	    }

	    status->UnixModeBits = 0755;
	    afs_PutCellAlias(ca);

	} else if (VNUM_TO_VNTYPE(avc->f.fid.Fid.Vnode) == VN_TYPE_MOUNT) {
	    c = afs_GetCellByIndex(cellidx, READ_LOCK);
	    if (!c) {
		afs_warn("dynroot vnode inconsistency, can't find cell %d\n",
			 cellidx);
		return 0;
	    }

	    /*
	     * linkData needs to contain "%cell:volumeid"
	     */
	    namelen = strlen(c->cellName);
	    bp = afs_cv2string(&tbuf[CVBS], avc->f.fid.Fid.Unique);
	    linklen = 2 + namelen + strlen(bp);
	    avc->linkData = afs_osi_Alloc(linklen + 1);
	    osi_Assert(avc->linkData != NULL);
	    osi_Assert(strlcpy(avc->linkData, "%", linklen + 1) < linklen + 1);
	    osi_Assert(strlcat(avc->linkData, c->cellName, linklen + 1) < linklen + 1);
	    osi_Assert(strlcat(avc->linkData, ":", linklen + 1) < linklen + 1);
	    osi_Assert(strlcat(avc->linkData, bp, linklen + 1) < linklen + 1);

	    status->UnixModeBits = 0644;
	    status->ParentVnode = AFS_DYNROOT_MOUNT_VNODE;
	    afs_PutCell(c, READ_LOCK);

	} else {
	    char *prefix = rw ? "%" : "#";

	    c = afs_GetCellByIndex(cellidx, READ_LOCK);
	    if (!c) {
		afs_warn("dynroot vnode inconsistency, can't find cell %d\n",
			 cellidx);
		return 0;
	    }

	    /*
	     * linkData needs to contain "#cell:root.cell" or "%cell:root.cell"
	     */
	    namelen = strlen(c->cellName);
	    linklen = 1 + namelen + 10;
	    avc->linkData = afs_osi_Alloc(linklen + 1);
	    osi_Assert(avc->linkData != NULL);
	    osi_Assert(strlcpy(avc->linkData, prefix, linklen + 1) < linklen + 1);
	    osi_Assert(strlcat(avc->linkData, c->cellName, linklen + 1) < linklen + 1);
	    osi_Assert(strlcat(avc->linkData, ":root.cell", linklen + 1) < linklen + 1);

	    status->UnixModeBits = 0644;
	    afs_PutCell(c, READ_LOCK);
	}

	status->Length = linklen;
	goto succeed;
    }

    return 0;

    /* make sure we set type correctly when we do this. used to stay VREG */
 succeed:
    switch (status->FileType) {
    case File:
	vSetType(avc, VREG);
	break;
    case Directory:
	vSetType(avc, VDIR);
	break;
    case SymbolicLink:
	if (afs_fakestat_enable && (avc->f.m.Mode & 0111) == 0)
	    vSetType(avc, VDIR);
	else
	    vSetType(avc, VLNK);
	break;
    default:
	/* shouldn't happen */
      ;
    }
    return 1;
}

/*
 * Make sure dynroot initialization has been done.
 */
int
afs_InitDynroot(void)
{
    if (afs_dynrootInit)
	return 0;
    AFS_RWLOCK_INIT(&afs_dynrootDirLock, "afs_dynrootDirLock");
    AFS_RWLOCK_INIT(&afs_dynSymlinkLock, "afs_dynSymlinkLock");
    afs_dynrootInit = 0;
    return afs_dynrootCellInit();
}

/*
 * Enable or disable dynroot.  Returns 0 if successful.
 */
int
afs_SetDynrootEnable(int enable)
{
    afs_dynrootEnable = enable;
    return afs_InitDynroot();
}

/*
 * Check if dynroot support is enabled.
 */
int
afs_GetDynrootEnable(void)
{
    return afs_dynrootEnable;
}

/*
 * Remove a temporary symlink entry from /afs.
 */
int
afs_DynrootVOPRemove(struct vcache *avc, afs_ucred_t *acred, char *aname)
{
    struct afs_dynSymlink **tpps;
    struct afs_dynSymlink *tps;
    int found = 0;

#if defined(AFS_SUN510_ENV)
    if (crgetruid(acred))
#else
    if (afs_cr_uid(acred))
#endif
	return EPERM;

    ObtainWriteLock(&afs_dynSymlinkLock, 97);
    tpps = &afs_dynSymlinkBase;
    while (*tpps) {
	tps = *tpps;
	if (afs_strcasecmp(aname, tps->name) == 0) {
	    afs_osi_Free(tps->name, strlen(tps->name) + 1);
	    afs_osi_Free(tps->target, strlen(tps->target) + 1);
	    *tpps = tps->next;
	    afs_osi_Free(tps, sizeof(*tps));
	    afs_dynSymlinkIndex++;
	    found = 1;
	    break;
	}
	tpps = &(tps->next);
    }
    ReleaseWriteLock(&afs_dynSymlinkLock);
    if (found) {
	afs_DynrootInvalidate();
	return 0;
    }

    if (afs_CellOrAliasExists(aname))
	return EROFS;
    else
	return ENOENT;
}

/*
 * Create a temporary symlink entry in /afs.
 */
int
afs_DynrootVOPSymlink(struct vcache *avc, afs_ucred_t *acred,
		      char *aname, char *atargetName)
{
    struct afs_dynSymlink *tps;
    size_t len;

    if (afs_cr_uid(acred))
	return EPERM;
    if (afs_CellOrAliasExists(aname))
	return EEXIST;

    /* Check if it's already a symlink */
    ObtainWriteLock(&afs_dynSymlinkLock, 91);
    tps = afs_dynSymlinkBase;
    while (tps) {
	if (afs_strcasecmp(aname, tps->name) == 0) {
	    ReleaseWriteLock(&afs_dynSymlinkLock);
	    return EEXIST;
	}
	tps = tps->next;
    }

    /* Doesn't already exist -- go ahead and create it */
    tps = afs_osi_Alloc(sizeof(*tps));
    osi_Assert(tps != NULL);
    tps->index = afs_dynSymlinkIndex++;
    tps->next = afs_dynSymlinkBase;
    len = strlen(aname) + 1;
    tps->name = afs_osi_Alloc(len);
    osi_Assert(tps->name != NULL);
    osi_Assert(strlcpy(tps->name, aname, len) < len);
    len = strlen(atargetName) + 1;
    tps->target = afs_osi_Alloc(len);
    osi_Assert(tps->target != NULL);
    osi_Assert(strlcpy(tps->target, atargetName, len) < len);
    afs_dynSymlinkBase = tps;
    ReleaseWriteLock(&afs_dynSymlinkLock);

    afs_DynrootInvalidate();
    return 0;
}
