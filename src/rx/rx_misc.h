/* Copyright Transarc Corporation 1998 - All Rights Reserved
 *
 * rx_misc.h - Various RX configuration macros.
 */

#ifndef _RX_MISC_H_
#define _RX_MISC_H_

#ifndef	AFS_SUN5_ENV
#define MISCMTU
#define ADAPT_MTU
#endif

#if defined(AFS_SUN5_ENV) && !defined(KERNEL)
#define MISCMTU
#define ADAPT_MTU
#include <sys/sockio.h>
#include <sys/fcntl.h>
#endif

#if	defined(AFS_AIX41_ENV) && defined(KERNEL)
#define PIN(a, b) pin(a, b);
#define UNPIN(a, b) unpin(a, b);
#else 
#define PIN(a, b) ;
#define UNPIN(a, b) ;
#endif



/* Include the following to use the lock data base. */
/* #define RX_LOCKS_DB 1 */
/* The lock database uses a file id number and the line number to identify
 * where in the code a lock was obtained. Each file containing locks
 * has a separate file id called: rxdb_fileID.
 */
#define RXDB_FILE_RX        1	/* rx.c */
#define RXDB_FILE_RX_EVENT  2	/* rx_event.c */
#define RXDB_FILE_RX_PACKET 3	/* rx_packet.c */
#define RXDB_FILE_RX_RDWR   4	/* rx_rdwr.c */

#endif /* _RX_MISC_H_ */
