/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSCLASS_FUNCTION_H
#define AFSCLASS_FUNCTION_H

/*
 * Like everything else in the AfsClass library, these routines should not
 * be called on your application's main thread; instead, devote that thread
 * to performing user-interface work, and use these routines in as many
 * background threads as you need.
 *
 * See the Overview.doc file in this directory for more information on the
 * intended threading model for the AfsClass library.
 *
 */


/*
 * SERVER MANIPULATION ________________________________________________________
 *
 */

BOOL    AfsClass_GetServerLogFile     (LPIDENT lpiServer, LPTSTR pszLocal, LPTSTR pszRemote, ULONG *pStatus = NULL);
BOOL    AfsClass_SetServerAuth        (LPIDENT lpiServer, BOOL fEnabled, ULONG *pStatus = NULL);

BOOL    AfsClass_InstallFile          (LPIDENT lpiServer, LPTSTR pszTarget, LPTSTR pszSource, ULONG *pStatus = NULL);
BOOL    AfsClass_UninstallFile        (LPIDENT lpiServer, LPTSTR pszUninstall, ULONG *pStatus = NULL);
BOOL    AfsClass_PruneOldFiles        (LPIDENT lpiServer, BOOL fBAK, BOOL fOLD, BOOL fCore, ULONG *pStatus = NULL);
BOOL    AfsClass_GetFileDates         (LPIDENT lpiServer, LPTSTR pszFilename, SYSTEMTIME *pstFile, SYSTEMTIME *pstBAK, SYSTEMTIME *pstOld, ULONG *pStatus = NULL);

BOOL    AfsClass_ExecuteCommand       (LPIDENT lpiServer, LPTSTR pszCommand, ULONG *pStatus = NULL);

BOOL    AfsClass_Salvage              (LPIDENT lpiSalvage, LPTSTR *ppszLogData, int nProcesses = 0, LPTSTR pszTempDir = NULL, LPTSTR pszLogFile = NULL, BOOL fForce = FALSE, BOOL fReadonly = FALSE, BOOL fLogInodes = FALSE, BOOL fLogRootInodes = FALSE, BOOL fRebuildDirs = FALSE, BOOL fReadBlocks = FALSE, ULONG *pStatus = NULL);
void    AfsClass_FreeSalvageLog       (LPTSTR pszLogData);

BOOL    AfsClass_SyncVLDB             (LPIDENT lpiSync, BOOL fForce = TRUE, ULONG *pStatus = NULL);

BOOL    AfsClass_ChangeAddress        (LPIDENT lpiServer, LPSOCKADDR_IN pAddrOld, LPSOCKADDR_IN pAddrNew = NULL, ULONG *pStatus = NULL);
BOOL    AfsClass_ChangeAddress        (LPIDENT lpiServer, LPSERVERSTATUS pStatusOld, LPSERVERSTATUS pStatusNew, ULONG *pStatus = NULL);


/*
 * ADMIN LISTS ________________________________________________________________
 *
 */

typedef struct
   {
   TCHAR szAdmin[ cchNAME ];
   BOOL fAdded;
   BOOL fDeleted;
   } ADMINLISTENTRY, *LPADMINLISTENTRY;

typedef struct
   {
   LPIDENT lpiServer;
   size_t cEntries;
   ADMINLISTENTRY *aEntries;
   LONG cRef;
   } ADMINLIST, *LPADMINLIST;

LPADMINLIST AfsClass_AdminList_Load   (LPIDENT lpiServer, ULONG *pStatus = NULL);
LPADMINLIST AfsClass_AdminList_Copy   (LPADMINLIST lpList);
BOOL    AfsClass_AdminList_Save       (LPADMINLIST lpList, ULONG *pStatus = NULL);
void    AfsClass_AdminList_Free       (LPADMINLIST lpList);
size_t  AfsClass_AdminList_AddEntry   (LPADMINLIST lpList, LPTSTR pszAdmin);
BOOL    AfsClass_AdminList_DelEntry   (LPADMINLIST lpList, size_t iIndex);
        // note: _addentry and _delentry are safe to call on the main thread;
        //       they don't commit their changes until _save is called.

/*
 * DATABASE-HOST LISTS ________________________________________________________
 *
 */

typedef struct
   {
   TCHAR szHost[ cchNAME ];
   BOOL fAdded;
   BOOL fDeleted;
   } HOSTLISTENTRY, *LPHOSTLISTENTRY;

typedef struct
   {
   LPIDENT lpiServer;
   size_t cEntries;
   HOSTLISTENTRY *aEntries;
   LONG cRef;
   } HOSTLIST, *LPHOSTLIST;

LPHOSTLIST AfsClass_HostList_Load     (LPIDENT lpiServer, ULONG *pStatus = NULL);
LPHOSTLIST AfsClass_HostList_Copy     (LPHOSTLIST lpList);
BOOL    AfsClass_HostList_Save        (LPHOSTLIST lpList, ULONG *pStatus = NULL);
void    AfsClass_HostList_Free        (LPHOSTLIST lpList);
size_t  AfsClass_HostList_AddEntry    (LPHOSTLIST lpList, LPTSTR pszHost);
BOOL    AfsClass_HostList_DelEntry    (LPHOSTLIST lpList, size_t iIndex);
        // note: _addentry and _delentry are safe to call on the main thread;
        //       they don't commit their changes until _save is called.


/*
 * SERVER KEYS ________________________________________________________________
 *
 */

typedef struct
   {
   ENCRYPTIONKEY keyData;
   ENCRYPTIONKEYINFO keyInfo;
   int keyVersion;
   } SERVERKEY, *LPSERVERKEY;

typedef struct
   {
   LPIDENT lpiServer;
   size_t cKeys;
   SERVERKEY *aKeys;
   } KEYLIST, *LPKEYLIST;

LPKEYLIST AfsClass_KeyList_Load       (LPIDENT lpiServer, ULONG *pStatus = NULL);
void    AfsClass_KeyList_Free         (LPKEYLIST lpList);
BOOL    AfsClass_AddKey               (LPIDENT lpiServer, int keyVersion, LPENCRYPTIONKEY pKey, ULONG *pStatus = NULL);
BOOL    AfsClass_AddKey               (LPIDENT lpiServer, int keyVersion, LPTSTR pszString, ULONG *pStatus = NULL);
BOOL    AfsClass_DeleteKey            (LPIDENT lpiServer, int keyVersion, ULONG *pStatus = NULL);
BOOL    AfsClass_GetRandomKey         (LPIDENT lpi, LPENCRYPTIONKEY pKey, ULONG *pStatus = NULL);


/*
 * SERVICE MANIPULATION _______________________________________________________
 *
 */

LPIDENT AfsClass_CreateService        (LPIDENT lpiServer, LPTSTR pszService, LPTSTR pszCommand, LPTSTR pszParams, LPTSTR pszNotifier, AFSSERVICETYPE type, SYSTEMTIME *pstIfCron, ULONG *pStatus = NULL);
BOOL    AfsClass_DeleteService        (LPIDENT lpiService, ULONG *pStatus = NULL);

BOOL    AfsClass_StartService         (LPIDENT lpiStart, BOOL fTemporary, ULONG *pStatus = NULL);
BOOL    AfsClass_StopService          (LPIDENT lpiStop, BOOL fTemporary, BOOL fWait = TRUE, ULONG *pStatus = NULL);
BOOL    AfsClass_RestartService       (LPIDENT lpiRestart, ULONG *pStatus = NULL);

BOOL    AfsClass_GetRestartTimes      (LPIDENT lpiServer, BOOL *pfWeekly, LPSYSTEMTIME pstWeekly, BOOL *pfDaily, LPSYSTEMTIME pstDaily, ULONG *pStatus = NULL);
BOOL    AfsClass_SetRestartTimes      (LPIDENT lpiServer, LPSYSTEMTIME pstWeekly = NULL, LPSYSTEMTIME pstDaily = NULL, ULONG *pStatus = NULL);


/*
 * FILESET MANIPULATION _______________________________________________________
 *
 */

LPIDENT AfsClass_CreateFileset        (LPIDENT lpiAggregate, LPTSTR pszFileset, ULONG ckQuota, ULONG *pStatus = NULL);
BOOL    AfsClass_DeleteFileset        (LPIDENT lpiFileset, BOOL fVLDB = TRUE, BOOL fServer = TRUE, ULONG *pStatus = NULL);
BOOL    AfsClass_MoveFileset          (LPIDENT lpiFileset, LPIDENT lpiAggregateTarget, ULONG *pStatus = NULL);
BOOL    AfsClass_MoveReplica          (LPIDENT lpiReplica, LPIDENT lpiAggregateTarget, ULONG *pStatus = NULL);
BOOL    AfsClass_SetFilesetQuota      (LPIDENT lpiFileset, size_t ckQuotaNew, ULONG *pStatus = NULL);
BOOL    AfsClass_RenameFileset        (LPIDENT lpiFileset, LPTSTR pszNewName, ULONG *pStatus = NULL);

BOOL    AfsClass_LockFileset          (LPIDENT lpiFileset, ULONG *pStatus = NULL);
BOOL    AfsClass_UnlockFileset        (LPIDENT lpiFileset, ULONG *pStatus = NULL);
BOOL    AfsClass_UnlockAllFilesets    (LPIDENT lpi, ULONG *pStatus = NULL);

LPIDENT AfsClass_CreateReplica        (LPIDENT lpiRW, LPIDENT lpiAggregate, ULONG *pStatus = NULL);
BOOL    AfsClass_DeleteReplica        (LPIDENT lpiReplica, ULONG *pStatus = NULL);

BOOL    AfsClass_DeleteClone          (LPIDENT lpiClone, ULONG *pStatus = NULL);

BOOL    AfsClass_ReleaseFileset       (LPIDENT lpiFilesetRW, BOOL fForce = FALSE, ULONG *pStatus = NULL);

BOOL    AfsClass_DumpFileset          (LPIDENT lpiFileset, LPTSTR pszFilename, LPSYSTEMTIME pstDate = NULL, ULONG *pStatus = NULL);
BOOL    AfsClass_RestoreFileset       (LPIDENT lpiFilesetOrAggregate, LPTSTR pszFileset, LPTSTR pszFilename, BOOL fIncremental, ULONG *pStatus = NULL);

BOOL    AfsClass_Clone                (LPIDENT lpiRW, ULONG *pStatus = NULL);
BOOL    AfsClass_CloneMultiple        (LPIDENT lpiSvrAggOrCell, LPTSTR pszPrefixOrNull, BOOL fExclusionaryPrefix, ULONG *pStatus = NULL);


/*
 * USER/GROUP MANIPULATION ____________________________________________________
 *
 */

typedef struct
   {
   DWORD dwMask;
   BOOL fAdmin;
   BOOL fGrantTickets;
   BOOL fCanEncrypt;
   BOOL fCanChangePassword;
   BOOL fCanReusePasswords;
   SYSTEMTIME timeAccountExpires;
   LONG cdayPwExpires;
   LONG csecTicketLifetime;
   LONG nFailureAttempts;
   LONG csecFailedLoginLockTime;
   LONG cGroupCreationQuota;
   ACCOUNTACCESS aaListStatus;
   ACCOUNTACCESS aaGroupsOwned;
   ACCOUNTACCESS aaMembership;
   } USERPROPERTIES, *LPUSERPROPERTIES;

#define MASK_USERPROP_fAdmin                   0x00000001
#define MASK_USERPROP_fGrantTickets            0x00000002
#define MASK_USERPROP_fCanEncrypt              0x00000004
#define MASK_USERPROP_fCanChangePassword       0x00000008
#define MASK_USERPROP_fCanReusePasswords       0x00000010
#define MASK_USERPROP_timeAccountExpires       0x00000020
#define MASK_USERPROP_cdayPwExpires            0x00000040
#define MASK_USERPROP_csecTicketLifetime       0x00000080
#define MASK_USERPROP_nFailureAttempts         0x00000100
#define MASK_USERPROP_csecFailedLoginLockTime  0x00000200
#define MASK_USERPROP_cGroupCreationQuota      0x00000400
#define MASK_USERPROP_aaListStatus             0x00000800
#define MASK_USERPROP_aaGroupsOwned            0x00001000
#define MASK_USERPROP_aaMembership             0x00002000

LPIDENT AfsClass_CreateUser           (LPIDENT lpiCell, LPTSTR pszUserName, LPTSTR pszInstance, LPTSTR pszPassword, int idUser = 0, BOOL fCreateKAS = TRUE, BOOL fCreatePTS = TRUE, ULONG *pStatus = NULL);
BOOL    AfsClass_SetUserProperties    (LPIDENT lpiUser, LPUSERPROPERTIES pProperties, ULONG *pStatus = NULL);
BOOL    AfsClass_SetUserPassword      (LPIDENT lpiUser, int keyVersion, LPTSTR pszPassword, ULONG *pStatus = NULL);
BOOL    AfsClass_SetUserPassword      (LPIDENT lpiUser, int keyVersion, LPENCRYPTIONKEY pKey, ULONG *pStatus = NULL);
BOOL    AfsClass_DeleteUser           (LPIDENT lpiUser, BOOL fDeleteKAS, BOOL fDeletePTS, ULONG *pStatus = NULL);
BOOL    AfsClass_UnlockUser           (LPIDENT lpiUser, ULONG *pStatus = NULL);


typedef struct
   {
   DWORD dwMask;
   TCHAR szOwner[ cchNAME ];
   ACCOUNTACCESS aaListStatus;
   ACCOUNTACCESS aaListGroupsOwned;
   ACCOUNTACCESS aaListMembers;
   ACCOUNTACCESS aaAddMember;
   ACCOUNTACCESS aaDeleteMember;
   } GROUPPROPERTIES, *LPGROUPPROPERTIES;

#define MASK_GROUPPROP_szOwner                 0x00000001
#define MASK_GROUPPROP_aaListStatus            0x00000002
#define MASK_GROUPPROP_aaListGroupsOwned       0x00000004
#define MASK_GROUPPROP_aaListMembers           0x00000008
#define MASK_GROUPPROP_aaAddMember             0x00000010
#define MASK_GROUPPROP_aaDeleteMember          0x00000012

LPIDENT AfsClass_CreateGroup          (LPIDENT lpiCell, LPTSTR pszGroupName, LPIDENT lpiOwner, int idGroup, ULONG *pStatus = NULL);
BOOL    AfsClass_SetGroupProperties   (LPIDENT lpiGroup, LPGROUPPROPERTIES pProperties, ULONG *pStatus = NULL);
BOOL    AfsClass_RenameGroup          (LPIDENT lpiGroup, LPTSTR pszNewName, ULONG *pStatus = NULL);
BOOL    AfsClass_DeleteGroup          (LPIDENT lpiGroup, ULONG *pStatus = NULL);
BOOL    AfsClass_AddUserToGroup       (LPIDENT lpiGroup, LPIDENT lpiUser, ULONG *pStatus = NULL);
BOOL    AfsClass_RemoveUserFromGroup  (LPIDENT lpiGroup, LPIDENT lpiUser, ULONG *pStatus = NULL);


typedef struct
   {
   int idUserMax;
   int idGroupMax;
   } PTSPROPERTIES, *LPPTSPROPERTIES;

BOOL    AfsClass_GetPtsProperties     (LPIDENT lpiCell, LPPTSPROPERTIES pProperties, ULONG *pStatus = NULL);
BOOL    AfsClass_SetPtsProperties     (LPIDENT lpiCell, LPPTSPROPERTIES pProperties, ULONG *pStatus = NULL);


#endif // AFSCLASS_FUNCTION_H

