/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
#endif
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <lock.h>
#include <afs/voldefs.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/vlserver.h>
#include <afs/nfs.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/volser.h>
#include <afs/volint.h>
#include "../../volser/lockdata.h"
#include "../../vlserver/vlclient.h"
#include <afs/com_err.h>
#include <rx/rxkad.h>
#include <afs/kautils.h>
#include <afs/cmd.h>
#include <errno.h>
#include <afs/afs_Admin.h>
#include "../adminutil/afs_AdminInternal.h"

#define	CLOCKSKEW   2		/* not really skew, but resolution */

extern int ubik_Call();
extern int ubik_Call_New();


extern int UV_NukeVolume(afs_cell_handle_p cellHandle,
			 struct rx_connection *server, unsigned int partition,
			 unsigned int volumeId, afs_status_p st);

extern int UV_CreateVolume(afs_cell_handle_p cellHandle,
			   struct rx_connection *server,
			   unsigned int partition, const char *volumeName,
			   unsigned int quota, unsigned int *volumeId,
			   afs_status_p st);

extern int UV_DeleteVolume(afs_cell_handle_p cellHandle,
			   struct rx_connection *server,
			   unsigned int partition, unsigned int volumeId,
			   afs_status_p st);

extern int UV_MoveVolume(afs_cell_handle_p cellHandle, afs_int32 afromvol,
			 afs_int32 afromserver, afs_int32 afrompart,
			 afs_int32 atoserver, afs_int32 atopart,
			 afs_status_p st);

extern int UV_BackupVolume(afs_cell_handle_p cellHandle, afs_int32 aserver,
			   afs_int32 apart, afs_int32 avolid,
			   afs_status_p st);

extern int UV_ReleaseVolume(afs_cell_handle_p cellHandle, afs_int32 afromvol,
			    afs_int32 afromserver, afs_int32 afrompart,
			    int forceflag, afs_status_p st);

extern int UV_DumpVolume(afs_cell_handle_p cellHandle, afs_int32 afromvol,
			 afs_int32 afromserver, afs_int32 afrompart,
			 afs_int32 fromdate, const char *filename,
			 afs_status_p st);

extern int UV_RestoreVolume(afs_cell_handle_p cellHandle, afs_int32 toserver,
			    afs_int32 topart, afs_int32 tovolid,
			    const char *tovolname, int flags,
			    const char *dumpFile, afs_status_p st);

extern int UV_AddSite(afs_cell_handle_p cellHandle, afs_int32 server,
		      afs_int32 part, afs_int32 volid, afs_status_p st);

extern int UV_RemoveSite(afs_cell_handle_p cellHandle, afs_int32 server,
			 afs_int32 part, afs_int32 volid, afs_status_p st);

extern int UV_ListPartitions(struct rx_connection *server,
			     struct partList *ptrPartList, afs_int32 * cntp,
			     afs_status_p st);

extern int UV_XListVolumes(struct rx_connection *server, afs_int32 a_partID,
			   int a_all, struct volintXInfo **a_resultPP,
			   afs_int32 * a_numEntsInResultP, afs_status_p st);

extern int UV_XListOneVolume(struct rx_connection *server, afs_int32 a_partID,
			     afs_int32 a_volID,
			     struct volintXInfo **a_resultPP,
			     afs_status_p st);

extern int UV_ListOneVolume(struct rx_connection *server, afs_int32 a_partID,
		  afs_int32 a_volID, struct volintInfo **a_resultPP,
		  afs_status_p st);
			    
extern int UV_SyncVldb(afs_cell_handle_p cellHandle,
		       struct rx_connection *server, afs_int32 apart,
		       int flags, int force, afs_status_p st);

extern int CheckVldb(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
		     afs_int32 * modified, afs_status_p st);

extern int UV_SyncServer(afs_cell_handle_p cellHandle,
			 struct rx_connection *server, afs_int32 apart,
			 int flags, afs_status_p st);


extern int UV_VolserStatus(struct rx_connection *server,
			   transDebugInfo ** rpntr, afs_int32 * rcount,
			   afs_status_p st);

extern int UV_VolumeZap(afs_cell_handle_p cellHandle,
			struct rx_connection *serverHandle,
			unsigned int partition, unsigned int volumeId,
			afs_status_p st);

extern int UV_SetVolume(struct rx_connection *server, afs_int32 partition,
			afs_int32 volid, afs_int32 transflag,
			afs_int32 setflag, unsigned int sleep,
			afs_status_p st);

extern int UV_RenameVolume(afs_cell_handle_p cellHandle,
			   struct nvldbentry *entry, const char *newname,
			   afs_status_p st);
