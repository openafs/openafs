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
#include "set_create.h"
#include "set_general.h"
#include "propcache.h"
#include "columns.h"
#include "svr_window.h"
#include "display.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Create_OnInitDialog (HWND hDlg, LPSET_CREATE_PARAMS pscp);
void Filesets_Create_OnSelectServer (HWND hDlg, LPIDENT *plpiTarget);
void Filesets_Create_OnEndTask_FindQuotaLimits (HWND hDlg, LPTASKPACKET ptp, LPSET_CREATE_PARAMS pscp);
void Filesets_Create_OnEndTask_EnumAggregates (HWND hDlg);
void Filesets_Create_StartDisplay_Aggregates (HWND hDlg, LPIDENT *plpiTarget);
void Filesets_Create_EnableOK (HWND hDlg, LPSET_CREATE_PARAMS pscp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_Create (LPIDENT lpiParent)
{
   LPSET_CREATE_PARAMS pscp = New (SET_CREATE_PARAMS);
   memset (pscp, 0x00, sizeof(SET_CREATE_PARAMS));
   pscp->lpiParent = lpiParent;
   pscp->szName[0] = TEXT('\0');
   pscp->fCreateClone = FALSE;
   pscp->ckQuota = ckQUOTA_DEFAULT;

   // Show the Create dialog.
   //
   int rc = ModalDialogParam (IDD_SET_CREATE, GetActiveWindow(), (DLGPROC)Filesets_Create_DlgProc, (LPARAM)pscp);

   if (rc != IDOK)
      {
      Delete (pscp);
      }
   else if (!ASSERT( pscp->lpiParent && pscp->lpiParent->fIsAggregate() ))
      {
      Delete (pscp);
      }
   else if (!ASSERT( pscp->szName[0] != TEXT('\0') ))
      {
      Delete (pscp);
      }
   else
      {
      StartTask (taskSET_CREATE, NULL, pscp);
      }
}


BOOL CALLBACK Filesets_Create_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static LPSET_CREATE_PARAMS pscp = NULL;
   if (msg == WM_INITDIALOG)
      pscp = (LPSET_CREATE_PARAMS)lp;

   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewAggCreate))
      return FALSE;

   if (AfsAppLib_HandleHelp (IDD_SET_CREATE, hDlg, msg, wp, lp))
      return TRUE;

   if (pscp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            FastList_SetTextCallback (GetDlgItem (hDlg, IDC_AGG_LIST), GetItemText, (DWORD)&gr.viewAggCreate);
            Filesets_Create_OnInitDialog (hDlg, pscp);
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

         case WM_ENDTASK: 
            LPTASKPACKET ptp;
            if ((ptp = (LPTASKPACKET)lp) != NULL)
               {
               if (ptp->idTask == taskAGG_FIND_QUOTA_LIMITS)
                  Filesets_Create_OnEndTask_FindQuotaLimits (hDlg, ptp, pscp);
               else if (ptp->idTask == taskAGG_ENUM_TO_LISTVIEW)
                  Filesets_Create_OnEndTask_EnumAggregates (hDlg);
               else if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  {
                  EnableWindow (GetDlgItem (hDlg, IDC_SET_SERVER), TRUE);
                  Filesets_Create_StartDisplay_Aggregates (hDlg, &pscp->lpiParent);
                  }
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COLUMNS_CHANGED:
            LPARAM lpSel;
            lpSel = FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST));
            FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggCreate);
            Filesets_Create_OnSelectServer (hDlg, (LPIDENT*)&lpSel);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  if (pscp->lpiParent && pscp->lpiParent->fIsAggregate() && pscp->szName[0])
                     EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_SET_SERVER:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     Filesets_Create_OnSelectServer (hDlg, &pscp->lpiParent);
                  break;

               case IDC_SET_NAME:
                  GetDlgItemText (hDlg, IDC_SET_NAME, pscp->szName, cchNAME);
                  Filesets_Create_EnableOK (hDlg, pscp);
                  break;

               case IDC_SET_QUOTA:
                  if (HIWORD(wp) == SPN_UPDATE)
                     {
                     pscp->ckQuota = SP_GetPos (GetDlgItem (hDlg, IDC_SET_QUOTA));
                     if (gr.cbQuotaUnits == cb1MB)
                        pscp->ckQuota *= ck1MB;
                     }
                  break;

               case IDC_SET_QUOTA_UNITS:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     {
                     gr.cbQuotaUnits = (size_t)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS));
                     StartTask (taskAGG_FIND_QUOTA_LIMITS, hDlg, pscp->lpiParent);
                     }
                  break;

               case IDC_SET_CLONE:
                  pscp->fCreateClone = IsDlgButtonChecked (hDlg, IDC_SET_CLONE);
                  break;
               }
            break;

         case WM_DESTROY:
            FL_StoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggCreate);
            pscp = NULL;
            break;

         case WM_NOTIFY: 
            switch (((LPNMHDR)lp)->code)
               { 
               case FLN_ITEMSELECT:
                  LPIDENT lpi;
                  if ((lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST))) != NULL)
                     {
                     pscp->lpiParent = lpi;

                     StartTask (taskAGG_FIND_QUOTA_LIMITS, hDlg, pscp->lpiParent);
                     Filesets_Create_EnableOK (hDlg, pscp);
                     }
                  break;
               }
            break;
         }
      }

   return FALSE;
}


static LONG procFilesetsCreateList = 0;

LRESULT CALLBACK Filesets_Create_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procFilesetsCreateList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = CallWindowProc ((WNDPROC)procFilesetsCreateList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procFilesetsCreateList != 0)
            SetWindowLong (hList, GWL_WNDPROC, procFilesetsCreateList);
         break;

      case WM_COMMAND: 
         switch (LOWORD(wp))
            {
            case M_COLUMNS:
               ShowColumnsDialog (GetParent (hList), &gr.viewAggCreate);
               break;
            }
         break; 
      }

   return rc;
}


void Filesets_Create_OnInitDialog (HWND hDlg, LPSET_CREATE_PARAMS pscp)
{
   HWND hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   if (procFilesetsCreateList == 0)
      procFilesetsCreateList = GetWindowLong (hList, GWL_WNDPROC);
   SetWindowLong (hList, GWL_WNDPROC, (LONG)Filesets_Create_SubclassListProc);

   if (gr.viewAggCreate.lvsView == 0)  // never initialized this?
      {
      if (gr.viewAggMove.lvsView == 0)
         memcpy (&gr.viewAggCreate, &gr.viewAgg, sizeof(VIEWINFO));
      else
         memcpy (&gr.viewAggCreate, &gr.viewAggMove, sizeof(VIEWINFO));
      }
   FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggCreate);

   SetDlgItemText (hDlg, IDC_SET_NAME, pscp->szName);

   CB_StartChange (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), TRUE);
   CB_AddItem (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), IDS_QUOTAUNITS_KB, (LPARAM)cb1KB);
   CB_AddItem (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), IDS_QUOTAUNITS_MB, (LPARAM)cb1MB);
   CB_EndChange (GetDlgItem (hDlg, IDC_SET_QUOTA_UNITS), (LPARAM)gr.cbQuotaUnits);

   CheckDlgButton (hDlg, IDC_SET_CLONE, pscp->fCreateClone);

   // Create a spinner, and give it an appropriate min/max range.  We'll
   // have to *find* that range in a separate thread, 'cause it involves
   // getting the status of an aggregate.
   //
   StartTask (taskAGG_FIND_QUOTA_LIMITS, hDlg, pscp->lpiParent);

   // Fill in the Servers combobox, and select the default server
   // (if one was specified).
   //
   LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lpp->hCombo = GetDlgItem (hDlg, IDC_SET_SERVER);
   lpp->lpiSelect = (pscp->lpiParent) ? pscp->lpiParent->GetServer() : NULL;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpp);

   EnableWindow (GetDlgItem (hDlg, IDC_SET_SERVER), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_LIST), FALSE);
   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);
}


void Filesets_Create_OnSelectServer (HWND hDlg, LPIDENT *plpiTarget)
{
   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);

   Filesets_Create_StartDisplay_Aggregates (hDlg, plpiTarget);
}


void Filesets_Create_StartDisplay_Aggregates (HWND hDlg, LPIDENT *plpiTarget)
{
   LPIDENT lpiServerNew = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_SET_SERVER));

   if (*plpiTarget && (*plpiTarget)->fIsServer())
      *plpiTarget = NULL;
   if (*plpiTarget && ((*plpiTarget)->GetServer() != lpiServerNew))
      *plpiTarget = NULL;

   SetWindowLong (hDlg, DWL_USER, (LONG)lpiServerNew);

   LPAGG_ENUM_TO_LISTVIEW_PACKET lpp = New (AGG_ENUM_TO_LISTVIEW_PACKET);
   lpp->lpiServer = lpiServerNew;
   lpp->hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   lpp->lpiSelect = *plpiTarget;
   lpp->lpvi = &gr.viewAggCreate;
   StartTask (taskAGG_ENUM_TO_LISTVIEW, hDlg, lpp);
}


void Filesets_Create_OnEndTask_EnumAggregates (HWND hDlg)
{
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_LIST), TRUE);
}


void Filesets_Create_EnableOK (HWND hDlg, LPSET_CREATE_PARAMS pscp)
{
   if (pscp->lpiParent && pscp->lpiParent->fIsAggregate() && pscp->szName[0])
      EnableWindow (GetDlgItem (hDlg, IDOK), TRUE);
   else
      EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);
}


void Filesets_Create_OnEndTask_FindQuotaLimits (HWND hDlg, LPTASKPACKET ptp, LPSET_CREATE_PARAMS pscp)
{
   size_t cMin = TASKDATA(ptp)->ckMin;
   size_t cNow = pscp->ckQuota;
   size_t cMax = TASKDATA(ptp)->ckMax;

   if (gr.cbQuotaUnits == cb1MB)
      {
      cMin /= ck1MB;
      cNow /= ck1MB;
      cMax /= ck1MB;
      }

   HWND hQuota = GetDlgItem (hDlg, IDC_SET_QUOTA);
   if (!fHasSpinner (hQuota))
      {
      CreateSpinner (hQuota, 10, FALSE, cMin, cNow, cMax);
      }
   else
      {
      SP_SetRange (hQuota, cMin, cMax);
      SP_SetPos (hQuota, cNow);
      }
}

