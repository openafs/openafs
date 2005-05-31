/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Some Cheap statistics which should be shared with the rxkad definitions, but
 * aren't.  The layout should match the layout in rxkad/rxkad.p.h. */

#ifndef OPENAFS_DES_STATS_H
#define OPENAFS_DES_STATS_H


/* this is an optimization for pthreads environments.  Instead of having global
   rxkad statistics, make them thread-specific, and only perform aggregation on
   the stats structures when necessary.  Furthermore, don't do any locking, and
   live with the nearly accurate aggregation (e.g. you might miss a few increments,
   but the resulting aggregation should be almost correct). */

#ifdef AFS_PTHREAD_ENV
typedef struct rxkad_stats_t {
#else
struct rxkad_stats {
#endif
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
    afs_uint32 clientObjects;
    afs_uint32 serverObjects;
    long spares[8];
#ifdef AFS_PTHREAD_ENV
    struct rxkad_stats_t * next;
    struct rxkad_stats_t * prev;
} rxkad_stats_t;			/* put these here for convenience */
#else /* AFS_PTHREAD_ENV */
};
#ifdef AFS_NT40_ENV
struct rxkad_stats rxkad_stats;         /* put this here for convenience */
#endif 
#endif /* AFS_PTHREAD_ENV */


#ifdef AFS_PTHREAD_ENV
struct rxkad_global_stats {
    rxkad_stats_t * first;
    rxkad_stats_t * last;
};

#include <pthread.h>
#include <assert.h>
extern pthread_mutex_t rxkad_global_stats_lock;
extern pthread_key_t rxkad_stats_key;

extern void rxkad_global_stats_init();
extern rxkad_stats_t * rxkad_thr_stats_init();
extern int rxkad_stats_agg(rxkad_stats_t *);

#ifndef BEGIN
#define BEGIN do {
#define END   } while(0)
#endif
#define RXKAD_GLOBAL_STATS_LOCK assert(pthread_mutex_lock(&rxkad_global_stats_lock)==0)
#define RXKAD_GLOBAL_STATS_UNLOCK assert(pthread_mutex_unlock(&rxkad_global_stats_lock)==0)
#define GET_RXKAD_STATS(stats) rxkad_stats_agg(stats)
#define GET_RXKAD_THR_STATS(rxkad_stats) \
    BEGIN \
        (rxkad_stats) = ((rxkad_stats_t*)pthread_getspecific(rxkad_stats_key)); \
        if ((rxkad_stats) == NULL) { \
            assert(((rxkad_stats) = rxkad_thr_stats_init()) != NULL); \
        } \
    END
#define INC_RXKAD_STATS(stats_elem) \
    BEGIN \
        rxkad_stats_t * rxkad_stats; \
        rxkad_stats = ((rxkad_stats_t*)pthread_getspecific(rxkad_stats_key)); \
        if (rxkad_stats == NULL) { \
            assert(((rxkad_stats) = rxkad_thr_stats_init()) != NULL); \
        } \
        rxkad_stats->stats_elem++; \
    END
#define DEC_RXKAD_STATS(stats_elem) \
    BEGIN \
        rxkad_stats_t * rxkad_stats; \
        rxkad_stats = ((rxkad_stats_t*)pthread_getspecific(rxkad_stats_key)); \
        if (rxkad_stats == NULL) { \
            assert(((rxkad_stats) = rxkad_thr_stats_init()) != NULL); \
        } \
        rxkad_stats->stats_elem--; \
    END
#define ADD_RXKAD_STATS(stats_elem, inc_value) \
    BEGIN \
        rxkad_stats_t * rxkad_stats; \
        rxkad_stats = ((rxkad_stats_t*)pthread_getspecific(rxkad_stats_key)); \
        if (rxkad_stats == NULL) { \
            assert(((rxkad_stats) = rxkad_thr_stats_init()) != NULL); \
        } \
        rxkad_stats->stats_elem += inc_value; \
    END
#define SUB_RXKAD_STATS(stats_elem, dec_value) \
    BEGIN \
        rxkad_stats_t * rxkad_stats; \
        rxkad_stats = ((rxkad_stats_t*)pthread_getspecific(rxkad_stats_key)); \
        if (rxkad_stats == NULL) { \
            assert(((rxkad_stats) = rxkad_thr_stats_init()) != NULL); \
        } \
        rxkad_stats->stats_elem -= dec_value; \
    END
#else /* AFS_PTHREAD_ENV */
#define INC_RXKAD_STATS(stats_elem) rxkad_stats.stats_elem++
#define DEC_RXKAD_STATS(stats_elem) rxkad_stats.stats_elem--
#define ADD_RXKAD_STATS(stats_elem, inc_value) rxkad_stats.stats_elem += (inc_value)
#define SUB_RXKAD_STATS(stats_elem, dec_value) rxkad_stats.stats_elem -= (dec_value)
#define GET_RXKAD_STATS(stats) memcpy((stats), &rxkad_stats, sizeof(rxkad_stats))
#endif /* AFS_PTHREAD_ENV */


#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#ifndef RXKAD_STATS_DECLSPEC
#define RXKAD_STATS_DECLSPEC __declspec(dllimport) extern
#endif
#else
#define RXKAD_STATS_DECLSPEC extern
#endif

#ifdef AFS_PTHREAD_ENV
RXKAD_STATS_DECLSPEC struct rxkad_global_stats rxkad_global_stats;
#else /* AFS_PTHREAD_ENV */
RXKAD_STATS_DECLSPEC struct rxkad_stats rxkad_stats;
#endif


#endif /* OPENAFS_DES_STATS_H */
