/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/stds.h>
#include <afs/afs_Admin.h>
#include <afs/vlserver.h>
#include "../adminutil/afs_AdminInternal.h"

extern int VLDB_CreateEntry(afs_cell_handle_p cellHandle,
			    struct nvldbentry *entryp, afs_status_p st);

extern int aVLDB_GetEntryByID(afs_cell_handle_p cellHandle, afs_int32 volid,
			     afs_int32 voltype, struct nvldbentry *entryp,
			     afs_status_p st);


extern int aVLDB_GetEntryByName(afs_cell_handle_p cellHandle,
			       const char *namep, struct nvldbentry *entryp,
			       afs_status_p st);

extern int VLDB_ReplaceEntry(afs_cell_handle_p cellHandle, afs_int32 volid,
			     afs_int32 voltype, struct nvldbentry *entryp,
			     afs_int32 releasetype, afs_status_p st);

extern int VLDB_ListAttributes(afs_cell_handle_p cellHandle,
			       VldbListByAttributes * attrp,
			       afs_int32 * entriesp,
			       nbulkentries * blkentriesp, afs_status_p st);

extern int VLDB_ListAttributesN2(afs_cell_handle_p cellHandle,
				 VldbListByAttributes * attrp, char *name,
				 afs_int32 thisindex, afs_int32 * nentriesp,
				 nbulkentries * blkentriesp,
				 afs_int32 * nextindexp, afs_status_p st);

extern int VLDB_IsSameAddrs(afs_cell_handle_p cellHandle, afs_int32 serv1,
			    afs_int32 serv2, int *equal, afs_status_p st);

extern int GetVolumeInfo(afs_cell_handle_p cellHandle, unsigned int volid,
			 struct nvldbentry *rentry, afs_int32 * server,
			 afs_int32 * partition, afs_int32 * voltype,
			 afs_status_p st);

extern int ValidateVolumeName(const char *volumeName, afs_status_p st);

extern int vsu_ExtractName(char *rname, char *name);

extern int RemoveBadAddresses(afs_int32 * totalp, bulkaddrs * addrsp);
