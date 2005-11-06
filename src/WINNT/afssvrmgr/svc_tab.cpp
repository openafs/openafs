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
#include "svc_tab.h"
#include "svr_window.h"
#include "svr_general.h"
#include "display.h"
#include "command.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdServices[] = {
    { IDC_SVC_DESC,        raSizeX,		0,	0 },
    { IDC_SVC_LIST,        raSizeX | raSizeY,	0,	0 },
    { IDC_SVC_CREATE,      raMoveX | raMoveY,	0,	0 },
    { IDC_SVC_DELETE,      raMoveX | raMoveY,	0,	0 },
    { IDC_SVC_RESTART,     raMoveX | raMoveY,	0,	0 },
    { idENDLIST,           0,               	0,	0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Services_OnSelect (HWND hDlg);

void Services_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns);

void Services_SubclassList (HWND hDlg);

void Services_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen);

void Services_OnEndTask_Menu (LPTASKPACKET ptp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Services_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewSvc))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         ResizeWindow (hDlg, awdServices, rwaMoveToHere, &rTab);

         FL_RestoreView (GetDlgItem (hDlg, IDC_SVC_LIST), &gr.viewSvc);
         FastList_SetTextCallback (GetDlgItem (hDlg, IDC_SVC_LIST), GetItemText, &gr.viewSvc);

         Services_SubclassList (hDlg);
         Services_OnSelect (hDlg);
         }
         break;

      case WM_HELP:
         WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case WM_DESTROY:
         DontNotifyMeEver (hDlg);
         FL_StoreView (GetDlgItem (hDlg, IDC_SVC_LIST), &gr.viewSvc);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdServices, rwaFixupGuts);
         break;

      case WM_SERVER_CHANGED:
         {
         LPIDENT lpiServer = Server_GetServerForChild (hDlg);
         DontNotifyMeEver (hDlg);
         NotifyMe (WHEN_SVCS_CHANGE, lpiServer, hDlg, 0);

         // Fix the text at the top of the Services tab:
         //
         TCHAR szName[ cchRESOURCE ];
         LPTSTR pszText;

         if (lpiServer != NULL)
            {
            LPSERVER_PREF lpsp = (LPSERVER_PREF)lpiServer->GetUserParam();
            lpiServer->GetServerName (szName);
            if (lpsp && !lpsp->fIsMonitored)
               pszText = FormatString (IDS_SERVICE_UNMON, TEXT("%s"), szName);
            else
               pszText = FormatString (IDS_SERVICES_IN_SERVER, TEXT("%s"), szName);
            }
         else if (g.lpiCell != NULL)
            {
            g.lpiCell->GetCellName (szName);
            if (g.sub)
               pszText = FormatString (IDS_SERVICE_SOME, TEXT("%s"), szName);
            else
               pszText = FormatString (IDS_SERVICES_IN_CELL, TEXT("%s"), szName);
            }
         else
            {
            pszText = FormatString (IDS_SERVICES_IN_NOTHING);
            }

         SetDlgItemText (hDlg, IDC_SVC_DESC, pszText);
         FreeString (pszText);

         UpdateDisplay_Services (FALSE, hDlg, NULL, 0);
         }
         break;

      case WM_NOTIFY_FROM_DISPATCH:
         Services_OnNotifyFromDispatch ((LPNOTIFYSTRUCT)lp);
         Delete ((LPNOTIFYSTRUCT)lp);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSVC_MENU)
               Services_OnEndTask_Menu (ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_SVC_RESTART:
               SendMessage (GetDlgItem (hDlg, IDC_SVC_LIST), WM_COMMAND, M_SVC_RESTART, 0);
               break;

            case IDC_SVC_CREATE:
               SendMessage (GetDlgItem (hDlg, IDC_SVC_LIST), WM_COMMAND, M_SVC_CREATE, 0);
               break;

            case IDC_SVC_DELETE:
               SendMessage (GetDlgItem (hDlg, IDC_SVC_LIST), WM_COMMAND, M_SVC_DELETE, 0);
               break;
            }
         break;

      case WM_NOTIFY: 
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_ITEMSELECT:
               Services_OnSelect (hDlg);
               break;

            case FLN_LDBLCLICK:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_SVC_LIST))
                  {
                  if (FL_GetSelectedData (GetDlgItem (hDlg, IDC_SVC_LIST)))
                     PostMessage (GetDlgItem (hDlg, IDC_SVC_LIST), WM_COMMAND, M_PROPERTIES, 0);
                  }
               break;
            } 
         break; 

      case WM_CONTEXTMENU: 
         {
         POINT ptScreen;
         POINT ptClient;
         ptScreen.x = LOWORD(lp);
         ptScreen.y = HIWORD(lp);

         ptClient = ptScreen;
         ScreenToClient ((HWND)wp, &ptClient);

         if ((HWND)wp == GetDlgItem (hDlg, IDC_SVC_LIST))
            Services_ShowPopupMenu ((HWND)wp, ptClient, ptScreen);
         }
         break; 
      }

   return FALSE;
}


void Services_OnSelect (HWND hDlg)
{
   LPIDENT lpi;
   if ( ((lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_SVC_LIST))) == NULL) ||
        (!lpi->fIsService()) )
      {
      EnableWindow (GetDlgItem (hDlg, IDC_SVC_RESTART), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SVC_DELETE), FALSE);
      }
   else
      {
      TCHAR szName[ cchRESOURCE ];
      lpi->GetServiceName (szName);

      if (!lstrcmpi (szName, TEXT("BOS")))
         EnableWindow (GetDlgItem (hDlg, IDC_SVC_DELETE), FALSE);
      else
         EnableWindow (GetDlgItem (hDlg, IDC_SVC_DELETE), TRUE);

      EnableWindow (GetDlgItem (hDlg, IDC_SVC_RESTART), TRUE);
      }
}


void Services_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns)
{
   switch (lpns->evt)
      {
      case evtRefreshServicesEnd:
      case evtRefreshStatusEnd:
      case evtAlertsChanged:
      case evtDestroy:
         UpdateDisplay_Services (FALSE, lpns->hwndTarget, (LPIDENT)lpns->Params.lpi1, lpns->Params.status);
         break;
      }
}


static LONG_PTR procServicesList = 0;

LRESULT CALLBACK Services_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procServicesList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = (LRESULT) CallWindowProc ((WNDPROC)procServicesList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procServicesList != 0)
            SetWindowLongPtr (hList, GWLP_WNDPROC, procServicesList);
         break;

      case WM_COMMAND:
         StartContextCommand (Server_GetWindowForChild (GetParent(hList)),
                              Server_GetServerForChild (GetParent(hList)),
                              (LPIDENT)FL_GetSelectedData (hList),
                              LOWORD(wp));
         break;
      }

   return rc;
}


void Services_SubclassList (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_SVC_LIST);
   procServicesList = GetWindowLongPtr (hList, GWLP_WNDPROC);
   SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)Services_SubclassListProc);
}


void Services_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen)
{
   if (!ptScreen.x && !ptScreen.y)
      {
      RECT rWindow;
      GetWindowRect (hList, &rWindow);
      ptScreen.x = rWindow.left + (rWindow.right -rWindow.left)/2;
      ptScreen.y = rWindow.top + (rWindow.bottom -rWindow.top)/2;
      Services_ShowParticularPopupMenu (hList, ptScreen, NULL);
      }
   else if (FL_HitTestForHeaderBar (hList, ptList))
      {
      HMENU hm = TaLocale_LoadMenu (MENU_COLUMNS);
      DisplayContextMenu (hm, ptScreen, hList);
      }
   else
      {
      LPIDENT lpiSelected = NULL;

      HLISTITEM hItem;
      if ((hItem = FastList_ItemFromPoint (hList, &ptList, TRUE)) != NULL)
         lpiSelected = (LPIDENT)FL_GetData (hList, hItem);

      if (lpiSelected && (lpiSelected != (LPIDENT)FL_GetSelectedData (hList)))
         lpiSelected = NULL;

      if (lpiSelected && lpiSelected->fIsServer())
         Server_ShowParticularPopupMenu (hList, ptScreen, lpiSelected);
      else
         Services_ShowParticularPopupMenu (hList, ptScreen, lpiSelected);
      }
}


void Services_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiService)
{
   LPMENUTASK pmt;
   if ((pmt = New (MENUTASK)) != NULL)
      {
      pmt->hParent = hParent;
      pmt->ptScreen = ptScreen;
      pmt->lpi = lpiService;
      StartTask (taskSVC_MENU, GetParent(hParent), pmt);
      }
}


void Services_OnEndTask_Menu (LPTASKPACKET ptp)
{
   HMENU hm;
   if (!TASKDATA(ptp)->mt.lpi)
      hm = TaLocale_LoadMenu (MENU_SVC_NONE);
   else
      {
      TCHAR szName[ cchRESOURCE ];
      TASKDATA(ptp)->mt.lpi->GetServiceName (szName);
      if (!lstrcmpi (szName, TEXT("BOS")))
         hm = TaLocale_LoadMenu (MENU_SVC_BOS);
      else
         hm = TaLocale_LoadMenu (MENU_SVC);
      }

   if (hm != NULL)
      {
      if (TASKDATA(ptp)->mt.lpi == NULL)
         {
         HMENU hmView = GetSubMenu (hm, 0);

         CheckMenuRadioItem (hmView,
                             M_VIEW_ONEICON, M_VIEW_STATUS,
                             (gr.ivSvc == ivTWOICONS) ? M_VIEW_TWOICONS :
                             (gr.ivSvc == ivONEICON)  ? M_VIEW_ONEICON : M_VIEW_STATUS,
                             MF_BYCOMMAND);
         }
      else // (TASKDATA(ptp)->mt.lpi)
         {
         if (!ptp->rc)
            {
            EnableMenu (hm, M_SVC_START,   FALSE);
            EnableMenu (hm, M_SVC_STOP,    FALSE);
            EnableMenu (hm, M_SVC_RESTART, FALSE);
            }
         else
            {
            if (TASKDATA(ptp)->cs.state != SERVICESTATE_STOPPED)
               EnableMenu (hm, M_SVC_START, FALSE);
            else if (TASKDATA(ptp)->cs.state != SERVICESTATE_RUNNING)
               EnableMenu (hm, M_SVC_STOP, FALSE);
            }
         }

      DisplayContextMenu (hm, TASKDATA(ptp)->mt.ptScreen, TASKDATA(ptp)->mt.hParent);
      }
}

