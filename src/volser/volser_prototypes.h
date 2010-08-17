/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	AFS_SRC_VOLSER_PROTOTYPES_H
#define AFS_SRC_VOLSER_PROTOTYPES_H

struct nvldbentry;
struct volintInfo;

/* vsprocs.c */
extern void MapPartIdIntoName(afs_int32 partId, char *partName);

extern void MapHostToNetwork(struct nvldbentry *entry);

extern struct rx_connection *UV_Bind(afs_uint32 aserver, afs_int32 port);

extern int UV_CreateVolume(afs_uint32 aserver, afs_int32 apart, char *aname,
			   afs_uint32 * anewid);

extern int UV_DeleteVolume(afs_uint32 aserver, afs_int32 apart,
			   afs_uint32 avolid);

extern int UV_SetSecurity(struct rx_securityClass *as,
                          afs_int32 aindex);

extern int UV_ListOneVolume(afs_uint32 aserver, afs_int32 apart,
			    afs_uint32 volid, struct volintInfo **resultPtr);

extern int UV_RestoreVolume(afs_uint32 toserver, afs_int32 topart,
			    afs_uint32 tovolid, char tovolname[],
			    int restoreflags,
			    afs_int32(*WriteData) (struct rx_call *, void *),
			    void *rock);
#endif
