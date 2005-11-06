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
#include "subset.h"
#include "propcache.h"


#define REGVAL_INCLUSIVE   TEXT("Inclusive List")

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Subsets_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Subsets_OnInitDialog (HWND hDlg, LPSUBSET sub);
void Subsets_OnApply (HWND hDlg, LPSUBSET sub);
LPSUBSET Subsets_OnLoad (HWND hDlg, LPSUBSET sub);
void Subsets_OnSave (HWND hDlg, LPSUBSET sub);
void Subsets_SetName (HWND hDlg, LPSUBSET sub);
void Subsets_PutSubsetOnDialog (HWND hDlg, LPSUBSET sub);
void Subsets_GetSubsetFromDialog (HWND hDlg, LPSUBSET sub);
LPSUBSET Subsets_OnCheck (HWND hDlg, int iSel, LPSUBSET subOld);
LPSUBSET Subsets_OnCheckAll (HWND hDlg, LPSUBSET subOld, BOOL fCheck);

BOOL Subsets_GetLoadName (HWND hParent, LPTSTR pszSubset);
BOOL Subsets_GetSaveName (HWND hParent, LPTSTR pszSubset);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL Subsets_fMonitorServer (LPSUBSET sub, LPIDENT lpiServer)
{
   BOOL fMonitor = TRUE;

   TCHAR szLong[ cchNAME ];
   TCHAR szShort[ cchNAME ];
   lpiServer->GetLongServerName (szLong);
   lpiServer->GetShortServerName (szShort);

   if (sub)
      {
      if (sub->pszMonitored)
         {
         fMonitor = FALSE;  // unless it shows up here.

         for (LPTSTR psz = sub->pszMonitored; !fMonitor && *psz; psz += 1+lstrlen(psz))
            {
            if (!lstrcmpi (psz, szLong))
               fMonitor = TRUE;
            else if (!lstrcmpi (psz, szShort))
               fMonitor = TRUE;
            }
         }
      else if (sub->pszUnmonitored)
         {
         for (LPTSTR psz = sub->pszUnmonitored; fMonitor && *psz; psz += 1+lstrlen(psz))
            {
            if (!lstrcmpi (psz, szLong))
               fMonitor = FALSE;
            else if (!lstrcmpi (psz, szShort))
               fMonitor = FALSE;
            }
         }
      }

   return fMonitor;
}


LPSUBSET Subsets_SetMonitor (LPSUBSET sub, LPIDENT lpiServer, BOOL fMonitor)
{
   if (sub == NULL)
      {
      sub = New (SUBSET);
      memset (sub, 0x00, sizeof(SUBSET));
      }

   if (fMonitor != Subsets_fMonitorServer (sub, lpiServer))
      {
      sub->fModified = TRUE;

      TCHAR szLong[ cchNAME ];
      TCHAR szShort[ cchNAME ];
      lpiServer->GetShortServerName (szShort);
      lpiServer->GetLongServerName (szLong);

      // First ensure that the server name doesn't appear anywhere
      // in the subset.
      //
      LPTSTR pszMonitoredNew = NULL;
      LPTSTR pszUnmonitoredNew = NULL;

      if (sub->pszMonitored)
         {
         for (LPTSTR psz = sub->pszMonitored; *psz; psz += 1+lstrlen(psz))
            {
            if (lstrcmpi (psz, szLong) && lstrcmpi (psz, szShort))
               {
               FormatMultiString (&pszMonitoredNew, TRUE, TEXT("%1"), TEXT("%s"), psz);
               }
            }
         }
      else if (sub->pszUnmonitored)
         {
         for (LPTSTR psz = sub->pszUnmonitored; *psz; psz += 1+lstrlen(psz))
            {
            if (lstrcmpi (psz, szLong) && lstrcmpi (psz, szShort))
               {
               FormatMultiString (&pszUnmonitoredNew, TRUE, TEXT("%1"), TEXT("%s"), psz);
               }
            }
         }

      // Then ensure it shows up only where necessary.
      //
      if (sub->pszMonitored && fMonitor)
         {
         FormatMultiString (&pszMonitoredNew, TRUE, TEXT("%1"), TEXT("%s"), szLong);
         }
      else if (!sub->pszMonitored && !fMonitor)
         {
         FormatMultiString (&pszUnmonitoredNew, TRUE, TEXT("%1"), TEXT("%s"), szLong);
         }

      if (sub->pszMonitored && !pszMonitoredNew)
         {
         pszMonitoredNew = AllocateString (2);
         pszMonitoredNew[0] = TEXT('\0');
         pszMonitoredNew[1] = TEXT('\0');
         }
      if (sub->pszUnmonitored && !pszUnmonitoredNew)
         {
         pszUnmonitoredNew = AllocateString (2);
         pszUnmonitoredNew[0] = TEXT('\0');
         pszUnmonitoredNew[1] = TEXT('\0');
         }

      // Finally, update the subset's members.
      //
      if (sub->pszMonitored)
         FreeString (sub->pszMonitored);
      if (sub->pszUnmonitored)
         FreeString (sub->pszUnmonitored);

      sub->pszMonitored = pszMonitoredNew;
      sub->pszUnmonitored = pszUnmonitoredNew;
      }

   return sub;
}


void ShowSubsetsDialog (void)
{
   LPPROPSHEET psh = PropSheet_Create (IDS_SUBSET_TAB, FALSE);
   psh->sh.hwndParent = g.hMain;

   LPSUBSET sub = Subsets_CopySubset (g.sub);
   PropSheet_AddTab (psh, 0, IDD_SUBSETS, (DLGPROC)Subsets_DlgProc, (LPARAM)sub, TRUE);
   PropSheet_ShowModal (psh, PumpMessage);
}


BOOL CALLBACK Subsets_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_SUBSETS, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   LPSUBSET sub = (LPSUBSET)GetWindowLongPtr (hDlg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcGENERAL, NULL, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         Subsets_OnInitDialog (hDlg, sub);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Subsets_OnApply (hDlg, sub);
               break;

            case IDOK:
            case IDCANCEL:
               EndDialog (hDlg, LOWORD(wp));
               break;

            case IDC_SUBSET_LOAD:
               LPSUBSET subNew;
               subNew = Subsets_OnLoad (hDlg, sub);
               if (subNew != NULL)
                  {
                  if (sub)
                     Subsets_FreeSubset (sub);
                  SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)subNew);
                  }
               break;

            case IDC_SUBSET_LIST:
               if (HIWORD(wp) == LBN_CLICKED)  // checked or unchecked?
                  {
                  int iSel = LB_GetSelected (GetDlgItem (hDlg, IDC_SUBSET_LIST));
                  subNew = Subsets_OnCheck (hDlg, iSel, sub);
                  if (subNew && (subNew != sub))
                     {
                     if (sub)
                        Subsets_FreeSubset (sub);
                     SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)subNew);
                     }
                  }
               break;

            case IDC_SUBSET_SAVE:
               Subsets_OnSave (hDlg, sub);
               break;

            case IDC_SUBSET_ALL:
            case IDC_SUBSET_NONE:
               subNew = Subsets_OnCheckAll (hDlg, sub, (LOWORD(wp) == IDC_SUBSET_ALL) ? TRUE : FALSE);
               if (subNew && (subNew != sub))
                  {
                  if (sub)
                     Subsets_FreeSubset (sub);
                  SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)subNew);
                  }
               break;
            }
         break;
      }

   return FALSE;
}


void Subsets_OnInitDialog (HWND hDlg, LPSUBSET sub)
{
   Subsets_SetName (hDlg, sub);
   Subsets_PutSubsetOnDialog (hDlg, sub);
}


void Subsets_SetName (HWND hDlg, LPSUBSET sub)
{
   LPTSTR pszText;

   BOOL fIsOneServer = FALSE;
   if (sub && sub->pszMonitored && *(sub->pszMonitored))
      {
      LPTSTR pszNext = &sub->pszMonitored[ 1+lstrlen(sub->pszMonitored) ];
      if (!*pszNext)
         fIsOneServer = TRUE;
      }

   if (sub && sub->szSubset[0])
      {
      if (sub->fModified)
         pszText = FormatString (IDS_SUBSET_CHANGED, TEXT("%s"), sub->szSubset);
      else
         pszText = FormatString (TEXT("%1"), TEXT("%s"), sub->szSubset);
      }
   else if (fIsOneServer)
      {
      pszText = FormatString (IDS_SUBSET_SERVERSUBSET, TEXT("%s"), sub->pszMonitored);
      }
   else if (sub) // && !sub->szSubset[0]
      {
      if (sub->fModified)
         pszText = FormatString (IDS_SUBSET_CHANGED, TEXT("%m"), IDS_SUBSET_NONAME);
      else
         pszText = FormatString (TEXT("%1"), TEXT("%m"), IDS_SUBSET_NONAME);
      }
   else // no current subset specified
      {
      pszText = FormatString (TEXT("%1"), TEXT("%m"), IDS_SUBSET_NOSUBSET);
      }

   SetDlgItemText (hDlg, IDC_SUBSET_NAME, pszText);
   FreeString (pszText);
}


void Subsets_PutSubsetOnDialog (HWND hDlg, LPSUBSET sub)
{
   LPSUBSET_TO_LIST_PACKET lpp = New (SUBSET_TO_LIST_PACKET);
   memset (lpp, 0x00, sizeof(SUBSET_TO_LIST_PACKET));

   lpp->hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);
   lpp->sub = Subsets_CopySubset (sub);

   StartTask (taskSUBSET_TO_LIST, NULL, lpp);
}


void Subsets_GetSubsetFromDialog (HWND hDlg, LPSUBSET sub)
{
   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);

   if (sub->pszMonitored)
      {
      FreeString (sub->pszMonitored);
      sub->pszMonitored = NULL;
      }
   if (sub->pszUnmonitored)
      {
      FreeString (sub->pszUnmonitored);
      sub->pszUnmonitored = NULL;
      }

   // Is there only one server box checked?
   //
   int iiMax = (int) SendMessage (hList, LB_GETCOUNT, 0, 0);

   size_t cChecked = 0;
   int iiChecked;
   for (int ii = 0; ii < iiMax; ++ii)
      {
      if (LB_GetCheck (hList, ii))
         {
         iiChecked = ii;
         if ((++cChecked) > 1)
            break;
         }
      }
   if (cChecked == 1)  // Only one is checked--use pszMonitored.
      {
      TCHAR szServer[ cchNAME ];
      SendMessage (hList, LB_GETTEXT, iiChecked, (LPARAM)szServer); 
      FormatMultiString (&sub->pszMonitored, TRUE, TEXT("%1"), TEXT("%s"), szServer);
      }
   else // Use pszUnmonitored.
      {
      for (int ii = 0; ii < iiMax; ++ii)
         {
         if (!LB_GetCheck (hList, ii))
            {
            TCHAR szServer[ cchNAME ];
            SendMessage (hList, LB_GETTEXT, ii, (LPARAM)szServer); 
            FormatMultiString (&sub->pszUnmonitored, TRUE, TEXT("%1"), TEXT("%s"), szServer);
            }
         }
      }
}


LPSUBSET Subsets_OnCheck (HWND hDlg, int iSel, LPSUBSET subOld)
{
   LPSUBSET sub = subOld;
   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);

   if (!LB_GetCheck (hList, iSel))  // unchecked?
      {
      if (!sub)
         {
         sub = New (SUBSET);
         memset (sub, 0x00, sizeof(SUBSET));
         }
      }

   if (sub)
      {
      sub->fModified = TRUE;
      Subsets_GetSubsetFromDialog (hDlg, sub);
      Subsets_SetName (hDlg, sub);
      PropSheetChanged (hDlg);
      }

   return sub;
}


LPSUBSET Subsets_OnCheckAll (HWND hDlg, LPSUBSET subOld, BOOL fCheck)
{
   LPSUBSET sub = subOld;
   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);

   if (!fCheck)  // unchecking things?
      {
      if (!sub)
         {
         sub = New (SUBSET);
         memset (sub, 0x00, sizeof(SUBSET));
         }
      }

   int iiMax = (int) SendMessage (hList, LB_GETCOUNT, 0, 0);

   for (int ii = 0; ii < iiMax; ++ii)
      {
      if (LB_GetCheck (hList, ii) != fCheck)
         {
         LB_SetCheck (hList, ii, fCheck);
         }
      }

   if (sub)
      {
      sub->fModified = TRUE;
      Subsets_GetSubsetFromDialog (hDlg, sub);
      Subsets_SetName (hDlg, sub);
      PropSheetChanged (hDlg);
      }

   return sub;
}


LPSUBSET Subsets_OnLoad (HWND hDlg, LPSUBSET subOld)
{
   LPSUBSET subNew = NULL;

   TCHAR szSubset[ cchNAME ] = TEXT("");
   if (subOld)
      lstrcpy (szSubset, subOld->szSubset);

   if (Subsets_GetLoadName (hDlg, szSubset))
      {
      if ((subNew = Subsets_LoadSubset (NULL, szSubset)) != NULL)
         {
         Subsets_SetName (hDlg, subNew);
         Subsets_PutSubsetOnDialog (hDlg, subNew);
         PropSheetChanged (hDlg);
         }
      }

   return subNew;
}


void Subsets_OnSave (HWND hDlg, LPSUBSET sub)
{
   if (sub)
      {
      TCHAR szSubset[ cchNAME ];
      lstrcpy (szSubset, sub->szSubset);

      if (Subsets_GetSaveName (hDlg, szSubset))
         {
         Subsets_GetSubsetFromDialog (hDlg, sub);

         if (Subsets_SaveSubset (NULL, szSubset, sub))
            {
            lstrcpy (sub->szSubset, szSubset);
            sub->fModified = FALSE;

            Subsets_SetName (hDlg, sub);
            }
         }
      }
}


void Subsets_OnApply (HWND hDlg, LPSUBSET sub)
{
   LPSUBSET subCopy;
   if ((subCopy = Subsets_CopySubset (sub, TRUE)) != NULL)
      {
      Subsets_GetSubsetFromDialog (hDlg, subCopy);
      }

   LPSUBSET subOld = g.sub;
   g.sub = subCopy;
   if (subOld)
      Subsets_FreeSubset (subOld);

   StartTask (taskAPPLY_SUBSET, NULL, sub);
}


BOOL Subsets_SaveIfDirty (LPSUBSET sub)
{
   if (!sub || !sub->fModified)
      return TRUE;

   int rc;
   rc = Message (MB_YESNOCANCEL | MB_ICONQUESTION, IDS_SUBSET_DISCARD_TITLE, IDS_SUBSET_DISCARD_DESC);

   if (rc == IDNO)
      {
      sub->fModified = FALSE;
      sub->szSubset[0] = TEXT('\0');
      }
   else if (rc == IDYES)
      {
      TCHAR szSubset[ cchNAME ];
      lstrcpy (szSubset, sub->szSubset);

      if (!Subsets_GetSaveName (g.hMain, szSubset))
         return FALSE;

      if (!Subsets_SaveSubset (NULL, szSubset, sub))
         return FALSE;

      sub->fModified = FALSE;
      lstrcpy (sub->szSubset, szSubset);
      }
   else // (rc == IDCANCEL)
      {
      return FALSE;
      }

   return TRUE;
}


BOOL Subsets_EnumSubsets (LPTSTR pszCell, size_t iIndex, LPTSTR pszSubset)
{
   BOOL rc = FALSE;

   HKEY hk;
   if ((hk = OpenSubsetsKey (pszCell, FALSE)) != NULL)
      {
      if (RegEnumKey (hk, (DWORD)iIndex, pszSubset, cchNAME) == 0)
         rc = TRUE;

      RegCloseKey (hk);
      }

   return rc;
}


BOOL Subsets_SaveSubset (LPTSTR pszCell, LPTSTR pszSubset, LPSUBSET sub)
{
   BOOL rc = FALSE;

   if (sub && pszSubset && *pszSubset)
      {
      HKEY hk;
      if ((hk = OpenSubsetsSubKey (pszCell, pszSubset, TRUE)) != NULL)
         {
         DWORD dwMonitored = (sub->pszMonitored) ? 1 : 0;
         RegSetValueEx (hk, REGVAL_INCLUSIVE, 0, REG_DWORD, (LPBYTE)&dwMonitored, sizeof(DWORD));

         for (LPTSTR psz = (sub->pszMonitored) ? sub->pszMonitored : sub->pszUnmonitored;
              psz && *psz;
              psz += 1+lstrlen(psz))
            {
            RegSetValueEx (hk, psz, 0, REG_SZ, (PBYTE)TEXT("X"), sizeof(TCHAR)*2);
            }

         rc = TRUE;

         RegCloseKey (hk);
         }
      }

   return rc;
}


LPSUBSET Subsets_LoadSubset (LPTSTR pszCell, LPTSTR pszSubset)
{
   LPSUBSET sub = NULL;

   HKEY hk;
   if ((hk = OpenSubsetsSubKey (pszCell, pszSubset, FALSE)) != NULL)
      {
      DWORD dwMonitored;
      DWORD dwSize;
      DWORD dwType;
      dwSize = sizeof(dwMonitored);
      if (RegQueryValueEx (hk, REGVAL_INCLUSIVE, 0, &dwType, (LPBYTE)&dwMonitored, &dwSize) == 0)
         {
         sub = New (SUBSET);
         memset (sub, 0x00, sizeof(SUBSET));
         lstrcpy (sub->szSubset, pszSubset);
         sub->fModified = FALSE;

         LPTSTR *ppsz;
         ppsz = (dwMonitored) ? &sub->pszMonitored : &sub->pszUnmonitored;

         for (size_t iIndex = 0; ; ++iIndex)
            {
            TCHAR szServer[ cchNAME ];
            dwSize = sizeof(szServer);

            if (RegEnumValue (hk, (DWORD)iIndex, szServer, &dwSize, 0, NULL, NULL, NULL) != 0)
               break;

            if (szServer[0] && lstrcmpi (szServer, REGVAL_INCLUSIVE))
               FormatMultiString (ppsz, FALSE, TEXT("%1"), TEXT("%s"), szServer);
            }

         if (dwMonitored && !sub->pszMonitored)
            {
            sub->pszMonitored = AllocateString(2);
            sub->pszMonitored[0] = TEXT('\0');
            sub->pszMonitored[1] = TEXT('\0');
            }
         }

      RegCloseKey (hk);
      }

   return sub;
}


LPSUBSET Subsets_CopySubset (LPSUBSET sub, BOOL fCreateIfNULL)
{
   LPSUBSET subCopy = NULL;

   if (sub != NULL)
      {
      subCopy = New (SUBSET);
      memset (subCopy, 0x00, sizeof(SUBSET));
      lstrcpy (subCopy->szSubset, sub->szSubset);
      subCopy->fModified = sub->fModified;

      size_t cch;
      if (sub->pszMonitored)
         {
         cch = 1;
         for (LPTSTR psz = sub->pszMonitored; *psz; psz += 1+lstrlen(psz))
            cch += 1+lstrlen(psz);
         subCopy->pszMonitored = AllocateString (cch);
         memcpy (subCopy->pszMonitored, sub->pszMonitored, sizeof(TCHAR)*cch);
         }

      if (sub->pszUnmonitored)
         {
         cch = 1;
         for (LPTSTR psz = sub->pszUnmonitored; *psz; psz += 1+lstrlen(psz))
            cch += 1+lstrlen(psz);
         subCopy->pszUnmonitored = AllocateString (cch);
         memcpy (subCopy->pszUnmonitored, sub->pszUnmonitored, sizeof(TCHAR)*cch);
         }
      }
   else if (fCreateIfNULL)
      {
      subCopy = New (SUBSET);
      memset (subCopy, 0x00, sizeof(SUBSET));
      }

   return subCopy;
}


void Subsets_FreeSubset (LPSUBSET sub)
{
   if (sub != NULL)
      {
      if (sub->pszMonitored)
         FreeString (sub->pszMonitored);
      if (sub->pszUnmonitored)
         FreeString (sub->pszUnmonitored);

      Delete (sub);
      }
}


/*
 * SUBSET OPEN/SAVE DIALOG ____________________________________________________
 *
 */

typedef struct
   {
   BOOL fOpen;
   TCHAR szCell[ cchNAME ];
   TCHAR szSubset[ cchNAME ];
   } SUBSET_OPENSAVE_PARAMS, *LPSUBSET_OPENSAVE_PARAMS;

BOOL CALLBACK Subsets_OpenSave_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Subsets_OpenSave_OnInitDialog (HWND hDlg, LPSUBSET_OPENSAVE_PARAMS lpp);
void Subsets_OpenSave_OnSelect (HWND hDlg);
void Subsets_OpenSave_Populate (HWND hDlg);
void Subsets_OpenSave_OnDelete (HWND hDlg);
void Subsets_OpenSave_OnRename (HWND hDlg);


BOOL Subsets_GetLoadName (HWND hParent, LPTSTR pszSubset)
{
   SUBSET_OPENSAVE_PARAMS lpp;
   lpp.fOpen = TRUE;
   lpp.szCell[0] = TEXT('\0');
   lstrcpy (lpp.szSubset, pszSubset);

   if (g.lpiCell)
      g.lpiCell->GetCellName (lpp.szCell);

   INT_PTR rc = ModalDialogParam (IDD_SUBSET_LOADSAVE,
                          hParent, (DLGPROC)Subsets_OpenSave_DlgProc,
                          (LPARAM)&lpp);

   if (rc == IDOK)
      {
      lstrcpy (pszSubset, lpp.szSubset);
      }

   return (rc == IDOK) ? TRUE : FALSE;
}


BOOL Subsets_GetSaveName (HWND hParent, LPTSTR pszSubset)
{
   SUBSET_OPENSAVE_PARAMS lpp;
   lpp.fOpen = FALSE;
   lpp.szCell[0] = TEXT('\0');
   lstrcpy (lpp.szSubset, pszSubset);

   if (g.lpiCell)
      g.lpiCell->GetCellName (lpp.szCell);

   INT_PTR rc = ModalDialogParam (IDD_SUBSET_LOADSAVE,
                          hParent, (DLGPROC)Subsets_OpenSave_DlgProc,
                          (LPARAM)&lpp);

   if (rc == IDOK)
      {
      lstrcpy (pszSubset, lpp.szSubset);
      }

   return (rc == IDOK) ? TRUE : FALSE;
}


BOOL CALLBACK Subsets_OpenSave_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   static BOOL fEditing = FALSE;

   if (msg == WM_INITDIALOG)
      SetWindowLongPtr (hDlg, DWLP_USER, lp);

   LPSUBSET_OPENSAVE_PARAMS lpp;
   if ((lpp = (LPSUBSET_OPENSAVE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            fEditing = FALSE;
            Subsets_OpenSave_OnInitDialog (hDlg, lpp);
            break;

         case WM_COMMAND:
            if (!fEditing)
               {
               switch (LOWORD(wp))
                  {
                  case IDCANCEL:
                     EndDialog (hDlg, IDCANCEL);
                     break;

                  case IDOK:
                     GetDlgItemText (hDlg, IDC_SUBSET_NAME, lpp->szSubset, cchNAME);
                     if (lpp->szSubset[0] != TEXT('\0'))
                        {
                        BOOL fClose = TRUE;

                        if (!lpp->fOpen)
                           {
                           HKEY hk;
                           if ((hk = OpenSubsetsSubKey (NULL, lpp->szSubset, FALSE)) != NULL)
                              {
                              RegCloseKey (hk);

                              int rc = Message (MB_YESNO | MB_ICONASTERISK, IDS_SUBSET_SAVE_TITLE, IDS_SUBSET_SAVE_DESC, TEXT("%s"), lpp->szSubset);
                              if (rc != IDYES)
                                 fClose = FALSE;
                              }
                           }

                        if (fClose)
                           EndDialog (hDlg, IDOK);
                        }
                     break;

                  case IDC_SUBSET_DELETE:
                     Subsets_OpenSave_OnDelete (hDlg);
                     break;

                  case IDC_SUBSET_RENAME:
                     Subsets_OpenSave_OnRename (hDlg);
                     break;
                  }
               }
            break;

         case WM_NOTIFY:
            switch (((LPNMHDR)lp)->code)
               {
               case LVN_ITEMCHANGED:
                  if (!fEditing)
                     {
                     if ( ((LPNM_LISTVIEW)lp)->uNewState & LVIS_SELECTED )
                        Subsets_OpenSave_OnSelect (hDlg);
                     }
                  break;

               case NM_DBLCLK:
                  if (!fEditing)
                     {
                     if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_SUBSET_LIST))
                        {
                        Subsets_OpenSave_OnSelect (hDlg);
                        PostMessage (hDlg, WM_COMMAND, IDOK, 0);
                        }
                     }
                  break;

               case LVN_BEGINLABELEDIT:
                  fEditing = TRUE;
                  return FALSE;  // okay to edit label

               case LVN_ENDLABELEDIT:
                  LV_DISPINFO *plvdi;
                  if ((plvdi = (LV_DISPINFO*)lp) != NULL)
                     {
                     if ((plvdi->item.iItem != -1) &&
                         (plvdi->item.pszText != NULL))
                        {
                        HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);
                        TCHAR szOldName[ cchNAME ];
                        LV_GetItemText (hList, plvdi->item.iItem, 0, szOldName);

                        if (lstrcmpi (szOldName, plvdi->item.pszText))
                           {
                           BOOL fRename = TRUE;
                           BOOL fRepopulate = FALSE;

                           HKEY hk;
                           if ((hk = OpenSubsetsSubKey (NULL, plvdi->item.pszText, FALSE)) != NULL)
                              {
                              RegCloseKey (hk);

                              int rc = Message (MB_YESNO | MB_ICONASTERISK, IDS_SUBSET_SAVE_TITLE, IDS_SUBSET_SAVE_DESC, TEXT("%s"), lpp->szSubset);
                              if (rc != IDYES)
                                 fRename = FALSE;
                              else
                                 fRepopulate = TRUE;
                              }

                           if (fRename)
                              {
                              LPSUBSET subRename;
                              if ((subRename = Subsets_LoadSubset (NULL, szOldName)) != NULL)
                                 {
                                 if (Subsets_SaveSubset (NULL, plvdi->item.pszText, subRename))
                                    {
                                    (void)OpenSubsetsSubKey (NULL, szOldName, 2); // 0=open,1=create,2=delete

                                    if (fRepopulate)
                                       Subsets_OpenSave_Populate (hDlg);
                                    else
                                       {
                                       LV_SetItemText (hList, plvdi->item.iItem, 0, plvdi->item.pszText);
                                       Subsets_OpenSave_OnSelect (hDlg);
                                       }

                                    Subsets_FreeSubset (subRename);
                                    }
                                 }
                              }
                           }
                        }
                     }

                  fEditing = FALSE;
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void Subsets_OpenSave_OnInitDialog (HWND hDlg, LPSUBSET_OPENSAVE_PARAMS lpp)
{
   // Fix the buttons in the toolbar, so that they looks pretty
   //
   HWND hButton = GetDlgItem (hDlg, IDC_SUBSET_DELETE);
   HICON hi = TaLocale_LoadIcon (IDI_BTN_DELETE);
   SendMessage (hButton, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hi);

   hButton = GetDlgItem (hDlg, IDC_SUBSET_RENAME);
   hi = TaLocale_LoadIcon (IDI_BTN_RENAME);
   SendMessage (hButton, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hi);

   // Set up an ImageList so we'll have icons in the ListView
   //
   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);
   HIMAGELIST hil = ImageList_Create (16, 16, ILC_COLOR4 | ILC_MASK, 1, 1);

   hi = TaLocale_LoadIcon (IDI_SUBSET);
   ImageList_AddIcon (hil, hi);

   ListView_SetImageList (hList, hil, LVSIL_SMALL);

   // Then populate the ListView with the names of the subsets
   // defined for this cell
   //
   Subsets_OpenSave_Populate (hDlg);
   // Finally, fill in the rest of the dialog.
   //
   SetDlgItemText (hDlg, IDC_SUBSET_NAME, lpp->szSubset);

   TCHAR szText[ cchRESOURCE ];
   GetString (szText, (lpp->fOpen) ? IDS_SUBSET_TITLE_LOAD : IDS_SUBSET_TITLE_SAVE);
   SetWindowText (hDlg, szText);

   GetString (szText, (lpp->fOpen) ? IDS_BUTTON_OPEN : IDS_BUTTON_SAVE);
   SetDlgItemText (hDlg, IDOK, szText);
}


void Subsets_OpenSave_Populate (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);
   LV_StartChange (hList, TRUE);

   TCHAR szSubset[ cchNAME ];
   for (size_t iIndex = 0; Subsets_EnumSubsets (NULL, iIndex, szSubset); ++iIndex)
      {
      LV_AddItem (hList, 1, 0, 0, 0, szSubset);
      }

   LV_EndChange (hList);
}


void Subsets_OpenSave_OnSelect (HWND hDlg)
{
   TCHAR szSubset[ cchNAME ];

   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);
   short idxSelected = LV_GetSelected (hList);

   if (idxSelected == -1)
      szSubset[0] = TEXT('\0');
   else
      LV_GetItemText (hList, idxSelected, 0, szSubset);

   SetDlgItemText (hDlg, IDC_SUBSET_NAME, szSubset);
}


void Subsets_OpenSave_OnDelete (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);
   short idxSelected = LV_GetSelected (hList);

   if (idxSelected != -1)
      {
      TCHAR szSubset[ cchNAME ];
      LV_GetItemText (hList, idxSelected, 0, szSubset);

      if (Message (MB_ICONASTERISK | MB_YESNO, IDS_SUBSET_DELETE_TITLE, IDS_SUBSET_DELETE_DESC, TEXT("%s"), szSubset) == IDYES)
         {
         (void)OpenSubsetsSubKey (NULL, szSubset, 2); // 0=open,1=create,2=delete
         Subsets_OpenSave_Populate (hDlg);
         }
      }
}


void Subsets_OpenSave_OnRename (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_SUBSET_LIST);
   short idxSelected = LV_GetSelected (hList);

   if (idxSelected != -1)
      {
      SetFocus (hList);
      ListView_EditLabel (hList, idxSelected);
      }
}

