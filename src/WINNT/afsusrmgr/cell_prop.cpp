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

#include "TaAfsUsrMgr.h"
#include "cell_prop.h"
#include "winlist.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define nID_USERMAX_MIN    1
#define nID_USERMAX_MAX   -1

#define nID_GROUPMAX_MIN  -0x7FFFFFFF
#define nID_GROUPMAX_MAX  -1


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK CellProp_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void CellProp_General_OnInitDialog (HWND hDlg);
void CellProp_General_OnDestroy (HWND hDlg);
void CellProp_General_OnApply (HWND hDlg);
void CellProp_General_UpdateDialog (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Cell_ShowProperties (CELLPROPTAB cptTarget)
{
   HWND hSheet = NULL;

   // If there is already a window open for this cell, switch to it.
   //
   if ((hSheet = WindowList_Search (wltCELL_PROPERTIES, g.idCell)) != NULL)
      {
      SetForegroundWindow (hSheet);
      if (cptTarget != cptANY)
         {
         HWND hTab = GetDlgItem (hSheet, IDC_PROPSHEET_TABCTRL);
         int nTabs = TabCtrl_GetItemCount (hTab);
         if (nTabs < nCELLPROPTAB_MAX)
            cptTarget = (CELLPROPTAB)( (cptTarget == cptPROBLEMS) ? ((int)cptGENERAL-1) : ((int)cptTarget-1) );
         TabCtrl_SwitchToTab (hTab, cptTarget);
         }
      return;
      }

   // Okay, we're clear to create the new properties window.
   //
   TCHAR szCell[ cchRESOURCE ];
   asc_CellNameGet_Fast (g.idClient, g.idCell, szCell);

   LPTSTR pszTitle = FormatString (IDS_CELL_PROPERTIES_TITLE, TEXT("%s"), szCell);

   LPPROPSHEET psh = PropSheet_Create (pszTitle, TRUE, NULL, (LPARAM)g.idCell);
   PropSheet_AddTab (psh, 0, IDD_CELL_GENERAL, (DLGPROC)CellProp_General_DlgProc, (LPARAM)g.idCell, TRUE, (cptTarget == cptGENERAL) ? TRUE : FALSE);
   PropSheet_ShowModeless (psh, SW_SHOW);
}


BOOL CALLBACK CellProp_General_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_CELL_GENERAL, hDlg, msg, wp, lp))
      return TRUE;

   ASID idCell = (ASID)PropSheet_FindTabParam (hDlg);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         WindowList_Add (hDlg, wltCELL_PROPERTIES, idCell);
         break;

      case WM_DESTROY_SHEET:
         WindowList_Remove (hDlg);
         break;

      case WM_INITDIALOG:
         CellProp_General_OnInitDialog (hDlg);
         break;

      case WM_DESTROY:
         CellProp_General_OnDestroy (hDlg);
         break;

      case WM_ASC_NOTIFY_OBJECT:
         CellProp_General_UpdateDialog (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               CellProp_General_OnApply (hDlg);
               break;

            case IDC_CELL_USERMAX:
            case IDC_CELL_GROUPMAX:
               PropSheetChanged (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void CellProp_General_OnInitDialog (HWND hDlg)
{
   ASID idCell = (ASID)PropSheet_FindTabParam (hDlg);

   // Indicate we want to know if anything changes with this cell
   //
   LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
   memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
   pTask->hNotify = hDlg;
   asc_AsidListCreate (&pTask->pAsidList);
   asc_AsidListAddEntry (&pTask->pAsidList, idCell, 0);
   StartTask (taskOBJECT_LISTEN, NULL, pTask);

   // Fix the name shown on the dialog
   //
   TCHAR szText[ cchRESOURCE ];
   GetDlgItemText (hDlg, IDC_CELL_NAME, szText, cchRESOURCE);

   TCHAR szName[ cchRESOURCE ];
   asc_CellNameGet_Fast (g.idClient, idCell, szName);

   LPTSTR pszText = FormatString (szText, TEXT("%s"), szName);
   SetDlgItemText (hDlg, IDC_CELL_NAME, pszText);
   FreeString (pszText);

   // Fill in the rest of the information about the selected users
   //
   CellProp_General_UpdateDialog (hDlg);
}


void CellProp_General_OnDestroy (HWND hDlg)
{
   // Indicate we no longer care if anything changes with this cell
   //
   LPOBJECT_LISTEN_PARAMS pTask = New (OBJECT_LISTEN_PARAMS);
   memset (pTask, 0x00, sizeof(OBJECT_LISTEN_PARAMS));
   pTask->hNotify = hDlg;
   StartTask (taskOBJECT_LISTEN, NULL, pTask);
}


void CellProp_General_UpdateDialog (HWND hDlg)
{
   ASID idCell = (ASID)PropSheet_FindTabParam (hDlg);

   ULONG status;
   ASOBJPROP Properties;
   asc_ObjectPropertiesGet_Fast (g.idClient, idCell, idCell, &Properties, &status);

   if (fHasSpinner (GetDlgItem (hDlg, IDC_CELL_USERMAX)))
      SP_SetPos (GetDlgItem (hDlg, IDC_CELL_USERMAX), Properties.u.CellProperties.idUserMax);
   else
      CreateSpinner (GetDlgItem (hDlg, IDC_CELL_USERMAX), 10, FALSE, nID_USERMAX_MIN, Properties.u.CellProperties.idUserMax, nID_USERMAX_MAX);

   if (fHasSpinner (GetDlgItem (hDlg, IDC_CELL_GROUPMAX)))
      SP_SetPos (GetDlgItem (hDlg, IDC_CELL_GROUPMAX), Properties.u.CellProperties.idGroupMax);
   else
      CreateSpinner (GetDlgItem (hDlg, IDC_CELL_GROUPMAX), 10, TRUE, nID_GROUPMAX_MIN, Properties.u.CellProperties.idGroupMax, nID_GROUPMAX_MAX);
}


void CellProp_General_OnApply (HWND hDlg)
{
   ASID idCell = (ASID)PropSheet_FindTabParam (hDlg);

   LPCELL_CHANGE_PARAMS pTask = New (CELL_CHANGE_PARAMS);
   memset (pTask, 0x00, sizeof(CELL_CHANGE_PARAMS));
   pTask->idCell = idCell;
   pTask->idUserMax = SP_GetPos (GetDlgItem (hDlg, IDC_CELL_USERMAX));
   pTask->idGroupMax = SP_GetPos (GetDlgItem (hDlg, IDC_CELL_GROUPMAX));
   StartTask (taskCELL_CHANGE, NULL, pTask);
}

