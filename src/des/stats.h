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

struct {
    afs_uint32 rxkad_stuff[34];	/* see rxkad.h */
    afs_uint32 fc_stuff[3];		/* see rxkad.h */
    afs_uint32 des_encrypts[2];	/* DECRYPT==0, ENCRYPT==1 */
    afs_uint32 des_key_scheds;	/* key schedule creations */
    afs_uint32 des_randoms;		/* random blocks generated */
    unsigned long spares[10];
} rxkad_stats; /* put these here for convenience */

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#include <assert.h>
extern pthread_mutex_t rxkad_stats_mutex;
#define LOCK_RXKAD_STATS assert(pthread_mutex_lock(&rxkad_stats_mutex)==0);
#define UNLOCK_RXKAD_STATS assert(pthread_mutex_unlock(&rxkad_stats_mutex)==0);
#else
#define LOCK_RXKAD_STATS
#define UNLOCK_RXKAD_STATS
#endif

