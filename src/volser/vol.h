/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *  Module:	    Vol.h
 *  System:	    Volser
 *  Instituition:   ITC, CMU
 *  Date:	    December, 88
 */

/* pick up the declaration of Inode */
#include <afs/afssyscalls.h>

typedef struct DirHandle {
    afs_uint32 dirh_volume;
    int dirh_device;
    Inode dirh_inode;
    afs_int32 dirh_cacheCheck;
    IHandle_t *dirh_handle;
} DirHandle;
