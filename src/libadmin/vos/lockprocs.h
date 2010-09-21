/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <afs/voldefs.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxstat.h>
#include <afs/vlserver.h>
#include <afs/nfs.h>
#include <afs/afsint.h>
#include <afs/volint.h>
#include <afs/volser.h>
#include "../../volser/lockdata.h"
#include <afs/afs_utilAdmin.h>
#include "../adminutil/afs_AdminInternal.h"
#include "vosutils.h"

extern void Lp_SetRWValue(afs_cell_handle_p cellHandle,
			  struct nvldbentry *entry, afs_int32 oserver,
			  afs_int32 opart, afs_int32 nserver,
			  afs_int32 npart);

extern void Lp_SetROValue(afs_cell_handle_p cellHandle,
			  struct nvldbentry *entry, afs_int32 oserver,
			  afs_int32 opart, afs_int32 nserver,
			  afs_int32 npart);

extern int Lp_Match(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
		    afs_int32 server, afs_int32 part, afs_status_p st);

extern int Lp_ROMatch(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
		      afs_int32 server, afs_int32 part, afs_status_p st);

extern int Lp_GetRwIndex(afs_cell_handle_p cellHandle,
			 struct nvldbentry *entry, afs_status_p st);

extern void Lp_QInit(struct qHead *ahead);

extern void Lp_QAdd(struct qHead *ahead, struct aqueue *elem);

extern int Lp_QScan(struct qHead *ahead, afs_int32 id, int *success,
		    struct aqueue **elem, afs_status_p st);

extern void Lp_QEnumerate(struct qHead *ahead, int *success,
			  struct aqueue *elem, afs_status_p st);
