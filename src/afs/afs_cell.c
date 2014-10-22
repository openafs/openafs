/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "afs/afs_osi.h"
#include "afs/afs_md5.h"

/* Local variables. */
afs_rwlock_t afs_xcell;		/* Export for cmdebug peeking at locks */

/*
 * AFSDB implementation:
 *
 * afs_StopAFSDB: terminate the AFSDB handler, used on shutdown
 * afs_AFSDBHandler: entry point for user-space AFSDB request handler
 * afs_GetCellHostsAFSDB: query the AFSDB handler and wait for response
 * afs_LookupAFSDB: look up AFSDB for given cell name and create locally
 */

afs_rwlock_t afsdb_client_lock;	/* Serializes client requests */
afs_rwlock_t afsdb_req_lock;	/* Serializes client requests */
static char afsdb_handler_running;	/* Protected by GLOCK */
static char afsdb_handler_shutdown;	/* Protected by GLOCK */

/* from cellconfig.h */
#define MAXCELLCHARS    64
static struct {
    /* lock moved to afsdb_req_lock for cmdebug */
    char pending;
    char complete;
    char *cellname;
} afsdb_req;

/*!
 * Terminate the AFSDB handler, used on shutdown.
 */
void
afs_StopAFSDB(void)
{
    if (afsdb_handler_running) {
	afs_osi_Wakeup(&afsdb_req);
    } else {
	afsdb_handler_shutdown = 1;
#if defined(AFS_SUN5_ENV) || defined(RXK_LISTENER_ENV) || defined(RXK_UPCALL_ENV)
	afs_termState = AFSOP_STOP_RXEVENT;
#else
	afs_termState = AFSOP_STOP_COMPLETE;
#endif
	afs_osi_Wakeup(&afs_termState);
    }
}

/*!
 * \brief Entry point for user-space AFSDB request handler.
 * Reads cell data from kerlenMsg and add new cell, or alias.
 * \param acellName Cell name. If a cell is found, it's name will be filled in here.
 * \param acellNameLen Cell name length.
 * \param kernelMsg Buffer containing data about host count, time out, and cell hosts ids.
 * \return 0 for success, < 0 for error.
 */
int
afs_AFSDBHandler(char *acellName, int acellNameLen, afs_int32 * kernelMsg)
{
    afs_int32 timeout, code;
    afs_int32 cellHosts[AFS_MAXCELLHOSTS];

    if (afsdb_handler_shutdown)
	return -2;
    afsdb_handler_running = 1;

    ObtainSharedLock(&afsdb_req_lock, 683);
    if (afsdb_req.pending) {
	int i, hostCount;

	UpgradeSToWLock(&afsdb_req_lock, 684);
	hostCount = kernelMsg[0];
	timeout = kernelMsg[1];
	if (timeout)
	    timeout += osi_Time();

	for (i = 0; i < AFS_MAXCELLHOSTS; i++) {
	    if (i >= hostCount)
		cellHosts[i] = 0;
	    else
		cellHosts[i] = kernelMsg[2 + i];
	}

	if (hostCount)
	    code = afs_NewCell(acellName, cellHosts, CNoSUID, NULL, 0, 0,
			       timeout);

	if (!hostCount || (code && code != EEXIST))
	    /* null out the cellname if the lookup failed */
	    afsdb_req.cellname = NULL;
	else
	    /* If we found an alias, create it */
	    if (afs_strcasecmp(afsdb_req.cellname, acellName))
		afs_NewCellAlias(afsdb_req.cellname, acellName);

	/* Request completed, wake up the relevant thread */
	afsdb_req.pending = 0;
	afsdb_req.complete = 1;
	afs_osi_Wakeup(&afsdb_req);
	ConvertWToSLock(&afsdb_req_lock);
    }
    ConvertSToRLock(&afsdb_req_lock);

    /* Wait for a request */
    while (afsdb_req.pending == 0 && afs_termState != AFSOP_STOP_AFSDB) {
	ReleaseReadLock(&afsdb_req_lock);
	afs_osi_Sleep(&afsdb_req);
	ObtainReadLock(&afsdb_req_lock);
    }

    /* Check if we're shutting down */
    if (afs_termState == AFSOP_STOP_AFSDB) {
	ReleaseReadLock(&afsdb_req_lock);

	/* Inform anyone waiting for us that we're going away */
	afsdb_handler_shutdown = 1;
	afsdb_handler_running = 0;
	afs_osi_Wakeup(&afsdb_req);

	afs_termState = AFSOP_STOP_RXEVENT;
	afs_osi_Wakeup(&afs_termState);
	return -2;
    }

    /* Return the lookup request to userspace */
    strncpy(acellName, afsdb_req.cellname, acellNameLen);
    ReleaseReadLock(&afsdb_req_lock);
    return 0;
}

/*!
 * \brief Query the AFSDB handler and wait for response.
 * \param  acellName
 * \return 0 for success. < 0 is error.
 */
static int
afs_GetCellHostsAFSDB(char *acellName)
{
    AFS_ASSERT_GLOCK();
    if (!afsdb_handler_running)
	return ENOENT;

    ObtainWriteLock(&afsdb_client_lock, 685);
    ObtainWriteLock(&afsdb_req_lock, 686);

    afsdb_req.cellname = acellName;

    afsdb_req.complete = 0;
    afsdb_req.pending = 1;
    afs_osi_Wakeup(&afsdb_req);
    ConvertWToRLock(&afsdb_req_lock);

    while (afsdb_handler_running && !afsdb_req.complete) {
	ReleaseReadLock(&afsdb_req_lock);
	afs_osi_Sleep(&afsdb_req);
	ObtainReadLock(&afsdb_req_lock);
    };

    ReleaseReadLock(&afsdb_req_lock);
    ReleaseWriteLock(&afsdb_client_lock);

    if (afsdb_req.cellname) {
	return 0;
    } else
	return ENOENT;
}


/*!
 * Look up AFSDB for given cell name and create locally.
 * \param acellName Cell name.
 */
void
afs_LookupAFSDB(char *acellName)
{
    int code;
    char *cellName = afs_strdup(acellName);

    code = afs_GetCellHostsAFSDB(cellName);
    afs_Trace2(afs_iclSetp, CM_TRACE_AFSDB, ICL_TYPE_STRING, cellName,
               ICL_TYPE_INT32, code);
    afs_osi_FreeStr(cellName);
}

/*
 * Cell name-to-ID mapping
 *
 * afs_cellname_new: create a new cell name, optional cell number
 * afs_cellname_lookup_id: look up a cell name
 * afs_cellname_lookup_name: look up a cell number
 * afs_cellname_ref: note that this cell name was referenced somewhere
 * afs_cellname_init: load the list of cells from given inode
 * afs_cellname_write: write in-kernel list of cells to disk
 */

struct cell_name *afs_cellname_head;	/* Export for kdump */
static afs_dcache_id_t afs_cellname_inode;
static int afs_cellname_inode_set;
static int afs_cellname_dirty;
static afs_int32 afs_cellnum_next;

/*!
 * Create a new cell name, optional cell number.
 * \param  name Name of cell.
 * \param cellnum Cellname number.
 * \return Initialized structure.
 */
static struct cell_name *
afs_cellname_new(char *name, afs_int32 cellnum)
{
    struct cell_name *cn;

    if (cellnum == 0)
	cellnum = afs_cellnum_next;

    cn = afs_osi_Alloc(sizeof(*cn));
    osi_Assert(cn != NULL);
    cn->next = afs_cellname_head;
    cn->cellnum = cellnum;
    cn->cellname = afs_strdup(name);
    cn->used = 0;
    afs_cellname_head = cn;

    if (cellnum >= afs_cellnum_next)
	afs_cellnum_next = cellnum + 1;

    return cn;
}

/*!
 * Look up a cell name by id.
 * \param cellnum
 * \return
 */
static struct cell_name *
afs_cellname_lookup_id(afs_int32 cellnum)
{
    struct cell_name *cn;

    for (cn = afs_cellname_head; cn; cn = cn->next)
	if (cn->cellnum == cellnum)
	    return cn;

    return NULL;
}

/*!
 * Look up a cell name.
 * \param name Cell name.
 * \return
 */
static struct cell_name *
afs_cellname_lookup_name(char *name)
{
    struct cell_name *cn;

    for (cn = afs_cellname_head; cn; cn = cn->next)
	if (strcmp(cn->cellname, name) == 0)
	    return cn;

    return NULL;
}

/*!
 * Note that this cell name was referenced somewhere.
 * \param  cn
 */
static void
afs_cellname_ref(struct cell_name *cn)
{
    if (!cn->used) {
	cn->used = 1;
	afs_cellname_dirty = 1;
    }
}

/*!
 * \brief Load the list of cells from given inode.
 * \param inode Source inode.
 * \param lookupcode
 * \return 0 for success. < 0 for error.
 */
int
afs_cellname_init(afs_dcache_id_t *inode, int lookupcode)
{
    struct osi_file *tfile;
    int cc, off = 0;

    ObtainWriteLock(&afs_xcell, 692);

    afs_cellnum_next = 1;
    afs_cellname_dirty = 0;

    if (cacheDiskType == AFS_FCACHE_TYPE_MEM) {
	ReleaseWriteLock(&afs_xcell);
	return 0;
    }
    if (lookupcode) {
	ReleaseWriteLock(&afs_xcell);
	return lookupcode;
    }

    tfile = osi_UFSOpen(inode);
    if (!tfile) {
	ReleaseWriteLock(&afs_xcell);
	return EIO;
    }

    afs_copy_inode(&afs_cellname_inode, inode);
    afs_cellname_inode_set = 1;

    while (1) {
	afs_int32 cellnum, clen, magic;
	char *cellname;

	cc = afs_osi_Read(tfile, off, &magic, sizeof(magic));
	if (cc != sizeof(magic))
	    break;
	if (magic != AFS_CELLINFO_MAGIC)
	    break;
	off += cc;

	cc = afs_osi_Read(tfile, off, &cellnum, sizeof(cellnum));
	if (cc != sizeof(cellnum))
	    break;
	off += cc;

	cc = afs_osi_Read(tfile, off, &clen, sizeof(clen));
	if (cc != sizeof(clen))
	    break;
	off += cc;

	cellname = afs_osi_Alloc(clen + 1);
	if (!cellname)
	    break;

	cc = afs_osi_Read(tfile, off, cellname, clen);
	if (cc != clen) {
	    afs_osi_Free(cellname, clen + 1);
	    break;
	}
	off += cc;
	cellname[clen] = '\0';

	if (afs_cellname_lookup_name(cellname)
	    || afs_cellname_lookup_id(cellnum)) {
	    afs_osi_Free(cellname, clen + 1);
	    break;
	}

	afs_cellname_new(cellname, cellnum);
	afs_osi_Free(cellname, clen + 1);
    }

    osi_UFSClose(tfile);
    ReleaseWriteLock(&afs_xcell);
    return 0;
}

/*!
 * Write in-kernel list of cells to disk.
 */
int
afs_cellname_write(void)
{
    struct osi_file *tfile;
    struct cell_name *cn;
    int off;

    if (!afs_cellname_dirty || !afs_cellname_inode_set)
	return 0;
    if (afs_initState != 300)
	return 0;

    ObtainWriteLock(&afs_xcell, 693);
    afs_cellname_dirty = 0;
    off = 0;
    tfile = osi_UFSOpen(&afs_cellname_inode);
    if (!tfile) {
	ReleaseWriteLock(&afs_xcell);
	return EIO;
    }

    for (cn = afs_cellname_head; cn; cn = cn->next) {
	afs_int32 magic, cellnum, clen;
	int cc;

	if (!cn->used)
	    continue;

	magic = AFS_CELLINFO_MAGIC;
	cc = afs_osi_Write(tfile, off, &magic, sizeof(magic));
	if (cc != sizeof(magic))
	    break;
	off += cc;

	cellnum = cn->cellnum;
	cc = afs_osi_Write(tfile, off, &cellnum, sizeof(cellnum));
	if (cc != sizeof(cellnum))
	    break;
	off += cc;

	clen = strlen(cn->cellname);
	cc = afs_osi_Write(tfile, off, &clen, sizeof(clen));
	if (cc != sizeof(clen))
	    break;
	off += cc;

	cc = afs_osi_Write(tfile, off, cn->cellname, clen);
	if (cc != clen)
	    break;
	off += clen;
    }

    osi_UFSClose(tfile);
    ReleaseWriteLock(&afs_xcell);
    return 0;
}

/*
 * Cell alias implementation
 *
 * afs_FindCellAlias: look up cell alias by alias name
 * afs_GetCellAlias: get cell alias by index (starting at 0)
 * afs_PutCellAlias: put back a cell alias returned by Find or Get
 * afs_NewCellAlias: create new cell alias entry
 */

struct cell_alias *afs_cellalias_head;	/* Export for kdump */
static afs_int32 afs_cellalias_index;
static int afs_CellOrAliasExists_nl(char *aname);	/* Forward declaration */

/*!
 * Look up cell alias by alias name.
 * \param  alias
 * \return Found struct or NULL.
 */
static struct cell_alias *
afs_FindCellAlias(char *alias)
{
    struct cell_alias *tc;

    for (tc = afs_cellalias_head; tc != NULL; tc = tc->next)
	if (!strcmp(alias, tc->alias))
	    break;
    return tc;
}

/*!
 * Get cell alias by index (starting at 0).
 * \param index Cell index.
 * \return Found struct or null.
 */
struct cell_alias *
afs_GetCellAlias(int index)
{
    struct cell_alias *tc;

    ObtainReadLock(&afs_xcell);
    for (tc = afs_cellalias_head; tc != NULL; tc = tc->next)
	if (tc->index == index)
	    break;
    ReleaseReadLock(&afs_xcell);

    return tc;
}


 /*!
  * Put back a cell alias returned by Find or Get.
  * \param a Alias.
  * \return
  */
void
afs_PutCellAlias(struct cell_alias *a)
{
    return;
}

/*!
 * Create new cell alias entry and update dynroot vnode.
 * \param alias
 * \param cell
 * \return
 */
afs_int32
afs_NewCellAlias(char *alias, char *cell)
{
    struct cell_alias *tc;

    ObtainSharedLock(&afs_xcell, 681);
    if (afs_CellOrAliasExists_nl(alias)) {
	ReleaseSharedLock(&afs_xcell);
	return EEXIST;
    }

    UpgradeSToWLock(&afs_xcell, 682);
    tc = afs_osi_Alloc(sizeof(struct cell_alias));
    osi_Assert(tc != NULL);
    tc->alias = afs_strdup(alias);
    tc->cell = afs_strdup(cell);
    tc->next = afs_cellalias_head;
    tc->index = afs_cellalias_index++;
    afs_cellalias_head = tc;
    ReleaseWriteLock(&afs_xcell);

    afs_DynrootInvalidate();
    return 0;
}

/*
 * Actual cell list implementation
 *
 * afs_UpdateCellLRU: bump given cell up to the front of the LRU queue
 * afs_RefreshCell: look up cell information in AFSDB if timeout expired
 *
 * afs_TraverseCells: execute a callback for each existing cell
 * afs_TraverseCells_nl: same as above except without locking afs_xcell
 * afs_choose_cell_by_{name,num,index}: useful traversal callbacks
 *
 * afs_FindCellByName: return a cell with a given name, if it exists
 * afs_FindCellByName_nl: same as above, without locking afs_xcell
 * afs_GetCellByName: same as FindCellByName but tries AFSDB if not found
 * afs_GetCell: return a cell with a given cell number
 * afs_GetCellStale: same as GetCell, but does not try to refresh the data
 * afs_GetCellByIndex: return a cell with a given index number (starting at 0)
 *
 * afs_GetPrimaryCell: return the primary cell, if any
 * afs_IsPrimaryCell: returns true iff the given cell is the primary cell
 * afs_IsPrimaryCellNum: returns afs_IsPrimaryCell(afs_GetCell(cellnum))
 * afs_SetPrimaryCell: set the primary cell name to the given cell name
 *
 * afs_NewCell: create or update a cell entry
 */

struct afs_q CellLRU;		/* Export for kdump */
static char *afs_thiscell = NULL;
afs_int32 afs_cellindex;	/* Export for kdump */

/*!
 * Bump given cell up to the front of the LRU queue.
 * \param c Cell to set.
 */
static void
afs_UpdateCellLRU(struct cell *c)
{
    ObtainWriteLock(&afs_xcell, 100);
    QRemove(&c->lruq);
    QAdd(&CellLRU, &c->lruq);
    ReleaseWriteLock(&afs_xcell);
}

/*!
 * Look up cell information in AFSDB if timeout expired
 * \param ac Cell to be refreshed.
 * \return
 */
static void
afs_RefreshCell(struct cell *ac)
{
    if (ac->states & CNoAFSDB)
	return;
    if (!ac->cellHosts[0] || (ac->timeout && ac->timeout <= osi_Time()))
	afs_LookupAFSDB(ac->cellName);
}

/*!
 * Execute a callback for each existing cell, without a lock on afs_xcell.
 * Iterate on CellLRU, and execute a callback for each cell until given arguments are met.
 * \see afs_TraverseCells
 * \param cb Traversal callback for each cell.
 * \param arg Callback arguments.
 * \return Found data or NULL.
 */
static void *
afs_TraverseCells_nl(void *(*cb) (struct cell *, void *), void *arg)
{
    struct afs_q *cq, *tq;
    struct cell *tc;
    void *ret = NULL;

    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq);

	/* This is assuming that a NULL return is acceptable. */
	if (cq) {
	    tq = QNext(cq);
	} else {
	    return NULL;
	}

	ret = cb(tc, arg);
	if (ret)
	    break;
    }

    return ret;
}

/*!
 * Execute a callback for each existing cell, with a lock on afs_xcell.
 * \see afs_TraverseCells_nl
 * \param cb Traversal callback for each cell.
 * \param arg
 * \return Found data or NULL.
 */
void *
afs_TraverseCells(void *(*cb) (struct cell *, void *), void *arg)
{
    void *ret;

    ObtainReadLock(&afs_xcell);
    ret = afs_TraverseCells_nl(cb, arg);
    ReleaseReadLock(&afs_xcell);

    return ret;
}

/*!
 * Useful traversal callback: Match by name.
 * \param cell
 * \param arg Cell name (compared with cell->cellName).
 * \return Returns found cell or NULL.
 */
static void *
afs_choose_cell_by_name(struct cell *cell, void *arg)
{
    if (!arg) {
	/* Safety net */
	return cell;
    } else {
	return strcmp(cell->cellName, (char *)arg) ? NULL : cell;
    }
}

/*!
 * Useful traversal callback: Match by handle.
 * \param cell
 * \param arg Cell handle (compared with cell->cellHandle).
 * \return Returns found cell or NULL.
 */
static void *
afs_choose_cell_by_handle(struct cell *cell, void *arg)
{
    if (!arg) {
	/* Safety net */
	return cell;
    } else {
	return memcmp(cell->cellHandle, (char *)arg, 16) ? NULL : cell;
    }
}

/*!
 * Useful traversal callback: Match by cell number.
 * \param cell
 * \param arg Cell number (compared with cell->cellNum).
 * \return Returns found cell or NULL.
 */
static void *
afs_choose_cell_by_num(struct cell *cell, void *arg)
{
    return (cell->cellNum == *((afs_int32 *) arg)) ? cell : NULL;
}

/*!
 * Useful traversal callback: Match by index.
 * \param cell
 * \param arg Cell index (compared with cell->cellIndex).
 * \return Returns found cell or NULL.
 */
static void *
afs_choose_cell_by_index(struct cell *cell, void *arg)
{
    return (cell->cellIndex == *((afs_int32 *) arg)) ? cell : NULL;
}

/*!
 * Return a cell with a given name, if it exists. No lock version.
 * Does not check AFSDB.
 * \param acellName Cell name.
 * \param locktype Type of lock to be used (not used).
 * \return
 */
static struct cell *
afs_FindCellByName_nl(char *acellName, afs_int32 locktype)
{
    return afs_TraverseCells_nl(&afs_choose_cell_by_name, acellName);
}

/*!
 * Return a cell with a given name, if it exists.It uses locks.
 * Does not check AFSDB.
 * \param acellName Cell name.
 * \param locktype Type of lock to be used.
 * \return
 */
static struct cell *
afs_FindCellByName(char *acellName, afs_int32 locktype)
{
    return afs_TraverseCells(&afs_choose_cell_by_name, acellName);
}

/*!
 * Same as FindCellByName but tries AFSDB if not found.
 * \param acellName Cell name.
 * \param locktype Type of lock to be used.
 * \return
 */
struct cell *
afs_GetCellByName(char *acellName, afs_int32 locktype)
{
    struct cell *tc;

    tc = afs_FindCellByName(acellName, locktype);
    if (!tc) {
	afs_LookupAFSDB(acellName);
	tc = afs_FindCellByName(acellName, locktype);
    }
    if (tc) {
	afs_cellname_ref(tc->cnamep);
	afs_UpdateCellLRU(tc);
	afs_RefreshCell(tc);
    }

    return tc;
}

/*!
 * Return a cell with a given cell number.
 * \param cellnum Cell number.
 * \param locktype Lock to be used.
 * \return
 */
struct cell *
afs_GetCell(afs_int32 cellnum, afs_int32 locktype)
{
    struct cell *tc;
    struct cell_name *cn;

    tc = afs_GetCellStale(cellnum, locktype);
    if (tc) {
	afs_RefreshCell(tc);
    } else {
	ObtainReadLock(&afs_xcell);
	cn = afs_cellname_lookup_id(cellnum);
	ReleaseReadLock(&afs_xcell);
	if (cn)
	    tc = afs_GetCellByName(cn->cellname, locktype);
    }
    return tc;
}

/*!
 * Same as GetCell, but does not try to refresh the data.
 * \param cellnum Cell number.
 * \param locktype What lock should be used.
 * \return
 */
struct cell *
afs_GetCellStale(afs_int32 cellnum, afs_int32 locktype)
{
    struct cell *tc;

    tc = afs_TraverseCells(&afs_choose_cell_by_num, &cellnum);
    if (tc) {
	afs_cellname_ref(tc->cnamep);
	afs_UpdateCellLRU(tc);
    }
    return tc;
}

/*!
 * Return a cell with a given index number (starting at 0). Update CellLRU as well.
 * \param index
 * \param locktype Type of lock used.
 * \return
 */
struct cell *
afs_GetCellByIndex(afs_int32 index, afs_int32 locktype)
{
    struct cell *tc;

    tc = afs_TraverseCells(&afs_choose_cell_by_index, &index);
    if (tc)
	afs_UpdateCellLRU(tc);
    return tc;
}

/*!
 * Return a cell with a given handle..
 * \param index
 * \param locktype Type of lock used.
 * \return
 */
struct cell *
afs_GetCellByHandle(void *handle, afs_int32 locktype)
{
    struct cell *tc;

    tc = afs_TraverseCells(&afs_choose_cell_by_handle, handle);
    if (tc)
	afs_UpdateCellLRU(tc);
    return tc;
}

/*!
 * Return primary cell, if any.
 * \param locktype Type of lock used.
 * \return
 */
struct cell *
afs_GetPrimaryCell(afs_int32 locktype)
{
    return afs_GetCellByName(afs_thiscell, locktype);
}

/*!
 * Returns true if the given cell is the primary cell.
 * \param cell
 * \return
 */
int
afs_IsPrimaryCell(struct cell *cell)
{
    /* Simple safe checking */
    if (!cell) {
	return 0;
    } else if (!afs_thiscell) {
	/* This is simply a safety net to avoid seg faults especially when
	 * using a user-space library.  afs_SetPrimaryCell() should be set
	 * prior to this call. */
	afs_SetPrimaryCell(cell->cellName);
	return 1;
    } else {
	return strcmp(cell->cellName, afs_thiscell) ? 0 : 1;
    }
}

/*!
 * Returns afs_IsPrimaryCell(afs_GetCell(cellnum)).
 * \param cellnum
 * \return
 */
int
afs_IsPrimaryCellNum(afs_int32 cellnum)
{
    struct cell *tc;
    int primary = 0;

    tc = afs_GetCellStale(cellnum, READ_LOCK);
    if (tc) {
	primary = afs_IsPrimaryCell(tc);
	afs_PutCell(tc, READ_LOCK);
    }

    return primary;
}

/*!
 * Set the primary cell name to the given cell name.
 * \param acellName Cell name.
 * \return 0 for success, < 0 for error.
 */
afs_int32
afs_SetPrimaryCell(char *acellName)
{
    ObtainWriteLock(&afs_xcell, 691);
    if (afs_thiscell)
	afs_osi_FreeStr(afs_thiscell);
    afs_thiscell = afs_strdup(acellName);
    ReleaseWriteLock(&afs_xcell);
    return 0;
}

/*!
 * Create or update a cell entry.
 * \param acellName Name of cell.
 * \param acellHosts Array of hosts that this cell has.
 * \param aflags Cell flags.
 * \param linkedcname
 * \param fsport File server port.
 * \param vlport Volume server port.
 * \param timeout Cell timeout value, 0 means static AFSDB entry.
 * \return
 */
afs_int32
afs_NewCell(char *acellName, afs_int32 * acellHosts, int aflags,
	    char *linkedcname, u_short fsport, u_short vlport, int timeout)
{
    struct cell *tc, *tcl = 0;
    afs_int32 i, newc = 0, code = 0;

    AFS_STATCNT(afs_NewCell);

    ObtainWriteLock(&afs_xcell, 103);

    tc = afs_FindCellByName_nl(acellName, READ_LOCK);
    if (tc) {
	aflags &= ~CNoSUID;
    } else {
	tc = afs_osi_Alloc(sizeof(struct cell));
	osi_Assert(tc != NULL);
	memset(tc, 0, sizeof(*tc));
	tc->cellName = afs_strdup(acellName);
	tc->fsport = AFS_FSPORT;
	tc->vlport = AFS_VLPORT;
	AFS_MD5_String(tc->cellHandle, tc->cellName, strlen(tc->cellName));
	AFS_RWLOCK_INIT(&tc->lock, "cell lock");
	newc = 1;
	aflags |= CNoSUID;
    }
    ObtainWriteLock(&tc->lock, 688);

    /* If the cell we've found has the correct name but no timeout,
     * and we're called with a non-zero timeout, bail out:  never
     * override static configuration entries with AFSDB ones.
     * One exception: if the original cell entry had no servers,
     * it must get servers from AFSDB.
     */
    if (timeout && !tc->timeout && tc->cellHosts[0]) {
	code = EEXIST;		/* This code is checked for in afs_LookupAFSDB */
	goto bad;
    }

    /* we don't want to keep pinging old vlservers which were down,
     * since they don't matter any more.  It's easier to do this than
     * to remove the server from its various hash tables. */
    for (i = 0; i < AFS_MAXCELLHOSTS; i++) {
	if (!tc->cellHosts[i])
	    break;
	tc->cellHosts[i]->flags &= ~SRVR_ISDOWN;
	tc->cellHosts[i]->flags |= SRVR_ISGONE;
    }

    if (fsport)
	tc->fsport = fsport;
    if (vlport)
	tc->vlport = vlport;

    if (aflags & CLinkedCell) {
	if (!linkedcname) {
	    code = EINVAL;
	    goto bad;
	}
	tcl = afs_FindCellByName_nl(linkedcname, READ_LOCK);
	if (!tcl) {
	    code = ENOENT;
	    goto bad;
	}
	if (tcl->lcellp) {	/* XXX Overwriting if one existed before! XXX */
	    tcl->lcellp->lcellp = (struct cell *)0;
	    tcl->lcellp->states &= ~CLinkedCell;
	}
	tc->lcellp = tcl;
	tcl->lcellp = tc;
    }
    tc->states |= aflags;
    tc->timeout = timeout;

    memset(tc->cellHosts, 0, sizeof(tc->cellHosts));
    for (i = 0; i < AFS_MAXCELLHOSTS; i++) {
	/* Get server for each host and link this cell in.*/
	struct server *ts;
	afs_uint32 temp = acellHosts[i];
	if (!temp)
	    break;
	ts = afs_GetServer(&temp, 1, 0, tc->vlport, WRITE_LOCK, NULL, 0, NULL);
	ts->cell = tc;
	ts->flags &= ~SRVR_ISGONE;
	/* Set the server as a host of the new cell. */
	tc->cellHosts[i] = ts;
	afs_PutServer(ts, WRITE_LOCK);
    }
    afs_SortServers(tc->cellHosts, AFS_MAXCELLHOSTS);	/* randomize servers */

    /* New cell: Build and add to LRU cell queue. */
    if (newc) {
	struct cell_name *cn;

	cn = afs_cellname_lookup_name(acellName);
	if (!cn)
	    cn = afs_cellname_new(acellName, 0);

	tc->cnamep = cn;
	tc->cellNum = cn->cellnum;
	tc->cellIndex = afs_cellindex++;
	afs_stats_cmperf.numCellsVisible++;
	QAdd(&CellLRU, &tc->lruq);
    }

    ReleaseWriteLock(&tc->lock);
    ReleaseWriteLock(&afs_xcell);
    afs_PutCell(tc, 0);
    if (!(aflags & CHush))
	afs_DynrootInvalidate();
    return 0;

  bad:
    ReleaseWriteLock(&tc->lock);

    if (newc) {
	/* If we're a new cell, nobody else can see us, so doing this
	 * after lock release is safe */
	afs_osi_FreeStr(tc->cellName);
	afs_osi_Free(tc, sizeof(struct cell));
    }

    ReleaseWriteLock(&afs_xcell);
    return code;
}

/*
 * Miscellaneous stuff
 *
 * afs_CellInit: perform whatever initialization is necessary
 * shutdown_cell: called on shutdown, should deallocate memory, etc
 * afs_RemoveCellEntry: remove a server from a cell's server list
 * afs_CellOrAliasExists: check if the given name exists as a cell or alias
 * afs_CellOrAliasExists_nl: same as above without locking afs_xcell
 * afs_CellNumValid: check if a cell number is valid (also set the used flag)
 */

/*!
 * Perform whatever initialization is necessary.
 */
void
afs_CellInit(void)
{
    static char CellInit_done = 0;

    if (CellInit_done)
	return;

    CellInit_done = 1;

    AFS_RWLOCK_INIT(&afs_xcell, "afs_xcell");
    AFS_RWLOCK_INIT(&afsdb_client_lock, "afsdb_client_lock");
    AFS_RWLOCK_INIT(&afsdb_req_lock, "afsdb_req_lock");
    QInit(&CellLRU);

    afs_cellindex = 0;
    afs_cellalias_index = 0;
}

/*!
 * Called on shutdown, should deallocate memory, etc.
 */
void
shutdown_cell(void)
{
    struct afs_q *cq, *tq;
    struct cell *tc;

#ifdef AFS_CACHE_VNODE_PATH
    if (cacheDiskType != AFS_FCACHE_TYPE_MEM) {
	afs_osi_FreeStr(afs_cellname_inode.ufs);
    }
#endif
    AFS_RWLOCK_INIT(&afs_xcell, "afs_xcell");

    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq);
	tq = QNext(cq);
	if (tc->cellName)
	    afs_osi_FreeStr(tc->cellName);
	afs_osi_Free(tc, sizeof(struct cell));
    }
    QInit(&CellLRU);

{
    struct cell_name *cn = afs_cellname_head;

    while (cn) {
	struct cell_name *next = cn->next;

	afs_osi_FreeStr(cn->cellname);
	afs_osi_Free(cn, sizeof(struct cell_name));
	cn = next;
    }
}
}

/*!
 * Remove a server from a cell's server list.
 * \param srvp Server to be removed.
 * \return
 */
void
afs_RemoveCellEntry(struct server *srvp)
{
    struct cell *tc;
    afs_int32 j, k;

    tc = srvp->cell;
    if (!tc)
	return;

    /* Remove the server structure from the cell list - if there */
    ObtainWriteLock(&tc->lock, 200);
    for (j = k = 0; j < AFS_MAXCELLHOSTS; j++) {
	if (!tc->cellHosts[j])
	    break;
	if (tc->cellHosts[j] != srvp) {
	    tc->cellHosts[k++] = tc->cellHosts[j];
	}
    }
    if (k == 0) {
	/* What do we do if we remove the last one? */
    }
    for (; k < AFS_MAXCELLHOSTS; k++) {
	tc->cellHosts[k] = 0;
    }
    ReleaseWriteLock(&tc->lock);
}

/*!
 * Check if the given name exists as a cell or alias. Does not lock afs_xcell.
 * \param aname
 * \return
 */
static int
afs_CellOrAliasExists_nl(char *aname)
{
    struct cell *c;
    struct cell_alias *ca;

    c = afs_FindCellByName_nl(aname, READ_LOCK);
    if (c) {
	afs_PutCell(c, READ_LOCK);
	return 1;
    }

    ca = afs_FindCellAlias(aname);
    if (ca) {
	afs_PutCellAlias(ca);
	return 1;
    }

    return 0;
}

/*!
 * Check if the given name exists as a cell or alias. Locks afs_xcell.
 * \param aname
 * \return
 */
int
afs_CellOrAliasExists(char *aname)
{
    int ret;

    ObtainReadLock(&afs_xcell);
    ret = afs_CellOrAliasExists_nl(aname);
    ReleaseReadLock(&afs_xcell);

    return ret;
}

/*!
 * Check if a cell number is valid (also set the used flag).
 * \param cellnum
 * \return 1 - true, 0 - false
 */
int
afs_CellNumValid(afs_int32 cellnum)
{
    struct cell_name *cn;

    ObtainReadLock(&afs_xcell);
    cn = afs_cellname_lookup_id(cellnum);
    ReleaseReadLock(&afs_xcell);
    if (cn) {
	cn->used = 1;
	return 1;
    } else {
	return 0;
    }
}
