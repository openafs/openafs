#include <afsconfig.h>
#include "afs/param.h"

RCSID
("$Header$");

#include "afs/sysincludes.h"    /* Standard vendor system headers */
#include "afsincludes.h"        /* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "afs/osi.h"

#include "discon.h"
#include "discon_log.h"

#define lockedPutDCache(ad) ((ad)->refCount--)

extern struct vfs *afs_globalVFS;
extern  struct osi_file *disconnected_log;
extern struct vcache **afs_indexVTable;
extern short freeDVCList;
extern long afs_indexVCounter;
extern struct translations *get_translation();
extern int CacheFetchProc();
extern struct dcache *OB_GetDCache();
extern int Free_OBDcache();
extern char *get_UniqueName();
extern struct vcache *afs_NewVCache();
extern struct vcache *Lookup_VC();
extern struct osi_file *afs_CFileOpen();
extern char *afs_indexFlags;
extern long afs_dvhashTbl[DVHASHSIZE];     /*Data cache hash table*/
extern struct afs_lock afs_xdcache;
extern struct afs_lock afs_xvcache;
extern struct afs_lock afs_xvolume;
extern struct afs_lock afs_xserver;
extern struct afs_lock afs_xcell;
extern long *afs_indexVTimes;
extern char *afs_VindexFlags;

extern struct volume *afs_volumes[NVOLS];

extern u_long cur_dvnode;
extern u_long cur_fvnode;
extern struct afs_q VLRU;

extern struct vcache *LowVSrange;
extern struct vcache *HighVSrange;

extern struct afs_lock afs_ftf;

extern struct translations*  translate_table[VCSIZE];

extern struct volume *slot_to_Volume[REALLY_BIG];
extern struct cell *slot_to_Cell[REALLY_BIG];
extern struct server *slot_to_Server[REALLY_BIG];

extern struct server *afs_servers[NSERVERS];
extern struct cell *afs_cells;
extern struct volume *afs_volumes[NVOLS];
extern	discon_modes_t	discon_state;

extern struct DirEntry *dir_GetBlob();
extern int duplicate_vc();
extern	void use_translation();
extern	void discon_log();
extern	int afs_InitReq();
extern	int afs_setattr();

extern backing_store_t trans_names;
extern	struct map_header	*tname_hdr;
extern	char	*tname_map;
int recon_compare_dvs(struct conn*, struct vcache *, struct VenusFid *,
    struct translations * , char *);

/* forward function definitions */

int replay_init(struct VenusFid *, struct vcache **, int,
	struct translations **, struct VenusFid **, char *);

/*
 * Function to do common reconcile setup.  It finds the associated
 * vcache, translation struct, and the last known fid for the file.
 */

int
replay_init(cachefid, vcpp, needvc,  transpp, fidpp, errstr)
	struct VenusFid *cachefid;
	struct vcache ** vcpp;
	int needvc;
	struct translations **transpp;
	struct VenusFid **fidpp;
	char *errstr;
{
		
        /* get translation struct */
        *transpp = get_translation(cachefid);


        if ((*transpp)->newfid.Fid.Volume) {
		*fidpp = &(*transpp)->newfid;
        } else {
		*fidpp = cachefid;
	}


        /* get vcache entry */
        *vcpp = Lookup_VC(*fidpp);

        /* if not found, print error */

        if (!(*vcpp) && needvc) {
                discon_log(DISCON_ERR, "%s: failed to get vc <%x %x %x> \n",
		    errstr, (*fidpp)->Fid.Volume, (*fidpp)->Fid.Vnode,
		    (*fidpp)->Fid.Unique);
                return ENOENT;
        }

	return 0;
}


/*
 * set up the vrequest structure and get a connection.
 * Note: we assume person preforming replay is the same as
 * originally modified entry
 */
int
replay_conn_init(fidp, reqp, cred, tcp, errstr)
	struct VenusFid *fidp;
	struct vrequest	*reqp;
	struct ucred *cred;
	struct conn **tcp;
	char *errstr;
	
	
{
	afs_InitReq(reqp, cred);

	*tcp = afs_Conn(fidp, reqp);
		
	if (!*tcp) {
		discon_log(DISCON_ERR,
		    "%s: failed to get connection <%x %x %x>\n",
		    errstr, fidp->Fid.Volume, fidp->Fid.Vnode,
		    fidp->Fid.Unique);
		return -1;
	}
	return 0;
}

/*
 * compare the data version passed in with the servers data version.
 * return 0 if they match, -1 otherwise.
 */

int
recon_compare_dvs(tcp, tvc, fidp, trans, namestr)
	struct conn *tcp;
	struct vcache *tvc;
	struct VenusFid *fidp;
	struct translations *trans;
	char *namestr;
{
	struct AFSFetchStatus OutStatus;
	struct AFSCallBack CallBack;
	struct AFSVolSync tsync;
	int code;	


	/* expire any callbacks */
	if (tvc && tvc->callback && tvc->cbExpires <= osi_Time()) {
        	afs_VindexFlags[tvc->index] &= ~VC_HAVECB;
        	tvc->dflags |= VC_DIRTY;
        	tvc->callback = 0;
	}
#ifdef LHUSTON
	/* XXX turning on this code greatly helps performance,  but
	 * it doesn't always work right for some reason. I wouldn't 
	 * recommend it for every day work.
	 */

	/*
	 *  if we are in partially connected and have callback, we
	 *  don't need to go to the net.
	 */

	if ((IS_PARTIAL(discon_state)) && (tvc) && (tvc->callback)) {
		if (tvc->m.DataVersion == trans->validDV) {
			return 0;
		} else {
			return -1;
		}
	}
	if (IS_PARTIAL(discon_state)) {
		printf("check_dvs: going to net callback \n");
		printf("check_dvs: fid <%x %x %x %x>  \n",
	       	    tvc->fid.Cell, tvc->fid.Fid.Volume, tvc->fid.Fid.Vnode,
		    tvc->fid.Fid.Unique);
	}
#endif

	/* get current attributes */

	code = RXAFS_FetchStatus(tcp->id, (struct AFSFid *) &fidp->Fid,
	    &OutStatus, &CallBack, &tsync);

	if(code) {
		discon_log(DISCON_ERR,
		    "%s: failed fetchstatus <%x %x %x>\n", namestr,
		    fidp->Fid.Volume, fidp->Fid.Vnode, fidp->Fid.Unique);
		discon_log(DISCON_ERR,"\t code %d \n",code);
		return code;
	}

	/* Compare data version numbers */
	if(OutStatus.DataVersion != trans->validDV) {
		discon_log(DISCON_ERR, "%s: DV's don't match. <%x %x %x>\n",
		    namestr, fidp->Fid.Volume, fidp->Fid.Vnode,
		    fidp->Fid.Unique);
		discon_log(DISCON_NOTICE, "\toutstatus dv %d trans dv %d \n",
		    OutStatus.DataVersion, trans->validDV);
		/* set the knownbad flag */
		trans->flags |= KNOWNBAD;
		return -1;
	} else {
		/* we have a good vno, save callback information for later */
		trans->callback = tcp->server->host;
		trans->cbExpires = CallBack.ExpirationTime+osi_Time();
	}

	return 0;
}

char *
replay_check_name(name, ptransp, ctransp, pvc, pfid, reqp, ext, errname)
	char *name;
	struct translations *ptransp;
	struct translations *ctransp;
	struct vcache *pvc;
	struct VenusFid *pfid;
	struct vrequest *reqp;
	char *ext;
	char *errname;
{
	struct dcache  *pdc = 0;
	char * gen_name = 0;

	/* expire any callbacks */
	if (pvc && pvc->callback && pvc->cbExpires <= osi_Time()) {
        	afs_VindexFlags[pvc->index] &= ~VC_HAVECB;
        	pvc->dflags |= VC_DIRTY;
        	pvc->callback = 0;
	}

#ifdef LHUSTON
	/* XXX turning on this code greatly helps performance,  but
	 * it doesn't always work right for some reason. I wouldn't 
	 * recommend it for every day work.
	 */

	/*
	 *  if we are in partially connected and have callback, we
	 *  try to use the locally cache copy.
	 */

	if (IS_PARTIAL(discon_state) && pvc->callback) {
		pdc = afs_FindDCache(pvc, 0);
		/*
		 * if we have a dir cached and it is valid,
		 * then the name we used should be valid
		 */
		if ((pdc) && (pvc->m.DataVersion == pdc->f.versionNo)) { 
			afs_PutDCache(pdc);
			return (name);
		} else if (pdc) {
			afs_PutDCache(pdc);
		}
	}

	if (IS_PARTIAL(discon_state)) {
		printf("check_name: going to net \n");

	}
#endif	

	/* Get the current parent dcache */
	pdc = OB_GetDCache(pvc, pfid, 0, reqp, 1);

	if (!pdc) {
		discon_log(DISCON_ERR,
		    "%s: chk name didn't get parent dc <%x %x %x>\n",
		    errname, pfid->Fid.Volume, pfid->Fid.Vnode, 
		    pfid->Fid.Unique);
		return ((char *) 0);
	}

	/* clear out the old directory buffer */
	DZap(&pdc->f.inode);

	/* Ensure the name we are creating in directory with is unique. */

	if (gen_name = get_UniqueName(pdc, name, ext)) {
		struct name_trans *ntrans;
		int idx;

		discon_log(DISCON_ERR, "%s: name conflict, %s renamed to %s \n",
		    errname, name, gen_name);

		ntrans = (struct name_trans *) osi_Alloc(sizeof
		    (struct name_trans));

		idx = allocate_map_ent(&trans_names, sizeof(struct name_trans));

		/* populate the entry */
		ntrans->pfid = pvc->fid;
		ntrans->ntrans_idx = idx;
		ntrans->next_nt_idx = ptransp->nl_idx;
		ntrans->next = ctransp->name_list;

		/* save the name info */
		ntrans->old_name = (char *) osi_Alloc(MAX_NAME);
		strcpy(ntrans->old_name, name);
		ntrans->oname_idx = allocate_name(name);

		ntrans->new_name = (char *) osi_Alloc(MAX_NAME);
		strcpy(ntrans->new_name, gen_name);
		ntrans->nname_idx = allocate_name(gen_name); 

		store_name_translation(ntrans);
	
		/* free memory that get_UniqueName allocates */
		osi_Free(gen_name, MAX_NAME);

		/* set a pointer to the current name */
		gen_name = ntrans->new_name;

		/* Update parent translation info */
		ctransp->name_list = ntrans;
		ctransp->nl_idx = idx;
		store_translation(ctransp);	

		/* Our cached directory is out of date, so mark it bad */
		ptransp->flags |= KNOWNBAD;
	}

	/* put pdc back on free list */
	Free_OBDcache(pdc);

	if (gen_name)
		return gen_name;
	else 
		return name;
}

char *
check_input_name(oldname, trans, pfid)
	char *oldname;
	struct translations *trans;
	struct VenusFid *pfid;
{
	struct name_trans *cur_name;

	cur_name = trans->name_list;
	
	while (cur_name != NULL ) {

		if (!strcmp(cur_name->old_name, oldname) && (!FidCmp(pfid,
		    &cur_name->pfid))) 
			return (cur_name->new_name);
			
		cur_name = cur_name->next;			
	}
	
	return oldname;

}


/* A entry an changes the references from old fid to new fid */

void
recon_update_fid(trans, vcp, oname, nname)
	struct translations *trans;
	struct vcache *vcp;
	char *oname;
	char *nname;
{
	int code = 0;

	/* make sure we have a new fid to rename to */

       	if (!(trans->newfid.Fid.Volume)) {
		/* we should never get here, just print a message */
		printf("recon_update_fid:  no new fid \n");
		return;
	}


	/* change the parent directory entry to reflect the new fid*/
	/* can't hold vcp lock here because this will get afs_xvcache */
	code = fix_dirents(vcp, &trans->newfid, oname, nname);

	ObtainWriteLock(&vcp->lock);

	if(code) {
		discon_log(DISCON_INFO,
		    "Failed to move entry <%x %x %x %x> \n",
		    vcp->fid.Cell, vcp->fid.Fid.Vnode,
		    vcp->fid.Fid.Volume, vcp->fid.Fid.Unique);  
	}

	/* change the dcache to use the new fid */
	code = move_dcache(vcp, &trans->newfid);

	if(code) { 
		discon_log(DISCON_NOTICE,
		   "Update fid: failed to move_dc on %d\n", code);
	}

	
	/* now use move the vcache */

	/* release lock to maintain hierachy */
	ReleaseWriteLock(&vcp->lock);

	ObtainWriteLock(&afs_xvcache);
	ObtainWriteLock(&vcp->lock);
	un_string_vc(vcp);
	vcp->fid = trans->newfid;
	string_vc(vcp);

	ReleaseWriteLock(&afs_xvcache);

	vcp->dflags |= VC_DIRTY;
	ReleaseWriteLock(&vcp->lock);
}



/*
 * update some stats associated with each operation.  Also try
 * to clean up the vcache if we are done operating on it
 * 
 * The vcache needs to be write locked when this is called.
 */

void
recon_update_data(trans, vcp, opno, OutStatus)
	struct translations *trans;
	struct vcache *vcp;
	long opno;
	struct AFSFetchStatus *OutStatus;
{
	int code = 0;
	struct dcache *tdc;

	/* set the translation information */
	trans->validDV = OutStatus->DataVersion; 

	store_translation(trans);

	ObtainWriteLock(&vcp->lock);

        vcp->m.Date = OutStatus->ClientModTime;
        vcp->last_replay_op = opno;

	/* update the fcache version number */
	
	ObtainWriteLock(&afs_xdcache);
	tdc = afs_FindDCache(vcp, 0);
	
	if (tdc) {
		tdc->f.versionNo = OutStatus->DataVersion;

		/* if the last op on the file, clear the KEEP_DC */
        	if (vcp->last_mod_op == opno) {
			tdc->f.dflags &= ~KEEP_DC;
			afs_indexFlags[tdc->index] &= ~IFFKeep_DC;
		}
		tdc->flags |= DFEntryMod;
		afs_PutDCache(tdc);
		/* XXX lhuston do we need to put tdc ? */
	}

	ReleaseWriteLock(&afs_xdcache);
	if (OutStatus) {
		vcp->m.DataVersion = OutStatus->DataVersion;
	}

	/*
	 * If there are more operations, just mark the vcache dirty and
	 * return 
	 */

        if (vcp->last_mod_op != opno) {
        	vcp->dflags |= VC_DIRTY;
		ReleaseWriteLock(&vcp->lock);
		return;
	}
	
	/*
	 * there are not outstanding ops on the file.  If we know
	 * the cached data is bad, then flush it and return
 	 */

	if (trans->flags & KNOWNBAD) {

		remove_translation(trans);
		/* XXX do this for all dcaches */
		ObtainWriteLock(&afs_xdcache);

		tdc = afs_FindDCache(vcp, 0);

		if (tdc) {
			afs_FlushDCache(tdc);
		}

		ReleaseWriteLock(&afs_xdcache);

		ReleaseWriteLock(&vcp->lock);

		/* clear associated flags */
		vcp->dflags &= ~KEEP_VC;
		afs_VindexFlags[vcp->index] &= ~KEEP_VC;

		ObtainWriteLock(&afs_xvcache);
		code = afs_FlushVCache(vcp);
		/* for some reason we failed, so mark invalid and save */
		if (code) {
			vcp->callback = 0;
			vcp->cbExpires = 0;
			vcp->states &= ~CStatd;
			vcp->dflags |= (DBAD_VC|VC_DIRTY);
		}
		ReleaseWriteLock(&afs_xvcache);
		return;
	}


	vcp->m.Owner = OutStatus->Owner;
	vcp->m.Mode = OutStatus->UnixModeBits;
	vcp->m.Group = OutStatus->Group;
	vcp->m.LinkCount = OutStatus->LinkCount;

	if (trans->callback) {
		vcp->callback = trans->callback;
		vcp->cbExpires = trans->cbExpires;
	}

	/* Clear the flags */
	vcp->dflags &= ~KEEP_VC;
	afs_VindexFlags[vcp->index] &= ~KEEP_VC;

	vcp->dflags |= VC_DIRTY;

	remove_translation(trans);
	ReleaseWriteLock(&vcp->lock);
}


/*
 * This attempts to replay the setattr.  The algorithm for this is 
 * simple.  If the data version has changed on the file, then we know that
 * it has been modified, so we abort.
 */


int
reconcile_setattr(log_ent)
	log_ent_t *log_ent;

{
	struct conn *tc;
	struct vcache *tvc;
	struct AFSStoreStatus InStatus;
	struct AFSFetchStatus OutStatus;
	struct AFSStoreStatus astat;
	struct AFSVolSync tsync;
	struct vrequest areq ;
	struct translations *trans;
	struct rx_call *tcall;
	struct  VenusFid *afid;
	char *namestr = "reconcile_setattr";
	long tlen;
	long code;
	
	if (code = replay_init(&log_ent->sa_fid, &tvc, 1, &trans, &afid,
	    namestr))
		return (code);

	if (trans->validDV == -1) 
		trans->validDV = log_ent->sa_origdv;

	if (code = replay_conn_init(afid, &areq, u.u_cred, &tc, namestr))
		return code;

	/* compare the version numbers */

	if (code = recon_compare_dvs(tc, tvc, afid, trans, namestr)) 
		return 0;

	/* we should be able to go ahead and do the setattr now. */

	afs_VAttrToAS(&log_ent->sa_vattr, &astat);

	/* perform any truncation */
	if (log_ent->sa_vattr.va_size != -1) {
		InStatus.Mask = 0;
		do {
			tcall = rx_NewCall(tc->id);
			tlen = log_ent->sa_vattr.va_size;
			code = StartRXAFS_StoreData(tcall, (struct AFSFid *)
			    &afid->Fid, &InStatus, 0, 0, tlen);
			if (code == 0) {
				code = EndRXAFS_StoreData(tcall, &OutStatus,
				    &tsync);
			}
			code = rx_EndCall(tcall, code);
		} while (afs_Analyze(tc, code, afid, &areq));

		/* XXX lhuston debug, if this works we fix */
		/* XXX lhuston this is a hack to fix the problem
		 * when we update data more than once, we give
		 * and older opno.  this should be fixed.
		 */
		recon_update_data(trans, tvc, log_ent->log_opno-1, &OutStatus);
	}

	/* do the rest of the setattr */
	do {
		code = RXAFS_StoreStatus(tc->id, (struct AFSFid *) &afid->Fid, 
		    &astat, &OutStatus, &tsync);
	} while (afs_Analyze(tc, code, afid, &areq));

	if(code) {
	    discon_log(DISCON_ERR,"afs_setattr: failed on %d for <%x %x %x>\n",
		code, afid->Fid.Volume, afid->Fid.Vnode, afid->Fid.Unique);
	    return code;
	}

	recon_update_data(trans, tvc, log_ent->log_opno, &OutStatus);

	discon_log(DISCON_INFO,"afs_setattr: suceeded for <%x %x %x %x>\n",
	    afid->Cell, afid->Fid.Volume, afid->Fid.Vnode, afid->Fid.Unique);

	return code;
}





/*
 *  This is the code that handles the disconnected create.
 *
 *  The different possible case are:
 *      1) the parent directory hasn't changed since we cache the information.
 *      2) the parent directory has changed, but an entry with the same name
 *         was not created.
 *      3) the parent directory has has and entry added with the same name.
 *      4) the parent directory no longer exists.
 *
 */

int
reconcile_create(log_ent)
        log_ent_t *log_ent;


{
	struct conn *tc;
	struct AFSCallBack CallBack;
	struct AFSVolSync tsync;
	long code;
	struct  VenusFid *pfid;
	struct  VenusFid *cfid;
	struct vrequest areq ;
	struct  vcache  *pvc;
	struct  vcache  *cvc;
	char  *fname;
	struct translations *ctrans;
	struct translations *ptrans;
	struct AFSStoreStatus InStatus;
	struct AFSFetchStatus OutFidStatus, OutDirStatus;
	struct VenusFid newFid;
	char *errname = "reconcile_create";

	if (code = replay_init(&log_ent->cr_parentfid, &pvc, 1, &ptrans, &pfid,
	    errname))
		return (code);

	if (code = replay_init(&log_ent->cr_filefid, &cvc, 1,  &ctrans, &cfid,
	    errname))
		return (code);

	if (code = replay_conn_init(pfid, &areq, u.u_cred, &tc, errname))
		return code;

	fname = replay_check_name(log_ent->cr_name, ptrans, ctrans,
		    pvc, pfid, &areq, CREATE_EXT, errname); 
	if (!fname) {
		return ENOENT;
	}


	/* now try to create the file, we do it ourself instead of calling
	 * afs_create.  This is to make it easier to patch up the local cache
	 */ 

	InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE;
	InStatus.ClientModTime = osi_Time();
	/* only care about protection bits */
	InStatus.UnixModeBits = cvc->m.Mode & 0xffff; 

	do {
		/* remember for callback processing */
		code = RXAFS_CreateFile(tc->id, 
		    (struct AFSFid *) &pfid->Fid, fname, &InStatus,
		    (struct AFSFid *) &newFid.Fid, &OutFidStatus,
		    &OutDirStatus, &CallBack, &tsync);

	} while(afs_Analyze(tc, code, pfid, &areq));

	if(code) {
		discon_log(DISCON_ERR,
		    "recon_create: failed to create %s on code %d \n",
		    fname,code);
		return code;
	}

	/* Save the new fid */
	newFid.Cell = pvc->fid.Cell;
	newFid.Fid.Volume = pvc->fid.Fid.Volume;
	ctrans->newfid = newFid;

	/* save the callback info */
	ctrans->callback = tc->server->host;
	ctrans->cbExpires = CallBack.ExpirationTime+osi_Time();

	store_translation(ctrans);

	/* update the cache to use the new fid */
	recon_update_fid(ctrans, cvc, log_ent->cr_name, fname);

	/* these need to be done in the order child, then parent */
	recon_update_data(ctrans, cvc, log_ent->log_opno, &OutFidStatus);
	recon_update_data(ptrans, pvc, log_ent->log_opno, &OutDirStatus);

	discon_log(DISCON_INFO,"reconcile_create: Succesfully created %s \n",
	    fname);

	return 0;

}  /* reconcile_create */





/* This replays the link attempt.  */
int
reconcile_link(log_ent)
	log_ent_t *log_ent;

{

	struct conn *tc;
	struct vcache  *pvc;
	struct vcache *lvc;
	struct AFSVolSync tsync;
	struct vrequest areq ;
	struct VenusFid *pfid;
	struct VenusFid *lfid;
	struct translations *ptrans;
	struct translations *ltrans;
	struct AFSFetchStatus OutFidStatus, OutDirStatus;
	long code;
	char *fname;
	char *errname = "reconcile_link";


	if (code = replay_init(&log_ent->ln_parentfid, &pvc, 1, &ptrans, &pfid,
	    errname))
		return (code);
 
	if (code = replay_init(&log_ent->ln_linkfid, &lvc, 1,  &ltrans, &lfid,
	    errname))

		return (code);

	if (code = replay_conn_init(lfid, &areq, u.u_cred, &tc, errname))
		return code;

	/* find what name to use for the link */
 
	fname = replay_check_name(log_ent->ln_name, ptrans, ltrans, pvc, pfid,
	    &areq, LINK_EXT, errname); 
	if (!fname) {
		return ENOENT;
	}

	/* do the link call to the server */
	do {
		code = RXAFS_Link(tc->id, (struct AFSFid *) &pfid->Fid, 
		    fname, (struct AFSFid *) &lfid->Fid, &OutFidStatus,
		    &OutDirStatus, &tsync);
	} while (afs_Analyze(tc, code, &lvc->fid, &areq));

	if(code) {
		discon_log(DISCON_ERR,
		    "reconcile_link: failed to make link %s on code %d \n",
		    fname, code);
		return code;
	}

	/* if the link fid has changed, then we need to udate it */
       	if (ltrans->newfid.Fid.Volume) {
		/* can't hold vcp lock here because this will get afs_xvcache */
		code = fix_dirents(lvc, &ltrans->newfid, log_ent->ln_name, fname);
		if(code) {
			discon_log(DISCON_INFO, 
			    "Failed to update link entry <%x %x %x %x> \n",
		    	    lvc->fid.Cell, lvc->fid.Fid.Vnode, lvc->fid.Fid.Volume,
			    lvc->fid.Fid.Unique);  
		}

	}

	/* these need to be done in the order child, then parent */
	recon_update_data(ltrans, lvc, log_ent->log_opno, &OutFidStatus);
	recon_update_data(ptrans, pvc, log_ent->log_opno, &OutDirStatus);

	discon_log(DISCON_INFO,
	    "reconcile_link: Successfully created link: %s \n",fname);

	return 0;
}


/*  This is the code that handles the disconnected rmdir.  */
int
reconcile_rmdir(log_ent)
        log_ent_t *log_ent;

{
	struct conn *tc;
	struct AFSVolSync tsync;
	long code;
	struct  VenusFid *pfid;
	struct  VenusFid *rd_fid;
	struct vrequest areq ;
	struct  vcache  *pvc;
	struct  vcache  *rdvc;
	char  *dir_name;
	struct translations *ptrans;
	struct translations *rd_trans;
	struct AFSFetchStatus OutDirStatus;
	char *errname = "reconcile_rmdir";



	if (code = replay_init(&log_ent->rd_parentfid, &pvc, 1, &ptrans, &pfid,
	    errname))
		return (code);

	if (code = replay_init(&log_ent->rd_dirfid, &rdvc, 0, &rd_trans,
	    &rd_fid, errname))
		return (code);

	if (code = replay_conn_init(rd_fid, &areq, u.u_cred, &tc, errname))
		return code;

	dir_name = check_input_name(log_ent->rd_name, rd_trans, pfid);

	/* remove the directory */
    
	do {
		code = RXAFS_RemoveDir(tc->id, (struct AFSFid *) 
		    &pfid->Fid, dir_name, &OutDirStatus, &tsync);
	} while (afs_Analyze(tc, code, &pvc->fid, &areq));


	if((code) && (code != ENOENT) && (code != ENOTEMPTY)) {
		discon_log(DISCON_ERR,
		    "reconcile_rmdir: failed removing %s, code %d \n", dir_name,
		    code);
		return code;
	}

	/* free up the translation associated with the removed dir */
	remove_translation(rd_trans);
	recon_update_data(ptrans, pvc, log_ent->log_opno, &OutDirStatus);

	discon_log(DISCON_INFO,
	    "reconcile_rmdir: successfully removed dir: %s \n",dir_name);
	
	return 0;
}



/* This is the code that handles the disconnected remove.  */

int
reconcile_remove(log_ent)
        log_ent_t *log_ent;

{
	struct conn *tc;
	struct AFSFetchStatus OutStatus;
	struct AFSVolSync tsync;
	long code;
	struct  VenusFid *rm_fid;
	struct  VenusFid *pfid;
	struct vrequest areq ;
	struct  vcache  *pvc;
	struct  vcache  *rmvc;
	char  *name;
	struct translations *ptrans;
	struct translations *rm_trans;
	char *errname = "reconcile_remove";

	if (code = replay_init(&log_ent->rm_parentfid, &pvc, 1, &ptrans, &pfid,
	    errname))
		return (code);

	if (code = replay_init(&log_ent->rm_filefid, &rmvc, 0, &rm_trans,
	    &rm_fid, errname))
		return (code);

	if(rm_trans->validDV == -1)
		rm_trans->validDV = log_ent->rm_origdv;

	if (code = replay_conn_init(rm_fid, &areq, u.u_cred, &tc, errname))
		return code;


	/* determine the correct name to use */
	name = check_input_name(log_ent->rm_name, rm_trans, pfid);


        /* compare the version numbers */
	if (code = recon_compare_dvs(tc, rmvc, rm_fid, rm_trans, errname)) {
	    discon_log(DISCON_INFO,
		"afs_remove: dv's don't match on %s log dv %d \n", 
		    log_ent->rm_name, rm_trans->validDV);
		return 0;
	}


	/* Now lets try to perform the remove */

	do {
		code = RXAFS_RemoveFile(tc->id, (struct AFSFid *) &pfid->Fid,
		    name, &OutStatus, &tsync);
	} while (afs_Analyze(tc, code, pfid, &areq));

	/*
	 * if we get ENOENT, we don't worry about it because the file
	 * was already removed.
	 */
	if((code) && (code != ENOENT)) {
		discon_log(DISCON_ERR,
		    "reconcile_remove: failed to remove %s, on code %d \n",
		    name,code);
		return code;
	}

	/*
	 * free up the translation associated with the removed file
	 * if it was the last operation on the file.
	 */ 

	if ((rmvc) && rmvc->last_mod_op == log_ent->log_opno) {
		remove_translation(rm_trans);
	} 

	recon_update_data(ptrans, pvc, log_ent->log_opno, &OutStatus);

	if (!code)
	    discon_log(DISCON_INFO,"afs_remove: successfully removed %s\n",
	    	name);
	return 0;
}


/* replay the symlink attempt. */

int 
reconcile_symlink(log_ent)
	log_ent_t *log_ent;


{
	struct vcache *pvc;
	struct vcache *svc;

	struct AFSVolSync tsync;
	long code;
	struct  VenusFid *sfid;
	struct  VenusFid newFid;
	struct vrequest areq ;
	struct  VenusFid *pfid;
	char *name;
	char *targetName;
	struct translations *strans;
	struct translations *ptrans;
	AFSStoreStatus InStatus;
	struct AFSFetchStatus OutFidStatus, OutDirStatus;
	struct conn *tc;
	char *errname = "reconcile_symlink";

	if (code = replay_init(&log_ent->sy_parentfid, &pvc, 1, &ptrans, &pfid,
	    errname))
		return (code);

	if (code = replay_init(&log_ent->sy_linkfid, &svc, 1,  &strans, &sfid,
	    errname))
		return (code);

	if (code = replay_conn_init(pfid, &areq, u.u_cred, &tc, errname))
		return code;


	name = log_ent->sy_names;
	targetName = name + strlen(name) + 1;

	/* use the right name */
        name = replay_check_name(log_ent->sy_names, ptrans, strans, pvc,
	    pfid, &areq, SYMLINK_EXT, errname);

	if (!name) {
		return ENOENT;
	}


	InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE;
	InStatus.ClientModTime = osi_Time();
	if (*targetName == '#' || *targetName == '%' || *targetName == '$')
		/* mt pt: null from "." at end */
		InStatus.UnixModeBits = 0644;
	else {
		InStatus.UnixModeBits = 0755;
	}

	do {
		code = RXAFS_Symlink(tc->id, (struct AFSFid *) &pfid->Fid, name,
		    targetName, &InStatus, (struct AFSFid *) &newFid.Fid,
		    &OutFidStatus, &OutDirStatus, &tsync);
	} while(afs_Analyze(tc, code, pfid, &areq));


	if(code) {
		discon_log(DISCON_ERR,
		    "afs_symlink: link for %s, to %s :code %d \n", name,
		    targetName,code);
		return code;
        }

	/* save new fid */

	newFid.Cell = pfid->Cell;
	newFid.Fid.Volume = pfid->Fid.Volume;
	strans->newfid = newFid;
	store_translation(strans);

	/* update the cache to use the new fid */

	/* XXX lhuston, what if we don't find svc ? */
	recon_update_fid(strans, svc, log_ent->sy_names, name);


	/* these need to be done in the order child, then parent */
	/* XXX lhuston, what if we don't find svc ? */
	recon_update_data(strans, svc, log_ent->log_opno, &OutFidStatus);
	recon_update_data(ptrans, pvc, log_ent->log_opno, &OutDirStatus);


	discon_log(DISCON_INFO, "afs_symlink: successfully created %s \n",
	    name);
	return 0;
}



/*
 *  This is the code that hadles the disconnected mkdir.
 *
 *  The different possible case are:
 *      1) the parent directory hasn't changed since we cache the information.
 *      2) the parent directory has changed, but an entry with the same name
 *         was not created.
 *      3) the parent directory has has and entry added with the same name.
 *      4) the parent directory no longer exists.
 *
 */

int
reconcile_mkdir(log_ent)
        log_ent_t *log_ent;

{
	struct conn *tc;
	struct AFSCallBack CallBack;
	struct AFSVolSync tsync;
	long code;
	struct VenusFid *pfid;
	struct VenusFid *mdfid;
	struct VenusFid newFid;
	struct vrequest areq ;
	struct vcache  *pvc;
	struct vcache  *mdvc;
	char  *fname;
	struct translations *ptrans;
	struct translations *mdtrans;
	struct AFSStoreStatus InStatus;
	struct AFSFetchStatus OutFidStatus, OutDirStatus;
	char *errname = "reconcile_mkdir";


	if (code = replay_init(&log_ent->md_parentfid, &pvc, 1, &ptrans, &pfid,
	    errname))
		return (code);

	if (code = replay_init(&log_ent->md_dirfid, &mdvc, 1, &mdtrans, &mdfid,
	    errname))
		return (code);

	if (code = replay_conn_init(pfid, &areq, u.u_cred, &tc, errname))
		return code;

        fname = replay_check_name(log_ent->md_name, ptrans, mdtrans, pvc,
	    pfid, &areq, MKDIR_EXT, errname);
	if (!fname) {
		return ENOENT;
	}


	/* set the instatus bits */

	InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE;
	InStatus.ClientModTime = osi_Time();
	/* only care about protection bits */
	InStatus.UnixModeBits = mdvc->m.Mode & 0xffff; 

	do {
		code = RXAFS_MakeDir(tc->id, (struct AFSFid *) &pfid->Fid, 
		    fname, &InStatus, (struct AFSFid *) &newFid.Fid,
		    &OutFidStatus, &OutDirStatus, &CallBack, &tsync);
	} while(afs_Analyze(tc, code, pfid, &areq));

	if(code) {
		discon_log(DISCON_ERR,
		    "reconcile_mkdir: failed to mkdir %s, on code %d \n",
		    fname, code);
		return code;
	}

	newFid.Cell = pvc->fid.Cell;
	newFid.Fid.Volume = pvc->fid.Fid.Volume;
	/* save the callback info */
	mdtrans->callback = tc->server->host;
	mdtrans->cbExpires = CallBack.ExpirationTime+osi_Time();

	mdtrans->newfid = newFid;
	store_translation(mdtrans);

	/* update the fid used by the cache manager */
	recon_update_fid(mdtrans, mdvc, log_ent->md_name, fname);

	/* these need to be done in the order child, then parent */
	recon_update_data(mdtrans, mdvc, log_ent->log_opno, &OutFidStatus);
	recon_update_data(ptrans, pvc, log_ent->log_opno, &OutDirStatus);

	discon_log(DISCON_INFO,
	    "reconcile_mkdir: Succesfully completed mkdir of %s \n",fname);
	return 0;
}




/*
 * given a save_data structure that corresponds to a close
 * on a write request,  this determines if that file has been
 * modified since we last were connected.  If it has not, then
 * we can replace the file, otherwise we make a copy of
 * our modified version and save it int filename.dis
 */

int
reconcile_store(log_ent)
        log_ent_t *log_ent;

{
	struct conn *tc;
	struct vcache *svc;
	struct VenusFid newFid;
	struct AFSFetchStatus OutStatus;
	struct AFSCallBack CallBack;
	struct AFSVolSync tsync;
	long code;
	struct  VenusFid *sfid;
	struct vrequest areq ;
	struct  vcache  *pvc;
	struct  VenusFid *pfid;
	struct  VenusFid lpfid;
	struct dcache *tdc;
	char *fname = 0;
	char *newname = 0;
	struct AFSStoreStatus InStatus;
	struct AFSFetchStatus OutFidStatus, OutDirStatus;
	struct translations *strans;
	struct translations *ptrans;
	char *errname = "reconcile_store";	
	long index;
	int hash;
	struct rx_call *tcall;
	long base;
	struct osi_file *tfile;
	long tlen;
	int	updated = 0;



	if (code = replay_init(&log_ent->st_fid, &svc, 1, &strans, &sfid,
	    errname))
		return (code);

	if(strans->validDV == -1)
		strans->validDV = log_ent->st_origdv;

	if (code = replay_conn_init(sfid, &areq, u.u_cred, &tc, errname))
		return code;

	/* compare the version numbers */

	if (code = recon_compare_dvs(tc, svc, sfid, strans, errname)) {

		/*
		 * we need to find the parent vcache so that we can
		 * look up the name of the file we are trying to write.
		 */

		lpfid.Cell = sfid->Cell;
		lpfid.Fid.Volume = sfid->Fid.Volume;
		lpfid.Fid.Vnode = svc->parentVnode;
		lpfid.Fid.Unique = svc->parentUnique;


		if (code = replay_init(&lpfid, &pvc, 1, &ptrans, &pfid,
		    errname))
			return (code);
	

		/* find the current file name */	
		fname = (char *) osi_Alloc(MAX_NAME);
		code = determine_file_name(svc, fname);

		/* if we couln't find the name, then print message and exit */
		/* XXX maybe should do orphanage thing */

		if (code) {
			discon_log(DISCON_ERR,
			    "reconcile_store: can't find name  <%x %x %x>\n",
			    lpfid.Fid.Volume, lpfid.Fid.Vnode, 
			    lpfid.Fid.Unique);
			osi_Free(fname, MAX_NAME);
			return code;	
		}

		/* get a new name to use */
       		newname = replay_check_name(fname, ptrans, strans, pvc, pfid,
		    &areq, STORE_EXT, errname);

		if (!newname) {
			return ENOENT;
		}

		/* now create the new file */

		/*
		 * now try to create the file, we do it ourself instead 
		 * of calling afs_create.  This is to make it easier to 
		 * patch up the local cache
		 */ 

		InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE;
		InStatus.ClientModTime = osi_Time();
		/* only care about protection bits */
		InStatus.UnixModeBits = svc->m.Mode & 0xffff; 

		do {
			code = RXAFS_CreateFile(tc->id, (struct AFSFid *) 
			    &pfid->Fid, newname, &InStatus, (struct AFSFid *)
			    &newFid.Fid, &OutFidStatus, &OutDirStatus,
			    &CallBack, &tsync);
		} while(afs_Analyze(tc, code, pfid, &areq));


		if(code) {
			discon_log(DISCON_ERR,
			    "recon_store: failed to create %s on code %d \n",
			    newname, code);
			return code;
		}

		/* Save the new fid */
		newFid.Cell = pvc->fid.Cell;
		newFid.Fid.Volume = pvc->fid.Fid.Volume;
		strans->newfid = newFid;
		strans->validDV = OutFidStatus.DataVersion; 
		/* save the callback info */
		strans->callback = tc->server->host;
		strans->cbExpires = CallBack.ExpirationTime+osi_Time();

		store_translation(strans);

		/* update the fid used by the cache manager */
		recon_update_fid(strans, svc, fname, newname);

		/* set sfid to the new fid */
		sfid = &strans->newfid;
		osi_Free(fname, MAX_NAME);
	
	}

	/* Now store the data */
	ObtainWriteLock(&afs_xdcache);

	hash = DVHash(&svc->fid);

	/* Find all dcaches associated with the fid */

	for (index = afs_dvhashTbl[hash]; index != NULLIDX;) {
		tdc = afs_GetDSlot(index, 0);
		/* XXX maybe we should cont if data not dirty */
		if (FidCmp(&tdc->f.fid, &svc->fid)) {
			index = tdc->f.hvNextp;	
			lockedPutDCache(tdc);
			continue;
		}
			

		/* get the dcaches */
	
		base = AFS_CHUNKTOBASE(tdc->f.chunk);
		tlen = svc->m.Length;

		ReleaseWriteLock(&afs_xdcache);

		do {
			updated = 1;
			tfile = afs_CFileOpen(&tdc->f);
			if (!tfile)
				panic("bad inode in store");

			InStatus.Mask = AFS_SETMODTIME;
			InStatus.ClientModTime = osi_Time();

			tcall = rx_NewCall(tc->id);
			code = StartRXAFS_StoreData(tcall, (struct AFSFid *)
			    &sfid->Fid, &InStatus, base, tdc->f.chunkBytes, 
			    tlen);

			if (code == 0) {
				/* transfer the file */
				code = CacheStoreProc(tcall, tfile, tdc, svc);
			}

			if (code == 0)
				code = EndRXAFS_StoreData(tcall, &OutStatus,
				    &tsync);

			code = rx_EndCall(tcall, code);
			osi_Close(tfile);
	
		} while (afs_Analyze(tc, code, sfid, &areq));

		ObtainWriteLock(&afs_xdcache);

		index = tdc->f.hvNextp;	
		lockedPutDCache(tdc);
		if (code)
			break;
	}
	
	ReleaseWriteLock(&afs_xdcache);

	if(code) {
		discon_log(DISCON_ERR,
		    "reconcile_store: failed to store new file %d \n", code);
		discon_log(DISCON_ERR,
		    "reconcile_store: failed fid <%x %x %x %x> \n",
		    sfid->Cell, sfid->Fid.Volume, sfid->Fid.Vnode,
		    sfid->Fid.Unique);

#ifndef LHUSTON
		return code;
#else
		/* XXX this needs to be re written */
		return save_lost_data(svc, &areq);
#endif
	}


	/* XXX lhuston this isn't quite right */
	if (updated) 
		recon_update_data(strans, svc, log_ent->log_opno, &OutStatus);

        /*  set the fields of the translation structure. XXX lhuston huh? */


	discon_log(DISCON_INFO,
	    "reconcile_store: Succesfully completed store <%x %x %x %x> \n",
		svc->fid.Cell, svc->fid.Fid.Volume, svc->fid.Fid.Vnode,
		svc->fid.Fid.Unique);
        return 0;
}



int
reconcile_rename(log_ent)
        log_ent_t *log_ent;

{
	struct conn *tc;
	struct AFSVolSync tsync;
	long code;
	struct  VenusFid *spfid;
	struct  VenusFid *dpfid;
	struct  VenusFid *destfid;
	struct  VenusFid *rename_fid;
	struct vrequest areq ;
	struct  vcache  *spvc;
	struct  vcache  *dpvc;
	struct  vcache  *destvc;
	struct translations *sptrans;
	struct translations *dptrans;
	struct translations *rename_trans;
	struct translations *desttrans;
	struct AFSFetchStatus OutOldDirStatus, OutNewDirStatus;
	char *newname;
	char *oldname;
	char *errname = "reconcile_rename";


	if (code = replay_init(&log_ent->rn_oparentfid, &spvc, 1, &sptrans,
	    &spfid, errname))
		return (code);

	if (code = replay_init(&log_ent->rn_nparentfid, &dpvc, 1, &dptrans,
	    &dpfid, errname))
		return (code);

	if (code = replay_conn_init(dpfid, &areq, u.u_cred, &tc, errname))
		return code;

    	oldname = log_ent->rn_names;
 	newname = oldname + strlen(oldname) + 1;


	/*
	 * Get the renamed fid information to see if there is a 
	 * name translation.  If so use it for the orig name
	 */

	rename_fid = &log_ent->rn_renamefid;
	rename_trans = get_translation(rename_fid);
	oldname = check_input_name(oldname, rename_trans, spfid);

	/* 
	 * look to see if there originally was a dest file which we blew
	 * away.  If there was make sure it the is same version.  If not
	 * make sure we aren't going to erase one if we do it now.
	 */


	if(log_ent->rn_overfid.Fid.Volume == 0) {
		newname = replay_check_name(newname, dptrans, rename_trans,
		    dpvc, dpfid, &areq, RENAME_EXT, errname);
		if (!newname) 
			return ENOENT;
	} else {
		/*
		 * there was a file in the rename destination.  We need
		 * to make sure the file hasn't been modified before we
		 * go ahead with the rename.  If it was then we can't do
		 * the rename since it would violate consistancy.
		 */

		if (code = replay_init(&log_ent->rn_overfid, &destvc, 1,
		    &desttrans, &destfid, errname))
			return (code);

        	if(desttrans->validDV == -1) 
			desttrans->validDV = log_ent->rn_overdv;

		if (code = recon_compare_dvs(tc, destvc, destfid, desttrans,
		    errname)) {
			/* find what name to use for the link */

			/* handle zero return code */
			/* XXX lhuston , should I pass rename_trans instead? */
			newname = replay_check_name(newname, dptrans,
			    desttrans, dpvc, dpfid, &areq, RENAME_EXT, errname);
			if (!newname) {
				return ENOENT;
			}
		}
	}

	do {
		code = RXAFS_Rename(tc->id, 
		    (struct AFSFid *) &spfid->Fid, oldname,
		    (struct AFSFid *) &dpfid->Fid, newname,
		    &OutOldDirStatus, &OutNewDirStatus, &tsync);

	} while (afs_Analyze(tc, code, &dpvc->fid, &areq));


	if(code) {
		discon_log(DISCON_ERR,
		   "reconcile_rename: failed to rename %s to %s, on code %d \n",
		    oldname, newname, code);
		if (code != ENOENT)
			return code;
	}

	/* these need to be done in the order child, then parent */
	/* XXX lhuston make sure these aren't the same vcaches */
	recon_update_data(sptrans, spvc, log_ent->log_opno, &OutOldDirStatus);

	if (spvc != dpvc) 
		recon_update_data(dptrans, dpvc, log_ent->log_opno, 
		    &OutNewDirStatus);

	discon_log(DISCON_INFO,
	    "reconcile_rename: succeeded renaming %s to %s\n", oldname,
	    newname,code);

	return 0;
}


/*
This copyright pertains to modifications set off by the compile
option DISCONN
****************************************************************************
*Copyright 1993 Regents of The University of Michigan - All Rights Reserved*
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appears in all copies and that  *
* both that copyright notice and this permission notice appear in          *
* supporting documentation, and that the name of The University of Michigan*
* not be used in advertising or publicity pertaining to distribution of    *
* the software without specific, written prior permission. This software   *
* is supplied as is without expressed or implied warranties of any kind.   *
*                                                                          *
*            Center for Information Technology Integration                 *
*                  Information Technology Division                         *
*                     The University of Michigan                           *
*                       535 W. William Street                              *
*                        Ann Arbor, Michigan                               *
*                            313-763-4403                                  *
*                        info@citi.umich.edu                               *
*                                                                          *
****************************************************************************
*/



#endif /* DISCONN */
