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

#include <WINNT/afsapplib.h>

#define WM_FOUNDNAME   (WM_USER +100)
#define WM_THREADSTART (WM_USER +101)
#define WM_THREADDONE  (WM_USER +102)


/*
 * VARIABLES __________________________________________________________________
 *
 */


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK BrowseSet_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void BrowseSet_OnInitDialog (HWND hDlg, LPBROWSESETDLG_PARAMS pszFilename);
void BrowseSet_OnDestroy (HWND hDlg);
void BrowseSet_OnAddString (HWND hDlg, LPTSTR pszString);
void BrowseSet_OnSelectedEntry (HWND hDlg);

void BrowseSet_StartSearch (HWND hDlg, LPBROWSESETDLG_PARAMS lpp);
void BrowseSet_OnSearchStart (HWND hDlg);
void BrowseSet_OnSearchDone (HWND hDlg);
void BrowseSet_EmptyList (HWND hDlg);
DWORD WINAPI BrowseSet_Init_ThreadProc (LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL AfsAppLib_ShowBrowseFilesetDialog (LPBROWSESETDLG_PARAMS lpp)
{
   return (ModalDialogParam (IDD_APPLIB_BROWSE_FILESET, lpp->hParent, (DLGPROC)BrowseSet_DlgProc, (LPARAM)lpp) == IDOK) ? TRUE : FALSE;
}


BOOL CALLBACK BrowseSet_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_APPLIB_BROWSE_FILESET, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, lp);

   LPBROWSESETDLG_PARAMS lpp;
   if ((lpp = (LPBROWSESETDLG_PARAMS)GetWindowLong (hDlg, DWL_USER)) != NULL)
      {
      switch (msg)
         {
         case WM_INITDIALOG:
            lpp->pInternal = (PVOID)hDlg;
            BrowseSet_OnInitDialog (hDlg, lpp);
            break;

         case WM_DESTROY:
            BrowseSet_OnDestroy (hDlg);
            lpp->pInternal = NULL;
            break;

         case WM_THREADSTART:
            BrowseSet_OnSearchStart (hDlg);
            break;

         case WM_THREADDONE:
            BrowseSet_OnSearchDone (hDlg);
            break;

         case WM_FOUNDNAME:
            BrowseSet_OnAddString (hDlg, (LPTSTR)wp);
            break;

         case WM_COMMAND:
            switch (LOWORD(wp))
               {
               case IDOK:
                  GetDlgItemText (hDlg, IDC_BROWSE_NAMED, lpp->szFileset, cchNAME);
               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_BROWSE_CELL:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                     {
                     GetDlgItemText (hDlg, IDC_BROWSE_CELL, lpp->szCell, cchNAME);
                     BrowseSet_StartSearch (hDlg, lpp);
                     }
                  break;

               case IDC_BROWSE_NAMED:
                  if (HIWORD(wp) == EN_UPDATE)  // has the user hit Enter here?
                     {
                     TCHAR szTest[ cchRESOURCE ];
                     GetDlgItemText (hDlg, IDC_BROWSE_NAMED, szTest, cchRESOURCE);

                     if ( (lstrlen (szTest) > 0) && (szTest[ lstrlen(szTest)-1 ] == TEXT('\n')) )
                        {
                        szTest[ lstrlen(szTest)-1 ] = TEXT('\0');

                        if ( (lstrlen (szTest) > 0) && (szTest[ lstrlen(szTest)-1 ] == TEXT('\r')) )
                           szTest[ lstrlen(szTest)-1 ] = TEXT('\0');

                        SetDlgItemText (hDlg, IDC_BROWSE_NAMED, szTest);
                        PostMessage (hDlg, WM_COMMAND, MAKELONG(IDOK,BN_CLICKED), (LPARAM)GetDlgItem(hDlg,IDOK));
                        }
                     }
                  break;

               case IDC_BROWSE_RESTART:
                  GetDlgItemText (hDlg, IDC_BROWSE_CELL, lpp->szCell, cchNAME);
                  BrowseSet_StartSearch (hDlg, lpp);
                  PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (hDlg, IDC_BROWSE_NAMED), (LPARAM)TRUE);
                  break;
               }
            break;

         case WM_NOTIFY:
            switch (((LPNMHDR)lp)->code)
               {
               case LVN_ITEMCHANGED:
                  if ( ((LPNM_LISTVIEW)lp)->uNewState & LVIS_SELECTED )
                     BrowseSet_OnSelectedEntry (hDlg);
                  break;

               case NM_DBLCLK:
                  PostMessage (hDlg, WM_COMMAND, MAKELONG(IDOK,BN_CLICKED), (LPARAM)GetDlgItem(hDlg,IDOK));
                  break;
               }
            break;
         }
      }

   return FALSE;
}


void BrowseSet_OnInitDialog (HWND hDlg, LPBROWSESETDLG_PARAMS lpp)
{
   if (lpp->idsTitle)
      {
      TCHAR szText[ cchRESOURCE ];
      GetString (szText, lpp->idsTitle);
      SetWindowText (hDlg, szText);
      }

   if (lpp->idsPrompt)
      {
      TCHAR szText[ cchRESOURCE ];
      GetString (szText, lpp->idsPrompt);
      SetDlgItemText (hDlg, IDC_BROWSE_TYPE, szText);
      }

   ListView_SetImageList (GetDlgItem (hDlg, IDC_BROWSE_LIST), AfsAppLib_CreateImageList(FALSE), LVSIL_SMALL);
   SetDlgItemText (hDlg, IDC_BROWSE_NAMED, lpp->szFileset);

   if (!lpp->lpcl)
      {
      EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_CELL), FALSE);
      }
   else
      {
      CB_StartChange (GetDlgItem (hDlg, IDC_BROWSE_CELL));
      for (size_t ii = 0; ii < lpp->lpcl->nCells; ++ii)
         CB_AddItem (GetDlgItem (hDlg, IDC_BROWSE_CELL), lpp->lpcl->aCells[ii], 1+ii);
      CB_EndChange (GetDlgItem (hDlg, IDC_BROWSE_CELL), 1);
      }
   if (lpp->szCell[0] != TEXT('\0'))
      {
      SetDlgItemText (hDlg, IDC_BROWSE_CELL, lpp->szCell);
      }

   BrowseSet_StartSearch (hDlg, lpp);

   // There's a default pushbutton on this dialog, so that hitting
   // RETURN when you're in the Cell combobox will restart the search
   // (a default pushbutton always gets called when RETURN is hit,
   // unless your control traps it).  But the user doesn't want to *see*
   // that thing, so move it way off the dialog's client area.
   //
   RECT r;
   GetWindowRect (GetDlgItem (hDlg, IDC_BROWSE_RESTART), &r);
   SetWindowPos (GetDlgItem (hDlg, IDC_BROWSE_RESTART), NULL,
                 0 - (r.right-r.left), 0 - (r.bottom-r.top), 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}


void BrowseSet_OnDestroy (HWND hDlg)
{
   BrowseSet_EmptyList (hDlg);
}


void BrowseSet_OnSelectedEntry (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);
   LPTSTR pszString = (LPTSTR)LV_GetSelectedData (hList);
   if (pszString != NULL)
      {
      SetDlgItemText (hDlg, IDC_BROWSE_NAMED, pszString);
      }
}


void BrowseSet_StartSearch (HWND hDlg, LPBROWSESETDLG_PARAMS lpp)
{
   BrowseSet_EmptyList (hDlg);

   LPBROWSESETDLG_PARAMS lppNew = New (BROWSESETDLG_PARAMS);
   memcpy (lppNew, lpp, sizeof(BROWSESETDLG_PARAMS));

   DWORD dwThreadID;
   HANDLE hThread;
   if ((hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE)BrowseSet_Init_ThreadProc, (PVOID)lppNew, 0, &dwThreadID)) == INVALID_HANDLE_VALUE)
      {
      EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_LIST), FALSE);
      Delete (lppNew);
      }
}


void BrowseSet_OnSearchStart (HWND hDlg)
{
   TCHAR szWait[ cchRESOURCE ];
   GetString (szWait, IDS_BROWSE_WAITING);
   SetDlgItemText (hDlg, IDC_BROWSE_STATUS, szWait);
}


void BrowseSet_OnSearchDone (HWND hDlg)
{
   SetDlgItemText (hDlg, IDC_BROWSE_STATUS, TEXT(""));
   EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_LIST), TRUE);
}


void BrowseSet_OnAddString (HWND hDlg, LPTSTR pszString)
{
   HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);
   LV_AddItem (hList, 1, INDEX_SORT, (LPARAM)pszString, imageFILESET, pszString);
   // string freed when list destroyed
}


void BrowseSet_EmptyList (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);

   int iiMax = ListView_GetItemCount (hList);
   for (int ii = 0; ii < iiMax; ++ii)
      {
      LV_ITEM lvi;
      memset (&lvi, 0x00, sizeof(LV_ITEM));
      lvi.mask = LVIF_PARAM;
      lvi.iItem = ii;
      if (ListView_GetItem (hList, &lvi))
         {
         LPTSTR psz;
         if ((psz = (LPTSTR)lvi.lParam) != NULL)
            {
            lvi.mask = LVIF_PARAM;
            lvi.iItem = ii;
            lvi.lParam = 0;
            ListView_SetItem (hList, &lvi);
            FreeString (psz);
            }
         }
      }
}


DWORD WINAPI BrowseSet_Init_ThreadProc (LPARAM lp)
{
   LPBROWSESETDLG_PARAMS lpp = (LPBROWSESETDLG_PARAMS)lp;

   char szCellA[ cchNAME ];
   CopyStringToAnsi (szCellA, lpp->szCell);

   if (IsWindow ((HWND)(lpp->pInternal)))
      PostMessage ((HWND)(lpp->pInternal), WM_THREADSTART, 0, 0);

   // Fill in hList with the names of all filesets in the cell.
   //
   ULONG status = 0;
#if 0
   // TODO
   if (OpenFTS (&status))
      {
      PVOID hCell;
      if ((status = FtsOpenCell (szCellA, &hCell)) == 0)
         {
         fldb_short_ft_info_t *fti;
         if ((status = FtsAllocateShortFtInfoBuffer (&fti)) == 0)
            {
            PVOID cookie = 0;
            ULONG nEntries;

            while ( (FtsListFilesetsFromFldb (hCell, fti, &nEntries, &cookie) == 0) && (nEntries > 0) )
               {
               if (!IsWindow ((HWND)(lpp->pInternal)))
                  break;

               for (ULONG ii = 0; ii < nEntries; ++ii)
                  {
#define DECODE_SHINFO_TYPE_FLAGS(_fl) ((_fl) >> 12)
                  if (DECODE_SHINFO_TYPE_FLAGS(fti[ ii ].flags) & FTS_FT_SHINFO_TYPE_RW)
                     {
                     LPTSTR pszFileset;
                     if ((pszFileset = CloneString (fti[ii].name)) != NULL)
                        {
                        PostMessage ((HWND)(lpp->pInternal), WM_FOUNDNAME, (WPARAM)pszFileset, 0);
                        // string memory is freed by recipient of message
                        }
                     }
                  }
               }
            FtsFreeShortFtInfoBuffer (fti);
            }
         FtsCloseCell (hCell);
         }
      CloseFTS();
      }
#endif

   if (IsWindow ((HWND)(lpp->pInternal)))
      PostMessage ((HWND)(lpp->pInternal), WM_THREADDONE, status, 0);

   Delete (lpp);
   return 0;
}

