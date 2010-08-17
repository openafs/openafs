/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_BOS_ADMIN_H
#define OPENAFS_BOS_ADMIN_H

#include <afs/param.h>
#include <afs/afs_Admin.h>
#include <afs/afs_vosAdmin.h>
#include <afs/afs_kasAdmin.h>

#define BOS_MAX_NAME_LEN 256
#define BOS_MAX_PROCESS_PARAMETERS 6
#define BOS_ENCRYPTION_KEY_LEN 8

typedef enum {
    BOS_PROCESS_SIMPLE,
    BOS_PROCESS_FS,
    BOS_PROCESS_CRON
} bos_ProcessType_t, *bos_ProcessType_p;

/*
 * ASSUMPTION
 * These execution state flags are defined to be == the BOZO BSTAT_
 * values.  This allows us to directly map from one to the other.
 * If the BOZO BSTAT_ flags are ever modified, these guys need to
 * be changed as well.
 */

typedef enum {
    BOS_PROCESS_STOPPED,
    BOS_PROCESS_RUNNING,
    BOS_PROCESS_STOPPING,
    BOS_PROCESS_STARTING
} bos_ProcessExecutionState_t, *bos_ProcessExecutionState_p;

typedef enum {
    BOS_PROCESS_OK = 0x0,
    BOS_PROCESS_CORE_DUMPED = 0x1,
    BOS_PROCESS_TOO_MANY_ERRORS = 0x2,
    BOS_PROCESS_BAD_FILE_ACCESS = 0x4
} bos_ProcessState_t, *bos_ProcessState_p;

typedef enum {
    BOS_AUTH_REQUIRED,
    BOS_NO_AUTH
} bos_Auth_t, *bos_Auth_p;

typedef enum {
    BOS_PRUNE,
    BOS_DONT_PRUNE
} bos_Prune_t, *bos_Prune_p;

typedef enum {
    BOS_RESTART_WEEKLY,
    BOS_RESTART_DAILY
} bos_Restart_t, *bos_Restart_p;

typedef struct bos_ProcessInfo {
    bos_ProcessExecutionState_t processGoal;
    unsigned long processStartTime;
    unsigned long numberProcessStarts;
    unsigned long processExitTime;
    unsigned long processExitErrorTime;
    unsigned long processErrorCode;
    unsigned long processErrorSignal;
    bos_ProcessState_t state;
} bos_ProcessInfo_t, *bos_ProcessInfo_p;

typedef struct bos_encryptionKeyStatus {
    int lastModificationDate;
    int lastModificationMicroSeconds;
    unsigned int checkSum;
} bos_encryptionKeyStatus_t, *bos_encryptionKeyStatus_p;

typedef struct bos_KeyInfo {
    int keyVersionNumber;
    kas_encryptionKey_t key;
    bos_encryptionKeyStatus_t keyStatus;
} bos_KeyInfo_t, *bos_KeyInfo_p;

typedef enum {
    BOS_RESTART_TIME_HOUR = 0x1,
    BOS_RESTART_TIME_MINUTE = 0x2,
    BOS_RESTART_TIME_SECOND = 0x4,
    BOS_RESTART_TIME_DAY = 0x8,
    BOS_RESTART_TIME_NEVER = 0x10,
    BOS_RESTART_TIME_NOW = 0x20
} bos_RestartTimeFields_t, *bos_RestartTimeFields_p;

typedef struct bos_RestartTime {
    bos_RestartTimeFields_t mask;
    short hour;
    short min;
    short sec;
    short day;
} bos_RestartTime_t, *bos_RestartTime_p;

typedef enum {
    BOS_RESTART_BOS_SERVER,
    BOS_DONT_RESTART_BOS_SERVER
} bos_RestartBosServer_t, *bos_RestartBosServer_p;

typedef enum {
    BOS_SALVAGE_DAMAGED_VOLUMES,
    BOS_DONT_SALVAGE_DAMAGED_VOLUMES
} bos_SalvageDamagedVolumes_t, *bos_SalvageDamagedVolumes_p;

typedef enum {
    BOS_SALVAGE_WRITE_INODES,
    BOS_SALVAGE_DONT_WRITE_INODES
} bos_WriteInodes_t, *bos_WriteInodes_p;

typedef enum {
    BOS_SALVAGE_WRITE_ROOT_INODES,
    BOS_SALVAGE_DONT_WRITE_ROOT_INODES
} bos_WriteRootInodes_t, *bos_WriteRootInodes_p;

typedef enum {
    BOS_SALVAGE_FORCE_DIRECTORIES,
    BOS_SALVAGE_DONT_FORCE_DIRECTORIES
} bos_ForceDirectory_t, *bos_ForceDirectory_p;

typedef enum {
    BOS_SALVAGE_FORCE_BLOCK_READS,
    BOS_SALVAGE_DONT_FORCE_BLOCK_READS
} bos_ForceBlockRead_t, *bos_ForceBlockRead_p;

extern int ADMINAPI bos_ServerOpen(const void *cellHandle,
				   const char *serverName,
				   void **serverHandleP, afs_status_p st);

extern int ADMINAPI bos_ServerClose(const void *serverHandle,
				    afs_status_p st);

extern int ADMINAPI bos_ProcessCreate(const void *serverHandle,
				      char *processName,
				      bos_ProcessType_t processType,
				      char *process,
				      char *cronTime,
				      char *notifier, afs_status_p st);

extern int ADMINAPI bos_FSProcessCreate(const void *serverHandle,
					char *processName,
					char *fileserverPath,
					char *volserverPath,
					char *salvagerPath,
					char *notifier,
					afs_status_p st);

extern int ADMINAPI bos_ProcessDelete(const void *serverHandle,
				      char *processName,
				      afs_status_p st);

extern int ADMINAPI bos_ProcessExecutionStateGet(const void *serverHandle,
						 char *processName,
						 bos_ProcessExecutionState_p
						 processStatusP,
						 char *auxiliaryProcessStatus,
						 afs_status_p st);

extern int ADMINAPI bos_ProcessExecutionStateSet(const void *serverHandle,
						 const char *processName,
						 const bos_ProcessExecutionState_t
						 processStatus,
						 afs_status_p st);

extern int ADMINAPI bos_ProcessExecutionStateSetTemporary(const void
							  *serverHandle, char
							  *processName,
							  bos_ProcessExecutionState_t
							  processStatus,
							  afs_status_p st);

extern int ADMINAPI bos_ProcessNameGetBegin(const void *serverHandle,
					    void **iterationIdP,
					    afs_status_p st);

extern int ADMINAPI bos_ProcessNameGetNext(const void *iterationId,
					   char *processName,
					   afs_status_p st);

extern int ADMINAPI bos_ProcessNameGetDone(const void *iterationId,
					   afs_status_p st);

extern int ADMINAPI bos_ProcessInfoGet(const void *serverHandle,
				       char *processName,
				       bos_ProcessType_p processTypeP,
				       bos_ProcessInfo_p processInfoP,
				       afs_status_p st);

extern int ADMINAPI bos_ProcessParameterGetBegin(const void *serverHandle,
						 const char *processName,
						 void **iterationIdP,
						 afs_status_p st);

extern int ADMINAPI bos_ProcessParameterGetNext(const void *iterationId,
						char *parameter,
						afs_status_p st);

extern int ADMINAPI bos_ProcessParameterGetDone(const void *iterationId,
						afs_status_p st);

extern int ADMINAPI bos_ProcessNotifierGet(const void *serverHandle,
					   const char *processName,
					   char *notifier, afs_status_p st);

extern int ADMINAPI bos_ProcessRestart(const void *serverHandle,
				       const char *processName,
				       afs_status_p st);

extern int ADMINAPI bos_ProcessAllStop(const void *serverHandle,
				       afs_status_p st);

extern int ADMINAPI bos_ProcessAllStart(const void *serverHandle,
					afs_status_p st);

extern int ADMINAPI bos_ProcessAllWaitStop(const void *serverHandle,
					   afs_status_p st);

extern int ADMINAPI bos_ProcessAllWaitTransition(const void *serverHandle,
						 afs_status_p st);

extern int ADMINAPI bos_ProcessAllStopAndRestart(const void *serverHandle,
						 bos_RestartBosServer_t
						 restartBosServer,
						 afs_status_p st);

extern int ADMINAPI bos_AdminCreate(const void *serverHandle,
				    const char *adminName, afs_status_p st);

extern int ADMINAPI bos_AdminDelete(const void *serverHandle,
				    const char *adminName, afs_status_p st);

extern int ADMINAPI bos_AdminGetBegin(const void *serverHandle,
				      void **iterationIdP, afs_status_p st);

extern int ADMINAPI bos_AdminGetNext(const void *iterationId, char *adminName,
				     afs_status_p st);

extern int ADMINAPI bos_AdminGetDone(const void *iterationId,
				     afs_status_p st);

extern int ADMINAPI bos_KeyCreate(const void *serverHandle,
				  int keyVersionNumber,
				  const kas_encryptionKey_p key,
				  afs_status_p st);

extern int ADMINAPI bos_KeyDelete(const void *serverHandle,
				  int keyVersionNumber, afs_status_p st);

extern int ADMINAPI bos_KeyGetBegin(const void *serverHandle,
				    void **iterationIdP, afs_status_p st);

extern int ADMINAPI bos_KeyGetNext(const void *iterationId,
				   bos_KeyInfo_p keyP, afs_status_p st);

extern int ADMINAPI bos_KeyGetDone(const void *iterationId, afs_status_p st);

extern int ADMINAPI bos_CellSet(const void *serverHandle,
				const char *cellName, afs_status_p st);

extern int ADMINAPI bos_CellGet(const void *serverHandle, char *cellName,
				afs_status_p st);

extern int ADMINAPI bos_HostCreate(const void *serverHandle,
				   const char *hostName, afs_status_p st);

extern int ADMINAPI bos_HostDelete(const void *serverHandle,
				   const char *hostName, afs_status_p st);

extern int ADMINAPI bos_HostGetBegin(const void *serverHandle,
				     void **iterationIdP, afs_status_p st);

extern int ADMINAPI bos_HostGetNext(const void *iterationId, char *hostName,
				    afs_status_p st);

extern int ADMINAPI bos_HostGetDone(const void *iterationId, afs_status_p st);

extern int ADMINAPI bos_ExecutableCreate(const void *serverHandle,
					 const char *sourceFile,
					 const char *destFile,
					 afs_status_p st);

extern int ADMINAPI bos_ExecutableRevert(const void *serverHandle,
					 const char *execFile,
					 afs_status_p st);

extern int ADMINAPI bos_ExecutableTimestampGet(const void *serverHandle,
					       const char *execFile,
					       afs_int32 *newTime,
					       afs_int32 *oldTime,
					       afs_int32 *bakTime,
					       afs_status_p st);

extern int ADMINAPI bos_ExecutablePrune(const void *serverHandle,
					bos_Prune_t oldFiles,
					bos_Prune_t bakFiles,
					bos_Prune_t coreFiles,
					afs_status_p st);

extern int ADMINAPI bos_ExecutableRestartTimeSet(const void *serverHandle,
						 bos_Restart_t type,
						 bos_RestartTime_t time,
						 afs_status_p st);

extern int ADMINAPI bos_ExecutableRestartTimeGet(const void *serverHandle,
						 bos_Restart_t type,
						 bos_RestartTime_p timeP,
						 afs_status_p st);

extern int ADMINAPI bos_LogGet(const void *serverHandle, const char *log,
			       unsigned long *logBufferSizeP, char *logData,
			       afs_status_p st);

extern int ADMINAPI bos_AuthSet(const void *serverHandle, bos_Auth_t auth,
				afs_status_p st);

extern int ADMINAPI bos_CommandExecute(const void *serverHandle,
				       const char *command, afs_status_p st);

extern int ADMINAPI bos_Salvage(const void *cellHandle,
				const void *serverHandle,
				const char *partitionName,
				const char *volumeName, int numSalvagers,
				const char *tmpDir, const char *logFile,
				vos_force_t force,
				bos_SalvageDamagedVolumes_t
				salvageDamagedVolumes,
				bos_WriteInodes_t writeInodes,
				bos_WriteRootInodes_t writeRootInodes,
				bos_ForceDirectory_t forceDirectory,
				bos_ForceBlockRead_t forceBlockRead,
				afs_status_p st);

static_inline struct bozo_key *
kas_to_bozoptr(kas_encryptionKey_p key)
{
    return (struct bozo_key *)key;
}
#endif /* OPENAFS_BOS_ADMIN_H */
