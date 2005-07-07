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
#include "action.h"
#include "window.h"
#include "messages.h"
#include "display.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cREALLOC_ACTIONLIST  4

typedef enum
   {
   actcolOPERATION,
   actcolELAPSED,
   } ACTIONCOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
ACTIONCOLUMNS[] =
   {
      { IDS_ACTCOL_OPERATION, 100 }, // actcolOPERATIONS
      { IDS_ACTCOL_ELAPSED,   100 }, // actcolELAPSED
   };

#define nACTIONCOLUMNS  (sizeof(ACTIONCOLUMNS)/sizeof(ACTIONCOLUMNS[0]))

#define cxMIN_ACTION  75
#define cyMIN_ACTION  50


/*
 * ACTION TYPES _______________________________________________________________
 *
 */

typedef enum
   {
   atUNUSED = 0,
   atOPENCELL,
   atSCOUT,
   atTIMEOUT,
   atREFRESH,
   atGETSERVERLOGFILE,
   atSETSERVERAUTH,
   atSTARTSERVICE,
   atSTOPSERVICE,
   atRESTARTSERVICE,
   atCREATEFILESET,
   atDELETEFILESET,
   atMOVEFILESET,
   atSETFILESETQUOTA,
   atSYNCVLDB,
   atSALVAGE,
   atSETFILESETREPPARAMS,
   atCREATEREPLICA,
   atPRUNEFILES,
   atINSTALLFILE,
   atUNINSTALLFILE,
   atRENAMEFILESET,
   atCREATESERVICE,
   atDELETESERVICE,
   atRELEASEFILESET,
   atGETFILEDATES,
   atEXECUTECOMMAND,
   atADMINLIST_LOAD,
   atADMINLIST_SAVE,
   atHOSTLIST_LOAD,
   atHOSTLIST_SAVE,
   atCLONE,
   atCLONESYS,
   atDUMP,
   atRESTORE,
   atSETRESTART,
   atCHANGEADDR,
   } ACTIONTYPE;

typedef struct
   {
   ACTIONTYPE Type;
   NOTIFYPARAMS Params;
   DWORD dwTickStart;
   HWND hDlg;
   } ACTION, *PACTION;


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct l
   {
   PACTION *aActions;
   size_t cActions;
   size_t cActionsInUse;
   BOOL fShowConfirmations;
   } l;

rwWindowData awdActions[] = {
    { IDC_ACTION_DESC,     raSizeX | raRepaint,	0,					0 },
    { IDC_ACTION_LIST,     raSizeX | raSizeY,   MAKELONG(cxMIN_ACTION,cyMIN_ACTION),	0 },
    { idENDLIST,           0,			0,					0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void TicksToElapsedTime (LPSYSTEMTIME pst, DWORD dwTicks);
void CopyFirstWord (LPTSTR pszTarget, LPTSTR pszSource);

BOOL CALLBACK Action_Window_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Action_Window_Refresh (void);
LPTSTR Action_GetDescription (size_t ii);

BOOL Action_CompareNotifyParams (PNOTIFYPARAMS pParams1, PNOTIFYPARAMS pParams2);
PACTION Action_Begin (ACTIONTYPE Type, PNOTIFYPARAMS pParams);
PACTION Action_Find (ACTIONTYPE Type, PNOTIFYPARAMS pParams);
void Action_OnDestroy (HWND hDlg);
void Action_SpecifyWindow (PACTION pParams, HWND hDlg);
void Action_End (PACTION pParams, DWORD dwStatus);

void Action_GetServerLogFile_Begin (PNOTIFYPARAMS pParams);
void Action_GetServerLogFile_End (PNOTIFYPARAMS pParams);
void Action_SetServerAuth_Begin (PNOTIFYPARAMS pParams);
void Action_SetServerAuth_End (PNOTIFYPARAMS pParams);
void Action_InstallFile_Begin (PNOTIFYPARAMS pParams);
void Action_InstallFile_End (PNOTIFYPARAMS pParams);
void Action_UninstallFile_Begin (PNOTIFYPARAMS pParams);
void Action_UninstallFile_End (PNOTIFYPARAMS pParams);
void Action_PruneFiles_Begin (PNOTIFYPARAMS pParams);
void Action_PruneFiles_End (PNOTIFYPARAMS pParams);
void Action_StartService_Begin (PNOTIFYPARAMS pParams);
void Action_StartService_End (PNOTIFYPARAMS pParams);
void Action_StopService_Begin (PNOTIFYPARAMS pParams);
void Action_StopService_End (PNOTIFYPARAMS pParams);
void Action_RestartService_Begin (PNOTIFYPARAMS pParams);
void Action_RestartService_End (PNOTIFYPARAMS pParams);
void Action_CreateFileset_Begin (PNOTIFYPARAMS pParams);
void Action_CreateFileset_End (PNOTIFYPARAMS pParams);
void Action_DeleteFileset_Begin (PNOTIFYPARAMS pParams);
void Action_DeleteFileset_End (PNOTIFYPARAMS pParams);
void Action_SetFilesetQuota_Begin (PNOTIFYPARAMS pParams);
void Action_SetFilesetQuota_End (PNOTIFYPARAMS pParams);
void Action_SyncVLDB_Begin (PNOTIFYPARAMS pParams);
void Action_SyncVLDB_End (PNOTIFYPARAMS pParams);
void Action_Salvage_Begin (PNOTIFYPARAMS pParams);
void Action_Salvage_End (PNOTIFYPARAMS pParams);
void Action_Scout_Begin (PNOTIFYPARAMS pParams);
void Action_Scout_End (PNOTIFYPARAMS pParams);
void Action_SetFilesetRepParams_Begin (PNOTIFYPARAMS pParams);
void Action_SetFilesetRepParams_End (PNOTIFYPARAMS pParams);
void Action_CreateReplica_Begin (PNOTIFYPARAMS pParams);
void Action_CreateReplica_End (PNOTIFYPARAMS pParams);
void Action_RenameFileset_Begin (PNOTIFYPARAMS pParams);
void Action_RenameFileset_End (PNOTIFYPARAMS pParams);
void Action_CreateService_Begin (PNOTIFYPARAMS pParams);
void Action_CreateService_End (PNOTIFYPARAMS pParams);
void Action_DeleteService_Begin (PNOTIFYPARAMS pParams);
void Action_DeleteService_End (PNOTIFYPARAMS pParams);
void Action_ReleaseFileset_Begin (PNOTIFYPARAMS pParams);
void Action_ReleaseFileset_End (PNOTIFYPARAMS pParams);
void Action_GetFileDates_Begin (PNOTIFYPARAMS pParams);
void Action_GetFileDates_End (PNOTIFYPARAMS pParams);
void Action_ExecuteCommand_Begin (PNOTIFYPARAMS pParams);
void Action_ExecuteCommand_End (PNOTIFYPARAMS pParams);
void Action_AdminListLoad_Begin (PNOTIFYPARAMS pParams);
void Action_AdminListLoad_End (PNOTIFYPARAMS pParams);
void Action_AdminListSave_Begin (PNOTIFYPARAMS pParams);
void Action_AdminListSave_End (PNOTIFYPARAMS pParams);
void Action_HostListLoad_Begin (PNOTIFYPARAMS pParams);
void Action_HostListLoad_End (PNOTIFYPARAMS pParams);
void Action_HostListSave_Begin (PNOTIFYPARAMS pParams);
void Action_HostListSave_End (PNOTIFYPARAMS pParams);
void Action_Clone_Begin (PNOTIFYPARAMS pParams);
void Action_Clone_End (PNOTIFYPARAMS pParams);
void Action_Clonesys_Begin (PNOTIFYPARAMS pParams);
void Action_Clonesys_End (PNOTIFYPARAMS pParams);
void Action_SetRestart_Begin (PNOTIFYPARAMS pParams);
void Action_SetRestart_End (PNOTIFYPARAMS pParams);
void Action_ChangeAddr_Begin (PNOTIFYPARAMS pParams);
void Action_ChangeAddr_End (PNOTIFYPARAMS pParams);

void Action_Refresh_Begin (PNOTIFYPARAMS pParams);
void Action_Refresh_Update (PNOTIFYPARAMS pParams);
void Action_Refresh_SectionStart (PNOTIFYPARAMS pParams);
void Action_Refresh_SectionEnd (PNOTIFYPARAMS pParams);
void Action_Refresh_End (PNOTIFYPARAMS pParams);
void Action_Refresh_UpdateText (HWND hDlg, LPIDENT lpi, DWORD dwPerComplete);
void Action_Refresh_SetSection (HWND hDlg, BOOL fStart, int idSection);
void Action_Refresh_SkipSection (HWND hDlg);
BOOL CALLBACK Action_Refresh_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Action_MoveFileset_Begin (PNOTIFYPARAMS pParams);
void Action_MoveFileset_End (PNOTIFYPARAMS pParams);
BOOL CALLBACK Action_MoveFileset_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Action_DumpFileset_Begin (PNOTIFYPARAMS pParams);
void Action_DumpFileset_End (PNOTIFYPARAMS pParams);
BOOL CALLBACK Action_DumpFileset_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Action_RestoreFileset_Begin (PNOTIFYPARAMS pParams);
void Action_RestoreFileset_End (PNOTIFYPARAMS pParams);
BOOL CALLBACK Action_RestoreFileset_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Action_OpenCell_Begin (PNOTIFYPARAMS pParams);
void Action_OpenCell_End (PNOTIFYPARAMS pParams);
BOOL CALLBACK Action_OpenCell_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void ActionNotification_MainThread (NOTIFYEVENT evt, PNOTIFYPARAMS pParams)
{
   BOOL fRefresh = TRUE;

   switch (evt)
      {

      // When Scout starts and stops checking a server, it'll notify us.
      //
      case evtScoutBegin:                Action_Scout_Begin (pParams);                  break;
      case evtScoutEnd:                  Action_Scout_End (pParams);                    break;

      // If AFSClass is going to perform a lengthy refresh, it will send
      // us notifications for when it starts, when it's done, and periodic
      // updates about how far along it is.  Use that information to present
      // a dialog to the user...
      //
      case evtRefreshAllBegin:           Action_Refresh_Begin (pParams);                break;
      case evtRefreshAllUpdate:          Action_Refresh_Update (pParams);               break;
      case evtRefreshAllSectionStart:    Action_Refresh_SectionStart (pParams);         break;
      case evtRefreshAllSectionEnd:      Action_Refresh_SectionEnd (pParams);           break;
      case evtRefreshAllEnd:             Action_Refresh_End(pParams);                   break;

      case evtGetServerLogFileBegin:     Action_GetServerLogFile_Begin (pParams);       break;
      case evtGetServerLogFileEnd:       Action_GetServerLogFile_End (pParams);         break;
      case evtSetServerAuthBegin:        Action_SetServerAuth_Begin (pParams);          break;
      case evtSetServerAuthEnd:          Action_SetServerAuth_End (pParams);            break;
      case evtInstallFileBegin:          Action_InstallFile_Begin (pParams);            break;
      case evtInstallFileEnd:            Action_InstallFile_End (pParams);              break;
      case evtUninstallFileBegin:        Action_UninstallFile_Begin (pParams);          break;
      case evtUninstallFileEnd:          Action_UninstallFile_End (pParams);            break;
      case evtPruneFilesBegin:           Action_PruneFiles_Begin (pParams);             break;
      case evtPruneFilesEnd:             Action_PruneFiles_End (pParams);               break;
      case evtStartServiceBegin:         Action_StartService_Begin (pParams);           break;
      case evtStartServiceEnd:           Action_StartService_End (pParams);             break;
      case evtStopServiceBegin:          Action_StopService_Begin (pParams);            break;
      case evtStopServiceEnd:            Action_StopService_End (pParams);              break;
      case evtRestartServiceBegin:       Action_RestartService_Begin (pParams);         break;
      case evtRestartServiceEnd:         Action_RestartService_End (pParams);           break;
      case evtCreateFilesetBegin:        Action_CreateFileset_Begin (pParams);          break;
      case evtCreateFilesetEnd:          Action_CreateFileset_End (pParams);            break;
      case evtDeleteFilesetBegin:        Action_DeleteFileset_Begin (pParams);          break;
      case evtDeleteFilesetEnd:          Action_DeleteFileset_End (pParams);            break;
      case evtMoveFilesetBegin:          Action_MoveFileset_Begin (pParams);            break;
      case evtMoveFilesetEnd:            Action_MoveFileset_End (pParams);              break;
      case evtSetFilesetQuotaBegin:      Action_SetFilesetQuota_Begin (pParams);        break;
      case evtSetFilesetQuotaEnd:        Action_SetFilesetQuota_End (pParams);          break;
      case evtSyncVLDBBegin:             Action_SyncVLDB_Begin (pParams);               break;
      case evtSyncVLDBEnd:               Action_SyncVLDB_End (pParams);                 break;
      case evtSalvageBegin:              Action_Salvage_Begin (pParams);                break;
      case evtSalvageEnd:                Action_Salvage_End (pParams);                  break;
      case evtSetFilesetRepParamsBegin:  Action_SetFilesetRepParams_Begin (pParams);    break;
      case evtSetFilesetRepParamsEnd:    Action_SetFilesetRepParams_End (pParams);      break;
      case evtCreateReplicaBegin:        Action_CreateReplica_Begin (pParams);          break;
      case evtCreateReplicaEnd:          Action_CreateReplica_End (pParams);            break;
      case evtRenameFilesetBegin:        Action_RenameFileset_Begin (pParams);          break;
      case evtRenameFilesetEnd:          Action_RenameFileset_End (pParams);            break;
      case evtCreateServiceBegin:        Action_CreateService_Begin (pParams);          break;
      case evtCreateServiceEnd:          Action_CreateService_End (pParams);            break;
      case evtDeleteServiceBegin:        Action_DeleteService_Begin (pParams);          break;
      case evtDeleteServiceEnd:          Action_DeleteService_End (pParams);            break;
      case evtReleaseFilesetBegin:       Action_ReleaseFileset_Begin (pParams);         break;
      case evtReleaseFilesetEnd:         Action_ReleaseFileset_End (pParams);           break;
      case evtGetFileDatesBegin:         Action_GetFileDates_Begin (pParams);           break;
      case evtGetFileDatesEnd:           Action_GetFileDates_End (pParams);             break;
      case evtExecuteCommandBegin:       Action_ExecuteCommand_Begin (pParams);         break;
      case evtExecuteCommandEnd:         Action_ExecuteCommand_End (pParams);           break;
      case evtAdminListLoadBegin:        Action_AdminListLoad_Begin (pParams);          break;
      case evtAdminListLoadEnd:          Action_AdminListLoad_End (pParams);            break;
      case evtAdminListSaveBegin:        Action_AdminListSave_Begin (pParams);          break;
      case evtAdminListSaveEnd:          Action_AdminListSave_End (pParams);            break;
      case evtHostListLoadBegin:         Action_HostListLoad_Begin (pParams);           break;
      case evtHostListLoadEnd:           Action_HostListLoad_End (pParams);             break;
      case evtHostListSaveBegin:         Action_HostListSave_Begin (pParams);           break;
      case evtHostListSaveEnd:           Action_HostListSave_End (pParams);             break;
      case evtCloneBegin:                Action_Clone_Begin (pParams);                  break;
      case evtCloneEnd:                  Action_Clone_End (pParams);                    break;
      case evtCloneMultipleBegin:        Action_Clonesys_Begin (pParams);               break;
      case evtCloneMultipleEnd:          Action_Clonesys_End (pParams);                 break;
      case evtDumpFilesetBegin:          Action_DumpFileset_Begin (pParams);            break;
      case evtDumpFilesetEnd:            Action_DumpFileset_End (pParams);              break;
      case evtRestoreFilesetBegin:       Action_RestoreFileset_Begin (pParams);         break;
      case evtRestoreFilesetEnd:         Action_RestoreFileset_End (pParams);           break;
      case evtSetRestartTimesBegin:      Action_SetRestart_Begin (pParams);             break;
      case evtSetRestartTimesEnd:        Action_SetRestart_End (pParams);               break;
      case evtChangeAddressBegin:        Action_ChangeAddr_Begin (pParams);             break;
      case evtChangeAddressEnd:          Action_ChangeAddr_End (pParams);               break;
      case evtOpenCellBegin:             Action_OpenCell_Begin (pParams);               break;
      case evtOpenCellEnd:               Action_OpenCell_End (pParams);                 break;

      default:
         fRefresh = FALSE;
      }

   if (fRefresh)
      Action_Window_Refresh();
}


void TicksToElapsedTime (LPSYSTEMTIME pst, DWORD dwTicks)
{
#define msecSECOND  (1000L)
#define msecMINUTE  (1000L * 60L)
#define msecHOUR    (1000L * 60L * 60L)
#define msecDAY     (1000L * 60L * 60L * 24L)

   memset (pst, 0x00, sizeof(SYSTEMTIME));

   pst->wDay = (int)( dwTicks / msecDAY );
   dwTicks %= msecDAY;
   pst->wHour = (int)( dwTicks / msecHOUR );
   dwTicks %= msecHOUR;
   pst->wMinute = (int)( dwTicks / msecMINUTE );
   dwTicks %= msecMINUTE;
   pst->wSecond = (int)( dwTicks / msecSECOND );
   dwTicks %= msecSECOND;
   pst->wMilliseconds = (int)( dwTicks );
}


void CopyFirstWord (LPTSTR pszTarget, LPTSTR pszSource)
{
   lstrcpy (pszTarget, pszSource);

   LPTSTR pch;
   if ((pch = (LPTSTR)lstrchr (pszTarget, TEXT(' '))) != NULL)
      *pch = TEXT('\0');
   if ((pch = (LPTSTR)lstrchr (pszTarget, TEXT('\t'))) != NULL)
      *pch = TEXT('\0');
}


void Action_OpenWindow (void)
{
   if (!g.hAction || !IsWindow (g.hAction))
      {
      g.hAction = ModelessDialog (IDD_ACTIONS, NULL, (DLGPROC)Action_Window_DlgProc);
      ShowWindow (g.hAction, SW_SHOW);
      }
}


void Action_CloseWindow (void)
{
   if (g.hAction && IsWindow (g.hAction))
      {
      DestroyWindow (g.hAction);
      }
}


BOOL CALLBACK Action_Window_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewAct))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         g.hAction = hDlg;
         HWND hList;
         hList = GetDlgItem (hDlg, IDC_ACTION_LIST);
         FL_RestoreView (hList, &gr.viewAct);

         if (gr.rActions.right == 0)
            GetWindowRect (hDlg, &gr.rActions);
         ResizeWindow (hDlg, awdActions, rwaMoveToHere, &gr.rActions);

         Action_Window_Refresh();
         SetTimer (hDlg, ID_ACTION_TIMER, 1000, NULL);  // timer message every sec

         SetWindowPos (g.hAction, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
         break;

      case WM_DESTROY:
         gr.fActions = FALSE;
         g.hAction = NULL;
         Main_SetActionMenus();
         FL_StoreView (GetDlgItem (hDlg, IDC_ACTION_LIST), &gr.viewAct);
         KillTimer (hDlg, ID_ACTION_TIMER);
         break;

      case WM_TIMER:
         if (FastList_GetItemCount (GetDlgItem (hDlg, IDC_ACTION_LIST)))
            Action_Window_Refresh();
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            {
            ResizeWindow (hDlg, awdActions, rwaFixupGuts);
            GetWindowRect (hDlg, &gr.rActions);
            }
         break;

      case WM_MOVE:
         GetWindowRect (hDlg, &gr.rActions);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Action_WindowToTop (BOOL fTop)
{
   if (g.hAction && IsWindow(g.hAction))
      {
      if (fTop)
         {
         SetWindowPos (g.hAction, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
         }
      else //(!fTop)
         {
         SetWindowPos (g.hAction, g.hMain, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
         }
      }
}


void Action_Window_Refresh (void)
{
   size_t nItems = 0;
   TCHAR szText[ cchRESOURCE ];

   HWND hList = GetDlgItem (g.hAction, IDC_ACTION_LIST);
   if (hList != NULL)
      {
      LPARAM lpOld = FL_StartChange (hList, TRUE);

      for (size_t ii = 0; ii < l.cActions; ++ii)
         {
         LPTSTR pszDesc;
         if ((pszDesc = Action_GetDescription (ii)) != NULL)
            {
            SYSTEMTIME st;
            TicksToElapsedTime (&st, GetTickCount() - l.aActions[ ii ]->dwTickStart);
            FormatElapsed (szText, TEXT("%s"), &st);
            FL_AddItem (hList, &gr.viewAct, (LPARAM)ii, IMAGE_NOIMAGE, pszDesc, szText);
            ++nItems;

            FreeString (pszDesc);
            }
         }

      FL_EndChange (hList, lpOld);
      }

   if (nItems == 0)
      GetString (szText, IDS_ACTION_DESC_NONE);
   else if (nItems == 1)
      GetString (szText, IDS_ACTION_DESC_ONE);
   else // (nItems >= 2)
      GetString (szText, IDS_ACTION_DESC_MULT);
   SetDlgItemText (g.hAction, IDC_ACTION_DESC, szText);
}


LPTSTR Action_GetDescription (size_t ii)
{
   TCHAR szText[ cchRESOURCE ];
   TCHAR szText2[ cchRESOURCE ];
   TCHAR szText3[ cchRESOURCE ];
   LPTSTR pszDesc = NULL;

   if (!l.aActions[ ii ])
      return NULL;

   switch (l.aActions[ ii ]->Type)
      {
      case atSCOUT:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_SCOUT, TEXT("%s"), szText);
         break;

      case atREFRESH:
         pszDesc = FormatString (IDS_ACTION_REFRESH);
         break;

      case atGETSERVERLOGFILE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_GETSERVERLOGFILE, TEXT("%s%s"), szText, l.aActions[ ii ]->Params.sz1);
         break;

      case atSETSERVERAUTH:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_SETSERVERAUTH, TEXT("%s"), szText);
         break;

      case atINSTALLFILE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         CopyBaseFileName (szText2, l.aActions[ ii ]->Params.sz1);
         pszDesc = FormatString (IDS_ACTION_INSTALLFILE, TEXT("%s%s"), szText, szText2);
         break;

      case atUNINSTALLFILE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         CopyBaseFileName (szText2, l.aActions[ ii ]->Params.sz1);
         pszDesc = FormatString (IDS_ACTION_UNINSTALLFILE, TEXT("%s%s"), szText, szText2);
         break;

      case atPRUNEFILES:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_PRUNEFILES, TEXT("%s"), szText);
         break;

      case atSTARTSERVICE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetServiceName (szText2);
         pszDesc = FormatString (IDS_ACTION_STARTSERVICE, TEXT("%s%s"), szText, szText2);
         break;

      case atSTOPSERVICE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetServiceName (szText2);
         pszDesc = FormatString (IDS_ACTION_STOPSERVICE, TEXT("%s%s"), szText, szText2);
         break;

      case atRESTARTSERVICE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetServiceName (szText2);
         pszDesc = FormatString (IDS_ACTION_RESTARTSERVICE, TEXT("%s%s"), szText, szText2);
         break;

      case atCREATEFILESET:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
         pszDesc = FormatString (IDS_ACTION_CREATEFILESET, TEXT("%s%s%s"), szText, szText2, l.aActions[ ii ]->Params.sz1);
         break;

      case atDELETEFILESET:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_DELETEFILESET, TEXT("%s%s%s"), szText, szText2, szText3);
         break;

      case atMOVEFILESET:
         l.aActions[ ii ]->Params.lpi2->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi2->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_MOVEFILESET, TEXT("%s%s%s"), szText, szText2, szText3);
         break;

      case atSETFILESETQUOTA:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_SETFILESETQUOTA, TEXT("%s%s%s"), szText, szText2, szText3);
         break;

      case atSETFILESETREPPARAMS:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_SETREPPARAMS, TEXT("%s%s%s"), szText, szText2, szText3);
         break;

      case atCREATEREPLICA:
         l.aActions[ ii ]->Params.lpi2->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi2->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_CREATEREPLICA, TEXT("%s%s%s"), szText, szText2, szText3);
         break;

      case atSYNCVLDB:
         if (l.aActions[ ii ]->Params.lpi1->fIsServer())
            {
            l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
            pszDesc = FormatString (IDS_ACTION_SYNCVLDB_SVR, TEXT("%s"), szText);
            }
         else
            {
            l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
            l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
            pszDesc = FormatString (IDS_ACTION_SYNCVLDB_AGG, TEXT("%s%s"), szText, szText2);
            }
         break;

      case atSALVAGE:
         if (l.aActions[ ii ]->Params.lpi1->fIsServer())
            {
            l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
            pszDesc = FormatString (IDS_ACTION_SALVAGE_SVR, TEXT("%s"), szText);
            }
         else if (l.aActions[ ii ]->Params.lpi1->fIsAggregate())
            {
            l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
            l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
            pszDesc = FormatString (IDS_ACTION_SALVAGE_AGG, TEXT("%s%s"), szText, szText2);
            }
         else // (l.aActions[ ii ]->Params.lpi1->fIsFileset())
            {
            l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
            l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
            l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
            pszDesc = FormatString (IDS_ACTION_SALVAGE_VOL, TEXT("%s%s%s"), szText, szText2, szText3);
            }
         break;

      case atRENAMEFILESET:
         pszDesc = FormatString (IDS_ACTION_RENAMEFILESET, TEXT("%s%s"), l.aActions[ ii ]->Params.sz1, l.aActions[ ii ]->Params.sz2);
         break;

      case atCREATESERVICE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_CREATESERVICE, TEXT("%s%s"), szText, l.aActions[ ii ]->Params.sz1);
         break;

      case atDELETESERVICE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetServiceName (szText2);
         pszDesc = FormatString (IDS_ACTION_DELETESERVICE, TEXT("%s%s"), szText, szText2);
         break;

      case atRELEASEFILESET:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_RELEASEFILESET, TEXT("%s%s%s"), szText, szText2, szText3);
         break;

      case atGETFILEDATES:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         CopyBaseFileName (szText2, l.aActions[ ii ]->Params.sz1);
         pszDesc = FormatString (IDS_ACTION_GETDATES, TEXT("%s%s"), szText, szText2);
         break;

      case atEXECUTECOMMAND:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         CopyFirstWord (szText2, l.aActions[ ii ]->Params.sz1);
         pszDesc = FormatString (IDS_ACTION_EXECUTE, TEXT("%s%s"), szText, szText2);
         break;

      case atADMINLIST_LOAD:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_ADMINLIST_LOAD, TEXT("%s"), szText);
         break;

      case atADMINLIST_SAVE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_ADMINLIST_SAVE, TEXT("%s"), szText);
         break;

      case atHOSTLIST_LOAD:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_HOSTLIST_LOAD, TEXT("%s"), szText);
         break;

      case atHOSTLIST_SAVE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_HOSTLIST_SAVE, TEXT("%s"), szText);
         break;

      case atCLONE:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_CLONE, TEXT("%s%s%s"), szText, szText2, szText3);
         break;

      case atCLONESYS:
         pszDesc = FormatString (IDS_ACTION_CLONESYS);
         break;

      case atDUMP:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         l.aActions[ ii ]->Params.lpi1->GetAggregateName (szText2);
         l.aActions[ ii ]->Params.lpi1->GetFilesetName (szText3);
         pszDesc = FormatString (IDS_ACTION_DUMP, TEXT("%s%s%s%s"), szText, szText2, szText3, FindBaseFileName(l.aActions[ ii ]->Params.sz1));
         break;

      case atRESTORE:
         pszDesc = FormatString (IDS_ACTION_RESTORE, TEXT("%s%s"), l.aActions[ ii ]->Params.sz1, FindBaseFileName(l.aActions[ ii ]->Params.sz2));
         break;

      case atSETRESTART:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_SETRESTART, TEXT("%s"), szText);
         break;

      case atCHANGEADDR:
         l.aActions[ ii ]->Params.lpi1->GetServerName (szText);
         pszDesc = FormatString (IDS_ACTION_CHANGEADDR, TEXT("%s"), szText);
         break;

      case atOPENCELL:
         pszDesc = FormatString (IDS_ACTION_OPENCELL, TEXT("%s"), l.aActions[ ii ]->Params.sz1);
         break;
      }

   return pszDesc;
}


void Action_SetDefaultView (LPVIEWINFO lpvi)
{
   lpvi->lvsView = FLS_VIEW_LIST;
   lpvi->nColsAvail = nACTIONCOLUMNS;

   for (size_t iCol = 0; iCol < nACTIONCOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = ACTIONCOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = ACTIONCOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = actcolELAPSED | COLUMN_SORTREV;

   lpvi->nColsShown = 2;
   lpvi->aColumns[0] = (int)actcolOPERATION;
   lpvi->aColumns[1] = (int)actcolELAPSED;
}


void Action_ShowConfirmations (BOOL fShow)
{
   l.fShowConfirmations = fShow;
}


BOOL Action_CompareNotifyParams (PNOTIFYPARAMS pParams1, PNOTIFYPARAMS pParams2)
{
   if (!pParams1 || !pParams2)
      return FALSE;

   if (pParams1->lpi1 != pParams2->lpi1)
      return FALSE;
   if (pParams1->lpi2 != pParams2->lpi2)
      return FALSE;
   if (lstrcmp (pParams1->sz1, pParams2->sz1))
      return FALSE;
   if (lstrcmp (pParams1->sz2, pParams2->sz2))
      return FALSE;
   if (pParams1->dw1 != pParams2->dw1)
      return FALSE;

   return TRUE;
}


PACTION Action_Begin (ACTIONTYPE Type, PNOTIFYPARAMS pParams)
{
   size_t ii;
   for (ii = 0; ii < l.cActions; ++ii)
      {
      if (!l.aActions[ii])
         break;
      }
   if (!REALLOC (l.aActions, l.cActions, 1+ii, cREALLOC_ACTIONLIST))
      return NULL;

   PACTION pAction = New (ACTION);
   memcpy (&pAction->Params, pParams, sizeof(NOTIFYPARAMS));
   pAction->Type = Type;
   pAction->dwTickStart = GetTickCount();
   pAction->hDlg = NULL;

   l.aActions[ii] = pAction;

   if (Type != atREFRESH)
      l.cActionsInUse++;

   return l.aActions[ii];
}


void Action_SpecifyWindow (PACTION pAction, HWND hDlg)
{
   pAction->hDlg = hDlg;
   ShowWindow (hDlg, SW_SHOW);
}


PACTION Action_Find (ACTIONTYPE Type, PNOTIFYPARAMS pParams)
{
   for (size_t ii = 0; ii < l.cActions; ++ii)
      {
      if (!l.aActions[ ii ])
         continue;

      if (l.aActions[ ii ]->Type != Type)
         continue;

      if (Action_CompareNotifyParams (&l.aActions[ ii ]->Params, pParams))
         return l.aActions[ ii ];
      }

   return NULL;
}


void Action_End (PACTION pParams, DWORD dwStatus)
{
   if (!pParams)
      return;

   ACTIONTYPE TypeClosed = atUNUSED;

   for (size_t ii = 0; ii < l.cActions; ++ii)
      {
      if (l.aActions[ ii ] == pParams)
         {
         if (l.fShowConfirmations && !dwStatus)
            {
            switch (l.aActions[ ii ]->Type)
               {
               case atUNUSED:
               case atSCOUT:
               case atTIMEOUT:
               case atREFRESH:
                  // don't show confirmation messages for these operations
                  break;

               default:
                  LPTSTR pszDesc;
                  if ((pszDesc = Action_GetDescription (ii)) != NULL)
                     {
                     Message (MB_OK | MB_ICONINFORMATION | MB_MODELESS, IDS_CONFIRMATION_TITLE, TEXT("%1"), TEXT("%s"), pszDesc);
                     FreeString (pszDesc);
                     }
                  break;
               }
            }

         TypeClosed = l.aActions[ ii ]->Type;

         if (IsWindow (l.aActions[ ii ]->hDlg))
            PostMessage (l.aActions[ ii ]->hDlg, WM_COMMAND, IDCANCEL, 0);

         Delete (l.aActions[ii]);
         l.aActions[ii] = NULL;
         }
      }

   // If we've finished all operations in progress, and if the user has
   // already closed the app (well, tried to--we would've just hidden the
   // window), close for real.
   //
   if ((TypeClosed != atUNUSED) && (TypeClosed != atREFRESH))
      {
      if ((--l.cActionsInUse) == 0)
         {
         if (g.lpiCell && !IsWindowVisible (g.hMain))
            {
            Quit (0);
            }
         }
      }
}


BOOL Action_fAnyActive (void)
{
   return (l.cActionsInUse >= 1) ? TRUE : FALSE;
}


void Action_OnDestroy (HWND hDlg)
{
   for (size_t ii = 0; ii < l.cActions; ++ii)
      {
      if (!l.aActions[ ii ])
         continue;
      if (l.aActions[ ii ]->hDlg == hDlg)
         l.aActions[ ii ]->hDlg = NULL;
      }
}


/*
 * NO-DIALOG OPERATIONS
 *
 */

void Action_Scout_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSCOUT, pParams);
}

void Action_Scout_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSCOUT, pParams), pParams->status);
}

void Action_GetServerLogFile_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atGETSERVERLOGFILE, pParams);
}

void Action_GetServerLogFile_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atGETSERVERLOGFILE, pParams), pParams->status);
}

void Action_SetServerAuth_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSETSERVERAUTH, pParams);
}

void Action_SetServerAuth_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSETSERVERAUTH, pParams), pParams->status);
}

void Action_InstallFile_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atINSTALLFILE, pParams);
}

void Action_InstallFile_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atINSTALLFILE, pParams), pParams->status);
}

void Action_UninstallFile_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atUNINSTALLFILE, pParams);
}

void Action_UninstallFile_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atUNINSTALLFILE, pParams), pParams->status);
}

void Action_PruneFiles_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atPRUNEFILES, pParams);
}

void Action_PruneFiles_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atPRUNEFILES, pParams), pParams->status);
}

void Action_StartService_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSTARTSERVICE, pParams);
}

void Action_StartService_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSTARTSERVICE, pParams), pParams->status);
}

void Action_StopService_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSTOPSERVICE, pParams);
}

void Action_StopService_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSTOPSERVICE, pParams), pParams->status);
}

void Action_RestartService_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atRESTARTSERVICE, pParams);
}

void Action_RestartService_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atRESTARTSERVICE, pParams), pParams->status);
}

void Action_CreateFileset_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atCREATEFILESET, pParams);
}

void Action_CreateFileset_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atCREATEFILESET, pParams), pParams->status);
}

void Action_DeleteFileset_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atDELETEFILESET, pParams);
}

void Action_DeleteFileset_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atDELETEFILESET, pParams), pParams->status);
}

void Action_SetFilesetQuota_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSETFILESETQUOTA, pParams);
}

void Action_SetFilesetQuota_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSETFILESETQUOTA, pParams), pParams->status);
}

void Action_SyncVLDB_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSYNCVLDB, pParams);
}

void Action_SyncVLDB_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSYNCVLDB, pParams), pParams->status);
}

void Action_Salvage_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSALVAGE, pParams);
}

void Action_Salvage_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSALVAGE, pParams), pParams->status);
}

void Action_SetFilesetRepParams_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSETFILESETREPPARAMS, pParams);
}

void Action_SetFilesetRepParams_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSETFILESETREPPARAMS, pParams), pParams->status);
}

void Action_CreateReplica_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atCREATEREPLICA, pParams);
}

void Action_CreateReplica_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atCREATEREPLICA, pParams), pParams->status);
}

void Action_RenameFileset_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atRENAMEFILESET, pParams);
}

void Action_RenameFileset_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atRENAMEFILESET, pParams), pParams->status);
}

void Action_CreateService_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atCREATESERVICE, pParams);
}

void Action_CreateService_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atCREATESERVICE, pParams), pParams->status);
}

void Action_DeleteService_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atDELETESERVICE, pParams);
}

void Action_DeleteService_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atDELETESERVICE, pParams), pParams->status);
}

void Action_ReleaseFileset_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atRELEASEFILESET, pParams);
}

void Action_ReleaseFileset_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atRELEASEFILESET, pParams), pParams->status);
}

void Action_GetFileDates_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atGETFILEDATES, pParams);
}

void Action_GetFileDates_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atGETFILEDATES, pParams), pParams->status);
}

void Action_ExecuteCommand_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atEXECUTECOMMAND, pParams);
}

void Action_ExecuteCommand_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atEXECUTECOMMAND, pParams), pParams->status);
}

void Action_AdminListLoad_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atADMINLIST_LOAD, pParams);
}

void Action_AdminListLoad_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atADMINLIST_LOAD, pParams), pParams->status);
}

void Action_AdminListSave_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atADMINLIST_SAVE, pParams);
}

void Action_AdminListSave_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atADMINLIST_SAVE, pParams), pParams->status);
}

void Action_HostListLoad_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atHOSTLIST_LOAD, pParams);
}

void Action_HostListLoad_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atHOSTLIST_LOAD, pParams), pParams->status);
}

void Action_HostListSave_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atHOSTLIST_SAVE, pParams);
}

void Action_HostListSave_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atHOSTLIST_SAVE, pParams), pParams->status);
}

void Action_Clone_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atCLONE, pParams);
}

void Action_Clone_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atCLONE, pParams), pParams->status);
}

void Action_Clonesys_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atCLONESYS, pParams);
}

void Action_Clonesys_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atCLONESYS, pParams), pParams->status);
}

void Action_SetRestart_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atSETRESTART, pParams);
}

void Action_SetRestart_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atSETRESTART, pParams), pParams->status);
}

void Action_ChangeAddr_Begin (PNOTIFYPARAMS pParams)
{
   Action_Begin (atCHANGEADDR, pParams);
}

void Action_ChangeAddr_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atCHANGEADDR, pParams), pParams->status);
}


/*
 * REFRESHALL DIALOGS
 *
 */

void Action_Refresh_Begin (PNOTIFYPARAMS pParams)
{
   NOTIFYPARAMS ZeroParams;
   memset (&ZeroParams, 0x00, sizeof(NOTIFYPARAMS));

   PACTION pAction;
   if ((pAction = Action_Find (atREFRESH, &ZeroParams)) == NULL)
      {
      if ((pAction = Action_Begin (atREFRESH, &ZeroParams)) != NULL)
         {
         HWND hDlg = ModelessDialogParam (IDD_REFRESHALL, NULL, (DLGPROC)Action_Refresh_DlgProc, (LPARAM)(pParams->lpi1));

         if (hDlg == NULL)
            Action_End (pAction, pParams->status);
         else
            Action_SpecifyWindow (pAction, hDlg);
         }
      }
}


void Action_Refresh_Update (PNOTIFYPARAMS pParams)
{
   NOTIFYPARAMS ZeroParams;
   memset (&ZeroParams, 0x00, sizeof(NOTIFYPARAMS));

   PACTION pAction;
   if ((pAction = Action_Find (atREFRESH, &ZeroParams)) != NULL)
      {
      if (pAction->hDlg && IsWindow(pAction->hDlg))
         PostMessage (pAction->hDlg, WM_REFRESH_UPDATE, (WPARAM)pParams->dw1, (LPARAM)pParams->lpi1);
      }
}


void Action_Refresh_SectionStart (PNOTIFYPARAMS pParams)
{
   NOTIFYPARAMS ZeroParams;
   memset (&ZeroParams, 0x00, sizeof(NOTIFYPARAMS));

   PACTION pAction;
   if ((pAction = Action_Find (atREFRESH, &ZeroParams)) != NULL)
      {
      if (pAction->hDlg && IsWindow(pAction->hDlg))
         PostMessage (pAction->hDlg, WM_REFRESH_SETSECTION, (WPARAM)TRUE, (LPARAM)pParams->dw1);
      }
}


void Action_Refresh_SectionEnd (PNOTIFYPARAMS pParams)
{
   NOTIFYPARAMS ZeroParams;
   memset (&ZeroParams, 0x00, sizeof(NOTIFYPARAMS));

   PACTION pAction;
   if ((pAction = Action_Find (atREFRESH, &ZeroParams)) != NULL)
      {
      if (pAction->hDlg && IsWindow(pAction->hDlg))
         PostMessage (pAction->hDlg, WM_REFRESH_SETSECTION, (WPARAM)FALSE, (LPARAM)pParams->dw1);
      }
}


void Action_Refresh_End (PNOTIFYPARAMS pParams)
{
   NOTIFYPARAMS ZeroParams;
   memset (&ZeroParams, 0x00, sizeof(NOTIFYPARAMS));

   Action_End (Action_Find (atREFRESH, &ZeroParams), pParams->status);
}


BOOL CALLBACK Action_Refresh_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_INITDIALOG:
         LPIDENT lpi;
         if ((lpi = (LPIDENT)lp) != NULL)
            {
            LPTSTR pszDesc = NULL;
            TCHAR szName[ cchRESOURCE ];

            if (lpi->fIsCell())
               {
               lpi->GetCellName (szName);
               pszDesc = FormatString (IDS_REFRESH_DESC_CELL, TEXT("%s"), szName);
               }
            else
               {
               lpi->GetServerName (szName);
               pszDesc = FormatString (IDS_REFRESH_DESC_SERVER, TEXT("%s"), szName);
               }
            if (pszDesc != NULL)
               {
               SetDlgItemText (hDlg, IDC_REFRESH_DESC, pszDesc);
               FreeString (pszDesc);
               }
            }

         SendDlgItemMessage (hDlg, IDC_REFRESH_PERCENTBAR, PBM_SETRANGE, 0, MAKELPARAM(0,100));
         Action_Refresh_UpdateText (hDlg, lpi, 0);
         Action_Refresh_SetSection (hDlg, TRUE, -1);
         break;

      case WM_REFRESH_UPDATE:
         Action_Refresh_UpdateText (hDlg, (LPIDENT)lp, (DWORD)wp);
         break;

      case WM_REFRESH_SETSECTION:
         Action_Refresh_SetSection (hDlg, (BOOL)wp, (int)lp);
         break;

      case WM_DESTROY:
         Action_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;

            case IDC_REFRESH_SKIP:
               Action_Refresh_SkipSection (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Action_Refresh_UpdateText (HWND hDlg, LPIDENT lpi, DWORD dwPerComplete)
{
   dwPerComplete = limit (0, dwPerComplete, 100);

   SendDlgItemMessage (hDlg, IDC_REFRESH_PERCENTBAR, PBM_SETPOS, (WPARAM)dwPerComplete, 0);

   LPTSTR pszComplete = FormatString (IDS_REFRESH_PERCENT, TEXT("%lu"), dwPerComplete);
   SetDlgItemText (hDlg, IDC_REFRESH_PERCENT, pszComplete);
   FreeString (pszComplete);

   if (lpi == NULL)
      {
      TCHAR szText[ cchRESOURCE ];
      GetString (szText, IDS_REFRESH_CURRENT_VLDB);
      SetDlgItemText (hDlg, IDC_REFRESH_CURRENT, szText);
      }
   else
      {
      TCHAR szCell[ cchNAME ];
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      TCHAR szFileset[ cchNAME ];
      TCHAR szService[ cchNAME ];

      lpi->GetCellName (szCell);
      if (!lpi->fIsCell())
         lpi->GetServerName (szServer);
      if (lpi->fIsAggregate() || lpi->fIsFileset())
         lpi->GetAggregateName (szAggregate);
      if (lpi->fIsFileset())
         lpi->GetFilesetName (szFileset);
      if (lpi->fIsService())
         lpi->GetServiceName (szService);

      LPTSTR pszCurrent = NULL;
      if (lpi->fIsCell())
         pszCurrent = FormatString (IDS_REFRESH_CURRENT_CELL, TEXT("%s"), szCell);
      if (lpi->fIsServer())
         pszCurrent = FormatString (IDS_REFRESH_CURRENT_SERVER, TEXT("%s%s"), szCell, szServer);
      else if (lpi->fIsAggregate())
         pszCurrent = FormatString (IDS_REFRESH_CURRENT_AGGREGATE, TEXT("%s%s%s"), szCell, szServer, szAggregate);
      else if (lpi->fIsFileset())
         pszCurrent = FormatString (IDS_REFRESH_CURRENT_FILESET, TEXT("%s%s%s%s"), szCell, szServer, szAggregate, szFileset);
      else if (lpi->fIsService())
         pszCurrent = FormatString (IDS_REFRESH_CURRENT_SERVICE, TEXT("%s%s%s"), szCell, szServer, szService);

      if (pszCurrent != NULL)
         {
         SetDlgItemText (hDlg, IDC_REFRESH_CURRENT, pszCurrent);
         FreeString (pszCurrent);
         }
      }
}


void Action_Refresh_SetSection (HWND hDlg, BOOL fStart, int idSection)
{
   // Are we ending a section that we don't care about?
   //
   if ((!fStart) && (idSection != (int)GetWindowLong (hDlg, DWL_USER)))
      return;

   if (!fStart)
      idSection = -1;  // ending a section means new section = -1 (invalid)

   SetWindowLong (hDlg, DWL_USER, idSection);
   EnableWindow (GetDlgItem (hDlg, IDC_REFRESH_SKIP), (idSection == -1) ? FALSE : TRUE);
}


void Action_Refresh_SkipSection (HWND hDlg)
{
   int idSection;
   if ((idSection = (int)GetWindowLong (hDlg, DWL_USER)) != -1)
      {
      AfsClass_SkipRefresh (idSection);
      Action_Refresh_SetSection (hDlg, FALSE, idSection);
      }
}


/*
 * MOVE-FILESET DIALOGS
 *
 */

void Action_MoveFileset_Begin (PNOTIFYPARAMS pParams)
{
   PACTION pAction;
   if ((pAction = Action_Begin (atMOVEFILESET, pParams)) != NULL)
      {
      HWND hDlg = ModelessDialogParam (IDD_SET_MOVING, NULL, (DLGPROC)Action_MoveFileset_DlgProc, (LPARAM)pAction);

      if (hDlg == NULL)
         Action_End (pAction, pParams->status);
      else
         Action_SpecifyWindow (pAction, hDlg);
      }
}


void Action_MoveFileset_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atMOVEFILESET, pParams), pParams->status);
}


BOOL CALLBACK Action_MoveFileset_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   HWND hAnimate = GetDlgItem (hDlg, IDC_ANIMATE);

   switch (msg)
      {
      case WM_INITDIALOG:
         Animate_Open (hAnimate, MAKEINTRESOURCE( AVI_SETMOVE ));
         Animate_Play (hAnimate, 0, -1, -1);

         PACTION pAction;
         if ((pAction = (PACTION)lp) != NULL)
            {
            TCHAR szServerSource[ cchNAME ];
            TCHAR szServerTarget[ cchNAME ];
            TCHAR szAggregateSource[ cchNAME ];
            TCHAR szAggregateTarget[ cchNAME ];
            TCHAR szFileset[ cchNAME ];
            pAction->Params.lpi1->GetServerName (szServerSource);
            pAction->Params.lpi1->GetAggregateName (szAggregateSource);
            pAction->Params.lpi1->GetFilesetName (szFileset);
            pAction->Params.lpi2->GetServerName (szServerTarget);
            pAction->Params.lpi2->GetAggregateName (szAggregateTarget);

            TCHAR szText[ cchRESOURCE ];
            GetDlgItemText (hDlg, IDC_MOVESET_DESC, szText, cchRESOURCE);

            LPTSTR pszText;
            pszText = FormatString (szText, TEXT("%s%s%s%s%s"), szServerSource, szAggregateSource, szFileset, szServerTarget, szAggregateTarget);
            SetDlgItemText (hDlg, IDC_MOVESET_DESC, pszText);
            FreeString (pszText);
            }
         break;

      case WM_DESTROY:
         Animate_Stop (hAnimate);
         Animate_Close (hAnimate);
         Action_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


/*
 * DUMP-FILESET DIALOGS
 *
 */

void Action_DumpFileset_Begin (PNOTIFYPARAMS pParams)
{
   PACTION pAction;
   if ((pAction = Action_Begin (atDUMP, pParams)) != NULL)
      {
      HWND hDlg = ModelessDialogParam (IDD_SET_DUMPING, NULL, (DLGPROC)Action_DumpFileset_DlgProc, (LPARAM)pAction);

      if (hDlg == NULL)
         Action_End (pAction, pParams->status);
      else
         Action_SpecifyWindow (pAction, hDlg);
      }
}

void Action_DumpFileset_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atDUMP, pParams), pParams->status);
}

BOOL CALLBACK Action_DumpFileset_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   HWND hAnimate = GetDlgItem (hDlg, IDC_ANIMATE);

   switch (msg)
      {
      case WM_INITDIALOG:
         Animate_Open (hAnimate, MAKEINTRESOURCE( AVI_SETMOVE ));
         Animate_Play (hAnimate, 0, -1, -1);

         PACTION pAction;
         if ((pAction = (PACTION)lp) != NULL)
            {
            TCHAR szServer[ cchNAME ];
            TCHAR szAggregate[ cchNAME ];
            TCHAR szFileset[ cchNAME ];
            pAction->Params.lpi1->GetServerName (szServer);
            pAction->Params.lpi1->GetAggregateName (szAggregate);
            pAction->Params.lpi1->GetFilesetName (szFileset);

            TCHAR szText[ cchRESOURCE ];
            GetDlgItemText (hDlg, IDC_DUMPSET_DESC, szText, cchRESOURCE);

            LPTSTR pszText;
            pszText = FormatString (szText, TEXT("%s%s%s%s"), szServer, szAggregate, szFileset, FindBaseFileName(pAction->Params.sz1));
            SetDlgItemText (hDlg, IDC_DUMPSET_DESC, pszText);
            FreeString (pszText);
            }
         break;

      case WM_DESTROY:
         Animate_Stop (hAnimate);
         Animate_Close (hAnimate);
         Action_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


/*
 * RESTORE-FILESET DIALOGS
 *
 */

void Action_RestoreFileset_Begin (PNOTIFYPARAMS pParams)
{
   PACTION pAction;
   if ((pAction = Action_Begin (atRESTORE, pParams)) != NULL)
      {
      HWND hDlg = ModelessDialogParam (IDD_SET_RESTORING, NULL, (DLGPROC)Action_RestoreFileset_DlgProc, (LPARAM)pAction);

      if (hDlg == NULL)
         Action_End (pAction, pParams->status);
      else
         Action_SpecifyWindow (pAction, hDlg);
      }
}

void Action_RestoreFileset_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atRESTORE, pParams), pParams->status);
}

BOOL CALLBACK Action_RestoreFileset_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   HWND hAnimate = GetDlgItem (hDlg, IDC_ANIMATE);

   switch (msg)
      {
      case WM_INITDIALOG:
         Animate_Open (hAnimate, MAKEINTRESOURCE( AVI_SETMOVE ));
         Animate_Play (hAnimate, 0, -1, -1);

         PACTION pAction;
         if ((pAction = (PACTION)lp) != NULL)
            {
            TCHAR szText[ cchRESOURCE ];
            GetDlgItemText (hDlg, IDC_RESTORESET_DESC, szText, cchRESOURCE);

            LPTSTR pszText;
            pszText = FormatString (szText, TEXT("%s%s"), ((LPTSTR)pAction->Params.sz1), FindBaseFileName(pAction->Params.sz2));
            SetDlgItemText (hDlg, IDC_RESTORESET_DESC, pszText);
            FreeString (pszText);
            }
         break;

      case WM_DESTROY:
         Animate_Stop (hAnimate);
         Animate_Close (hAnimate);
         Action_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


/*
 * OPEN-CELL DIALOGS
 *
 */

void Action_OpenCell_Begin (PNOTIFYPARAMS pParams)
{
   PACTION pAction;
   if ((pAction = Action_Begin (atOPENCELL, pParams)) != NULL)
      {
      if (!IsWindow (g.hMain) || !IsWindowVisible (g.hMain))
         {
         HWND hDlg = ModelessDialogParam (IDD_OPENINGCELL, NULL, (DLGPROC)Action_OpenCell_DlgProc, (LPARAM)pAction);

         if (hDlg == NULL)
            Action_End (pAction, pParams->status);
         else
            Action_SpecifyWindow (pAction, hDlg);
         }
      }
}

void Action_OpenCell_End (PNOTIFYPARAMS pParams)
{
   Action_End (Action_Find (atOPENCELL, pParams), pParams->status);
}

BOOL CALLBACK Action_OpenCell_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static int iFrameLast = 0;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   switch (msg)
      {
      case WM_INITDIALOG:
         AfsAppLib_StartAnimation (GetDlgItem (hDlg, IDC_ANIMATE));

         PACTION pAction;
         if ((pAction = (PACTION)lp) != NULL)
            {
            TCHAR szText[ cchRESOURCE ];
            GetDlgItemText (hDlg, IDC_OPENCELL_DESC, szText, cchRESOURCE);

            LPTSTR pszText;
            pszText = FormatString (szText, TEXT("%s"), pAction->Params.sz1);
            SetDlgItemText (hDlg, IDC_OPENCELL_DESC, pszText);
            FreeString (pszText);
            }
         break;

      case WM_DESTROY:
         Action_OnDestroy (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}

