#ifndef _DISCON_H
#define _DISCON_H

#ifndef AFS_DISCON_ENV
#define AFS_IS_DISCONNECTED 0
#define AFS_IS_LOGGING 0
#define AFS_DISCON_LOCK()
#define AFS_DISCON_UNLOCK()

#else

extern afs_int32    afs_is_disconnected;
extern afs_int32    afs_is_logging;
extern afs_rwlock_t afs_discon_lock;

#define AFS_IS_DISCONNECTED (afs_is_disconnected)
#define AFS_IS_LOGGING (afs_is_logging)
#define AFS_DISCON_LOCK() ObtainReadLock(&afs_discon_lock)
#define AFS_DISCON_UNLOCK() ReleaseReadLock(&afs_discon_lock)

#endif /* AFS_DISCON_ENV */
#endif /* _DISCON_H */
