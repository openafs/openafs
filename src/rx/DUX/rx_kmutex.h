/* Copyright Transarc Corporation 1998 - All Rights Reserved
 *
 * rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * DUX implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#ifdef AFS_DUX40_ENV

#include <kern/lock.h>
#include <kern/sched_prim.h>
#include <sys/unix_defs.h>

#define RX_ENABLE_LOCKS		1
#define AFS_GLOBAL_RXLOCK_KERNEL

/*
 * Condition variables
 *
 * In Digital Unix (OSF/1), we use something akin to the ancient sleep/wakeup
 * mechanism.  The condition variable itself plays no role; we just use its
 * address as a convenient unique number.
 */
#define CV_INIT(cv,a,b,c)
#define CV_DESTROY(cv)
#define CV_WAIT(cv, lck)    { \
				int isGlockOwner = ISAFS_GLOCK(); \
				if (isGlockOwner) AFS_GUNLOCK();  \
				assert_wait((vm_offset_t)(cv), 0);	\
				MUTEX_EXIT(lck);	\
				thread_block();		\
				if (isGlockOwner) AFS_GLOCK();  \
				MUTEX_ENTER(lck); \
			    }

#define CV_TIMEDWAIT(cv,lck,t)	{ \
				int isGlockOwner = ISAFS_GLOCK(); \
				if (isGlockOwner) AFS_GUNLOCK();  \
				assert_wait((vm_offset_t)(cv), 0);	\
				thread_set_timeout(t);	\
				MUTEX_EXIT(lck);	\
				thread_block();		\
				if (isGlockOwner) AFS_GLOCK();  \
				MUTEX_ENTER(lck);	\

#define CV_SIGNAL(cv)		thread_wakeup_one((vm_offset_t)(cv))
#define CV_BROADCAST(cv)	thread_wakeup((vm_offset_t)(cv))

typedef struct {
    simple_lock_data_t lock;
    thread_t owner;
} afs_kmutex_t;
typedef int afs_kcondvar_t;

#define osi_rxWakeup(cv)	thread_wakeup((vm_offset_t)(cv))

#define LOCK_INIT(a,b) \
    do { \
	usimple_lock_init(&(a)->lock); \
	(a)->owner = (thread_t)0; \
    } while(0);
#define MUTEX_INIT(a,b,c,d) \
    do { \
	usimple_lock_init(&(a)->lock); \
	(a)->owner = (thread_t)0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	usimple_lock_terminate(&(a)->lock); \
	(a)->owner = (thread_t)-1; \
    } while(0);
#define MUTEX_ENTER(a) \
    do { \
	usimple_lock(&(a)->lock); \
	osi_Assert((a)->owner == (thread_t)0); \
	(a)->owner = current_thread(); \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( usimple_lock_try(&(a)->lock) ? ((a)->owner = current_thread(), 1) : 0 )
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == current_thread()); \
	(a)->owner = (thread_t)0; \
	usimple_unlock(&(a)->lock); \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a)	(((afs_kmutex_t *)(a))->owner == current_thread())

#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t *lockaddr, char *msg);

#endif	/* DUX40 */


#endif /* _RX_KMUTEX_H_ */

