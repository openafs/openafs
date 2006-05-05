/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "set_move.h"
#include "columns.h"
#include "svr_window.h"
#include "display.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_MoveTo_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_MoveTo_OnInitDialog (HWND hDlg, LPSET_MOVE_PARAMS psmp);
void Filesets_MoveTo_OnSelectServer (HWND hDlg, LPIDENT *plpiTarget);
void Filesets_MoveTo_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSET_MOVE_PARAMS psmp);
void Filesets_MoveTo_OnEndTask_EnumAggregates (HWND hDlg, LPSET_MOVE_PARAMS psmp);
void Filesets_MoveTo_EnableOK (HWND hDlg, LPSET_MOVE_PARAMS psmp);
void Filesets_MoveTo_StartDisplay_Aggregates (HWND hDlg, LPIDENT lpiTarget);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_ShowMoveTo (LPIDENT lpiSource, LPIDENT lpiTarget)
{
   LPSET_MOVE_PARAMS psmp = New (SET_MOVE_PARAMS);
   psmp->lpiSource = lpiSource;
   psmp->lpiTarget = lpiTarget;

   INT_PTR rc = ModalDialogParam (IDD_SET_MOVETO, GetActiveWindow(), (DLGPROC)Filesets_MoveTo_DlgProc, (LPARAM)psmp);
   if (rc != IDOK)
      {
      Delete (psmp);
      }
   else if (!ASSERT( psmp->lpiTarget && psmp->lpiTarget->fIsAggregate() ))
      {
      Delete (psmp);
      }
   else
      {
      StartTask (taskSET_MOVE, NULL, psmp);
      }
}


BOOL CALLBACK Filesets_MoveTo_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static LPSET_MOVE_PARAMS psmp = NULL;
   if (msg == WM_INITDIALOG)
      psmp = (LPSET_MOVE_PARAMS)lp;

   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewAggMove))
      return FALSE;

   if (AfsAppLib_HandleHelp (IDD_SET_MOVETO, hDlg, msg, wp, lp))
      return TRUE;

   if (psmp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            FastList_SetTextCallback (GetDlgItem (hDlg, IDC_AGG_LIST), GetItemText, &gr.viewAggMove);
            Filesets_MoveTo_OnInitDialog (hDlg, psmp);
            StartTask (taskSET_MOVETO_INIT, hDlg, psmp->lpiSource);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskSET_MOVETO_INIT)
                  Filesets_MoveTo_OnEndTask_InitDialog (hDlg, ptp, psmp);
               else if (ptp->idTask == taskAGG_ENUM_TO_LISTVIEW)
                  Filesets_MoveTo_OnEndTask_EnumAggregates (hDlg, psmp);
               else if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  {
                  EnableWindow (GetDlgItem (hDlg, IDC_MOVESET_SERVER), TRUE);
                  Filesets_MoveTo_OnSelectServer (hDlg, &psmp->lpiTarget);
                  }
               FreeTaskPacket (ptp);
               }
            break; 

         case WM_CONTEXTMENU:
            HWND hList;
            hList = GetDlgItem (hDlg, IDC_AGG_LIST);

            POINT ptScreen;
            POINT ptClient;
            ptScreen.x = LOWORD(lp);
            ptScreen.y = HIWORD(lp);

            ptClient = ptScreen;
            ScreenToClient (hList, &ptClient);

            if (FL_HitTestForHeaderBar (hList, ptClient))
               {
               HMENU hm = TaLocale_LoadMenu (MENU_COLUMNS);
               DisplayContextMenu (hm, ptScreen, hList);
               }
            break;

         case WM_COLUMNS_CHANGED:
            LPARAM lpSel;
            lpSel = FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST));
            FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggMove);
            Filesets_MoveTo_OnSelectServer (hDlg, (LPIDENT*)&lpSel);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  if (psmp->lpiTarget)
                     EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_MOVESET_SERVER:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     Filesets_MoveTo_OnSelectServer (hDlg, &psmp->lpiTarget);
                  break;
               }
            break;

         case WM_DESTROY:
            FL_StoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggMove);
            psmp = NULL;
            break;

         case WM_NOTIFY: 
            switch (((LPNMHDR)lp)->code)
               { 
               case FLN_ITEMSELECT:
                  LPIDENT lpi;
                  if ((lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST))) != NULL)
                     {
                     psmp->lpiTarget = lpi;
                     Filesets_MoveTo_EnableOK (hDlg, psmp);
                     }
                  break;
               }
            break;
         }
      }

   return FALSE;
}


static LONG_PTR procFilesetsMoveToList = 0;

LRESULT CALLBACK Filesets_MoveTo_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procFilesetsMoveToList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = (LRESULT) CallWindowProc ((WNDPROC)procFilesetsMoveToList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procFilesetsMoveToList != 0)
            SetWindowLongPtr (hList, GWLP_WNDPROC, procFilesetsMoveToList);
         break;

      case WM_COMMAND: 
         switch (LOWORD(wp))
            {
            case M_COLUMNS:
               ShowColumnsDialog (GetParent (hList), &gr.viewAggMove);
               break;
            }
         break; 
      }

   return rc;
}


void Filesets_MoveTo_OnInitDialog (HWND hDlg, LPSET_MOVE_PARAMS psmp)
{
   HWND hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   if (procFilesetsMoveToList == 0)
      procFilesetsMoveToList = GetWindowLongPtr (hList, GWLP_WNDPROC);
   SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)Filesets_MoveTo_SubclassListProc);

   if (gr.viewAggMove.lvsView == 0) // never initialized this?
      {
      if (gr.viewAggCreate.lvsView == 0)
         memcpy (&gr.viewAggMove, &gr.viewAgg, sizeof(VIEWINFO));
      else
         memcpy (&gr.viewAggMove, &gr.viewAggCreate, sizeof(VIEWINFO));
      }
   FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggMove);

   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_MOVESET_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_LIST), FALSE);
}


void Filesets_MoveTo_OnEndTask_InitDialog (HWND hDlg, LPTASKPACKET ptp, LPSET_MOVE_PARAMS psmp)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   psmp->lpiSource->GetServerName (szServer);
   psmp->lpiSource->GetAggregateName (szAggregate);
   psmp->lpiSource->GetFilesetName (szFileset);

   if (!ptp->rc)
      {
      ErrorDialog (ptp->status, IDS_ERROR_REFRESH_FILESET_STATUS, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      EndDialog (hDlg, IDCANCEL);
      }
   else
      {
      int ids = (TASKDATA(ptp)->fs.Type == ftREADWRITE) ? IDS_MOVESET_READWRITE : IDS_MOVESET_READONLY;
      LPTSTR pszDesc = FormatString (ids, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      SetDlgItemText (hDlg, IDC_MOVESET_DESC, pszDesc);
      FreeString (pszDesc);

      // Fill in the Servers combobox, and select the default server
      // (if one was specified).
      //
      LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = New (SVR_ENUM_TO_COMBOBOX_PACKET);
      lpp->hCombo = GetDlgItem (hDlg, IDC_MOVESET_SERVER);
      lpp->lpiSelect = (psmp->lpiTarget) ? psmp->lpiTarget->GetServer() : NULL;
      StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpp);
      }
}


void Filesets_MoveTo_OnSelectServer (HWND hDlg, LPIDENT *plpiTarget)
{
   LPIDENT lpiServerNew = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_MOVESET_SERVER));

   SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)lpiServerNew);

   if (*plpiTarget && (*plpiTarget)->fIsServer())
      *plpiTarget = NULL;
   if (*plpiTarget && ((*plpiTarget)->GetServer() != lpiServerNew))
      *plpiTarget = NULL;

   Filesets_MoveTo_StartDisplay_Aggregates (hDlg, *plpiTarget);
}


void Filesets_MoveTo_OnEndTask_EnumAggregates (HWND hDlg, LPSET_MOVE_PARAMS psmp)
{
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_LIST), TRUE);
   Filesets_MoveTo_EnableOK (hDlg, psmp);
}


void Filesets_MoveTo_EnableOK (HWND hDlg, LPSET_MOVE_PARAMS psmp)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), (psmp->lpiTarget) ? TRUE : FALSE);
}


void Filesets_MoveTo_StartDisplay_Aggregates (HWND hDlg, LPIDENT lpiTarget)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);

   LPAGG_ENUM_TO_LISTVIEW_PACKET lpp = New (AGG_ENUM_TO_LISTVIEW_PACKET);
   lpp->lpiServer = NULL;
   lpp->hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   lpp->lpiSelect = lpiTarget;
   lpp->lpvi = &gr.viewAggMove;
   StartTask (taskAGG_ENUM_TO_LISTVIEW, hDlg, lpp);
}

