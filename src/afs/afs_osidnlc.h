/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define AFSNCNAMESIZE 36	/* multiple of 4 */
struct nc {
    unsigned int key;
    struct nc *next, *prev;
    struct vcache *dirp, *vp;
    unsigned char name[AFSNCNAMESIZE];
    /* I think that we can avoid wasting a byte for NULL, with a
     * a little bit of thought.
     */
};

typedef struct {
    unsigned int enters, lookups, misses, removes;
    unsigned int purgeds, purgevs, purgevols, purges;
    unsigned int cycles, lookuprace;
} dnlcstats_t;
