#ifndef _DISCON_H
#define _DISCON_H

#ifndef AFS_DISCON_ENV
#define AFS_IS_DISCONNECTED 0
#define AFS_IS_LOGGING 0
#define AFS_IS_DISCON_RW 0
#define AFS_IN_SYNC 0
#define AFS_DISCON_LOCK()
#define AFS_DISCON_UNLOCK()

#define AFS_DISCON_ADD_DIRTY(avc, lock)

#else

extern afs_int32    afs_is_disconnected;
extern afs_int32    afs_is_logging;
extern afs_int32    afs_is_discon_rw;
extern afs_int32    afs_in_sync;
extern afs_rwlock_t afs_discon_lock;

extern struct vcache *afs_DDirtyVCList;
extern struct vcache *afs_DDirtyVCListStart;
extern struct vcache *afs_DDirtyVCListPrev;
extern afs_rwlock_t afs_DDirtyVCListLock;
extern afs_int32 afs_ConflictPolicy;

extern void afs_RemoveAllConns();
extern afs_uint32 afs_DisconVnode; /* XXX: not protected. */

/* For afs_GenFakeFid. */
extern struct vcache *afs_FindVCache(struct VenusFid *afid,
					afs_int32 *retry,
					afs_int32 flag);

extern int afs_WriteVCacheDiscon(register struct vcache *avc,
					register struct AFSStoreStatus *astatus,
					struct vattr *attrs);
extern int afs_ResyncDisconFiles(struct vrequest *areq,
					struct AFS_UCRED *acred);
extern void afs_RemoveAllConns();
extern void afs_GenFakeFid(struct VenusFid *afid, afs_uint32 avtype);
extern void afs_GenShadowFid(struct VenusFid *afid);
extern void afs_GenDisconStatus(struct vcache *adp,
					struct vcache *avc,
					struct VenusFid *afid,
					struct vattr *attrs,
					struct vrequest *areq,
					int file_type);
extern int afs_HashOutDCache(struct dcache *adc, int zap);
extern int afs_MakeShadowDir(struct vcache *avc);
extern void afs_DeleteShadowDir(struct vcache *avc);
extern struct dcache *afs_FindDCacheByFid(register struct VenusFid *afid);
extern void afs_UpdateStatus(struct vcache *avc,
					struct VenusFid *afid,
					struct vrequest *areq,
					struct AFSFetchStatus *Outsp,
					struct AFSCallBack *acb,
					afs_uint32 start);
extern void afs_RemoveAllConns();

#define AFS_IS_DISCONNECTED (afs_is_disconnected)
#define AFS_IS_LOGGING (afs_is_logging)
#define AFS_IS_DISCON_RW (afs_is_discon_rw)
#define AFS_IN_SYNC (afs_in_sync)
#define AFS_DISCON_LOCK() ObtainReadLock(&afs_discon_lock)
#define AFS_DISCON_UNLOCK() ReleaseReadLock(&afs_discon_lock)

/* Call with avc and afs_DDirtyVCListLock w locks held. */
#define AFS_DISCON_ADD_DIRTY(avc, lock)				\
do {								\
    int retry = 0;						\
    if (!afs_DDirtyVCListStart) {				\
    	afs_DDirtyVCListStart = afs_DDirtyVCList = avc;		\
    } else {							\
    	afs_DDirtyVCList->ddirty_next = avc;			\
	afs_DDirtyVCList = avc;					\
    }								\
    if (lock)							\
	ObtainWriteLock(&afs_xvcache, 763);			\
    osi_vnhold(avc, 0);						\
    if (lock)							\
	ReleaseWriteLock(&afs_xvcache);				\
} while(0);

#endif /* AFS_DISCON_ENV */
#endif /* _DISCON_H */
