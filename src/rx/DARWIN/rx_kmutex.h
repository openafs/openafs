/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * MACOS implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#ifdef AFS_DARWIN80_ENV
#include <kern/locks.h>
#else
#include <sys/lock.h>
#endif
#include <kern/thread.h>
#include <sys/vm.h>

#define RX_ENABLE_LOCKS         1
#define AFS_GLOBAL_RXLOCK_KERNEL

/*
 * Condition variables
 *
 * In Digital Unix (OSF/1), we use something akin to the ancient sleep/wakeup
 * mechanism.  The condition variable itself plays no role; we just use its
 * address as a convenient unique number.
 * 
 * XXX in darwin, both mach and bsd facilities are available. Should really
 * stick to one or the other (but mach locks don't have a _try.....)
 *
 * in darwin 8.0, the bsd lock facility is no longer available, and only one
 * sleep variant is available. Still no lock_try, but we can work around that.
 * We can't pass the mutex into msleep, even if we didn't need the two mutex 
 * hack for lock_try emulation, since msleep won't fixup the owner variable
 * and we'll panic.
 */
#define CV_INIT(cv,a,b,c)
#define CV_DESTROY(cv)
#ifdef AFS_DARWIN14_ENV
#ifdef AFS_DARWIN80_ENV
#define CV_WAIT(cv, lck)    do { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
				osi_Assert((lck)->owner == current_thread()); \
				(lck)->owner = (thread_t)0; \
				lck_mtx_lock((lck)->meta); \
				(lck)->waiters--; \
				lck_mtx_unlock((lck)->meta); \
                                msleep(cv, (lck)->lock, PDROP|PVFS, "afs_CV_WAIT", NULL); \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck); \
	                    } while(0)

#define CV_TIMEDWAIT(cv,lck,t)  do { \
	                        struct timespec ts; \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        ts.ts_sec = t; \
	                        ts.ts_nsec = 0; \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
				osi_Assert((lck)->owner == current_thread()); \
				(lck)->owner = (thread_t)0; \
				lck_mtx_lock((lck)->meta); \
				(lck)->waiters--; \
				lck_mtx_unlock((lck)->meta); \
                                msleep(cv, (lck)->lock, PDROP|PVFS, "afs_CV_TIMEDWAIT", &ts); \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck);       \
                            } while(0)
#else
#define CV_WAIT(cv, lck)    do { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        MUTEX_EXIT(lck);        \
	                        sleep(cv, PVFS);                \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck); \
	                    } while(0)

#define CV_TIMEDWAIT(cv,lck,t)  do { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        MUTEX_EXIT(lck);        \
	                        tsleep(cv,PVFS, "afs_CV_TIMEDWAIT",t);  \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck);       \
                            } while(0)
#endif
#define CV_SIGNAL(cv)           wakeup_one((void *)(cv))
#define CV_BROADCAST(cv)        wakeup((void *)(cv))
#else
#define CV_WAIT(cv, lck)    { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        assert_wait((event_t)(cv), 0);  \
	                        MUTEX_EXIT(lck);        \
	                        thread_block(0);                \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck); \
	                    }

#define CV_TIMEDWAIT(cv,lck,t)  { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        assert_wait((event_t)(cv), 0);  \
	                        thread_set_timer(t, NSEC_PER_SEC/hz);   \
	                        MUTEX_EXIT(lck);        \
	                        thread_block(0);                \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck);       \
				}

#define CV_SIGNAL(cv)           thread_wakeup_one((event_t)(cv))
#define CV_BROADCAST(cv)        thread_wakeup((event_t)(cv))
#endif

#ifdef AFS_DARWIN80_ENV
typedef struct {
    lck_mtx_t *meta;
    int waiters; /* also includes anyone holding the lock */
    lck_mtx_t *lock;
    thread_t owner;
} afs_kmutex_t;
typedef int afs_kcondvar_t;

extern lck_grp_t * openafs_lck_grp;

#define MUTEX_SETUP() rx_kmutex_setup()
#define MUTEX_FINISH() rx_kmutex_finish()
#define LOCKINIT(a) \
    do { \
        lck_attr_t *openafs_lck_attr = lck_attr_alloc_init(); \
        (a) = lck_mtx_alloc_init(openafs_lck_grp, openafs_lck_attr); \
        lck_attr_free(openafs_lck_attr); \
    } while(0)
#define MUTEX_INIT(a,b,c,d) \
    do { \
        lck_attr_t *openafs_lck_attr = lck_attr_alloc_init(); \
        (a)->meta = lck_mtx_alloc_init(openafs_lck_grp, openafs_lck_attr); \
        (a)->lock = lck_mtx_alloc_init(openafs_lck_grp, openafs_lck_attr); \
        lck_attr_free(openafs_lck_attr); \
	(a)->waiters = 0; \
	(a)->owner = (thread_t)0; \
    } while(0)
#define MUTEX_DESTROY(a) \
    do { \
        lck_mtx_destroy((a)->lock, openafs_lck_grp); \
        lck_mtx_destroy((a)->meta, openafs_lck_grp); \
	(a)->owner = (thread_t)-1; \
    } while(0)
#define MUTEX_ENTER(a) \
    do { \
	lck_mtx_lock((a)->meta); \
	(a)->waiters++; \
	lck_mtx_unlock((a)->meta); \
	lck_mtx_lock((a)->lock); \
	osi_Assert((a)->owner == (thread_t)0); \
	(a)->owner = current_thread(); \
    } while(0)

/* acquire main lock before releasing meta lock, so we don't race */
#define MUTEX_TRYENTER(a) ({ \
    int _ret; \
    lck_mtx_lock((a)->meta); \
    if ((a)->waiters) { \
       lck_mtx_unlock((a)->meta); \
       _ret = 0; \
    } else { \
       (a)->waiters++; \
       lck_mtx_lock((a)->lock); \
       lck_mtx_unlock((a)->meta); \
       osi_Assert((a)->owner == (thread_t)0); \
       (a)->owner = current_thread(); \
       _ret = 1; \
    } \
    _ret; \
})

#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == current_thread()); \
	(a)->owner = (thread_t)0; \
	lck_mtx_unlock((a)->lock); \
	lck_mtx_lock((a)->meta); \
	(a)->waiters--; \
	lck_mtx_unlock((a)->meta); \
    } while(0)

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == current_thread())
#else
typedef struct {
    struct lock__bsd__ lock;
    thread_t owner;
} afs_kmutex_t;
typedef int afs_kcondvar_t;

#define LOCK_INIT(a,b) \
    do { \
	lockinit(&(a)->lock,PSOCK, "afs rx lock", 0, 0); \
	(a)->owner = (thread_t)0; \
    } while(0);
#define MUTEX_INIT(a,b,c,d) \
    do { \
	lockinit(&(a)->lock,PSOCK, "afs rx mutex", 0, 0); \
	(a)->owner = (thread_t)0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (thread_t)-1; \
    } while(0);
#define MUTEX_ENTER(a) \
    do { \
	lockmgr(&(a)->lock, LK_EXCLUSIVE, 0, current_proc()); \
	osi_Assert((a)->owner == (thread_t)0); \
	(a)->owner = current_thread(); \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( lockmgr(&(a)->lock, LK_EXCLUSIVE|LK_NOWAIT, 0, current_proc()) ? 0 : ((a)->owner = current_thread(), 1) )
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == current_thread()); \
	(a)->owner = (thread_t)0; \
	lockmgr(&(a)->lock, LK_RELEASE, 0, current_proc()); \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == current_thread())
#endif

#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t * lockaddr, char *msg);

#endif /* _RX_KMUTEX_H_ */
