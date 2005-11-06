/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef WORKER_H
#define WORKER_H
extern "C" {
#include <afs/afs_Admin.h>
#include <afs/afs_utilAdmin.h>
#include <afs/afs_vosAdmin.h>
#include <afs/afs_bosAdmin.h>
#include <afs/afs_kasAdmin.h>
#include <afs/afs_ptsAdmin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/pterror.h>

#define __KAUTILS__ // prevents including anything but error #s from kautils.h
#include <afs/kautils.h>
} // extern "C"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define NO_PARTITION  ((unsigned int)-1)
#define NO_VOLUME     ((VOLUMEID)-1)

typedef enum // WORKERTASK
   {
   wtaskVosBackupVolumeCreate,
   wtaskVosBackupVolumeCreateMultiple,
   wtaskVosPartitionGet,
   wtaskVosPartitionGetBegin,
   wtaskVosPartitionGetNext,
   wtaskVosPartitionGetDone,
   wtaskVosServerOpen,
   wtaskVosServerClose,
   wtaskVosServerSync,
   wtaskVosFileServerAddressChange,
   wtaskVosFileServerAddressRemove,
   wtaskVosFileServerGetBegin,
   wtaskVosFileServerGetNext,
   wtaskVosFileServerGetDone,
   wtaskVosServerTransactionStatusGetBegin,
   wtaskVosServerTransactionStatusGetNext,
   wtaskVosServerTransactionStatusGetDone,
   wtaskVosVLDBGet,
   wtaskVosVLDBGetBegin,
   wtaskVosVLDBGetNext,
   wtaskVosVLDBGetDone,
   wtaskVosVLDBEntryRemove,
   wtaskVosVLDBEntryLock,
   wtaskVosVLDBEntryUnlock,
   wtaskVosVLDBReadOnlySiteCreate,
   wtaskVosVLDBReadOnlySiteDelete,
   wtaskVosVLDBSync,
   wtaskVosVolumeCreate,
   wtaskVosVolumeDelete,
   wtaskVosVolumeRename,
   wtaskVosVolumeDump,
   wtaskVosVolumeRestore,
   wtaskVosVolumeOnline,
   wtaskVosVolumeOffline,
   wtaskVosVolumeGet,
   wtaskVosVolumeGetBegin,
   wtaskVosVolumeGetNext,
   wtaskVosVolumeGetDone,
   wtaskVosVolumeMove,
   wtaskVosVolumeRelease,
   wtaskVosVolumeZap,
   wtaskVosPartitionNameToId,
   wtaskVosPartitionIdToName,
   wtaskVosVolumeQuotaChange,
   wtaskBosServerOpen,
   wtaskBosServerClose,
   wtaskBosProcessCreate,
   wtaskBosProcessDelete,
   wtaskBosProcessExecutionStateGet,
   wtaskBosAuxiliaryProcessStatusDeallocate,
   wtaskBosProcessExecutionStateSet,
   wtaskBosProcessExecutionStateSetTemporary,
   wtaskBosProcessNameGetBegin,
   wtaskBosProcessNameGetDone,
   wtaskBosProcessNameGetNext,
   wtaskBosProcessInfoGet,
   wtaskBosProcessParameterGetBegin,
   wtaskBosProcessParameterGetDone,
   wtaskBosProcessParameterGetNext,
   wtaskBosProcessNotifierGet,
   wtaskBosProcessRestart,
   wtaskBosProcessAllStop,
   wtaskBosProcessAllStart,
   wtaskBosProcessAllWaitStop,
   wtaskBosProcessAllStopAndRestart,
   wtaskBosAdminCreate,
   wtaskBosAdminDelete,
   wtaskBosAdminGetBegin,
   wtaskBosAdminGetDone,
   wtaskBosAdminGetNext,
   wtaskBosKeyCreate,
   wtaskBosKeyDelete,
   wtaskBosKeyGetBegin,
   wtaskBosKeyGetDone,
   wtaskBosKeyGetNext,
   wtaskBosCellSet,
   wtaskBosCellGet,
   wtaskBosHostCreate,
   wtaskBosHostDelete,
   wtaskBosHostGetBegin,
   wtaskBosHostGetDone,
   wtaskBosHostGetNext,
   wtaskBosExecutableCreate,
   wtaskBosExecutableRevert,
   wtaskBosExecutableTimestampGet,
   wtaskBosExecutablePrune,
   wtaskBosExecutableRestartTimeSet,
   wtaskBosExecutableRestartTimeGet,
   wtaskBosLogGet,
   wtaskBosAuthSet,
   wtaskBosCommandExecute,
   wtaskBosSalvage,
   wtaskKasServerOpen,
   wtaskKasServerClose,
   wtaskKasServerRandomKeyGet,
   wtaskKasStringToKey,
   wtaskKasPrincipalCreate,
   wtaskKasPrincipalDelete,
   wtaskKasPrincipalGet,
   wtaskKasPrincipalGetBegin,
   wtaskKasPrincipalGetNext,
   wtaskKasPrincipalGetDone,
   wtaskKasPrincipalKeySet,
   wtaskKasPrincipalLockStatusGet,
   wtaskKasPrincipalUnlock,
   wtaskKasPrincipalFieldsSet,
   wtaskPtsGroupMemberAdd,
   wtaskPtsGroupOwnerChange,
   wtaskPtsGroupCreate,
   wtaskPtsGroupGet,
   wtaskPtsGroupDelete,
   wtaskPtsGroupMaxGet,
   wtaskPtsGroupMaxSet,
   wtaskPtsGroupMemberListBegin,
   wtaskPtsGroupMemberListNext,
   wtaskPtsGroupMemberListDone,
   wtaskPtsGroupMemberRemove,
   wtaskPtsGroupRename,
   wtaskPtsGroupModify,
   wtaskPtsUserCreate,
   wtaskPtsUserDelete,
   wtaskPtsUserGet,
   wtaskPtsUserRename,
   wtaskPtsUserModify,
   wtaskPtsUserMaxGet,
   wtaskPtsUserMaxSet,
   wtaskPtsUserMemberListBegin,
   wtaskPtsUserMemberListNext,
   wtaskPtsUserMemberListDone,
   wtaskPtsOwnedGroupListBegin,
   wtaskPtsOwnedGroupListNext,
   wtaskPtsOwnedGroupListDone,
   wtaskClientTokenGetExisting,
   wtaskClientCellOpen,
   wtaskClientCellClose,
   wtaskClientLocalCellGet,
   wtaskClientAFSServerGet,
   wtaskClientAFSServerGetBegin,
   wtaskClientAFSServerGetNext,
   wtaskClientAFSServerGetDone,
   wtaskUtilDatabaseServerGetBegin,
   wtaskUtilDatabaseServerGetNext,
   wtaskUtilDatabaseServerGetDone,
   // ADD HERE
   } WORKERTASK;

typedef union // WORKERPACKET
   {

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         VOLUMEID idVolume;	// [in] id of read/write volume
      } wpVosBackupVolumeCreate;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] server token (or NULL)
         int idPartition;	// [in] partition ID (or NO_PARTITION)
         LPTSTR pszPrefix;	// [in] volume prefix to match (or NULL)
         BOOL fExclude;	// [in] TRUE if prefix is exclusionary
      } wpVosBackupVolumeCreateMultiple;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         vos_partitionEntry_t Data;	// [out] partition properties
      } wpVosPartitionGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpVosPartitionGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         vos_partitionEntry_t Data;	// [out] partition properties
      } wpVosPartitionGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpVosPartitionGetDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszServer;	// [in] server name
         PVOID hServer;	// [out] server token
      } wpVosServerOpen;

      struct {
         PVOID hServer;	// [in] token from VosServerOpen()
      } wpVosServerClose;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID (or NO_PARTITION)
         BOOL fForce;	// [in] TRUE to force sync
      } wpVosServerSync;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         SOCKADDR_IN addrOld;	// [in] old address (or 0's to add new)
         SOCKADDR_IN addrNew;	// [in] new address (or 0's to del old)
      } wpVosFileServerAddressChange;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         SOCKADDR_IN addr;	// [in] address to remove
      } wpVosFileServerAddressRemove;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpVosFileServerGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         vos_fileServerEntry_t Entry;	// [out] server entry (with addresses)
      } wpVosFileServerGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpVosFileServerGetDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpVosServerTransactionStatusGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         vos_serverTransactionStatus_t Data; // [out] transaction information
      } wpVosServerTransactionStatusGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpVosServerTransactionStatusGetDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         VOLUMEID idVolume;	// [in] read/write volume ID
         vos_vldbEntry_t Data;	// [out] VLDB entry
      } wpVosVLDBGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] server token (or NULL)
         int idPartition;	// [in] partition ID (or NO_PARTITION)
         PVOID hEnum;	// [out] enumeration token
      } wpVosVLDBGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         vos_vldbEntry_t Data;	// [out] VLDB entry
      } wpVosVLDBGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpVosVLDBGetDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] server token (or NULL)
         int idPartition;	// [in] partition ID (or NO_PARTITION)
         VOLUMEID idVolume;	// [in] read/write volume ID
      } wpVosVLDBEntryRemove;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         VOLUMEID idVolume;	// [in] read/write volume ID
      } wpVosVLDBEntryLock;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] server token (or NULL)
         int idPartition;	// [in] partition ID (or NO_PARTITION)
         VOLUMEID idVolume;	// [in] read/write volume ID
      } wpVosVLDBEntryUnlock;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] read/write volume ID
      } wpVosVLDBReadOnlySiteCreate;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] read/write volume ID
      } wpVosVLDBReadOnlySiteDelete;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID (or NO_PARTITION)
         BOOL fForce;	// [in] TRUE to force sync
      } wpVosVLDBSync;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         LPTSTR pszVolume;	// [in] volume name
         size_t ckQuota;	// [in] initial quota
         VOLUMEID idVolume;	// [out] volume ID
      } wpVosVolumeCreate;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] volume ID
      } wpVosVolumeDelete;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         VOLUMEID idVolume;	// [in] read/write volume ID
         LPTSTR pszVolume;	// [in] new volume name
      } wpVosVolumeRename;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] parition ID
         VOLUMEID idVolume;	// [in] volume ID
         SYSTEMTIME stStart;	// [in] start time
         LPTSTR pszFilename;	// [in] target filename (local)
      } wpVosVolumeDump;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] read/write volume ID (or 0)
         LPTSTR pszVolume;	// [in] new volume name (or NULL)
         LPTSTR pszFilename;	// [in] source filename (local)
         BOOL fIncremental;	// [in] TRUE if restoring inc backup
      } wpVosVolumeRestore;

      struct {
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] read/write volume ID (or 0)
         int csecSleep;	// [in] sleep time
         vos_volumeOnlineType_t Status;	// [in] new status to indicate
      } wpVosVolumeOnline;

      struct {
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] volume ID
      } wpVosVolumeOffline;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] volume ID
         vos_volumeEntry_t Data;	// [out] volume properties
      } wpVosVolumeGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         PVOID hEnum;	// [out] enumeration token
      } wpVosVolumeGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         vos_volumeEntry_t Data;	// [out] volume properties
      } wpVosVolumeGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpVosVolumeGetDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServerFrom;	// [in] source server token
         int idPartitionFrom;	// [in] source partition ID
         PVOID hServerTo;	// [in] target server token
         int idPartitionTo;	// [in] target partition ID
         VOLUMEID idVolume;	// [in] read/write volume ID
      } wpVosVolumeMove;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         VOLUMEID idVolume;	// [in] read/write volume ID
         BOOL fForce;	// [in] TRUE to force release
      } wpVosVolumeRelease;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] volume ID
         BOOL fForce;	// [in] TRUE to force delete
      } wpVosVolumeZap;

      struct {
         LPTSTR pszPartition;	// [in] partition name
         int idPartition;	// [out] partition ID
      } wpVosPartitionNameToId;

      struct {
         int idPartition;	// [in] partition ID
         LPTSTR pszPartition;	// [out] partition name
      } wpVosPartitionIdToName;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from VosServerOpen()
         int idPartition;	// [in] partition ID
         VOLUMEID idVolume;	// [in] id of read/write volume
         size_t ckQuota;	// [in] new quota
      } wpVosVolumeQuotaChange;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszServer;	// [in] name of server to open
         PVOID hServer;	// [out] handle for server
      } wpBosServerOpen;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
      } wpBosServerClose;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] name of new service
         AFSSERVICETYPE type;	// [in] service type (stCRON, stFS, etc)
         LPTSTR pszCommand;	// [in] full command-line to execute
         LPTSTR pszTimeCron;	// [in] date/time (CRON only)
         LPTSTR pszNotifier; 	// [in] command executed on exit
      } wpBosProcessCreate;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
      } wpBosProcessDelete;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
         SERVICESTATE state;	// [out] current execution state
         LPTSTR pszAuxStatus;	// [out] additional status information
      } wpBosProcessExecutionStateGet;

      struct {
         char *pszAuxStatusA;	// [in] service string
      } wpBosAuxiliaryProcessStatusDeallocate;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
         SERVICESTATE state;	// [in] desired new service state
      } wpBosProcessExecutionStateSet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
         SERVICESTATE state;	// [in] desired new service state
      } wpBosProcessExecutionStateSetTemporary;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpBosProcessNameGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpBosProcessNameGetDone;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszService;	// [in] service name
      } wpBosProcessNameGetNext;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
         SERVICESTATUS ss;	// [out] service properties
      } wpBosProcessInfoGet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
         PVOID hEnum;	// [out] enumeration token
      } wpBosProcessParameterGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpBosProcessParameterGetDone;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszParam;	// [out] parameter
      } wpBosProcessParameterGetNext;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
         LPTSTR pszNotifier;	// [out] current notifier command
      } wpBosProcessNotifierGet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszService;	// [in] service name
      } wpBosProcessRestart;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
      } wpBosProcessAllStop;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
      } wpBosProcessAllStart;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
      } wpBosProcessAllWaitStop;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         BOOL fRestartBOS;	// [in] TRUE to also restart BOS
      } wpBosProcessAllStopAndRestart;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszAdmin;	// [in] administrator name
      } wpBosAdminCreate;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszAdmin;	// [in] administrator name
      } wpBosAdminDelete;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpBosAdminGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpBosAdminGetDone;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszAdmin;	// [out] administrator name
      } wpBosAdminGetNext;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         ENCRYPTIONKEY key;	// [in] encryption key to add
         int keyVersion;	// [in] version of new key
      } wpBosKeyCreate;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         int keyVersion;	// [in] key version number to delete
      } wpBosKeyDelete;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpBosKeyGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpBosKeyGetDone;

      struct {
         PVOID hEnum;	// [in] enumeration token
         int keyVersion;	// [out] key version number
         ENCRYPTIONKEY keyData;	// [out] encryption key
         ENCRYPTIONKEYINFO keyInfo;	// [out] additional key information
      } wpBosKeyGetNext;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszCell;	// [in] cell name to assign
      } wpBosCellSet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszCell;	// [out] currently-assigned cell name
      } wpBosCellGet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszServer;	// [in] name of server to create
      } wpBosHostCreate;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszServer;	// [in] name of server to remove
      } wpBosHostDelete;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpBosHostGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpBosHostGetDone;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszServer;	// [out] name of server
      } wpBosHostGetNext;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszLocal;	// [in] filename on local machine
         LPTSTR pszRemoteDir;	// [in] target dir on remote machine
      } wpBosExecutableCreate;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszFilename;	// [in] filename on remote machine
      } wpBosExecutableRevert;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszFilename;	// [in] filename on remote machine
         SYSTEMTIME timeNew;	// [out] file's last-modified time
         SYSTEMTIME timeOld;	// [out] .OLD file's last-modified time
         SYSTEMTIME timeBak;	// [out] .BAK file's last-modified time
      } wpBosExecutableTimestampGet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         BOOL fPruneOld;	// [in] TRUE to remove .OLD files
         BOOL fPruneBak;	// [in] TRUE to remove .BAK files
         BOOL fPruneCore;	// [in] TRUE to remove CORE files
      } wpBosExecutablePrune;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         BOOL fWeeklyRestart;	// [in] TRUE to enable weekly restart
         SYSTEMTIME timeWeekly;	// [in] new weekly restart time
         BOOL fDailyRestart;	// [in] TRUE to enable weekly restart
         SYSTEMTIME timeDaily;	// [in] new daily restart time
      } wpBosExecutableRestartTimeSet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         BOOL fWeeklyRestart;	// [out] TRUE if doing weekly restart
         SYSTEMTIME timeWeekly;	// [out] weekly restart time
         BOOL fDailyRestart;	// [out] TRUE if doing weekly restart
         SYSTEMTIME timeDaily;	// [out] daily restart time
      } wpBosExecutableRestartTimeGet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszLogName;	// [in] name of log file to retrieve
         LPTSTR pszLogData;	// [out] 0-init; GlobalFree() when done
      } wpBosLogGet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         BOOL fEnableAuth;	// [in] TRUE to enable auth checking
      } wpBosAuthSet;

      struct {
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszCommand;	// [in] command-line to execute
      } wpBosCommandExecute;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from BosServerOpen()
         LPTSTR pszAggregate;	// [in] NULL or aggregate to salvage
         LPTSTR pszFileset;	// [in] NULL or fileset to salvage
         int nProcesses;	// [in] 0 for default (4)
         LPTSTR pszTempDir;	// [in] NULL or temporary directory
         LPTSTR pszLogFile;	// [in] NULL for SalvageLog
         BOOL fForce;	// [in] TRUE to salvage all filesets
         BOOL fReadonly;	// [in] TRUE to only bring up undamaged
         BOOL fLogInodes;	// [in] TRUE to log damaged inodes
         BOOL fLogRootInodes;	// [in] TRUE to log AFS-owned inodes
         BOOL fRebuildDirs;	// [in] TRUE to rebuild dir structure
         BOOL fReadBlocks;	// [in] TRUE to read in small blocks
      } wpBosSalvage;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         char **apszServers;	// [in] 0-terminated array, or NULL
         PVOID hServer;	// [out] handle for server(s)
      } wpKasServerOpen;

      struct {
         PVOID hServer;	// [in] token from KasServerOpen()
      } wpKasServerClose;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         ENCRYPTIONKEY key;	// [out] random encryption key
      } wpKasServerRandomKeyGet;

      struct {
         LPTSTR pszCell;	// [in] cell name
         LPTSTR pszString;	// [in] string to hash
         ENCRYPTIONKEY key;	// [out] generated encryption key
      } wpKasStringToKey;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         LPTSTR pszPrincipal;	// [in] principal name
         LPTSTR pszInstance;	// [in] principal instance or NULL
         LPTSTR pszPassword;	// [in] password for new principal
      } wpKasPrincipalCreate;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         LPTSTR pszPrincipal;	// [in] principal name
         LPTSTR pszInstance;	// [in] principal instance or NULL
      } wpKasPrincipalDelete;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         LPTSTR pszPrincipal;	// [in] principal name
         LPTSTR pszInstance;	// [in] principal instance or NULL
         kas_principalEntry_t Data;	// [out] principal information
      } wpKasPrincipalGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         PVOID hEnum;	// [out] enumeration token
      } wpKasPrincipalGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszPrincipal;	// [out] principal name
         LPTSTR pszInstance;	// [out] principal instance
      } wpKasPrincipalGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpKasPrincipalGetDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         LPTSTR pszPrincipal;	// [in] principal name
         LPTSTR pszInstance;	// [in] principal instance or NULL
         int keyVersion;	// [in] key version number
         ENCRYPTIONKEY key;	// [in] encryption key
      } wpKasPrincipalKeySet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         LPTSTR pszPrincipal;	// [in] principal name
         LPTSTR pszInstance;	// [in] principal instance or NULL
         SYSTEMTIME timeUnlocked;	// [out] time lock released
      } wpKasPrincipalLockStatusGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         LPTSTR pszPrincipal;	// [in] principal name
         LPTSTR pszInstance;	// [in] principal instance or NULL
      } wpKasPrincipalUnlock;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         PVOID hServer;	// [in] token from KasServerOpen()
         LPTSTR pszPrincipal;	// [in] principal name
         LPTSTR pszInstance;	// [in] principal instance or NULL
         BOOL fIsAdmin;	// [in] TRUE if admin
         BOOL fGrantTickets;	// [in] TRUE if can get tokens
         BOOL fCanEncrypt;	// [in] TRUE if can encrypt
         BOOL fCanChangePassword;	// [in] TRUE if can change password
         BOOL fCanReusePasswords;	// [in] TRUE if can change password
         SYSTEMTIME timeExpires;	// [in] time account expires (or 0's)
         LONG cdayPwExpires;	// [in] days til password expires (or 0)
         LONG csecTicketLifetime;	// [in] ticket expiration timeout
         LONG nFailureAttempts;	// [in] num failures to tolerate (or 0)
         LONG csecFailedLoginLockTime;	// [in] csec to lock account if failed
      } wpKasPrincipalFieldsSet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to modify
         LPTSTR pszUser;	// [in] user to add to group
      } wpPtsGroupMemberAdd;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to modify
         LPTSTR pszOwner;	// [in] new owner for group
      } wpPtsGroupOwnerChange;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to create
         LPTSTR pszOwner;	// [in] owner for group
         int idGroup;	// [in out] new group ID (or 0)
      } wpPtsGroupCreate;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to query
         pts_GroupEntry_t Entry;	// [out] group information
      } wpPtsGroupGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to remove
      } wpPtsGroupDelete;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         int idGroupMax;	// [out] highest current group number
      } wpPtsGroupMaxGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         int idGroupMax;	// [in] new highest current group number
      } wpPtsGroupMaxSet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to enumerate
         PVOID hEnum;	// [out] enumeration token
      } wpPtsGroupMemberListBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszMember;	// [out] member name
      } wpPtsGroupMemberListNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpPtsGroupMemberListDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to modify
         LPTSTR pszUser;	// [in] member to remove
      } wpPtsGroupMemberRemove;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to rename
         LPTSTR pszNewName;	// [in] new name for group
      } wpPtsGroupRename;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszGroup;	// [in] group to modify
         pts_GroupUpdateEntry_t Delta;	// [in] new properties for group
      } wpPtsGroupModify;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszUser;	// [in] name for new user account
         int idUser;	// [in out] new user account ID (or 0)
      } wpPtsUserCreate;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszUser;	// [in] user to remove
      } wpPtsUserDelete;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszUser;	// [in] user to query
         pts_UserEntry_t Entry;	// [out] user properties
      } wpPtsUserGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszUser;	// [in] user to modify
         LPTSTR pszNewName;	// [in] new name for user
      } wpPtsUserRename;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszUser;	// [in] user to modify
         pts_UserUpdateEntry_t Delta;	// [in] new user properties
      } wpPtsUserModify;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         int idUserMax;	// [out] highest user ID
      } wpPtsUserMaxGet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         int idUserMax;	// [in] new highest user ID
      } wpPtsUserMaxSet;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszUser;	// [in] user to query
         PVOID hEnum;	// [out] enumeration token
      } wpPtsUserMemberListBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszGroup;	// [in] group to which user belongs
      } wpPtsUserMemberListNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpPtsUserMemberListDone;

      struct {
         PVOID hCell;	// [in] token from AdminCellOpen()
         LPTSTR pszOwner;	// [in] user to query
         PVOID hEnum;	// [out] enumeration token
      } wpPtsOwnedGroupListBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         LPTSTR pszGroup;	// [in] group which owner owns
      } wpPtsOwnedGroupListNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpPtsOwnedGroupListDone;

      struct {
         LPTSTR pszCell;	// [in] cell in which to query
         PVOID hCreds;	// [out] user credentials token
      } wpClientTokenGetExisting;

      struct {
         LPTSTR pszCell;	// [in] cell to open
         PVOID hCreds;	// [in] user credentials token
         PVOID hCell;	// [out] cell token
      } wpClientCellOpen;

      struct {
         PVOID hCell;	// [in] cell token
      } wpClientCellClose;

      struct {
         LPTSTR pszCell;	// [out] local cell name
      } wpClientLocalCellGet;

      struct {
         PVOID hCell;	// [in] cell token
         LPTSTR pszServer;	// [in] full name of server to query
         afs_serverEntry_t Entry;	// [out] server information
      } wpClientAFSServerGet;

      struct {
         PVOID hCell;	// [in] cell token
         PVOID hEnum;	// [out] enumeration token
      } wpClientAFSServerGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         afs_serverEntry_t Entry;	// [out] server information
      } wpClientAFSServerGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpClientAFSServerGetDone;

      struct {
         LPTSTR pszCell;	// [in] cell name
         PVOID hEnum;	// [out] enumeration token
      } wpUtilDatabaseServerGetBegin;

      struct {
         PVOID hEnum;	// [in] enumeration token
         SOCKADDR_IN Address;	// [out] server address
      } wpUtilDatabaseServerGetNext;

      struct {
         PVOID hEnum;	// [in] enumeration token
      } wpUtilDatabaseServerGetDone;

      // ADD HERE

   } WORKERPACKET, *LPWORKERPACKET;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL Worker_Initialize (ULONG *pStatus = NULL);

BOOL Worker_DoTask (WORKERTASK wtask, LPWORKERPACKET lpwp = NULL, ULONG *pStatus = NULL);

#endif

