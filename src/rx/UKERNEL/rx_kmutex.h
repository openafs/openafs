/* Copyright Transarc Corporation 1998 - All Rights Reserved
 *
 * rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * Solaris implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#define AFS_GLOBAL_RXLOCK_KERNEL
#define RX_ENABLE_LOCKS		1

#define	afs_kmutex_t		usr_mutex_t
#define	afs_kcondvar_t		usr_cond_t
#define MUTEX_INIT(A,B,C,D)	usr_mutex_init(A)
#define MUTEX_ENTER(A)		usr_mutex_lock(A)
#define MUTEX_TRYENTER(A)	usr_mutex_trylock(A)
#define MUTEX_ISMINE(A)		(1)
#define MUTEX_EXIT(A)		usr_mutex_unlock(A)
#define MUTEX_DESTROY(A)	usr_mutex_destroy(A)
#define CV_INIT(A,B,C,D)	usr_cond_init(A)
#define CV_TIMEDWAIT(A,B,C)	usr_cond_timedwait(A,B,C)
#define CV_SIGNAL(A)		usr_cond_signal(A)
#define CV_BROADCAST(A)		usr_cond_broadcast(A)
#define CV_DESTROY(A)		usr_cond_destroy(A)
#define CV_WAIT(_cv, _lck)	{ \
				    int isGlockOwner = ISAFS_GLOCK(); \
				    if (isGlockOwner) { \
					AFS_GUNLOCK(); \
				    } \
				    usr_cond_wait(_cv, _lck); \
				    if (isGlockOwner) { \
					MUTEX_EXIT(_lck); \
					AFS_GLOCK(); \
					MUTEX_ENTER(_lck); \
				    } \
				}

extern void osirx_AssertMine(afs_kmutex_t *lockaddr, char *msg);

#define AFS_RXGLOCK()
#define AFS_RXGUNLOCK()
#define ISAFS_RXGLOCK()		1
#define AFS_ASSERT_RXGLOCK()

#define SPLVAR
#define NETPRI
#define USERPRI

#endif /* _RX_KMUTEX_H_ */

