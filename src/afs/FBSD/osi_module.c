/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


#include <afsconfig.h>
#include <afs/param.h>


#include <afs/sysincludes.h>
#include <afsincludes.h>
#include <sys/module.h>
#include <sys/sysproto.h>
#include <sys/syscall.h>

extern struct vfsops afs_vfsops;
extern struct vnodeopv_desc afs_vnodeop_opv_desc;
extern struct mount *afs_globalVFS;

MALLOC_DEFINE(M_AFS, "afsmisc", "memory used by the AFS filesystem");

VFS_SET(afs_vfsops, afs, VFCF_NETWORK);
