/* Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _BUCOORD_INTERNAL_H
#define _BUCOORD_INTERNAL_H

/* bc_status.c */

extern void *statusWatcher(void *);
extern afs_int32 bc_jobNumber(void);
extern int waitForTask(afs_uint32 taskId);

/* command.c */
extern char *bc_CopyString(char *astring);
extern afs_int32 getPortOffset(char *port);
extern afs_int32 bc_SafeATOI(char *anum);
extern int bc_WaitForNoJobs(void);

struct cmd_syndesc;
extern int bc_DumpCmd(struct cmd_syndesc *as, void *arock);
extern int bc_VolRestoreCmd(struct cmd_syndesc *as, void *arock);
extern int bc_DiskRestoreCmd(struct cmd_syndesc *as, void *arock);
extern int bc_QuitCmd(struct cmd_syndesc *as, void *arock);
extern int bc_VolsetRestoreCmd(struct cmd_syndesc *as, void *arock);
extern int bc_AddHostCmd(struct cmd_syndesc *as, void *arock);
extern int bc_DeleteHostCmd(struct cmd_syndesc *as, void *arock);
extern int bc_ListHostsCmd(struct cmd_syndesc *as, void *arock);
extern int bc_JobsCmd(struct cmd_syndesc *as, void *arock);
extern int bc_KillCmd(struct cmd_syndesc *as, void *arock);
extern int bc_ListVolSetCmd(struct cmd_syndesc *as, void *arock);
extern int bc_ListDumpScheduleCmd(struct cmd_syndesc *as, void *arock);
extern int bc_AddVolSetCmd(struct cmd_syndesc *as, void *arock);
extern int bc_GetTapeStatusCmd(struct cmd_syndesc *as, void *arock);
extern int bc_DeleteVolSetCmd(struct cmd_syndesc *as, void *arock);
extern int bc_AddVolEntryCmd(struct cmd_syndesc *as, void *arock);
extern int bc_DeleteVolEntryCmd(struct cmd_syndesc *as, void *arock);
extern int bc_AddDumpCmd(struct cmd_syndesc *as, void *arock);
extern int bc_DeleteDumpCmd(struct cmd_syndesc *as, void *arock);
extern int bc_LabelTapeCmd(struct cmd_syndesc *as, void *arock);
extern int bc_ReadLabelCmd(struct cmd_syndesc *as, void *arock);
extern int bc_ScanDumpsCmd(struct cmd_syndesc *as, void *arock);
extern int bc_dblookupCmd(struct cmd_syndesc *as, void *arock);
extern int bc_SetExpCmd(struct cmd_syndesc *as, void *arock);
extern int bc_saveDbCmd(struct cmd_syndesc *as, void *arock);
extern int bc_restoreDbCmd(struct cmd_syndesc *as, void *arock);
extern int bc_dumpInfoCmd(struct cmd_syndesc *as, void *arock);
extern int bc_dbVerifyCmd(struct cmd_syndesc *as, void *arock);
extern int bc_deleteDumpCmd(struct cmd_syndesc *as, void *arock);

/* config.c */
extern int bc_AddTapeHost(struct bc_config *aconfig, char *aname,
			  afs_int32 aport);
extern int bc_DeleteTapeHost(struct bc_config *aconfig, char *aname,
			     afs_int32 aport);
extern int bc_InitConfig(char *apath);

/* dsstub.c */
extern char *tailCompPtr(char *pathNamePtr);

/* dsvs.c */
extern struct bc_volumeSet *bc_FindVolumeSet(struct bc_config *aconfig,
					     char *aname);
extern void FreeVolumeSet(struct bc_volumeSet *);
extern int bc_AddVolumeItem(struct bc_config *aconfig, char *avolName,
			    char *ahost, char *apart, char *avol);
extern int bc_CreateVolumeSet(struct bc_config *aconfig, char *avolName,
		              afs_int32 aflags);
extern int bc_DeleteVolumeItem(struct bc_config *aconfig, char *avolName,
		               afs_int32 anumber);
extern int bc_DeleteVolumeSet(struct bc_config *aconfig, char *avolName,
		              afs_int32 *flags);
extern int bc_ParseHost(char *aname, struct sockaddr_in *asockaddr);
extern afs_int32 bc_GetPartitionID(char *aname, afs_int32 *aval);
extern int bc_CreateDumpSchedule(struct bc_config *aconfig, char *adumpName,
		                 afs_int32 expDate, afs_int32 expType);
extern int bc_DeleteDumpSchedule(struct bc_config *aconfig, char *adumpName);
extern int FindDump(struct bc_config *aconfig, char *nodeString,
		    struct bc_dumpSchedule **parentptr,
		    struct bc_dumpSchedule **nodeptr);
extern int bc_ProcessDumpSchedule(struct bc_config *aconfig);
extern struct bc_dumpSchedule * bc_FindDumpSchedule(struct bc_config *aconfig,
						    char *aname);


/* dump.c */
extern int CheckTCVersion(struct rx_connection *tconn);
extern int ConnectButc(struct bc_config *config, afs_int32 port,
		       struct rx_connection **tconn);
extern int bc_StartDmpRst(struct bc_config *aconfig, char *adname,
		          char *avname, struct bc_volumeDump *avolsToDump,
			  struct sockaddr_in *adestServer,
			  afs_int32 adestPartition, afs_int32 afromDate,
			  char *anewExt, int aoldFlag, afs_int32 aparent,
			  afs_int32 alevel, int (*aproc) (int),
			  afs_int32 *ports, afs_int32 portCount,
			  struct bc_dumpSchedule *dsptr, int append,
			  int dontExecute);
extern int bc_Dumper(int);
extern int bc_LabelTape(char *afsname, char *pname, afs_int32 size,
		        struct bc_config *config, afs_int32 port);
extern int bc_ReadLabel(struct bc_config *config, afs_int32 port);
extern int bc_ScanDumps(struct bc_config *config, afs_int32 dbAddFlag,
		        afs_int32 port);




/* dump_sched.c */
extern afs_int32 bc_UpdateDumpSchedule(void);
extern int bc_SaveDumpSchedule(void);

/* expire.c */
struct cmd_parmdesc;
extern afs_int32 bc_ParseExpiration(struct cmd_parmdesc *paramPtr,
		                    afs_int32 *expType, afs_int32 *expDate);
/* main.c */
extern time_t tokenExpires;
extern afs_int32 doDispatch(afs_int32, char *[], afs_int32);
extern void bc_HandleMisc(afs_int32 code);

/* regex.c */
extern char *re_comp(const char *sp);
extern int re_exec(const char *p1);

/* restore.c */
extern int BackupName(char *);
extern int bc_Restorer(afs_int32);

/* status.c */
extern void initStatus(void);
extern void lock_cmdLine(void);
extern void unlock_cmdLine(void);
extern void clearStatus(afs_uint32, afs_uint32);

/* tape_hosts.c */
extern afs_int32 bc_UpdateHosts(void);
extern int bc_SaveHosts(void);

/* ubik_db_if.c */
extern afs_int32 filesize(FILE *stream);
extern int bc_CheckTextVersion(udbClientTextP ctPtr);
extern int bc_openTextFile(udbClientTextP ctPtr, char *tmpFileName);
extern int bcdb_GetTextFile(udbClientTextP ctPtr);
extern afs_int32 bcdb_FindVolumes(afs_int32 dumpID, char *volumeName,
		                  struct budb_volumeEntry *returnArray,
				  afs_int32 last, afs_int32 *next,
				  afs_int32 maxa, afs_int32 *nEntries);
extern int bcdb_FindDump(char *volumeName, afs_int32 beforeDate,
		         struct budb_dumpEntry *deptr);
extern afs_int32 bcdb_FindLastVolClone(char *volSetName, char *dumpName,
				       char *volName, afs_int32 *clonetime);
extern afs_int32 bcdb_listDumps (afs_int32 sflags, afs_int32 groupId,
		                 afs_int32 fromTime, afs_int32 toTime,
				 budb_dumpsList *dumps, budb_dumpsList *flags);
extern afs_int32 bcdb_DeleteVDP(char *, char *, afs_int32 );
extern afs_int32 bcdb_FindClone(afs_int32, char *, afs_int32 *);
extern afs_int32 bcdb_LookupVolume(char *volumeName,
				   struct budb_volumeEntry *returnArray,
		                   afs_int32 last, afs_int32 *next,
				   afs_int32 maxa, afs_int32 *nEntries);
extern int bcdb_FindTape(afs_int32 dumpid, char *tapeName,
		         struct budb_tapeEntry *teptr);

extern afs_int32 udbClientInit(int noAuthFlag, int localauth, char *cellName);

/* vol_sets.c */
extern afs_int32 bc_UpdateVolumeSet(void);
extern int bc_SaveVolumeSet(void);

/* volstub.c */

extern afs_int32 volImageTime(afs_uint32 serv, afs_int32 part, afs_uint32 volid,
		              afs_int32 voltype, afs_int32 *clDatePtr);
#endif

