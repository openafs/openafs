/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TASK_H
#define TASK_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

typedef struct
   {
   HWND hParent;
   POINT ptScreen;
   LPIDENT lpi;
   } MENUTASK, *LPMENUTASK;

typedef struct
   {
   HWND hCombo;
   LPIDENT lpiSelect;
   } SVR_ENUM_TO_COMBOBOX_PACKET, *LPSVR_ENUM_TO_COMBOBOX_PACKET;

typedef struct
   {
   HWND hList;
   LPIDENT lpiServer;
   LPIDENT lpiSelect;
   LPVIEWINFO lpvi;
   } AGG_ENUM_TO_LISTVIEW_PACKET, *LPAGG_ENUM_TO_LISTVIEW_PACKET;

typedef struct
   {
   HWND hCombo;
   LPIDENT lpiServer;
   LPIDENT lpiSelect;
   } AGG_ENUM_TO_COMBOBOX_PACKET, *LPAGG_ENUM_TO_COMBOBOX_PACKET;

typedef struct
   {
   HWND hCombo;
   LPIDENT lpiServer;
   LPIDENT lpiAggregate;
   LPIDENT lpiSelect;
   } SET_ENUM_TO_COMBOBOX_PACKET, *LPSET_ENUM_TO_COMBOBOX_PACKET;

typedef struct
   {
   TCHAR szFileset[ cchRESOURCE ];
   } SET_LOOKUP_PACKET, *LPSET_LOOKUP_PACKET;

typedef struct
   {
   TCHAR szCell[ cchNAME ];
   LPSUBSET sub;
   BOOL fCloseAppOnFail;
   PVOID hCreds;
   } OPENCELL_PACKET, *LPOPENCELL_PACKET;

typedef struct
   {
   HWND hList;
   LPSUBSET sub;
   } SUBSET_TO_LIST_PACKET, *LPSUBSET_TO_LIST_PACKET;

typedef enum
   {
#ifdef DEBUG
   taskEXPORTCELL,	// lp = (LPTSTR)pszFileName
#endif
   taskOPENCELL,	// lp = new OPENCELL_PACKET
   taskOPENEDCELL,	// lp = (LPIDENT)lpiCellOpened
   taskCLOSEDCELL,	// lp = (LPIDENT)lpiCellClosed
   taskREFRESH,	// lp = (LPIDENT)lpiToRefresh
   taskREFRESH_CREDS,	// lp = (LPIDENT)lpiCellToRefresh
   taskSUBSET_TO_LIST,	// lp = new SUBSET_TO_LIST_PACKET
   taskAPPLY_SUBSET,	// lp = (LPSUBSET)subNew
   taskSVR_PROP_INIT,	// lp = (LPIDENT)lpiServer
   taskSVR_SCOUT_INIT,	// lp = (LPIDENT)lpiServer
   taskSVR_SCOUT_APPLY,	// lp = new SVR_SCOUT_APPLY_PACKET
   taskSVR_ENUM_TO_COMBOBOX,	// lp = new SVR_ENUM_TO_COMBOBOX_PACKET
   taskSVR_GETWINDOWPOS,	// lp = (LPIDENT)lpiServer
   taskSVR_SETWINDOWPOS,	// lp = new SVR_SETWINDOWPOS_PARAMS
   taskSVR_SYNCVLDB,	// lp = new SVR_SYNCVLDB_PARAMS
   taskSVR_SALVAGE,	// lp = new SVR_SALVAGE_PARAMS
   taskSVR_INSTALL,	// lp = new SVR_INSTALL_PARAMS
   taskSVR_UNINSTALL,	// lp = new SVR_UNINSTALL_PARAMS
   taskSVR_PRUNE,	// lp = new SVR_PRUNE_PARAMS
   taskSVR_GETDATES,	// lp = new SVR_GETDATES_PARAMS
   taskSVR_EXECUTE,	// lp = new SVR_EXECUTE_PARAMS
   taskSVR_SETAUTH,	// lp = new SVR_SETAUTH_PARAMS
   taskSVR_ADMLIST_OPEN,	// lp = (LPIDENT)lpiServer
   taskSVR_ADMLIST_SAVE,	// lp = AfsClass_AdminList_Copy(lpList)
   taskSVR_KEYLIST_OPEN,	// lp = (LPIDENT)lpiServer
   taskSVR_KEY_CREATE,	// lp = LPKEY_CREATE_PARAMS
   taskSVR_KEY_DELETE,	// lp = LPKEY_DELETE_PARAMS
   taskSVR_GETRANDOMKEY,	// lp = (LPIDENT)lpiServer
   taskSVR_HOSTLIST_OPEN,	// lp = (LPIDENT)lpiServer
   taskSVR_HOSTLIST_SAVE,	// lp = AfsClass_HostList_Copy(lpList)
   taskSVR_MONITOR_ONOFF,	// lp = (LPIDENT)lpiServer
   taskSVR_CHANGEADDR,	// lp = new SVR_CHANGEADDR_PARAMS
   taskSVC_MENU,	// lp = new MENUTASK
   taskSVC_PROP_INIT,	// lp = (LPIDENT)lpiService
   taskSVC_PROP_APPLY,	// lp = new SVC_PROP_APPLY_PACKET
   taskSVC_START,	// lp = new SVC_START_PARAMS
   taskSVC_STOP,	// lp = new SVC_STOP_PARAMS
   taskSVC_RESTART,	// lp = (LPIDENT)lpiService
   taskSVC_FINDLOG,	// lp = (LPIDENT)lpiService
   taskSVC_VIEWLOG,	// lp = new LPSVC_VIEWLOG_PACKET
   taskSVC_CREATE,	// lp = new LPSVC_CREATE_PARAMS
   taskSVC_DELETE,	// lp = (LPIDENT)lpiService
   taskSVC_GETRESTARTTIMES,	// lp = new SVC_RESTARTTIMES_PARAMS
   taskSVC_SETRESTARTTIMES,	// lp = new SVC_RESTARTTIMES_PARAMS
   taskAGG_PROP_INIT,	// lp = (LPIDENT)lpiAggregate
   taskAGG_PROP_APPLY,	// lp = new AGG_PROP_APPLY_PACKET
   taskAGG_FIND_QUOTA_LIMITS,	// lp = (LPIDENT)lpiAggregate
   taskAGG_ENUM_TO_LISTVIEW,	// lp = new AGG_ENUM_TO_LISTVIEW_PACKET
   taskAGG_ENUM_TO_COMBOBOX,	// lp = new AGG_ENUM_TO_COMBOBOX_PACKET
   taskAGG_FIND_GHOST,	// lp = (LPIDENT)lpiAggregate
   taskSET_ENUM_TO_COMBOBOX,	// lp = new SET_ENUM_TO_COMBOBOX_PACKET
   taskSET_FIND_GHOST,	// lp = (LPIDENT)lpiFileset
   taskSET_CREATE,	// lp = new SET_CREATE_PARAMS
   taskSET_DELETE,	// lp = new SET_DELETE_PARAMS
   taskSET_MOVE,	// lp = new SET_MOVE_PARAMS
   taskSET_MOVETO_INIT,	// lp = (LPIDENT)lpiFileset
   taskSET_PROP_INIT,	// lp = (LPIDENT)lpiFileset
   taskSET_PROP_APPLY,	// lp = new SET_PROP_APPLY_PARAMS
   taskSET_SETQUOTA_INIT,	// lp = (LPIDENT)lpiFileset
   taskSET_SETQUOTA_APPLY,	// lp = new SET_SETQUOTA_APPLY_PARAMS
   taskSET_REPPROP_INIT,	// lp = (LPIDENT)lpiFileset
   taskSET_SELECT,	// lp = (LPIDENT)lpiFileset
   taskSET_BEGINDRAG,	// lp = (LPIDENT)lpiFileset
   taskSET_DRAGMENU,	// lp = (LPMENUTASK)new MENUTASK
   taskSET_MENU,	// lp = (LPMENUTASK)new MENUTASK
   taskSET_LOCK,	// lp = (LPIDENT)lpiFileset
   taskSET_UNLOCK,	// lp = (LPIDENT)lpi (set,agg,svr,cell)
   taskSET_CREATEREP,	// lp = new SET_CREATEREP_PARAMS
   taskSET_RENAME_INIT,	// lp = new SET_RENAME_INIT_PARAMS
   taskSET_RENAME_APPLY,	// lp = new SET_RENAME_APPLY_PARAMS
   taskSET_RELEASE,	// lp = new SET_RELEASE_PARAMS
   taskSET_CLONE,	// lp = (LPIDENT)lpiRW
   taskSET_CLONESYS,	// lp = new SET_CLONESYS_PARAMS
   taskSET_DUMP,	// lp = new SET_DUMP_PARAMS
   taskSET_RESTORE,	// lp = new SET_RESTORE_PARAMS
   taskSET_LOOKUP,	// lp = new SET_LOOKUP_PACKET
   taskEXPIRED_CREDS,	// lp = unused
   } TASK;

typedef struct
   {
   LPIDENT lpiCell;	// what was found out?
   LPTSTR pszText1;
   LPTSTR pszText2;	// these parameters are filled in
   LPTSTR pszText3;	// selectively, depending upon the
   ALERT alert;
   LPIDENT lpi;
   SERVERSTATUS ss;
   SERVICESTATUS cs;
   AGGREGATESTATUS as;
   FILESETSTATUS fs;
   LPSERVER_PREF lpsp;
   LPSERVICE_PREF lpcp;
   LPAGGREGATE_PREF lpap;
   LPFILESET_PREF lpfp;
   size_t nAggr;
   MENUTASK mt;
   size_t ckMin;
   size_t ckMax;
   size_t ckCapacity;
   size_t ckAllocation;
   short wGhost;
   RECT rWindow;
   size_t nFilesets;
   LPADMINLIST lpAdmList;
   LPHOSTLIST lpHostList;
   BOOL fHasReplicas;
   LPKEYLIST lpKeyList;
   ENCRYPTIONKEY key;
   } TASKPACKETDATA, *LPTASKPACKETDATA;

#define TASKDATA(_ptp) ((LPTASKPACKETDATA)(ptp->pReturn))


LPTASKPACKET CreateTaskPacket (int idTask, HWND hReply, PVOID lpUser);
void FreeTaskPacket (LPTASKPACKET ptp);
void PerformTask (LPTASKPACKET ptp);


#endif

