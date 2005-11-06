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
#include "set_repprop.h"
#include "set_createrep.h"
#include "set_delete.h"
#include "set_release.h"
#include "propcache.h"
#include "display.h"
#include "columns.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   LPIDENT lpiReq;
   LPIDENT lpiRW;
   FILESETSTATUS fs;
   } SET_REPPROP_PARAMS, *LPSET_REPPROP_PARAMS;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_RepSites_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_RepSites_OnInitDialog (HWND hDlg, LPSET_REPPROP_PARAMS prp);
void Filesets_RepSites_OnSelect (HWND hDlg);
void Filesets_RepSites_OnDelete (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_ShowReplication (HWND hDlg, LPIDENT lpiFileset, LPIDENT lpiTarget)
{
   LPSET_REPPROP_INIT_PARAMS lpp = New (SET_REPPROP_INIT_PARAMS);
   memset (lpp, 0x00, sizeof(SET_REPPROP_INIT_PARAMS));
   lpp->lpiReq = lpiFileset;

   StartTask (taskSET_REPPROP_INIT, g.hMain, lpp);
}

void Filesets_OnEndTask_ShowReplication (LPTASKPACKET ptp)
{
   LPSET_REPPROP_INIT_PARAMS lpp = (LPSET_REPPROP_INIT_PARAMS)(ptp->lpUser);

   TCHAR szSvrName[ cchNAME ];
   TCHAR szAggName[ cchNAME ];
   TCHAR szSetName[ cchNAME ];
   lpp->lpiReq->GetServerName (szSvrName);
   lpp->lpiReq->GetAggregateName (szAggName);
   lpp->lpiReq->GetFilesetName (szSetName);

   if (!ptp->rc)
      {
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_FILESET_STATUS, TEXT("%s%s%s"), szSvrName, szAggName, szSetName);
      }
   else if (!lpp->lpiRW) // couldn't find RW fileset entry?
      {
      ErrorDialog (ptp->status, IDS_ERROR_NOT_REPLICATED, TEXT("%s"), szSetName);
      }
   else
      {
      HWND hCurrent;
      if ((hCurrent = PropCache_Search (pcSET_REP, lpp->lpiRW)) != NULL)
         {
         SetFocus (hCurrent);
         }
      else
         {
         LPSET_REPPROP_PARAMS prp = New (SET_REPPROP_PARAMS);
         prp->lpiReq = lpp->lpiReq;
         prp->lpiRW = lpp->lpiRW;
         memcpy (&prp->fs, &lpp->fs, sizeof(lpp->fs));

         LPTSTR pszTitle = FormatString (IDS_SET_REP_TITLE, TEXT("%s"), szSetName);
         LPPROPSHEET psh = PropSheet_Create (pszTitle, FALSE);
         psh->sh.dwFlags |= PSH_NOAPPLYNOW;
         psh->fMadeCaption = TRUE;

         if (PropSheet_AddTab (psh, IDS_SET_REPSITES_TAB, IDD_SET_REPSITES, (DLGPROC)Filesets_RepSites_DlgProc, (LPARAM)prp, TRUE, TRUE))
            {
            PropSheet_ShowModeless (psh);
            }
         }
      }

   Delete (lpp);
}


BOOL CALLBACK Filesets_RepSites_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewRep))
      return FALSE;

   if (AfsAppLib_HandleHelp (IDD_SET_REPSITES, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   if (msg == WM_INITDIALOG_SHEET)
      {
      PropCache_Add (pcSET_REP, ((LPSET_REPPROP_PARAMS)lp)->lpiRW, hDlg);
      }
   else if (msg == WM_DESTROY_SHEET)
      {
      PropCache_Delete (hDlg);
      }
   else
      {
      LPSET_REPPROP_PARAMS prp;
      if ((prp = (LPSET_REPPROP_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
         {
         switch (msg)
            {
            case WM_INITDIALOG:
               FastList_SetTextCallback (GetDlgItem (hDlg, IDC_SET_REP_LIST), GetItemText, &gr.viewRep);
               Filesets_RepSites_OnInitDialog (hDlg, prp);
               NotifyMe (WHEN_SETS_CHANGE, NULL, hDlg, 0);
               break;

            case WM_DESTROY:
               DontNotifyMeEver (hDlg);
               SetWindowLongPtr (hDlg, DWLP_USER, 0);
               Delete (prp);
               break;

            case WM_CONTEXTMENU:
               if ((HWND)wp == GetDlgItem (hDlg, IDC_SET_REP_LIST))
                  {
                  HWND hList = GetDlgItem (hDlg, IDC_SET_REP_LIST);

                  POINT ptScreen;
                  ptScreen.x = LOWORD(lp);
                  ptScreen.y = HIWORD(lp);

                  POINT ptClient;
                  ptClient = ptScreen;
                  ScreenToClient ((HWND)wp, &ptClient);

                  if (FL_HitTestForHeaderBar (hList, ptClient))
                     {
                     HMENU hm = TaLocale_LoadMenu (MENU_COLUMNS);
                     DisplayContextMenu (hm, ptScreen, hList);
                     }
                  }
               break; 

            case WM_COLUMNS_CHANGED:
               HWND hList;
               hList = GetDlgItem (hDlg, IDC_SET_REP_LIST);
               LPIDENT lpiSel;
               lpiSel = (LPIDENT)FL_GetSelectedData (hList);
               FL_RestoreView (hList, &gr.viewRep);
               UpdateDisplay_Replicas (FALSE, hList, prp->lpiRW, lpiSel);
               break;

            case WM_COMMAND:
               switch (LOWORD(wp))
                  {
                  case IDOK:
                  case IDCANCEL:
                  case IDAPPLY:
                     break;

                  case IDC_SET_REPSITE_ADD:
                     Filesets_CreateReplica (prp->lpiRW);
                     break;

                  case IDC_SET_REPSITE_DELETE:
                     Filesets_RepSites_OnDelete (hDlg);
                     break;

                  case IDC_SET_RELEASE:
                     Filesets_Release (prp->lpiRW);
                     break;
                  }
               break;

            case WM_NOTIFY_FROM_DISPATCH:
               UpdateDisplay_Replicas (FALSE, GetDlgItem (hDlg, IDC_SET_REP_LIST), prp->lpiRW, prp->lpiReq);
               Delete ((LPNOTIFYSTRUCT)lp);
               break;

            case WM_NOTIFY:
               switch (((LPNMHDR)lp)->code)
                  {
                  case FLN_COLUMNRESIZE:
                     FL_StoreView (GetDlgItem (hDlg, IDC_SET_REP_LIST), &gr.viewRep);
                     break;

                  case FLN_ITEMSELECT:
                     Filesets_RepSites_OnSelect (hDlg);
                     break;
                  }
               break;
            }
         }
      }

   return FALSE;
}


static LONG_PTR procRepSitesList = 0;

LRESULT CALLBACK Filesets_RepSites_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procRepSitesList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = CallWindowProc ((WNDPROC)procRepSitesList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procRepSitesList != 0)
            SetWindowLongPtr (hList, GWLP_WNDPROC, procRepSitesList);
         break;

      case WM_COMMAND: 
         switch (LOWORD(wp))
            {
            case M_COLUMNS:
               ShowColumnsDialog (GetParent (hList), &gr.viewRep);
               break;
            }
         break; 
      }

   return rc;
}


void Filesets_RepSites_OnInitDialog (HWND hDlg, LPSET_REPPROP_PARAMS prp)
{
   HWND hList = GetDlgItem (hDlg, IDC_SET_REP_LIST);
   if (procRepSitesList == 0)
      procRepSitesList = GetWindowLongPtr (hList, GWLP_WNDPROC);
   SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)Filesets_RepSites_SubclassListProc);

   TCHAR szServer[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   prp->lpiRW->GetServerName (szServer);
   prp->lpiRW->GetAggregateName (szAggregate);
   prp->lpiRW->GetFilesetName (szFileset);
   SetDlgItemText (hDlg, IDC_SET_SERVER, szServer);
   SetDlgItemText (hDlg, IDC_SET_AGGREGATE, szAggregate);
   SetDlgItemText (hDlg, IDC_SET_NAME, szFileset);

   FL_RestoreView (GetDlgItem (hDlg, IDC_SET_REP_LIST), &gr.viewRep);
   UpdateDisplay_Replicas (FALSE, GetDlgItem (hDlg, IDC_SET_REP_LIST), prp->lpiRW, prp->lpiReq);
}


void Filesets_RepSites_OnSelect (HWND hDlg)
{
   LPIDENT lpiSelected = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_SET_REP_LIST));
   EnableWindow (GetDlgItem (hDlg, IDC_SET_REPSITE_DELETE), (lpiSelected != NULL));
}


void Filesets_RepSites_OnDelete (HWND hDlg)
{
   LPIDENT lpiSelected = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_SET_REP_LIST));
   if (lpiSelected)
      Filesets_Delete (lpiSelected);
}

