#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_
#include <sys/spinlock.h>
#include <sys/sem_sync.h>
#include <sys/ksleep.h>
#include <net/netmp.h>

#define RX_ENABLE_LOCKS         1
extern lock_t*  rx_sleepLock;
#define AFS_GLOBAL_RXLOCK_KERNEL

#define CV_INIT(cv,a,b,c)
#define CV_DESTROY(a)

/* These 3, at least, need to do something */
#define CV_WAIT(cv, lck) {                                                \
                        }

#define CV_SIGNAL(cv)    {                                                \
                         }

#define CV_BROADCAST(cv) {                                                \
                         }

typedef sync_t  afs_kmutex_t;
typedef caddr_t afs_kcondvar_t;

#define RXObtainWriteLock(a) 
#define RXReleaseWriteLock(a)

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)  
#define MUTEX_INIT(a,b,c,d) 
#define MUTEX_ISMINE(a) (1)
#endif
