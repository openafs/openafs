/* Copyright Transarc Corporation 1998 - All Rights Reserved
 *
 * rx_lwp.h - mutexes and condition variables do not exist for LWP.
 */

#ifndef _RX_LWP_H_
#define _RX_LWP_H_

/* KDUMP_KERNEL is defined when kdump includes this header. */
#ifndef KDUMP_KERNEL

#define CALL_HOLD(call, type)
#define CALL_RELE(call, type)
#define RXObtainWriteLock(a) AFS_ASSERT_RXGLOCK()
#define RXReleaseWriteLock(a)

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)  
#define MUTEX_INIT(a,b,c,d) 
#define CV_INIT(a,b,c,d)
#define CV_DESTROY(a)
#define osirx_AssertMine(a, b)

#endif /* KERNEL */

#endif /* _RX_LWP_H_ */
