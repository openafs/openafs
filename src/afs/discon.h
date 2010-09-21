#ifndef _DISCON_H
#define _DISCON_H

#if !defined(inline) && !defined(__GNUC__)
#define inline
#endif

extern afs_int32    afs_is_disconnected;
extern afs_int32    afs_is_discon_rw;
extern afs_int32    afs_in_sync;
extern afs_rwlock_t afs_discon_lock;

extern struct afs_q afs_disconDirty;
extern struct afs_q afs_disconShadow;
extern afs_rwlock_t afs_disconDirtyLock;
extern afs_int32    afs_ConflictPolicy;

extern afs_uint32 afs_DisconVnode; /* XXX: not protected. */

extern int afs_WriteVCacheDiscon(struct vcache *avc,
					struct AFSStoreStatus *astatus,
					struct vattr *attrs);
extern int afs_ResyncDisconFiles(struct vrequest *areq,
					afs_ucred_t *acred);
extern void afs_RemoveAllConns(void);
extern void afs_GenFakeFid(struct VenusFid *afid, afs_uint32 avtype,
			   int lock);
extern void afs_GenShadowFid(struct VenusFid *afid);
extern void afs_GenDisconStatus(struct vcache *adp,
					struct vcache *avc,
					struct VenusFid *afid,
					struct vattr *attrs,
					struct vrequest *areq,
					int file_type);
extern int afs_MakeShadowDir(struct vcache *avc, struct dcache *adc);
extern void afs_DeleteShadowDir(struct vcache *avc);
extern struct dcache *afs_FindDCacheByFid(struct VenusFid *afid);
extern void afs_UpdateStatus(struct vcache *avc,
					struct VenusFid *afid,
					struct vrequest *areq,
					struct AFSFetchStatus *Outsp,
					struct AFSCallBack *acb,
					afs_uint32 start);
extern void afs_DisconDiscardAll(afs_ucred_t *);

#define AFS_IS_DISCONNECTED (afs_is_disconnected)
#define AFS_IS_DISCON_RW (afs_is_discon_rw)
#define AFS_IN_SYNC (afs_in_sync)
#define AFS_DISCON_LOCK() ObtainReadLock(&afs_discon_lock)
#define AFS_DISCON_UNLOCK() ReleaseReadLock(&afs_discon_lock)

/* Call with avc lock held */
static_inline void afs_DisconAddDirty(struct vcache *avc, int operation, int lock) {
    if (!avc->f.ddirty_flags) {
	if (lock)
	    ObtainWriteLock(&afs_xvcache, 702);
	ObtainWriteLock(&afs_disconDirtyLock, 703);
	QAdd(&afs_disconDirty, &avc->dirtyq);
	osi_Assert((afs_RefVCache(avc) == 0));
	ReleaseWriteLock(&afs_disconDirtyLock);
	if (lock)
	    ReleaseWriteLock(&afs_xvcache);
    }
    avc->f.ddirty_flags |= operation;
}

/* Call with avc lock held */
static_inline void afs_DisconRemoveDirty(struct vcache *avc) {
    ObtainWriteLock(&afs_disconDirtyLock, 704);
    QRemove(&avc->dirtyq);
    ReleaseWriteLock(&afs_disconDirtyLock);
    avc->f.ddirty_flags = 0;
    afs_PutVCache(avc);
}
#endif /* _DISCON_H */
