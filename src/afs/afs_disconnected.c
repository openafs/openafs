/*
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"
 
RCSID("$Header$");
 
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */
#include "afs/lock.h"
#include "afs/afs_cbqueue.h"

#ifdef AFS_DISCON_ENV

#define dv_match(vc, fstat) 				 \
	((vc->m.DataVersion.low == fstat.DataVersion) && \
     	(vc->m.DataVersion.high == fstat.dataVersionHigh))

/*! Global list of dirty vcaches. */
/*! Last added element. */
struct vcache *afs_DDirtyVCList = NULL;
/*! Head of list. */
struct vcache *afs_DDirtyVCListStart = NULL;
/*! Previous element in the list. */
struct vcache *afs_DDirtyVCListPrev = NULL;

/*! Locks list of dirty vcaches. */
afs_rwlock_t afs_DDirtyVCListLock;

extern afs_int32 *afs_dvhashTbl;	/*Data cache hash table */
extern afs_int32 *afs_dchashTbl;	/*Data cache hash table */
extern afs_int32 *afs_dvnextTbl;	/*Dcache hash table links */
extern afs_int32 *afs_dcnextTbl;	/*Dcache hash table links */
extern struct dcache **afs_indexTable;	/*Pointers to dcache entries */

/*! Vnode number. On file creation, use the current
 *  value and increment it.
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

/*!
 * Find the first dcache of a file that has the specified fid.
 * Similar to afs_FindDCache, only that it takes a fid instead
 * of a vcache and it can get the first dcache.
 *
 * \param afid
 *
 * \return The found dcache or NULL.
 */
struct dcache *afs_FindDCacheByFid(register struct VenusFid *afid)
{
    register afs_int32 i, index;
    register struct dcache *tdc = NULL;

    i = DVHash(afid);
    ObtainWriteLock(&afs_xdcache, 758);
    for (index = afs_dvhashTbl[i]; index != NULLIDX;) {
	if (afs_indexUnique[index] == afid->Fid.Unique) {
	    tdc = afs_GetDSlot(index, NULL);
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, afid)) {
		break;		/* leaving refCount high for caller */
	    }
	    afs_PutDCache(tdc);
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
int afs_GenStoreStatus(struct vcache *avc, struct AFSStoreStatus *astat)
{
    if (!avc || !astat || !avc->ddirty_flags)
    	return 0;

    /* Clean up store stat. */
    memset(astat, 0, sizeof(struct AFSStoreStatus));

    if (avc->ddirty_flags & VDisconSetTime) {
	/* Update timestamp. */
	astat->ClientModTime = avc->m.Date;
	astat->Mask |= AFS_SETMODTIME;
    }

    if (avc->ddirty_flags & VDisconSetMode) {
	/* Copy the mode bits. */
	astat->UnixModeBits = avc->m.Mode;
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
int get_parent_dir_fid_hook(void *hdata,
				char *aname,
				afs_int32 vnode,
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
 */
int afs_GetParentDirFid(struct vcache *avc, struct VenusFid *afid)
{
    struct dcache *tdc;

    afid->Cell = avc->fid.Cell;
    afid->Fid.Volume = avc->fid.Fid.Volume;

    if (vType(avc) == VREG) {
	/* Normal files have the dir fid embedded in the vcache. */
	afid->Fid.Vnode = avc->parentVnode;
	afid->Fid.Unique = avc->parentUnique;

    } else if (vType(avc) == VDIR) {
	/* If dir or parent dir created locally*/
	tdc = afs_FindDCacheByFid(&avc->fid);
    	if (tdc) {
	    /* Lookup each entry for the fid. It should be the first. */
    	    afs_dir_EnumerateDir(tdc, &get_parent_dir_fid_hook, afid);
    	    afs_PutDCache(tdc);
	}
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
int get_vnode_name_hook(void *hdata,
				char *aname,
				afs_int32 vnode,
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
int afs_GetVnodeName(struct vcache *avc,
			struct VenusFid *afid,
			char *aname,
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
	parent_fid.Cell = avc->fid.Cell;
	parent_fid.Fid.Volume = avc->fid.Fid.Volume;
	if (avc->ddirty_flags & VDisconRename) {
	    /* For renames the old dir fid is needed. */
	    parent_fid.Fid.Vnode = avc->oldVnode;
	    parent_fid.Fid.Unique = avc->oldUnique;
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

	shadow_fid.Cell = parent_vc->fid.Cell;
    	shadow_fid.Fid.Volume = parent_vc->fid.Fid.Volume;
    	shadow_fid.Fid.Vnode = parent_vc->shVnode;
    	shadow_fid.Fid.Unique = parent_vc->shUnique;

	afs_PutVCache(parent_vc);

	/* Get shadow dir's dcache. */
	tdc = afs_FindDCacheByFid(&shadow_fid);

    } else {

	/* For normal files, look into the current dir's entry. */
	tdc = afs_FindDCacheByFid(afid);
    }			/* if (deleted) */

    if (tdc) {
	tnf.fid = &avc->fid;
   	tnf.name_len = -1;
    	tnf.name = aname;
    	afs_dir_EnumerateDir(tdc, &get_vnode_name_hook, &tnf);
	afs_PutDCache(tdc);
	if (tnf.name_len == -1)
	    code = ENOENT;
    } else {
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
int chk_del_children_hook(void *hdata,
				char *aname,
				afs_int32 vnode,
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
    tfid.Cell = v->vc->fid.Cell;
    tfid.Fid.Volume = v->vc->fid.Fid.Volume;
    tfid.Fid.Vnode = vnode;
    tfid.Fid.Unique = unique;

    ObtainSharedLock(&afs_xvcache, 757);
    tvc = afs_FindVCache(&tfid, 0, 1);
    ReleaseSharedLock(&afs_xvcache);

    /* Count unfinished dirty children. VDisconShadowed can still be set,
     * because we need it to remove the shadow dir.
     */
    if (tvc) {
	if (tvc->ddirty_flags)
	    v->count++;

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
int afs_CheckDeletedChildren(struct vcache *avc)
{
    struct dcache *tdc;
    struct DirtyChildrenCount dcc;
    struct VenusFid shadow_fid;

    if (!(avc->ddirty_flags & VDisconShadowed))
    	/* Empty dir. */
    	return 0;

    shadow_fid.Cell = avc->fid.Cell;
    shadow_fid.Fid.Volume = avc->fid.Fid.Volume;
    shadow_fid.Fid.Vnode = avc->shVnode;
    shadow_fid.Fid.Unique = avc->shUnique;

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
int fix_children_fids_hook(void *hdata,
				char *aname,
				afs_int32 vnode,
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
	    tvc->parentVnode = afid->Fid.Vnode;
	    tvc->parentUnique = afid->Fid.Unique;

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
void afs_FixChildrenFids(struct VenusFid *old_fid, struct VenusFid *new_fid)
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

int list_dir_hook(void *hdata, char *aname, afs_int32 vnode, afs_int32 unique)
{
    printf("list_dir_hook: %s v:%u u:%u\n", aname, vnode, unique);
    return 0;
}

void afs_DbgListDirEntries(struct VenusFid *afid)
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
 * Handles file renaming on reconnection:
 * - Get the old name from the old dir's shadow dir.
 * - Get the new name from the current dir.
 * - Old dir fid and new dir fid are collected along the way.
 * */
int afs_ProcessOpRename(struct vcache *avc, struct vrequest *areq)
{
    struct VenusFid old_pdir_fid, new_pdir_fid;
    char *old_name, *new_name;
    struct AFSFetchStatus OutOldDirStatus, OutNewDirStatus;
    struct AFSVolSync tsync;
    struct afs_conn *tc;
    afs_uint32 code = 0;
    XSTATS_DECLS;

    /* Get old dir vcache. */
    old_pdir_fid.Cell = avc->fid.Cell;
    old_pdir_fid.Fid.Volume = avc->fid.Fid.Volume;
    old_pdir_fid.Fid.Vnode = avc->oldVnode;
    old_pdir_fid.Fid.Unique = avc->oldUnique;

    /* Get old name. */
    old_name = (char *) afs_osi_Alloc(AFSNAMEMAX);
    if (!old_name) {
	printf("afs_ProcessOpRename: Couldn't alloc space for old name.\n");
	return ENOMEM;
    }
    code = afs_GetVnodeName(avc, &old_pdir_fid, old_name, 1);
    if (code) {
	printf("afs_ProcessOpRename: Couldn't find old name.\n");
	code = ENOENT;
	goto end2;
    }

    /* Alloc data first. */
    new_name = (char *) afs_osi_Alloc(AFSNAMEMAX);
    if (!new_name) {
	printf("afs_ProcessOpRename: Couldn't alloc space for new name.\n");
	code = ENOMEM;
	goto end2;
    }

    if (avc->ddirty_flags & VDisconRenameSameDir) {
    	/* If we're in the same dir, don't do the lookups all over again,
	 * just copy fid and vcache from the old dir.
	 */
	memcpy(&new_pdir_fid, &old_pdir_fid, sizeof(struct VenusFid));
    } else {
	/* Get parent dir's FID.*/
    	new_pdir_fid.Fid.Unique = 0;
    	afs_GetParentDirFid(avc, &new_pdir_fid);
    	if (!new_pdir_fid.Fid.Unique) {
	    printf("afs_ProcessOpRename: Couldn't find new parent dir FID.\n");
	    code = ENOENT;
	    goto end1;
        }
    }

    /* And finally get the new name. */
    code = afs_GetVnodeName(avc, &new_pdir_fid, new_name, 0);
    if (code) {
	printf("afs_ProcessOpRename: Couldn't find new name.\n");
	code = ENOENT;
	goto end1;
    }

    /* Send to data to server. */
    do {
    	tc = afs_Conn(&old_pdir_fid, areq, SHARED_LOCK);
    	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_RENAME);
	    RX_AFS_GUNLOCK();
	    code = RXAFS_Rename(tc->id,
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
    		code,
		&new_pdir_fid,
		areq,
		AFS_STATS_FS_RPCIDX_RENAME,
		SHARED_LOCK,
		NULL));

    if (code)
    	printf("afs_ProcessOpRename: server code=%u\n", code);
end1:
    afs_osi_Free(new_name, AFSNAMEMAX);
end2:
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
int afs_ProcessOpCreate(struct vcache *avc,
				struct vrequest *areq,
				struct AFS_UCRED *acred)
{
    char *tname = NULL;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutFidStatus, OutDirStatus;
    struct VenusFid pdir_fid, newFid;
    struct server *hostp = NULL;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    struct vcache *tdp = NULL, *tvc = NULL;
    struct dcache *tdc = NULL;
    struct afs_conn *tc;
    afs_int32 now, hash, new_hash, index;
    int code = 0;
    XSTATS_DECLS;

    /* Get parent dir's FID. */
    pdir_fid.Fid.Unique = 0;
    afs_GetParentDirFid(avc, &pdir_fid);
    if (!pdir_fid.Fid.Unique) {
	printf("afs_ProcessOpCreate: Couldn't find parent dir'sFID.\n");
	return ENOENT;
    }

    tname = afs_osi_Alloc(AFSNAMEMAX);
    if (!tname) {
	printf("afs_ProcessOpCreate: Couldn't alloc space for file name\n");
	return ENOMEM;
    }

    /* Get vnode's name. */
    code = afs_GetVnodeName(avc, &pdir_fid, tname, 0);
    if (code) {
	printf("afs_ProcessOpCreate: Couldn't find file name\n");
	code = ENOENT;
	goto end;
    }

    /* Get parent dir vcache. */
    ObtainSharedLock(&afs_xvcache, 760);
    tdp = afs_FindVCache(&pdir_fid, 0, 1);
    ReleaseSharedLock(&afs_xvcache);
    if (!tdp) {
	printf("afs_ProcessOpCreate: Couldn't find parent dir's vcache\n");
	code = ENOENT;
	goto end;
    }

    if (tdp->ddirty_flags & VDisconCreate) {
    	/* If the parent dir has been created locally, defer
	 * this vnode for later by moving it to the end.
	 */
	afs_DDirtyVCList->ddirty_next = avc;
	afs_DDirtyVCList = avc;
	printf("afs_ProcessOpRemove: deferring this vcache\n");
    	code = ENOTEMPTY;
	goto end;
    }

    /* Set status. */
    InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETGROUP;
    InStatus.ClientModTime = avc->m.Date;
    InStatus.Owner = avc->m.Owner;
    InStatus.Group = (afs_int32) acred->cr_gid;
    /* Only care about protection bits. */
    InStatus.UnixModeBits = avc->m.Mode & 0xffff;

    /* Connect to server. */
    if (vType(avc) == VREG) {
        /* Make file on server. */
        do {
            tc = afs_Conn(&tdp->fid, areq, SHARED_LOCK);
            if (tc) {
		/* Remember for callback processing. */
                hostp = tc->srvr->server;
                now = osi_Time();
                XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_CREATEFILE);
                RX_AFS_GUNLOCK();
                code = RXAFS_CreateFile(tc->id,
                                (struct AFSFid *)&tdp->fid.Fid,
                                tname,
                                &InStatus,
                                (struct AFSFid *) &newFid.Fid,
                                &OutFidStatus,
                                &OutDirStatus,
                                &CallBack,
                                &tsync);
                RX_AFS_GLOCK();
                XSTATS_END_TIME;
                CallBack.ExpirationTime += now;
            } else
                code = -1;
        } while (afs_Analyze(tc,
                        code,
                        &tdp->fid,
                        areq,
                        AFS_STATS_FS_RPCIDX_CREATEFILE,
                        SHARED_LOCK,
                        NULL));

    } else if (vType(avc) == VDIR) {
        /* Make dir on server. */
        do {
            tc = afs_Conn(&tdp->fid, areq, SHARED_LOCK);
            if (tc) {
                XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_MAKEDIR);
                now = osi_Time();
                RX_AFS_GUNLOCK();
                code = RXAFS_MakeDir(tc->id,
                                (struct AFSFid *) &tdp->fid.Fid,
                                tname,
                                &InStatus,
                                (struct AFSFid *) &newFid.Fid,
                                &OutFidStatus,
                                &OutDirStatus,
                                &CallBack,
                                &tsync);
                RX_AFS_GLOCK();
                XSTATS_END_TIME;
                CallBack.ExpirationTime += now;
                /* DON'T forget to set the callback at some point. */
            } else
               code = -1;
         } while (afs_Analyze(tc,
                        code,
                        &tdp->fid,
                        areq,
                        AFS_STATS_FS_RPCIDX_MAKEDIR,
                        SHARED_LOCK,
                        NULL));
    }					/* Do server changes. */

    /* TODO: Handle errors. */
    if (code) {
	printf("afs_ProcessOpCreate: error while creating vnode on server, code=%d .\n", code);
	code = EIO;
	goto end;
    }

    /* The rpc doesn't set the cell number. */
    newFid.Cell = avc->fid.Cell;

    /*
     * Change the fid in the dir entry.
     */

    /* Seek the dir's dcache. */
    tdc = afs_FindDCacheByFid(&tdp->fid);
    if (tdc) {
    	/* And now change the fid in the parent dir entry. */
    	afs_dir_ChangeFid(tdc, tname, &avc->fid.Fid.Vnode, &newFid.Fid.Vnode);
    	afs_PutDCache(tdc);
    }

    if (vType(avc) == VDIR) {
	/* Change fid in the dir for the "." entry. ".." has alredy been
	 * handled by afs_FixChildrenFids when processing the parent dir.
	 */
	tdc = afs_FindDCacheByFid(&avc->fid);
	if (tdc) {
   	    afs_dir_ChangeFid(tdc, ".", &avc->fid.Fid.Vnode, &newFid.Fid.Vnode);

	    if (avc->m.LinkCount >= 2)
	        /* For non empty dirs, fix children's parentVnode and parentUnique
	    	 * reference.
	     	 */
	    	afs_FixChildrenFids(&avc->fid, &newFid);

	    afs_PutDCache(tdc);
	}
    }

    /* Recompute hash chain positions for vnode and dcaches.
     * Then change to the new FID.
     */

    /* The vcache goes first. */
    ObtainWriteLock(&afs_xvcache, 735);

    /* Old fid hash. */
    hash = VCHash(&avc->fid);
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
 	//for (tvc = afs_vhashT[hash]; tdp; tdp = tdp->hnext) {
 	for (tvc = afs_vhashT[hash]; tvc; tvc = tvc->hnext) {
	    if (tvc->hnext == avc) {
		tvc->hnext = avc->hnext;
		break;
	    }
        }
    }                           /* if (!afs_vhashT[i]->hnext) */
    QRemove(&afs_vhashTV[hash]);

    /* Insert hash in new position. */
    avc->hnext = afs_vhashT[new_hash];
    afs_vhashT[new_hash] = avc;
    QAdd(&afs_vhashTV[new_hash], &avc->vhashq);

    ReleaseWriteLock(&afs_xvcache);

    /* Do the same thing for all dcaches. */
    hash = DVHash(&avc->fid);
    ObtainWriteLock(&afs_xdcache, 743);
    for (index = afs_dvhashTbl[hash]; index != NULLIDX; index = hash) {
        hash = afs_dvnextTbl[index];
        tdc = afs_GetDSlot(index, NULL);
        ReleaseReadLock(&tdc->tlock);
	if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
            if (!FidCmp(&tdc->f.fid, &avc->fid)) {

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
                //afs_MaybeWakeupTruncateDaemon();
           }                   /* if fid match */
	}                       /* if uniquifier match */
    	if (tdc)
	    afs_PutDCache(tdc);
    }                           /* for all dcaches in this hash bucket */
    ReleaseWriteLock(&afs_xdcache);

    /* Now we can set the new fid. */
    memcpy(&avc->fid, &newFid, sizeof(struct VenusFid));

    if (tdp) {
    	/* Unset parent dir CStat flag, so it will get refreshed on next
	 * online stat.
	 */
	ObtainWriteLock(&tdp->lock, 745);
	tdp->states &= ~CStatd;
    	ReleaseWriteLock(&tdp->lock);
    }
end:
    if (tdp)
    	afs_PutVCache(tdp);
    afs_osi_Free(tname, AFSNAMEMAX);
    return code;
}

/*!
 * Remove a vnode on the server, be it file or directory.
 * Not much to do here only get the parent dir's fid and call the
 * removel rpc.
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
int afs_ProcessOpRemove(struct vcache *avc, struct vrequest *areq)
{
    char *tname = NULL;
    struct AFSFetchStatus OutDirStatus;
    struct VenusFid pdir_fid;
    struct AFSVolSync tsync;
    struct afs_conn *tc;
    struct vcache *tdp = NULL;
    int code = 0;
    XSTATS_DECLS;

    /* Get parent dir's FID. */
    pdir_fid.Fid.Unique = 0;
    afs_GetParentDirFid(avc, &pdir_fid);
    if (!pdir_fid.Fid.Unique) {
	printf("afs_ProcessOpRemove: Couldn't find parent dir's FID.\n");
	return ENOENT;
    }

    tname = afs_osi_Alloc(AFSNAMEMAX);
    if (!tname) {
	printf("afs_ProcessOpRemove: Couldn't find file name\n");
	return ENOMEM;
    }

    /* Get file name. */
    code = afs_GetVnodeName(avc, &pdir_fid, tname, 1);
    if (code) {
	printf("afs_ProcessOpRemove: Couldn't find file name\n");
	code = ENOENT;
	goto end;
    }

    if ((vType(avc) == VDIR) && (afs_CheckDeletedChildren(avc))) {
	/* Deleted children of this dir remain unsynchronized.
	 * Defer this vcache.
	 */
	afs_DDirtyVCList->ddirty_next = avc;
	afs_DDirtyVCList = avc;
    	code = ENOTEMPTY;
	goto end;
    }

    if (vType(avc) == VREG) {
    	/* Remove file on server. */
	do {
	    tc = afs_Conn(&pdir_fid, areq, SHARED_LOCK);
	    if (tc) {
	    	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEFILE);
		RX_AFS_GUNLOCK();
		code = RXAFS_RemoveFile(tc->id,
				&pdir_fid.Fid,
				tname,
				&OutDirStatus,
				&tsync);

		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	    } else
		code = -1;
	} while (afs_Analyze(tc,
			code,
			&pdir_fid,
			areq,
			AFS_STATS_FS_RPCIDX_REMOVEFILE,
			SHARED_LOCK,
			NULL));

    } else if (vType(avc) == VDIR) {
    	/* Remove dir on server. */
	do {
	    tc = afs_Conn(&pdir_fid, areq, SHARED_LOCK);
	    if (tc) {
		XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEDIR);
		RX_AFS_GUNLOCK();
		code = RXAFS_RemoveDir(tc->id,
				&pdir_fid.Fid,
				tname,
				&OutDirStatus,
				&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	   } else
		code = -1;
	} while (afs_Analyze(tc,
			code,
			&pdir_fid,
			areq,
			AFS_STATS_FS_RPCIDX_REMOVEDIR,
			SHARED_LOCK,
			NULL));

    }				/* if (vType(avc) == VREG) */

    if (code)
    	printf("afs_ProcessOpRemove: server returned code=%u\n", code);

    /* Remove the statd flag from parent dir's vcache. */
    ObtainSharedLock(&afs_xvcache, 761);
    tdp = afs_FindVCache(&pdir_fid, 0, 1);
    ReleaseSharedLock(&afs_xvcache);
    if (tdp) {
    	ObtainWriteLock(&tdp->lock, 746);
	tdp->states &= ~CStatd;
	ReleaseWriteLock(&tdp->lock);
	afs_PutVCache(tdp);
    }
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
int afs_SendChanges(struct vcache *avc, struct vrequest *areq)
{
    struct afs_conn *tc;
    struct AFSStoreStatus sstat;
    struct AFSFetchStatus fstat;
    struct AFSVolSync tsync;
    int code = 0;
    int flags = 0;
    XSTATS_DECLS;

    /* Start multiplexing dirty operations from ddirty_flags field: */
    if (avc->ddirty_flags & VDisconSetAttrMask) {
	/* Setattr OPS: */
	/* Turn dirty vc data into a new store status... */
	if (afs_GenStoreStatus(avc, &sstat) > 0) {
	    do {
		tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
		if (tc) {
		    /* ... and send it. */
		    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STORESTATUS);
		    RX_AFS_GUNLOCK();
		    code = RXAFS_StoreStatus(tc->id,
				(struct AFSFid *) &avc->fid.Fid,
				&sstat,
				&fstat,
				&tsync);

		    RX_AFS_GLOCK();
		    XSTATS_END_TIME;
		} else
		    code = -1;

	} while (afs_Analyze(tc,
			code,
			&avc->fid,
			areq,
			AFS_STATS_FS_RPCIDX_STORESTATUS,
			SHARED_LOCK,
			NULL));

	}		/* if (afs_GenStoreStatus() > 0)*/
    }			/* disconnected SETATTR */

    if (code)
    	return code;

    if (avc->ddirty_flags &
	(VDisconTrunc
	| VDisconWriteClose
	| VDisconWriteFlush
	| VDisconWriteOsiFlush)) {

	/* Truncate OP: */
	do {
	    tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	    if (tc) {
		/* Set storing flags. XXX: A tad inefficient ... */
		if (avc->ddirty_flags & VDisconWriteClose)
		    flags |= AFS_LASTSTORE;
		if (avc->ddirty_flags & VDisconWriteOsiFlush)
		    flags |= (AFS_SYNC | AFS_LASTSTORE);
		if (avc->ddirty_flags & VDisconWriteFlush)
		    flags |= AFS_SYNC;

		/* Try to send store to server. */
		/* XXX: AFS_LASTSTORE for writes? Or just AFS_SYNC for all? */
		code = afs_StoreAllSegments(avc, areq, flags);
	    } else
		code = -1;

	} while (afs_Analyze(tc,
			code,
			&avc->fid,
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
 * return 1.
 *
 * \note For now, it's the request from the PDiscon pioctl.
 *
 */
int afs_ResyncDisconFiles(struct vrequest *areq, struct AFS_UCRED *acred)
{
    struct afs_conn *tc;
    struct vcache *tvc, *tmp;
    struct AFSFetchStatus fstat;
    struct AFSCallBack callback;
    struct AFSVolSync tsync;
    struct vcache *shList, *shListStart;
    int code;
    int sync_failed = 0;
    int ret_code = 0;
    int defered = 0;
    afs_int32 start = 0;
    XSTATS_DECLS;
    //AFS_STATCNT(afs_ResyncDisconFiles);

    shList = shListStart = NULL;

    ObtainReadLock(&afs_DDirtyVCListLock);

    tvc = afs_DDirtyVCListStart;
    while (tvc) {

	/* Get local write lock. */
	ObtainWriteLock(&tvc->lock, 704);
  	sync_failed = 0;

	if ((tvc->ddirty_flags & VDisconRemove) &&
	    (tvc->ddirty_flags & VDisconCreate)) {
	   /* Data created and deleted locally. The server doesn't
	    * need to know about this, so we'll just skip this file
	    * from the dirty list.
	    */
	    goto skip_file;

	} else if (tvc->ddirty_flags & VDisconRemove) {
	    /* Delete the file on the server and just move on
	     * to the next file. After all, it has been deleted
	     * we can't replay any other operation it.
	     */
	    code = afs_ProcessOpRemove(tvc, areq);
	    if (code == ENOTEMPTY)
	    	defered = 1;
	    goto skip_file;

	} else if (tvc->ddirty_flags & VDisconCreate) {
	    /* For newly created files, we don't need a server lock. */
	    code = afs_ProcessOpCreate(tvc, areq, acred);
	    if (code == ENOTEMPTY)
	    	defered = 1;
	    if (code)
	    	goto skip_file;
	}

  	/* Get server write lock. */
  	do {
	    tc = afs_Conn(&tvc->fid, areq, SHARED_LOCK);
  	    if (tc) {
	    	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_SETLOCK);
  		RX_AFS_GUNLOCK();
  		code = RXAFS_SetLock(tc->id,
					(struct AFSFid *)&tvc->fid.Fid,
					LockWrite,
					&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	   } else
		code = -1;

	} while (afs_Analyze(tc,
			code,
			&tvc->fid,
			areq,
			AFS_STATS_FS_RPCIDX_SETLOCK,
			SHARED_LOCK,
			NULL));

	if (code) {
	    sync_failed = 1;
	    goto skip_file;
	}

	if ((tvc->ddirty_flags & VDisconRename) &&
		!(tvc->ddirty_flags & VDisconCreate)) {
	    /* Rename file only if it hasn't been created locally. */
	    code = afs_ProcessOpRename(tvc, areq);
	    if (code)
	    	goto skip_file;
	}

	/* Issue a FetchStatus to get info about DV and callbacks. */
	do {
	    tc = afs_Conn(&tvc->fid, areq, SHARED_LOCK);
	    if (tc) {
	    	tvc->callback = tc->srvr->server;
		start = osi_Time();
		XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHSTATUS);
		RX_AFS_GUNLOCK();
		code = RXAFS_FetchStatus(tc->id,
				(struct AFSFid *)&tvc->fid.Fid,
				&fstat,
				&callback,
				&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	    } else
		code = -1;

	} while (afs_Analyze(tc,
			code,
			&tvc->fid,
			areq,
			AFS_STATS_FS_RPCIDX_FETCHSTATUS,
			SHARED_LOCK,
			NULL));

	if (code) {
	    sync_failed = 1;
	    goto unlock_srv_file;
	}

	if ((dv_match(tvc, fstat) && (tvc->m.Date == fstat.ServerModTime)) ||
	    	(afs_ConflictPolicy == CLIENT_WINS) ||
		(tvc->ddirty_flags & VDisconCreate)) {
	    /*
	     * Send changes to the server if there's data version match, or
	     * client wins policy has been selected or file has been created
	     * but doesn't have it's the contents on to the server yet.
	     */
	   /*
	    * XXX: Checking server attr changes by timestamp might not the
	    * most elegant solution, but it's the most viable one that we could find.
	    */
	    afs_UpdateStatus(tvc, &tvc->fid, areq, &fstat, &callback, start);
	    code = afs_SendChanges(tvc, areq);

	} else if (afs_ConflictPolicy == SERVER_WINS) {
	    /* DV mismatch, apply collision resolution policy. */
	    /* Dequeue whatever callback is on top (XXX: propably none). */
      	    ObtainWriteLock(&afs_xcbhash, 706);
	    afs_DequeueCallback(tvc);
	    tvc->callback = NULL;
	    tvc->states &= ~(CStatd | CDirty | CUnique);
	    ReleaseWriteLock(&afs_xcbhash);

	    /* Save metadata. File length gets updated as well because we
	     * just removed CDirty from the avc.
	     */
	    //afs_ProcessFS(tvc, &fstat, areq);

	    /* Discard this files chunks and remove from current dir. */
	    afs_TryToSmush(tvc, acred, 1);
	    osi_dnlc_purgedp(tvc);
	    if (tvc->linkData && !(tvc->states & CCore)) {
		/* Take care of symlinks. */
		afs_osi_Free(tvc->linkData, strlen(tvc->linkData) + 1);
		tvc->linkData = NULL;
	    }

	    /* Otherwise file content's won't be synchronized. */
	    tvc->truncPos = AFS_NOTRUNC;

	} else {
	    printf("afs_ResyncDisconFiles: no resolution policy selected.\n");
	}		/* if DV match or client wins policy */

	if (code) {
	    sync_failed = 1;
	    printf("Sync FAILED.\n");
	}

unlock_srv_file:
	/* Release server write lock. */
	do {
	    tc = afs_Conn(&tvc->fid, areq, SHARED_LOCK);
	    if (tc) {
	    	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_RELEASELOCK);
	    	RX_AFS_GUNLOCK();
		code = RXAFS_ReleaseLock(tc->id,
				(struct AFSFid *) &tvc->fid.Fid,
				&tsync);
		RX_AFS_GLOCK();
		XSTATS_END_TIME;
	    } else
		code = -1;
	} while (afs_Analyze(tc,
			code,
			&tvc->fid,
			areq,
			AFS_STATS_FS_RPCIDX_RELEASELOCK,
			SHARED_LOCK,
			NULL));

skip_file:
	/* Pop this dirty vc out. */
	tmp = tvc;
	tvc = tvc->ddirty_next;

	if (!defered) {
	    /* Vnode not deferred. Clean it up. */
	    if (!sync_failed) {
	    	if (tmp->ddirty_flags == VDisconShadowed) {
		    /* Dirs that have only the shadow flag set might still
		     * be used so keep them in a different list, that gets
		     * deleted after resync is done.
		     */
		    if (!shListStart)
		    	shListStart = shList = tmp;
		    else {
		    	shList->ddirty_next = tmp;
			shList = tmp;
		    }
		} else if (tmp->ddirty_flags & VDisconShadowed)
	    	    /* We can discard the shadow dir now. */
	    	    afs_DeleteShadowDir(tmp);

		/* Drop the refcount on this vnode because it's not in the
		 * list anymore.
		 */
		 afs_PutVCache(tmp);

	    	/* Only if sync was successfull,
		 * clear flags and dirty references.
		 */
	    	tmp->ddirty_next = NULL;
	    	tmp->ddirty_flags = 0;
	    } else
	    	ret_code = 1;
	} else {
	    tmp->ddirty_next = NULL;
	    defered = 0;
	}			/* if (!defered) */

	/* Release local write lock. */
	ReleaseWriteLock(&tmp->lock);
    }			/* while (tvc) */

    /* Delete the rest of shadow dirs. */
    tvc = shListStart;
    while (tvc) {
    	ObtainWriteLock(&tvc->lock, 764);

	afs_DeleteShadowDir(tvc);
	tvc->shVnode = 0;
	tvc->shUnique = 0;

	tmp = tvc;
	tvc = tvc->ddirty_next;
	tmp->ddirty_next = NULL;

	ReleaseWriteLock(&tmp->lock);
    }				/* while (tvc) */

    if (ret_code == 0) {
    	/* NULLIFY dirty list only if resync complete. */
	afs_DDirtyVCListStart = NULL;
	afs_DDirtyVCList = NULL;
    }
    ReleaseReadLock(&afs_DDirtyVCListLock);

    return ret_code;
}

/*!
 * Print list of disconnected files.
 *
 * \note Call with afs_DDirtyVCListLock read locked.
 */
void afs_DbgDisconFiles()
{
    struct vcache *tvc;
    int i = 0;

    tvc = afs_DDirtyVCListStart;
    printf("List of dirty files: \n");
    while (tvc) {
	printf("Cell=%u Volume=%u VNode=%u Unique=%u\n",
		tvc->fid.Cell,
		tvc->fid.Fid.Volume,
		tvc->fid.Fid.Vnode,
		tvc->fid.Fid.Unique);
	tvc = tvc->ddirty_next;
	i++;
	if (i >= 30)
	    osi_Panic("afs_DbgDisconFiles: loop in dirty list\n");
    }
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
void afs_GenShadowFid(struct VenusFid *afid)
{
    afs_uint32 i, index, max_unique = 1;
    struct vcache *tvc = NULL;

    /* Try generating a fid that isn't used in the vhash. */
    do {
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
 * \param avc afid The fid structre that will be filled.
 * \param avtype Vnode type: VDIR/VREG.
 *
 * \note The cell number must be completed somewhere else.
 */
void afs_GenFakeFid(struct VenusFid *afid, afs_uint32 avtype)
{
    struct vcache *tvc;
    afs_uint32 max_unique = 1, i;

    if (avtype == VDIR)
    	afid->Fid.Vnode = afs_DisconVnode + 1;
    else if (avtype == VREG)
    	afid->Fid.Vnode = afs_DisconVnode;

    ObtainWriteLock(&afs_xvcache, 736);
    i = VCHash(afid);
    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
        if (tvc->fid.Fid.Unique > max_unique)
	    max_unique = tvc->fid.Fid.Unique;
    }
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
void afs_GenDisconStatus(
        struct vcache *adp,
        struct vcache *avc,
	struct VenusFid *afid,
        struct vattr *attrs,
        struct vrequest *areq,
        int file_type)
{
    memcpy(&avc->fid, afid, sizeof(struct VenusFid));
    avc->m.Mode = attrs->va_mode;
    /* Used to do this:
     * avc->m.Owner = attrs->va_uid;
     * But now we use the parent dir's ownership,
     * there's no other way to get a server owner id.
     * XXX: Does it really matter?
     */
    avc->m.Group = adp->m.Group;
    avc->m.Owner = adp->m.Owner;
    hset64(avc->m.DataVersion, 0, 0);
    avc->m.Length = attrs->va_size;
    avc->m.Date = osi_Time();
    if (file_type == VREG) {
        vSetType(avc, VREG);
        avc->m.Mode |= S_IFREG;
	avc->m.LinkCount = 1;
    } else if (file_type == VDIR) {
        vSetType(avc, VDIR);
        avc->m.Mode |= S_IFDIR;
	avc->m.LinkCount = 2;
    }

    avc->anyAccess = adp->anyAccess;
    afs_AddAxs(avc->Access, areq->uid, adp->Access->axess);

    avc->callback = NULL;
    avc->states |= CStatd;
    avc->states &= ~CBulkFetching;
}
#endif
