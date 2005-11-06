/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "task.h"
#include "display.h"
#include "action.h"
#include "svr_general.h"
#include "svr_window.h"
#include "svr_prop.h"
#include "svr_syncvldb.h"
#include "svr_salvage.h"
#include "svr_install.h"
#include "svr_uninstall.h"
#include "svr_prune.h"
#include "svr_getdates.h"
#include "svr_execute.h"
#include "svr_security.h"
#include "svr_address.h"
#include "svc_prop.h"
#include "svc_create.h"
#include "svc_general.h"
#include "svc_startstop.h"
#include "svc_viewlog.h"
#include "agg_general.h"
#include "agg_prop.h"
#include "set_create.h"
#include "set_delete.h"
#include "set_move.h"
#include "set_prop.h"
#include "set_general.h"
#include "set_quota.h"
#include "set_repprop.h"
#include "set_createrep.h"
#include "set_rename.h"
#include "set_release.h"
#include "set_clone.h"
#include "set_dump.h"
#include "set_restore.h"
#include "subset.h"
#include "messages.h"
#include "creds.h"


#ifdef DEBUG
#include <exportcl.h>
#endif


// As an additional debugging measure, you may want to enable the
// definition below--doing so causes the tool to ensure that
// AFSCLASS's critical section (accessed via AfsClass_Enter()/Leave()) is
// not leaking during tasks: for instance, if AfsClass_MoveFileset()
// calls AfsClass_Enter() three times but Leave() only twice, then an
// assertion will trigger in Task_Perform() (at which point it is
// not too late to easily determine which task leaked the critsec).
// Unfortunately, this has a side-effect: enabling the definition
// below prevents the AFSCLASS library from entirely exiting the
// critical section during its work--which means that only one
// AfsClass_* function can ever be run at once.  So, enable this
// definition if you see a lockup--and disable it when you're done.
//
#ifdef DEBUG
// #define DEBUG_CRIT
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

#ifdef DEBUG
void Task_ExportCell (LPTASKPACKET ptp);
#endif
void Task_OpenCell (LPTASKPACKET ptp);
void Task_OpenedCell (LPTASKPACKET ptp);
void Task_ClosedCell (LPTASKPACKET ptp);
void Task_Refresh (LPTASKPACKET ptp, BOOL fNewCreds);
void Task_Subset_To_List (LPTASKPACKET ptp);
void Task_Apply_Subset (LPTASKPACKET ptp);
void Task_Svr_Prop_Init (LPTASKPACKET ptp);
void Task_Svr_Scout_Init (LPTASKPACKET ptp);
void Task_Svr_Scout_Apply (LPTASKPACKET ptp);
void Task_Svr_Enum_To_ComboBox (LPTASKPACKET ptp);
void Task_Svr_GetWindowPos (LPTASKPACKET ptp);
void Task_Svr_SetWindowPos (LPTASKPACKET ptp);
void Task_Svr_SyncVLDB (LPTASKPACKET ptp);
void Task_Svr_Salvage (LPTASKPACKET ptp);
void Task_Svr_Install (LPTASKPACKET ptp);
void Task_Svr_Uninstall (LPTASKPACKET ptp);
void Task_Svr_Prune (LPTASKPACKET ptp);
void Task_Svr_GetDates (LPTASKPACKET ptp);
void Task_Svr_Execute (LPTASKPACKET ptp);
void Task_Svr_SetAuth (LPTASKPACKET ptp);
void Task_Svr_AdmList_Open (LPTASKPACKET ptp);
void Task_Svr_AdmList_Save (LPTASKPACKET ptp);
void Task_Svr_KeyList_Open (LPTASKPACKET ptp);
void Task_Svr_Key_Create (LPTASKPACKET ptp);
void Task_Svr_Key_Delete (LPTASKPACKET ptp);
void Task_Svr_HostList_Open (LPTASKPACKET ptp);
void Task_Svr_HostList_Save (LPTASKPACKET ptp);
void Task_Svr_GetRandomKey (LPTASKPACKET ptp);
void Task_Svr_Monitor_OnOff (LPTASKPACKET ptp);
void Task_Svr_ChangeAddr (LPTASKPACKET ptp);
void Task_Svc_Menu (LPTASKPACKET ptp);
void Task_Svc_Prop_Init (LPTASKPACKET ptp);
void Task_Svc_Prop_Apply (LPTASKPACKET ptp);
void Task_Svc_Start (LPTASKPACKET ptp);
void Task_Svc_Stop (LPTASKPACKET ptp);
void Task_Svc_Restart (LPTASKPACKET ptp);
void Task_Svc_FindLog (LPTASKPACKET ptp);
void Task_Svc_ViewLog (LPTASKPACKET ptp);
void Task_Svc_Create (LPTASKPACKET ptp);
void Task_Svc_Delete (LPTASKPACKET ptp);
void Task_Svc_GetRestartTimes (LPTASKPACKET ptp);
void Task_Svc_SetRestartTimes (LPTASKPACKET ptp);
void Task_Agg_Prop_Init (LPTASKPACKET ptp);
void Task_Agg_Prop_Apply (LPTASKPACKET ptp);
void Task_Agg_Find_Quota_Limits (LPTASKPACKET ptp);
void Task_Agg_Enum_To_ListView (LPTASKPACKET ptp);
void Task_Agg_Enum_To_ComboBox (LPTASKPACKET ptp);
void Task_Agg_Find_Ghost (LPTASKPACKET ptp);
void Task_Set_Enum_To_ComboBox (LPTASKPACKET ptp);
void Task_Set_Find_Ghost (LPTASKPACKET ptp);
void Task_Set_Create (LPTASKPACKET ptp);
void Task_Set_Delete (LPTASKPACKET ptp);
void Task_Set_Move (LPTASKPACKET ptp);
void Task_Set_MoveTo_Init (LPTASKPACKET ptp);
void Task_Set_Prop_Init (LPTASKPACKET ptp);
void Task_Set_Prop_Apply (LPTASKPACKET ptp);
void Task_Set_SetQuota_Init (LPTASKPACKET ptp);
void Task_Set_SetQuota_Apply (LPTASKPACKET ptp);
void Task_Set_RepProp_Init (LPTASKPACKET ptp);
void Task_Set_Select (LPTASKPACKET ptp);
void Task_Set_BeginDrag (LPTASKPACKET ptp);
void Task_Set_DragMenu (LPTASKPACKET ptp);
void Task_Set_Menu (LPTASKPACKET ptp);
void Task_Set_Lock (LPTASKPACKET ptp);
void Task_Set_Unlock (LPTASKPACKET ptp);
void Task_Set_CreateRep (LPTASKPACKET ptp);
void Task_Set_Rename_Init (LPTASKPACKET ptp);
void Task_Set_Rename_Apply (LPTASKPACKET ptp);
void Task_Set_Release (LPTASKPACKET ptp);
void Task_Set_Clone (LPTASKPACKET ptp);
void Task_Set_Clonesys (LPTASKPACKET ptp);
void Task_Set_Dump (LPTASKPACKET ptp);
void Task_Set_Restore (LPTASKPACKET ptp);
void Task_Set_Lookup (LPTASKPACKET ptp);
void Task_Expired_Creds (LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

LPTASKPACKET CreateTaskPacket (int idTask, HWND hReply, PVOID lpUser)
{
   LPTASKPACKET ptp;

   if ((ptp = New (TASKPACKET)) != NULL)
      {
      memset (ptp, 0x00, sizeof(TASKPACKET));

      ptp->idTask = idTask;
      ptp->hReply = hReply;
      ptp->lpUser = lpUser;

      ptp->rc = TRUE;
      ptp->status = 0;

      if ((ptp->pReturn = New (TASKPACKETDATA)) != NULL)
         {
         memset (ptp->pReturn, 0x00, sizeof(TASKPACKETDATA));
         }
      }

   return ptp;
}


void FreeTaskPacket (LPTASKPACKET ptp)
{
   if (ptp)
      {
      if (ptp->pReturn)
         {
         if (TASKDATA(ptp)->pszText1)
            FreeString (TASKDATA(ptp)->pszText1);
         if (TASKDATA(ptp)->pszText2)
            FreeString (TASKDATA(ptp)->pszText2);
         if (TASKDATA(ptp)->pszText3)
            FreeString (TASKDATA(ptp)->pszText3);
         Delete (ptp->pReturn);
         }

      Delete (ptp);
      }
}


void PerformTask (LPTASKPACKET ptp)
{
#ifdef DEBUG_CRIT
   AfsClass_Enter();
   ASSERT( AfsClass_GetEnterCount() == 1 );
#endif

   switch (ptp->idTask)
      {
#ifdef DEBUG
      case taskEXPORTCELL:
         Task_ExportCell (ptp);
         break;
#endif

      case taskOPENCELL:
         Task_OpenCell (ptp);
         break;

      case taskOPENEDCELL:
         Task_OpenedCell (ptp);
         break;

      case taskCLOSEDCELL:
         Task_ClosedCell (ptp);
         break;

      case taskREFRESH:
         Task_Refresh (ptp, FALSE);
         break;

      case taskREFRESH_CREDS:
         Task_Refresh (ptp, TRUE);
         break;

      case taskSUBSET_TO_LIST:
         Task_Subset_To_List (ptp);
         break;

      case taskAPPLY_SUBSET:
         Task_Apply_Subset (ptp);
         break;

      case taskSVR_PROP_INIT:
         Task_Svr_Prop_Init (ptp);
         break;

      case taskSVR_SCOUT_INIT:
         Task_Svr_Scout_Init (ptp);
         break;

      case taskSVR_SCOUT_APPLY:
         Task_Svr_Scout_Apply (ptp);
         break;

      case taskSVR_ENUM_TO_COMBOBOX:
         Task_Svr_Enum_To_ComboBox (ptp);
         break;

      case taskSVR_GETWINDOWPOS:
         Task_Svr_GetWindowPos (ptp);
         break;

      case taskSVR_SETWINDOWPOS:
         Task_Svr_SetWindowPos (ptp);
         break;

      case taskSVR_SYNCVLDB:
         Task_Svr_SyncVLDB (ptp);
         break;

      case taskSVR_SALVAGE:
         Task_Svr_Salvage (ptp);
         break;

      case taskSVR_INSTALL:
         Task_Svr_Install (ptp);
         break;

      case taskSVR_UNINSTALL:
         Task_Svr_Uninstall (ptp);
         break;

      case taskSVR_PRUNE:
         Task_Svr_Prune (ptp);
         break;

      case taskSVR_GETDATES:
         Task_Svr_GetDates (ptp);
         break;

      case taskSVR_EXECUTE:
         Task_Svr_Execute (ptp);
         break;

      case taskSVR_SETAUTH:
         Task_Svr_SetAuth (ptp);
         break;

      case taskSVR_ADMLIST_OPEN:
         Task_Svr_AdmList_Open (ptp);
         break;

      case taskSVR_ADMLIST_SAVE:
         Task_Svr_AdmList_Save (ptp);
         break;

      case taskSVR_KEYLIST_OPEN:
         Task_Svr_KeyList_Open (ptp);
         break;

      case taskSVR_KEY_CREATE:
         Task_Svr_Key_Create (ptp);
         break;

      case taskSVR_KEY_DELETE:
         Task_Svr_Key_Delete (ptp);
         break;

      case taskSVR_GETRANDOMKEY:
         Task_Svr_GetRandomKey (ptp);
         break;

      case taskSVR_HOSTLIST_OPEN:
         Task_Svr_HostList_Open (ptp);
         break;

      case taskSVR_HOSTLIST_SAVE:
         Task_Svr_HostList_Save (ptp);
         break;

      case taskSVR_MONITOR_ONOFF:
         Task_Svr_Monitor_OnOff (ptp);
         break;

      case taskSVR_CHANGEADDR:
         Task_Svr_ChangeAddr (ptp);
         break;

      case taskSVC_MENU:
         Task_Svc_Menu (ptp);
         break;

      case taskSVC_PROP_INIT:
         Task_Svc_Prop_Init (ptp);
         break;

      case taskSVC_PROP_APPLY:
         Task_Svc_Prop_Apply (ptp);
         break;

      case taskSVC_START:
         Task_Svc_Start (ptp);
         break;

      case taskSVC_STOP:
         Task_Svc_Stop (ptp);
         break;

      case taskSVC_RESTART:
         Task_Svc_Restart (ptp);
         break;

      case taskSVC_FINDLOG:
         Task_Svc_FindLog (ptp);
         break;

      case taskSVC_VIEWLOG:
         Task_Svc_ViewLog (ptp);
         break;

      case taskSVC_CREATE:
         Task_Svc_Create (ptp);
         break;

      case taskSVC_DELETE:
         Task_Svc_Delete (ptp);
         break;

      case taskSVC_GETRESTARTTIMES:
         Task_Svc_GetRestartTimes (ptp);
         break;

      case taskSVC_SETRESTARTTIMES:
         Task_Svc_SetRestartTimes (ptp);
         break;

      case taskAGG_PROP_INIT:
         Task_Agg_Prop_Init (ptp);
         break;

      case taskAGG_PROP_APPLY:
         Task_Agg_Prop_Apply (ptp);
         break;

      case taskAGG_FIND_QUOTA_LIMITS:
         Task_Agg_Find_Quota_Limits (ptp);
         break;

      case taskAGG_ENUM_TO_LISTVIEW:
         Task_Agg_Enum_To_ListView (ptp);
         break;

      case taskAGG_ENUM_TO_COMBOBOX:
         Task_Agg_Enum_To_ComboBox (ptp);
         break;

      case taskAGG_FIND_GHOST:
         Task_Agg_Find_Ghost (ptp);
         break;

      case taskSET_ENUM_TO_COMBOBOX:
         Task_Set_Enum_To_ComboBox (ptp);
         break;

      case taskSET_FIND_GHOST:
         Task_Set_Find_Ghost (ptp);
         break;

      case taskSET_CREATE:
         Task_Set_Create (ptp);
         break;

      case taskSET_DELETE:
         Task_Set_Delete (ptp);
         break;

      case taskSET_MOVE:
         Task_Set_Move (ptp);
         break;

      case taskSET_MOVETO_INIT:
         Task_Set_MoveTo_Init (ptp);
         break;

      case taskSET_PROP_INIT:
         Task_Set_Prop_Init (ptp);
         break;

      case taskSET_PROP_APPLY:
         Task_Set_Prop_Apply (ptp);
         break;

      case taskSET_SETQUOTA_INIT:
         Task_Set_SetQuota_Init (ptp);
         break;

      case taskSET_SETQUOTA_APPLY:
         Task_Set_SetQuota_Apply (ptp);
         break;

      case taskSET_REPPROP_INIT:
         Task_Set_RepProp_Init (ptp);
         break;

      case taskSET_SELECT:
         Task_Set_Select (ptp);
         break;

      case taskSET_BEGINDRAG:
         Task_Set_BeginDrag (ptp);
         break;

      case taskSET_DRAGMENU:
         Task_Set_DragMenu (ptp);
         break;

      case taskSET_MENU:
         Task_Set_Menu (ptp);
         break;

      case taskSET_LOCK:
         Task_Set_Lock (ptp);
         break;

      case taskSET_UNLOCK:
         Task_Set_Unlock (ptp);
         break;

      case taskSET_CREATEREP:
         Task_Set_CreateRep (ptp);
         break;

      case taskSET_RENAME_INIT:
         Task_Set_Rename_Init (ptp);
         break;

      case taskSET_RENAME_APPLY:
         Task_Set_Rename_Apply (ptp);
         break;

      case taskSET_RELEASE:
         Task_Set_Release (ptp);
         break;

      case taskSET_CLONE:
         Task_Set_Clone (ptp);
         break;

      case taskSET_CLONESYS:
         Task_Set_Clonesys (ptp);
         break;

      case taskSET_DUMP:
         Task_Set_Dump (ptp);
         break;

      case taskSET_RESTORE:
         Task_Set_Restore (ptp);
         break;

      case taskSET_LOOKUP:
         Task_Set_Lookup (ptp);
         break;

      case taskEXPIRED_CREDS:
         Task_Expired_Creds (ptp);
         break;

      default:
         ptp->rc = FALSE;
         ptp->status = ERROR_INVALID_FUNCTION;
         break;
      }

#ifdef DEBUG_CRIT
   ASSERT( AfsClass_GetEnterCount() == 1 );
   AfsClass_Leave();
#endif
}


void CoverServerList (LPIDENT lpiCell, ULONG status)
{
   TCHAR szName[ cchRESOURCE ];
   lpiCell->GetCellName (szName);

   int idsButton;

   switch (status)
      {
      case ERROR_NOT_AUTHENTICATED:
         idsButton = IDS_ALERT_BUTTON_GETCREDS;
         break;
      default:
         idsButton = IDS_ALERT_BUTTON_TRYAGAIN;
         break;
      }


   LPTSTR pszCover = FormatString (IDS_ERROR_REFRESH_CELLSERVERS, TEXT("%s%e"), szName, status);
   LPTSTR pszButton = FormatString (idsButton);

   AfsAppLib_CoverWindow (GetDlgItem (g.hMain, IDC_SERVERS), pszCover, pszButton);

   FreeString (pszButton);
   FreeString (pszCover);
}


#ifdef DEBUG
ULONG SystemTimeToUnixTime (SYSTEMTIME *pst)
{
   SYSTEMTIME st;
   if (pst == NULL)
      GetSystemTime (&st);
   else
      st = *pst;

   FILETIME ft;
   if (!SystemTimeToFileTime (&st, &ft))
      return 0;

   LARGE_INTEGER now;
   now.HighPart = (LONG)ft.dwHighDateTime;
   now.LowPart = (ULONG)ft.dwLowDateTime;

   LARGE_INTEGER offset;
   offset.HighPart = 0x019db1de;
   offset.LowPart = 0xd53e8000;

   LARGE_INTEGER result;
   result.QuadPart = (now.QuadPart - offset.QuadPart) / 10000000;
   return (ULONG)result.LowPart;
}

static size_t nExportLevels = 0;
void Task_ExportCell_Spacing (LPTSTR pszSpacing)
{
   wsprintf (pszSpacing, TEXT("%99s"), TEXT(""));
   pszSpacing[ nExportLevels *3 ] = TEXT('\0');
}

#define chLBRC TEXT('{')
#define chRBRC TEXT('}')

void Task_ExportCell_Begin (HANDLE fh, LPTSTR eck, LPTSTR pszName)
{
   TCHAR szSpacing[ 256 ];
   Task_ExportCell_Spacing (szSpacing);
   TCHAR szLine[ 256 ];
   wsprintf (szLine, TEXT("\r\n%s%s %s %c\r\n"), szSpacing, eck, pszName, chLBRC);
   DWORD cbWrote = 0;
   WriteFile (fh, szLine, lstrlen(szLine), &cbWrote, NULL);
   ++nExportLevels;
}

void Task_ExportCell_End (HANDLE fh)
{
   --nExportLevels;
   TCHAR szSpacing[ 256 ];
   Task_ExportCell_Spacing (szSpacing);
   TCHAR szLine[ 256 ];
   wsprintf (szLine, TEXT("%s%c\r\n"), szSpacing, chRBRC);
   DWORD cbWrote = 0;
   WriteFile (fh, szLine, lstrlen(szLine), &cbWrote, NULL);
}

void Task_ExportCell_Line (HANDLE fh, LPTSTR eck, LPTSTR pszRHS)
{
   TCHAR szSpacing[ 256 ];
   Task_ExportCell_Spacing (szSpacing);
   TCHAR szLine[ 256 ];
   wsprintf (szLine, TEXT("%s%s = \"%s\"\r\n"), szSpacing, eck, pszRHS);
   DWORD cbWrote = 0;
   WriteFile (fh, szLine, lstrlen(szLine), &cbWrote, NULL);
}

void Task_ExportCell_LineAddr (HANDLE fh, LPTSTR eck, SOCKADDR_IN *pAddr)
{
   LPSTR pszLine = FormatString (TEXT("%1"), TEXT("%a"), pAddr);
   Task_ExportCell_Line (fh, eck, pszLine);
   FreeString (pszLine);
}

void Task_ExportCell_LineInt (HANDLE fh, LPTSTR eck, size_t dw)
{
   TCHAR szLine[256];
   wsprintf (szLine, TEXT("%lu"), dw);
   Task_ExportCell_Line (fh, eck, szLine);
}

void Task_ExportCell_LineLarge (HANDLE fh, LPTSTR eck, LARGE_INTEGER *pldw)
{
   TCHAR szLine[256];
   wsprintf (szLine, TEXT("%lu,,%lu"), pldw->HighPart, pldw->LowPart);
   Task_ExportCell_Line (fh, eck, szLine);
}

void Task_ExportCell_LineDate (HANDLE fh, LPTSTR eck, SYSTEMTIME *pst)
{
   Task_ExportCell_LineInt (fh, eck, SystemTimeToUnixTime(pst));
}

void Task_ExportService (HANDLE fh, LPSERVICE lpService)
{
   TCHAR szName[ cchNAME ];
   lpService->GetName (szName);
   Task_ExportCell_Begin (fh, eckSERVICE, szName);

   SERVICESTATUS ss;
   if (lpService->GetStatus (&ss))
      {
      Task_ExportCell_LineInt  (fh, eckRUNNING,   (ss.state == SERVICESTATE_RUNNING));
      Task_ExportCell_LineInt  (fh, eckSTATE,     ss.state);
      Task_ExportCell_LineInt  (fh, eckNSTARTS,   ss.nStarts);
      Task_ExportCell_LineInt  (fh, eckERRLAST,   ss.dwErrLast);
      Task_ExportCell_LineInt  (fh, eckSIGLAST,   ss.dwSigLast);
      Task_ExportCell_Line     (fh, eckPARAMS,    ss.szParams);
      Task_ExportCell_Line     (fh, eckNOTIFIER,  ss.szNotifier);
      Task_ExportCell_LineDate (fh, eckSTARTTIME, &ss.timeLastStart);
      Task_ExportCell_LineDate (fh, eckSTOPTIME,  &ss.timeLastStop);
      Task_ExportCell_LineDate (fh, eckERRORTIME, &ss.timeLastFail);

      if (ss.type == SERVICETYPE_SIMPLE)
         Task_ExportCell_Line  (fh, eckTYPE,    eckSIMPLE);
      else if (ss.type == SERVICETYPE_CRON)
         Task_ExportCell_Line  (fh, eckTYPE,    eckCRON);
      else // (ss.type == SERVICETYPE_FS)
         Task_ExportCell_Line  (fh, eckTYPE,    eckFS);
      }

   Task_ExportCell_End (fh);
}

void Task_ExportFileset (HANDLE fh, LPFILESET lpFileset)
{
   TCHAR szName[ cchNAME ];
   lpFileset->GetName (szName);
   Task_ExportCell_Begin (fh, eckFILESET, szName);

   FILESETSTATUS fs;
   if (lpFileset->GetStatus (&fs))
      {
      Task_ExportCell_LineInt   (fh, eckID,          fs.id);
      Task_ExportCell_LineInt   (fh, eckID_RW,       fs.idReadWrite);
      Task_ExportCell_LineInt   (fh, eckID_BK,       fs.idClone);
      Task_ExportCell_LineInt   (fh, eckUSED,        fs.ckUsed);
      Task_ExportCell_LineInt   (fh, eckQUOTA,       fs.ckQuota);
      Task_ExportCell_LineDate  (fh, eckCREATETIME, &fs.timeCreation);
      Task_ExportCell_LineDate  (fh, eckUPDATETIME, &fs.timeLastUpdate);
      Task_ExportCell_LineDate  (fh, eckACCESSTIME, &fs.timeLastAccess);
      Task_ExportCell_LineDate  (fh, eckBACKUPTIME, &fs.timeLastBackup);
      }

   Task_ExportCell_End (fh);
}

void Task_ExportAggregate (HANDLE fh, LPAGGREGATE lpAggregate)
{
   TCHAR szName[ cchNAME ];
   lpAggregate->GetName (szName);
   Task_ExportCell_Begin (fh, eckAGGREGATE, szName);

   lpAggregate->GetDevice (szName);
   Task_ExportCell_Line (fh, eckDEVICE, szName);

   AGGREGATESTATUS as;
   if (lpAggregate->GetStatus (&as))
      {
      Task_ExportCell_LineInt  (fh, eckID,      (int)as.dwID);
      Task_ExportCell_LineInt  (fh, eckTOTAL,   (int)as.ckStorageTotal);
      Task_ExportCell_LineInt  (fh, eckFREECUR, (int)as.ckStorageFree);
      }

   HENUM hEnum;
   for (LPFILESET lpFileset = lpAggregate->FilesetFindFirst (&hEnum); lpFileset; lpFileset = lpAggregate->FilesetFindNext (&hEnum))
      {
      Task_ExportFileset (fh, lpFileset);
      lpFileset->Close();
      }

   Task_ExportCell_End (fh);
}

void Task_ExportServer (HANDLE fh, LPSERVER lpServer)
{
   TCHAR szName[ cchNAME ];
   lpServer->GetLongName (szName);
   Task_ExportCell_Begin (fh, eckSERVER, szName);

   // First, properties of the server
   //
   SERVERSTATUS ss;
   if (lpServer->GetStatus (&ss))
      {
      for (size_t iAddr = 0; iAddr < ss.nAddresses; ++iAddr)
         Task_ExportCell_LineAddr (fh, eckADDRESS, &ss.aAddresses[ iAddr ]);
      }

   // Then, services on the server
   //
   HENUM hEnum;
   for (LPSERVICE lpService = lpServer->ServiceFindFirst (&hEnum); lpService; lpService = lpServer->ServiceFindNext (&hEnum))
      {
      Task_ExportService (fh, lpService);
      lpService->Close();
      }

   // Then, aggregates and filesets on the server
   //
   for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&hEnum); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&hEnum))
      {
      Task_ExportAggregate (fh, lpAggregate);
      lpAggregate->Close();
      }

   Task_ExportCell_End (fh);
}

void Task_ExportCell (LPTASKPACKET ptp)
{
   LPTSTR pszFilename = (LPTSTR)(ptp->lpUser);

   HANDLE fh;
   fh = CreateFile (pszFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_ARCHIVE, NULL);
   if (fh != INVALID_HANDLE_VALUE)
      {
      TCHAR szName[ cchNAME ];
      g.lpiCell->GetCellName (szName);
      Task_ExportCell_Begin (fh, eckCELL, szName);

      LPCELL lpCell;
      if ((lpCell = g.lpiCell->OpenCell()) != NULL)
         {
         HENUM hEnum;
         for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
            {
            Task_ExportServer (fh, lpServer);
            lpServer->Close();
            }

         lpCell->Close();
         }

      Task_ExportCell_End (fh);
      CloseHandle (fh);
      }

   Delete (pszFilename);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}
#endif


void Task_OpenCell (LPTASKPACKET ptp)
{
   LPOPENCELL_PACKET lpp = (LPOPENCELL_PACKET)(ptp->lpUser);

   LPSUBSET subOld = g.sub;
   g.sub = lpp->sub;

   if ((TASKDATA(ptp)->lpiCell = CELL::OpenCell (lpp->szCell, (PVOID)lpp->hCreds, &ptp->status)) == NULL)
      {
      ptp->rc = FALSE;

      g.sub = subOld;
      if (lpp->sub)
         Subsets_FreeSubset (lpp->sub);
      }
   else if (subOld)
      {
      Subsets_FreeSubset (subOld);
      }

   if (ptp->rc)
      {
      PostMessage (g.hMain, WM_SHOW_YOURSELF, 0, 1);

      if (gr.fActions)
         PostMessage (g.hMain, WM_OPEN_ACTIONS, 0, 0);
      }
   else if (lpp->fCloseAppOnFail)
      {
      FatalErrorDialog (ptp->status, IDS_ERROR_CANT_OPEN_CELL, TEXT("%s"), lpp->szCell);
      }

   Delete (lpp);
   ptp->lpUser = 0; // we freed this; don't let the caller use it again
}


void Task_OpenedCell (LPTASKPACKET ptp)
{
   LPIDENT lpiCell = (LPIDENT)(ptp->lpUser);

   if (lpiCell && lpiCell->fIsCell())
      {
      AfsClass_Enter();

      DontNotifyMeEver (g.hMain);
      NotifyMe (WHEN_CELL_OPENED, NULL, g.hMain, 0);
      NotifyMe (WHEN_OBJECT_CHANGES, lpiCell, g.hMain, 0);
      NotifyMe (WHEN_SVRS_CHANGE, lpiCell, g.hMain, 0);

      if (g.lpiCell != NULL)
         {
         CELL::CloseCell (g.lpiCell);
         }

      g.lpiCell = lpiCell;
      UpdateDisplay_Cell (TRUE);

      TCHAR szName[ cchRESOURCE ];
      lpiCell->GetCellName (szName);

      LPTSTR pszCover = FormatString (IDS_SEARCHING_FOR_SERVERS, TEXT("%s"), szName);
      AfsAppLib_CoverWindow (GetDlgItem (g.hMain, IDC_SERVERS), pszCover);
      FreeString (pszCover);

      LPCELL lpCell;
      if ((lpCell = g.lpiCell->OpenCell (&ptp->status)) == NULL)
         {
         CoverServerList (g.lpiCell, ptp->status);
         }
      else
         {
         lpCell->Invalidate();
         if ((ptp->rc = lpCell->RefreshAll (&ptp->status)) != TRUE)
            {
            CoverServerList (g.lpiCell, ptp->status);
            }
         lpCell->Close();
         }

      PostMessage (g.hMain, WM_OPEN_SERVERS, 0, 0);

      Alert_StartScout();
      AfsClass_Leave();
      }
}


void Task_ClosedCell (LPTASKPACKET ptp)
{
   LPIDENT lpiCell = (LPIDENT)(ptp->lpUser);

   if (lpiCell && lpiCell->fIsCell())
      {
      AfsClass_Enter();

      if (g.lpiCell == lpiCell)
         {
         LPCELL lpCellTest;
         if ((lpCellTest = g.lpiCell->OpenCell()) != NULL)
            {
            // the user must have opened a cell, then opened it again.
            // this is a bogus request to close the old copy of the
            // cell.
            lpCellTest->Close();
            }
         else
            {
            Server_CloseAll (FALSE);
            g.lpiCell = NULL;
            Main_Redraw_ThreadProc ((PVOID)FALSE);
            }
         }

      AfsClass_Leave();
      }
}


void Task_Refresh (LPTASKPACKET ptp, BOOL fNewCreds)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);
   AfsClass_Enter();

   if (lpi && lpi->fIsCell())
      {
      AfsAppLib_Uncover (GetDlgItem (g.hMain, IDC_SERVERS));

      LPCELL lpCell;
      if ((lpCell = lpi->OpenCell (&ptp->status)) == NULL)
         {
         CoverServerList (lpi, ptp->status);
         }
      else
         {
         if (fNewCreds)
            lpCell->SetCurrentCredentials ((PVOID)g.hCreds);

         lpCell->Invalidate();
         if ((ptp->rc = lpCell->RefreshAll (&ptp->status)) != TRUE)
            {
            CoverServerList (lpi, ptp->status);
            }
         lpCell->Close();
         }
      }
   else if (lpi && lpi->GetServer())
      {
      BOOL fWasMonitored = TRUE;

      LPSERVER lpServer;
      if ((lpServer = lpi->OpenServer()) != NULL)
         {
         if ((fWasMonitored = lpServer->fIsMonitored()) == FALSE)
            lpServer->SetMonitor (TRUE);
         lpServer->Close();
         }

      if (fWasMonitored) // if it was already monitored, we didn't just refresh
         {
         if (lpi && lpi->fIsServer())
            {
            if ((lpServer = lpi->OpenServer()) != NULL)
               {
               lpServer->Invalidate();
               lpServer->RefreshAll();
               lpServer->Close();
               }
            }
         else if (lpi && lpi->fIsService())
            {
            LPSERVICE lpService;
            if ((lpService = lpi->OpenService()) != NULL)
               {
               lpService->Invalidate();
               lpService->RefreshStatus();
               lpService->Close();
               }
            }
         else if (lpi && lpi->fIsAggregate())
            {
            LPAGGREGATE lpAggregate;
            if ((lpAggregate = lpi->OpenAggregate()) != NULL)
               {
               // if we implied this aggregate from fileset VLDB entries,
               // refresh its parent server, not this aggregate.
               //
               if (!(lpAggregate->GetGhostStatus() & GHOST_HAS_SERVER_ENTRY))
                  {
                  lpAggregate->Close();

                  LPSERVER lpServer;
                  if ((lpServer = lpi->OpenServer()) != NULL)
                     {
                     lpServer->Invalidate();
                     lpServer->RefreshAll();
                     lpServer->Close();
                     }
                  }
               else
                  {
                  lpAggregate->Invalidate();
                  lpAggregate->RefreshStatus();
                  lpAggregate->RefreshFilesets();
                  lpAggregate->Close();

                  LPCELL lpCell;
                  if ((lpCell = lpi->OpenCell()) != NULL)
                     {
                     lpCell->RefreshVLDB (lpi);
                     lpCell->Close();
                     }
                  }
               }
            }
         else if (lpi && lpi->fIsFileset())
            {
            LPFILESET lpFileset;
            if ((lpFileset = lpi->OpenFileset()) != NULL)
               {
               lpFileset->Invalidate();
               lpFileset->RefreshStatus();
               lpFileset->Close();

               LPCELL lpCell;
               if ((lpCell = lpi->OpenCell()) != NULL)
                  {
                  lpCell->RefreshVLDB (lpi);
                  lpCell->Close();
                  }
               }
            }
         }

      Alert_Scout_QueueCheckServer (lpi);
      }

   AfsClass_Leave();
}


void Task_Subset_To_List (LPTASKPACKET ptp)
{
   LPSUBSET_TO_LIST_PACKET lpp = (LPSUBSET_TO_LIST_PACKET)( ptp->lpUser );

   LB_StartChange (lpp->hList, TRUE);

   LPCELL lpCell = NULL;
   if (g.lpiCell)
      lpCell = g.lpiCell->OpenCell();
   if (lpCell)
      {
      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         LPIDENT lpiServer = lpServer->GetIdentifier();
         TCHAR szServer[ cchNAME ];
         lpServer->GetName (szServer);
         lpServer->Close();

         BOOL fMonitor = Subsets_fMonitorServer (lpp->sub, lpiServer);

         LB_AddItem (lpp->hList, szServer, (LPARAM)fMonitor);
         }

      lpCell->Close();
      }

   LB_EndChange (lpp->hList);

   Delete (lpp);
   ptp->lpUser = 0;  // we freed this; don't let anyone use it.
}


void Task_Apply_Subset (LPTASKPACKET ptp)
{
   LPSUBSET sub = (LPSUBSET)( ptp->lpUser );

   LPCELL lpCell = NULL;
   if (g.lpiCell)
      lpCell = g.lpiCell->OpenCell();
   if (lpCell)
      {
      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         LPIDENT lpiServer = lpServer->GetIdentifier();
         TCHAR szServer[ cchNAME ];
         lpServer->GetName (szServer);

         BOOL fMonitor = Subsets_fMonitorServer (sub, lpiServer);
         lpServer->SetMonitor (fMonitor);

         lpServer->Close();
         }

      lpCell->Close();
      }
}



void Task_Svr_Prop_Init (LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)(ptp->lpUser);

   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpServer->GetStatus (&TASKDATA(ptp)->ss, TRUE, &ptp->status))
         {
         ptp->rc = FALSE;
         }
      else
         {
         HENUM hEnum;
         for (LPAGGREGATE lpAggregate = lpServer->AggregateFindFirst (&hEnum); lpAggregate; lpAggregate = lpServer->AggregateFindNext (&hEnum))
            {
            AGGREGATESTATUS as;
            if (lpAggregate->GetStatus (&as))
               {
               TASKDATA(ptp)->ckCapacity += as.ckStorageTotal;
               TASKDATA(ptp)->ckAllocation += as.ckStorageAllocated;
               }

            TASKDATA(ptp)->nAggr++;
            lpAggregate->Close();
            }
         }

      lpServer->Close();
      }
}


void Task_Svr_Scout_Init (LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)(ptp->lpUser);

   if ((TASKDATA(ptp)->lpsp = (LPSERVER_PREF)lpiServer->GetUserParam()) == NULL)
      {
      ptp->rc = FALSE;
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      }
}


void Task_Svr_Scout_Apply (LPTASKPACKET ptp)
{
   LPSVR_SCOUT_APPLY_PACKET lpp = (LPSVR_SCOUT_APPLY_PACKET)(ptp->lpUser);

   BOOL fRefreshVLDB = FALSE;

   LPSERVER_PREF lpsp = NULL;
   if ((lpsp = (LPSERVER_PREF)lpp->lpiServer->GetUserParam()) == NULL)
      {
      ptp->rc = FALSE;
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      }
   else
      {
      if (!lpp->fIDC_SVR_WARN_AGGFULL)
         lpsp->perWarnAggFull = 0;
      else
         lpsp->perWarnAggFull = lpp->wIDC_SVR_WARN_AGGFULL_PERCENT;

      if (!lpp->fIDC_SVR_WARN_SETFULL)
         lpsp->perWarnSetFull = 0;
      else
         lpsp->perWarnSetFull = lpp->wIDC_SVR_WARN_SETFULL_PERCENT;

      DWORD fOldWarnSet = (lpsp->fWarnSetNoVLDB << 1) | lpsp->fWarnSetNoServ;

      lpsp->fWarnAggAlloc   = lpp->fIDC_SVR_WARN_AGGALLOC;
      lpsp->fWarnSvcStop    = lpp->fIDC_SVR_WARN_SVCSTOP;
      lpsp->fWarnSvrTimeout = lpp->fIDC_SVR_WARN_TIMEOUT;
      lpsp->fWarnSetNoVLDB  = lpp->fIDC_SVR_WARN_SETNOVLDB;
      lpsp->fWarnSetNoServ  = lpp->fIDC_SVR_WARN_SETNOSERV;
      lpsp->fWarnAggNoServ  = lpp->fIDC_SVR_WARN_AGGNOSERV;

      DWORD fNewWarnSet = (lpsp->fWarnSetNoVLDB << 1) | lpsp->fWarnSetNoServ;

      if (fNewWarnSet != fOldWarnSet)
         fRefreshVLDB = TRUE;

      if (!lpp->fIDC_SVR_AUTOREFRESH)
         lpsp->oa.cTickRefresh = 0;
      else
         lpsp->oa.cTickRefresh = (DWORD)(cmsec1MINUTE * lpp->dwIDC_SVR_AUTOREFRESH_MINUTES);

      if (!Server_SavePreferences (lpp->lpiServer))
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      }

   if (fRefreshVLDB)
      {
      LPCELL lpCell;
      if ((lpCell = lpp->lpiServer->OpenCell()) != NULL)
         {
         lpCell->RefreshVLDB (NULL);
         lpCell->Close();
         }
      }

   (void)Alert_Scout_QueueCheckServer (lpp->lpiServer);

   Delete (lpp);
   ptp->lpUser = 0;  // we freed this; don't let anyone use it.
}


void Task_Svr_Enum_To_ComboBox (LPTASKPACKET ptp)
{
   LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = (LPSVR_ENUM_TO_COMBOBOX_PACKET)(ptp->lpUser);

   // Fill in the Servers combobox, and select the default server
   // (if one was specified, either as a server or as a fileset).
   //
   CB_StartChange (lpp->hCombo, TRUE);

   LPCELL lpCell;
   if ((lpCell = g.lpiCell->OpenCell (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      HENUM hEnum;
      for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
         {
         TCHAR szName[ cchNAME ];
         lpServer->GetName (szName);
         CB_AddItem (lpp->hCombo, szName, (LPARAM)lpServer->GetIdentifier());
         lpServer->Close();
         }
      lpCell->Close();
      }

   if (lpp->lpiSelect && lpp->lpiSelect->GetServer())
      CB_EndChange (lpp->hCombo, (LPARAM)(lpp->lpiSelect->GetServer()));
   else
      {
      CB_EndChange (lpp->hCombo, 0);
      CB_SetSelected (lpp->hCombo, 0);
      }

   Delete (lpp);
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.
}


void Task_Svr_GetWindowPos (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   SetRectEmpty (&TASKDATA(ptp)->rWindow);

   if (lpi != NULL)
      {
      LPSERVER_PREF lpsp = (LPSERVER_PREF)lpi->GetUserParam();

      if (lpsp != NULL && (lpsp->rLast.right != 0))
         {
         TASKDATA(ptp)->rWindow = lpsp->rLast;
         }
      }
}


void Task_Svr_SetWindowPos (LPTASKPACKET ptp)
{
   LPSVR_SETWINDOWPOS_PARAMS lpp = (LPSVR_SETWINDOWPOS_PARAMS)(ptp->lpUser);

   if (lpp->lpi)
      {
      LPSERVER_PREF lpsp = (LPSERVER_PREF)(lpp->lpi->GetUserParam());

      if (lpsp != NULL)
         {
         lpsp->rLast = lpp->rWindow;
         lpsp->fOpen = lpp->fOpen;

         StorePreferences (lpp->lpi, lpsp, sizeof(SERVER_PREF));
         }
      }

   Delete (lpp);
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.
}


void Task_Svr_SyncVLDB (LPTASKPACKET ptp)
{
   LPSVR_SYNCVLDB_PARAMS lpp = (LPSVR_SYNCVLDB_PARAMS)(ptp->lpUser);

   if (!AfsClass_SyncVLDB (lpp->lpi, lpp->fForce, &ptp->status))
      ptp->rc = FALSE;

   if (!ptp->rc && !IsWindow (ptp->hReply))
      {
      ErrorDialog (ptp->status, IDS_ERROR_CANT_SYNCVLDB);
      }

   Delete (lpp);
   ptp->lpUser = 0;  // we freed this; don't let anyone use it.
}


void Task_Svr_Salvage (LPTASKPACKET ptp)
{
   LPSVR_SALVAGE_PARAMS lpp = (LPSVR_SALVAGE_PARAMS)(ptp->lpUser);

   LPTSTR pszTempDir = (lpp->szTempDir[0] != TEXT('\0')) ? lpp->szTempDir : NULL;
   LPTSTR pszLogFile = (lpp->szLogFile[0] != TEXT('\0')) ? lpp->szLogFile : NULL;

   if (!AfsClass_Salvage (lpp->lpiSalvage, &TASKDATA(ptp)->pszText1, lpp->nProcesses, pszTempDir, pszLogFile, lpp->fForce, lpp->fReadonly, lpp->fLogInodes, lpp->fLogRootInodes, lpp->fRebuildDirs, lpp->fReadBlocks, &ptp->status))
      ptp->rc = FALSE;

   Delete (lpp);
   ptp->lpUser = 0;  // we freed this; don't let anyone use it.
}


void Task_Svr_Install (LPTASKPACKET ptp)
{
   LPSVR_INSTALL_PARAMS lpp = (LPSVR_INSTALL_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_InstallFile (lpp->lpiServer, lpp->szTarget, lpp->szSource, &ptp->status);

   if (!ptp->rc && !ptp->hReply)
      {
      TCHAR szFilename[ MAX_PATH ];
      TCHAR szSvrName[ cchRESOURCE ];
      lpp->lpiServer->GetServerName (szSvrName);
      CopyBaseFileName (szFilename, lpp->szSource);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_INSTALL_FILE, TEXT("%s%s"), szSvrName, szFilename);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_Uninstall (LPTASKPACKET ptp)
{
   LPSVR_UNINSTALL_PARAMS lpp = (LPSVR_UNINSTALL_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_UninstallFile (lpp->lpiServer, lpp->szUninstall, &ptp->status);

   if (!ptp->rc && !ptp->hReply)
      {
      TCHAR szFilename[ MAX_PATH ];
      TCHAR szSvrName[ cchRESOURCE ];
      lpp->lpiServer->GetServerName (szSvrName);
      CopyBaseFileName (szFilename, lpp->szUninstall);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_UNINSTALL_FILE, TEXT("%s%s"), szSvrName, szFilename);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_Prune (LPTASKPACKET ptp)
{
   LPSVR_PRUNE_PARAMS lpp = (LPSVR_PRUNE_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_PruneOldFiles (lpp->lpiServer, lpp->fBAK, lpp->fOLD, lpp->fCore, &ptp->status);

   if (!ptp->rc && !ptp->hReply)
      {
      TCHAR szSvrName[ cchRESOURCE ];
      lpp->lpiServer->GetServerName (szSvrName);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_PRUNE_FILES, TEXT("%s"), szSvrName);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_GetDates (LPTASKPACKET ptp)
{
   LPSVR_GETDATES_PARAMS lpp = (LPSVR_GETDATES_PARAMS)(ptp->lpUser);

   SYSTEMTIME stFile;
   SYSTEMTIME stOLD;
   SYSTEMTIME stBAK;

   ptp->rc = AfsClass_GetFileDates (lpp->lpiServer, lpp->szFilename, &stFile, &stBAK, &stOLD, &ptp->status);

   if (ptp->rc)
      {
      TCHAR szText[ cchRESOURCE ];
      if (FormatTime (szText, TEXT("%s"), &stFile))
         TASKDATA(ptp)->pszText1 = CloneString (szText);

      if (FormatTime (szText, TEXT("%s"), &stBAK))
         TASKDATA(ptp)->pszText2 = CloneString (szText);

      if (FormatTime (szText, TEXT("%s"), &stOLD))
         TASKDATA(ptp)->pszText3 = CloneString (szText);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_Execute (LPTASKPACKET ptp)
{
   LPSVR_EXECUTE_PARAMS lpp = (LPSVR_EXECUTE_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_ExecuteCommand (lpp->lpiServer, lpp->szCommand, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      lpp->lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_EXECUTE_COMMAND, TEXT("%s%s"), szServer, lpp->szCommand);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_SetAuth (LPTASKPACKET ptp)
{
   LPSVR_SETAUTH_PARAMS lpp = (LPSVR_SETAUTH_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_SetServerAuth (lpp->lpiServer, lpp->fEnableAuth, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      lpp->lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, (lpp->fEnableAuth) ? IDS_ERROR_CANT_AUTH_ON : IDS_ERROR_CANT_AUTH_OFF, TEXT("%s"), szServer);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_AdmList_Open (LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)(ptp->lpUser);

   if ((TASKDATA(ptp)->lpAdmList = AfsClass_AdminList_Load (lpiServer, &ptp->status)) == NULL)
      {
      ptp->rc = FALSE;

      TCHAR szServer[ cchNAME ];
      lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_LOAD_ADMLIST, TEXT("%s"), szServer);
      }
}


void Task_Svr_AdmList_Save (LPTASKPACKET ptp)
{
   LPADMINLIST lpAdmList = (LPADMINLIST)(ptp->lpUser);

   // Increment the reference counter on this admin list before handing
   // it off to the Save routine, as that routine will be posting
   // notifications using its handle. When our notification handler
   // receives the End notification, it will attempt to free the list--
   // which will decrement the counter again, and actually free the list
   // if the counter hits zero.
   //
   InterlockedIncrement (&lpAdmList->cRef);

   ptp->rc = AfsClass_AdminList_Save (lpAdmList, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      lpAdmList->lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_SAVE_ADMLIST, TEXT("%s"), szServer);
      }

   AfsClass_AdminList_Free (lpAdmList);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_KeyList_Open (LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)(ptp->lpUser);

   if ((TASKDATA(ptp)->lpKeyList = AfsClass_KeyList_Load (lpiServer, &ptp->status)) == NULL)
      {
      ptp->rc = FALSE;

      TCHAR szServer[ cchNAME ];
      lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_LOAD_KEYLIST, TEXT("%s"), szServer);
      }
}


void Task_Svr_Key_Create (LPTASKPACKET ptp)
{
   LPKEY_CREATE_PARAMS lpp = (LPKEY_CREATE_PARAMS)(ptp->lpUser);

   if (lpp->szString[0])
      ptp->rc = AfsClass_AddKey (lpp->lpiServer, lpp->keyVersion, lpp->szString, &ptp->status);
   else
      ptp->rc = AfsClass_AddKey (lpp->lpiServer, lpp->keyVersion, &lpp->key, &ptp->status);

   if ((TASKDATA(ptp)->lpKeyList = AfsClass_KeyList_Load (lpp->lpiServer, &ptp->status)) == NULL)
      {
      ptp->rc = FALSE;

      TCHAR szServer[ cchNAME ];
      lpp->lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CREATE_KEY, TEXT("%s"), szServer);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_Key_Delete (LPTASKPACKET ptp)
{
   LPKEY_DELETE_PARAMS lpp = (LPKEY_DELETE_PARAMS)(ptp->lpUser);

   if (!AfsClass_DeleteKey (lpp->lpiServer, lpp->keyVersion, &ptp->status))
      {
      ptp->rc = FALSE;

      TCHAR szServer[ cchNAME ];
      lpp->lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_DELETE_KEY, TEXT("%s%lu"), szServer, lpp->keyVersion);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_GetRandomKey (LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)(ptp->lpUser);

   if (!AfsClass_GetRandomKey (lpiServer, &TASKDATA(ptp)->key, &ptp->status))
      {
      ptp->rc = FALSE;

      TCHAR szServer[ cchNAME ];
      lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_GETRANDOMKEY, TEXT("%s"), szServer);
      }
}


void Task_Svr_HostList_Open (LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)(ptp->lpUser);

   if ((TASKDATA(ptp)->lpHostList = AfsClass_HostList_Load (lpiServer, &ptp->status)) == NULL)
      {
      ptp->rc = FALSE;

      TCHAR szServer[ cchNAME ];
      lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_LOAD_HOSTLIST, TEXT("%s"), szServer);
      }
}


void Task_Svr_HostList_Save (LPTASKPACKET ptp)
{
   LPHOSTLIST lpHostList = (LPHOSTLIST)(ptp->lpUser);

   // Increment the reference counter on this admin list before handing
   // it off to the Save routine, as that routine will be posting
   // notifications using its handle. When our notification handler
   // receives the End notification, it will attempt to free the list--
   // which will decrement the counter again, and actually free the list
   // if the counter hits zero.
   //
   InterlockedIncrement (&lpHostList->cRef);

   ptp->rc = AfsClass_HostList_Save (lpHostList, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      lpHostList->lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_SAVE_HOSTLIST, TEXT("%s"), szServer);
      }

   AfsClass_HostList_Free (lpHostList);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svr_Monitor_OnOff (LPTASKPACKET ptp)
{
   LPIDENT lpiServer = (LPIDENT)(ptp->lpUser);

   LPSERVER lpServer;
   if ((lpServer = lpiServer->OpenServer()) != NULL)
      {
      BOOL fMonitored = lpServer->fIsMonitored();

      g.sub = Subsets_SetMonitor (g.sub, lpiServer, !fMonitored);

      lpServer->SetMonitor (!fMonitored);

      lpServer->Close();
      }
}


void Task_Svr_ChangeAddr (LPTASKPACKET ptp)
{
   LPSVR_CHANGEADDR_PARAMS lpp = (LPSVR_CHANGEADDR_PARAMS)(ptp->lpUser);

   if ((ptp->rc = AfsClass_ChangeAddress (lpp->lpiServer, &lpp->ssOld, &lpp->ssNew, &ptp->status)) == FALSE)
      {
      TCHAR szName[ cchRESOURCE ];
      lpp->lpiServer->GetServerName (szName);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CHANGEADDR, TEXT("%s"), szName);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svc_Menu (LPTASKPACKET ptp)
{
   TASKDATA(ptp)->mt = *(LPMENUTASK)(ptp->lpUser);
   Delete ((LPMENUTASK)(ptp->lpUser));
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.

   if (TASKDATA(ptp)->mt.lpi && TASKDATA(ptp)->mt.lpi->fIsService())
      {
      LPSERVICE lpService;
      if ((lpService = TASKDATA(ptp)->mt.lpi->OpenService (&ptp->status)) == NULL)
         ptp->rc = FALSE;
      else
         {
         if (!lpService->GetStatus (&TASKDATA(ptp)->cs, FALSE, &ptp->status))
            ptp->rc = FALSE;
         lpService->Close();
         }
      }
}


void Task_Svc_Prop_Init (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPSERVICE lpService;
   if ((lpService = lpi->OpenService (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpService->GetStatus (&TASKDATA(ptp)->cs, TRUE, &ptp->status))
         {
         ptp->rc = FALSE;
         }
      else if ((TASKDATA(ptp)->lpcp = (LPSERVICE_PREF)lpi->GetUserParam()) == NULL)
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      lpService->Close();
      }

   if (ptp->rc)
      {
      if ((TASKDATA(ptp)->lpsp = (LPSERVER_PREF)lpi->GetServer()->GetUserParam()) == NULL)
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      }
}


void Task_Svc_Prop_Apply (LPTASKPACKET ptp)
{
   LPSVC_PROP_APPLY_PACKET lpp = (LPSVC_PROP_APPLY_PACKET)(ptp->lpUser);

   LPSERVICE lpService;
   if ((lpService = lpp->lpi->OpenService (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      LPSERVICE_PREF lpcp = NULL;
      if ((lpcp = (LPSERVICE_PREF)lpp->lpi->GetUserParam()) == NULL)
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      else
         {
         LPIDENT lpiServer;
         if ((lpiServer = lpp->lpi->GetServer()) != NULL)
            {
            LPSERVER_PREF lpsp;
            if ((lpsp = (LPSERVER_PREF)lpiServer->GetUserParam()) != NULL)
               {
               if (lpsp->fWarnSvcStop)
                  lpcp->fWarnSvcStop = lpp->fIDC_SVC_WARNSTOP;
               }
            }

         if (!Services_SavePreferences (lpp->lpi))
            {
            ptp->rc = FALSE;
            ptp->status = GetLastError();
            }
         }

      lpService->Close();
      }

   (void)Alert_Scout_QueueCheckServer (lpp->lpi);

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svc_Start (LPTASKPACKET ptp)
{
   LPSVC_START_PARAMS lpp = (LPSVC_START_PARAMS)(ptp->lpUser);

   if (!AfsClass_StartService (lpp->lpiStart, lpp->fTemporary, &ptp->status))
      ptp->rc = FALSE;

   if (!ptp->rc && !ptp->hReply)
      {
      TCHAR szSvrName[ cchRESOURCE ];
      TCHAR szSvcName[ cchRESOURCE ];
      lpp->lpiStart->GetServerName (szSvrName);
      lpp->lpiStart->GetServiceName (szSvcName);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_START_SERVICE, TEXT("%s%s"), szSvrName, szSvcName);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svc_Stop (LPTASKPACKET ptp)
{
   LPSVC_STOP_PARAMS lpp = (LPSVC_STOP_PARAMS)(ptp->lpUser);

   if (!AfsClass_StopService (lpp->lpiStop, lpp->fTemporary, TRUE, &ptp->status))
      ptp->rc = FALSE;

   if (!ptp->rc && !ptp->hReply)
      {
      TCHAR szSvrName[ cchRESOURCE ];
      TCHAR szSvcName[ cchRESOURCE ];
      lpp->lpiStop->GetServerName (szSvrName);
      lpp->lpiStop->GetServiceName (szSvcName);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_STOP_SERVICE, TEXT("%s%s"), szSvrName, szSvcName);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svc_Restart (LPTASKPACKET ptp)
{
   LPIDENT lpiService = (LPIDENT)(ptp->lpUser);

   if (!AfsClass_RestartService (lpiService, &ptp->status))
      ptp->rc = FALSE;

   if (!ptp->rc && !ptp->hReply)
      {
      TCHAR szSvrName[ cchRESOURCE ];
      TCHAR szSvcName[ cchRESOURCE ];
      lpiService->GetServerName (szSvrName);
      lpiService->GetServiceName (szSvcName);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_RESTART_SERVICE, TEXT("%s%s"), szSvrName, szSvcName);
      }
}


void Task_Svc_FindLog (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   if ((TASKDATA(ptp)->lpcp = (LPSERVICE_PREF)lpi->GetUserParam()) == NULL)
      {
      ptp->rc = FALSE;
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      }

   if (ptp->rc)
      {
      if (TASKDATA(ptp)->lpcp->szLogFile[0] == TEXT('\0'))
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_FILE_NOT_FOUND;
         }
      }

   if (ptp->rc)
      {
      if ((TASKDATA(ptp)->pszText1 = CloneString (TASKDATA(ptp)->lpcp->szLogFile)) == NULL)
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      }
}


void Task_Svc_ViewLog (LPTASKPACKET ptp)
{
   LPSVC_VIEWLOG_PACKET lpp = (LPSVC_VIEWLOG_PACKET)(ptp->lpUser);

   TCHAR szTempPath[ MAX_PATH ];
   GetTempPath (MAX_PATH, szTempPath);

   TCHAR szFilename[ MAX_PATH ];
   GetTempFileName (szTempPath, TEXT("log"), 0, szFilename);

   if (!AfsClass_GetServerLogFile (lpp->lpiServer, szFilename, lpp->szRemote, &ptp->status))
      {
      DeleteFile (szFilename);
      ptp->rc = FALSE;
      }

   if (ptp->rc)
      {
      if ((TASKDATA(ptp)->pszText1 = CloneString (szFilename)) == NULL)
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svc_Create (LPTASKPACKET ptp)
{
   LPSVC_CREATE_PARAMS lpp = (LPSVC_CREATE_PARAMS)(ptp->lpUser);

   LPIDENT lpiService;
   if ((lpiService = AfsClass_CreateService (lpp->lpiServer, lpp->szService, lpp->szCommand, lpp->szParams, lpp->szNotifier, lpp->type, &lpp->stIfCron, &ptp->status)) == NULL)
      {
      ptp->rc = FALSE;
      }

   if (ptp->rc)
      {
      // Start it if necessary
      //
      if ((lpp->type == SERVICETYPE_SIMPLE) && (lpp->fRunNow))
         {
         if (!AfsClass_StartService (lpiService, FALSE, &ptp->status))
            {
            ptp->rc = FALSE;
            }
         }
      }

   if (ptp->rc && lpp->szLogFile[0])
      {
      LPSERVICE_PREF lpsp;
      if ((lpsp = (LPSERVICE_PREF)lpiService->GetUserParam()) != NULL)
         {
         lstrcpy (lpsp->szLogFile, lpp->szLogFile);

         if (!Services_SavePreferences (lpiService))
            {
            ptp->rc = FALSE;
            ptp->status = ERROR_NOT_ENOUGH_MEMORY;
            }
         }
      }

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      lpp->lpiServer->GetServerName (szServer);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CREATE_SERVICE, TEXT("%s%s"), szServer, lpp->szService);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Svc_Delete (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   ptp->rc = AfsClass_DeleteService (lpi, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szService[ cchNAME ];
      lpi->GetServerName (szServer);
      lpi->GetServiceName (szService);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_DELETE_SERVICE, TEXT("%s%s"), szServer, szService);
      }
}


void Task_Svc_GetRestartTimes (LPTASKPACKET ptp)
{
   LPSVC_RESTARTTIMES_PARAMS lpp = (LPSVC_RESTARTTIMES_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_GetRestartTimes (lpp->lpi, &lpp->fGeneral, &lpp->stGeneral, &lpp->fNewBinary, &lpp->stNewBinary, &ptp->status);

   if (!ptp->rc)
      {
      lpp->fGeneral = FALSE;
      lpp->fNewBinary = FALSE;

      TCHAR szSvrName[ cchNAME ];
      lpp->lpi->GetServerName (szSvrName);
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_SERVICE_STATUS, TEXT("%s%s"), szSvrName, TEXT("BOS"));
      }

   if (!ptp->hReply)
      {
      Delete (lpp);
      ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
      }
}


void Task_Svc_SetRestartTimes (LPTASKPACKET ptp)
{
   LPSVC_RESTARTTIMES_PARAMS lpp = (LPSVC_RESTARTTIMES_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_SetRestartTimes (lpp->lpi, ((lpp->fGeneral) ? &lpp->stGeneral : NULL), ((lpp->fNewBinary) ? &lpp->stNewBinary : NULL), &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szSvrName[ cchNAME ];
      lpp->lpi->GetServerName (szSvrName);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_SET_RESTART_TIMES, TEXT("%s"), szSvrName);
      }

   if (!ptp->hReply)
      {
      Delete (lpp);
      ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
      }
}


void Task_Agg_Prop_Init (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpi->OpenAggregate (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpAggregate->GetStatus (&TASKDATA(ptp)->as, TRUE, &ptp->status))
         {
         ptp->rc = FALSE;
         }

      if ((TASKDATA(ptp)->lpap = (LPAGGREGATE_PREF)lpAggregate->GetUserParam()) == NULL)
         {
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         ptp->rc = FALSE;
         }

      TCHAR szText[ cchRESOURCE ];
      lpAggregate->GetDevice (szText);
      TASKDATA(ptp)->pszText1 = CloneString (szText);

      if (ptp->rc)
         {
         TASKDATA(ptp)->nFilesets = 0;

         HENUM hEnum;
         for (LPFILESET lpFileset = lpAggregate->FilesetFindFirst (&hEnum); lpFileset; lpFileset = lpAggregate->FilesetFindNext (&hEnum))
            {
            (TASKDATA(ptp)->nFilesets)++;
            lpFileset->Close();
            }
         }

      lpAggregate->Close();
      }

   if (ptp->rc)
      {
      if ((TASKDATA(ptp)->lpsp = (LPSERVER_PREF)lpi->GetServer()->GetUserParam()) == NULL)
         {
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         ptp->rc = FALSE;
         }
      }
}


void Task_Agg_Prop_Apply (LPTASKPACKET ptp)
{
   LPAGG_PROP_APPLY_PACKET lpp = (LPAGG_PROP_APPLY_PACKET)(ptp->lpUser);

   LPAGGREGATE_PREF lpap;
   if ((lpap = (LPAGGREGATE_PREF)lpp->lpi->GetAggregate()->GetUserParam()) == NULL)
      {
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      ptp->rc = FALSE;
      }
   else
      {
      LPIDENT lpiServer;
      if ((lpiServer = lpp->lpi->GetServer()) != NULL)
         {
         LPSERVER_PREF lpsp;
         if ((lpsp = (LPSERVER_PREF)lpiServer->GetUserParam()) != NULL)
            {
            if (lpsp->fWarnAggAlloc)
               lpap->fWarnAggAlloc = lpp->fIDC_AGG_WARNALLOC;
            }
         }

      if (!lpp->fIDC_AGG_WARN)
         lpap->perWarnAggFull = 0;
      else if (lpp->fIDC_AGG_WARN_AGGFULL_DEF)
         lpap->perWarnAggFull = -1;
      else
         lpap->perWarnAggFull = lpp->wIDC_AGG_WARN_AGGFULL_PERCENT;

      if (!Aggregates_SavePreferences (lpp->lpi))
         {
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         ptp->rc = FALSE;
         }

      (void)Alert_Scout_QueueCheckServer (lpp->lpi);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Agg_Find_Quota_Limits (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   TASKDATA(ptp)->ckMin = ckQUOTA_MINIMUM;
   TASKDATA(ptp)->ckMax = ckQUOTA_MAXIMUM;

   if (lpi && (lpi->fIsAggregate() || lpi->fIsFileset()))
      {
      LPAGGREGATE lpAggregate;
      if ((lpAggregate = lpi->OpenAggregate (&ptp->status)) == NULL)
         ptp->rc = FALSE;
      else
         {
         AGGREGATESTATUS as;
         if (!lpAggregate->GetStatus (&as, TRUE, &ptp->status))
            ptp->rc = FALSE;
         else
            {
            TASKDATA(ptp)->ckMax = max( 1L, as.ckStorageTotal );
            }
         lpAggregate->Close();
         }
      }
}


void Task_Agg_Enum_To_ListView (LPTASKPACKET ptp)
{
   LPAGG_ENUM_TO_LISTVIEW_PACKET lpp = (LPAGG_ENUM_TO_LISTVIEW_PACKET)(ptp->lpUser);

   UpdateDisplay_Aggregates (TRUE, lpp->hList, NULL, 0, lpp->lpiServer, lpp->lpiSelect, lpp->lpvi);

   Delete (lpp);
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.
}


void Task_Agg_Enum_To_ComboBox (LPTASKPACKET ptp)
{
   LPAGG_ENUM_TO_COMBOBOX_PACKET lpp = (LPAGG_ENUM_TO_COMBOBOX_PACKET)(ptp->lpUser);

   UpdateDisplay_Aggregates (TRUE, lpp->hCombo, NULL, 0, lpp->lpiServer, lpp->lpiSelect, NULL);

   Delete (lpp);
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.
}


void Task_Agg_Find_Ghost (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPAGGREGATE lpAggregate;
   if ((lpAggregate = lpi->OpenAggregate (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      TASKDATA(ptp)->wGhost = lpAggregate->GetGhostStatus();
      lpAggregate->Close();
      }
}


void Task_Set_Enum_To_ComboBox (LPTASKPACKET ptp)
{
   LPSET_ENUM_TO_COMBOBOX_PACKET lpp = (LPSET_ENUM_TO_COMBOBOX_PACKET)(ptp->lpUser);

   UpdateDisplay_Filesets (TRUE, lpp->hCombo, NULL, 0, lpp->lpiServer, lpp->lpiAggregate, lpp->lpiSelect);

   Delete (lpp);
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.
}


void Task_Set_Find_Ghost (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpi->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      TASKDATA(ptp)->wGhost = lpFileset->GetGhostStatus();
      TASKDATA(ptp)->fHasReplicas = FALSE;

      if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs))
         {
         TASKDATA(ptp)->fs.Type = ftREADWRITE;
         }
      else if (TASKDATA(ptp)->fs.Type == ftREADWRITE)
         {
         HENUM hEnum;
         for (LPIDENT lpiSearch = IDENT::FindFirst (&hEnum, &TASKDATA(ptp)->fs.idReplica); lpiSearch; lpiSearch = IDENT::FindNext (&hEnum))
            {
            LPFILESET lpReplica;
            if ((lpReplica = lpiSearch->OpenFileset()) != NULL)
               {
               lpReplica->Close();
               TASKDATA(ptp)->fHasReplicas = TRUE;
               break;
               }
            }
         IDENT::FindClose (&hEnum);
         }

      lpFileset->Close();
      }
}


void Task_Set_Create (LPTASKPACKET ptp)
{
   LPSET_CREATE_PARAMS lpp = (LPSET_CREATE_PARAMS)(ptp->lpUser);

   LPIDENT lpiFileset;
   if ((lpiFileset = AfsClass_CreateFileset (lpp->lpiParent, lpp->szName, (ULONG)lpp->ckQuota, &ptp->status)) == NULL)
      ptp->rc = FALSE;

   if (ptp->rc && lpp->fCreateClone)
      {
      ptp->rc = AfsClass_Clone (lpiFileset, &ptp->status);
      }

   if (ptp->rc)
      {
      TASKDATA(ptp)->lpi = lpiFileset;
      }
   else
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      lpp->lpiParent->GetServerName (szServer);
      lpp->lpiParent->GetAggregateName (szAggregate);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CREATE_FILESET, TEXT("%s%s%s"), szServer, szAggregate, lpp->szName);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_Delete (LPTASKPACKET ptp)
{
   LPSET_DELETE_PARAMS lpp = (LPSET_DELETE_PARAMS)(ptp->lpUser);

   // First, what kind of fileset are we deleting here?
   //
   LPIDENT lpiClone = NULL;
   FILESETTYPE setType = ftREADWRITE;
   LPFILESET lpFileset;
   if ((lpFileset = lpp->lpiFileset->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      lpiClone = lpFileset->GetCloneIdentifier();

      if (lpFileset->GetStatus (&TASKDATA(ptp)->fs))
         setType = TASKDATA(ptp)->fs.Type;

      lpFileset->Close();
      }

   // Delete the fileset in whichever way is appropriate
   //
   if (setType == ftREADWRITE)
      {
      if (lpp->fVLDB && lpiClone)
         {
         LPFILESET lpClone;
         if ((lpClone = lpiClone->OpenFileset()) != NULL) // clone really there?
            {
            lpClone->Close();

            if (!AfsClass_DeleteClone (lpiClone, &ptp->status))
               ptp->rc = FALSE;
            }
         }
      if (ptp->rc)
         {
         if (!AfsClass_DeleteFileset (lpp->lpiFileset, lpp->fVLDB, lpp->fServer, &ptp->status))
            ptp->rc = FALSE;
         }
      }
   else if (setType == ftREPLICA)
      {
      if (!AfsClass_DeleteReplica (lpp->lpiFileset, &ptp->status))
         ptp->rc = FALSE;
      }
   else if (setType == ftCLONE)
      {
      if (!AfsClass_DeleteClone (lpp->lpiFileset, &ptp->status))
         ptp->rc = FALSE;
      }

   // Clean up
   //
   if ( (!ptp->rc) ||
        (!lpp->fVLDB   && (lpp->wGhost & GHOST_HAS_VLDB_ENTRY)) ||
        (!lpp->fServer && (lpp->wGhost & GHOST_HAS_SERVER_ENTRY)) )
      {
      Alert_Scout_QueueCheckServer (lpp->lpiFileset->GetServer());
      }

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpp->lpiFileset->GetServerName (szServer);
      lpp->lpiFileset->GetAggregateName (szAggregate);
      lpp->lpiFileset->GetFilesetName (szFileset);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_DELETE_FILESET, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_Move (LPTASKPACKET ptp)
{
   LPSET_MOVE_PARAMS lpp = (LPSET_MOVE_PARAMS)(ptp->lpUser);

   BOOL fIsReplica = FALSE;

   LPFILESET lpFileset;
   if ((lpFileset = lpp->lpiSource->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      FILESETSTATUS fs;
      if (!lpFileset->GetStatus (&fs, TRUE, &ptp->status))
         ptp->rc = FALSE;
      else
         {
         if (fs.Type == ftREPLICA)
            fIsReplica = TRUE;
         }
      lpFileset->Close();
      }

   if (ptp->rc)
      {
      if (fIsReplica)
         {
         if (!AfsClass_MoveReplica (lpp->lpiSource, lpp->lpiTarget, &ptp->status))
            ptp->rc = FALSE;
         }
      else
         {
         if (!AfsClass_MoveFileset (lpp->lpiSource, lpp->lpiTarget, &ptp->status))
            ptp->rc = FALSE;
         }
      }

   if (!ptp->rc)
      {
      TCHAR szServerSource[ cchNAME ];
      TCHAR szServerTarget[ cchNAME ];
      TCHAR szAggregateSource[ cchNAME ];
      TCHAR szAggregateTarget[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpp->lpiSource->GetServerName (szServerSource);
      lpp->lpiSource->GetAggregateName (szAggregateSource);
      lpp->lpiSource->GetFilesetName (szFileset);
      lpp->lpiTarget->GetServerName (szServerTarget);
      lpp->lpiTarget->GetAggregateName (szAggregateTarget);
      ErrorDialog (ptp->status, IDS_ERROR_MOVE_FILESET, TEXT("%s%s%s%s%s"), szServerSource, szAggregateSource, szFileset, szServerTarget, szAggregateTarget);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_MoveTo_Init (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpi->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs, TRUE, &ptp->status))
         ptp->rc = FALSE;
      lpFileset->Close();
      }
}


void Task_Set_Prop_Init (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpi->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs, TRUE, &ptp->status))
         ptp->rc = FALSE;
      else if ((TASKDATA(ptp)->lpfp = (LPFILESET_PREF)lpFileset->GetUserParam()) == NULL)
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      lpFileset->Close();
      }

   if (ptp->rc)
      {
      if ((TASKDATA(ptp)->lpsp = (LPSERVER_PREF)lpi->GetServer()->GetUserParam()) == NULL)
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      }
}


void Task_Set_Prop_Apply (LPTASKPACKET ptp)
{
   LPSET_PROP_APPLY_PARAMS lpp = (LPSET_PROP_APPLY_PARAMS)(ptp->lpUser);

   LPFILESET_PREF lpfp = NULL;
   if ((lpfp = (LPFILESET_PREF)lpp->lpi->GetUserParam()) == NULL)
      {
      ptp->rc = FALSE;
      ptp->status = ERROR_NOT_ENOUGH_MEMORY;
      }
   else
      {
      if (!lpp->fIDC_SET_WARN)
         lpfp->perWarnSetFull = 0;
      else if (lpp->fIDC_SET_WARN_SETFULL_DEF)
         lpfp->perWarnSetFull = -1;
      else
         lpfp->perWarnSetFull = lpp->wIDC_SET_WARN_SETFULL_PERCENT;

      if (!Filesets_SavePreferences (lpp->lpi))
         {
         ptp->rc = FALSE;
         ptp->status = ERROR_NOT_ENOUGH_MEMORY;
         }
      }

   Alert_Scout_QueueCheckServer (lpp->lpi);

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_SetQuota_Init (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpi->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs, TRUE, &ptp->status))
         ptp->rc = FALSE;

      lpFileset->Close();
      }

   if (ptp->rc)
      {
      Task_Agg_Find_Quota_Limits (ptp);
      }
}


void Task_Set_SetQuota_Apply (LPTASKPACKET ptp)
{
   LPSET_SETQUOTA_APPLY_PARAMS lpp = (LPSET_SETQUOTA_APPLY_PARAMS)(ptp->lpUser);

   if (!AfsClass_SetFilesetQuota (lpp->lpiFileset, lpp->ckQuota, &ptp->status))
      ptp->rc = FALSE;

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpp->lpiFileset->GetServerName (szServer);
      lpp->lpiFileset->GetAggregateName (szAggregate);
      lpp->lpiFileset->GetFilesetName (szFileset);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_SET_FILESET_QUOTA, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_RepProp_Init (LPTASKPACKET ptp)
{
   LPSET_REPPROP_INIT_PARAMS lpp = (LPSET_REPPROP_INIT_PARAMS)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpp->lpiReq->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if ((lpp->lpiRW = lpFileset->GetReadWriteIdentifier (&ptp->status)) == NULL)
         ptp->rc = FALSE;
      else if (!lpFileset->GetStatus (&lpp->fs, TRUE, &ptp->status))
         ptp->rc = FALSE;
      lpFileset->Close();
      }

   // don't delete this packet (ptp->lpUser)!  the caller will free it.
}


void Task_Set_Select (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpi->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs, TRUE, &ptp->status))
         ptp->rc = FALSE;

      lpFileset->Close();
      }
}


void Task_Set_BeginDrag (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpi->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs, TRUE, &ptp->status))
         ptp->rc = FALSE;
      lpFileset->Close();
      }
}


void Task_Set_DragMenu (LPTASKPACKET ptp)
{
   TASKDATA(ptp)->mt = *(LPMENUTASK)(ptp->lpUser);
   Delete ((LPMENUTASK)(ptp->lpUser));
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.

   if (TASKDATA(ptp)->mt.lpi && TASKDATA(ptp)->mt.lpi->fIsFileset())
      {
      LPFILESET lpFileset;
      if ((lpFileset = TASKDATA(ptp)->mt.lpi->OpenFileset (&ptp->status)) == NULL)
         ptp->rc = FALSE;
      else
         {
         if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs, TRUE, &ptp->status))
            ptp->rc = FALSE;
         lpFileset->Close();
         }
      }
}


void Task_Set_Menu (LPTASKPACKET ptp)
{
   TASKDATA(ptp)->mt = *(LPMENUTASK)(ptp->lpUser);
   Delete ((LPMENUTASK)(ptp->lpUser));
   ptp->lpUser = NULL;  // we deleted this; don't let the caller use it again.

   if (TASKDATA(ptp)->mt.lpi && TASKDATA(ptp)->mt.lpi->fIsFileset())
      {
      LPFILESET lpFileset;
      if ((lpFileset = TASKDATA(ptp)->mt.lpi->OpenFileset (&ptp->status)) == NULL)
         ptp->rc = FALSE;
      else
         {
         if (!lpFileset->GetStatus (&TASKDATA(ptp)->fs, TRUE, &ptp->status))
            ptp->rc = FALSE;

         lpFileset->Close();
         }
      }
}


void Task_Set_Lock (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);
   ptp->rc = AfsClass_LockFileset (lpi, &ptp->status);
}


void Task_Set_Unlock (LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   if (lpi->fIsFileset())
      ptp->rc = AfsClass_UnlockFileset (lpi, &ptp->status);
   else
      ptp->rc = AfsClass_UnlockAllFilesets (lpi, &ptp->status);
}


void Task_Set_CreateRep (LPTASKPACKET ptp)
{
   LPSET_CREATEREP_PARAMS lpp = (LPSET_CREATEREP_PARAMS)(ptp->lpUser);

   LPIDENT lpiReplica;
   if ((lpiReplica = AfsClass_CreateReplica (lpp->lpiSource, lpp->lpiTarget, &ptp->status)) == NULL)
      ptp->rc = FALSE;

   if (ptp->rc)
      {
      TASKDATA(ptp)->lpi = lpiReplica;
      }
   else
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpp->lpiTarget->GetServerName (szServer);
      lpp->lpiTarget->GetAggregateName (szAggregate);
      lpp->lpiSource->GetFilesetName (szFileset);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CREATE_REPLICA, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_Rename_Init (LPTASKPACKET ptp)
{
   LPSET_RENAME_INIT_PARAMS lpp = (LPSET_RENAME_INIT_PARAMS)(ptp->lpUser);

   LPFILESET lpFileset;
   if ((lpFileset = lpp->lpiReq->OpenFileset (&ptp->status)) == NULL)
      ptp->rc = FALSE;
   else
      {
      if ((lpp->lpiRW = lpFileset->GetReadWriteIdentifier (&ptp->status)) == NULL)
         ptp->rc = FALSE;
      lpFileset->Close();
      }

   // don't delete this packet (ptp->lpUser)!  the caller will free it.
}


void Task_Set_Rename_Apply (LPTASKPACKET ptp)
{
   LPSET_RENAME_APPLY_PARAMS lpp = (LPSET_RENAME_APPLY_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_RenameFileset (lpp->lpiFileset, lpp->szNewName, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szFileset[ cchNAME ];
      lpp->lpiFileset->GetFilesetName (szFileset);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_RENAME_FILESET, TEXT("%s%s"), szFileset, lpp->szNewName);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_Release (LPTASKPACKET ptp)
{
   LPSET_RELEASE_PARAMS lpp = (LPSET_RELEASE_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_ReleaseFileset (lpp->lpiRW, lpp->fForce, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpp->lpiRW->GetServerName (szServer);
      lpp->lpiRW->GetAggregateName (szAggregate);
      lpp->lpiRW->GetFilesetName (szFileset);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_RELEASE_FILESET, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      }

   Delete (lpp);
   ptp->lpUser = 0;  // we freed this; don't let anyone use it.
}


void Task_Set_Clone (LPTASKPACKET ptp)
{
   LPIDENT lpiRW = (LPIDENT)(ptp->lpUser);

   ptp->rc = AfsClass_Clone (lpiRW, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpiRW->GetServerName (szServer);
      lpiRW->GetAggregateName (szAggregate);
      lpiRW->GetFilesetName (szFileset);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CLONE, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      }
}


void Task_Set_Clonesys (LPTASKPACKET ptp)
{
   LPSET_CLONESYS_PARAMS lpp = (LPSET_CLONESYS_PARAMS)(ptp->lpUser);

   LPTSTR pszPrefix = (lpp->fUsePrefix) ? lpp->szPrefix : NULL;
   if (!AfsClass_CloneMultiple (lpp->lpi, pszPrefix, lpp->fExcludePrefix, &ptp->status))
      ptp->rc = FALSE;

   if (!ptp->rc && !IsWindow (ptp->hReply))
      {
      ErrorDialog (ptp->status, IDS_ERROR_CANT_CLONESYS);
      }

   Delete (lpp);
   ptp->lpUser = 0;  // we freed this; don't let anyone use it.
}


void Task_Set_Dump (LPTASKPACKET ptp)
{
   LPSET_DUMP_PARAMS lpp = (LPSET_DUMP_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_DumpFileset (lpp->lpi, lpp->szFilename,
                                   (lpp->fDumpByDate) ? &lpp->stDump : NULL,
                                   &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      lpp->lpi->GetServerName (szServer);
      lpp->lpi->GetAggregateName (szAggregate);
      lpp->lpi->GetFilesetName (szFileset);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_DUMP_FILESET, TEXT("%s%s%s%s"), szServer, szAggregate, szFileset, lpp->szFilename);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_Restore (LPTASKPACKET ptp)
{
   LPSET_RESTORE_PARAMS lpp = (LPSET_RESTORE_PARAMS)(ptp->lpUser);

   ptp->rc = AfsClass_RestoreFileset (lpp->lpi, lpp->szFileset, lpp->szFilename, lpp->fIncremental, &ptp->status);

   if (!ptp->rc)
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      lpp->lpi->GetServerName (szServer);
      lpp->lpi->GetAggregateName (szAggregate);
      ErrorDialog (ptp->status, IDS_ERROR_CANT_RESTORE_FILESET, TEXT("%s%s%s%s"), szServer, szAggregate, lpp->szFileset, lpp->szFilename);
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Set_Lookup (LPTASKPACKET ptp)
{
   LPSET_LOOKUP_PACKET lpp = (LPSET_LOOKUP_PACKET)(ptp->lpUser);

   if ((TASKDATA(ptp)->lpi = IDENT::FindFileset (g.lpiCell, lpp->szFileset)) != NULL)
      {
      LPFILESET lpFileset;
      if ((lpFileset = TASKDATA(ptp)->lpi->OpenFileset()) == NULL)
         TASKDATA(ptp)->lpi = NULL;  // fileset was probably deleted earlier
      else
         lpFileset->Close();
      }

   Delete (lpp);
   ptp->lpUser = NULL; // deleted this, so don't let the caller use it.
}


void Task_Expired_Creds (LPTASKPACKET ptp)
{
   if (g.lpiCell)
      {
      CheckForExpiredCredentials();
      }
}

