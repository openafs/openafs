
#if !defined(lint) && !defined(LOCORE) && defined(RCS_HDRS)
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*

	System:		VICE-TWO
	Module:		salvage.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#include <afs/afssyscalls.h>
/* Definition of DirHandle for salvager.  Not the same as for the file server */

typedef struct DirHandle {
    int dirh_volume;
    int dirh_device;
    Inode dirh_inode;
    IHandle_t   *dirh_handle;
    afs_int32	dirh_cacheCheck;
} DirHandle;

