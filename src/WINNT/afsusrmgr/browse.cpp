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
#include "browse.h"
#include "usr_col.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ID_SEARCH_TIMER     0

#define msecSEARCH_TIMER  650

static struct
   {
   DWORD dwTickLastType;
   } l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Browse_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Browse_OnInitDialog (HWND hDlg);
void Browse_OnOK (HWND hDlg);
void Browse_OnEndTask_EnumObjects (HWND hDlg, LPTASKPACKET ptp);
void Browse_OnEndTask_Translate (HWND hDlg, LPTASKPACKET ptp);
void Browse_OnSelect (HWND hDlg);
void Browse_UpdateDialog (HWND hDlg);
void Browse_Enable (HWND hDlg, BOOL fEnable);
ASOBJTYPE Browse_GetSelectedType (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL ShowBrowseDialog (LPBROWSE_PARAMS lpp)
{
   int nTypesToShow = 0;
   if (lpp->TypeToShow & TYPE_USER)
      ++nTypesToShow;
   if (lpp->TypeToShow & TYPE_GROUP)
      ++nTypesToShow;
   lpp->fQuerying = FALSE;

   int idd = (nTypesToShow == 1) ? IDD_BROWSE : (lpp->idsCheck) ? IDD_BROWSE_BOTH : IDD_BROWSE_COMBO;

   if (ModalDialogParam (idd, lpp->hParent, (DLGPROC)Browse_DlgProc, (LPARAM)lpp) != IDOK)
      return FALSE;

   if (!lpp->pObjectsSelected)
      return FALSE;

   return TRUE;
}


BOOL CALLBACK Browse_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPBROWSE_PARAMS lpp;
   if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      if (AfsAppLib_HandleHelp (lpp->iddForHelp, hDlg, msg, wp, lp))
         return FALSE;
      }

   switch (msg)
      {
      case WM_INITDIALOG:
         SetWindowLongPtr (hDlg, DWLP_USER, lp);
         Browse_OnInitDialog (hDlg);
         l.dwTickLastType = 0;
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if ((ptp->idTask == taskUSER_ENUM) || (ptp->idTask == taskGROUP_ENUM))
               Browse_OnEndTask_EnumObjects (hDlg, ptp);
            else if (ptp->idTask == taskLIST_TRANSLATE)
               Browse_OnEndTask_Translate (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_TIMER:
         switch (wp)
            {
            case ID_SEARCH_TIMER:
               if ( (l.dwTickLastType) && (GetTickCount() > l.dwTickLastType + msecSEARCH_TIMER) )
                  {
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  Browse_UpdateDialog (hDlg);
                  }
               break;
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_BROWSE_SELECT:
               Browse_OnOK (hDlg);
               break;

            case IDCANCEL:
               EndDialog (hDlg, IDCANCEL);
               break;

            case IDC_BROWSE_CHECK:
               Browse_UpdateDialog (hDlg);
               break;

            case IDC_BROWSE_COMBO:
               if (HIWORD(wp) == CBN_SELCHANGE)
                  {
                  SetDlgItemText (hDlg, IDC_BROWSE_NAMED, TEXT(""));
                  Browse_UpdateDialog (hDlg);
                  }
               break;

            case IDC_BROWSE_RESTART:
               Browse_UpdateDialog (hDlg);
               PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (hDlg, IDC_BROWSE_NAMED), (LPARAM)TRUE);
               break;

            case IDC_BROWSE_NAMED:
               if (HIWORD(wp) == EN_UPDATE)  // has the user hit Enter here?
                  {
                  TCHAR szTest[ 1024 ];
                  GetDlgItemText (hDlg, IDC_BROWSE_NAMED, szTest, 1024);

                  if ((lstrlen (szTest)) && (szTest[lstrlen(szTest)-1]==TEXT('\n')))
                     {
                     szTest[ lstrlen(szTest)-1 ] = TEXT('\0');
                     if ((lstrlen (szTest)) && (szTest[lstrlen(szTest)-1]==TEXT('\r')))
                        szTest[ lstrlen(szTest)-1 ] = TEXT('\0');

                     SetDlgItemText (hDlg, IDC_BROWSE_NAMED, szTest);
                     PostMessage (hDlg, WM_COMMAND, MAKELONG(IDC_BROWSE_SELECT,BN_CLICKED), (LPARAM)GetDlgItem(hDlg,IDC_BROWSE_SELECT));
                     }
                  }
               break;

            case IDC_BROWSE_PATTERN:
               if (HIWORD(wp) == EN_UPDATE)
                  {
                  l.dwTickLastType = GetTickCount();
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  SetTimer (hDlg, ID_SEARCH_TIMER, msecSEARCH_TIMER +15, NULL);
                  }
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               Browse_OnSelect (hDlg);
               break;

            case FLN_LDBLCLICK:
               PostMessage (hDlg, WM_COMMAND, MAKELONG(IDC_BROWSE_SELECT,BN_CLICKED), (LPARAM)GetDlgItem (hDlg, IDC_BROWSE_SELECT));
               break;
            }
         break;
      }

   return FALSE;
}


void Browse_OnInitDialog (HWND hDlg)
{
   LPBROWSE_PARAMS lpp;
   if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      // First, the text of the dialog is woefully ugly right now. Put
      // in the strings which the caller supplied.
      //
      TCHAR szText[ cchRESOURCE ];
      GetString (szText, lpp->idsTitle);
      SetWindowText (hDlg, szText);

      GetString (szText, lpp->idsPrompt);
      SetDlgItemText (hDlg, IDC_BROWSE_TYPE, szText);

      SetDlgItemText (hDlg, IDC_BROWSE_NAMED, lpp->szName);

      SetDlgItemText (hDlg, IDC_BROWSE_PATTERN, TEXT(""));

      ULONG status;
      asc_CellNameGet_Fast (g.idClient, g.idCell, szText, &status);
      SetDlgItemText (hDlg, IDC_BROWSE_CELL, szText);
      EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_CELL), FALSE);

      if (GetDlgItem (hDlg, IDC_BROWSE_CHECK))
         {
         if (lpp->idsCheck == 0)
            ShowWindow (GetDlgItem (hDlg, IDC_BROWSE_CHECK), FALSE);
         else
            {
            GetString (szText, lpp->idsCheck);
            SetDlgItemText (hDlg, IDC_BROWSE_CHECK, szText);
            }

         CheckDlgButton (hDlg, IDC_BROWSE_CHECK, (lpp->pObjectsToSkip) ? TRUE : FALSE);
         }

      if (GetDlgItem (hDlg, IDC_BROWSE_COMBO))
         {
         HWND hCombo = GetDlgItem (hDlg, IDC_BROWSE_COMBO);
         LPARAM lpSelect = 0;
         CB_StartChange (hCombo, TRUE);

         if (lpp->TypeToShow & TYPE_USER)
            {
            CB_AddItem (hCombo, IDS_SHOW_USERS, TYPE_USER);
            if (!lpSelect)
               lpSelect = TYPE_USER;
            }

         if (lpp->TypeToShow & TYPE_GROUP)
            {
            CB_AddItem (hCombo, IDS_SHOW_GROUPS, TYPE_GROUP);
            if (!lpSelect)
               lpSelect = TYPE_GROUP;
            }

         CB_EndChange (hCombo, lpSelect);
         }

      // If the caller requested that we disallow multiple selection,
      // change the fastlist's styles...
      //
      if (!lpp->fAllowMultiple)
         {
         DWORD dwStyle = GetWindowLong (GetDlgItem (hDlg, IDC_BROWSE_LIST), GWL_STYLE);
         dwStyle &= ~(FLS_SELECTION_MULTIPLE);
         SetWindowLong (GetDlgItem (hDlg, IDC_BROWSE_LIST), GWL_STYLE, dwStyle);
         }

      // Finally, update the listbox to show a valid list of users/groups.
      // Oh--since we're in initdialog, add an imagelist to the window too.
      //
      FastList_SetImageLists (GetDlgItem (hDlg, IDC_BROWSE_LIST), AfsAppLib_CreateImageList(FALSE), NULL);
      Browse_UpdateDialog (hDlg);
      }
}


void Browse_OnSelect (HWND hDlg)
{
   LPBROWSE_PARAMS lpp;
   if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      if (!lpp->fQuerying)
         {
         HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);

         static TCHAR szSeparator[ cchRESOURCE ] = TEXT("");
         if (szSeparator[0] == TEXT('\0'))
            {
            if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SLIST, szSeparator, cchRESOURCE))
               lstrcpy (szSeparator, TEXT(","));
            lstrcat (szSeparator, TEXT(" "));
            }

         LPTSTR pszText = NULL;

         HLISTITEM hItem = NULL;
         while ((hItem = FastList_FindNextSelected (hList, hItem)) != NULL)
            {
            LPCTSTR pszName;
            if ((pszName = FastList_GetItemText (hList, hItem, 0)) != NULL)
               {
               LPTSTR pszNew;
               if (pszText)
                  pszNew = FormatString (TEXT("%1%2%3"), TEXT("%s%s%s"), pszText, szSeparator, pszName);
               else
                  pszNew = FormatString (TEXT("%1"), TEXT("%s"), pszName);
               if (pszText)
                  FreeString (pszText);
               pszText = pszNew;
               }
            if (pszText && !lpp->fAllowMultiple)
               break;
            }

         SetDlgItemText (hDlg, IDC_BROWSE_NAMED, (pszText) ? pszText : TEXT(""));
         if (pszText)
            FreeString (pszText);
         }
      }
}


void Browse_OnOK (HWND hDlg)
{
   LPBROWSE_PARAMS lpp;
   if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      LPTSTR pszNames = GetEditText (GetDlgItem (hDlg, IDC_BROWSE_NAMED));

      // Disable the controls on the dialog for a bit...
      //
      Browse_Enable (hDlg, FALSE);

      // Start a background task to translate the typed list-of-names into
      // a usable ASID list. When it comes back, we'll close the dialog.
      //
      LPLIST_TRANSLATE_PARAMS pTask = New (LIST_TRANSLATE_PARAMS);
      pTask->Type = Browse_GetSelectedType (hDlg);
      pTask->pszNames = pszNames;
      StartTask (taskLIST_TRANSLATE, hDlg, pTask);
      }
}


void Browse_OnEndTask_EnumObjects (HWND hDlg, LPTASKPACKET ptp)
{
   LPBROWSE_PARAMS lpp;
   size_t ii;
   if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);
      FastList_Begin (hList);
      FastList_RemoveAll (hList);

      // If we were successful, we now have a list of all objects of the
      // appropriate type. However, the user may not want us to display
      // items which are on our pObjectsToSkip list; if not, convert the
      // later to a hashlist (for fast lookup) and remove the offending
      // entries from the former.
      //
      if (ptp->rc && TASKDATA(ptp)->pAsidList)
         {
         if ((IsDlgButtonChecked (hDlg, IDC_BROWSE_CHECK)) && (lpp->pObjectsToSkip))
            {
            LPHASHLIST pListToSkip = New (HASHLIST);

            for (ii = 0; ii < lpp->pObjectsToSkip->cEntries; ++ii)
               pListToSkip->AddUnique ((PVOID)(lpp->pObjectsToSkip->aEntries[ii].idObject));

            for (ii = 0; ii < TASKDATA(ptp)->pAsidList->cEntries; )
               {
               if (pListToSkip->fIsInList ((PVOID)(TASKDATA(ptp)->pAsidList->aEntries[ii].idObject)))
                  asc_AsidListRemoveEntryByIndex (&TASKDATA(ptp)->pAsidList, ii);
               else
                  ii++;
               }

            Delete (pListToSkip);
            }
         }

      // OK, we're ready to go--populate that list!
      //
      for (ii = 0; ii < TASKDATA(ptp)->pAsidList->cEntries; ++ii)
         {
         ULONG status;
         ASOBJPROP Properties;
         if (!asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, TASKDATA(ptp)->pAsidList->aEntries[ii].idObject, &Properties, &status))
            continue;

         TCHAR szName[ MAX_PATH ];
         if (Properties.Type == TYPE_USER)
            User_GetDisplayName (szName, &Properties);
         else
            lstrcpy (szName, Properties.szName);

         FASTLISTADDITEM flai;
         memset (&flai, 0x00, sizeof(flai));
         flai.iFirstImage = (Properties.Type == TYPE_USER) ? imageUSER : (Properties.Type == TYPE_GROUP) ? imageGROUP : IMAGE_NOIMAGE;
         flai.iSecondImage = IMAGE_NOIMAGE;
         flai.pszText = szName;
         FastList_AddItem (hList, &flai);
         }

      FastList_End (hList);
      lpp->fQuerying --;
      }
}


void Browse_OnEndTask_Translate (HWND hDlg, LPTASKPACKET ptp)
{
   LPBROWSE_PARAMS lpp;
    if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      if (!ptp->rc || !TASKDATA(ptp)->pAsidList || !TASKDATA(ptp)->pAsidList->cEntries)
         {
         ErrorDialog (ptp->status, (TASKDATA(ptp)->Type == TYPE_USER) ? IDS_ERROR_CANT_TRANSLATE_USER : IDS_ERROR_CANT_TRANSLATE_GROUP);
         Browse_Enable (hDlg, TRUE);
         }
      else
         {
         lpp->pObjectsSelected = TASKDATA(ptp)->pAsidList;
         TASKDATA(ptp)->pAsidList = NULL; // don't let FreeTaskPacket free this

         // Fill in {lpp->szName}, for convenience
         //
         lpp->szName[0] = TEXT('\0');

         if (lpp->pObjectsSelected && lpp->pObjectsSelected->cEntries)
            {
            ULONG status;
            ASOBJPROP Properties;
            if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, lpp->pObjectsSelected->aEntries[0].idObject, &Properties, &status))
               lstrcpy (lpp->szName, Properties.szName);
            }

         EndDialog (hDlg, IDOK);
         }
      }
}


void Browse_UpdateDialog (HWND hDlg)
{
   LPBROWSE_PARAMS lpp;
   if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      lpp->fQuerying ++;

      // First we'll need to empty the list, and add some non-selectable thing
      // that says "querying"
      //
      HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);
      FastList_Begin (hList);
      FastList_RemoveAll (hList);

      TCHAR szText[ cchRESOURCE ];
      GetString (szText, IDS_QUERYING_LONG);

      FASTLISTADDITEM flai;
      memset (&flai, 0x00, sizeof(flai));
      flai.iFirstImage = IMAGE_NOIMAGE;
      flai.iSecondImage = IMAGE_NOIMAGE;
      flai.pszText = szText;
      flai.dwFlags = FLIF_DISALLOW_SELECT;
      FastList_AddItem (hList, &flai);

      FastList_End (hList);

      // Then start a background task to obtain the appropriate list
      // of stuff to show. When that task completes, we'll populate the
      // list.
      //
      TCHAR szPattern[ cchNAME ];
      GetDlgItemText (hDlg, IDC_BROWSE_PATTERN, szPattern, cchNAME);

      LPTSTR pszPattern = NULL;
      if (szPattern[0] != TEXT('\0'))
         pszPattern = CloneString (szPattern);

      StartTask ((Browse_GetSelectedType (hDlg) == TYPE_USER) ? taskUSER_ENUM : taskGROUP_ENUM, hDlg, pszPattern);
      }
}


void Browse_Enable (HWND hDlg, BOOL fEnable)
{
   EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_LIST), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_SELECT), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_NAMED), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_CHECK), fEnable);
   EnableWindow (GetDlgItem (hDlg, IDCANCEL), fEnable);
}


ASOBJTYPE Browse_GetSelectedType (HWND hDlg)
{
   HWND hCombo;
   if ((hCombo = GetDlgItem (hDlg, IDC_BROWSE_COMBO)) != NULL)
      {
      return (ASOBJTYPE)CB_GetSelectedData (hCombo);
      }

   LPBROWSE_PARAMS lpp;
   if ((lpp = (LPBROWSE_PARAMS)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
      {
      return lpp->TypeToShow;
      }

   return (ASOBJTYPE)0;
}

