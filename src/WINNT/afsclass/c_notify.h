/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_NOTIFY_H
#define AFSCLASS_NOTIFY_H

#include <WINNT/afsclass.h>


/*
 * NOTIFICATION CLASS _________________________________________________________
 *
 */

typedef enum
   {
   evtCreate,	// lpi1 = new object
   evtDestroy,	// lpi1 = object to delete

   evtInvalidate,	// lpi1 = object invalidated
   evtRefreshStatusBegin,	// lpi1 = object to refresh
   evtRefreshStatusEnd,	// lpi1 = object refreshed
   evtRefreshServersBegin,	// lpi1 = parent cell
   evtRefreshServersEnd,	// lpi1 = parent cell
   evtRefreshServicesBegin,	// lpi1 = parent server
   evtRefreshServicesEnd,	// lpi1 = parent server
   evtRefreshAggregatesBegin,	// lpi1 = parent server
   evtRefreshAggregatesEnd,	// lpi1 = parent server
   evtRefreshFilesetsBegin,	// lpi1 = parent aggregate
   evtRefreshFilesetsEnd,	// lpi1 = parent aggregate
   evtRefreshUsersBegin,	// lpi1 = parent cell
   evtRefreshUsersEnd,	// lpi1 = parent cell

   // For the evtRefreshAll* events, the dw1 parameter accompanying the event
   // is an integer percentage complete.
   //
   evtRefreshAllBegin,	// lpi1 = object refreshing
   evtRefreshAllUpdate,	// lpi1 = object working now
   evtRefreshAllSectionStart,	// dw2 = section identifier
   evtRefreshAllSectionEnd,	// dw2 = section identifier
   evtRefreshAllEnd,

   // For the evtOpenCell* events, the sz1 parameter accompanying the event
   // is a string reflecting the name of the cell being opened.
   //
   evtOpenCellBegin,	// sz1 = pszCellName
   evtOpenCellEnd,	// sz1 = pszCellName

   // For the evtSyncVldb* events, the lpi1 parameter accompanying the event
   // describes the server or aggregate being sync'd.
   //
   evtSyncVLDBBegin,	// lpi1 = server or aggregate
   evtSyncVLDBEnd,	// lpi1 = server or aggregate

   // For the evtGetServerLogFile events, the sz1 parameter accompanying the
   // event is a string describing the filename of the log file to be obtained.
   // The lpi1 parameter accompanying the event indicates the server being
   // queried.
   //
   evtGetServerLogFileBegin,	// lpi1 = server, sz1 = log name
   evtGetServerLogFileEnd,	// lpi1 = server, sz1 = log name

   // For the evtInstallFile*, evtUninstallFile*, and evtGetFileDates* events,
   // the lpi1 parameter accompanying the event indicates the server where
   // the operation is taking place, while the sz1 parameter indicates the
   // name of the file being manipulated.
   //
   evtInstallFileBegin,	// lpi1 = server, sz1 = filename
   evtInstallFileEnd,	// lpi1 = server, sz1 = filename

   evtUninstallFileBegin,	// lpi1 = server, sz1 = filename
   evtUninstallFileEnd,	// lpi1 = server, sz1 = filename

   evtGetFileDatesBegin,	// lpi1 = server, sz1 = filename
   evtGetFileDatesEnd,	// lpi1 = server, sz1 = filename

   // For most other evt*Server* events, the lpi1 parameter accompanying
   // the event indicates the server being changed.
   //
   evtPruneFilesBegin,	// lpi1 = server
   evtPruneFilesEnd,	// lpi1 = server

   evtSetServerAuthBegin,	// lpi1 = server
   evtSetServerAuthEnd,	// lpi1 = server

   evtGetRestartTimesBegin,	// lpi1 = server
   evtGetRestartTimesEnd,	// lpi1 = server

   evtSetRestartTimesBegin,	// lpi1 = server
   evtSetRestartTimesEnd,	// lpi1 = server

   evtChangeAddressBegin,	// lpi1 = server
   evtChangeAddressEnd,	// lpi1 = server

   // For the evtAdminListLoad* events, the lpi1 parameter accompanying the
   // event indicates the server from which the admin list is being read.
   //
   evtAdminListLoadBegin,	// lpi1 = server
   evtAdminListLoadEnd,	// lpi1 = server

   // For most other evtAdminList* events, the lpi1 parameter accompanying
   // the event indicates the server being manipulated.
   //
   evtAdminListSaveBegin,	// lpi1 = server
   evtAdminListSaveEnd,	// lpi1 = server

   // For the evtKeyListLoad* events, the lpi1 parameter accompanying the
   // event indicates the server from which the list of keys is being read.
   //
   evtKeyListLoadBegin,	// lpi1 = server
   evtKeyListLoadEnd,	// lpi1 = server

   // For the evtHostListLoad* events, the lpi1 parameter accompanying the
   // event indicates the server from which the host list is being read.
   //
   evtHostListLoadBegin,	// lpi1 = server
   evtHostListLoadEnd,	// lpi1 = server

   // For most other evtHostList* events, the lpi1 parameter accompanying the
   // event indicates the server being manipulated.
   //
   evtHostListSaveBegin,	// lpi1 = server
   evtHostListSaveEnd,	// lpi1 = server

   // For the evtCreateService events, the lpi1 parameter accompanying the
   // event indicates the parent server, while the sz1 parameter indicates
   // the name of the service being created.
   //
   evtCreateServiceBegin,	// lpi1 = server, sz1 = service
   evtCreateServiceEnd,	// lpi1 = server, sz1 = service

   // For most other evt*Service* events, the lpi1 parameter accompanying
   // the event describes the service being changed.
   //
   evtDeleteServiceBegin,	// lpi1 = service
   evtDeleteServiceEnd,	// lpi1 = service

   evtStartServiceBegin,	// lpi1 = service
   evtStartServiceEnd,	// lpi1 = service

   evtStopServiceBegin,	// lpi1 = service
   evtStopServiceEnd,	// lpi1 = service

   evtRestartServiceBegin,	// lpi1 = service
   evtRestartServiceEnd,	// lpi1 = service

   // For the evtCreateFileset events, the lpi1 parameter accompanying the
   // event indicates the parent aggregate, while the sz1 parameter indicates
   // the name of the fileset.
   //
   evtCreateFilesetBegin,	// lpi1 = aggregate, sz1 = fileset name
   evtCreateFilesetEnd,	// lpi1 = aggregate, sz1 = fileset name

   // For the evtMoveFileset* events, the lpi1 parameter indicates the fileset
   // being moved while the lpi2 parameter indicates the target aggregate.
   //
   evtMoveFilesetBegin,	// lpi1 = fileset, lpi2 = aggregate
   evtMoveFilesetEnd,	// lpi1 = fileset, lpi2 = aggregate

   // For the evtCreateReplica* events, the lpi1 parameter indicates the
   // fileset being replicated, while the lpi2 parameter indicates the
   // aggregate on which the new replica will be created.
   //
   evtCreateReplicaBegin,	// lpi1 = fileset, lpi2 = aggregate
   evtCreateReplicaEnd,	// lpi1 = fileset, lpi2 = aggregate

   // For the evtRenameFileset* events, the lpi1 parameter indicates the
   // fileset being renamed, while the sz1 parameter indicates the new
   // name for the fileset.
   //
   evtRenameFilesetBegin,	// lpi1 = fileset, sz1 = new name
   evtRenameFilesetEnd,	// lpi1 = fileset, sz1 = new name

   // For the evtCloneMultiple* events, the lpi1 parameter accompanying the
   // event indicates the scope of the operation.
   //
   evtCloneMultipleBegin,	// lpi1 = scope of clonesys
   evtCloneMultipleEnd,	// lpi1 = scope of clonesys

   // For the evtDumpFileset* events, the lpi1 parameter accompanying the
   // event indicates the fileset being dumped, while the sz1 parameter
   // indicates the filename of the dump.
   //
   evtDumpFilesetBegin,	// lpi1 = fileset, sz1 = dump filename
   evtDumpFilesetEnd,	// lpi1 = fileset, sz1 = dump filename

   // For the evtRestoreFileset* events, the sz1 parameter accompanying the
   // event indicates the name of the fileset being restored, while the sz2
   // parameter indicates the name of the dump file being used. The lpi1
   // parameter indicates the aggregate or fileset on which the restore
   // operation will be performed.
   //
   evtRestoreFilesetBegin,	// sz1 = fileset name, sz2 = filename
   evtRestoreFilesetEnd,	// sz1 = fileset name, sz2 = filename

   // For most other evt*Fileset* events, the lpi1 parameter accompanying the
   // event describes the fileset being changed.
   //
   evtDeleteFilesetBegin,	// lpi1 = fileset
   evtDeleteFilesetEnd,	// lpi1 = fileset

   evtSetFilesetQuotaBegin,	// lpi1 = fileset
   evtSetFilesetQuotaEnd,	// lpi1 = fileset

   evtLockFilesetBegin,	// lpi1 = fileset
   evtLockFilesetEnd,	// lpi1 = fileset

   evtUnlockFilesetBegin,	// lpi1 = fileset
   evtUnlockFilesetEnd,	// lpi1 = fileset

   evtUnlockAllFilesetsBegin,	// lpi1 = cell, server or agg
   evtUnlockAllFilesetsEnd,	// lpi1 = cell, server or agg

   evtSetFilesetRepParamsBegin,	// lpi1 = fileset
   evtSetFilesetRepParamsEnd,	// lpi1 = fileset

   evtReleaseFilesetBegin,	// lpi1 = rw fileset
   evtReleaseFilesetEnd,	// lpi1 = rw fileset

   evtCloneBegin,	// lpi1 = rw fileset
   evtCloneEnd,	// lpi1 = rw fileset

   evtSetFilesetOwnerGroupBegin,	// lpi1 = fileset
   evtSetFilesetOwnerGroupEnd,	// lpi1 = fileset

   evtSetFilesetPermissionsBegin,	// lpi1 = fileset
   evtSetFilesetPermissionsEnd,	// lpi1 = fileset

   // For the evtExecuteCommand events, the lpi1 parameter accompanying the
   // event indicates the server on which the command is being executed,
   // while the sz1 parameter contains the command-line being executed.
   //
   evtExecuteCommandBegin,	// lpi1 = server, sz1 = command
   evtExecuteCommandEnd,	// lpi1 = server, sz1 = command

   // For the evtSalvage events, the lpi1 parameter accompanying the event
   // indicates the scope of the salvage--it will point to either a server,
   // an aggregate, or a volume.
   //
   evtSalvageBegin,	// lpi1 = salvage scope
   evtSalvageEnd,	// lpi1 = salvage scope

   // For the evtCreateUser events, the lpi1 parameter accompanying the event
   // indicates the parent cell for the operation, while the sz1 parameter
   // contains the name of the user being created.
   //
   evtCreateUserBegin,	// lpi1 = cell, sz1 = user name
   evtCreateUserEnd,	// lpi1 = cell, sz1 = user name

   // For most other evtUser* events, the lpi1 parameter accompanying the
   // event identifies the user being manipulated.
   //
   evtChangeUserBegin,	// lpi1 = user being changed
   evtChangeUserEnd,	// lpi1 = user being changed

   evtChangeUserPasswordBegin,	// lpi1 = user being changed
   evtChangeUserPasswordEnd,	// lpi1 = user being changed

   evtDeleteUserBegin,	// lpi1 = user being deleted
   evtDeleteUserEnd,	// lpi1 = user being deleted

   evtUnlockUserBegin,	// lpi1 = user being unlocked
   evtUnlockUserEnd,	// lpi1 = user being unlocked

   // For the evtCreateGroup events, the lpi1 parameter accompanying the event
   // indicates the parent cell for the operation, while the sz1 parameter
   // contains the name of the group being created.
   //
   evtCreateGroupBegin,	// lpi1 = cell, sz1 = group name
   evtCreateGroupEnd,	// lpi1 = cell, sz1 = group name

   // For most other evtGroup* events, the lpi1 parameter accompanying the
   // event identifies the group being manipulated.
   //
   evtChangeGroupBegin,	// lpi1 = group being changed
   evtChangeGroupEnd,	// lpi1 = group being changed

   evtRenameGroupBegin,	// lpi1 = group being renamed
   evtRenameGroupEnd,	// lpi1 = group being renamed

   evtDeleteGroupBegin,	// lpi1 = group being deleted
   evtDeleteGroupEnd,	// lpi1 = group being deleted

   // For the evtGroupMemberAdd/Remove* events, the lpi1 parameter
   // accompanying the event identifies the group being manipulated,
   // while the lpi2 parameter indicates the user being added or removed.
   //
   evtGroupMemberAddBegin,	// lpi1 = group, lpi2 = user
   evtGroupMemberAddEnd,	// lpi1 = group, lpi2 = user

   evtGroupMemberRemoveBegin,	// lpi1 = group, lpi2 = user
   evtGroupMemberRemoveEnd,	// lpi1 = group, lpi2 = user

   evtUser = 500	// add any new notifications here
   } NOTIFYEVENT;

typedef struct
   {
   LPIDENT lpi1;
   LPIDENT lpi2;
   TCHAR sz1[MAX_PATH];
   TCHAR sz2[MAX_PATH];
   DWORD dw1;
   ULONG status;
   LPARAM lpUser;
   } NOTIFYPARAMS, *PNOTIFYPARAMS;


typedef BOOL (CALLBACK *NOTIFYCALLBACKPROC)( NOTIFYEVENT evt, PNOTIFYPARAMS pParams );

class NOTIFYCALLBACK
   {
   public:
      NOTIFYCALLBACK (NOTIFYCALLBACKPROC procUser, LPARAM lpUser);
      ~NOTIFYCALLBACK (void);

      BOOL SendNotification (NOTIFYEVENT evt, PNOTIFYPARAMS pParams);

      static BOOL SendNotificationToAll (NOTIFYEVENT evt, ULONG status = 0);
      static BOOL SendNotificationToAll (NOTIFYEVENT evt, LPIDENT lpi1, ULONG status = 0);
      static BOOL SendNotificationToAll (NOTIFYEVENT evt, LPTSTR psz1, ULONG status = 0);
      static BOOL SendNotificationToAll (NOTIFYEVENT evt, LPIDENT lpi1, LPTSTR psz1, ULONG status);
      static BOOL SendNotificationToAll (NOTIFYEVENT evt, LPIDENT lpi1, LPIDENT lpi2, LPTSTR psz1, LPTSTR psz2, DWORD dw1, ULONG status);

   // Private data
   //
   private:
      NOTIFYCALLBACKPROC procSupplied;
      LPARAM lpSupplied;

      static size_t nNotifyList;
      static LPNOTIFYCALLBACK *aNotifyList;
   };


#endif // AFSCLASS_NOTIFY_H

