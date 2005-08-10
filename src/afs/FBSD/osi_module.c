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

RCSID
    ("$Header: /cvs/openafs/src/afs/FBSD/osi_module.c,v 1.5.2.2 2005/05/23 21:23:53 shadow Exp $");

#include <afs/sysincludes.h>
#include <afsincludes.h>
#include <sys/module.h>
#include <sys/sysproto.h>
#include <sys/syscall.h>

extern struct vfsops afs_vfsops;
extern struct vnodeopv_desc afs_vnodeop_opv_desc;
extern struct mount *afs_globalVFS;

MALLOC_DEFINE(M_AFS, "afsmisc", "memory used by the AFS filesystem");

#ifdef AFS_FBSD60_ENV
VFS_SET(afs_vfsops, afs, VFCF_NETWORK);
#else
int afs_module_handler(module_t mod, int what, void *arg);

static struct vfsconf afs_vfsconf;
static moduledata_t afs_mod = {
    "afs",
    afs_module_handler,
    &afs_mod
};

DECLARE_MODULE(afs, afs_mod, SI_SUB_VFS, SI_ORDER_MIDDLE);
#endif

#ifndef AFS_FBSD60_ENV
int
afs_module_handler(module_t mod, int what, void *arg)
{
    static int inited = 0;
    int error = 0;

    switch (what) {
    case MOD_LOAD:
	if (inited) {
	    printf("afs cannot be MOD_LOAD'd more than once\n");
	    error = EBUSY;
	    break;
	}
	memset(&afs_vfsconf, 0, sizeof(struct vfsconf));
#ifdef AFS_FBSD53_ENV
	afs_vfsconf.vfc_version = VFS_VERSION;
#endif
	strcpy(afs_vfsconf.vfc_name, "AFS");
	afs_vfsconf.vfc_vfsops = &afs_vfsops;
	afs_vfsconf.vfc_typenum = -1;	/* set by vfs_register */
	afs_vfsconf.vfc_flags = VFCF_NETWORK;
	if ((error = vfs_register(&afs_vfsconf)) != 0)
	    break;
	vfs_add_vnodeops(&afs_vnodeop_opv_desc);
	inited = 1;
	break;
    case MOD_UNLOAD:
#ifndef RXK_LISTENER_ENV
	/* shutdown is incomplete unless RXK_LISTENER_ENV */
	printf("afs: I can't be unloaded yet\n");
	return -1;
#endif
	if (!inited) {
	    error = 0;
	    break;
	}
	if ((error = vfs_unregister(&afs_vfsconf)) != 0) {
	    break;
	}
	vfs_rm_vnodeops(&afs_vnodeop_opv_desc);
	break;
    }

    return (error);
}
#endif
