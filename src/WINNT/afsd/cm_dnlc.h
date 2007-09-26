/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <ctype.h>

#define 	CM_AFSNCNAMESIZE 	40  /* multiple of 8 (for 64-bit) */
#define         NCSIZE 			512
#define         NHSIZE 			256 /* must be power of 2 */

#define         CM_DNLC_MAGIC           ('D' | 'N' <<8 | 'L'<<16 | 'C'<<24)

typedef struct nc {
    afs_uint32 magic;
    unsigned int key;
    struct nc *next, *prev;
    cm_scache_t *dirp, *vp;
    unsigned char name[CM_AFSNCNAMESIZE];   
} cm_nc_t;

typedef struct {
    unsigned int enters, lookups, misses, removes;
    unsigned int purgeds, purgevs, purgevols, purges;
    unsigned int cycles, lookuprace;
} cm_dnlcstats_t;

#define dnlcHash(ts, hval) for (hval=0; *ts; ts++) {    \
                                hval *= 173;            \
                                hval += cm_foldUpper[(unsigned char)(*ts)]; \
                           }
extern void cm_dnlcEnter(cm_scache_t *adp, char *name, cm_scache_t *avc);
extern void cm_dnlcRemove(cm_scache_t *adp, char *name);
extern void cm_dnlcPurgedp(cm_scache_t *adp);
extern void cm_dnlcPurgevp(cm_scache_t *avc);
extern void cm_dnlcPurge(void);
extern void cm_dnlcPurgeVol(struct AFSFid *fidp);
extern void cm_dnlcInit(int);
extern long cm_dnlcShutdown(void);
extern cm_scache_t* cm_dnlcLookup(cm_scache_t *adp, cm_lookupSearch_t* sp);
extern long cm_dnlcValidate(void);
