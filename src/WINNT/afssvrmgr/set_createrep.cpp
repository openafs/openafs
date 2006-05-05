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
#include "set_createrep.h"
#include "columns.h"
#include "svr_window.h"
#include "display.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_CreateReplica_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_CreateReplica_OnInitDialog (HWND hDlg, LPSET_CREATEREP_PARAMS pscp);
void Filesets_CreateReplica_OnSelectServer (HWND hDlg, LPIDENT *plpiTarget);
void Filesets_CreateReplica_OnEndTask_EnumAggregates (HWND hDlg, LPSET_CREATEREP_PARAMS pscp);
void Filesets_CreateReplica_EnableOK (HWND hDlg, LPSET_CREATEREP_PARAMS pscp);
void Filesets_CreateReplica_StartDisplay_Aggregates (HWND hDlg, LPIDENT lpiTarget);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_CreateReplica (LPIDENT lpiSource, LPIDENT lpiTarget)
{
   LPSET_CREATEREP_PARAMS pscp = New (SET_CREATEREP_PARAMS);
   pscp->lpiSource = lpiSource;
   pscp->lpiTarget = lpiTarget;

   INT_PTR rc = ModalDialogParam (IDD_SET_CREATEREP, NULL, (DLGPROC)Filesets_CreateReplica_DlgProc, (LPARAM)pscp);
   if (rc != IDOK)
      {
      Delete (pscp);
      }
   else if (!ASSERT( pscp->lpiTarget && pscp->lpiTarget->fIsAggregate() ))
      {
      Delete (pscp);
      }
   else
      {
      StartTask (taskSET_CREATEREP, NULL, pscp);
      }
}


BOOL CALLBACK Filesets_CreateReplica_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static LPSET_CREATEREP_PARAMS pscp = NULL;
   if (msg == WM_INITDIALOG)
      pscp = (LPSET_CREATEREP_PARAMS)lp;

   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewAggMove))
      return FALSE;

   if (AfsAppLib_HandleHelp (IDD_SET_CREATEREP, hDlg, msg, wp, lp))
      return TRUE;

   if (pscp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            FastList_SetTextCallback (GetDlgItem (hDlg, IDC_AGG_LIST), GetItemText, &gr.viewAggMove);
            Filesets_CreateReplica_OnInitDialog (hDlg, pscp);
            break;

         case WM_ENDTASK:
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskAGG_ENUM_TO_LISTVIEW)
                  Filesets_CreateReplica_OnEndTask_EnumAggregates (hDlg, pscp);
               else if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  {
                  EnableWindow (GetDlgItem (hDlg, IDC_SET_SERVER), TRUE);
                  Filesets_CreateReplica_OnSelectServer (hDlg, &pscp->lpiTarget);
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
            Filesets_CreateReplica_OnSelectServer (hDlg, (LPIDENT*)&lpSel);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  if (pscp->lpiTarget)
                     EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_SET_SERVER:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     Filesets_CreateReplica_OnSelectServer (hDlg, &pscp->lpiTarget);
                  break;
               }
            break;

         case WM_DESTROY:
            FL_StoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggMove);
            pscp = NULL;
            break;

         case WM_NOTIFY: 
            switch (((LPNMHDR)lp)->code)
               { 
               case FLN_ITEMSELECT:
                  LPIDENT lpi;
                  if ((lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST))) != NULL)
                     {
                     pscp->lpiTarget = lpi;
                     Filesets_CreateReplica_EnableOK (hDlg, pscp);
                     }
                  break;
               }
            break;
         }
      }

   return FALSE;
}


static LONG_PTR procFilesetsCreateReplicaList = 0;

LRESULT CALLBACK Filesets_CreateReplica_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procFilesetsCreateReplicaList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = (LRESULT) CallWindowProc ((WNDPROC)procFilesetsCreateReplicaList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procFilesetsCreateReplicaList != 0)
            SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)procFilesetsCreateReplicaList);
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


void Filesets_CreateReplica_OnInitDialog (HWND hDlg, LPSET_CREATEREP_PARAMS pscp)
{
   TCHAR szServer[ cchNAME ];
   TCHAR szAggregate[ cchNAME ];
   TCHAR szFileset[ cchNAME ];
   pscp->lpiSource->GetServerName (szServer);
   pscp->lpiSource->GetAggregateName (szAggregate);
   pscp->lpiSource->GetFilesetName (szFileset);

   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_SET_NAME, szText, cchRESOURCE);
   LPTSTR pszText = FormatString (szText, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
   SetDlgItemText (hDlg, IDC_SET_NAME, pszText);
   FreeString (pszText);

   HWND hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   if (procFilesetsCreateReplicaList == 0)
      procFilesetsCreateReplicaList = GetWindowLongPtr (hList, GWLP_WNDPROC);
   SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)Filesets_CreateReplica_SubclassListProc);

   if (gr.viewAggMove.lvsView == 0) // never initialized this?
      {
      if (gr.viewAggCreate.lvsView == 0)
         memcpy (&gr.viewAggMove, &gr.viewAgg, sizeof(VIEWINFO));
      else
         memcpy (&gr.viewAggMove, &gr.viewAggCreate, sizeof(VIEWINFO));
      }
   FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggMove);

   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_SET_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_LIST), FALSE);

   // Fill in the Servers combobox, and select the default server
   // (if one was specified).
   //
   LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lpp->hCombo = GetDlgItem (hDlg, IDC_SET_SERVER);
   lpp->lpiSelect = (pscp->lpiTarget) ? pscp->lpiTarget->GetServer() : NULL;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpp);
}


void Filesets_CreateReplica_OnSelectServer (HWND hDlg, LPIDENT *plpiTarget)
{
   LPIDENT lpiServerNew = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SET_SERVER));

   SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)lpiServerNew);

   if (*plpiTarget && (*plpiTarget)->fIsServer())
      *plpiTarget = NULL;
   if (*plpiTarget && ((*plpiTarget)->GetServer() != lpiServerNew))
      *plpiTarget = NULL;

   Filesets_CreateReplica_StartDisplay_Aggregates (hDlg, *plpiTarget);
}


void Filesets_CreateReplica_OnEndTask_EnumAggregates (HWND hDlg, LPSET_CREATEREP_PARAMS pscp)
{
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_LIST), TRUE);
   Filesets_CreateReplica_EnableOK (hDlg, pscp);
}


void Filesets_CreateReplica_EnableOK (HWND hDlg, LPSET_CREATEREP_PARAMS pscp)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), (pscp->lpiTarget) ? TRUE : FALSE);
}


void Filesets_CreateReplica_StartDisplay_Aggregates (HWND hDlg, LPIDENT lpiTarget)
{
   LPIDENT lpiServerNew = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SET_SERVER));

   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);

   LPAGG_ENUM_TO_LISTVIEW_PACKET lpp = New (AGG_ENUM_TO_LISTVIEW_PACKET);
   lpp->hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   lpp->lpiServer = lpiServerNew;
   lpp->lpiSelect = lpiTarget;
   lpp->lpvi = &gr.viewAggMove;
   StartTask (taskAGG_ENUM_TO_LISTVIEW, hDlg, lpp);
}

