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
#include "set_restore.h"
#include "set_general.h"
#include "propcache.h"
#include "columns.h"
#include "svr_window.h"
#include "display.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_Restore_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Filesets_Restore_OnInitDialog (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnEndTask_LookupFileset (HWND hDlg, LPTASKPACKET ptp, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnEndTask_EnumServers (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnEndTask_EnumAggregates (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnSelectServer (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnSetName (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnFileName (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnAggregate (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_OnBrowse (HWND hDlg, LPSET_RESTORE_PARAMS psrp);
void Filesets_Restore_EnableOK (HWND hDlg, LPSET_RESTORE_PARAMS psrp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Filesets_Restore (LPIDENT lpiParent)
{
   LPSET_RESTORE_PARAMS psrp = New (SET_RESTORE_PARAMS);
   psrp->lpi = lpiParent;
   psrp->szFileset[0] = TEXT('\0');
   psrp->szFilename[0] = TEXT('\0');
   psrp->fIncremental = FALSE;

   if (lpiParent && lpiParent->fIsFileset())
      lpiParent->GetFilesetName (psrp->szFileset);

   INT_PTR rc = ModalDialogParam (IDD_SET_RESTORE, GetActiveWindow(), (DLGPROC)Filesets_Restore_DlgProc, (LPARAM)psrp);
   if (rc != IDOK)
      {
      Delete (psrp);
      }
   else if (!ASSERT( psrp->lpi != NULL ))
      {
      Delete (psrp);
      }
   else if (!ASSERT( psrp->szFileset[0] != TEXT('\0') ))
      {
      Delete (psrp);
      }
   else if (!ASSERT( psrp->szFilename[0] != TEXT('\0') ))
      {
      Delete (psrp);
      }
   else
      {
      StartTask (taskSET_RESTORE, NULL, psrp);
      }
}


BOOL CALLBACK Filesets_Restore_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static LPSET_RESTORE_PARAMS psrp = NULL;
   if (msg == WM_INITDIALOG)
      psrp = (LPSET_RESTORE_PARAMS)lp;

   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewAggRestore))
      return FALSE;

   if (AfsAppLib_HandleHelp (IDD_SET_RESTORE, hDlg, msg, wp, lp))
      return TRUE;

   if (psrp != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            FastList_SetTextCallback (GetDlgItem (hDlg, IDC_AGG_LIST), GetItemText, &gr.viewAggRestore);
            Filesets_Restore_OnInitDialog (hDlg, psrp);
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
               if (ptp->idTask == taskSET_LOOKUP)
                  Filesets_Restore_OnEndTask_LookupFileset (hDlg, ptp, psrp);
               else if (ptp->idTask == taskSVR_ENUM_TO_COMBOBOX)
                  Filesets_Restore_OnEndTask_EnumServers (hDlg, psrp);
               else if (ptp->idTask == taskAGG_ENUM_TO_LISTVIEW)
                  Filesets_Restore_OnEndTask_EnumAggregates (hDlg, psrp);
               FreeTaskPacket (ptp);
               }
            break;

         case WM_COLUMNS_CHANGED:
            LPARAM lpSel;
            lpSel = FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST));
            FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggRestore);
            Filesets_Restore_OnSelectServer (hDlg, psrp);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_RESTORE_INCREMENTAL:
                  psrp->fIncremental = IsDlgButtonChecked (hDlg, IDC_RESTORE_INCREMENTAL);
                  break;

               case IDC_RESTORE_SERVER:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     Filesets_Restore_OnSelectServer (hDlg, psrp);
                  break;

               case IDC_RESTORE_SETNAME:
                  if (HIWORD(wp) == EN_UPDATE)
                     Filesets_Restore_OnSetName (hDlg, psrp);
                  break;

               case IDC_RESTORE_FILENAME:
                  Filesets_Restore_OnFileName (hDlg, psrp);
                  break;

               case IDC_RESTORE_BROWSE:
                  Filesets_Restore_OnBrowse (hDlg, psrp);
                  break;
               }
            break;

         case WM_DESTROY:
            FL_StoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggRestore);
            psrp = NULL;
            break;

         case WM_NOTIFY: 
            switch (((LPNMHDR)lp)->code)
               { 
               case FLN_ITEMSELECT:
                  Filesets_Restore_OnAggregate (hDlg, psrp);
                  break;
               }
            break;
         }
      }

   return FALSE;
}


static LONG_PTR procFilesetsRestoreList = 0;

LRESULT CALLBACK Filesets_Restore_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procFilesetsRestoreList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = (LRESULT) CallWindowProc ((WNDPROC)procFilesetsRestoreList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procFilesetsRestoreList != 0)
            SetWindowLongPtr (hList, GWLP_WNDPROC, procFilesetsRestoreList);
         break;

      case WM_COMMAND: 
         switch (LOWORD(wp))
            {
            case M_COLUMNS:
               ShowColumnsDialog (GetParent (hList), &gr.viewAggRestore);
               break;
            }
         break; 
      }

   return rc;
}


void Filesets_Restore_OnInitDialog (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   HWND hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   if (procFilesetsRestoreList == 0)
      procFilesetsRestoreList = GetWindowLongPtr (hList, GWLP_WNDPROC);
   SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)Filesets_Restore_SubclassListProc);

   if (gr.viewAggRestore.lvsView == 0)  // never initialized this?
      {
      if (gr.viewAggMove.lvsView != 0)
         memcpy (&gr.viewAggRestore, &gr.viewAggMove, sizeof(VIEWINFO));
      else if (gr.viewAggCreate.lvsView != 0)
         memcpy (&gr.viewAggRestore, &gr.viewAggCreate, sizeof(VIEWINFO));
      else
         memcpy (&gr.viewAggRestore, &gr.viewAgg, sizeof(VIEWINFO));
      }

   FL_RestoreView (GetDlgItem (hDlg, IDC_AGG_LIST), &gr.viewAggRestore);

   SetDlgItemText (hDlg, IDC_RESTORE_FILENAME, psrp->szFilename);
   SetDlgItemText (hDlg, IDC_RESTORE_SETNAME, psrp->szFileset);

   SetDlgItemText (hDlg, IDC_RESTORE_CREATE, TEXT(""));
   Filesets_Restore_OnEndTask_LookupFileset (hDlg, NULL, psrp);

   CheckDlgButton (hDlg, IDC_RESTORE_INCREMENTAL, psrp->fIncremental);

   // Fill in the Servers combobox, and select the default server
   // (if one was specified).
   //
   LPSVR_ENUM_TO_COMBOBOX_PACKET lpp = New (SVR_ENUM_TO_COMBOBOX_PACKET);
   lpp->hCombo = GetDlgItem (hDlg, IDC_RESTORE_SERVER);
   lpp->lpiSelect = (psrp->lpi) ? psrp->lpi->GetServer() : NULL;
   StartTask (taskSVR_ENUM_TO_COMBOBOX, hDlg, lpp);
}


void Filesets_Restore_OnSelectServer (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   LPIDENT lpiServerNew = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_RESTORE_SERVER));

   LPIDENT lpiSelect = NULL;
   if (psrp->lpi && !psrp->lpi->fIsServer())
      {
      if (psrp->lpi->GetServer() == lpiServerNew)
         lpiSelect = psrp->lpi->GetAggregate();
      }

   SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)lpiServerNew);

   LPAGG_ENUM_TO_LISTVIEW_PACKET lpp = New (AGG_ENUM_TO_LISTVIEW_PACKET);
   lpp->lpiServer = lpiServerNew;
   lpp->hList = GetDlgItem (hDlg, IDC_AGG_LIST);
   lpp->lpiSelect = lpiSelect;
   lpp->lpvi = &gr.viewAggRestore;
   StartTask (taskAGG_ENUM_TO_LISTVIEW, hDlg, lpp);
}


void Filesets_Restore_OnEndTask_EnumServers (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   Filesets_Restore_OnSelectServer (hDlg, psrp);
}


void Filesets_Restore_OnEndTask_EnumAggregates (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   ;
}


void Filesets_Restore_OnEndTask_LookupFileset (HWND hDlg, LPTASKPACKET ptp, LPSET_RESTORE_PARAMS psrp)
{
   if (ptp)
      {
      psrp->lpi = (ptp->rc) ? TASKDATA(ptp)->lpi : NULL;
      }
   if (!psrp->lpi)
      {
      psrp->lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST));
      }

   BOOL fCreate = (psrp->lpi && psrp->lpi->fIsFileset()) ? FALSE : TRUE;

   TCHAR szFileset[ cchNAME ];
   GetDlgItemText (hDlg, IDC_RESTORE_SETNAME, szFileset, cchNAME);

   LPTSTR pszText;
   if (szFileset[0] == TEXT('\0'))
      {
      pszText = CloneString (TEXT(""));
      }
   else if (fCreate)
      {
      pszText = FormatString (IDS_RESTORE_CREATESET, TEXT("%s"), szFileset);
      }
   else
      {
      TCHAR szServer[ cchNAME ];
      TCHAR szAggregate[ cchNAME ];
      psrp->lpi->GetServerName (szServer);
      psrp->lpi->GetAggregateName (szAggregate);
      pszText = FormatString (IDS_RESTORE_OVERWRITESET, TEXT("%s%s%s"), szServer, szAggregate, szFileset);
      }
   SetDlgItemText (hDlg, IDC_RESTORE_CREATE, pszText);
   FreeString (pszText);

   EnableWindow (GetDlgItem (hDlg, IDC_RESTORE_SERVER), fCreate);
   EnableWindow (GetDlgItem (hDlg, IDC_AGG_LIST), fCreate);

   if (psrp->lpi)
      {
      LPIDENT lpiServer = (LPIDENT)CB_GetSelectedData (GetDlgItem (hDlg, IDC_RESTORE_SERVER));

      if (psrp->lpi->GetServer() != lpiServer)
         {
         CB_SetSelectedByData (GetDlgItem (hDlg, IDC_RESTORE_SERVER), (LPARAM)psrp->lpi->GetServer());
         Filesets_Restore_OnSelectServer (hDlg, psrp);
         }
      else if (!psrp->lpi->fIsServer())
         {
         FL_SetSelectedByData (GetDlgItem (hDlg, IDC_AGG_LIST), (LPARAM)psrp->lpi->GetAggregate());
         }
      }

   Filesets_Restore_EnableOK (hDlg, psrp);
}


void Filesets_Restore_OnFileName (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   GetDlgItemText (hDlg, IDC_RESTORE_FILENAME, psrp->szFilename, MAX_PATH);
   Filesets_Restore_EnableOK (hDlg, psrp);
}


void Filesets_Restore_OnSetName (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   GetDlgItemText (hDlg, IDC_RESTORE_SETNAME, psrp->szFileset, cchNAME);
   EnableWindow (GetDlgItem (hDlg, IDOK), FALSE);

   LPSET_LOOKUP_PACKET lpp = New (SET_LOOKUP_PACKET);
   lstrcpy (lpp->szFileset, psrp->szFileset);
   StartTask (taskSET_LOOKUP, hDlg, lpp);
}


void Filesets_Restore_OnAggregate (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   if (!( psrp->lpi && psrp->lpi->fIsFileset() ))
      {
      psrp->lpi = (LPIDENT)FL_GetSelectedData (GetDlgItem (hDlg, IDC_AGG_LIST));
      }

   Filesets_Restore_EnableOK (hDlg, psrp);
}


void Filesets_Restore_EnableOK (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   BOOL fEnable = TRUE;

   if (psrp->szFilename[0] == TEXT('\0'))
      fEnable = FALSE;

   if (psrp->szFileset[0] == TEXT('\0'))
      fEnable = FALSE;

   if (!psrp->lpi || psrp->lpi->fIsServer())
      fEnable = FALSE;

   EnableWindow (GetDlgItem (hDlg, IDOK), fEnable);
}


void Filesets_Restore_OnBrowse (HWND hDlg, LPSET_RESTORE_PARAMS psrp)
{
   TCHAR szFilter[ cchRESOURCE ];
   GetString (szFilter, IDS_RESTORE_FILTER);
   TCHAR chFilter = szFilter[ lstrlen(szFilter)-1 ];
   for (LPTSTR pszFilter = szFilter;
        (*pszFilter) && ((pszFilter = (LPTSTR)lstrchr (pszFilter, chFilter)) != NULL);
        ++pszFilter)
      {
      *pszFilter = TEXT('\0');
      }

   TCHAR szFilename[ MAX_PATH ];
   GetDlgItemText (hDlg, IDC_RESTORE_FILENAME, szFilename, MAX_PATH);

   OPENFILENAME ofn;
   memset (&ofn, 0x00, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hDlg;
   ofn.hInstance = THIS_HINST;
   ofn.lpstrFilter = szFilter;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = szFilename;
   ofn.nMaxFile = MAX_PATH;
   ofn.Flags = OFN_HIDEREADONLY | OFN_NOREADONLYRETURN |
               OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
   ofn.lpstrDefExt = TEXT("dmp");

   if (GetOpenFileName (&ofn))
      {
      SetDlgItemText (hDlg, IDC_RESTORE_FILENAME, szFilename);
      Filesets_Restore_OnFileName (hDlg, psrp);
      }
}

