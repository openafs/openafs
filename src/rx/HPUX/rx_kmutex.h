/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * HPUX implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#if defined(AFS_HPUX110_ENV) && defined(KERNEL)
/* rx fine grain locking primitives */

#include <sys/ksleep.h>
#include <sys/spinlock.h>
#include <sys/sem_sync.h>
#include <sys/errno.h>
#include <net/netmp.h>

#define RX_ENABLE_LOCKS         1
#define AFS_GLOBAL_RXLOCK_KERNEL
extern lock_t*  rx_sleepLock;

#define CV_INIT(cv,a,b,c) 

/* this is supposed to atomically drop the mutex and go to sleep */
/* reacquire the mutex when the sleep is done */
#define CV_WAIT(cv, lck) {                                                \
                          int code;                                       \
                          ksleep_prepare();                               \
                          MP_SPINLOCK(rx_sleepLock);                      \
                          if (valusync(lck) < 1)                          \
                                vsync(lck);                               \
                          else                                            \
                                osi_Panic("mutex not held %d",valusync(lck)); \
                          code=ksleep_one(KERNEL_ADDRESS|KERN_SPINLOCK_OBJECT,\
                                        (cv),rx_sleepLock,0);             \
                          if ( code ) {                                   \
                              if ( code == EINTR ) {/* lock still held */ \
                                  MP_SPINUNLOCK(rx_sleepLock);            \
                              } else if (code != -EINTR) {                \
                                  osi_Panic("ksleep_one failed %d", code);\
			      }                                           \
                          }                                               \
                          psync(lck); /* grab the mutex again */          \
                        }

/* Wakes up one thread waiting on this condition */
#define CV_SIGNAL(cv)    {                                                \
                           int wo, code;                                  \
                           MP_SPINLOCK(rx_sleepLock);                     \
                           if( (code = kwakeup_one(KERNEL_ADDRESS,(cv),   \
                                                  WAKEUP_ONE, &wo))<0)    \
                                osi_Panic("kwakeup_one failed %d", code); \
                           MP_SPINUNLOCK(rx_sleepLock);                   \
                         }

/* wakeup all threads waiting on this condition */
#define CV_BROADCAST(cv) {                                                \
                           int wo, code;                                  \
                           MP_SPINLOCK(rx_sleepLock);                     \
                           if( (code = kwakeup_one(KERNEL_ADDRESS,(cv),   \
                                                 WAKEUP_ALL, &wo))<0)     \
                                osi_Panic("kwakeup_all failed %d", code); \
                           MP_SPINUNLOCK(rx_sleepLock);                   \
                         }

#define CV_DESTROY(a)

typedef sync_t  afs_kmutex_t;
typedef caddr_t afs_kcondvar_t;

#else /* AFS_HPUX110_ENV */

#if defined(AFS_HPUX102_ENV)
#define CV_INIT(a,b,c,d)
#define CV_DESTROY(a)
#endif /* AFS_HPUX102_ENV */
#endif /* else AFS_HPUX110_ENV */

#ifdef AFS_HPUX102_ENV

#define RXObtainWriteLock(a) AFS_ASSERT_RXGLOCK()
#define RXReleaseWriteLock(a)


#if defined(AFS_HPUX110_ENV) 
#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t *lockaddr, char *msg);

#define MUTEX_DESTROY(a)        ( dealloc_spinlock((a)->s_lock) )
#define MUTEX_ENTER(a)          psync(a)
#define MUTEX_TRYENTER(a)       ( (valusync(a)==1)? (MUTEX_ENTER(a), 1):0 )
#if 0
#define MUTEX_EXIT(a)           ((valusync(a)<1)? vsync(a) : \
                                                osi_Panic("mutex not held"))
#endif
#define MUTEX_EXIT(a)           vsync(a)
#define MUTEX_INIT(a,b,c,d)     (initsync(a), vsync(a))

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a)         ( (valusync(a)<1) ? 1 : 0 ) 

#else /* AFS_HPUX110_ENV */

#define osirx_AssertMine(addr, msg)

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)  
#define MUTEX_INIT(a,b,c,d) 

#endif /* else AFS_HPUX110_ENV */
#endif /* AFS_HPUX102_ENV */
#endif /* _RX_KMUTEX_H_ */

