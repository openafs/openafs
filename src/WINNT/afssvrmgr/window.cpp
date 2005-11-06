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
#include <shellapi.h>
#include "display.h"
#include "command.h"
#include "messages.h"
#include "svr_window.h"
#include "svr_general.h"
#include "svr_security.h"
#include "set_repprop.h"
#include "set_createrep.h"
#include "set_rename.h"
#include "columns.h"
#include "propcache.h"
#include "action.h"
#include "creds.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

#define cxMIN_SERVER   75
#define cyMIN_SERVER   70

#define cxMIN_TABS    100
#define cyMIN_TABS    100

#define cxMIN_WINDOW_PREVIEW  220
#define cyMIN_WINDOW_PREVIEW  250

#define cxMIN_WINDOW  120
#define cyMIN_WINDOW  120

rwWindowData awdMain[] = {
    { IDC_CELL_BORDER, raSizeX,			0,	0 },
    { IDC_CELL,        raSizeX | raRepaint,	0,	0 },
    { IDC_AFS_ID,      raSizeX | raRepaint,	0,	0 },
    { IDC_SERVERS,     raSizeX | raSizeY,  	MAKELONG(cxMIN_SERVER,cyMIN_SERVER), 0 },
    { IDC_COVERDLG,    raSizeX | raSizeY,	0,	0 },
    { IDC_ANIMATE,     raMoveX,			0,	0 },
    { idENDLIST,       0,			0,	0 }
 };

rwWindowData awdMainVert[] = {
    { IDC_CELL_BORDER,     raSizeX,		0,	0 },
    { IDC_CELL,            raSizeX | raRepaint,	0,	0 },
    { IDC_AFS_ID,          raSizeX | raRepaint,	0,	0 },
    { IDC_SERVERS,         raSizeX, 		MAKELONG(cxMIN_SERVER,cyMIN_SERVER), 0 },
    { IDC_SPLITTER_SERVER, raSizeX,		0,	0 },
    { IDC_TABS,            raSizeX | raSizeY, MAKELONG(cxMIN_TABS,cyMIN_TABS), 0 },
    { IDC_COVERDLG,        raSizeX,             0,	0 },
    { IDC_ANIMATE,         raMoveX,             0,	0 },
    { idENDLIST,           0,			0,	0  }
 };

rwWindowData awdMainHorz[] = {
    { IDC_CELL_BORDER,     raSizeX,			0,	0 },
    { IDC_CELL,            raSizeX  | raRepaint,	0,	0 },
    { IDC_AFS_ID,          raSizeX  | raRepaint,	0,	0 },
    { IDC_SERVERS,         raSizeY, 			MAKELONG(cxMIN_SERVER,cyMIN_SERVER), 0 },
    { IDC_SPLITTER_SERVER, raSizeY,			0,	0 },
    { IDC_TABS,            raSizeX | raSizeY, 		MAKELONG(cxMIN_TABS,cyMIN_TABS), 0 },
    { IDC_COVERDLG,        raSizeY,			0,	0 },
    { IDC_ANIMATE,         raMoveX,			0,	0 },
    { idENDLIST,           0,				0,	0 }
 };

rwWindowData awdSplitServer[] = {
    { IDC_CELL,            raRepaint,			0,	0 },
    { IDC_AFS_ID,          raRepaint,			0,	0 },
    { IDC_SERVERS,         raSizeX | raSizeY,		MAKELONG(cxMIN_SERVER,cyMIN_SERVER), 0 },
    { IDC_SPLITTER_SERVER, raMoveX | raMoveY,		0,	0 },
    { IDC_TABS,            raMoveX | raMoveY | raSizeXB | raSizeYB, MAKELONG(cxMIN_TABS,cyMIN_TABS), 0 },
    { IDC_COVERDLG,        raSizeX | raSizeY,		0,	0 },
    { IDC_ANIMATE,         0,				0,	0 },
    { idENDLIST,           0,				0,	0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Main_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns);

void Main_OnPreviewPane (BOOL fPreviewNew, BOOL fVertNew, BOOL fStoreView);
DWORD WINAPI Main_OnOpenServers_ThreadProc (PVOID lp);

void Main_SubclassServers (HWND hDlg);

void Main_CreateTabControl (void);
void Main_DeleteTabControl (void);
void Main_DisplayTab (CHILDTAB iTab);
void Main_RearrangeChildren (BOOL fSplitNew, BOOL fVertNew);

BOOL Main_HandleDialogKeys (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Main_OnKey_CtrlTab (BOOL fForward);
void Main_OnKey_Tab (BOOL fForward);

#ifdef DEBUG
void ExportCell (void);
#endif


/*
 * ROUTINES ___________________________________________________________________
 *
 */


static BOOL fEnforceMinimumSize = TRUE;
static BOOL fNotifyChildrenForResize = TRUE;
static LONG nReqWorking = 0;
static int  iFrameLast = 0;

BOOL CALLBACK Main_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (HandleColumnNotify (hDlg, msg, wp, lp, (gr.fPreview && !gr.fVert) ? &gr.diHorz.viewSvr : &gr.diVert.viewSvr))
      return FALSE;

   if (Main_HandleDialogKeys (hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)	// get this out of the way.  :)
      {
      g.hMain = hDlg;
      AfsAppLib_SetMainWindow (g.hMain);
      }

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         LPRECT prTarget = (gr.fPreview) ? &gr.rMainPreview : &gr.rMain;
         if (prTarget->right == 0)
            GetWindowRect (g.hMain, prTarget);
         ResizeWindow (g.hMain, awdMain, rwaMoveToHere, prTarget);

         Main_SetActionMenus();

         // Subclass the Servers window, so we can watch for focus changes
         // and context-menu requests
         //
         Main_SubclassServers (hDlg);
         FastList_SetTextCallback (GetDlgItem (hDlg, IDC_SERVERS), GetItemText, ((gr.fPreview && !gr.fVert) ? &gr.diHorz.viewSvr : &gr.diVert.viewSvr));

         // Create the preview pane and rearrange the children of this dialog.
         // This also fixes the Servers display--was it in Large Icons view?
         // What columns were shown?
         //
         Main_OnPreviewPane (gr.fPreview, gr.fVert, FALSE);

         // Tell our notification dispatcher to let this window know whenever
         // a cell gets opened or closed (that's what the NULL does--otherwise,
         // an LPIDENT would go there to indicate we're interested in a
         // particular server etc).
         //
         NotifyMe (WHEN_CELL_OPENED, NULL, hDlg, 0); // tell me about any cell

         // If we weren't around when the cell was created, we might have
         // missed some notifications.  Send a redraw just in case.
         //
         StartThread (Main_Redraw_ThreadProc, FALSE);

         SetTimer (hDlg, ID_DISPATCH_TIMER, 200, NULL); // do all notifications
         }
         break;

      case WM_DESTROY:
         KillTimer (hDlg, ID_DISPATCH_TIMER);
         break;

      case WM_QUERYENDSESSION:
         if (Action_fAnyActive())
            {
            ShowWindow (g.hMain, SW_SHOW);
            if (Message (MB_ICONHAND | MB_YESNO, IDS_CANT_QUIT_TITLE, IDS_CANT_QUIT_REBOOT) != IDYES)
               return TRUE;
            }
         break;

      case WM_TIMER:
         if (wp == ID_DISPATCH_TIMER)
            {
            DispatchNotification_OnPump();
            }
         break;

      case WM_HELP:
         if ((lp == 0) || (IsAncestor (g.hMain, (HWND)(((LPHELPINFO)lp)->hItemHandle))))
            {
            WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
            }
         break;

      case WM_NOTIFY_FROM_DISPATCH:
         Main_OnNotifyFromDispatch ((LPNOTIFYSTRUCT)lp);
         Delete ((LPNOTIFYSTRUCT)lp);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVR_GETWINDOWPOS)
               Server_Open ((LPIDENT)(ptp->lpUser), &TASKDATA(ptp)->rWindow);
            else if (ptp->idTask == taskSET_REPPROP_INIT)
               Filesets_OnEndTask_ShowReplication (ptp);
            else if (ptp->idTask == taskSET_RENAME_INIT)
               Filesets_OnEndTask_ShowRename (ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_OPEN_SERVERS:
         StartThread (Main_OnOpenServers_ThreadProc, 0);
         break;

      case WM_OPEN_SERVER:
         StartTask (taskSVR_GETWINDOWPOS, g.hMain, (LPIDENT)lp);
         break;

      case WM_SHOW_CREATEREP_DIALOG:
         Filesets_CreateReplica ((LPIDENT)wp, (LPIDENT)lp);
         break;

      case WM_SHOW_YOURSELF:
         if (lp || g.lpiCell)
            {
            ShowWindow (g.hMain, SW_SHOW);
            BringWindowToTop (g.hMain);
            Action_WindowToTop (TRUE);
            }
         break;

      case WM_OPEN_ACTIONS:
         Action_OpenWindow();
         break;

      case WM_REFRESHED_CREDENTIALS:
         g.hCreds = (UINT_PTR)lp;
         UpdateDisplay_Cell(FALSE);
         StartTask (taskREFRESH_CREDS, NULL, g.lpiCell);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         Action_WindowToTop ((lp) ? TRUE : FALSE);
         if (lp != 0)
            {
            rwWindowData *pwdResizeInfo;

            if (!gr.fPreview)
               pwdResizeInfo = awdMain;
            else if (gr.fVert)
               pwdResizeInfo = awdMainVert;
            else // (!gr.fVert)
               pwdResizeInfo = awdMainHorz;

            if (fNotifyChildrenForResize)
               ResizeWindow (hDlg, pwdResizeInfo, rwaFixupGuts);
            else
               ResizeWindow (hDlg, pwdResizeInfo, rwaJustResync);
            }
         break;

      case WM_ACTIVATEAPP:
         Action_WindowToTop ((wp) ? TRUE : FALSE);

         if (wp)
            StartTask (taskEXPIRED_CREDS);
         break;

      case WM_CONTEXTMENU: 
         {
         POINT ptScreen;
         POINT ptClient;
         ptScreen.x = LOWORD(lp);
         ptScreen.y = HIWORD(lp);

         ptClient = ptScreen;
         ScreenToClient ((HWND)wp, &ptClient);

         if ((HWND)wp == GetDlgItem (g.hMain, IDC_SERVERS))
            Server_ShowPopupMenu ((HWND)wp, ptClient, ptScreen);
         }
         break; 

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
            case M_EXIT:
               Quit (0);
               break;

            case M_DIVIDE_NONE:
               Main_OnPreviewPane (FALSE, gr.fVert, TRUE);
               UpdateDisplay_Servers (FALSE, NULL, 0);
               break;
            case M_DIVIDE_H:
               Main_OnPreviewPane (TRUE, FALSE, TRUE);
               UpdateDisplay_Servers (FALSE, NULL, 0);
               break;
            case M_DIVIDE_V:
               Main_OnPreviewPane (TRUE, TRUE, TRUE);
               UpdateDisplay_Servers (FALSE, NULL, 0);
               break;

            case M_SVR_VIEW_LARGE:
               Main_OnServerView (FLS_VIEW_LARGE);
               break;
            case M_SVR_VIEW_SMALL:
               Main_OnServerView (FLS_VIEW_SMALL);
               break;
            case M_SVR_VIEW_REPORT:
               Main_OnServerView (FLS_VIEW_LIST);
               break;

            case M_COLUMNS:
               if (!gr.fPreview)
                  ShowColumnsDialog (hDlg, NULL);
               else
                  {
                  CHILDTAB iTab = Server_GetDisplayedTab (g.hMain);
                  if (iTab == tabSERVICES)
                     ShowColumnsDialog (hDlg, &gr.viewSvc);
                  else if (iTab == tabAGGREGATES)
                     ShowColumnsDialog (hDlg, &gr.viewAgg);
                  else if (iTab == tabFILESETS)
                     ShowColumnsDialog (hDlg, &gr.viewSet);
                  else
                     ShowColumnsDialog (hDlg, NULL);
                  }
               break;

            case M_ACTIONS:
               if ((gr.fActions = !gr.fActions) == TRUE)
                  Action_OpenWindow();
               else
                  Action_CloseWindow();
               Main_SetActionMenus();
               break;

#ifdef DEBUG
            case M_EXPORT:
               ExportCell();
               break;
#endif

            case IDC_COVER_BUTTON:
               // The user must have clicked the "Try Again" or "Credentials"
               // button on the cover we placed over the Servers list.
               // Refresh the cell or obtain new credentials, as appropriate.
               //
               if (g.lpiCell)
                  {
                  TCHAR szButton[ cchRESOURCE ];
                  GetWindowText ((HWND)lp, szButton, cchRESOURCE);

                  TCHAR szTest[ cchRESOURCE ];

                  GetString (szTest, IDS_ALERT_BUTTON_TRYAGAIN);
                  if (!lstrcmp (szTest, szButton))
                     {
                     StartTask (taskREFRESH, NULL, g.lpiCell);
                     break;
                     }

                  GetString (szTest, IDS_ALERT_BUTTON_GETCREDS);
                  if (!lstrcmp (szTest, szButton))
                     {
                     NewCredsDialog();
                     break;
                     }
                  }
               break;

            default:
               StartContextCommand (g.hMain, NULL, NULL, LOWORD(wp));
               break;
            }
         break;

      case WM_NOTIFY: 
         switch (((LPNMHDR)lp)->code)
            { 
            case TCN_SELCHANGE:
               { 
               int iPage = TabCtrl_GetCurSel (GetDlgItem (g.hMain, IDC_TABS));
               Main_DisplayTab ((CHILDTAB)iPage);
               } 
               break; 

            case FLN_ITEMSELECT:
               if (gr.fPreview)
                  {
                  HWND hServers = GetDlgItem (hDlg, IDC_SERVERS);
                  LPIDENT lpi = (LPIDENT)FL_GetSelectedData (hServers);

                  Server_SelectServer (SERVERWINDOW_PREVIEWPANE, lpi);
                  }
               break;

            case FLN_LDBLCLICK:
               {
               HWND hServers = GetDlgItem (g.hMain, IDC_SERVERS);

               if (((LPNMHDR)lp)->hwndFrom == hServers)
                  {
                  LPIDENT lpi = (LPIDENT)FL_GetSelectedData (hServers);

                  if (lpi && lpi->fIsServer())
                     {
                     BOOL fOpenWindow;

                     if (gr.fDoubleClickOpens == 0)
                        fOpenWindow = FALSE;
                     else if (gr.fDoubleClickOpens == 1)
                        fOpenWindow = TRUE;
                     else if (gr.fPreview)
                        fOpenWindow = FALSE;
                     else // (!gr.Preview)
                        fOpenWindow = TRUE;

                     if (!fOpenWindow)
                        {
                        PostMessage (hServers, WM_COMMAND, M_PROPERTIES, 0);
                        }
                     else // (fOpenWindow)
                        {
                        PostMessage (g.hMain, WM_OPEN_SERVER, 0, (LPARAM)lpi);
                        }
                     }
                  }
               }
               break;
            } 
         break; 
      }

   return FALSE;
}


void Main_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns)
{
   switch (lpns->evt)
      {
      case evtCreate:
         StartTask (taskOPENEDCELL, NULL, lpns->Params.lpi1);
         break;

      case evtDestroy:
         StartTask (taskCLOSEDCELL, NULL, lpns->Params.lpi1);
         break;

      case evtRefreshStatusEnd:
      case evtAlertsChanged:
         if (lpns->Params.lpi1 && lpns->Params.lpi1->fIsCell())
            {
            UpdateDisplay_Cell (FALSE);
            }
         else if (lpns->Params.lpi1 && lpns->Params.lpi1->fIsServer())
            {
            UpdateDisplay_Servers (FALSE, lpns->Params.lpi1, lpns->Params.status);
            }
         break;

      case evtRefreshServersEnd:
         if (g.lpiCell || lpns->Params.status == 0)
            {
            UpdateDisplay_Servers (FALSE, NULL, 0);
            }
         break;
      }
}


DWORD WINAPI Main_OnOpenServers_ThreadProc (PVOID lp)
{
   AfsClass_Enter();

   if (g.lpiCell != NULL)
      {
      LPCELL lpCell;
      if ((lpCell = g.lpiCell->OpenCell()) != NULL)
         {
         HENUM hEnum;
         for (LPSERVER lpServer = lpCell->ServerFindFirst (&hEnum); lpServer; lpServer = lpCell->ServerFindNext (&hEnum))
            {
            LPSERVER_PREF lpsp;
            if ((lpsp = (LPSERVER_PREF)lpServer->GetUserParam()) != NULL)
               {
               if (lpsp->fOpen && !PropCache_Search (pcSERVER, lpServer->GetIdentifier()))
                  {
                  PostMessage (g.hMain, WM_OPEN_SERVER, 0, (LPARAM)(lpServer->GetIdentifier()));
                  }
               }
            lpServer->Close();
            }
         lpCell->Close();
         }
      }

   AfsClass_Leave();
   return 0;
}


DWORD WINAPI Main_Redraw_ThreadProc (PVOID lp)
{
   BOOL fInvalidate = (BOOL)(UINT_PTR)lp;
   AfsClass_Enter();

   if (g.lpiCell == NULL)
      {
      AfsAppLib_Uncover (GetDlgItem (g.hMain, IDC_SERVERS));
      }
   else
      {
      TCHAR szName[ cchRESOURCE ];
      g.lpiCell->GetCellName (szName);

      LPTSTR pszCover = FormatString (IDS_SEARCHING_FOR_SERVERS, TEXT("%s"), szName);
      AfsAppLib_CoverWindow (GetDlgItem (g.hMain, IDC_SERVERS), pszCover);
      FreeString (pszCover);
      }

   LPCELL lpCell = NULL;
   if (g.lpiCell != NULL)
      lpCell = g.lpiCell->OpenCell();

   if (lpCell)
      {
      if (fInvalidate)
         {
         lpCell->Invalidate();
         lpCell->RefreshAll();
         }
      else
         {
         lpCell->RefreshStatus();
         lpCell->RefreshServers();
         }
      }
   else // no cell is selected; we'll have to redraw the window directly.
      {
      UpdateDisplay_Cell (TRUE);
      UpdateDisplay_Servers (TRUE, NULL, 0);
      }

   if (lpCell != NULL)
      lpCell->Close();

   AfsClass_Leave();
   return 0;
}


void Main_OnPreviewPane (BOOL fPreviewNew, BOOL fVertNew, BOOL fStoreCurrentView)
{
   if (fStoreCurrentView)
      {
      if (gr.fPreview && !gr.fVert)
         FL_StoreView (GetDlgItem (g.hMain, IDC_SERVERS), &gr.diHorz.viewSvr);
      else
         FL_StoreView (GetDlgItem (g.hMain, IDC_SERVERS), &gr.diVert.viewSvr);

      if (gr.fPreview)
         GetWindowRect (g.hMain, &gr.rMainPreview);
      else
         GetWindowRect (g.hMain, &gr.rMain);
      }

   // If switching from having a preview pane to not (or vice-versa),
   // and we have an alterate gr.rMain* to switch to, resize the window.
   //
   if (IsWindowVisible (g.hMain))
      {
      RECT rNow;
      GetWindowRect (g.hMain, &rNow);

      LPRECT prTarget = (fPreviewNew) ? &gr.rMainPreview : &gr.rMain;
      if (prTarget->right == 0)
         {
         *prTarget = rNow;
         if (gr.fPreview && gr.fVert) // preview pane exists below main window?
            {
            RECT rPreview;
            GetWindowRect (GetDlgItem (g.hMain, IDC_TABS), &rPreview);
            if (gr.fVert)
               prTarget->bottom -= cyRECT(rPreview);
            }
         }
      prTarget->right = prTarget->left + cxRECT(rNow); // keep the same width!

      fEnforceMinimumSize = FALSE;
      fNotifyChildrenForResize = FALSE;

      SetWindowPos (g.hMain, NULL,
                    0, 0, cxRECT(*prTarget), cyRECT(*prTarget),
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);

      fNotifyChildrenForResize = TRUE;
      fEnforceMinimumSize = TRUE;
      }

   // Create a tab control if necessary, or remove it if it should be gone.
   //
   if (fPreviewNew && !GetDlgItem (g.hMain, IDC_TABS))
      {
      Main_CreateTabControl();
      }
   if (!fPreviewNew && GetDlgItem (g.hMain, IDC_TABS))
      {
      Main_DeleteTabControl();
      }

   // If there's a tab control, we'll need a splitter; if not, not.
   //
   if (GetDlgItem (g.hMain, IDC_SPLITTER_SERVER))
      {
      DeleteSplitter (g.hMain, IDC_SPLITTER_SERVER);
      }

   Main_RearrangeChildren (fPreviewNew, fVertNew);

   if (fPreviewNew)
      {
      CreateSplitter (g.hMain, IDC_SERVERS, IDC_TABS, IDC_SPLITTER_SERVER,
                      (fVertNew) ? &gr.diVert.cSplitter : &gr.diHorz.cSplitter,
                      awdSplitServer,
                      TRUE);
      }

   if (GetDlgItem (g.hMain, IDC_TABS))
      {
      ShowWindow (GetDlgItem (g.hMain, IDC_TABS), SW_SHOW);
      }

   LPVIEWINFO lpvi = (fPreviewNew && !fVertNew) ? &gr.diHorz.viewSvr : &gr.diVert.viewSvr;

   FL_RestoreView (GetDlgItem (g.hMain, IDC_SERVERS), lpvi);

   CheckMenuRadioItem (GetMenu (g.hMain),
                       M_DIVIDE_NONE, M_DIVIDE_H,
                       ( (!fPreviewNew) ? M_DIVIDE_NONE :
                             (fVertNew) ? M_DIVIDE_V :
                                          M_DIVIDE_H ),
                       MF_BYCOMMAND);

   gr.fVert = fVertNew;
   gr.fPreview = fPreviewNew;

   Main_SetServerViewMenus();
}


void Main_OnServerView (int lvsNew)
{
   HWND hServers = GetDlgItem (g.hMain, IDC_SERVERS);

   if (gr.fPreview && !gr.fVert)
      FL_StoreView (hServers, &gr.diHorz.viewSvr);
   else
      FL_StoreView (hServers, &gr.diVert.viewSvr);

   if (gr.fPreview && !gr.fVert)
      gr.diHorz.viewSvr.lvsView = lvsNew;
   else
      gr.diVert.viewSvr.lvsView = lvsNew;

   if (gr.fPreview && !gr.fVert)
      FL_RestoreView (hServers, &gr.diHorz.viewSvr);
   else
      FL_RestoreView (hServers, &gr.diVert.viewSvr);

   Main_SetServerViewMenus();
   FastList_SetTextCallback (GetDlgItem (g.hMain, IDC_SERVERS), GetItemText, ((gr.fPreview && !gr.fVert) ? &gr.diHorz.viewSvr : &gr.diVert.viewSvr));
   UpdateDisplay_Servers (FALSE, NULL, 0);
}


void Main_SetServerViewMenus (void)
{
   LONG lvs;

   if (gr.fPreview && !gr.fVert)
      lvs = gr.diHorz.viewSvr.lvsView;
   else
      lvs = gr.diVert.viewSvr.lvsView;

   CheckMenuRadioItem (GetMenu (g.hMain),
                       M_SVR_VIEW_LARGE, M_SVR_VIEW_REPORT,
                       ( (lvs == FLS_VIEW_SMALL) ? M_SVR_VIEW_SMALL :
                         (lvs == FLS_VIEW_LIST)  ? M_SVR_VIEW_REPORT :
                                                   M_SVR_VIEW_LARGE ),
                       MF_BYCOMMAND);

   ICONVIEW ivSvr = Display_GetServerIconView();

   CheckMenuRadioItem (GetMenu (g.hMain),
                       M_SVR_VIEW_ONEICON, M_SVR_VIEW_STATUS,
                       (ivSvr == ivTWOICONS) ? M_SVR_VIEW_TWOICONS :
                       (ivSvr == ivONEICON)  ? M_SVR_VIEW_ONEICON : M_SVR_VIEW_STATUS,
                       MF_BYCOMMAND);

   EnableMenu (GetMenu (g.hMain), M_SVR_VIEW_ONEICON,  (lvs == FLS_VIEW_LIST) ? TRUE : FALSE);
   EnableMenu (GetMenu (g.hMain), M_SVR_VIEW_TWOICONS, (lvs == FLS_VIEW_LIST) ? TRUE : FALSE);
   EnableMenu (GetMenu (g.hMain), M_SVR_VIEW_STATUS,   (lvs == FLS_VIEW_LIST) ? TRUE : FALSE);
}


void Main_SetActionMenus (void)
{
   CheckMenu (GetMenu (g.hMain), M_ACTIONS, gr.fActions);
}


void Main_CreateTabControl (void)
{
   HWND hTab = CreateWindowEx (WS_EX_CLIENTEDGE,
                               WC_TABCONTROL,
                               TEXT(""),
                               WS_CHILD | WS_CLIPSIBLINGS | WS_TABSTOP,
                               0, 0, 128, 128,   // arbitrary, but not too small
                               SERVERWINDOW_PREVIEWPANE,
                               (HMENU)IDC_TABS,
                               THIS_HINST,
                               NULL);

   if (hTab != NULL)
      {
      HFONT hf = (HFONT)GetStockObject (DEFAULT_GUI_FONT);
      SendMessage (hTab, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE,0));

      Server_PrepareTabControl (hTab);

      LPIDENT lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (g.hMain, IDC_SERVERS));
      Server_SelectServer (SERVERWINDOW_PREVIEWPANE, lpi);
      }
}


void Main_DeleteTabControl (void)
{
   if (GetDlgItem (SERVERWINDOW_PREVIEWPANE, IDC_TABS))
      {
      DestroyWindow (GetDlgItem (SERVERWINDOW_PREVIEWPANE, IDC_TABS));
      }
}


void Main_DisplayTab (CHILDTAB iTab)
{
   if (gr.fPreview)
      {
      Server_DisplayTab (SERVERWINDOW_PREVIEWPANE, iTab);
      gr.tabLast = (CHILDTAB)iTab;
      }
}


void Main_RearrangeChildren (BOOL fPreviewNew, BOOL fVertNew)
{
   RECT rServers;
   RECT rPreview;

   // Start with the Servers window taking up the entire dialog's client area,
   // leaving a space at the top for the cell combobox.
   //
   RECT rCell;
   GetRectInParent (GetDlgItem (g.hMain, IDC_AFS_ID), &rCell);

   GetClientRect (g.hMain, &rServers);
   rServers.top = rCell.bottom +3;

   // Then, if the preview tab is to be displayed, make the server window
   // share the space equally with the tab control (vertically or horizontally)
   //
   if (fPreviewNew)
      {
      rPreview = rServers;

      if (fVertNew)
         {
         rServers.bottom = rServers.top + cyRECT(rServers)/2 - 1;
         rPreview.top = rServers.bottom + 2;
         }
      else
         {
         rServers.right = rServers.left + cxRECT(rServers)/2 - 1;
         rPreview.left = rServers.right + 2;
         }

      // Adjust the server/tab windows for the Server splitter's position
      //
      if (fVertNew)
         {
         LONG cyMod = gr.diVert.cSplitter;

         if (cyRECT(rServers) > cyMIN_SERVER)
            cyMod = min( cyMod, cyRECT(rServers)-cyMIN_SERVER );
         if (cyRECT(rPreview) > cyMIN_TABS)
            cyMod = min( cyMod, cyRECT(rPreview)-cyMIN_TABS );

         rServers.bottom += cyMod;
         rPreview.top += cyMod;
         }
      else
         {
         LONG cxMod = gr.diHorz.cSplitter;

         if (cxRECT(rServers) > cxMIN_SERVER)
            cxMod = min( cxMod, cxRECT(rServers)-cxMIN_SERVER );
         if (cxRECT(rPreview) > cxMIN_TABS)
            cxMod = min( cxMod, cxRECT(rPreview)-cxMIN_TABS );

         rServers.right += cxMod;
         rPreview.left += cxMod;
         }
      }

   // Now move the children to their new places!
   //
   int nWindows = 1;
   if (GetDlgItem (g.hMain, IDC_COVERDLG))
      ++nWindows;
   if (GetDlgItem (g.hMain, IDC_TABS))
      ++nWindows;
   HDWP dwp = BeginDeferWindowPos (nWindows);

   DeferWindowPos (dwp, GetDlgItem (g.hMain, IDC_SERVERS), NULL,
                   rServers.left, rServers.top,
                   cxRECT(rServers), cyRECT(rServers),
                   SWP_NOACTIVATE | SWP_NOZORDER);

   if (GetDlgItem (g.hMain, IDC_COVERDLG))
      {
      DeferWindowPos (dwp, GetDlgItem (g.hMain, IDC_COVERDLG), NULL,
                      rServers.left, rServers.top,
                      cxRECT(rServers), cyRECT(rServers),
                      SWP_NOACTIVATE | SWP_NOZORDER);
      }

   if (GetDlgItem (g.hMain, IDC_TABS))
      {
      DeferWindowPos (dwp, GetDlgItem (g.hMain, IDC_TABS), NULL,
                      rPreview.left, rPreview.top,
                      cxRECT(rPreview), cyRECT(rPreview),
                      SWP_NOACTIVATE | SWP_NOZORDER);
      }

   EndDeferWindowPos (dwp);
}



static LONG_PTR procServers = 0;

LRESULT CALLBACK Main_SubclassServersProc (HWND hServers, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procServers == 0)
      rc = DefWindowProc (hServers, msg, wp, lp);
   else
      rc = (LRESULT) CallWindowProc ((WNDPROC)procServers, hServers, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procServers != 0)
            SetWindowLongPtr (hServers, GWLP_WNDPROC, procServers);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case M_SVR_VIEW_LARGE:
            case M_SVR_VIEW_SMALL:
            case M_SVR_VIEW_REPORT:
               SendMessage (g.hMain, msg, wp, lp);
               break;

            case M_VIEW_ONEICON:
            case M_VIEW_TWOICONS:
            case M_VIEW_STATUS:
               SendMessage (g.hMain, msg, wp, lp);
               break;

            case M_COLUMNS:
               ShowColumnsDialog (g.hMain, NULL);
               break;

            default:
               StartContextCommand (g.hMain,
                                    NULL,
                                    (LPIDENT)FL_GetSelectedData (hServers),
                                    LOWORD(wp));
               break;
            }
         break;
      }

   return rc;
}


void Main_SubclassServers (HWND hDlg)
{
   HWND hServers = GetDlgItem (hDlg, IDC_SERVERS);
   procServers = GetWindowLongPtr (hServers, GWLP_WNDPROC);
   SetWindowLongPtr (hServers, GWLP_WNDPROC, (LONG_PTR)Main_SubclassServersProc);
}


#ifdef DEBUG
void ExportCell (void)
{
   if (!g.lpiCell)
      return;

   TCHAR szFilter[ cchRESOURCE ];
   lstrcpy (szFilter, TEXT("Text File|*.TXT|"));
   TCHAR chFilter = szFilter[ lstrlen(szFilter)-1 ];
   for (LPTSTR pszFilter = szFilter;
        (*pszFilter) && ((pszFilter = (LPTSTR)lstrchr (pszFilter, chFilter)) != NULL);
        ++pszFilter)
      {
      *pszFilter = TEXT('\0');
      }

   TCHAR szSaveAs[ MAX_PATH ] = TEXT("export");

   OPENFILENAME sfn;
   memset (&sfn, 0x00, sizeof(sfn));
   sfn.lStructSize = sizeof(sfn);
   sfn.hwndOwner = g.hMain;
   sfn.hInstance = THIS_HINST;
   sfn.lpstrFilter = szFilter;
   sfn.nFilterIndex = 1;
   sfn.lpstrFile = szSaveAs;
   sfn.nMaxFile = MAX_PATH;
   sfn.Flags = OFN_HIDEREADONLY | OFN_NOREADONLYRETURN |
               OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
   sfn.lpstrDefExt = TEXT("txt");

   if (GetSaveFileName (&sfn))
      {
      LPTSTR psz = CloneString (szSaveAs);
      StartTask (taskEXPORTCELL, NULL, psz);
      }
}
#endif


void Main_StartWorking (void)
{
   if (InterlockedIncrement(&nReqWorking) == 1)
      {
      AfsAppLib_StartAnimation (GetDlgItem (g.hMain, IDC_ANIMATE));
      }
}

void Main_StopWorking (void)
{
   if (InterlockedDecrement(&nReqWorking) == 0)
      {
      AfsAppLib_StopAnimation (GetDlgItem (g.hMain, IDC_ANIMATE));
      }
}


void Main_AnimateIcon (HWND hIcon, int *piFrameLast)
{
   static HICON hiStop;
   static HICON hiFrame[8];
   static BOOL fLoaded = FALSE;

   if (!fLoaded)
      {
      hiStop     = TaLocale_LoadIcon (IDI_SPINSTOP);
      hiFrame[0] = TaLocale_LoadIcon (IDI_SPIN1);
      hiFrame[1] = TaLocale_LoadIcon (IDI_SPIN2);
      hiFrame[2] = TaLocale_LoadIcon (IDI_SPIN3);
      hiFrame[3] = TaLocale_LoadIcon (IDI_SPIN4);
      hiFrame[4] = TaLocale_LoadIcon (IDI_SPIN5);
      hiFrame[5] = TaLocale_LoadIcon (IDI_SPIN6);
      hiFrame[6] = TaLocale_LoadIcon (IDI_SPIN7);
      hiFrame[7] = TaLocale_LoadIcon (IDI_SPIN8);
      fLoaded = TRUE;
      }

   if (piFrameLast)
      {
      *piFrameLast = (*piFrameLast == 7) ? 0 : (1+*piFrameLast);
      }

   SendMessage (hIcon, STM_SETICON, (WPARAM)((piFrameLast) ? hiFrame[ *piFrameLast ] : hiStop), 0);
}


BOOL Main_HandleDialogKeys (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (msg == WM_COMMAND)
      {
      switch (LOWORD(wp))
         {
         case M_KEY_RETURN:
            Server_OnKey_Return();
            return TRUE;

         case M_KEY_CTRLTAB:
            Main_OnKey_CtrlTab (TRUE);
            return TRUE;

         case M_KEY_CTRLBACKTAB:
            Main_OnKey_CtrlTab (FALSE);
            return TRUE;

         case M_KEY_TAB:
            Main_OnKey_Tab (TRUE);
            return TRUE;

         case M_KEY_BACKTAB:
            Main_OnKey_Tab (FALSE);
            return TRUE;

         case M_KEY_MENU:
            Server_OnKey_Menu();
            return TRUE;

         case M_KEY_ESC:
             Server_OnKey_Esc();
            return TRUE;

         case M_KEY_PROPERTIES:
            Server_OnKey_Properties();
            return TRUE;
         }
      }

   return FALSE;
}


void Main_OnKey_Tab (BOOL fForward)
{
   // The tab-cycle should go:
   //    ServerList <-> TabsOnQuickViewPane <-> TabChildControls
   //
   // If the quick-view pane isn't showing, there's nowhere to tab to.
   //
   if (gr.fPreview)
      {
      HWND hFocus = GetFocus();
      HWND hTabChild = GetTabChild (GetDlgItem (g.hMain, IDC_TABS));

      if (fForward)
         {
         if (hFocus == GetDlgItem (g.hMain, IDC_SERVERS))
            {
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (g.hMain, IDC_TABS), TRUE);
            }
         else if (hFocus == GetDlgItem (g.hMain, IDC_TABS))
            {
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, NULL, FALSE), TRUE);
            }
         else
            {
            if (GetNextDlgTabItem (hTabChild, hFocus, FALSE) == GetNextDlgTabItem (hTabChild, NULL, FALSE))
               PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (g.hMain, IDC_SERVERS), TRUE);
            else
               PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, hFocus, FALSE), TRUE);
            }
         }
      else // (!fForward)
         {
         if (hFocus == GetDlgItem (g.hMain, IDC_SERVERS))
            {
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetLastDlgTabItem (hTabChild), TRUE);
            }
         else if (hFocus == GetDlgItem (g.hMain, IDC_TABS))
            {
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (g.hMain, IDC_SERVERS), TRUE);
            }
         else
            {
            if (hFocus == GetNextDlgTabItem (hTabChild, NULL, FALSE))
               PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (g.hMain, IDC_TABS), TRUE);
            else
               PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, hFocus, TRUE), TRUE);
            }
         }
      }
}


void Main_OnKey_CtrlTab (BOOL fForward)
{
   if (gr.fPreview)
      {
      Server_OnKey_CtrlTab (SERVERWINDOW_PREVIEWPANE, fForward);
      }
}

