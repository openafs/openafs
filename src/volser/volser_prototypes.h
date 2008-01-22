/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_VOLSER_PROTOTYPES_H
#define _VOLSER_PROTOTYPES_H

/* common.c */

/* vsprocs.c */
struct nvldbentry;
extern void MapPartIdIntoName(afs_int32 partId, char *partName);
extern int yesprompt(char *str);
extern int PrintError(char *msg, afs_int32 errcode);
extern void init_volintInfo(struct volintInfo *vinfo);
extern int UV_SetSecurity(register struct rx_securityClass *as,
			  afs_int32 aindex);
extern struct rx_connection *UV_Bind(afs_int32 aserver, afs_int32 port);
extern void SubEnumerateEntry(struct nvldbentry *entry);
extern void EnumerateEntry(struct nvldbentry *entry);
extern int UV_NukeVolume(afs_int32 server, afs_int32 partid, afs_int32 volid);
extern int UV_PartitionInfo(afs_int32 server, char *pname,
			    struct diskPartition *partition);
extern int UV_CreateVolume(afs_int32 aserver, afs_int32 apart, char *aname,
			   afs_int32 * anewid);
extern int UV_CreateVolume2(afs_int32 aserver, afs_int32 apart, char *aname,
			    afs_int32 aquota, afs_int32 aspare1,
			    afs_int32 aspare2, afs_int32 aspare3,
			    afs_int32 aspare4, afs_int32 * anewid);
extern int UV_AddVLDBEntry(afs_int32 aserver, afs_int32 apart, char *aname,
			   afs_int32 aid);
extern int UV_DeleteVolume(afs_int32 aserver, afs_int32 apart,
			   afs_int32 avolid);
extern void sigint_handler(int x);
extern int UV_MoveVolume(afs_int32 afromvol, afs_int32 afromserver,
			 afs_int32 afrompart, afs_int32 atoserver,
			 afs_int32 atopart);
extern int UV_BackupVolume(afs_int32 aserver, afs_int32 apart,
			   afs_int32 avolid);
extern int UV_ReleaseVolume(afs_int32 afromvol, afs_int32 afromserver,
			    afs_int32 afrompart, int forceflag);
extern void dump_sig_handler(int x);
extern int UV_DumpVolume(afs_int32 afromvol, afs_int32 afromserver,
			 afs_int32 afrompart, afs_int32 fromdate,
			 afs_int32(*DumpFunction) (), char *rock, afs_int32 flags);
extern int UV_RestoreVolume(afs_int32 toserver, afs_int32 topart,
			    afs_int32 tovolid, char tovolname[], int flags,
			    afs_int32(*WriteData) (), char *rock);
extern int UV_LockRelease(afs_int32 volid);
extern int UV_AddSite(afs_int32 server, afs_int32 part, afs_int32 volid, afs_int32 valid);
extern int UV_RemoveSite(afs_int32 server, afs_int32 part, afs_int32 volid);
extern int UV_ChangeLocation(afs_int32 server, afs_int32 part,
			     afs_int32 volid);
extern int UV_ListPartitions(afs_int32 aserver, struct partList *ptrPartList,
			     afs_int32 * cntp);
extern int UV_ZapVolumeClones(afs_int32 aserver, afs_int32 apart,
			      struct volDescription *volPtr,
			      afs_int32 arraySize);
extern int UV_GenerateVolumeClones(afs_int32 aserver, afs_int32 apart,
				   struct volDescription *volPtr,
				   afs_int32 arraySize);
extern int UV_ListVolumes(afs_int32 aserver, afs_int32 apart, int all,
			  struct volintInfo **resultPtr, afs_int32 * size);
extern int UV_XListVolumes(afs_int32 a_serverID, afs_int32 a_partID,
			   int a_all, struct volintXInfo **a_resultPP,
			   afs_int32 * a_numEntsInResultP);
extern int UV_ListOneVolume(afs_int32 aserver, afs_int32 apart,
			    afs_int32 volid, struct volintInfo **resultPtr);
extern int UV_XListOneVolume(afs_int32 a_serverID, afs_int32 a_partID,
			     afs_int32 a_volID,
			     struct volintXInfo **a_resultPP);
extern int sortVolumes(const void *a, const void *b);
extern int UV_SyncVolume(afs_int32 aserver, afs_int32 apart, char *avolname,
			 int flags);
extern int UV_SyncVldb(afs_int32 aserver, afs_int32 apart, int flags,
		       int force);
extern afs_int32 VolumeExists(afs_int32 server, afs_int32 partition,
			      afs_int32 volumeid);
extern afs_int32 CheckVldbRWBK(struct nvldbentry *entry,
			       afs_int32 * modified);
extern int CheckVldbRO(struct nvldbentry *entry, afs_int32 * modified);
extern afs_int32 CheckVldb(struct nvldbentry *entry, afs_int32 * modified);
extern int UV_SyncServer(afs_int32 aserver, afs_int32 apart, int flags,
			 int force);
extern int UV_RenameVolume(struct nvldbentry *entry, char oldname[],
			   char newname[]);
extern int UV_VolserStatus(afs_int32 server, transDebugInfo ** rpntr,
			   afs_int32 * rcount);
extern int UV_VolumeZap(afs_int32 server, afs_int32 part, afs_int32 volid);
extern int UV_SetVolume(afs_int32 server, afs_int32 partition,
			afs_int32 volid, afs_int32 transflag,
			afs_int32 setflag, int sleeptime);
extern int UV_SetVolumeInfo(afs_int32 server, afs_int32 partition,
			    afs_int32 volid, volintInfo * infop);
extern void MapNetworkToHost(struct nvldbentry *old, struct nvldbentry *new);
extern void MapHostToNetwork(struct nvldbentry *entry);

#endif
