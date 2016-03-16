/*
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */
#include "afs/lock.h"
#include "afs/afs_cbqueue.h"

#define dv_match(vc, fstat) 				 \
	((vc->f.m.DataVersion.low == fstat.DataVersion) && \
     	(vc->f.m.DataVersion.high == fstat.dataVersionHigh))

/*! Circular queue of dirty vcaches */
struct afs_q afs_disconDirty;

/*! Circular queue of vcaches with shadow directories */
struct afs_q afs_disconShadow;

/*! Locks both of these lists. Must be write locked for anything other than
 *  list traversal */
afs_rwlock_t afs_disconDirtyLock;

extern afs_int32 *afs_dvhashTbl;	/*Data cache hash table */
extern afs_int32 *afs_dchashTbl;	/*Data cache hash table */
extern afs_int32 *afs_dvnextTbl;	/*Dcache hash table links */
extern afs_int32 *afs_dcnextTbl;	/*Dcache hash table links */
extern struct dcache **afs_indexTable;	/*Pointers to dcache entries */

/*! Vnode number. On file creation, use the current value and increment it.
 */
afs_uint32 afs_DisconVnode = 2;

/*! Conflict policy. */
enum {
	CLIENT_WINS = 0,
	SERVER_WINS,
	LAST_CLOSER_WINS,
	ASK
};

afs_int32 afs_ConflictPolicy = SERVER_WINS;

static void afs_DisconDiscardAllShadows(int, afs_ucred_t *);
void afs_DbgListDirEntries(struct VenusFid *afid);


/*!
 * Find the first dcache of a file that has the specified fid.
 * Similar to afs_FindDCache, only that it takes a fid instead
 * of a vcache and it can get the first dcache.
 *
 * \param afid
 *
 * \return The found dcache or NULL.
 */
struct dcache *
afs_FindDCacheByFid(struct VenusFid *afid)
{
    afs_int32 i, index;
    struct dcache *tdc = NULL;

    i = DVHash(afid);
    ObtainWriteLock(&afs_xdcache, 758);
    for (index = afs_dvhashTbl[i]; index != NULLIDX;) {
	if (afs_indexUnique[index] == afid->Fid.Unique) {
	    tdc = afs_GetValidDSlot(index);
	    if (tdc) {
		ReleaseReadLock(&tdc->tlock);
		if (!FidCmp(&tdc->f.fid, afid)) {
		    break;		/* leaving refCount high for caller */
		}
		afs_PutDCache(tdc);
	    }
	}
	index = afs_dvnextTbl[index];
    }
    ReleaseWriteLock(&afs_xdcache);

    if (index == NULLIDX)
    	tdc = NULL;
    return tdc;
}

/*!
 * Generate a store status from a dirty vcache entry.
 *
 * \param avc Dirty vcache entry.
 * \param astat
 *
 * \note The vnode must be share locked. It is called only on resync,
 * where the vnode is write locked locally and and the server.
 *
 * \return Mask of operations.
 */
int
afs_GenStoreStatus(struct vcache *avc, struct AFSStoreStatus *astat)
{
    if (!avc || !astat || !avc->f.ddirty_flags)
    	return 0;

    /* Clean up store stat. */
    memset(astat, 0, sizeof(struct AFSStoreStatus));

    if (avc->f.ddirty_flags & VDisconSetTime) {
	/* Update timestamp. */
	astat->ClientModTime = avc->f.m.Date;
	astat->Mask |= AFS_SETMODTIME;
    }

    if (avc->f.ddirty_flags & VDisconSetMode) {
	/* Copy the mode bits. */
	astat->UnixModeBits = avc->f.m.Mode;
	astat->Mask |= AFS_SETMODE;
   }

   /* XXX: more to come... ?*/

   return astat->Mask;
}

/*!
 * Hook for filtering the local dir fid by searching the "." entry.
 *
 * \param hdata The fid to be filled.
 */
static int
get_parent_dir_fid_hook(void *hdata, char *aname, afs_int32 vnode,
			afs_int32 unique)
{
    struct VenusFid *tfid = (struct VenusFid *) hdata;

    if ((aname[0] == '.') && (aname[1] == '.') && !aname[2]) {
    	tfid->Fid.Vnode = vnode;
	tfid->Fid.Unique = unique;
	return 1;
    }

    return 0;
}

/*!
 * Get a the dir's fid by looking in the vcache for simple files and
 * in the ".." entry for directories.
 *
 * \param avc The file's vhash entry.
 * \param afid Put the fid here.
 *
 * \return 0 on success, -1 on failure
 */
int
afs_GetParentDirFid(struct vcache *avc, struct VenusFid *afid)
{
    struct dcache *tdc;

    afid->Cell = avc->f.fid.Cell;
    afid->Fid.Volume = avc->f.fid.Fid.Volume;

    switch (vType(avc)) {
    case VREG:
    case VLNK:
	/* Normal files have the dir fid embedded in the vcache. */
	afid->Fid.Vnode = avc->f.parent.vnode;
	afid->Fid.Unique = avc->f.parent.unique;
	break;
    case VDIR:
	/* If dir or parent dir created locally*/
	tdc = afs_FindDCacheByFid(&avc->f.fid);
    	if (tdc) {
	    afid->Fid.Unique = 0;
	    /* Lookup each entry for the fid. It should be the first. */
    	    afs_dir_EnumerateDir(tdc, &get_parent_dir_fid_hook, afid);
    	    afs_PutDCache(tdc);
	    if (afid->Fid.Unique == 0) {
		return -1;
	    }
	} else {
	    return -1;
	}
	break;
    default:
	return -1;
    }

    return 0;
}

struct NameAndFid {
    struct VenusFid *fid;
    char *name;
    int name_len;
};

/*!
 * Hook that searches a certain fid's name.
 *
 * \param hdata NameAndFid structure containin a pointer to a fid
 * and an allocate name. The name will be filled when hit.
 */
static int
get_vnode_name_hook(void *hdata, char *aname, afs_int32 vnode,
		    afs_int32 unique)
{
    struct NameAndFid *nf = (struct NameAndFid *) hdata;

    if ((nf->fid->Fid.Vnode == vnode) &&
    	(nf->fid->Fid.Unique == unique)) {
	nf->name_len = strlen(aname);
	memcpy(nf->name, aname, nf->name_len);
	nf->name[nf->name_len] = 0;

	return 1;
    }

    return 0;
}

/*!
 * Try to get a vnode's name by comparing all parent dir's entries
 * to the given fid. It can also return the dir's dcache.
 *
 * \param avc The file's vcache.
 * \param afid The parent dir's fid.
 * \param aname A preallocated string for the name.
 * \param deleted Has this file been deleted? If yes, use the shadow
 * dir for looking up the name.
 */
int
afs_GetVnodeName(struct vcache *avc, struct VenusFid *afid, char *aname,
		 int deleted)
{
    int code = 0;
    struct dcache *tdc;
    struct vcache *parent_vc;
    struct NameAndFid tnf;
    struct VenusFid parent_fid;
    struct VenusFid shadow_fid;

    /* List dir contents and get it's tdc. */
    if (deleted) {
	/* For deleted files, get the shadow dir's tdc: */

	/* Get the parent dir's vcache that contains the shadow fid. */
	parent_fid.Cell = avc->f.fid.Cell;
	parent_fid.Fid.Volume = avc->f.fid.Fid.Volume;
	if (avc->f.ddirty_flags & VDisconRename) {
	    /* For renames the old dir fid is needed. */
	    parent_fid.Fid.Vnode = avc->f.oldParent.vnode;
	    parent_fid.Fid.Unique = avc->f.oldParent.unique;
	} else {
	    parent_fid.Fid.Vnode = afid->Fid.Vnode;
	    parent_fid.Fid.Unique = afid->Fid.Unique;
	}

	/* Get the parent dir's vcache that contains the shadow fid. */
	ObtainSharedLock(&afs_xvcache, 755);
	parent_vc = afs_FindVCache(&parent_fid, 0, 1);
	ReleaseSharedLock(&afs_xvcache);
	if (!parent_vc) {
	    return ENOENT;
	}

	shadow_fid.Cell = parent_vc->f.fid.Cell;
    	shadow_fid.Fid.Volume = parent_vc->f.fid.Fid.Volume;
    	shadow_fid.Fid.Vnode = parent_vc->f.shadow.vnode;
    	shadow_fid.Fid.Unique = parent_vc->f.shadow.unique;

	afs_PutVCache(parent_vc);

	/* Get shadow dir's dcache. */
	tdc = afs_FindDCacheByFid(&shadow_fid);

    } else {

	/* For normal files, look into the current dir's entry. */
	tdc = afs_FindDCacheByFid(afid);
    }			/* if (deleted) */

    if (tdc) {
	tnf.fid = &avc->f.fid;
   	tnf.name_len = -1;
    	tnf.name = aname;
    	afs_dir_EnumerateDir(tdc, &get_vnode_name_hook, &tnf);
	afs_PutDCache(tdc);
	if (tnf.name_len == -1)
	    code = ENOENT;
    } else {
	/* printf("Directory dcache not found!\n"); */
        code = ENOENT;
    }

    return code;
}

struct DirtyChildrenCount {
    struct vcache *vc;
    afs_uint32 count;
};

/*!
 * Lookup dirty deleted vnodes in this dir.
 */
static int
chk_del_children_hook(void *hdata, char *aname, afs_int32 vnode,
		      afs_int32 unique)
{
    struct VenusFid tfid;
    struct DirtyChildrenCount *v = (struct DirtyChildrenCount *) hdata;
    struct vcache *tvc;

    if ((aname[0] == '.') && !aname[1])
    	/* Skip processing this dir again.
	 * It would result in an endless loop.
	 */
	return 0;

    if ((aname[0] == '.') && (aname[1] == '.') && !aname[2])
    	/* Don't process parent dir. */
    	return 0;

    /* Get this file's vcache. */
    tfid.Cell = v->vc->f.fid.Cell;
    tfid.Fid.Volume = v->vc->f.fid.Fid.Volume;
    tfid.Fid.Vnode = vnode;
    tfid.Fid.Unique = unique;

    ObtainSharedLock(&afs_xvcache, 757);
    tvc = afs_FindVCache(&tfid, 0, 1);
    ReleaseSharedLock(&afs_xvcache);

    /* Count unfinished dirty children. */
    if (tvc) {
	ObtainReadLock(&tvc->lock);
	if (tvc->f.ddirty_flags)
	    v->count++;
	ReleaseReadLock(&tvc->lock);

	afs_PutVCache(tvc);
    }

    return 0;
}

/*!
 * Check if entries have been deleted in a vnode's shadow
 * dir.
 *
 * \return Returns the number of dirty children.
 *
 * \note afs_DDirtyVCListLock must be write locked.
 */
int
afs_CheckDeletedChildren(struct vcache *avc)
{
    struct dcache *tdc;
    struct DirtyChildrenCount dcc;
    struct VenusFid shadow_fid;

    if (!avc->f.shadow.vnode)
    	/* Empty dir. */
    	return 0;

    shadow_fid.Cell = avc->f.fid.Cell;
    shadow_fid.Fid.Volume = avc->f.fid.Fid.Volume;
    shadow_fid.Fid.Vnode = avc->f.shadow.vnode;
    shadow_fid.Fid.Unique = avc->f.shadow.unique;

    dcc.count = 0;

    /* Get shadow dir's dcache. */
    tdc = afs_FindDCacheByFid(&shadow_fid);
    if (tdc) {
	dcc.vc = avc;
	afs_dir_EnumerateDir(tdc, &chk_del_children_hook, &dcc);
	afs_PutDCache(tdc);
    }

    return dcc.count;
}

/*!
 * Changes a file's parent fid references.
 */
static int
fix_children_fids_hook(void *hdata, char *aname, afs_int32 vnode,
		       afs_int32 unique)
{
    struct VenusFid tfid;
    struct VenusFid *afid = (struct VenusFid *) hdata;
    struct vcache *tvc;
    struct dcache *tdc = NULL;

    if ((aname[0] == '.') && !aname[1])
	return 0;

    if ((aname[0] == '.') && (aname[1] == '.') && !aname[2])
    	return 0;

    tfid.Cell = afid->Cell;
    tfid.Fid.Volume = afid->Fid.Volume;
    tfid.Fid.Vnode = vnode;
    tfid.Fid.Unique = unique;

    if (!(vnode % 2)) {
	/* vnode's parity indicates that it's a file. */

	/* Get the vcache. */
	ObtainSharedLock(&afs_xvcache, 759);
	tvc = afs_FindVCache(&tfid, 0, 1);
	ReleaseSharedLock(&afs_xvcache);

	/* Change the fields. */
	if (tvc) {
	    tvc->f.parent.vnode = afid->Fid.Vnode;
	    tvc->f.parent.unique = afid->Fid.Unique;

	    afs_PutVCache(tvc);
	}
    } else {
    	/* It's a dir. Fix this dir's .. entry to contain the new fid. */
	/* Seek the dir's dcache. */
    	tdc = afs_FindDCacheByFid(&tfid);
	if (tdc) {
	    /* Change the .. entry fid. */
	    afs_dir_ChangeFid(tdc, "..", NULL, &afid->Fid.Vnode);
	    afs_PutDCache(tdc);
	}
    }			/* if (!(vnode % 2))*/

    return 0;
}

/*!
 * Fixes the parentVnode and parentUnique fields of all
 * files (not dirs) contained in the directory pointed by
 * old_fid. This is useful on resync, when a locally created dir
 * get's a new fid and all the children references must be updated
 * to reflect the new fid.
 *
 * \note The dir's fid hasn't been changed yet, it is still referenced
 * with the old fid.
 *
 * \param old_fid The current dir's fid.
 * \param new_fid The new dir's fid.
 */
void
afs_FixChildrenFids(struct VenusFid *old_fid, struct VenusFid *new_fid)
{
    struct dcache *tdc;

    /* Get shadow dir's dcache. */
    tdc = afs_FindDCacheByFid(old_fid);
    /* Change the fids. */
    if (tdc) {
	afs_dir_EnumerateDir(tdc, &fix_children_fids_hook, new_fid);
	afs_PutDCache(tdc);
    }
}

static int
list_dir_hook(void *hdata, char *aname, afs_int32 vnode, afs_int32 unique)
{
    /* printf("list_dir_hook: %s v:%u u:%u\n", aname, vnode, unique); */
    return 0;
}

void
afs_DbgListDirEntries(struct VenusFid *afid)
{
    struct dcache *tdc;

    /* Get shadow dir's dcache. */
    tdc = afs_FindDCacheByFid(afid);
    if (tdc) {
	afs_dir_EnumerateDir(tdc, &list_dir_hook, NULL);
	afs_PutDCache(tdc);
    }
}

/*!
 * Find the parent vcache for a given child
 *
 * \param avc 	The vcache whose parent is required
 * \param afid  Fid structure in which parent's fid should be stored
 * \param aname An AFSNAMEMAX sized buffer to hold the parents name
 * \param adp	A pointer to a struct vcache* which will be set to the
 * 		parent vcache
 *
 * \return An error code. 0 indicates success, EAGAIN that the vnode should
 * 	   be deferred to later in the resync process
 */

int
afs_GetParentVCache(struct vcache *avc, int deleted, struct VenusFid *afid,
		    char *aname, struct vcache **adp)
{
    int code;

    *adp = NULL;

    if (afs_GetParentDirFid(avc, afid)) {
	/* printf("afs_GetParentVCache: Couldn't find parent dir's FID.\n"); */
	return ENOENT;
    }

    code = afs_GetVnodeName(avc, afid, aname, deleted);
    if (code) {
	/* printf("afs_GetParentVCache: Couldn't find file name\n"); */
	goto end;
    }

    ObtainSharedLock(&afs_xvcache, 766);
    *adp = afs_FindVCache(afid, 0, 1);
    ReleaseSharedLock(&afs_xvcache);
    if (!*adp) {
	/* printf("afs_GetParentVCache: Couldn't find parent dir's vcache\n"); */
	code = ENOENT;
	goto end;
    }

    if ((*adp)->f.ddirty_flags & VDisconCreate) {
	/* printf("afs_GetParentVCache: deferring until parent exists\n"); */
	code = EAGAIN;
	goto end;
    }

end:
    if (code && *adp) {
	afs_PutVCache(*adp);
	*adp = NULL;
    }
    return code;
}


/*!
 * Handles file renaming on reconnection:
 * - Get the old name from the old dir's shadow dir.
 * - Get the new name from the current dir.
 * - Old dir fid and new dir fid are collected along the way.
 * */
int
afs_ProcessOpRename(struct vcache *avc, struct vrequest *areq)
{
    struct VenusFid old_pdir_fid, new_pdir_fid;
    char *old_name = NULL, *new_name = NULL;
    struct AFSFetchStatus OutOldDirStatus, OutNewDirStatus;
    struct AFSVolSync tsync;
    struct afs_conn *tc;
    struct rx_connection *rxconn;
    afs_uint32 code = 0;
    XSTATS_DECLS;

    /* Get old dir vcache. */
    old_pdir_fid.Cell = avc->f.fid.Cell;
    old_pdir_fid.Fid.Volume = avc->f.fid.Fid.Volume;
    old_pdir_fid.Fid.Vnode = avc->f.oldParent.vnode;
    old_pdir_fid.Fid.Unique = avc->f.oldParent.unique;

    /* Get old name. */
    old_name = afs_osi_Alloc(AFSNAMEMAX);
    if (!old_name) {
	/* printf("afs_ProcessOpRename: Couldn't alloc space for old name.\n"); */
	return ENOMEM;
    }
    code = afs_GetVnodeName(avc, &old_pdir_fid, old_name, 1);
    if (code) {
	/* printf("afs_ProcessOpRename: Couldn't find old name.\n"); */
	goto done;
    }

    /* Alloc data first. */
    new_name = afs_osi_Alloc(AFSNAMEMAX);
    if (!new_name) {
	/* printf("afs_ProcessOpRename: Couldn't alloc space for new name.\n"); */
	code = ENOMEM;
	goto done;
    }

    if (avc->f.ddirty_flags & VDisconRenameSameDir) {
    	/* If we're in the same dir, don't do the lookups all over again,
	 * just copy fid and vcache from the old dir.
	 */
	memcpy(&new_pdir_fid, &old_pdir_fid, sizeof(struct VenusFid));
    } else {
	/* Get parent dir's FID.*/
    	if (afs_GetParentDirFid(avc, &new_pdir_fid)) {
	    /* printf("afs_ProcessOpRename: Couldn't find new parent dir FID.\n"); */
	    code = ENOENT;
	    goto done;
        }
    }

    /* And finally get the new name. */
    code = afs_GetVnodeName(avc, &new_pdir_fid, new_name, 0);
    if (code) {
	/* printf("afs_ProcessOpRename: Couldn't find new name.\n"); */
	goto done;
    }

    /* Send to data to server. */
    do {
    	tc = afs_Conn(&old_pdir_fid, areq, SHARED_LOCK, &rxconn);
    	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_RENAME);
	    RX_AFS_GUNLOCK();
	    code = RXAFS_Rename(rxconn,
	    	(struct AFSFid *)&old_pdir_fid.Fid,
		old_name,
		(struct AFSFid *)&new_pdir_fid.Fid,
		new_name,
		&OutOldDirStatus,
		&OutNewDirStatus,
		&tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
        } else
	    code = -1;

    } while (afs_Analyze(tc,
			 rxconn,
			 code,
			 &new_pdir_fid,
			 areq,
			 AFS_STATS_FS_RPCIDX_RENAME,
			 SHARED_LOCK,
			 NULL));

    /* if (code) printf("afs_ProcessOpRename: server code=%u\n", code); */
done:
    if (new_name)
	afs_osi_Free(new_name, AFSNAMEMAX);
    if (old_name)
	afs_osi_Free(old_name, AFSNAMEMAX);
    return code;
}

/*!
 * Handles all the reconnection details:
 * - Get all the details about the vnode: name, fid, and parent dir fid.
 * - Send data to server.
 * - Handle errors.
 * - Reorder vhash and dcaches in their hashes, using the newly acquired fid.
 */
int
afs_ProcessOpCreate(struct vcache *avc, struct vrequest *areq,
		    afs_ucred_t *acred)
{
    char *tname = NULL, *ttargetName = NULL;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutFidStatus, OutDirStatus;
    struct VenusFid pdir_fid, newFid;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    struct vcache *tdp = NULL, *tvc = NULL;
    struct dcache *tdc = NULL;
    struct afs_conn *tc;
    struct rx_connection *rxconn;
    afs_int32 hash, new_hash, index;
    afs_size_t tlen;
    int code, op = 0;
    XSTATS_DECLS;

    tname = afs_osi_Alloc(AFSNAMEMAX);
    if (!tname)
	return ENOMEM;
    memset(&InStatus, 0, sizeof(InStatus));

    code = afs_GetParentVCache(avc, 0, &pdir_fid, tname, &tdp);
    if (code)
	goto end;

    /* This data may also be in linkData, but then we have to deal with
     * the joy of terminating NULLs and . and file modes. So just get
     * it from the dcache where it won't have been fiddled with.
     */
    if (vType(avc) == VLNK) {
	afs_size_t offset;
	struct dcache *tdc;
	struct osi_file *tfile;

	tdc = afs_GetDCache(avc, 0, areq, &offset, &tlen, 0);
	if (!tdc) {
	    code = ENOENT;
	    goto end;
	}

	if (tlen > 1024) {
	    afs_PutDCache(tdc);
	    code = EFAULT;
	    goto end;
	}

	tlen++; /* space for NULL */
	ttargetName = afs_osi_Alloc(tlen);
	if (!ttargetName) {
	    afs_PutDCache(tdc);
	    return ENOMEM;
	}
	ObtainReadLock(&tdc->lock);
	tfile = afs_CFileOpen(&tdc->f.inode);
	code = afs_CFileRead(tfile, 0, ttargetName, tlen);
	ttargetName[tlen-1] = '\0';
	afs_CFileClose(tfile);
	ReleaseReadLock(&tdc->lock);
	afs_PutDCache(tdc);
    }

    /* Set status. */
    InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETGROUP;
    InStatus.ClientModTime = avc->f.m.Date;
    InStatus.Owner = avc->f.m.Owner;
    InStatus.Group = (afs_int32) afs_cr_gid(acred);
    /* Only care about protection bits. */
    InStatus.UnixModeBits = avc->f.m.Mode & 0xffff;

    do {
	tc = afs_Conn(&tdp->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
	    switch (vType(avc)) {
	    case VREG:
                /* Make file on server. */
		op = AFS_STATS_FS_RPCIDX_CREATEFILE;
		XSTATS_START_TIME(op);
                RX_AFS_GUNLOCK();
                code = RXAFS_CreateFile(tc->id,
					(struct AFSFid *)&tdp->f.fid.Fid,
					tname, &InStatus,
					(struct AFSFid *) &newFid.Fid,
					&OutFidStatus, &OutDirStatus,
					&CallBack, &tsync);
                RX_AFS_GLOCK();
                XSTATS_END_TIME;
		break;
	    case VDIR:
		/* Make dir on server. */
		op = AFS_STATS_FS_RPCIDX_MAKEDIR;
		XSTATS_START_TIME(op);
                RX_AFS_GUNLOCK();
		code = RXAFS_MakeDir(rxconn, (struct AFSFid *) &tdp->f.fid.Fid,
				     tname, &InStatus,
				     (struct AFSFid *) &newFid.Fid,
				     &OutFidStatus, &OutDirStatus,
				     &CallBack, &tsync);
                RX_AFS_GLOCK();
                XSTATS_END_TIME;
		break;
	    case VLNK:
		/* Make symlink on server. */
		op = AFS_STATS_FS_RPCIDX_SYMLINK;
		XSTATS_START_TIME(op);
		RX_AFS_GUNLOCK();
		code = RXAFS_Symlink(rxconn,
				(struct AFSFid *) &tdp->f.fid.Fid,
				tname, ttargetName, &InStatus,
				(struct AFSFid *) &newFid.Fid,
				&OutFidStatus, &OutDirStatus, &tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	        break;
	    default:
		op = AFS_STATS_FS_RPCIDX_CREATEFILE;
		code = 1;
		break;
	    }
        } else
	    code = -1;
    } while (afs_Analyze(tc, rxconn, code, &tdp->f.fid, areq, op, SHARED_LOCK, NULL));

    /* TODO: Handle errors. */
    if (code) {
	/* printf("afs_ProcessOpCreate: error while creating vnode on server, code=%d .\n", code); */
	goto end;
    }

    /* The rpc doesn't set the cell number. */
    newFid.Cell = avc->f.fid.Cell;

    /*
     * Change the fid in the dir entry.
     */

    /* Seek the dir's dcache. */
    tdc = afs_FindDCacheByFid(&tdp->f.fid);
    if (tdc) {
    	/* And now change the fid in the parent dir entry. */
    	afs_dir_ChangeFid(tdc, tname, &avc->f.fid.Fid.Vnode, &newFid.Fid.Vnode);
    	afs_PutDCache(tdc);
    }

    if (vType(avc) == VDIR) {
	/* Change fid in the dir for the "." entry. ".." has alredy been
	 * handled by afs_FixChildrenFids when processing the parent dir.
	 */
	tdc = afs_FindDCacheByFid(&avc->f.fid);
	if (tdc) {
   	    afs_dir_ChangeFid(tdc, ".", &avc->f.fid.Fid.Vnode,
			      &newFid.Fid.Vnode);

	    if (avc->f.m.LinkCount >= 2)
	        /* For non empty dirs, fix children's parentVnode and
		 * parentUnique reference.
	     	 */
	    	afs_FixChildrenFids(&avc->f.fid, &newFid);

	    afs_PutDCache(tdc);
	}
    }

    /* Recompute hash chain positions for vnode and dcaches.
     * Then change to the new FID.
     */

    /* The vcache goes first. */
    ObtainWriteLock(&afs_xvcache, 735);

    /* Old fid hash. */
    hash = VCHash(&avc->f.fid);
    /* New fid hash. */
    new_hash = VCHash(&newFid);

    /* Remove hash from old position. */
    /* XXX: not checking array element contents. It shouldn't be empty.
     * If it oopses, then something else might be wrong.
     */
    if (afs_vhashT[hash] == avc) {
        /* First in hash chain (might be the only one). */
	afs_vhashT[hash] = avc->hnext;
    } else {
        /* More elements in hash chain. */
 	for (tvc = afs_vhashT[hash]; tvc; tvc = tvc->hnext) {
	    if (tvc->hnext == avc) {
		tvc->hnext = avc->hnext;
		break;
	    }
        }
    }                           /* if (!afs_vhashT[i]->hnext) */
    QRemove(&avc->vhashq);

    /* Insert hash in new position. */
    avc->hnext = afs_vhashT[new_hash];
    afs_vhashT[new_hash] = avc;
    QAdd(&afs_vhashTV[VCHashV(&newFid)], &avc->vhashq);

    ReleaseWriteLock(&afs_xvcache);

    /* Do the same thing for all dcaches. */
    hash = DVHash(&avc->f.fid);
    ObtainWriteLock(&afs_xdcache, 743);
    for (index = afs_dvhashTbl[hash]; index != NULLIDX; index = hash) {
        hash = afs_dvnextTbl[index];
        tdc = afs_GetValidDSlot(index);
        ReleaseReadLock(&tdc->tlock);
	if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
            if (!FidCmp(&tdc->f.fid, &avc->f.fid)) {

		/* Safer but slower. */
 		afs_HashOutDCache(tdc, 0);

                /* Put dcache in new positions in the dchash and dvhash. */
 		new_hash = DCHash(&newFid, tdc->f.chunk);
 		afs_dcnextTbl[tdc->index] = afs_dchashTbl[new_hash];
 		afs_dchashTbl[new_hash] = tdc->index;

 		new_hash = DVHash(&newFid);
 		afs_dvnextTbl[tdc->index] = afs_dvhashTbl[new_hash];
 		afs_dvhashTbl[new_hash] = tdc->index;

 		afs_indexUnique[tdc->index] = newFid.Fid.Unique;
		memcpy(&tdc->f.fid, &newFid, sizeof(struct VenusFid));
           }                   /* if fid match */
	}                       /* if uniquifier match */
    	if (tdc)
	    afs_PutDCache(tdc);
    }                           /* for all dcaches in this hash bucket */
    ReleaseWriteLock(&afs_xdcache);

    /* Now we can set the new fid. */
    memcpy(&avc->f.fid, &newFid, sizeof(struct VenusFid));

end:
    if (tdp)
    	afs_PutVCache(tdp);
    afs_osi_Free(tname, AFSNAMEMAX);
    if (ttargetName)
	afs_osi_Free(ttargetName, tlen);
    return code;
}

/*!
 * Remove a vnode on the server, be it file or directory.
 * Not much to do here only get the parent dir's fid and call the
 * removal rpc.
 *
 * \param avc The deleted vcache
 * \param areq
 *
 * \note The vcache refcount should be dropped because it points to
 * a deleted vnode and has served it's purpouse, but we drop refcount
 * on shadow dir deletio (we still need it for that).
 *
 * \note avc must be write locked.
 */
int
afs_ProcessOpRemove(struct vcache *avc, struct vrequest *areq)
{
    char *tname = NULL;
    struct AFSFetchStatus OutDirStatus;
    struct VenusFid pdir_fid;
    struct AFSVolSync tsync;
    struct afs_conn *tc;
    struct rx_connection *rxconn;
    struct vcache *tdp = NULL;
    int code = 0;
    XSTATS_DECLS;

    tname = afs_osi_Alloc(AFSNAMEMAX);
    if (!tname) {
	/* printf("afs_ProcessOpRemove: Couldn't alloc space for file name\n"); */
	return ENOMEM;
    }

    code = afs_GetParentVCache(avc, 1, &pdir_fid, tname, &tdp);
    if (code)
	goto end;

    if ((vType(avc) == VDIR) && (afs_CheckDeletedChildren(avc))) {
	/* Deleted children of this dir remain unsynchronized.
	 * Defer this vcache.
	 */
    	code = EAGAIN;
	goto end;
    }

    if (vType(avc) == VREG || vType(avc) == VLNK) {
    	/* Remove file on server. */
	do {
	    tc = afs_Conn(&pdir_fid, areq, SHARED_LOCK, &rxconn);
	    if (tc) {
	    	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEFILE);
		RX_AFS_GUNLOCK();
		code = RXAFS_RemoveFile(rxconn,
				&pdir_fid.Fid,
				tname,
				&OutDirStatus,
				&tsync);

		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	    } else
		code = -1;
	} while (afs_Analyze(tc,
			     rxconn,
			     code,
			     &pdir_fid,
			     areq,
			     AFS_STATS_FS_RPCIDX_REMOVEFILE,
			     SHARED_LOCK,
			     NULL));

    } else if (vType(avc) == VDIR) {
    	/* Remove dir on server. */
	do {
	    tc = afs_Conn(&pdir_fid, areq, SHARED_LOCK, &rxconn);
	    if (tc) {
		XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEDIR);
		RX_AFS_GUNLOCK();
		code = RXAFS_RemoveDir(rxconn,
				&pdir_fid.Fid,
				tname,
				&OutDirStatus,
				&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	   } else
		code = -1;
	} while (afs_Analyze(tc,
			     rxconn,
			     code,
			     &pdir_fid,
			     areq,
			     AFS_STATS_FS_RPCIDX_REMOVEDIR,
			     SHARED_LOCK,
			     NULL));

    }				/* if (vType(avc) == VREG) */

    /* if (code) printf("afs_ProcessOpRemove: server returned code=%u\n", code); */

end:
    afs_osi_Free(tname, AFSNAMEMAX);
    return code;
}

/*!
 * Send disconnected file changes to the server.
 *
 * \note Call with vnode locked both locally and on the server.
 *
 * \param avc Vnode that gets synchronized to the server.
 * \param areq Used for obtaining a conn struct.
 *
 * \return 0 for success. On failure, other error codes.
 */
int
afs_SendChanges(struct vcache *avc, struct vrequest *areq)
{
    struct afs_conn *tc;
    struct rx_connection *rxconn;
    struct AFSStoreStatus sstat;
    struct AFSFetchStatus fstat;
    struct AFSVolSync tsync;
    int code = 0;
    int flags = 0;
    XSTATS_DECLS;

    /* Start multiplexing dirty operations from ddirty_flags field: */
    if (avc->f.ddirty_flags & VDisconSetAttrMask) {
	/* Setattr OPS: */
	/* Turn dirty vc data into a new store status... */
	if (afs_GenStoreStatus(avc, &sstat) > 0) {
	    do {
		tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
		if (tc) {
		    /* ... and send it. */
		    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STORESTATUS);
		    RX_AFS_GUNLOCK();
		    code = RXAFS_StoreStatus(rxconn,
				(struct AFSFid *) &avc->f.fid.Fid,
				&sstat,
				&fstat,
				&tsync);

		    RX_AFS_GLOCK();
		    XSTATS_END_TIME;
		} else
		    code = -1;

	} while (afs_Analyze(tc,
			     rxconn,
			     code,
			     &avc->f.fid,
			     areq,
			     AFS_STATS_FS_RPCIDX_STORESTATUS,
			     SHARED_LOCK,
			     NULL));

	}		/* if (afs_GenStoreStatus() > 0)*/
    }			/* disconnected SETATTR */

    if (code)
    	return code;

    if (avc->f.ddirty_flags &
	(VDisconTrunc
	| VDisconWriteClose
	| VDisconWriteFlush
	| VDisconWriteOsiFlush)) {

	/* Truncate OP: */
	do {
	    tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	    if (tc) {
		/* Set storing flags. XXX: A tad inefficient ... */
		if (avc->f.ddirty_flags & VDisconWriteClose)
		    flags |= AFS_LASTSTORE;
		if (avc->f.ddirty_flags & VDisconWriteOsiFlush)
		    flags |= (AFS_SYNC | AFS_LASTSTORE);
		if (avc->f.ddirty_flags & VDisconWriteFlush)
		    flags |= AFS_SYNC;

		/* Try to send store to server. */
		/* XXX: AFS_LASTSTORE for writes? Or just AFS_SYNC for all? */
		code = afs_StoreAllSegments(avc, areq, flags);
	    } else
		code = -1;

	} while (afs_Analyze(tc,
			     rxconn,
			     code,
			     &avc->f.fid,
			     areq,
			     AFS_STATS_FS_RPCIDX_STOREDATA,
			     SHARED_LOCK,
			     NULL));

    }			/* disconnected TRUNC | WRITE */

    return code;
}

/*!
 * All files that have been dirty before disconnection are going to
 * be replayed back to the server.
 *
 * \param areq Request from the user.
 * \param acred User credentials.
 *
 * \return If all files synchronized succesfully, return 0, otherwise
 * return error code
 *
 * \note For now, it's the request from the PDiscon pioctl.
 *
 */
int
afs_ResyncDisconFiles(struct vrequest *areq, afs_ucred_t *acred)
{
    struct afs_conn *tc;
    struct rx_connection *rxconn;
    struct vcache *tvc;
    struct AFSFetchStatus fstat;
    struct AFSCallBack callback;
    struct AFSVolSync tsync;
    int code = 0;
    afs_int32 start = 0;
    XSTATS_DECLS;
    /*AFS_STATCNT(afs_ResyncDisconFiles);*/

    ObtainWriteLock(&afs_disconDirtyLock, 707);

    while (!QEmpty(&afs_disconDirty)) {
	tvc = QEntry(QPrev(&afs_disconDirty), struct vcache, dirtyq);

	/* Can't lock tvc whilst holding the discon dirty lock */
	ReleaseWriteLock(&afs_disconDirtyLock);

	/* Get local write lock. */
	ObtainWriteLock(&tvc->lock, 705);

	if (tvc->f.ddirty_flags & VDisconRemove) {
	    /* Delete the file on the server and just move on
	     * to the next file. After all, it has been deleted
	     * we can't replay any other operation it.
	     */
	    code = afs_ProcessOpRemove(tvc, areq);
	    goto next_file;

	} else if (tvc->f.ddirty_flags & VDisconCreate) {
	    /* For newly created files, we don't need a server lock. */
	    code = afs_ProcessOpCreate(tvc, areq, acred);
	    if (code)
	    	goto next_file;

	    tvc->f.ddirty_flags &= ~VDisconCreate;
	    tvc->f.ddirty_flags |= VDisconCreated;
	}
#if 0
  	/* Get server write lock. */
  	do {
	    tc = afs_Conn(&tvc->f.fid, areq, SHARED_LOCK, &rxconn);
  	    if (tc) {
	    	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_SETLOCK);
  		RX_AFS_GUNLOCK();
  		code = RXAFS_SetLock(rxconn,
					(struct AFSFid *)&tvc->f.fid.Fid,
					LockWrite,
					&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	   } else
		code = -1;

	} while (afs_Analyze(tc,
			     rxconn,
			     code,
			     &tvc->f.fid,
			     areq,
			     AFS_STATS_FS_RPCIDX_SETLOCK,
			     SHARED_LOCK,
			     NULL));

	if (code)
	    goto next_file;
#endif
	if (tvc->f.ddirty_flags & VDisconRename) {
	    /* If we're renaming the file, do so now */
	    code = afs_ProcessOpRename(tvc, areq);
	    if (code)
	    	goto unlock_srv_file;
	}

	/* Issue a FetchStatus to get info about DV and callbacks. */
	do {
	    tc = afs_Conn(&tvc->f.fid, areq, SHARED_LOCK, &rxconn);
	    if (tc) {
	    	tvc->callback = tc->srvr->server;
		start = osi_Time();
		XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHSTATUS);
		RX_AFS_GUNLOCK();
		code = RXAFS_FetchStatus(rxconn,
				(struct AFSFid *)&tvc->f.fid.Fid,
				&fstat,
				&callback,
				&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	    } else
		code = -1;

	} while (afs_Analyze(tc,
			     rxconn,
			     code,
			     &tvc->f.fid,
			     areq,
			     AFS_STATS_FS_RPCIDX_FETCHSTATUS,
			     SHARED_LOCK,
			     NULL));

	if (code) {
	    goto unlock_srv_file;
	}

	if ((dv_match(tvc, fstat) && (tvc->f.m.Date == fstat.ServerModTime)) ||
	    	(afs_ConflictPolicy == CLIENT_WINS) ||
		(tvc->f.ddirty_flags & VDisconCreated)) {
	    /*
	     * Send changes to the server if there's data version match, or
	     * client wins policy has been selected or file has been created
	     * but doesn't have it's the contents on to the server yet.
	     */
	   /*
	    * XXX: Checking server attr changes by timestamp might not the
	    * most elegant solution, but it's the most viable one that we could find.
	    */
	    afs_UpdateStatus(tvc, &tvc->f.fid, areq, &fstat, &callback, start);
	    code = afs_SendChanges(tvc, areq);

	} else if (afs_ConflictPolicy == SERVER_WINS) {
	    /* DV mismatch, apply collision resolution policy. */
	    /* Discard this files chunks and remove from current dir. */
	    afs_ResetVCache(tvc, acred, 0);
	    tvc->f.truncPos = AFS_NOTRUNC;
	} else {
	    /* printf("afs_ResyncDisconFiles: no resolution policy selected.\n"); */
	}		/* if DV match or client wins policy */

unlock_srv_file:
	/* Release server write lock. */
#if 0
	do {
	    tc = afs_Conn(&tvc->f.fid, areq, SHARED_LOCK, &rxconn);
	    if (tc) {
	    	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_RELEASELOCK);
	    	RX_AFS_GUNLOCK();
		ucode = RXAFS_ReleaseLock(rxconn,
				(struct AFSFid *) &tvc->f.fid.Fid,
				&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	    } else
		ucode = -1;
	} while (afs_Analyze(tc,
			     rxconn,
			     ucode,
			     &tvc->f.fid,
			     areq,
			     AFS_STATS_FS_RPCIDX_RELEASELOCK,
			     SHARED_LOCK,
			     NULL));
#endif
next_file:
	ObtainWriteLock(&afs_disconDirtyLock, 710);
	if (code == 0) {
	    /* Replayed successfully - pull the vcache from the
	     * disconnected list */
	    tvc->f.ddirty_flags = 0;
	    QRemove(&tvc->dirtyq);
	    afs_PutVCache(tvc);
	} else {
	    if (code == EAGAIN)	{
		/* Operation was deferred. Pull it from the current place in
		 * the list, and stick it at the end again */
		QRemove(&tvc->dirtyq);
	   	QAdd(&afs_disconDirty, &tvc->dirtyq);
	    } else {
		/* Failed - keep state as is, and let the user know we died */

    		ReleaseWriteLock(&tvc->lock);
		break;
	    }
	}

	/* Release local write lock. */
	ReleaseWriteLock(&tvc->lock);
    }			/* while (tvc) */

    if (code) {
        ReleaseWriteLock(&afs_disconDirtyLock);
	return code;
    }

    /* Dispose of all of the shadow directories */
    afs_DisconDiscardAllShadows(0, acred);

    ReleaseWriteLock(&afs_disconDirtyLock);
    return code;
}

/*!
 * Discard all of our shadow directory copies. If squash is true, then
 * we also invalidate the vcache holding the shadow directory, to ensure
 * that any disconnected changes are deleted
 *
 * \param squash
 * \param acred
 *
 * \note afs_disconDirtyLock must be held on entry. It will be released
 * and reobtained
 */

static void
afs_DisconDiscardAllShadows(int squash, afs_ucred_t *acred)
{
   struct vcache *tvc;

   while (!QEmpty(&afs_disconShadow)) {
	tvc = QEntry(QNext(&afs_disconShadow), struct vcache, shadowq);

	/* Must release the dirty lock to be able to get a vcache lock */
	ReleaseWriteLock(&afs_disconDirtyLock);
	ObtainWriteLock(&tvc->lock, 706);

	if (squash)
	   afs_ResetVCache(tvc, acred, 0);

	afs_DeleteShadowDir(tvc);

	ReleaseWriteLock(&tvc->lock);
	ObtainWriteLock(&afs_disconDirtyLock, 709);
    }				/* while (tvc) */
}

/*!
 * This function throws away the whole disconnected state, allowing
 * the cache manager to reconnect to a server if we get into a state
 * where reconiliation is impossible.
 *
 * \param acred
 *
 */
void
afs_DisconDiscardAll(afs_ucred_t *acred)
{
    struct vcache *tvc;

    ObtainWriteLock(&afs_disconDirtyLock, 717);
    while (!QEmpty(&afs_disconDirty)) {
	tvc = QEntry(QPrev(&afs_disconDirty), struct vcache, dirtyq);
	QRemove(&tvc->dirtyq);
	ReleaseWriteLock(&afs_disconDirtyLock);

	ObtainWriteLock(&tvc->lock, 718);
	afs_ResetVCache(tvc, acred, 0);
	tvc->f.truncPos = AFS_NOTRUNC;
	ReleaseWriteLock(&tvc->lock);
	ObtainWriteLock(&afs_disconDirtyLock, 719);
	afs_PutVCache(tvc);
    }

    afs_DisconDiscardAllShadows(1, acred);

    ReleaseWriteLock(&afs_disconDirtyLock);
}

/*!
 * Print list of disconnected files.
 *
 * \note Call with afs_DDirtyVCListLock read locked.
 */
void
afs_DbgDisconFiles(void)
{
    struct vcache *tvc;
    struct afs_q *q;
    int i = 0;

    afs_warn("List of dirty files: \n");

    ObtainReadLock(&afs_disconDirtyLock);
    for (q = QPrev(&afs_disconDirty); q != &afs_disconDirty; q = QPrev(q)) {
        tvc = QEntry(q, struct vcache, dirtyq);

	afs_warn("Cell=%u Volume=%u VNode=%u Unique=%u\n",
		tvc->f.fid.Cell,
		tvc->f.fid.Fid.Volume,
		tvc->f.fid.Fid.Vnode,
		tvc->f.fid.Fid.Unique);

	i++;
	if (i >= 30)
	    osi_Panic("afs_DbgDisconFiles: loop in dirty list\n");
    }
    ReleaseReadLock(&afs_disconDirtyLock);
}

/*!
 * Generate a fake fid for a disconnected shadow dir.
 * Similar to afs_GenFakeFid, only that it uses the dhash
 * to search for a uniquifier because a shadow dir lives only
 * in the dcache.
 *
 * \param afid
 *
 * \note Don't forget to fill in afid with Cell and Volume.
 */
void
afs_GenShadowFid(struct VenusFid *afid)
{
    afs_uint32 i, index, max_unique = 1;
    struct vcache *tvc = NULL;

    /* Try generating a fid that isn't used in the vhash. */
    do {
	/* Shadow Fids are always directories */
    	afid->Fid.Vnode = afs_DisconVnode + 1;

    	i = DVHash(afid);
    	ObtainWriteLock(&afs_xdcache, 737);
    	for (index = afs_dvhashTbl[i]; index != NULLIDX; index = i) {
            i = afs_dvnextTbl[index];
            if (afs_indexUnique[index] > max_unique)
	    	max_unique = afs_indexUnique[index];
    	}

	ReleaseWriteLock(&afs_xdcache);
	afid->Fid.Unique = max_unique + 1;
	afs_DisconVnode += 2;
	if (!afs_DisconVnode)
    	    afs_DisconVnode = 2;

	/* Is this a used vnode? */
    	ObtainSharedLock(&afs_xvcache, 762);
    	tvc = afs_FindVCache(afid, 0, 1);
    	ReleaseSharedLock(&afs_xvcache);
	if (tvc)
	    afs_PutVCache(tvc);
    } while (tvc);
}

/*!
 * Generate a fake fid (vnode and uniquifier) for a vcache
 * (either dir or normal file). The vnode is generated via
 * afs_DisconVNode and the uniquifier by getting the highest
 * uniquifier on a hash chain and incrementing it by one.
 *
 * \param afid	 The fid structre that will be filled.
 * \param avtype Vnode type: VDIR/VREG.
 * \param lock	 True indicates that xvcache may be obtained,
 * 		 False that it is already held
 *
 * \note The cell number must be completed somewhere else.
 */
void
afs_GenFakeFid(struct VenusFid *afid, afs_uint32 avtype, int lock)
{
    struct vcache *tvc;
    afs_uint32 max_unique = 0, i;

    switch (avtype) {
    case VDIR:
    	afid->Fid.Vnode = afs_DisconVnode + 1;
	break;
    case VREG:
    case VLNK:
    	afid->Fid.Vnode = afs_DisconVnode;
	break;
    }

    if (lock)
	ObtainWriteLock(&afs_xvcache, 736);
    i = VCHash(afid);
    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
        if (tvc->f.fid.Fid.Unique > max_unique)
	    max_unique = tvc->f.fid.Fid.Unique;
    }
    if (lock)
	ReleaseWriteLock(&afs_xvcache);

    afid->Fid.Unique = max_unique + 1;
    afs_DisconVnode += 2;
    if (!afs_DisconVnode)
    	afs_DisconVnode = 2;
}

/*!
 * Fill in stats for a newly created file/directory.
 *
 * \param adp The parent dir's vcache.
 * \param avc The created vnode.
 * \param afid The new fid.
 * \param attrs
 * \param areq
 * \param file_type Specify if file or directory.
 *
 * \note Call with avc write locked.
 */
void
afs_GenDisconStatus(struct vcache *adp, struct vcache *avc,
		    struct VenusFid *afid, struct vattr *attrs,
		    struct vrequest *areq, int file_type)
{
    memcpy(&avc->f.fid, afid, sizeof(struct VenusFid));
    avc->f.m.Mode = attrs->va_mode;
    /* Used to do this:
     * avc->f.m.Owner = attrs->va_uid;
     * But now we use the parent dir's ownership,
     * there's no other way to get a server owner id.
     * XXX: Does it really matter?
     */
    avc->f.m.Group = adp->f.m.Group;
    avc->f.m.Owner = adp->f.m.Owner;
    hset64(avc->f.m.DataVersion, 0, 0);
    avc->f.m.Length = attrs->va_size;
    avc->f.m.Date = osi_Time();
    switch(file_type) {
      case VREG:
	vSetType(avc, VREG);
        avc->f.m.Mode |= S_IFREG;
	avc->f.m.LinkCount = 1;
	avc->f.parent.vnode = adp->f.fid.Fid.Vnode;
	avc->f.parent.unique = adp->f.fid.Fid.Unique;
	break;
      case VDIR:
        vSetType(avc, VDIR);
        avc->f.m.Mode |= S_IFDIR;
	avc->f.m.LinkCount = 2;
	break;
      case VLNK:
	vSetType(avc, VLNK);
	avc->f.m.Mode |= S_IFLNK;
	if ((avc->f.m.Mode & 0111) == 0)
	    avc->mvstat = 1;
	avc->f.parent.vnode = adp->f.fid.Fid.Vnode;
	avc->f.parent.unique = adp->f.fid.Fid.Unique;
	break;
      default:
	break;
    }
    avc->f.anyAccess = adp->f.anyAccess;
    afs_AddAxs(avc->Access, areq->uid, adp->Access->axess);

    avc->callback = NULL;
    avc->f.states |= CStatd;
    avc->f.states &= ~CBulkFetching;
}
