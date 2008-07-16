/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		salvage.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef __salvage_h_
#define __salvage_h_

#include <afs/afssyscalls.h>
/* Definition of DirHandle for salvager.  Not the same as for the file server */

typedef struct DirHandle {
    int dirh_volume;
    int dirh_device;
    Inode dirh_inode;
    IHandle_t *dirh_handle;
    afs_int32 dirh_cacheCheck;
} DirHandle;

#endif /* __salvage_h_ */
