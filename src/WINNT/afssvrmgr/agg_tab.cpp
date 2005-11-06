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
#include "agg_tab.h"
#include "agg_general.h"
#include "svr_window.h"
#include "svr_general.h"
#include "display.h"
#include "command.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdAggregates[] = {
    { IDC_AGG_DESC,        raSizeX,		0,	0 },
    { IDC_AGG_LIST,        raSizeX | raSizeY,	0,	0 },
    { IDC_AGG_CREATESET,   raMoveX | raMoveY,	0,	0 },
    { idENDLIST,           0,			0,	0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Aggregates_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns);

void Aggregates_SubclassList (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Aggregates_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewAgg))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         ResizeWindow (hDlg, awdAggregates, rwaMoveToHere, &rTab);

         FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAgg);
         FastList_SetTextCallback (GetDlgItem (hDlg, IDC_AGG_LIST), GetItemText, &gr.viewAgg);

         Aggregates_SubclassList (hDlg);
         }
         break;

      case WM_HELP:
         WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case WM_DESTROY:
         DontNotifyMeEver (hDlg);
         FL_StoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAgg);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdAggregates, rwaFixupGuts);
         break;

      case WM_SERVER_CHANGED:
         {
         LPIDENT lpiServer = Server_GetServerForChild (hDlg);
         DontNotifyMeEver (hDlg);
         NotifyMe (WHEN_AGGS_CHANGE, lpiServer, hDlg, 0);

         // Fix the text at the top of the Aggregates tab:
         //
         TCHAR szName[ cchRESOURCE ];
         LPTSTR pszText;

         if (lpiServer != NULL)
            {
            LPSERVER_PREF lpsp = (LPSERVER_PREF)lpiServer->GetUserParam();
            lpiServer->GetServerName (szName);
            if (lpsp && !lpsp->fIsMonitored)
               pszText = FormatString (IDS_AGGREGATE_UNMON, TEXT("%s"), szName);
            else
               pszText = FormatString (IDS_AGGREGATES_IN_SERVER, TEXT("%s"), szName);
            }
         else if (g.lpiCell != NULL)
            {
            g.lpiCell->GetCellName (szName);
            if (g.sub)
               pszText = FormatString (IDS_AGGREGATE_SOME, TEXT("%s"), szName);
            else
               pszText = FormatString (IDS_AGGREGATES_IN_CELL, TEXT("%s"), szName);
            }
         else
            {
            pszText = FormatString (IDS_AGGREGATES_IN_NOTHING);
            }

         SetDlgItemText (hDlg, IDC_AGG_DESC, pszText);
         FreeString (pszText);

         UpdateDisplay_Aggregates (FALSE, GetDlgItem (hDlg, IDC_AGG_LIST), NULL, 0, NULL, NULL, NULL);
         }
         break;

      case WM_NOTIFY_FROM_DISPATCH:
         Aggregates_OnNotifyFromDispatch ((LPNOTIFYSTRUCT)lp);
         Delete ((LPNOTIFYSTRUCT)lp);
         break;

      case WM_NOTIFY: 
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_LDBLCLICK:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_AGG_LIST))
                  {
                  if (FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST)))
                     PostMessage (GetDlgItem (hDlg, IDC_AGG_LIST), WM_COMMAND, M_PROPERTIES, 0);
                  return TRUE;
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

         if ((HWND)wp == GetDlgItem (hDlg, IDC_AGG_LIST))
            Aggregates_ShowPopupMenu ((HWND)wp, ptClient, ptScreen);
         }
         break; 

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_AGG_CREATESET:
               SendMessage (GetDlgItem (hDlg, IDC_AGG_LIST), WM_COMMAND, M_SET_CREATE, 0);
               break;
            }
         break;
      }

   return FALSE;
}


void Aggregates_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns)
{
   switch (lpns->evt)
      {
      case evtRefreshAggregatesEnd:
      case evtRefreshStatusEnd:
      case evtAlertsChanged:
      case evtDestroy:
         UpdateDisplay_Aggregates (FALSE, GetDlgItem (lpns->hwndTarget, IDC_AGG_LIST), lpns->Params.lpi1, lpns->Params.status, NULL, NULL, NULL);
         break;
      }
}


static UINT_PTR procAggregatesList = 0;

LRESULT CALLBACK Aggregates_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procAggregatesList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = CallWindowProc ((WNDPROC)procAggregatesList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procAggregatesList != 0)
            SetWindowLongPtr (hList, GWLP_WNDPROC, procAggregatesList);
         break;

      case WM_COMMAND:
         StartContextCommand (Server_GetWindowForChild (GetParent(hList)),
                              Server_GetServerForChild (GetParent(hList)),
                              Aggregates_GetFocused (GetParent(hList)),
                              LOWORD(wp));
         break;
      }

   return rc;
}


void Aggregates_SubclassList (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   procAggregatesList = GetWindowLongPtr (hList, GWLP_WNDPROC);
   SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)Aggregates_SubclassListProc);
}


void Aggregates_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen)
{
   if (!ptScreen.x && !ptScreen.y)
      {
      RECT rWindow;
      GetWindowRect (hList, &rWindow);
      ptScreen.x = rWindow.left + (rWindow.right -rWindow.left)/2;
      ptScreen.y = rWindow.top + (rWindow.bottom -rWindow.top)/2;
      Aggregates_ShowParticularPopupMenu (hList, ptScreen, NULL);
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
         lpiSelected = (LPIDENT)FastList_GetItemParam (hList, hItem);

      if (lpiSelected && (lpiSelected != (LPIDENT)FL_GetSelectedData (hList)))
         lpiSelected = NULL;

      if (lpiSelected && lpiSelected->fIsServer())
         Server_ShowParticularPopupMenu (hList, ptScreen, lpiSelected);
      else
         Aggregates_ShowParticularPopupMenu (hList, ptScreen, lpiSelected);
      }
}


void Aggregates_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiAggregate)
{
   HMENU hm;

   if (lpiAggregate == NULL)
      hm = TaLocale_LoadMenu (MENU_AGG_NONE);
   else
      hm = TaLocale_LoadMenu (MENU_AGG);

   if (hm != NULL)
      {
      if (lpiAggregate == NULL)
         {
         HMENU hmView = GetSubMenu (hm, 0);

         CheckMenuRadioItem (hmView,
                             M_VIEW_ONEICON, M_VIEW_STATUS,
                             (gr.ivAgg == ivTWOICONS) ? M_VIEW_TWOICONS :
                             (gr.ivAgg == ivONEICON)  ? M_VIEW_ONEICON : M_VIEW_STATUS,
                             MF_BYCOMMAND);
         }

      DisplayContextMenu (hm, ptScreen, hParent);
      }
}

