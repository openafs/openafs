/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* The Kerberos Authenticated DES security object. */


#ifndef OPENAFS_RXKAD_RXKAD_H
#define OPENAFS_RXKAD_RXKAD_H

		/* no ticket good for longer than 30 days */
#define MAXKTCTICKETLIFETIME (30*24*3600)
#define MINKTCTICKETLEN	      32

#if defined(AFS_AIX52_ENV)
#ifdef __XCOFF64__
#define	MAXKTCTICKETLEN	      12000	/* was 344 */
#else
#define MAXKTCTICKETLEN		344
#endif
#else
#define	MAXKTCTICKETLEN	      12000	/* was 344 */
#endif

#define	MAXKTCNAMELEN	      64	/* name & inst should be 256 */
#define MAXKTCREALMLEN	      64	/* should be 256 */
#define KTC_TIME_UNCERTAINTY (15*60)	/* max skew bet. machines' clocks */

#define MAXRANDOMNAMELEN 16	/* length of random generated 
				 * usernames used by afslog for high 
				 * security must be < MAXKTCNAMELEN && < MAXSMBNAMELEN */
#define MAXSMBNAMELEN    256	/* max length of an SMB name */

#define LOGON_OPTION_INTEGRATED 1
#define LOGON_OPTION_HIGHSECURITY 2

/*
 * Define ticket types. For Kerberos V4 tickets, this is overloaded as
 * the server key version number, so class numbers 0 through 255 are reserved
 * for V4 tickets. For Kerberos V5, tickets have an in-the-clear portion
 * containing the server key version, so we only use a single type number to
 * identify those tickets. The ticket type is carried in the kvno field
 * passed to/from ktc_[SG]etToken.
 */
#define RXKAD_TKT_TYPE_KERBEROS_V5		256
#define RXKAD_TKT_TYPE_KERBEROS_V5_ENCPART_ONLY	213

#define MAXKRB5TICKETLEN		   MAXKTCTICKETLEN

/*
 * The AFS/DFS translator may also make use of additional ticket types in
 * the range 257 through 511. DO NOT USE THESE FOR ANY OTHER PURPOSE.
 */
#define RXKAD_TKT_TYPE_ADAPT_RESERVED_MIN	257
#define RXKAD_TKT_TYPE_ADAPT_RESERVED_MAX	511

struct ktc_encryptionKey {
    char data[8];
};

struct ktc_principal {
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
#ifdef AFS_NT40_ENV
    char smbname[MAXSMBNAMELEN];
#endif
};

#ifndef NEVERDATE
#define NEVERDATE 0xffffffff
#endif

/* this function round a length to the correct encryption block size */
#define round_up_to_ebs(v) (((v) + 7) & (~7))

typedef char rxkad_type;
#define rxkad_client 1		/* bits definitions */
#define rxkad_server 2

typedef char rxkad_level;
#define rxkad_clear 0		/* send packets in the clear */
#define rxkad_auth 1		/* send encrypted sequence numbers */
#define rxkad_crypt 2		/* encrypt packet data */

/* many stats are kept per type and per level.  These are encoded into an index
 * from 0 to 5 by the StatIndex macro. */

#define rxkad_StatIndex(type,level) \
    (((((type) == 1) || ((type) == 2)) && ((level) >= 0) && ((level) <= 2)) \
     ? (((level)<<1)+(type)-1) : 0)
#define rxkad_LevelIndex(level) \
    ((((level) >= 0) && ((level) <= 2)) ? (level) : 0)
#define rxkad_TypeIndex(type) \
    ((((type) == 1) || ((type) == 2)) ? (type) : 0)

struct rxkad_stats {
    afs_uint32 connections[3];	/* client side only */
    afs_uint32 destroyObject;	/* client security objects */
    afs_uint32 destroyClient;	/* client connections */
    afs_uint32 destroyUnused;	/* unused server conn */
    afs_uint32 destroyUnauth;	/* unauthenticated server conn */
    afs_uint32 destroyConn[3];	/* server conn per level */
    afs_uint32 expired;		/* server packets rejected */
    afs_uint32 challengesSent;	/* server challenges sent */
    afs_uint32 challenges[3];	/* challenges seen by client */
    afs_uint32 responses[3];	/* responses seen by server */
    afs_uint32 preparePackets[6];
    afs_uint32 checkPackets[6];
    afs_uint32 bytesEncrypted[2];	/* index just by type */
    afs_uint32 bytesDecrypted[2];
    afs_uint32 fc_encrypts[2];	/* DECRYPT==0, ENCRYPT==1 */
    afs_uint32 fc_key_scheds;	/* key schedule creations */
    afs_uint32 des_encrypts[2];	/* DECRYPT==0, ENCRYPT==1 */
    afs_uint32 des_key_scheds;	/* key schedule creations */
    afs_uint32 des_randoms;	/* random blocks generated */
    long spares[10];
};

#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#ifndef RXKAD_STATS_DECLSPEC
#define RXKAD_STATS_DECLSPEC __declspec(dllimport) extern
#endif
#else
#define RXKAD_STATS_DECLSPEC extern
#endif
RXKAD_STATS_DECLSPEC struct rxkad_stats rxkad_stats;
#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#include <assert.h>
extern pthread_mutex_t rxkad_stats_mutex;
#define LOCK_RXKAD_STATS assert(pthread_mutex_lock(&rxkad_stats_mutex)==0)
#define UNLOCK_RXKAD_STATS assert(pthread_mutex_unlock(&rxkad_stats_mutex)==0)
#else
#define LOCK_RXKAD_STATS
#define UNLOCK_RXKAD_STATS
#endif


/* gak! using up spares already! */
#define rxkad_stats_clientObjects (rxkad_stats.spares[0])
#define rxkad_stats_serverObjects (rxkad_stats.spares[1])

extern int rxkad_EpochWasSet;	/* TRUE => we called rx_SetEpoch */

#include "rxkad_prototypes.h"

#endif /* OPENAFS_RXKAD_RXKAD_H */
