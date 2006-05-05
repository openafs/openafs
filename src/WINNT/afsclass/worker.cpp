/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <WINNT/afsclass.h>
#include "internal.h"
#include <errno.h>

extern "C" {
#include <afs/vlserver.h>
}


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#if 0
static LPCTSTR cszAFSVOSDLL = TCHAR("AfsVosAdmin.dll");
static LPCTSTR cszAFSBOSDLL = TCHAR("AfsBosAdmin.dll");
static LPCTSTR cszAFSKASDLL = TCHAR("AfsKasAdmin.dll");
static LPCTSTR cszAFSPTSDLL = TCHAR("AfsPtsAdmin.dll");
static LPCTSTR cszAFSUTILDLL = TCHAR("AfsAdminUtil.dll");
static LPCTSTR cszAFSCLIENTDLL = TCHAR("AfsClientAdmin.dll");
#else
static LPCTSTR cszAFSVOSDLL = __T("AfsVosAdmin.dll");
static LPCTSTR cszAFSBOSDLL = __T("AfsBosAdmin.dll");
static LPCTSTR cszAFSKASDLL = __T("AfsKasAdmin.dll");
static LPCTSTR cszAFSPTSDLL = __T("AfsPtsAdmin.dll");
static LPCTSTR cszAFSUTILDLL = __T("AfsAdminUtil.dll");
static LPCTSTR cszAFSCLIENTDLL = __T("AfsClientAdmin.dll");
#endif

/*
 * VARIABLES __________________________________________________________________
 *
 */

static HINSTANCE hiVOS = NULL;
static HINSTANCE hiBOS = NULL;
static HINSTANCE hiKAS = NULL;
static HINSTANCE hiPTS = NULL;
static HINSTANCE hiUtil = NULL;
static HINSTANCE hiClient = NULL;


/*
 * .DLL PROTOTYPES ____________________________________________________________
 *
 */

extern "C" {

typedef int (ADMINAPI *lpVosBackupVolumeCreate_t)(const void *cellHandle, vos_MessageCallBack_t callBack, unsigned int volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosBackupVolumeCreateMultiple_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, const unsigned int *partition, const char *volumePrefix, vos_exclude_t excludePrefix, afs_status_p st);
typedef int (ADMINAPI *lpVosPartitionGet_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, vos_partitionEntry_p partitionP, afs_status_p st);
typedef int (ADMINAPI *lpVosPartitionGetBegin_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpVosPartitionGetNext_t)(const void *iterationId, vos_partitionEntry_p partitionP, afs_status_p st);
typedef int (ADMINAPI *lpVosPartitionGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpVosServerOpen_t)(const void *cellHandle, const char *serverName, void **serverHandleP, afs_status_p st);
typedef int (ADMINAPI *lpVosServerClose_t)(const void *serverHandle, afs_status_p st);
typedef int (ADMINAPI *lpVosServerSync_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, const unsigned int *partition, vos_force_t force, afs_status_p st);
typedef int (ADMINAPI *lpVosFileServerAddressChange_t)(const void *cellHandle, vos_MessageCallBack_t callBack, int oldAddress, int newAddress, afs_status_p st);
typedef int (ADMINAPI *lpVosFileServerAddressRemove_t)(const void *cellHandle, vos_MessageCallBack_t callBack, int serverAddress, afs_status_p st);
typedef int (ADMINAPI *lpVosFileServerGetBegin_t)(const void *cellHandle, vos_MessageCallBack_t callBack, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpVosFileServerGetNext_t)(void *iterationId, vos_fileServerEntry_p serverEntryP, afs_status_p st);
typedef int (ADMINAPI *lpVosFileServerGetDone_t)(void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpVosServerTransactionStatusGetBegin_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpVosServerTransactionStatusGetNext_t)(const void *iterationId, vos_serverTransactionStatus_p serverTransactionStatusP, afs_status_p st);
typedef int (ADMINAPI *lpVosServerTransactionStatusGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBGet_t)(const void *cellHandle, vos_MessageCallBack_t callBack, const unsigned int *volumeId, const char *volumeName, vos_vldbEntry_p vldbEntry, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBGetBegin_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int *partition, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBGetNext_t)(const void *iterationId, vos_vldbEntry_p vldbEntry, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBEntryRemove_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, const unsigned int *partition, unsigned int *volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBUnlock_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, const unsigned int *partition, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBEntryLock_t)(const void *cellHandle, vos_MessageCallBack_t callBack, unsigned int volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBEntryUnlock_t)(const void *cellHandle, vos_MessageCallBack_t callBack, unsigned int volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBReadOnlySiteCreate_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBReadOnlySiteDelete_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVLDBSync_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, const unsigned int *partition, vos_force_t force, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeCreate_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, const char *volumeName, unsigned int quota, unsigned int *volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeDelete_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeRename_t)(const void *cellHandle, vos_MessageCallBack_t callBack, unsigned int readWriteVolumeId, const char *newVolumeName, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeDump_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int *partition, unsigned int volumeId, unsigned int startTime, const char *dumpFile, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeRestore_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int *volumeId, const char *volumeName, const char *dumpFile, vos_volumeRestoreType_t dumpType, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeOnline_t)(const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, unsigned int sleepTime, vos_volumeOnlineType_t volumeStatus, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeOffline_t)(const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeGet_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, vos_volumeEntry_p volumeP, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeGetBegin_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeGetNext_t)(const void *iterationId, vos_volumeEntry_p volumeP, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeMove_t)(const void *cellHandle, vos_MessageCallBack_t callBack, unsigned int volumeId, const void *fromServer, unsigned int fromPartition, const void *toServer, unsigned int toPartition, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeRelease_t)(const void *cellHandle, vos_MessageCallBack_t callBack, unsigned int volumeId, vos_force_t force, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeZap_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, vos_force_t force, afs_status_p st);
typedef int (ADMINAPI *lpVosPartitionNameToId_t)(const char *partitionName, unsigned int *partitionId, afs_status_p st);
typedef int (ADMINAPI *lpVosPartitionIdToName_t)(unsigned int partitionId, char *partitionName, afs_status_p st);
typedef int (ADMINAPI *lpVosVolumeQuotaChange_t)(const void *cellHandle, const void *serverHandle, vos_MessageCallBack_t callBack, unsigned int partition, unsigned int volumeId, unsigned int volumeQuota, afs_status_p st);

typedef int (ADMINAPI *lpBosServerOpen_t)(const void *cellHandle, const char *hostName, void **hostHandlePP, afs_status_p st);
typedef int (ADMINAPI *lpBosServerClose_t)(const void *hostHandleP, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessCreate_t)(const void *serverHandle, const char *processName, bos_ProcessType_t processType, const char *process, const char *cronTime, const char *notifier, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessDelete_t)(const void *serverHandle, const char *processName, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessExecutionStateGet_t)(const void *serverHandle, const char *processName, bos_ProcessExecutionState_p processStatusP, char *auxiliaryProcessStatus, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessExecutionStateSet_t)(const void *serverHandle, const char *processName, bos_ProcessExecutionState_t processStatus, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessExecutionStateSetTemporary_t)(const void *serverHandle, const char *processName, bos_ProcessExecutionState_t processStatus, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessNameGetBegin_t)(const void *serverHandle, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessNameGetNext_t)(const void *iterationId, char *processName, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessNameGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessInfoGet_t)(const void *serverHandle, const char *processName, bos_ProcessType_p processTypeP, bos_ProcessInfo_p processInfoP, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessParameterGetBegin_t)(const void *serverHandle, const char *processName, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessParameterGetNext_t)(const void *iterationId, char *parameter, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessParameterGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessNotifierGet_t)(const void *serverHandle, const char *processName, char *notifier, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessRestart_t)(const void *serverHandle, const char *processName, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessAllStop_t)(const void *serverHandle, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessAllStart_t)(const void *serverHandle, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessAllWaitStop_t)(const void *serverHandle, afs_status_p st);
typedef int (ADMINAPI *lpBosProcessAllStopAndRestart_t)(const void *serverHandle, bos_RestartBosServer_t restartBosServer, afs_status_p st);
typedef int (ADMINAPI *lpBosAdminCreate_t)(const void *serverHandle, const char *adminName, afs_status_p st);
typedef int (ADMINAPI *lpBosAdminDelete_t)(const void *serverHandle, const char *adminName, afs_status_p st);
typedef int (ADMINAPI *lpBosAdminGetBegin_t)(const void *serverHandle, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpBosAdminGetNext_t)(const void *iterationId, char *adminName, afs_status_p st);
typedef int (ADMINAPI *lpBosAdminGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpBosKeyCreate_t)(const void *serverHandle, int keyVersionNumber, const kas_encryptionKey_p key, afs_status_p st);
typedef int (ADMINAPI *lpBosKeyDelete_t)(const void *serverHandle, int keyVersionNumber, afs_status_p st);
typedef int (ADMINAPI *lpBosKeyGetBegin_t)(const void *serverHandle, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpBosKeyGetNext_t)(const void *iterationId, bos_KeyInfo_p keyP, afs_status_p st);
typedef int (ADMINAPI *lpBosKeyGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpBosCellSet_t)(const void *serverHandle, const char *cellName, afs_status_p st);
typedef int (ADMINAPI *lpBosCellGet_t)(const void *serverHandle, char *cellName, afs_status_p st);
typedef int (ADMINAPI *lpBosHostCreate_t)(const void *serverHandle, const char *hostName, afs_status_p st);
typedef int (ADMINAPI *lpBosHostDelete_t)(const void *serverHandle, const char *hostName, afs_status_p st);
typedef int (ADMINAPI *lpBosHostGetBegin_t)(const void *serverHandle, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpBosHostGetNext_t)(const void *iterationId, char *hostName, afs_status_p st);
typedef int (ADMINAPI *lpBosHostGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpBosExecutableCreate_t)(const void *serverHandle, const char *sourceFile, const char *destFile, afs_status_p st);
typedef int (ADMINAPI *lpBosExecutableRevert_t)(const void *serverHandle, const char *execFile, afs_status_p st);
typedef int (ADMINAPI *lpBosExecutableTimestampGet_t)(const void *serverHandle, const char *execFile, unsigned long *newTime, unsigned long *oldTime, unsigned long *bakTime, afs_status_p st);
typedef int (ADMINAPI *lpBosExecutablePrune_t)(const void *serverHandle, bos_Prune_t oldFiles, bos_Prune_t bakFiles, bos_Prune_t coreFiles, afs_status_p st);
typedef int (ADMINAPI *lpBosExecutableRestartTimeSet_t)(const void *serverHandle, bos_Restart_t type, bos_RestartTime_t time, afs_status_p st);
typedef int (ADMINAPI *lpBosExecutableRestartTimeGet_t)(const void *serverHandle, bos_Restart_t type, bos_RestartTime_p timeP, afs_status_p st);
typedef int (ADMINAPI *lpBosLogGet_t)(const void *serverHandle, const char *log, unsigned long *logBufferSizeP, char *logData, afs_status_p st);
typedef int (ADMINAPI *lpBosAuthSet_t)(const void *serverHandle, bos_Auth_t auth, afs_status_p st);
typedef int (ADMINAPI *lpBosCommandExecute_t)(const void *serverHandle, const char *command, afs_status_p st);
typedef int (ADMINAPI *lpBosSalvage_t)(const void *cellHandle, const void *serverHandle, const char *partitionName, const char *volumeName, int numSalvagers, const char *tmpDir, const char *logFile, vos_force_t force, bos_SalvageDamagedVolumes_t salvageDamagedVolumes, bos_WriteInodes_t writeInodes, bos_WriteRootInodes_t writeRootInodes, bos_ForceDirectory_t forceDirectory, bos_ForceBlockRead_t forceBlockRead, afs_status_p st);

typedef int (ADMINAPI *lpKasServerOpen_t)(const void *cellHandle, const char **serverList, void **serverHandleP, afs_status_p st); typedef int (ADMINAPI *lpKasServerClose_t)(void *serverHandle, afs_status_p st); typedef int (ADMINAPI *lpKasServerStatsGet_t)(const void *cellHandle, const void *serverHandle, kas_serverStats_p stats, afs_status_p st); typedef int (ADMINAPI *lpKasServerDebugGet_t)(const void *cellHandle, const void *serverHandle, kas_serverDebugInfo_p debug, afs_status_p st); typedef int (ADMINAPI *lpKasServerRandomKeyGet_t)(const void *cellHandle, const void *serverHandle, kas_encryptionKey_p key, afs_status_p st); typedef int (ADMINAPI *lpKasStringToKey_t)(const char *cellName, const char *string, kas_encryptionKey_p key, afs_status_p st); typedef int (ADMINAPI *lpKasPrincipalCreate_t)(const void *cellHandle, const void *serverHandle, const kas_identity_p who, const char *password, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalDelete_t)(const void *cellHandle, const void *serverHandle, const kas_identity_p who, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalGet_t)(const void *cellHandle, const void *serverHandle, const kas_identity_p who, kas_principalEntry_p principal, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalGetBegin_t)(const void *cellHandle, const void *serverHandle, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalGetNext_t)(const void *iterationId, kas_identity_p who, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalGetDone_t)(void *iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalKeySet_t)(const void *cellHandle, const void *serverHandle, const kas_identity_p who, int keyVersion, const kas_encryptionKey_p key, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalLockStatusGet_t)(const void *cellHandle, const void *serverHandle, const kas_identity_p who, unsigned long *lock_end_timeP, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalUnlock_t)(const void *cellHandle, const void *serverHandle, const kas_identity_p who, afs_status_p st);
typedef int (ADMINAPI *lpKasPrincipalFieldsSet_t)(const void *cellHandle, const void *serverHandle, const kas_identity_p who, const kas_admin_p isAdmin, const kas_tgs_p grantTickets, const kas_enc_p canEncrypt, const kas_cpw_p canChangePassword, const unsigned int *expirationDate, const unsigned int *maxTicketLifetime, const unsigned int *passwordExpires, const kas_rpw_p passwordReuse, const unsigned int *failedPasswordAttempts, const unsigned int *failedPasswordLockTime, afs_status_p st);

typedef int (ADMINAPI *lpPtsGroupMemberAdd_t)(const void *cellHandle, const char *userName, const char *groupName, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupOwnerChange_t)(const void *cellHandle, const char *targetGroup, const char *newOwner, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupCreate_t)(const void *cellHandle, const char *newGroup, const char *newOwner, const int *newGroupIdP, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupGet_t)(const void *cellHandle, const char *groupName, pts_GroupEntry_p groupP, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupDelete_t)(const void *cellHandle, const char *group, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupMaxGet_t)(const void *cellHandle, int *maxGroupId, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupMaxSet_t)(const void *cellHandle, int maxGroupId, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupMemberListBegin_t)(const void *cellHandle, const char *groupName, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupMemberListNext_t)(const void *iterationIdP, char *memberName, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupMemberListDone_t)(const void *iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupMemberRemove_t)(const void *cellHandle, const char *memberName, const char *groupName, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupRename_t)(const void *cellHandle, const char *oldName, const char *newName, afs_status_p st);
typedef int (ADMINAPI *lpPtsGroupModify_t)(const void *cellHandle, const char *groupName, pts_GroupUpdateEntry_p newEntryP, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserCreate_t)(const void *cellHandle, const char *newUser, const int *newUserIdP, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserDelete_t)(const void *cellHandle, const char *user, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserGet_t)(const void *cellHandle, const char *userName, pts_UserEntry_p userP, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserRename_t)(const void *cellHandle, const char *oldName, const char *newName, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserModify_t)(const void *cellHandle, const char *userName, pts_UserUpdateEntry_p newEntryP, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserMaxGet_t)(const void *cellHandle, int *maxUserId, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserMaxSet_t)(const void *cellHandle, int maxUserId, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserMemberListBegin_t)(const void *cellHandle, const char *userName, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserMemberListNext_t)(const void *iterationIdP, char *groupName, afs_status_p st);
typedef int (ADMINAPI *lpPtsUserMemberListDone_t)(const void *iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpPtsOwnedGroupListBegin_t)(const void *cellHandle, const char *ownerName, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpPtsOwnedGroupListNext_t)(const void *iterationIdP, char *groupName, afs_status_p st);
typedef int (ADMINAPI *lpPtsOwnedGroupListDone_t)(const void *iterationIdP, afs_status_p st);

typedef int (ADMINAPI *lpClientTokenGetExisting_t)(const char *cellName, void **tokenHandle, afs_status_p st);
typedef int (ADMINAPI *lpClientCellOpen_t)(const char *cellName, const void *credsHandle, void **cellHandleP, afs_status_p st);
typedef int (ADMINAPI *lpClientCellClose_t)(const void *cellHandle, afs_status_p st);
typedef int (ADMINAPI *lpClientLocalCellGet_t)(char *cellName, afs_status_p st);
typedef int (ADMINAPI *lpClientAFSServerGet_t)(const void *cellHandle, const char *serverName, afs_serverEntry_p serverEntryP, afs_status_p st);
typedef int (ADMINAPI *lpClientAFSServerGetBegin_t)(const void *cellHandle, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpClientAFSServerGetNext_t)(const void *iterationId, afs_serverEntry_p serverEntryP, afs_status_p st);
typedef int (ADMINAPI *lpClientAFSServerGetDone_t)(const void *iterationId, afs_status_p st);
typedef int (ADMINAPI *lpClientInit_t)(afs_status_p st);

typedef int (ADMINAPI *lpUtilDatabaseServerGetBegin_t)(const char *cellName, void **iterationIdP, afs_status_p st);
typedef int (ADMINAPI *lpUtilDatabaseServerGetNext_t)(const void *iterationId, int *serverAddressP, afs_status_p st);
typedef int (ADMINAPI *lpUtilDatabaseServerGetDone_t)(const void *iterationId, afs_status_p st);

// ADD HERE


static lpVosBackupVolumeCreate_t lpVosBackupVolumeCreate = NULL;
static lpVosBackupVolumeCreateMultiple_t lpVosBackupVolumeCreateMultiple = NULL;
static lpVosPartitionGet_t lpVosPartitionGet = NULL;
static lpVosPartitionGetBegin_t lpVosPartitionGetBegin = NULL;
static lpVosPartitionGetNext_t lpVosPartitionGetNext = NULL;
static lpVosPartitionGetDone_t lpVosPartitionGetDone = NULL;
static lpVosServerOpen_t lpVosServerOpen = NULL;
static lpVosServerClose_t lpVosServerClose = NULL;
static lpVosServerSync_t lpVosServerSync = NULL;
static lpVosFileServerAddressChange_t lpVosFileServerAddressChange = NULL;
static lpVosFileServerAddressRemove_t lpVosFileServerAddressRemove = NULL;
static lpVosFileServerGetBegin_t lpVosFileServerGetBegin = NULL;
static lpVosFileServerGetNext_t lpVosFileServerGetNext = NULL;
static lpVosFileServerGetDone_t lpVosFileServerGetDone = NULL;
static lpVosServerTransactionStatusGetBegin_t lpVosServerTransactionStatusGetBegin = NULL;
static lpVosServerTransactionStatusGetNext_t lpVosServerTransactionStatusGetNext = NULL;
static lpVosServerTransactionStatusGetDone_t lpVosServerTransactionStatusGetDone = NULL;
static lpVosVLDBGet_t lpVosVLDBGet = NULL;
static lpVosVLDBGetBegin_t lpVosVLDBGetBegin = NULL;
static lpVosVLDBGetNext_t lpVosVLDBGetNext = NULL;
static lpVosVLDBGetDone_t lpVosVLDBGetDone = NULL;
static lpVosVLDBEntryRemove_t lpVosVLDBEntryRemove = NULL;
static lpVosVLDBUnlock_t lpVosVLDBUnlock = NULL;
static lpVosVLDBEntryLock_t lpVosVLDBEntryLock = NULL;
static lpVosVLDBEntryUnlock_t lpVosVLDBEntryUnlock = NULL;
static lpVosVLDBReadOnlySiteCreate_t lpVosVLDBReadOnlySiteCreate = NULL;
static lpVosVLDBReadOnlySiteDelete_t lpVosVLDBReadOnlySiteDelete = NULL;
static lpVosVLDBSync_t lpVosVLDBSync = NULL;
static lpVosVolumeCreate_t lpVosVolumeCreate = NULL;
static lpVosVolumeDelete_t lpVosVolumeDelete = NULL;
static lpVosVolumeRename_t lpVosVolumeRename = NULL;
static lpVosVolumeDump_t lpVosVolumeDump = NULL;
static lpVosVolumeRestore_t lpVosVolumeRestore = NULL;
static lpVosVolumeOnline_t lpVosVolumeOnline = NULL;
static lpVosVolumeOffline_t lpVosVolumeOffline = NULL;
static lpVosVolumeGet_t lpVosVolumeGet = NULL;
static lpVosVolumeGetBegin_t lpVosVolumeGetBegin = NULL;
static lpVosVolumeGetNext_t lpVosVolumeGetNext = NULL;
static lpVosVolumeGetDone_t lpVosVolumeGetDone = NULL;
static lpVosVolumeMove_t lpVosVolumeMove = NULL;
static lpVosVolumeRelease_t lpVosVolumeRelease = NULL;
static lpVosVolumeZap_t lpVosVolumeZap = NULL;
static lpVosPartitionNameToId_t lpVosPartitionNameToId = NULL;
static lpVosPartitionIdToName_t lpVosPartitionIdToName = NULL;
static lpVosVolumeQuotaChange_t lpVosVolumeQuotaChange = NULL;

static lpBosServerOpen_t lpBosServerOpen = NULL;
static lpBosServerClose_t lpBosServerClose = NULL;
static lpBosProcessCreate_t lpBosProcessCreate = NULL;
static lpBosProcessDelete_t lpBosProcessDelete = NULL;
static lpBosProcessExecutionStateGet_t lpBosProcessExecutionStateGet = NULL;
static lpBosProcessExecutionStateSet_t lpBosProcessExecutionStateSet = NULL;
static lpBosProcessExecutionStateSetTemporary_t lpBosProcessExecutionStateSetTemporary = NULL;
static lpBosProcessNameGetBegin_t lpBosProcessNameGetBegin = NULL;
static lpBosProcessNameGetDone_t lpBosProcessNameGetDone = NULL;
static lpBosProcessNameGetNext_t lpBosProcessNameGetNext = NULL;
static lpBosProcessInfoGet_t lpBosProcessInfoGet = NULL;
static lpBosProcessParameterGetBegin_t lpBosProcessParameterGetBegin = NULL;
static lpBosProcessParameterGetDone_t lpBosProcessParameterGetDone = NULL;
static lpBosProcessParameterGetNext_t lpBosProcessParameterGetNext = NULL;
static lpBosProcessNotifierGet_t lpBosProcessNotifierGet = NULL;
static lpBosProcessRestart_t lpBosProcessRestart = NULL;
static lpBosProcessAllStop_t lpBosProcessAllStop = NULL;
static lpBosProcessAllStart_t lpBosProcessAllStart = NULL;
static lpBosProcessAllWaitStop_t lpBosProcessAllWaitStop = NULL;
static lpBosProcessAllStopAndRestart_t lpBosProcessAllStopAndRestart = NULL;
static lpBosAdminCreate_t lpBosAdminCreate = NULL;
static lpBosAdminDelete_t lpBosAdminDelete = NULL;
static lpBosAdminGetBegin_t lpBosAdminGetBegin = NULL;
static lpBosAdminGetDone_t lpBosAdminGetDone = NULL;
static lpBosAdminGetNext_t lpBosAdminGetNext = NULL;
static lpBosKeyCreate_t lpBosKeyCreate = NULL;
static lpBosKeyDelete_t lpBosKeyDelete = NULL;
static lpBosKeyGetBegin_t lpBosKeyGetBegin = NULL;
static lpBosKeyGetDone_t lpBosKeyGetDone = NULL;
static lpBosKeyGetNext_t lpBosKeyGetNext = NULL;
static lpBosCellSet_t lpBosCellSet = NULL;
static lpBosCellGet_t lpBosCellGet = NULL;
static lpBosHostCreate_t lpBosHostCreate = NULL;
static lpBosHostDelete_t lpBosHostDelete = NULL;
static lpBosHostGetBegin_t lpBosHostGetBegin = NULL;
static lpBosHostGetDone_t lpBosHostGetDone = NULL;
static lpBosHostGetNext_t lpBosHostGetNext = NULL;
static lpBosExecutableCreate_t lpBosExecutableCreate = NULL;
static lpBosExecutableRevert_t lpBosExecutableRevert = NULL;
static lpBosExecutableTimestampGet_t lpBosExecutableTimestampGet = NULL;
static lpBosExecutablePrune_t lpBosExecutablePrune = NULL;
static lpBosExecutableRestartTimeSet_t lpBosExecutableRestartTimeSet = NULL;
static lpBosExecutableRestartTimeGet_t lpBosExecutableRestartTimeGet = NULL;
static lpBosLogGet_t lpBosLogGet = NULL;
static lpBosAuthSet_t lpBosAuthSet = NULL;
static lpBosCommandExecute_t lpBosCommandExecute = NULL;
static lpBosSalvage_t lpBosSalvage = NULL;

static lpKasServerOpen_t lpKasServerOpen = NULL;
static lpKasServerClose_t lpKasServerClose = NULL;
static lpKasServerStatsGet_t lpKasServerStatsGet = NULL;
static lpKasServerDebugGet_t lpKasServerDebugGet = NULL;
static lpKasServerRandomKeyGet_t lpKasServerRandomKeyGet = NULL;
static lpKasStringToKey_t lpKasStringToKey = NULL;
static lpKasPrincipalCreate_t lpKasPrincipalCreate = NULL;
static lpKasPrincipalDelete_t lpKasPrincipalDelete = NULL;
static lpKasPrincipalGet_t lpKasPrincipalGet = NULL;
static lpKasPrincipalGetBegin_t lpKasPrincipalGetBegin = NULL;
static lpKasPrincipalGetNext_t lpKasPrincipalGetNext = NULL;
static lpKasPrincipalGetDone_t lpKasPrincipalGetDone = NULL;
static lpKasPrincipalKeySet_t lpKasPrincipalKeySet = NULL;
static lpKasPrincipalLockStatusGet_t lpKasPrincipalLockStatusGet = NULL;
static lpKasPrincipalUnlock_t lpKasPrincipalUnlock = NULL;
static lpKasPrincipalFieldsSet_t lpKasPrincipalFieldsSet = NULL;

static lpPtsGroupMemberAdd_t lpPtsGroupMemberAdd = NULL;
static lpPtsGroupOwnerChange_t lpPtsGroupOwnerChange = NULL;
static lpPtsGroupCreate_t lpPtsGroupCreate = NULL;
static lpPtsGroupGet_t lpPtsGroupGet = NULL;
static lpPtsGroupDelete_t lpPtsGroupDelete = NULL;
static lpPtsGroupMaxGet_t lpPtsGroupMaxGet = NULL;
static lpPtsGroupMaxSet_t lpPtsGroupMaxSet = NULL;
static lpPtsGroupMemberListBegin_t lpPtsGroupMemberListBegin = NULL;
static lpPtsGroupMemberListNext_t lpPtsGroupMemberListNext = NULL;
static lpPtsGroupMemberListDone_t lpPtsGroupMemberListDone = NULL;
static lpPtsGroupMemberRemove_t lpPtsGroupMemberRemove = NULL;
static lpPtsGroupRename_t lpPtsGroupRename = NULL;
static lpPtsGroupModify_t lpPtsGroupModify = NULL;
static lpPtsUserCreate_t lpPtsUserCreate = NULL;
static lpPtsUserDelete_t lpPtsUserDelete = NULL;
static lpPtsUserGet_t lpPtsUserGet = NULL;
static lpPtsUserRename_t lpPtsUserRename = NULL;
static lpPtsUserModify_t lpPtsUserModify = NULL;
static lpPtsUserMaxGet_t lpPtsUserMaxGet = NULL;
static lpPtsUserMaxSet_t lpPtsUserMaxSet = NULL;
static lpPtsUserMemberListBegin_t lpPtsUserMemberListBegin = NULL;
static lpPtsUserMemberListNext_t lpPtsUserMemberListNext = NULL;
static lpPtsUserMemberListDone_t lpPtsUserMemberListDone = NULL;
static lpPtsOwnedGroupListBegin_t lpPtsOwnedGroupListBegin = NULL;
static lpPtsOwnedGroupListNext_t lpPtsOwnedGroupListNext = NULL;
static lpPtsOwnedGroupListDone_t lpPtsOwnedGroupListDone = NULL;

static lpClientTokenGetExisting_t lpClientTokenGetExisting = NULL;
static lpClientCellOpen_t lpClientCellOpen = NULL;
static lpClientCellClose_t lpClientCellClose = NULL;
static lpClientLocalCellGet_t lpClientLocalCellGet = NULL;
static lpClientAFSServerGet_t lpClientAFSServerGet = NULL;
static lpClientAFSServerGetBegin_t lpClientAFSServerGetBegin = NULL;
static lpClientAFSServerGetNext_t lpClientAFSServerGetNext = NULL;
static lpClientAFSServerGetDone_t lpClientAFSServerGetDone = NULL;
static lpClientInit_t lpClientInit = NULL;

static lpUtilDatabaseServerGetBegin_t lpUtilDatabaseServerGetBegin = NULL;
static lpUtilDatabaseServerGetNext_t lpUtilDatabaseServerGetNext = NULL;
static lpUtilDatabaseServerGetDone_t lpUtilDatabaseServerGetDone = NULL;

// ADD HERE

} // extern "C"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL Worker_LoadLibraries (ULONG *pStatus);
void Worker_FreeLibraries (void);

DWORD Worker_PerformTask (WORKERTASK wtask, LPWORKERPACKET lpwp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL Worker_Initialize (ULONG *pStatus)
{
   static BOOL fInitialized = FALSE;

   if (!fInitialized)
      {
      fInitialized = Worker_LoadLibraries (pStatus);
      }

   return fInitialized;
}


BOOL Worker_DoTask (WORKERTASK wtask, LPWORKERPACKET lpwp, ULONG *pStatus)
{
   if (!Worker_Initialize (pStatus))
      return FALSE;

   ULONG status;
   try {
      status = Worker_PerformTask (wtask, lpwp);
   } catch(...) {
      status = ERROR_UNEXP_NET_ERR;
   }
   if (pStatus)
      *pStatus = status;
   return (status == 0) ? TRUE : FALSE;
}


DWORD Worker_PerformTask (WORKERTASK wtask, LPWORKERPACKET lpwp)
{
   afs_status_t status = 0;

   switch (wtask)
      {
      case wtaskVosBackupVolumeCreate:
         {
         if ((*lpVosBackupVolumeCreate)(lpwp->wpVosBackupVolumeCreate.hCell, NULL, lpwp->wpVosBackupVolumeCreate.idVolume, &status))
            status = 0;
         break;
         }

      case wtaskVosBackupVolumeCreateMultiple:
         {
         LPSTR pszPrefixA = StringToAnsi (lpwp->wpVosBackupVolumeCreateMultiple.pszPrefix);

         unsigned int *pPartition;
         if (*(pPartition = (unsigned int *)&lpwp->wpVosBackupVolumeCreateMultiple.idPartition) == NO_PARTITION)
            pPartition = NULL;

         if ((*lpVosBackupVolumeCreateMultiple)(lpwp->wpVosBackupVolumeCreateMultiple.hCell, lpwp->wpVosBackupVolumeCreateMultiple.hServer, NULL, pPartition, pszPrefixA, (lpwp->wpVosBackupVolumeCreateMultiple.fExclude) ? VOS_EXCLUDE : VOS_INCLUDE, &status))
            status = 0;

         FreeString (pszPrefixA, lpwp->wpVosBackupVolumeCreateMultiple.pszPrefix);
         break;
         }

      case wtaskVosPartitionGet:
         {
         if ((*lpVosPartitionGet)(lpwp->wpVosPartitionGet.hCell, lpwp->wpVosPartitionGet.hServer, NULL, lpwp->wpVosPartitionGet.idPartition, &lpwp->wpVosPartitionGet.Data, &status))
            status = 0;
         break;
         }

      case wtaskVosPartitionGetBegin:
         {
         if ((*lpVosPartitionGetBegin)(lpwp->wpVosPartitionGetBegin.hCell, lpwp->wpVosPartitionGetBegin.hServer, NULL, &lpwp->wpVosPartitionGetBegin.hEnum, &status))
            status = 0;
         else
            lpwp->wpVosPartitionGetBegin.hEnum = NULL;
         break;
         }

      case wtaskVosPartitionGetNext:
         {
         if ((*lpVosPartitionGetNext)(lpwp->wpVosPartitionGetNext.hEnum, &lpwp->wpVosPartitionGetNext.Data, &status))
            status = 0;
         break;
         }

      case wtaskVosPartitionGetDone:
         {
         if ((*lpVosPartitionGetDone)(lpwp->wpVosPartitionGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosServerOpen:
         {
         LPSTR pszServerA = StringToAnsi (lpwp->wpVosServerOpen.pszServer);

         if ((*lpVosServerOpen)(lpwp->wpVosServerOpen.hCell, pszServerA, &lpwp->wpVosServerOpen.hServer, &status))
            status = 0;

         FreeString (pszServerA, lpwp->wpVosServerOpen.pszServer);
         break;
         }

      case wtaskVosServerClose:
         {
         if ((*lpVosServerClose)(lpwp->wpVosServerClose.hServer, &status))
            status = 0;
         break;
         }

      case wtaskVosServerSync:
         {
         unsigned int *pPartition;
         if (*(pPartition = (unsigned int *)&lpwp->wpVosServerSync.idPartition) == NO_PARTITION)
            pPartition = NULL;

         if ((*lpVosServerSync)(lpwp->wpVosServerSync.hCell, lpwp->wpVosServerSync.hServer, NULL, pPartition, (lpwp->wpVosServerSync.fForce) ? VOS_FORCE : VOS_NORMAL, &status))
            status = 0;
         break;
         }

      case wtaskVosFileServerAddressChange:
         {
         int oldAddress;
         int newAddress;
         AfsClass_AddressToInt (&oldAddress, &lpwp->wpVosFileServerAddressChange.addrOld);
         AfsClass_AddressToInt (&newAddress, &lpwp->wpVosFileServerAddressChange.addrNew);

         if ((*lpVosFileServerAddressChange)(lpwp->wpVosFileServerAddressChange.hCell, NULL, oldAddress, newAddress, &status))
            status = 0;
         break;
         }

      case wtaskVosFileServerAddressRemove:
         {
         int oldAddress;
         AfsClass_AddressToInt (&oldAddress, &lpwp->wpVosFileServerAddressRemove.addr);

         if ((*lpVosFileServerAddressRemove)(lpwp->wpVosFileServerAddressRemove.hCell, NULL, oldAddress, &status))
            status = 0;

         if (status == VL_NOENT)  // RemoveAddress attempted on pre-AFS3.5?
            status = ENOSYS;
         break;
         }

      case wtaskVosFileServerGetBegin:
         {
         if ((*lpVosFileServerGetBegin)(lpwp->wpVosFileServerGetBegin.hCell, NULL, &lpwp->wpVosFileServerGetBegin.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosFileServerGetNext:
         {
         if ((*lpVosFileServerGetNext)(lpwp->wpVosFileServerGetNext.hEnum, &lpwp->wpVosFileServerGetNext.Entry, &status))
            status = 0;
         break;
         }

      case wtaskVosFileServerGetDone:
         {
         if ((*lpVosFileServerGetDone)(lpwp->wpVosFileServerGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosServerTransactionStatusGetBegin:
         {
         if ((*lpVosServerTransactionStatusGetBegin)(lpwp->wpVosServerTransactionStatusGetBegin.hCell, lpwp->wpVosServerTransactionStatusGetBegin.hServer, NULL, &lpwp->wpVosServerTransactionStatusGetBegin.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosServerTransactionStatusGetNext:
         {
         if ((*lpVosServerTransactionStatusGetNext)(lpwp->wpVosServerTransactionStatusGetNext.hEnum, &lpwp->wpVosServerTransactionStatusGetNext.Data, &status))
            status = 0;
         break;
         }

      case wtaskVosServerTransactionStatusGetDone:
         {
         if ((*lpVosServerTransactionStatusGetDone)(lpwp->wpVosServerTransactionStatusGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBGet:
         {
         if ((*lpVosVLDBGet)(lpwp->wpVosVLDBGet.hCell, NULL, &lpwp->wpVosVLDBGet.idVolume, NULL, &lpwp->wpVosVLDBGet.Data, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBGetBegin:
         {
         unsigned int *pPartition;
         if (*(pPartition = (unsigned int *)&lpwp->wpVosVLDBGetBegin.idPartition) == NO_PARTITION)
            pPartition = NULL;

         if ((*lpVosVLDBGetBegin)(lpwp->wpVosVLDBGetBegin.hCell, lpwp->wpVosVLDBGetBegin.hServer, NULL, pPartition, &lpwp->wpVosVLDBGetBegin.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBGetNext:
         {
         if ((*lpVosVLDBGetNext)(lpwp->wpVosVLDBGetNext.hEnum, &lpwp->wpVosVLDBGetNext.Data, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBGetDone:
         {
         if ((*lpVosVLDBGetDone)(lpwp->wpVosVLDBGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBEntryRemove:
         {
         unsigned int *pPartition;
         if (*(pPartition = (unsigned int *)&lpwp->wpVosVLDBEntryRemove.idPartition) == NO_PARTITION)
            pPartition = NULL;

         unsigned int *pVolume;
         if (*(pVolume = (unsigned int *)&lpwp->wpVosVLDBEntryRemove.idVolume) == NO_VOLUME)
            pVolume = NULL;

         if ((*lpVosVLDBEntryRemove)(lpwp->wpVosVLDBEntryRemove.hCell, lpwp->wpVosVLDBEntryRemove.hServer, NULL, pPartition, pVolume, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBEntryLock:
         {
         if ((*lpVosVLDBEntryLock)(lpwp->wpVosVLDBEntryLock.hCell, NULL, lpwp->wpVosVLDBEntryLock.idVolume, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBEntryUnlock:
         {
         if (lpwp->wpVosVLDBEntryUnlock.idVolume == NO_VOLUME)
            {
            unsigned int *pPartition;
            if (*(pPartition = (unsigned int *)&lpwp->wpVosVLDBEntryUnlock.idPartition) == NO_PARTITION)
               pPartition = NULL;

            if ((*lpVosVLDBUnlock)(lpwp->wpVosVLDBEntryUnlock.hCell, lpwp->wpVosVLDBEntryUnlock.hServer, NULL, pPartition, &status))
               status = 0;
            }
         else
            {
            if ((*lpVosVLDBEntryUnlock)(lpwp->wpVosVLDBEntryUnlock.hCell, NULL, lpwp->wpVosVLDBEntryUnlock.idVolume, &status))
               status = 0;
            }
         break;
         }

      case wtaskVosVLDBReadOnlySiteCreate:
         {
         if ((*lpVosVLDBReadOnlySiteCreate)(lpwp->wpVosVLDBReadOnlySiteCreate.hCell, lpwp->wpVosVLDBReadOnlySiteCreate.hServer, NULL, lpwp->wpVosVLDBReadOnlySiteCreate.idPartition, lpwp->wpVosVLDBReadOnlySiteCreate.idVolume, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBReadOnlySiteDelete:
         {
         if ((*lpVosVLDBReadOnlySiteDelete)(lpwp->wpVosVLDBReadOnlySiteDelete.hCell, lpwp->wpVosVLDBReadOnlySiteDelete.hServer, NULL, lpwp->wpVosVLDBReadOnlySiteDelete.idPartition, lpwp->wpVosVLDBReadOnlySiteDelete.idVolume, &status))
            status = 0;
         break;
         }

      case wtaskVosVLDBSync:
         {
         unsigned int *pPartition;
         if (*(pPartition = (unsigned int *)&lpwp->wpVosVLDBSync.idPartition) == NO_PARTITION)
            pPartition = NULL;

         if ((*lpVosVLDBSync)(lpwp->wpVosVLDBSync.hCell, lpwp->wpVosVLDBSync.hServer, NULL, pPartition, ((lpwp->wpVosVLDBSync.fForce) ? VOS_FORCE : VOS_NORMAL), &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeCreate:
         {
         LPSTR pszVolumeA = StringToAnsi (lpwp->wpVosVolumeCreate.pszVolume);

         if ((*lpVosVolumeCreate)(lpwp->wpVosVolumeCreate.hCell, lpwp->wpVosVolumeCreate.hServer, NULL, lpwp->wpVosVolumeCreate.idPartition, pszVolumeA, (unsigned int)lpwp->wpVosVolumeCreate.ckQuota, (unsigned int *)&lpwp->wpVosVolumeCreate.idVolume, &status))
            status = 0;

         FreeString (pszVolumeA, lpwp->wpVosVolumeCreate.pszVolume);
         break;
         }

      case wtaskVosVolumeDelete:
         {
         if ((*lpVosVolumeDelete)(lpwp->wpVosVolumeDelete.hCell, lpwp->wpVosVolumeDelete.hServer, NULL, lpwp->wpVosVolumeDelete.idPartition, lpwp->wpVosVolumeDelete.idVolume, &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeRename:
         {
         LPSTR pszVolumeA = StringToAnsi (lpwp->wpVosVolumeRename.pszVolume);

         if ((*lpVosVolumeRename)(lpwp->wpVosVolumeRename.hCell, NULL, lpwp->wpVosVolumeRename.idVolume, pszVolumeA, &status))
            status = 0;

         FreeString (pszVolumeA, lpwp->wpVosVolumeRename.pszVolume);
         break;
         }

      case wtaskVosVolumeDump:
         {
         LPSTR pszFilenameA = StringToAnsi (lpwp->wpVosVolumeDump.pszFilename);
         ULONG timeStart = AfsClass_SystemTimeToUnixTime (&lpwp->wpVosVolumeDump.stStart);

         if ((*lpVosVolumeDump)(lpwp->wpVosVolumeDump.hCell, lpwp->wpVosVolumeDump.hServer, NULL, (unsigned int *)&lpwp->wpVosVolumeDump.idPartition, lpwp->wpVosVolumeDump.idVolume, timeStart, pszFilenameA, &status))
            status = 0;

         FreeString (pszFilenameA, lpwp->wpVosVolumeDump.pszFilename);
         break;
         }

      case wtaskVosVolumeRestore:
         {
         LPSTR pszVolumeA = StringToAnsi (lpwp->wpVosVolumeRestore.pszVolume);
         LPSTR pszFilenameA = StringToAnsi (lpwp->wpVosVolumeRestore.pszFilename);
         vos_volumeRestoreType_t Type = (lpwp->wpVosVolumeRestore.fIncremental) ? VOS_RESTORE_INCREMENTAL : VOS_RESTORE_FULL;

         unsigned int *pVolume;
         if (*(pVolume = (unsigned int *)&lpwp->wpVosVolumeRestore.idVolume) == NO_VOLUME)
            pVolume = NULL;

         if ((*lpVosVolumeRestore)(lpwp->wpVosVolumeRestore.hCell, lpwp->wpVosVolumeRestore.hServer, NULL, lpwp->wpVosVolumeRestore.idPartition, pVolume, pszVolumeA, pszFilenameA, Type, &status))
            status = 0;

         FreeString (pszFilenameA, lpwp->wpVosVolumeRestore.pszFilename);
         FreeString (pszVolumeA, lpwp->wpVosVolumeRestore.pszVolume);
         break;
         }

      case wtaskVosVolumeOnline:
         {
         if ((*lpVosVolumeOnline)(lpwp->wpVosVolumeOnline.hServer, NULL, lpwp->wpVosVolumeOnline.idPartition, lpwp->wpVosVolumeOnline.idVolume, lpwp->wpVosVolumeOnline.csecSleep, lpwp->wpVosVolumeOnline.Status, &status)) 
            status = 0;
         break;
         }

      case wtaskVosVolumeOffline:
         {
         if ((*lpVosVolumeOffline)(lpwp->wpVosVolumeOffline.hServer, NULL, lpwp->wpVosVolumeOffline.idPartition, lpwp->wpVosVolumeOffline.idVolume, &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeGet:
         {
         if ((*lpVosVolumeGet)(lpwp->wpVosVolumeGet.hCell, lpwp->wpVosVolumeGet.hServer, NULL, lpwp->wpVosVolumeGet.idPartition, lpwp->wpVosVolumeGet.idVolume, &lpwp->wpVosVolumeGet.Data, &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeGetBegin:
         {
         if ((lpVosVolumeGetBegin)(lpwp->wpVosVolumeGetBegin.hCell, lpwp->wpVosVolumeGetBegin.hServer, NULL, lpwp->wpVosVolumeGetBegin.idPartition, &lpwp->wpVosVolumeGetBegin.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeGetNext:
         {
         if ((*lpVosVolumeGetNext)(lpwp->wpVosVolumeGetNext.hEnum, &lpwp->wpVosVolumeGetNext.Data, &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeGetDone:
         {
         if ((*lpVosVolumeGetDone)(lpwp->wpVosVolumeGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeMove:
         {
         if ((lpVosVolumeMove)(lpwp->wpVosVolumeMove.hCell, NULL, lpwp->wpVosVolumeMove.idVolume, lpwp->wpVosVolumeMove.hServerFrom, lpwp->wpVosVolumeMove.idPartitionFrom, lpwp->wpVosVolumeMove.hServerTo, lpwp->wpVosVolumeMove.idPartitionTo, &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeRelease:
         {
         if ((*lpVosVolumeRelease)(lpwp->wpVosVolumeRelease.hCell, NULL, lpwp->wpVosVolumeRelease.idVolume, ((lpwp->wpVosVolumeRelease.fForce) ? VOS_FORCE : VOS_NORMAL), &status))
            status = 0;
         break;
         }

      case wtaskVosVolumeZap:
         {
         if ((*lpVosVolumeZap)(lpwp->wpVosVolumeZap.hCell, lpwp->wpVosVolumeZap.hServer, NULL, lpwp->wpVosVolumeZap.idPartition, lpwp->wpVosVolumeZap.idVolume, ((lpwp->wpVosVolumeZap.fForce) ? VOS_FORCE : VOS_NORMAL), &status))
            status = 0;
         break;
         }

      case wtaskVosPartitionNameToId:
         {
         LPSTR pszPartitionA = StringToAnsi (lpwp->wpVosPartitionNameToId.pszPartition);

         if ((*lpVosPartitionNameToId)(pszPartitionA, (unsigned int *)&lpwp->wpVosPartitionNameToId.idPartition, &status))
            status = 0;

         FreeString (pszPartitionA, lpwp->wpVosPartitionNameToId.pszPartition);
         break;
         }

      case wtaskVosPartitionIdToName:
         {
         char szPartitionA[ cchNAME ];
         if ((*lpVosPartitionIdToName)(lpwp->wpVosPartitionIdToName.idPartition, szPartitionA, &status))
            {
            CopyAnsiToString (lpwp->wpVosPartitionIdToName.pszPartition, szPartitionA);
            status = 0;
            }
         break;
         }

      case wtaskVosVolumeQuotaChange:
         {
         if ((*lpVosVolumeQuotaChange)(lpwp->wpVosVolumeQuotaChange.hCell, lpwp->wpVosVolumeQuotaChange.hServer, NULL, lpwp->wpVosVolumeQuotaChange.idPartition, lpwp->wpVosVolumeQuotaChange.idVolume, (unsigned int)lpwp->wpVosVolumeQuotaChange.ckQuota, &status))
            status = 0;
         break;
         }

      case wtaskBosServerOpen:
         {
         LPSTR pszServerA = StringToAnsi (lpwp->wpBosServerOpen.pszServer);

         if ((*lpBosServerOpen)(lpwp->wpBosServerOpen.hCell, pszServerA, &lpwp->wpBosServerOpen.hServer, &status))
            status = 0;

         FreeString (pszServerA, lpwp->wpBosServerOpen.pszServer);
         break;
         }

      case wtaskBosServerClose:
         {
         if ((*lpBosServerClose)(lpwp->wpBosServerClose.hServer, &status))
            status = 0;
         break;
         }

      case wtaskBosProcessCreate:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessCreate.pszService);
         LPSTR pszCommandA = StringToAnsi (lpwp->wpBosProcessCreate.pszCommand);
         LPSTR pszTimeCronA = StringToAnsi (lpwp->wpBosProcessCreate.pszTimeCron);
         LPSTR pszNotifierA = StringToAnsi (lpwp->wpBosProcessCreate.pszNotifier);

         bos_ProcessType_t processType;
         switch (lpwp->wpBosProcessCreate.type)
            {
            case SERVICETYPE_FS:
               processType = BOS_PROCESS_FS;
               break;
            case SERVICETYPE_CRON:
               processType = BOS_PROCESS_CRON;
               break;
            default:
            case SERVICETYPE_SIMPLE:
               processType = BOS_PROCESS_SIMPLE;
               break;
            }

         if ((*lpBosProcessCreate)(lpwp->wpBosProcessCreate.hServer, pszServiceA, processType, pszCommandA, pszTimeCronA, pszNotifierA, &status))
            status = 0;

         FreeString (pszNotifierA, lpwp->wpBosProcessCreate.pszNotifier);
         FreeString (pszTimeCronA, lpwp->wpBosProcessCreate.pszTimeCron);
         FreeString (pszCommandA, lpwp->wpBosProcessCreate.pszCommand);
         FreeString (pszServiceA, lpwp->wpBosProcessCreate.pszService);
         break;
         }

      case wtaskBosProcessDelete:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessDelete.pszService);

         if ((*lpBosProcessDelete)(lpwp->wpBosProcessDelete.hServer, pszServiceA, &status))
            status = 0;

         FreeString (pszServiceA, lpwp->wpBosProcessDelete.pszService);
         break;
         }

      case wtaskBosProcessExecutionStateGet:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessExecutionStateGet.pszService);
         char szAuxStatusA[BOS_MAX_NAME_LEN] = "";

         bos_ProcessExecutionState_t processState = BOS_PROCESS_STOPPED;
         if ((*lpBosProcessExecutionStateGet)(lpwp->wpBosProcessExecutionStateGet.hServer, pszServiceA, &processState, szAuxStatusA, &status))
            {
            status = 0;

            if (lpwp->wpBosProcessExecutionStateGet.pszAuxStatus != NULL)
               {
               CopyAnsiToString (lpwp->wpBosProcessExecutionStateGet.pszAuxStatus, szAuxStatusA);
               }

            switch (processState)
               {
               case BOS_PROCESS_RUNNING:
                  lpwp->wpBosProcessExecutionStateGet.state = SERVICESTATE_RUNNING;
                  break;
               case BOS_PROCESS_STARTING:
                  lpwp->wpBosProcessExecutionStateGet.state = SERVICESTATE_STARTING;
                  break;
               case BOS_PROCESS_STOPPING:
                  lpwp->wpBosProcessExecutionStateGet.state = SERVICESTATE_STOPPING;
                  break;
               default:
               case BOS_PROCESS_STOPPED:
                  lpwp->wpBosProcessExecutionStateGet.state = SERVICESTATE_STOPPED;
                  break;
               }
            }

         FreeString (pszServiceA, lpwp->wpBosProcessExecutionStateGet.pszService);
         break;
         }

      case wtaskBosProcessExecutionStateSet:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessExecutionStateSet.pszService);

         bos_ProcessExecutionState_t processState;
         switch (lpwp->wpBosProcessExecutionStateSet.state)
            {
            case SERVICESTATE_STOPPING:
            case SERVICESTATE_STOPPED:
               processState = BOS_PROCESS_STOPPED;
               break;
            default:
            case SERVICESTATE_STARTING:
            case SERVICESTATE_RUNNING:
               processState = BOS_PROCESS_RUNNING;
               break;
            }

         if ((*lpBosProcessExecutionStateSet)(lpwp->wpBosProcessExecutionStateSet.hServer, pszServiceA, processState, &status))
            status = 0;

         FreeString (pszServiceA, lpwp->wpBosProcessExecutionStateSet.pszService);
         break;
         }

      case wtaskBosProcessExecutionStateSetTemporary:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessExecutionStateSetTemporary.pszService);

         bos_ProcessExecutionState_t processState;
         switch (lpwp->wpBosProcessExecutionStateSetTemporary.state)
            {
            case SERVICESTATE_STOPPING:
            case SERVICESTATE_STOPPED:
               processState = BOS_PROCESS_STOPPED;
               break;
            default:
            case SERVICESTATE_STARTING:
            case SERVICESTATE_RUNNING:
               processState = BOS_PROCESS_RUNNING;
               break;
            }

         if ((*lpBosProcessExecutionStateSetTemporary)(lpwp->wpBosProcessExecutionStateSetTemporary.hServer, pszServiceA, processState, &status))
            status = 0;

         FreeString (pszServiceA, lpwp->wpBosProcessExecutionStateSetTemporary.pszService);
         break;
         }

      case wtaskBosProcessNameGetBegin:
         {
         if (!(*lpBosProcessNameGetBegin)(lpwp->wpBosProcessNameGetBegin.hServer, &lpwp->wpBosProcessNameGetBegin.hEnum, &status))
            lpwp->wpBosProcessNameGetBegin.hEnum = 0;
         else
            status = 0;
         break;
         }

      case wtaskBosProcessNameGetDone:
         {
         if ((*lpBosProcessNameGetDone)(lpwp->wpBosProcessNameGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskBosProcessNameGetNext:
         {
         char szServiceA[ 256 ];
         if (!(*lpBosProcessNameGetNext)(lpwp->wpBosProcessNameGetNext.hEnum, szServiceA, &status))
            lpwp->wpBosProcessNameGetNext.pszService[0] = TEXT('\0');
         else
            {
            status = 0;
            CopyAnsiToString (lpwp->wpBosProcessNameGetNext.pszService, szServiceA);
            }
         break;
         }

      case wtaskBosProcessInfoGet:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessInfoGet.pszService);
         memset (&lpwp->wpBosProcessInfoGet.ss, 0x00, sizeof(SERVICESTATUS));

         bos_ProcessType_t processType;
         bos_ProcessInfo_t processInfo;
         if ((*lpBosProcessInfoGet)(lpwp->wpBosProcessInfoGet.hServer, pszServiceA, &processType, &processInfo, &status))
            {
            status = 0;

            switch (processType)
               {
               case BOS_PROCESS_FS:
                  lpwp->wpBosProcessInfoGet.ss.type = SERVICETYPE_FS;
                  break;
               case BOS_PROCESS_SIMPLE:
                  lpwp->wpBosProcessInfoGet.ss.type = SERVICETYPE_SIMPLE;
                  break;
               case BOS_PROCESS_CRON:
                  lpwp->wpBosProcessInfoGet.ss.type = SERVICETYPE_CRON;
                  break;
               }

            AfsClass_UnixTimeToSystemTime (&lpwp->wpBosProcessInfoGet.ss.timeLastStart, processInfo.processStartTime);
            AfsClass_UnixTimeToSystemTime (&lpwp->wpBosProcessInfoGet.ss.timeLastStop, processInfo.processExitTime);
            AfsClass_UnixTimeToSystemTime (&lpwp->wpBosProcessInfoGet.ss.timeLastFail, processInfo.processExitErrorTime);
            lpwp->wpBosProcessInfoGet.ss.nStarts = processInfo.numberProcessStarts;
            lpwp->wpBosProcessInfoGet.ss.dwErrLast = processInfo.processErrorCode;
            lpwp->wpBosProcessInfoGet.ss.dwSigLast = processInfo.processErrorSignal;
            }

         FreeString (pszServiceA, lpwp->wpBosProcessInfoGet.pszService);
         break;
         }

      case wtaskBosProcessParameterGetBegin:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessParameterGetBegin.pszService);

         if (!(*lpBosProcessParameterGetBegin)(lpwp->wpBosProcessParameterGetBegin.hServer, pszServiceA, &lpwp->wpBosProcessParameterGetBegin.hEnum, &status))
            lpwp->wpBosProcessParameterGetBegin.hEnum = 0;
         else
            status = 0;

         FreeString (pszServiceA, lpwp->wpBosProcessParameterGetBegin.pszService);
         break;
         }

      case wtaskBosProcessParameterGetDone:
         {
         if ((*lpBosProcessParameterGetDone)(lpwp->wpBosProcessParameterGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskBosProcessParameterGetNext:
         {
         char szParamA[ 1024 ];

         if (!(*lpBosProcessParameterGetNext)(lpwp->wpBosProcessParameterGetNext.hEnum, szParamA, &status))
            lpwp->wpBosProcessParameterGetNext.pszParam[0] = TEXT('\0');
         else
            {
            status = 0;
            CopyAnsiToString (lpwp->wpBosProcessParameterGetNext.pszParam, szParamA);
            }
         break;
         }

      case wtaskBosProcessNotifierGet:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessNotifierGet.pszService);
         char szNotifierA[ 1024 ];

         if ((*lpBosProcessNotifierGet)(lpwp->wpBosProcessNotifierGet.hServer, pszServiceA, szNotifierA, &status))
            {
            status = 0;
            CopyAnsiToString (lpwp->wpBosProcessNotifierGet.pszNotifier, szNotifierA);
            }

         FreeString (pszServiceA, lpwp->wpBosProcessNotifierGet.pszService);
         break;
         }

      case wtaskBosProcessRestart:
         {
         LPSTR pszServiceA = StringToAnsi (lpwp->wpBosProcessRestart.pszService);
         if ((*lpBosProcessRestart)(lpwp->wpBosProcessRestart.hServer, pszServiceA, &status))
            status = 0;

         FreeString (pszServiceA, lpwp->wpBosProcessRestart.pszService);
         break;
         }

      case wtaskBosProcessAllStop:
         {
         if ((*lpBosProcessAllStop)(lpwp->wpBosProcessAllStop.hServer, &status))
            status = 0;
         break;
         }

      case wtaskBosProcessAllStart:
         {
         if ((*lpBosProcessAllStart)(lpwp->wpBosProcessAllStart.hServer, &status))
            status = 0;
         break;
         }

      case wtaskBosProcessAllWaitStop:
         {
         if ((*lpBosProcessAllWaitStop)(lpwp->wpBosProcessAllWaitStop.hServer, &status))
            status = 0;
         break;
         }

      case wtaskBosProcessAllStopAndRestart:
         {
         bos_RestartBosServer_t bBosToo;
         bBosToo = (lpwp->wpBosProcessAllStopAndRestart.fRestartBOS) ? BOS_RESTART_BOS_SERVER : BOS_DONT_RESTART_BOS_SERVER;

         if ((*lpBosProcessAllStopAndRestart)(lpwp->wpBosProcessAllStopAndRestart.hServer, bBosToo, &status))
            status = 0;
         break;
         }

      case wtaskBosAdminCreate:
         {
         LPSTR pszAdminA = StringToAnsi (lpwp->wpBosAdminCreate.pszAdmin);
         if ((*lpBosAdminCreate)(lpwp->wpBosAdminCreate.hServer, pszAdminA, &status))
            status = 0;
         FreeString (pszAdminA, lpwp->wpBosAdminCreate.pszAdmin);
         break;
         }

      case wtaskBosAdminDelete:
         {
         LPSTR pszAdminA = StringToAnsi (lpwp->wpBosAdminDelete.pszAdmin);
         if ((*lpBosAdminDelete)(lpwp->wpBosAdminDelete.hServer, pszAdminA, &status))
            status = 0;
         FreeString (pszAdminA, lpwp->wpBosAdminDelete.pszAdmin);
         break;
         }

      case wtaskBosAdminGetBegin:
         {
         if (!(*lpBosAdminGetBegin)(lpwp->wpBosAdminGetBegin.hServer, &lpwp->wpBosAdminGetBegin.hEnum, &status))
            lpwp->wpBosAdminGetBegin.hEnum = 0;
         else
            status = 0;
         break;
         }

      case wtaskBosAdminGetDone:
         {
         if ((*lpBosAdminGetDone)(lpwp->wpBosAdminGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskBosAdminGetNext:
         {
         char szAdminA[ 1024 ];
         if (!(*lpBosAdminGetNext)(lpwp->wpBosAdminGetNext.hEnum, szAdminA, &status))
            lpwp->wpBosAdminGetNext.pszAdmin[0] = TEXT('\0');
         else
            {
            status = 0;
            CopyAnsiToString (lpwp->wpBosAdminGetNext.pszAdmin, szAdminA);
            }
         break;
         }

      case wtaskBosKeyCreate:
         {
         kas_encryptionKey_t key;
         memcpy (key.key, lpwp->wpBosKeyCreate.key.key, ENCRYPTIONKEY_LEN);

         if ((*lpBosKeyCreate)(lpwp->wpBosKeyCreate.hServer, lpwp->wpBosKeyCreate.keyVersion, &key, &status))
            status = 0;
         break;
         }

      case wtaskBosKeyDelete:
         {
         if ((*lpBosKeyDelete)(lpwp->wpBosKeyDelete.hServer, lpwp->wpBosKeyDelete.keyVersion, &status))
            status = 0;
         break;
         }

      case wtaskBosKeyGetBegin:
         {
         if (!(*lpBosKeyGetBegin)(lpwp->wpBosKeyGetBegin.hServer, &lpwp->wpBosKeyGetBegin.hEnum, &status))
            lpwp->wpBosKeyGetBegin.hEnum = 0;
         else
            status = 0;
         break;
         }

      case wtaskBosKeyGetDone:
         {
         if ((*lpBosKeyGetDone)(lpwp->wpBosKeyGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskBosKeyGetNext:
         {
         bos_KeyInfo_t keyInfo;

         if ((*lpBosKeyGetNext)(lpwp->wpBosKeyGetNext.hEnum, &keyInfo, &status))
            {
            status = 0;
            lpwp->wpBosKeyGetNext.keyVersion = keyInfo.keyVersionNumber;
            memcpy (lpwp->wpBosKeyGetNext.keyData.key, keyInfo.key.key, ENCRYPTIONKEY_LEN);
            AfsClass_UnixTimeToSystemTime (&lpwp->wpBosKeyGetNext.keyInfo.timeLastModification, keyInfo.keyStatus.lastModificationDate);
            lpwp->wpBosKeyGetNext.keyInfo.timeLastModification.wMilliseconds = keyInfo.keyStatus.lastModificationMicroSeconds;
            lpwp->wpBosKeyGetNext.keyInfo.dwChecksum = (DWORD)keyInfo.keyStatus.checkSum;
            }
         break;
         }

      case wtaskBosCellSet:
         {
         LPSTR pszCellA = StringToAnsi (lpwp->wpBosCellSet.pszCell);
         if ((*lpBosCellSet)(lpwp->wpBosCellSet.hServer, pszCellA, &status))
            status = 0;
         FreeString (pszCellA, lpwp->wpBosCellSet.pszCell);
         break;
         }

      case wtaskBosCellGet:
         {
         char szCellA[ 256 ];
         if ((*lpBosCellGet)(lpwp->wpBosCellGet.hServer, szCellA, &status))
            {
            status = 0;
            CopyAnsiToString (lpwp->wpBosCellGet.pszCell, szCellA);
            }
         break;
         }

      case wtaskBosHostCreate:
         {
         LPSTR pszServerA = StringToAnsi (lpwp->wpBosHostCreate.pszServer);
         if ((*lpBosHostCreate)(lpwp->wpBosHostCreate.hServer, pszServerA, &status))
            status = 0;
         FreeString (pszServerA, lpwp->wpBosHostCreate.pszServer);
         break;
         }

      case wtaskBosHostDelete:
         {
         LPSTR pszServerA = StringToAnsi (lpwp->wpBosHostDelete.pszServer);
         if ((*lpBosHostDelete)(lpwp->wpBosHostDelete.hServer, pszServerA, &status))
            status = 0;
         FreeString (pszServerA, lpwp->wpBosHostDelete.pszServer);
         break;
         }

      case wtaskBosHostGetBegin:
         {
         if (!(*lpBosHostGetBegin)(lpwp->wpBosHostGetBegin.hServer, &lpwp->wpBosHostGetBegin.hEnum, &status))
            lpwp->wpBosHostGetBegin.hEnum = 0;
         else
            status = 0;
         break;
         }

      case wtaskBosHostGetDone:
         {
         if ((*lpBosHostGetDone)(lpwp->wpBosHostGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskBosHostGetNext:
         {
         char szServerA[ 256 ];
         if (!(*lpBosHostGetNext)(lpwp->wpBosHostGetNext.hEnum, szServerA, &status))
            lpwp->wpBosHostGetNext.pszServer[0] = TEXT('\0');
         else
            {
            status = 0;
            CopyAnsiToString (lpwp->wpBosHostGetNext.pszServer, szServerA);
            }
         break;
         }

      case wtaskBosExecutableCreate:
         {
         LPSTR pszLocalA = StringToAnsi (lpwp->wpBosExecutableCreate.pszLocal);
         LPSTR pszRemoteDirA = StringToAnsi (lpwp->wpBosExecutableCreate.pszRemoteDir);

         if ((*lpBosExecutableCreate)(lpwp->wpBosExecutableCreate.hServer, pszLocalA, pszRemoteDirA, &status))
            status = 0;

         FreeString (pszRemoteDirA, lpwp->wpBosExecutableCreate.pszRemoteDir);
         FreeString (pszLocalA, lpwp->wpBosExecutableCreate.pszLocal);
         break;
         }

      case wtaskBosExecutableRevert:
         {
         LPSTR pszFilenameA = StringToAnsi (lpwp->wpBosExecutableRevert.pszFilename);

         if ((*lpBosExecutableRevert)(lpwp->wpBosExecutableRevert.hServer, pszFilenameA, &status))
            status = 0;

         FreeString (pszFilenameA, lpwp->wpBosExecutableRevert.pszFilename);
         break;
         }

      case wtaskBosExecutableTimestampGet:
         {
         LPSTR pszFilenameA = StringToAnsi (lpwp->wpBosExecutableTimestampGet.pszFilename);

         unsigned long timeNew = 0;
         unsigned long timeOld = 0;
         unsigned long timeBak = 0;
         if ((*lpBosExecutableTimestampGet)(lpwp->wpBosExecutableTimestampGet.hServer, pszFilenameA, &timeNew, &timeOld, &timeBak, &status))
            {
            status = 0;
            AfsClass_UnixTimeToSystemTime (&lpwp->wpBosExecutableTimestampGet.timeNew, timeNew);
            AfsClass_UnixTimeToSystemTime (&lpwp->wpBosExecutableTimestampGet.timeOld, timeOld);
            AfsClass_UnixTimeToSystemTime (&lpwp->wpBosExecutableTimestampGet.timeBak, timeBak);
            }

         FreeString (pszFilenameA, lpwp->wpBosExecutableTimestampGet.pszFilename);
         break;
         }

      case wtaskBosExecutablePrune:
         {
         bos_Prune_t oldFiles = (lpwp->wpBosExecutablePrune.fPruneOld) ? BOS_PRUNE : BOS_DONT_PRUNE;
         bos_Prune_t bakFiles = (lpwp->wpBosExecutablePrune.fPruneBak) ? BOS_PRUNE : BOS_DONT_PRUNE;
         bos_Prune_t coreFiles = (lpwp->wpBosExecutablePrune.fPruneCore) ? BOS_PRUNE : BOS_DONT_PRUNE;

         if ((*lpBosExecutablePrune)(lpwp->wpBosExecutablePrune.hServer, oldFiles, bakFiles, coreFiles, &status))
            status = 0;
         break;
         }

      case wtaskBosExecutableRestartTimeSet:
         {
         bos_RestartTime_t time;
         AfsClass_SystemTimeToRestartTime (&time, lpwp->wpBosExecutableRestartTimeSet.fWeeklyRestart, &lpwp->wpBosExecutableRestartTimeSet.timeWeekly);
         if ((*lpBosExecutableRestartTimeSet)(lpwp->wpBosExecutableRestartTimeSet.hServer, BOS_RESTART_WEEKLY, time, &status))
            {
            AfsClass_SystemTimeToRestartTime (&time, lpwp->wpBosExecutableRestartTimeSet.fDailyRestart, &lpwp->wpBosExecutableRestartTimeSet.timeDaily);
            if ((*lpBosExecutableRestartTimeSet)(lpwp->wpBosExecutableRestartTimeSet.hServer, BOS_RESTART_DAILY, time, &status))
               status = 0;
            }
         break;
         }

      case wtaskBosExecutableRestartTimeGet:
         {
         lpwp->wpBosExecutableRestartTimeGet.fDailyRestart = FALSE;
         lpwp->wpBosExecutableRestartTimeGet.fWeeklyRestart = FALSE;

         bos_RestartTime_t time;
         if ((*lpBosExecutableRestartTimeGet)(lpwp->wpBosExecutableRestartTimeGet.hServer, BOS_RESTART_WEEKLY, &time, &status))
            {
            AfsClass_RestartTimeToSystemTime (&lpwp->wpBosExecutableRestartTimeGet.fWeeklyRestart, &lpwp->wpBosExecutableRestartTimeGet.timeWeekly, &time);

            if ((*lpBosExecutableRestartTimeGet)(lpwp->wpBosExecutableRestartTimeGet.hServer, BOS_RESTART_DAILY, &time, &status))
               {
               AfsClass_RestartTimeToSystemTime (&lpwp->wpBosExecutableRestartTimeGet.fDailyRestart, &lpwp->wpBosExecutableRestartTimeGet.timeDaily, &time);
               status = 0;
               }
            }
         break;
         }

      case wtaskBosLogGet:
         {
         LPSTR pszLogNameA = StringToAnsi (lpwp->wpBosLogGet.pszLogName);

         LPSTR pszLogDataA = NULL;
         unsigned long cchAllocated = 0;
         unsigned long cchRequired = 1024;

         do {
            if (cchAllocated < cchRequired)
               {
               if (pszLogDataA)
                  {
                  FreeString (pszLogDataA);
                  pszLogDataA = NULL;
                  }
               if ((pszLogDataA = AllocateAnsi (cchRequired)) == 0)
                  break;
               cchAllocated = cchRequired;
               }

            cchRequired = cchAllocated;
            if ((*lpBosLogGet)(lpwp->wpBosLogGet.hServer, pszLogNameA, &cchRequired, pszLogDataA, &status))
               {
               lpwp->wpBosLogGet.pszLogData = AllocateString (cchRequired+1);
               CopyAnsiToString (lpwp->wpBosLogGet.pszLogData, pszLogDataA, cchRequired);
               lpwp->wpBosLogGet.pszLogData[ cchRequired ] = TEXT('\0');
               status = 0;
               break;
               }
            } while (cchRequired > cchAllocated);

         if (pszLogDataA)
            FreeString (pszLogDataA);
         FreeString (pszLogNameA, lpwp->wpBosLogGet.pszLogName);
         break;
         }

      case wtaskBosAuthSet:
         {
         bos_Auth_t auth = (lpwp->wpBosAuthSet.fEnableAuth) ? BOS_AUTH_REQUIRED : BOS_NO_AUTH;
         if ((*lpBosAuthSet)(lpwp->wpBosAuthSet.hServer, auth, &status))
            status = 0;
         break;
         }

      case wtaskBosCommandExecute:
         {
         LPSTR pszCommandA = StringToAnsi (lpwp->wpBosCommandExecute.pszCommand);
         if ((*lpBosCommandExecute)(lpwp->wpBosCommandExecute.hServer, pszCommandA, &status))
            status = 0;
         FreeString (pszCommandA, lpwp->wpBosCommandExecute.pszCommand);
         break;
         }

      case wtaskBosSalvage:
         {
         LPSTR pszAggregateA = StringToAnsi (lpwp->wpBosSalvage.pszAggregate);
         LPSTR pszFilesetA = StringToAnsi (lpwp->wpBosSalvage.pszFileset);
         LPSTR pszTempDirA = StringToAnsi (lpwp->wpBosSalvage.pszTempDir);
         LPSTR pszLogFileA = StringToAnsi (lpwp->wpBosSalvage.pszLogFile);

         vos_force_t force = (lpwp->wpBosSalvage.fForce) ? VOS_FORCE : VOS_NORMAL;
         bos_SalvageDamagedVolumes_t salvageDamagedVolumes = (lpwp->wpBosSalvage.fReadonly) ? BOS_DONT_SALVAGE_DAMAGED_VOLUMES : BOS_SALVAGE_DAMAGED_VOLUMES;
         bos_WriteInodes_t writeInodes = (lpwp->wpBosSalvage.fLogInodes) ? BOS_SALVAGE_WRITE_INODES : BOS_SALVAGE_DONT_WRITE_INODES;
         bos_WriteRootInodes_t writeRootInodes = (lpwp->wpBosSalvage.fLogRootInodes) ? BOS_SALVAGE_WRITE_ROOT_INODES : BOS_SALVAGE_DONT_WRITE_ROOT_INODES;
         bos_ForceDirectory_t forceDirectory = (lpwp->wpBosSalvage.fRebuildDirs) ? BOS_SALVAGE_FORCE_DIRECTORIES : BOS_SALVAGE_DONT_FORCE_DIRECTORIES;
         bos_ForceBlockRead_t forceBlockRead = (lpwp->wpBosSalvage.fReadBlocks) ? BOS_SALVAGE_FORCE_BLOCK_READS : BOS_SALVAGE_DONT_FORCE_BLOCK_READS;

         if ((*lpBosSalvage)(lpwp->wpBosSalvage.hCell, lpwp->wpBosSalvage.hServer, pszAggregateA, pszFilesetA, max(lpwp->wpBosSalvage.nProcesses,1), pszTempDirA, pszLogFileA, force, salvageDamagedVolumes, writeInodes, writeRootInodes, forceDirectory, forceBlockRead, &status))
             status = 0;

         FreeString (pszLogFileA, lpwp->wpBosSalvage.pszLogFile);
         FreeString (pszTempDirA, lpwp->wpBosSalvage.pszTempDir);
         FreeString (pszFilesetA, lpwp->wpBosSalvage.pszFileset);
         FreeString (pszAggregateA, lpwp->wpBosSalvage.pszAggregate);
         break;
         }

      case wtaskKasServerOpen:
         {
         if ((*lpKasServerOpen)(lpwp->wpKasServerOpen.hCell, (const char **)lpwp->wpKasServerOpen.apszServers, &lpwp->wpKasServerOpen.hServer, &status))
            status = 0;
         break;
         }

      case wtaskKasServerClose:
         {
         if ((*lpKasServerClose)(lpwp->wpKasServerClose.hServer, &status))
            status = 0;
         break;
         }

      case wtaskKasServerRandomKeyGet:
         {
         kas_encryptionKey_t key;
         if ((*lpKasServerRandomKeyGet)(lpwp->wpKasServerRandomKeyGet.hCell, lpwp->wpKasServerRandomKeyGet.hServer, &key, &status))
            {
            status = 0;
            memcpy (&lpwp->wpKasServerRandomKeyGet.key.key, &key.key, ENCRYPTIONKEY_LEN);
            }
         break;
         }

      case wtaskKasStringToKey:
         {
         LPSTR pszCellA = StringToAnsi (lpwp->wpKasStringToKey.pszCell);
         LPSTR pszStringA = StringToAnsi (lpwp->wpKasStringToKey.pszString);

         kas_encryptionKey_t key;
         if ((*lpKasStringToKey)(pszCellA, pszStringA, &key, &status))
            {
            status = 0;
            memcpy (&lpwp->wpKasStringToKey.key.key, &key.key, ENCRYPTIONKEY_LEN);
            }

         FreeString (pszStringA, lpwp->wpKasStringToKey.pszString);
         FreeString (pszCellA, lpwp->wpKasStringToKey.pszCell);
         break;
         }

      case wtaskKasPrincipalCreate:
         {
         kas_identity_t Identity;
         CopyStringToAnsi (Identity.principal, lpwp->wpKasPrincipalCreate.pszPrincipal);
         CopyStringToAnsi (Identity.instance, lpwp->wpKasPrincipalCreate.pszInstance);

         LPSTR pszPasswordA = StringToAnsi (lpwp->wpKasPrincipalCreate.pszPassword);

         if ((*lpKasPrincipalCreate)(lpwp->wpKasPrincipalCreate.hCell, lpwp->wpKasPrincipalCreate.hServer, &Identity, pszPasswordA, &status))
            status = 0;

         FreeString (pszPasswordA, lpwp->wpKasPrincipalCreate.pszPassword);
         break;
         }

      case wtaskKasPrincipalDelete:
         {
         kas_identity_t Identity;
         CopyStringToAnsi (Identity.principal, lpwp->wpKasPrincipalDelete.pszPrincipal);
         CopyStringToAnsi (Identity.instance, lpwp->wpKasPrincipalDelete.pszInstance);

         if ((*lpKasPrincipalDelete)(lpwp->wpKasPrincipalDelete.hCell, lpwp->wpKasPrincipalDelete.hServer, &Identity, &status))
            status = 0;
         break;
         }

      case wtaskKasPrincipalGet:
         {
         kas_identity_t Identity;
         CopyStringToAnsi (Identity.principal, lpwp->wpKasPrincipalGet.pszPrincipal);
         CopyStringToAnsi (Identity.instance, lpwp->wpKasPrincipalGet.pszInstance);

         if ((*lpKasPrincipalGet)(lpwp->wpKasPrincipalGet.hCell, lpwp->wpKasPrincipalGet.hServer, &Identity, &lpwp->wpKasPrincipalGet.Data, &status))
            status = 0;
         break;
         }

      case wtaskKasPrincipalGetBegin:
         {
         if ((*lpKasPrincipalGetBegin)(lpwp->wpKasPrincipalGetBegin.hCell, lpwp->wpKasPrincipalGetBegin.hServer, &lpwp->wpKasPrincipalGetBegin.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskKasPrincipalGetNext:
         {
         kas_identity_t Identity;
         if ((*lpKasPrincipalGetNext)(lpwp->wpKasPrincipalGetNext.hEnum, &Identity, &status))
            {
            CopyAnsiToString (lpwp->wpKasPrincipalGetNext.pszPrincipal, Identity.principal);
            CopyAnsiToString (lpwp->wpKasPrincipalGetNext.pszInstance, Identity.instance);
            status = 0;
            }
         break;
         }

      case wtaskKasPrincipalGetDone:
         {
         if ((*lpKasPrincipalGetDone)(lpwp->wpKasPrincipalGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskKasPrincipalKeySet:
         {
         kas_identity_t Identity;
         CopyStringToAnsi (Identity.principal, lpwp->wpKasPrincipalKeySet.pszPrincipal);
         CopyStringToAnsi (Identity.instance, lpwp->wpKasPrincipalKeySet.pszInstance);

         kas_encryptionKey_t Key;
         memcpy (&Key.key, &lpwp->wpKasPrincipalKeySet.key, ENCRYPTIONKEY_LEN);

         if ((*lpKasPrincipalKeySet)(lpwp->wpKasPrincipalKeySet.hCell, lpwp->wpKasPrincipalKeySet.hServer, &Identity, lpwp->wpKasPrincipalKeySet.keyVersion, &Key, &status))
            status = 0;
         break;
         }

      case wtaskKasPrincipalLockStatusGet:
         {
         kas_identity_t Identity;
         CopyStringToAnsi (Identity.principal, lpwp->wpKasPrincipalLockStatusGet.pszPrincipal);
         CopyStringToAnsi (Identity.instance, lpwp->wpKasPrincipalLockStatusGet.pszInstance);

         unsigned long date;
         if ((*lpKasPrincipalLockStatusGet)(lpwp->wpKasPrincipalLockStatusGet.hCell, lpwp->wpKasPrincipalLockStatusGet.hServer, &Identity, &date, &status))
            {
            AfsClass_UnixTimeToSystemTime (&lpwp->wpKasPrincipalLockStatusGet.timeUnlocked, date);
            status = 0;
            }
         break;
         }

      case wtaskKasPrincipalUnlock:
         {
         kas_identity_t Identity;
         CopyStringToAnsi (Identity.principal, lpwp->wpKasPrincipalUnlock.pszPrincipal);
         CopyStringToAnsi (Identity.instance, lpwp->wpKasPrincipalUnlock.pszInstance);

         if ((*lpKasPrincipalUnlock)(lpwp->wpKasPrincipalUnlock.hCell, lpwp->wpKasPrincipalUnlock.hServer, &Identity, &status))
            status = 0;
         break;
         }

      case wtaskKasPrincipalFieldsSet:
         {
         kas_identity_t Identity;
         CopyStringToAnsi (Identity.principal, lpwp->wpKasPrincipalFieldsSet.pszPrincipal);
         CopyStringToAnsi (Identity.instance, lpwp->wpKasPrincipalFieldsSet.pszInstance);

         ULONG timeExpires = AfsClass_SystemTimeToUnixTime (&lpwp->wpKasPrincipalFieldsSet.timeExpires);
         if (!timeExpires) // account never expires?
            timeExpires = (ULONG)-1;

         kas_admin_t isAdmin = (lpwp->wpKasPrincipalFieldsSet.fIsAdmin) ? KAS_ADMIN : NO_KAS_ADMIN;
         kas_tgs_t grantTickets = (lpwp->wpKasPrincipalFieldsSet.fGrantTickets) ? TGS : NO_TGS;
         kas_enc_t canEncrypt = (lpwp->wpKasPrincipalFieldsSet.fCanEncrypt) ? ENCRYPT : NO_ENCRYPT;
         kas_cpw_t canChangePassword = (lpwp->wpKasPrincipalFieldsSet.fCanChangePassword) ? CHANGE_PASSWORD : NO_CHANGE_PASSWORD;
         kas_rpw_t passwordReuse = (lpwp->wpKasPrincipalFieldsSet.fCanReusePasswords) ? REUSE_PASSWORD : NO_REUSE_PASSWORD;

         if ((*lpKasPrincipalFieldsSet)(lpwp->wpKasPrincipalFieldsSet.hCell, lpwp->wpKasPrincipalFieldsSet.hServer, &Identity,
                &isAdmin,
                &grantTickets,
                &canEncrypt,
                &canChangePassword,
                (unsigned int *)&timeExpires,
                (unsigned int *)&lpwp->wpKasPrincipalFieldsSet.csecTicketLifetime,
                (unsigned int *)&lpwp->wpKasPrincipalFieldsSet.cdayPwExpires,
                &passwordReuse,
                (unsigned int *)&lpwp->wpKasPrincipalFieldsSet.nFailureAttempts,
                (unsigned int *)&lpwp->wpKasPrincipalFieldsSet.csecFailedLoginLockTime,
                &status))
            {
            status = 0;
            }
         break;
         }

      case wtaskPtsGroupMemberAdd:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupMemberAdd.pszGroup);
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsGroupMemberAdd.pszUser);

         if ((*lpPtsGroupMemberAdd)(lpwp->wpPtsGroupMemberAdd.hCell, pszUserA, pszGroupA, &status))
            status = 0;

         FreeString (pszGroupA, lpwp->wpPtsGroupMemberAdd.pszGroup);
         FreeString (pszUserA, lpwp->wpPtsGroupMemberAdd.pszUser);
         break;
         }

      case wtaskPtsGroupOwnerChange:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupOwnerChange.pszGroup);
         LPSTR pszOwnerA = StringToAnsi (lpwp->wpPtsGroupOwnerChange.pszOwner);

         if ((*lpPtsGroupOwnerChange)(lpwp->wpPtsGroupOwnerChange.hCell, pszGroupA, pszOwnerA, &status))
            status = 0;

         FreeString (pszOwnerA, lpwp->wpPtsGroupOwnerChange.pszOwner);
         FreeString (pszGroupA, lpwp->wpPtsGroupOwnerChange.pszGroup);
         break;
         }

      case wtaskPtsGroupCreate:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupCreate.pszGroup);
         LPSTR pszOwnerA = StringToAnsi (lpwp->wpPtsGroupCreate.pszOwner);

         if ((*lpPtsGroupCreate)(lpwp->wpPtsGroupCreate.hCell, pszGroupA, pszOwnerA, &lpwp->wpPtsGroupCreate.idGroup, &status))
            status = 0;

         FreeString (pszOwnerA, lpwp->wpPtsGroupCreate.pszOwner);
         FreeString (pszGroupA, lpwp->wpPtsGroupCreate.pszGroup);
         break;
         }

      case wtaskPtsGroupGet:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupGet.pszGroup);

         if ((*lpPtsGroupGet)(lpwp->wpPtsGroupGet.hCell, pszGroupA, &lpwp->wpPtsGroupGet.Entry, &status))
            status = 0;

         FreeString (pszGroupA, lpwp->wpPtsGroupGet.pszGroup);
         break;
         }

      case wtaskPtsGroupDelete:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupDelete.pszGroup);

         if ((*lpPtsGroupDelete)(lpwp->wpPtsGroupDelete.hCell, pszGroupA, &status))
            status = 0;

         FreeString (pszGroupA, lpwp->wpPtsGroupDelete.pszGroup);
         break;
         }

      case wtaskPtsGroupMaxGet:
         {
         if ((*lpPtsGroupMaxGet)(lpwp->wpPtsGroupMaxGet.hCell, &lpwp->wpPtsGroupMaxGet.idGroupMax, &status))
            status = 0;
         break;
         }

      case wtaskPtsGroupMaxSet:
         {
         if ((*lpPtsGroupMaxSet)(lpwp->wpPtsGroupMaxSet.hCell, lpwp->wpPtsGroupMaxSet.idGroupMax, &status))
            status = 0;
         break;
         }

      case wtaskPtsGroupMemberListBegin:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupMemberListBegin.pszGroup);

         if ((*lpPtsGroupMemberListBegin)(lpwp->wpPtsGroupMemberListBegin.hCell, pszGroupA, &lpwp->wpPtsGroupMemberListBegin.hEnum, &status))
            status = 0;

         FreeString (pszGroupA, lpwp->wpPtsGroupMemberListBegin.pszGroup);
         break;
         }

      case wtaskPtsGroupMemberListNext:
         {
         char szMemberA[ cchNAME ];
         if ((*lpPtsGroupMemberListNext)(lpwp->wpPtsGroupMemberListNext.hEnum, szMemberA, &status))
            {
            CopyAnsiToString (lpwp->wpPtsGroupMemberListNext.pszMember, szMemberA);
            status = 0;
            }
         break;
         }

      case wtaskPtsGroupMemberListDone:
         {
         if ((*lpPtsGroupMemberListDone)(lpwp->wpPtsGroupMemberListDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskPtsGroupMemberRemove:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupMemberRemove.pszGroup);
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsGroupMemberRemove.pszUser);

         if ((*lpPtsGroupMemberRemove)(lpwp->wpPtsGroupMemberRemove.hCell, pszUserA, pszGroupA, &status))
            status = 0;

         FreeString (pszUserA, lpwp->wpPtsGroupMemberRemove.pszUser);
         FreeString (pszGroupA, lpwp->wpPtsGroupMemberRemove.pszGroup);
         break;
         }

      case wtaskPtsGroupRename:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupRename.pszGroup);
         LPSTR pszNewNameA = StringToAnsi (lpwp->wpPtsGroupRename.pszNewName);

         if ((*lpPtsGroupRename)(lpwp->wpPtsGroupRename.hCell, pszGroupA, pszNewNameA, &status))
            status = 0;

         FreeString (pszNewNameA, lpwp->wpPtsGroupRename.pszNewName);
         FreeString (pszGroupA, lpwp->wpPtsGroupRename.pszGroup);
         break;
         }

      case wtaskPtsGroupModify:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsGroupModify.pszGroup);

         if ((*lpPtsGroupModify)(lpwp->wpPtsGroupModify.hCell, pszGroupA, &lpwp->wpPtsGroupModify.Delta, &status))
            status = 0;

         FreeString (pszGroupA, lpwp->wpPtsGroupModify.pszGroup);
         break;
         }

      case wtaskPtsUserCreate:
         {
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsUserCreate.pszUser);

         if ((*lpPtsUserCreate)(lpwp->wpPtsUserCreate.hCell, pszUserA, &lpwp->wpPtsUserCreate.idUser, &status))
            status = 0;

         FreeString (pszUserA, lpwp->wpPtsUserCreate.pszUser);
         break;
         }

      case wtaskPtsUserDelete:
         {
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsUserDelete.pszUser);

         if ((*lpPtsUserDelete)(lpwp->wpPtsUserDelete.hCell, pszUserA, &status))
            status = 0;

         FreeString (pszUserA, lpwp->wpPtsUserDelete.pszUser);
         break;
         }

      case wtaskPtsUserGet:
         {
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsUserGet.pszUser);

         if ((*lpPtsUserGet)(lpwp->wpPtsUserGet.hCell, pszUserA, &lpwp->wpPtsUserGet.Entry, &status))
            status = 0;

         FreeString (pszUserA, lpwp->wpPtsUserGet.pszUser);
         break;
         }

      case wtaskPtsUserRename:
         {
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsUserRename.pszUser);
         LPSTR pszNewNameA = StringToAnsi (lpwp->wpPtsUserRename.pszNewName);

         if ((*lpPtsUserRename)(lpwp->wpPtsUserRename.hCell, pszUserA, pszNewNameA, &status))
            status = 0;

         FreeString (pszNewNameA, lpwp->wpPtsUserRename.pszNewName);
         FreeString (pszUserA, lpwp->wpPtsUserRename.pszUser);
         break;
         }

      case wtaskPtsUserModify:
         {
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsUserModify.pszUser);

         if ((*lpPtsUserModify)(lpwp->wpPtsUserModify.hCell, pszUserA, &lpwp->wpPtsUserModify.Delta, &status))
            status = 0;

         FreeString (pszUserA, lpwp->wpPtsUserModify.pszUser);
         break;
         }

      case wtaskPtsUserMaxGet:
         {
         if ((*lpPtsUserMaxGet)(lpwp->wpPtsUserMaxGet.hCell, &lpwp->wpPtsUserMaxGet.idUserMax, &status))
            status = 0;
         break;
         }

      case wtaskPtsUserMaxSet:
         {
         if ((*lpPtsUserMaxSet)(lpwp->wpPtsUserMaxSet.hCell, lpwp->wpPtsUserMaxSet.idUserMax, &status))
            status = 0;
         break;
         }

      case wtaskPtsUserMemberListBegin:
         {
         LPSTR pszUserA = StringToAnsi (lpwp->wpPtsUserMemberListBegin.pszUser);

         if ((*lpPtsUserMemberListBegin)(lpwp->wpPtsUserMemberListBegin.hCell, pszUserA, &lpwp->wpPtsUserMemberListBegin.hEnum, &status))
            status = 0;

         FreeString (pszUserA, lpwp->wpPtsUserMemberListBegin.pszUser);
         break;
         }

      case wtaskPtsUserMemberListNext:
         {
         char szGroupA[ cchNAME ];
         if ((*lpPtsUserMemberListNext)(lpwp->wpPtsUserMemberListNext.hEnum, szGroupA, &status))
            {
            CopyAnsiToString (lpwp->wpPtsUserMemberListNext.pszGroup, szGroupA);
            status = 0;
            }
         break;
         }

      case wtaskPtsUserMemberListDone:
         {
         if ((*lpPtsUserMemberListDone)(lpwp->wpPtsUserMemberListDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskPtsOwnedGroupListBegin:
         {
         LPSTR pszOwnerA = StringToAnsi (lpwp->wpPtsOwnedGroupListBegin.pszOwner);

         if ((*lpPtsOwnedGroupListBegin)(lpwp->wpPtsOwnedGroupListBegin.hCell, pszOwnerA, &lpwp->wpPtsOwnedGroupListBegin.hEnum, &status))
            status = 0;

         FreeString (pszOwnerA, lpwp->wpPtsOwnedGroupListBegin.pszOwner);
         break;
         }

      case wtaskPtsOwnedGroupListNext:
         {
         LPSTR pszGroupA = StringToAnsi (lpwp->wpPtsOwnedGroupListNext.pszGroup);

         if ((*lpPtsOwnedGroupListNext)(lpwp->wpPtsOwnedGroupListNext.hEnum, pszGroupA, &status))
            status = 0;

         FreeString (pszGroupA, lpwp->wpPtsOwnedGroupListNext.pszGroup);
         break;
         }

      case wtaskPtsOwnedGroupListDone:
         {
         if ((*lpPtsOwnedGroupListDone)(lpwp->wpPtsOwnedGroupListDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskClientTokenGetExisting:
         {
         LPTSTR pszCellA = StringToAnsi (lpwp->wpClientTokenGetExisting.pszCell);

         if ((*lpClientTokenGetExisting)(pszCellA, &lpwp->wpClientTokenGetExisting.hCreds, &status))
            status = 0;

         FreeString (pszCellA, lpwp->wpClientTokenGetExisting.pszCell);
         break;
         }

      case wtaskClientCellOpen:
         {
         LPTSTR pszCellA = StringToAnsi (lpwp->wpClientCellOpen.pszCell);

         if ((*lpClientCellOpen)(pszCellA, lpwp->wpClientCellOpen.hCreds, &lpwp->wpClientCellOpen.hCell, &status))
            status = 0;

         FreeString (pszCellA, lpwp->wpClientCellOpen.pszCell);
         break;
         }

      case wtaskClientCellClose:
         {
         if ((*lpClientCellClose)(lpwp->wpClientCellClose.hCell, &status))
            status = 0;
         break;
         }

      case wtaskClientLocalCellGet:
         {
         char szCellA[ cchNAME ];
         if ((*lpClientLocalCellGet)(szCellA, &status))
            {
            status = 0;
            CopyAnsiToString (lpwp->wpClientLocalCellGet.pszCell, szCellA);
            }
         break;
         }

      case wtaskClientAFSServerGet:
         {
         LPTSTR pszServerA = StringToAnsi (lpwp->wpClientAFSServerGet.pszServer);

         if ((*lpClientAFSServerGet)(lpwp->wpClientAFSServerGet.hCell, pszServerA, &lpwp->wpClientAFSServerGet.Entry, &status))
            status = 0;

         FreeString (pszServerA, lpwp->wpClientAFSServerGet.pszServer);
         break;
         }

      case wtaskClientAFSServerGetBegin:
         {
         if ((*lpClientAFSServerGetBegin)(lpwp->wpClientAFSServerGetBegin.hCell, &lpwp->wpClientAFSServerGetBegin.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskClientAFSServerGetNext:
         {
         if ((*lpClientAFSServerGetNext)(lpwp->wpClientAFSServerGetNext.hEnum, &lpwp->wpClientAFSServerGetNext.Entry, &status))
            status = 0;
         break;
         }

      case wtaskClientAFSServerGetDone:
         {
         if ((*lpClientAFSServerGetDone)(lpwp->wpClientAFSServerGetDone.hEnum, &status))
            status = 0;
         break;
         }

      case wtaskUtilDatabaseServerGetBegin:
         {
         LPSTR pszCellA = StringToAnsi (lpwp->wpUtilDatabaseServerGetBegin.pszCell);

         if ((*lpUtilDatabaseServerGetBegin)(pszCellA, &lpwp->wpUtilDatabaseServerGetBegin.hEnum, &status))
            status = 0;

         FreeString (pszCellA, lpwp->wpUtilDatabaseServerGetBegin.pszCell);
         break;
         }

      case wtaskUtilDatabaseServerGetNext:
         {
         int Address;
         if ((*lpUtilDatabaseServerGetNext)(lpwp->wpUtilDatabaseServerGetNext.hEnum, &Address, &status))
            {
            AfsClass_IntToAddress (&lpwp->wpUtilDatabaseServerGetNext.Address, Address);
            status = 0;
            }
         break;
         }

      case wtaskUtilDatabaseServerGetDone:
         {
         if ((*lpUtilDatabaseServerGetDone)(lpwp->wpUtilDatabaseServerGetDone.hEnum, &status))
            status = 0;
         break;
         }

      // ADD HERE
      }

   return (DWORD)status;
}


BOOL Worker_LoadLibraries (ULONG *pStatus)
{
   if ((hiVOS = LoadLibrary (cszAFSVOSDLL)) == NULL)
      {
      if (pStatus)
         *pStatus = GetLastError();
      return FALSE;
      }
   if ((hiBOS = LoadLibrary (cszAFSBOSDLL)) == NULL)
      {
      if (pStatus)
         *pStatus = GetLastError();
      return FALSE;
      }
   if ((hiKAS = LoadLibrary (cszAFSKASDLL)) == NULL)
      {
      if (pStatus)
         *pStatus = GetLastError();
      return FALSE;
      }
   if ((hiPTS = LoadLibrary (cszAFSPTSDLL)) == NULL)
      {
      if (pStatus)
         *pStatus = GetLastError();
      return FALSE;
      }
   if ((hiUtil = LoadLibrary (cszAFSUTILDLL)) == NULL)
      {
      if (pStatus)
         *pStatus = GetLastError();
      return FALSE;
      }
   if ((hiClient = LoadLibrary (cszAFSCLIENTDLL)) == NULL)
      {
      if (pStatus)
         *pStatus = GetLastError();
      return FALSE;
      }

   lpVosBackupVolumeCreate = (lpVosBackupVolumeCreate_t)GetProcAddress (hiVOS, "vos_BackupVolumeCreate");
   lpVosBackupVolumeCreateMultiple = (lpVosBackupVolumeCreateMultiple_t)GetProcAddress (hiVOS, "vos_BackupVolumeCreateMultiple");
   lpVosPartitionGet = (lpVosPartitionGet_t)GetProcAddress (hiVOS, "vos_PartitionGet");
   lpVosPartitionGetBegin = (lpVosPartitionGetBegin_t)GetProcAddress (hiVOS, "vos_PartitionGetBegin");
   lpVosPartitionGetNext = (lpVosPartitionGetNext_t)GetProcAddress (hiVOS, "vos_PartitionGetNext");
   lpVosPartitionGetDone = (lpVosPartitionGetDone_t)GetProcAddress (hiVOS, "vos_PartitionGetDone");
   lpVosServerOpen = (lpVosServerOpen_t)GetProcAddress (hiVOS, "vos_ServerOpen");
   lpVosServerClose = (lpVosServerClose_t)GetProcAddress (hiVOS, "vos_ServerClose");
   lpVosServerSync = (lpVosServerSync_t)GetProcAddress (hiVOS, "vos_ServerSync");
   lpVosFileServerAddressChange = (lpVosFileServerAddressChange_t)GetProcAddress (hiVOS, "vos_FileServerAddressChange");
   lpVosFileServerAddressRemove = (lpVosFileServerAddressRemove_t)GetProcAddress (hiVOS, "vos_FileServerAddressRemove");
   lpVosFileServerGetBegin = (lpVosFileServerGetBegin_t)GetProcAddress (hiVOS, "vos_FileServerGetBegin");
   lpVosFileServerGetNext = (lpVosFileServerGetNext_t)GetProcAddress (hiVOS, "vos_FileServerGetNext");
   lpVosFileServerGetDone = (lpVosFileServerGetDone_t)GetProcAddress (hiVOS, "vos_FileServerGetDone");
   lpVosServerTransactionStatusGetBegin = (lpVosServerTransactionStatusGetBegin_t)GetProcAddress (hiVOS, "vos_ServerTransactionStatusGetBegin");
   lpVosServerTransactionStatusGetNext = (lpVosServerTransactionStatusGetNext_t)GetProcAddress (hiVOS, "vos_ServerTransactionStatusGetNext");
   lpVosServerTransactionStatusGetDone = (lpVosServerTransactionStatusGetDone_t)GetProcAddress (hiVOS, "vos_ServerTransactionStatusGetDone");
   lpVosVLDBGet = (lpVosVLDBGet_t)GetProcAddress (hiVOS, "vos_VLDBGet");
   lpVosVLDBGetBegin = (lpVosVLDBGetBegin_t)GetProcAddress (hiVOS, "vos_VLDBGetBegin");
   lpVosVLDBGetNext = (lpVosVLDBGetNext_t)GetProcAddress (hiVOS, "vos_VLDBGetNext");
   lpVosVLDBGetDone = (lpVosVLDBGetDone_t)GetProcAddress (hiVOS, "vos_VLDBGetDone");
   lpVosVLDBEntryRemove = (lpVosVLDBEntryRemove_t)GetProcAddress (hiVOS, "vos_VLDBEntryRemove");
   lpVosVLDBUnlock = (lpVosVLDBUnlock_t)GetProcAddress (hiVOS, "vos_VLDBUnlock");
   lpVosVLDBEntryLock = (lpVosVLDBEntryLock_t)GetProcAddress (hiVOS, "vos_VLDBEntryLock");
   lpVosVLDBEntryUnlock = (lpVosVLDBEntryUnlock_t)GetProcAddress (hiVOS, "vos_VLDBEntryUnlock");
   lpVosVLDBReadOnlySiteCreate = (lpVosVLDBReadOnlySiteCreate_t)GetProcAddress (hiVOS, "vos_VLDBReadOnlySiteCreate");
   lpVosVLDBReadOnlySiteDelete = (lpVosVLDBReadOnlySiteDelete_t)GetProcAddress (hiVOS, "vos_VLDBReadOnlySiteDelete");
   lpVosVLDBSync = (lpVosVLDBSync_t)GetProcAddress (hiVOS, "vos_VLDBSync");
   lpVosVolumeCreate = (lpVosVolumeCreate_t)GetProcAddress (hiVOS, "vos_VolumeCreate");
   lpVosVolumeDelete = (lpVosVolumeDelete_t)GetProcAddress (hiVOS, "vos_VolumeDelete");
   lpVosVolumeRename = (lpVosVolumeRename_t)GetProcAddress (hiVOS, "vos_VolumeRename");
   lpVosVolumeDump = (lpVosVolumeDump_t)GetProcAddress (hiVOS, "vos_VolumeDump");
   lpVosVolumeRestore = (lpVosVolumeRestore_t)GetProcAddress (hiVOS, "vos_VolumeRestore");
   lpVosVolumeOnline = (lpVosVolumeOnline_t)GetProcAddress (hiVOS, "vos_VolumeOnline");
   lpVosVolumeOffline = (lpVosVolumeOffline_t)GetProcAddress (hiVOS, "vos_VolumeOffline");
   lpVosVolumeGet = (lpVosVolumeGet_t)GetProcAddress (hiVOS, "vos_VolumeGet");
   lpVosVolumeGetBegin = (lpVosVolumeGetBegin_t)GetProcAddress (hiVOS, "vos_VolumeGetBegin");
   lpVosVolumeGetNext = (lpVosVolumeGetNext_t)GetProcAddress (hiVOS, "vos_VolumeGetNext");
   lpVosVolumeGetDone = (lpVosVolumeGetDone_t)GetProcAddress (hiVOS, "vos_VolumeGetDone");
   lpVosVolumeMove = (lpVosVolumeMove_t)GetProcAddress (hiVOS, "vos_VolumeMove");
   lpVosVolumeRelease = (lpVosVolumeRelease_t)GetProcAddress (hiVOS, "vos_VolumeRelease");
   lpVosVolumeZap = (lpVosVolumeZap_t)GetProcAddress (hiVOS, "vos_VolumeZap");
   lpVosPartitionNameToId = (lpVosPartitionNameToId_t)GetProcAddress (hiVOS, "vos_PartitionNameToId");
   lpVosPartitionIdToName = (lpVosPartitionIdToName_t)GetProcAddress (hiVOS, "vos_PartitionIdToName");
   lpVosVolumeQuotaChange = (lpVosVolumeQuotaChange_t)GetProcAddress (hiVOS, "vos_VolumeQuotaChange");

   lpBosServerOpen = (lpBosServerOpen_t)GetProcAddress (hiBOS, "bos_ServerOpen");
   lpBosServerClose = (lpBosServerClose_t)GetProcAddress (hiBOS, "bos_ServerClose");
   lpBosProcessCreate = (lpBosProcessCreate_t)GetProcAddress (hiBOS, "bos_ProcessCreate");
   lpBosProcessDelete = (lpBosProcessDelete_t)GetProcAddress (hiBOS, "bos_ProcessDelete");
   lpBosProcessExecutionStateGet = (lpBosProcessExecutionStateGet_t)GetProcAddress (hiBOS, "bos_ProcessExecutionStateGet");
   lpBosProcessExecutionStateSet = (lpBosProcessExecutionStateSet_t)GetProcAddress (hiBOS, "bos_ProcessExecutionStateSet");
   lpBosProcessExecutionStateSetTemporary = (lpBosProcessExecutionStateSetTemporary_t)GetProcAddress (hiBOS, "bos_ProcessExecutionStateSetTemporary");
   lpBosProcessNameGetBegin = (lpBosProcessNameGetBegin_t)GetProcAddress (hiBOS, "bos_ProcessNameGetBegin");
   lpBosProcessNameGetDone = (lpBosProcessNameGetDone_t)GetProcAddress (hiBOS, "bos_ProcessNameGetDone");
   lpBosProcessNameGetNext = (lpBosProcessNameGetNext_t)GetProcAddress (hiBOS, "bos_ProcessNameGetNext");
   lpBosProcessInfoGet = (lpBosProcessInfoGet_t)GetProcAddress (hiBOS, "bos_ProcessInfoGet");
   lpBosProcessParameterGetBegin = (lpBosProcessParameterGetBegin_t)GetProcAddress (hiBOS, "bos_ProcessParameterGetBegin");
   lpBosProcessParameterGetDone = (lpBosProcessParameterGetDone_t)GetProcAddress (hiBOS, "bos_ProcessParameterGetDone");
   lpBosProcessParameterGetNext = (lpBosProcessParameterGetNext_t)GetProcAddress (hiBOS, "bos_ProcessParameterGetNext");
   lpBosProcessNotifierGet = (lpBosProcessNotifierGet_t)GetProcAddress (hiBOS, "bos_ProcessNotifierGet");
   lpBosProcessRestart = (lpBosProcessRestart_t)GetProcAddress (hiBOS, "bos_ProcessRestart");
   lpBosProcessAllStop = (lpBosProcessAllStop_t)GetProcAddress (hiBOS, "bos_ProcessAllStop");
   lpBosProcessAllStart = (lpBosProcessAllStart_t)GetProcAddress (hiBOS, "bos_ProcessAllStart");
   lpBosProcessAllWaitStop = (lpBosProcessAllWaitStop_t)GetProcAddress (hiBOS, "bos_ProcessAllWaitStop");
   lpBosProcessAllStopAndRestart = (lpBosProcessAllStopAndRestart_t)GetProcAddress (hiBOS, "bos_ProcessAllStopAndRestart");
   lpBosAdminCreate = (lpBosAdminCreate_t)GetProcAddress (hiBOS, "bos_AdminCreate");
   lpBosAdminDelete = (lpBosAdminDelete_t)GetProcAddress (hiBOS, "bos_AdminDelete");
   lpBosAdminGetBegin = (lpBosAdminGetBegin_t)GetProcAddress (hiBOS, "bos_AdminGetBegin");
   lpBosAdminGetDone = (lpBosAdminGetDone_t)GetProcAddress (hiBOS, "bos_AdminGetDone");
   lpBosAdminGetNext = (lpBosAdminGetNext_t)GetProcAddress (hiBOS, "bos_AdminGetNext");
   lpBosKeyCreate = (lpBosKeyCreate_t)GetProcAddress (hiBOS, "bos_KeyCreate");
   lpBosKeyDelete = (lpBosKeyDelete_t)GetProcAddress (hiBOS, "bos_KeyDelete");
   lpBosKeyGetBegin = (lpBosKeyGetBegin_t)GetProcAddress (hiBOS, "bos_KeyGetBegin");
   lpBosKeyGetDone = (lpBosKeyGetDone_t)GetProcAddress (hiBOS, "bos_KeyGetDone");
   lpBosKeyGetNext = (lpBosKeyGetNext_t)GetProcAddress (hiBOS, "bos_KeyGetNext");
   lpBosCellSet = (lpBosCellSet_t)GetProcAddress (hiBOS, "bos_CellSet");
   lpBosCellGet = (lpBosCellGet_t)GetProcAddress (hiBOS, "bos_CellGet");
   lpBosHostCreate = (lpBosHostCreate_t)GetProcAddress (hiBOS, "bos_HostCreate");
   lpBosHostDelete = (lpBosHostDelete_t)GetProcAddress (hiBOS, "bos_HostDelete");
   lpBosHostGetBegin = (lpBosHostGetBegin_t)GetProcAddress (hiBOS, "bos_HostGetBegin");
   lpBosHostGetDone = (lpBosHostGetDone_t)GetProcAddress (hiBOS, "bos_HostGetDone");
   lpBosHostGetNext = (lpBosHostGetNext_t)GetProcAddress (hiBOS, "bos_HostGetNext");
   lpBosExecutableCreate = (lpBosExecutableCreate_t)GetProcAddress (hiBOS, "bos_ExecutableCreate");
   lpBosExecutableRevert = (lpBosExecutableRevert_t)GetProcAddress (hiBOS, "bos_ExecutableRevert");
   lpBosExecutableTimestampGet = (lpBosExecutableTimestampGet_t)GetProcAddress (hiBOS, "bos_ExecutableTimestampGet");
   lpBosExecutablePrune = (lpBosExecutablePrune_t)GetProcAddress (hiBOS, "bos_ExecutablePrune");
   lpBosExecutableRestartTimeSet = (lpBosExecutableRestartTimeSet_t)GetProcAddress (hiBOS, "bos_ExecutableRestartTimeSet");
   lpBosExecutableRestartTimeGet = (lpBosExecutableRestartTimeGet_t)GetProcAddress (hiBOS, "bos_ExecutableRestartTimeGet");
   lpBosLogGet = (lpBosLogGet_t)GetProcAddress (hiBOS, "bos_LogGet");
   lpBosAuthSet = (lpBosAuthSet_t)GetProcAddress (hiBOS, "bos_AuthSet");
   lpBosCommandExecute = (lpBosCommandExecute_t)GetProcAddress (hiBOS, "bos_CommandExecute");
   lpBosSalvage = (lpBosSalvage_t)GetProcAddress (hiBOS, "bos_Salvage");

   lpKasServerOpen = (lpKasServerOpen_t)GetProcAddress (hiKAS, "kas_ServerOpen");
   lpKasServerClose = (lpKasServerClose_t)GetProcAddress (hiKAS, "kas_ServerClose");
   lpKasServerStatsGet = (lpKasServerStatsGet_t)GetProcAddress (hiKAS, "kas_ServerStatsGet");
   lpKasServerDebugGet = (lpKasServerDebugGet_t)GetProcAddress (hiKAS, "kas_ServerDebugGet");
   lpKasServerRandomKeyGet = (lpKasServerRandomKeyGet_t)GetProcAddress (hiKAS, "kas_ServerRandomKeyGet");
   lpKasStringToKey = (lpKasStringToKey_t)GetProcAddress (hiKAS, "kas_StringToKey");
   lpKasPrincipalCreate = (lpKasPrincipalCreate_t)GetProcAddress (hiKAS, "kas_PrincipalCreate");
   lpKasPrincipalDelete = (lpKasPrincipalDelete_t)GetProcAddress (hiKAS, "kas_PrincipalDelete");
   lpKasPrincipalGet = (lpKasPrincipalGet_t)GetProcAddress (hiKAS, "kas_PrincipalGet");
   lpKasPrincipalGetBegin = (lpKasPrincipalGetBegin_t)GetProcAddress (hiKAS, "kas_PrincipalGetBegin");
   lpKasPrincipalGetNext = (lpKasPrincipalGetNext_t)GetProcAddress (hiKAS, "kas_PrincipalGetNext");
   lpKasPrincipalGetDone = (lpKasPrincipalGetDone_t)GetProcAddress (hiKAS, "kas_PrincipalGetDone");
   lpKasPrincipalKeySet = (lpKasPrincipalKeySet_t)GetProcAddress (hiKAS, "kas_PrincipalKeySet");
   lpKasPrincipalLockStatusGet = (lpKasPrincipalLockStatusGet_t)GetProcAddress (hiKAS, "kas_PrincipalLockStatusGet");
   lpKasPrincipalUnlock = (lpKasPrincipalUnlock_t)GetProcAddress (hiKAS, "kas_PrincipalUnlock");
   lpKasPrincipalFieldsSet = (lpKasPrincipalFieldsSet_t)GetProcAddress (hiKAS, "kas_PrincipalFieldsSet");

   lpPtsGroupMemberAdd = (lpPtsGroupMemberAdd_t)GetProcAddress (hiPTS, "pts_GroupMemberAdd");
   lpPtsGroupOwnerChange = (lpPtsGroupOwnerChange_t)GetProcAddress (hiPTS, "pts_GroupOwnerChange");
   lpPtsGroupCreate = (lpPtsGroupCreate_t)GetProcAddress (hiPTS, "pts_GroupCreate");
   lpPtsGroupGet = (lpPtsGroupGet_t)GetProcAddress (hiPTS, "pts_GroupGet");
   lpPtsGroupDelete = (lpPtsGroupDelete_t)GetProcAddress (hiPTS, "pts_GroupDelete");
   lpPtsGroupMaxGet = (lpPtsGroupMaxGet_t)GetProcAddress (hiPTS, "pts_GroupMaxGet");
   lpPtsGroupMaxSet = (lpPtsGroupMaxSet_t)GetProcAddress (hiPTS, "pts_GroupMaxSet");
   lpPtsGroupMemberListBegin = (lpPtsGroupMemberListBegin_t)GetProcAddress (hiPTS, "pts_GroupMemberListBegin");
   lpPtsGroupMemberListNext = (lpPtsGroupMemberListNext_t)GetProcAddress (hiPTS, "pts_GroupMemberListNext");
   lpPtsGroupMemberListDone = (lpPtsGroupMemberListDone_t)GetProcAddress (hiPTS, "pts_GroupMemberListDone");
   lpPtsGroupMemberRemove = (lpPtsGroupMemberRemove_t)GetProcAddress (hiPTS, "pts_GroupMemberRemove");
   lpPtsGroupRename = (lpPtsGroupRename_t)GetProcAddress (hiPTS, "pts_GroupRename");
   lpPtsGroupModify = (lpPtsGroupModify_t)GetProcAddress (hiPTS, "pts_GroupModify");
   lpPtsUserCreate = (lpPtsUserCreate_t)GetProcAddress (hiPTS, "pts_UserCreate");
   lpPtsUserDelete = (lpPtsUserDelete_t)GetProcAddress (hiPTS, "pts_UserDelete");
   lpPtsUserGet = (lpPtsUserGet_t)GetProcAddress (hiPTS, "pts_UserGet");
   lpPtsUserRename = (lpPtsUserRename_t)GetProcAddress (hiPTS, "pts_UserRename");
   lpPtsUserModify = (lpPtsUserModify_t)GetProcAddress (hiPTS, "pts_UserModify");
   lpPtsUserMaxGet = (lpPtsUserMaxGet_t)GetProcAddress (hiPTS, "pts_UserMaxGet");
   lpPtsUserMaxSet = (lpPtsUserMaxSet_t)GetProcAddress (hiPTS, "pts_UserMaxSet");
   lpPtsUserMemberListBegin = (lpPtsUserMemberListBegin_t)GetProcAddress (hiPTS, "pts_UserMemberListBegin");
   lpPtsUserMemberListNext = (lpPtsUserMemberListNext_t)GetProcAddress (hiPTS, "pts_UserMemberListNext");
   lpPtsUserMemberListDone = (lpPtsUserMemberListDone_t)GetProcAddress (hiPTS, "pts_UserMemberListDone");
   lpPtsOwnedGroupListBegin = (lpPtsOwnedGroupListBegin_t)GetProcAddress (hiPTS, "pts_OwnedGroupListBegin");
   lpPtsOwnedGroupListNext = (lpPtsOwnedGroupListNext_t)GetProcAddress (hiPTS, "pts_OwnedGroupListNext");
   lpPtsOwnedGroupListDone = (lpPtsOwnedGroupListDone_t)GetProcAddress (hiPTS, "pts_OwnedGroupListDone");

   lpClientTokenGetExisting = (lpClientTokenGetExisting_t)GetProcAddress (hiClient, "afsclient_TokenGetExisting");
   lpClientCellOpen = (lpClientCellOpen_t)GetProcAddress (hiClient, "afsclient_CellOpen");
   lpClientCellClose = (lpClientCellClose_t)GetProcAddress (hiClient, "afsclient_CellClose");
   lpClientLocalCellGet = (lpClientLocalCellGet_t)GetProcAddress (hiClient, "afsclient_LocalCellGet");
   lpClientAFSServerGet = (lpClientAFSServerGet_t)GetProcAddress (hiClient, "afsclient_AFSServerGet");
   lpClientAFSServerGetBegin = (lpClientAFSServerGetBegin_t)GetProcAddress (hiClient, "afsclient_AFSServerGetBegin");
   lpClientAFSServerGetNext = (lpClientAFSServerGetNext_t)GetProcAddress (hiClient, "afsclient_AFSServerGetNext");
   lpClientAFSServerGetDone = (lpClientAFSServerGetDone_t)GetProcAddress (hiClient, "afsclient_AFSServerGetDone");
   lpClientInit = (lpClientInit_t)GetProcAddress (hiClient, "afsclient_Init");

   lpUtilDatabaseServerGetBegin = (lpUtilDatabaseServerGetBegin_t)GetProcAddress (hiUtil, "util_DatabaseServerGetBegin");
   lpUtilDatabaseServerGetNext = (lpUtilDatabaseServerGetNext_t)GetProcAddress (hiUtil, "util_DatabaseServerGetNext");
   lpUtilDatabaseServerGetDone = (lpUtilDatabaseServerGetDone_t)GetProcAddress (hiUtil, "util_DatabaseServerGetDone");

   // ADD HERE

   if ( (lpVosBackupVolumeCreate == NULL) ||
        (lpVosBackupVolumeCreateMultiple == NULL) ||
        (lpVosPartitionGet == NULL) ||
        (lpVosPartitionGetBegin == NULL) ||
        (lpVosPartitionGetNext == NULL) ||
        (lpVosPartitionGetDone == NULL) ||
        (lpVosServerOpen == NULL) ||
        (lpVosServerClose == NULL) ||
        (lpVosServerSync == NULL) ||
        (lpVosFileServerAddressChange == NULL) ||
        (lpVosFileServerAddressRemove == NULL) ||
        (lpVosFileServerGetBegin == NULL) ||
        (lpVosFileServerGetNext == NULL) ||
        (lpVosFileServerGetDone == NULL) ||
        (lpVosServerTransactionStatusGetBegin == NULL) ||
        (lpVosServerTransactionStatusGetNext == NULL) ||
        (lpVosServerTransactionStatusGetDone == NULL) ||
        (lpVosVLDBGet == NULL) ||
        (lpVosVLDBGetBegin == NULL) ||
        (lpVosVLDBGetNext == NULL) ||
        (lpVosVLDBGetDone == NULL) ||
        (lpVosVLDBEntryRemove == NULL) ||
        (lpVosVLDBUnlock == NULL) ||
        (lpVosVLDBEntryLock == NULL) ||
        (lpVosVLDBEntryUnlock == NULL) ||
        (lpVosVLDBReadOnlySiteCreate == NULL) ||
        (lpVosVLDBReadOnlySiteDelete == NULL) ||
        (lpVosVLDBSync == NULL) ||
        (lpVosVolumeCreate == NULL) ||
        (lpVosVolumeDelete == NULL) ||
        (lpVosVolumeRename == NULL) ||
        (lpVosVolumeDump == NULL) ||
        (lpVosVolumeRestore == NULL) ||
        (lpVosVolumeOnline == NULL) ||
        (lpVosVolumeOffline == NULL) ||
        (lpVosVolumeGet == NULL) ||
        (lpVosVolumeGetBegin == NULL) ||
        (lpVosVolumeGetNext == NULL) ||
        (lpVosVolumeGetDone == NULL) ||
        (lpVosVolumeMove == NULL) ||
        (lpVosVolumeRelease == NULL) ||
        (lpVosVolumeZap == NULL) ||
        (lpVosPartitionNameToId == NULL) ||
        (lpVosPartitionIdToName == NULL) ||
        (lpVosVolumeQuotaChange == NULL) ||
        (lpBosServerOpen == NULL) ||
        (lpBosServerClose == NULL) ||
        (lpBosProcessCreate == NULL) ||
        (lpBosProcessDelete == NULL) ||
        (lpBosProcessExecutionStateGet == NULL) ||
        (lpBosProcessExecutionStateSet == NULL) ||
        (lpBosProcessExecutionStateSetTemporary == NULL) ||
        (lpBosProcessNameGetBegin == NULL) ||
        (lpBosProcessNameGetDone == NULL) ||
        (lpBosProcessNameGetNext == NULL) ||
        (lpBosProcessInfoGet == NULL) ||
        (lpBosProcessParameterGetBegin == NULL) ||
        (lpBosProcessParameterGetDone == NULL) ||
        (lpBosProcessParameterGetNext == NULL) ||
        (lpBosProcessNotifierGet == NULL) ||
        (lpBosProcessRestart == NULL) ||
        (lpBosProcessAllStop == NULL) ||
        (lpBosProcessAllStart == NULL) ||
        (lpBosProcessAllWaitStop == NULL) ||
        (lpBosProcessAllStopAndRestart == NULL) ||
        (lpBosAdminCreate == NULL) ||
        (lpBosAdminDelete == NULL) ||
        (lpBosAdminGetBegin == NULL) ||
        (lpBosAdminGetDone == NULL) ||
        (lpBosAdminGetNext == NULL) ||
        (lpBosKeyCreate == NULL) ||
        (lpBosKeyDelete == NULL) ||
        (lpBosKeyGetBegin == NULL) ||
        (lpBosKeyGetDone == NULL) ||
        (lpBosKeyGetNext == NULL) ||
        (lpBosCellSet == NULL) ||
        (lpBosCellGet == NULL) ||
        (lpBosHostCreate == NULL) ||
        (lpBosHostDelete == NULL) ||
        (lpBosHostGetBegin == NULL) ||
        (lpBosHostGetDone == NULL) ||
        (lpBosHostGetNext == NULL) ||
        (lpBosExecutableCreate == NULL) ||
        (lpBosExecutableRevert == NULL) ||
        (lpBosExecutableTimestampGet == NULL) ||
        (lpBosExecutablePrune == NULL) ||
        (lpBosExecutableRestartTimeSet == NULL) ||
        (lpBosExecutableRestartTimeGet == NULL) ||
        (lpBosLogGet == NULL) ||
        (lpBosAuthSet == NULL) ||
        (lpBosCommandExecute == NULL) ||
        (lpBosSalvage == NULL) ||
        (lpKasServerOpen == NULL) ||
        (lpKasServerClose == NULL) ||
        (lpKasServerStatsGet == NULL) ||
        (lpKasServerDebugGet == NULL) ||
        (lpKasServerRandomKeyGet == NULL) ||
        (lpKasStringToKey == NULL) ||
        (lpKasPrincipalCreate == NULL) ||
        (lpKasPrincipalDelete == NULL) ||
        (lpKasPrincipalGet == NULL) ||
        (lpKasPrincipalGetBegin == NULL) ||
        (lpKasPrincipalGetNext == NULL) ||
        (lpKasPrincipalGetDone == NULL) ||
        (lpKasPrincipalKeySet == NULL) ||
        (lpKasPrincipalLockStatusGet == NULL) ||
        (lpKasPrincipalUnlock == NULL) ||
        (lpKasPrincipalFieldsSet == NULL) ||
        (lpPtsGroupMemberAdd == NULL) ||
        (lpPtsGroupOwnerChange == NULL) ||
        (lpPtsGroupCreate == NULL) ||
        (lpPtsGroupGet == NULL) ||
        (lpPtsGroupDelete == NULL) ||
        (lpPtsGroupMaxGet == NULL) ||
        (lpPtsGroupMaxSet == NULL) ||
        (lpPtsGroupMemberListBegin == NULL) ||
        (lpPtsGroupMemberListNext == NULL) ||
        (lpPtsGroupMemberListDone == NULL) ||
        (lpPtsGroupMemberRemove == NULL) ||
        (lpPtsGroupRename == NULL) ||
        (lpPtsGroupModify == NULL) ||
        (lpPtsUserCreate == NULL) ||
        (lpPtsUserDelete == NULL) ||
        (lpPtsUserGet == NULL) ||
        (lpPtsUserRename == NULL) ||
        (lpPtsUserModify == NULL) ||
        (lpPtsUserMaxGet == NULL) ||
        (lpPtsUserMaxSet == NULL) ||
        (lpPtsUserMemberListBegin == NULL) ||
        (lpPtsUserMemberListNext == NULL) ||
        (lpPtsUserMemberListDone == NULL) ||
        (lpPtsOwnedGroupListBegin == NULL) ||
        (lpPtsOwnedGroupListNext == NULL) ||
        (lpPtsOwnedGroupListDone == NULL) ||
        (lpClientTokenGetExisting == NULL) ||
        (lpClientCellOpen == NULL) ||
        (lpClientCellClose == NULL) ||
        (lpClientLocalCellGet == NULL) ||
        (lpClientInit == NULL) ||
        (lpClientAFSServerGet == NULL) ||
        (lpClientAFSServerGetBegin == NULL) ||
        (lpClientAFSServerGetNext == NULL) ||
        (lpClientAFSServerGetDone == NULL) ||
        (lpUtilDatabaseServerGetBegin == NULL) ||
        (lpUtilDatabaseServerGetNext == NULL) ||
        (lpUtilDatabaseServerGetDone == NULL) ||
        // ADD HERE
        0 )
      {
      if (pStatus)
         *pStatus = ERROR_DLL_INIT_FAILED;
      return FALSE;
      }

   // Initialize the client library
   //
   afs_status_t status;
   if (!(*lpClientInit)(&status))
      {
      if (pStatus)
         *pStatus = (ULONG)status;
      return FALSE;
      }

   return TRUE;
}


void Worker_FreeLibraries (void)
{
   if (hiVOS != NULL)
      {
      FreeLibrary (hiVOS);
      hiVOS = NULL;
      }
   if (hiBOS != NULL)
      {
      FreeLibrary (hiBOS);
      hiBOS = NULL;
      }
   if (hiKAS != NULL)
      {
      FreeLibrary (hiKAS);
      hiKAS = NULL;
      }
   if (hiPTS != NULL)
      {
      FreeLibrary (hiPTS);
      hiPTS = NULL;
      }
   if (hiUtil != NULL)
      {
      FreeLibrary (hiUtil);
      hiUtil = NULL;
      }
   if (hiClient != NULL)
      {
      FreeLibrary (hiClient);
      hiClient = NULL;
      }
}

