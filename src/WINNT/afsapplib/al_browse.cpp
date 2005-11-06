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
#include "al_dynlink.h"
#include <WINNT/TaAfsAdmSvrClient.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cxICON              19            // size of an ACL entry icon

#define cyICON              16            // size of an ACL entry icon

#define clrTRANSPARENT      RGB(0,255,0)  // background color on ACL entry icons


/*
 * BROWSE DIALOG ______________________________________________________________
 *
 */

typedef struct BROWSEDIALOGPARAMS {
   TCHAR szCell[ cchRESOURCE ];
   TCHAR szNamed[ cchRESOURCE ];
   BOOL fThisCellOnly;
   HIMAGELIST hImages;

   BROWSETYPE bt;	// for what entry type do we browse?

   HWND hDlg;
   HANDLE hThread;
   BOOL fCanStopThreadEasily;
   BOOL fShouldStopThread;
   BOOL fThreadActive;
   TCHAR szThreadCell[ cchRESOURCE ];

   int idsTitle;
   int idsPrompt;
   int idsNone;

   LPCELLLIST lpcl;
   UINT_PTR hCreds;
} BROWSEDIALOGPARAMS, *LPBROWSEDIALOGPARAMS;


BOOL CALLBACK DlgProc_Browse (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void DlgProc_Browse_OnInitDialog (HWND hDlg, BROWSEDIALOGPARAMS *pbdp);
void DlgProc_Browse_OnNone (HWND hDlg, BROWSEDIALOGPARAMS *pbdp);
void DlgProc_Browse_SelectedEntry (HWND hDlg, BROWSEDIALOGPARAMS *pbdp);
void DlgProc_Browse_UpdateCellText (BROWSEDIALOGPARAMS *pbdp, LPTSTR pszCell);

BOOL DlgProc_Browse_StartSearch (BROWSEDIALOGPARAMS *pbdp, BOOL fCloseDlgIfFail);
void DlgProc_Browse_StopSearch (BROWSEDIALOGPARAMS *pbdp);
DWORD _stdcall DlgProc_Browse_ThreadProc (LPARAM lp);

void EnumeratePrincipalsRemotely (LPBROWSEDIALOGPARAMS pbdp, UINT_PTR idClient);
void EnumeratePrincipalsLocally (LPBROWSEDIALOGPARAMS pbdp);


/*
 *** ShowBrowseDialog
 *
 * This function presents a dialog which allows the user to select a user
 * or group from a list.
 *
 */

BOOL AfsAppLib_ShowBrowseDialog (LPBROWSEDLG_PARAMS lpp)
{
   BROWSEDIALOGPARAMS *pbdp;
   BOOL rc = FALSE;

   if ((pbdp = New (BROWSEDIALOGPARAMS)) != NULL)
   {
      memset (pbdp, 0x00, sizeof(BROWSEDIALOGPARAMS));
      lstrcpy (pbdp->szNamed, lpp->szNamed);
      lstrcpy (pbdp->szCell, lpp->szCell);
      pbdp->lpcl = lpp->lpcl;
      pbdp->bt = lpp->bt;
      pbdp->idsTitle = lpp->idsTitle;
      pbdp->idsPrompt = lpp->idsPrompt;
      pbdp->idsNone = lpp->idsNone;
      pbdp->hCreds = lpp->hCreds;

      switch (pbdp->bt)
      {
         case btLOCAL_USER:
         case btLOCAL_GROUP:
            pbdp->fThisCellOnly = TRUE;
            break;

         case btANY_USER:
         case btANY_GROUP:
            pbdp->fThisCellOnly = FALSE;
            break;
      }

      if (ModalDialogParam (IDD_APPLIB_BROWSE, lpp->hParent, (DLGPROC)DlgProc_Browse, (LPARAM)pbdp) == IDOK)
      {
         lstrcpy (lpp->szCell, pbdp->szCell);
         lstrcpy (lpp->szNamed, pbdp->szNamed);
         rc = TRUE;
      }

      Delete (pbdp);
   }

   return rc;
}


/*
 *** DlgProc_Browse
 *
 * This is the dialog proc for the Browse Cell dialog.
 *
 */

#define WM_FOUNDNAME   (WM_USER +100)
#define WM_THREADSTART (WM_USER +101)
#define WM_THREADDONE  (WM_USER +102)

BOOL CALLBACK DlgProc_Browse (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   BROWSEDIALOGPARAMS *pbdp;

   if (AfsAppLib_HandleHelp (IDD_APPLIB_BROWSE, hDlg, msg, wp, lp))
   {
      return FALSE;
   }

   if (msg == WM_INITDIALOG)
   {
      SetWindowLongPtr (hDlg, DWLP_USER, lp);
   }

   if ((pbdp = (BROWSEDIALOGPARAMS *)GetWindowLongPtr (hDlg, DWLP_USER)) != NULL)
   {
      switch (msg)
      {
         case WM_INITDIALOG:
            DlgProc_Browse_OnInitDialog (hDlg, pbdp);
            break;

         case WM_NOTIFY:
            switch (((LPNMHDR)lp)->code)
            {
               case LVN_ITEMCHANGED:
                  if ( ((LPNM_LISTVIEW)lp)->uNewState & LVIS_SELECTED )
                  {
                     DlgProc_Browse_SelectedEntry (hDlg, pbdp);
                  }
                  break;

               case NM_DBLCLK:
                  PostMessage (hDlg, WM_COMMAND, MAKELONG(IDC_BROWSE_SELECT,BN_CLICKED), (LPARAM)GetDlgItem(hDlg,IDC_BROWSE_SELECT));
                  break;
            }
            break;

         case WM_DESTROY:
            DlgProc_Browse_StopSearch (pbdp);

            if (pbdp->hImages != NULL)
            {
               ListView_SetImageList (GetDlgItem (hDlg, IDC_BROWSE_LIST), 0, 0);
               ImageList_Destroy (pbdp->hImages);
            }
            break;

         case WM_FOUNDNAME:
         {
            LPTSTR pszName = (LPTSTR)lp;
            if (pszName != NULL)
            {
               HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);
               LV_AddItem (hList, 1, INDEX_SORT, 0, 0, pszName);
               FreeString (pszName);
            }
            break;
         }

         case WM_THREADSTART:
         {
            TCHAR szText[ cchRESOURCE ];
            GetString (szText, IDS_BROWSE_WAITING);
            SetDlgItemText (pbdp->hDlg, IDC_BROWSE_STATUS, szText);
            break;
         }

         case WM_THREADDONE:
         {
            SetDlgItemText (pbdp->hDlg, IDC_BROWSE_STATUS, TEXT(""));
            break;
         }

         case WM_COMMAND:
            switch (LOWORD(wp))
            {
               case IDCANCEL:
                  EndDialog (hDlg, LOWORD(wp));
                  break;

               case IDC_BROWSE_SELECT:
                  if ( (GetDlgItem (pbdp->hDlg, IDC_BROWSE_NONE) != NULL) &&
                       (IsDlgButtonChecked (pbdp->hDlg, IDC_BROWSE_NONE)) )
                  {
                     pbdp->szCell[0] = TEXT('\0');
                     pbdp->szNamed[0] = TEXT('\0');
                  }
                  else
                  {
                     GetDlgItemText (hDlg, IDC_BROWSE_CELL,  pbdp->szCell,  cchNAME);
                     GetDlgItemText (hDlg, IDC_BROWSE_NAMED, pbdp->szNamed, cchRESOURCE);
                  }
                  EndDialog (hDlg, IDOK);
                  break;

               case IDC_BROWSE_CELL:
                  if (HIWORD(wp) == CBN_SELCHANGE)
                  {
                     GetDlgItemText (hDlg, IDC_BROWSE_CELL, pbdp->szCell, cchNAME);
                     DlgProc_Browse_StartSearch (pbdp, FALSE);
                  }
                  break;

               case IDC_BROWSE_RESTART:
                  GetDlgItemText (hDlg, IDC_BROWSE_CELL, pbdp->szCell, cchNAME);
                  DlgProc_Browse_StartSearch (pbdp, FALSE);
                  PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (hDlg, IDC_BROWSE_NAMED), (LPARAM)TRUE);
                  break;

               case IDC_BROWSE_NAMED:
                  if (HIWORD(wp) == EN_UPDATE)  // has the user hit Enter here?
                  {
                     TCHAR szTest[ cchRESOURCE ];

                     GetDlgItemText (hDlg, IDC_BROWSE_NAMED, szTest, cchRESOURCE);

                     if ( (lstrlen (szTest) > 0) &&
                          (szTest[ lstrlen(szTest)-1 ] == TEXT('\n')) )
                     {
                        szTest[ lstrlen(szTest)-1 ] = TEXT('\0');

                        if ( (lstrlen (szTest) > 0) &&
                             (szTest[ lstrlen(szTest)-1 ] == TEXT('\r')) )
                        {
                           szTest[ lstrlen(szTest)-1 ] = TEXT('\0');
                        }

                        SetDlgItemText (hDlg, IDC_BROWSE_NAMED, szTest);
                        PostMessage (hDlg, WM_COMMAND, MAKELONG(IDC_BROWSE_SELECT,BN_CLICKED), (LPARAM)GetDlgItem(hDlg,IDC_BROWSE_SELECT));
                     }
                  }
                  break;

               case IDC_BROWSE_NONE:
                  DlgProc_Browse_OnNone (hDlg, pbdp);
                  break;
            }
            break;
      }
   }

   return FALSE;
}


/*
 *** DlgProc_Browse_OnInitDialog
 *
 * The WM_INITDIALOG handler for the Browse dialog.  This routine fills
 * in the dialog with any starting parameters, and kicks off a new
 * searching thread for the given cell.
 *
 */

void DlgProc_Browse_OnInitDialog (HWND hDlg, BROWSEDIALOGPARAMS *pbdp)
{
   TCHAR szText[ cchRESOURCE ];
   LPTSTR psz;

   pbdp->hDlg = hDlg;

   // We'll need an imagelist, if we want icons in the listview.
   // This looks difficult but it's not--tis just a matter of selecting
   // an appropriate bitmap.
   //
   if ((pbdp->hImages = ImageList_Create (cxICON, cyICON, ILC_COLOR4 | ILC_MASK, 1, 1)) != 0)
   {
      HBITMAP bmp;

      if (pbdp->bt == btLOCAL_USER)
      {
         bmp = LoadBitmap (APPLIB_HINST, MAKEINTRESOURCE( IDB_LOCAL_USER ));
      }
      else if (pbdp->bt == btANY_USER)
      {
         bmp = LoadBitmap (APPLIB_HINST, MAKEINTRESOURCE( IDB_FOREIGN_USER ));
      }
      else if (pbdp->bt == btLOCAL_GROUP)
      {
         bmp = LoadBitmap (APPLIB_HINST, MAKEINTRESOURCE( IDB_LOCAL_GROUP ));
      }
      else if (pbdp->bt == btANY_GROUP)
      {
         bmp = LoadBitmap (APPLIB_HINST, MAKEINTRESOURCE( IDB_FOREIGN_GROUP ));
      }

      if (bmp != NULL)
      {
         ImageList_AddMasked (pbdp->hImages, bmp, clrTRANSPARENT);
         DeleteObject (bmp);
      }

      ListView_SetImageList (GetDlgItem (hDlg, IDC_BROWSE_LIST), pbdp->hImages, LVSIL_SMALL);
   }

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

   // Fill in the choices underneath IDC_BROWSE_CELL.  Can the
   // user enter a new cell name?
   //
   if (pbdp->fThisCellOnly)
   {
      EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_CELL), FALSE);
   }
   else
   {
      CB_StartChange (GetDlgItem (hDlg, IDC_BROWSE_CELL));

      if (!pbdp->lpcl)
      {
         TCHAR szDefCell[ cchNAME ];
         if (AfsAppLib_GetLocalCell (szDefCell) && *szDefCell)
         {
            CB_AddItem (GetDlgItem (hDlg, IDC_BROWSE_CELL), szDefCell, 1);
         }
      }
      else for (size_t ii = 0; ii < pbdp->lpcl->nCells; ++ii)
      {
         CB_AddItem (GetDlgItem (hDlg, IDC_BROWSE_CELL), pbdp->lpcl->aCells[ii], 1+ii);
      }

      CB_EndChange (GetDlgItem (hDlg, IDC_BROWSE_CELL), 1);
   }

   // Select various texts to display in the dialog
   //
   psz = FormatString (TEXT("%1"), TEXT("%m"), pbdp->idsTitle);
   SetWindowText (hDlg, psz);
   FreeString (psz);
   GetString (szText, pbdp->idsPrompt);
   SetDlgItemText (hDlg, IDC_BROWSE_TYPE, szText);

   // If the caller wants us to display a "[X] No Group" checkbox, do so
   // by creating a checkbox right underneath the IDC_BROWSE_NAMED edit
   // control--note that we'll have to hide IDC_BROWSE_STATUS if that's
   // the case.
   //
   if (pbdp->idsNone != 0)
   {
      ShowWindow (GetDlgItem (pbdp->hDlg, IDC_BROWSE_STATUS), FALSE);

      RECT rr;
      GetRectInParent (GetDlgItem (pbdp->hDlg, IDC_BROWSE_NAMED), &rr);

      LONG cy;
      cy = rr.bottom -rr.top;
      rr.top += cy +3;
      rr.bottom = rr.top +cy;

      GetString (szText, pbdp->idsNone);
      CreateWindow ("Button", szText, WS_CHILD | BS_AUTOCHECKBOX,
                    rr.left, rr.top, rr.right-rr.left, rr.bottom-rr.top,
                    pbdp->hDlg, (HMENU)IDC_BROWSE_NONE, APPLIB_HINST, 0);

      HFONT hf = (HFONT)GetStockObject (DEFAULT_GUI_FONT);
      SendMessage (GetDlgItem (pbdp->hDlg, IDC_BROWSE_NONE), WM_SETFONT, (WPARAM)hf, FALSE);
      ShowWindow (GetDlgItem (pbdp->hDlg, IDC_BROWSE_NONE), TRUE);

      if (pbdp->szNamed[0] == TEXT('\0'))
         CheckDlgButton (pbdp->hDlg, IDC_BROWSE_NONE, TRUE);
      else
         CheckDlgButton (pbdp->hDlg, IDC_BROWSE_NONE, FALSE);

      DlgProc_Browse_OnNone (pbdp->hDlg, pbdp);
   }

   SetDlgItemText (hDlg, IDC_BROWSE_CELL,  pbdp->szCell);
   SetDlgItemText (hDlg, IDC_BROWSE_NAMED, pbdp->szNamed);

   // Start looking for users/groups
   //
   DlgProc_Browse_StartSearch (pbdp, TRUE);
}


/*
 *** DlgProc_Browse_OnNone
 *
 * This routine is called whenever the user checks or unchecks the
 * "[X] None" checkbox (which we may not even be displaying)
 *
 */

void DlgProc_Browse_OnNone (HWND hDlg, BROWSEDIALOGPARAMS *pbdp)
{
   if (GetDlgItem (pbdp->hDlg, IDC_BROWSE_NONE) != NULL)
   {
      BOOL fNone = IsDlgButtonChecked (pbdp->hDlg, IDC_BROWSE_NONE);

      EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_CELL),  !fNone && !pbdp->fThisCellOnly);
      EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_NAMED), !fNone);
      EnableWindow (GetDlgItem (hDlg, IDC_BROWSE_LIST),  !fNone);

      DlgProc_Browse_SelectedEntry (hDlg, pbdp);
   }
}


/*
 *** DlgProc_Browse_SelectedEntry
 *
 * This routine is called whenever the user selects a new entry within
 * the Browse dialog's listview
 *
 */

void DlgProc_Browse_SelectedEntry (HWND hDlg, BROWSEDIALOGPARAMS *pbdp)
{
   if ( (GetDlgItem (pbdp->hDlg, IDC_BROWSE_NONE) != NULL) &&
        (IsDlgButtonChecked (pbdp->hDlg, IDC_BROWSE_NONE)) )
   {
      pbdp->szNamed[0] = TEXT('\0');
   }
   else
   {
      HWND hList = GetDlgItem (hDlg, IDC_BROWSE_LIST);
      short idxSelected = LV_GetSelected (hList);

      if (idxSelected == -1)
         pbdp->szNamed[0] = TEXT('\0');
      else
         LV_GetItemText (hList, idxSelected, 0, pbdp->szNamed);

      SetDlgItemText (hDlg, IDC_BROWSE_NAMED, pbdp->szNamed);
   }
}


/*
 *** DlgProc_Browse_UpdateCellText
 *
 * This routine places the given cell name in the browse dialog.
 *
 */

void DlgProc_Browse_UpdateCellText (BROWSEDIALOGPARAMS *pbdp, LPTSTR pszCell)
{
   SetDlgItemText (pbdp->hDlg, IDC_BROWSE_CELL, pbdp->szCell);
}


/*
 *** DlgProc_Browse_StartSearch
 *
 * This routine initiates a thread which enumerates all users/groups within
 * the given cell, posting messages to describe what it finds to the Browse
 * dialog's DLGPROC.
 *
 */

BOOL DlgProc_Browse_StartSearch (BROWSEDIALOGPARAMS *pbdp, BOOL fCloseDlgIfFail)
{
   DWORD dwThreadID;

   // Can't start a new search until the old search terminates.
   //
   DlgProc_Browse_StopSearch (pbdp);

   // Then make sure that we have a valid cell to query--if not, it may
   // be grounds to terminate the entire browse dialog.
   //
   lstrcpy (pbdp->szThreadCell, pbdp->szCell);

   if (!pbdp->szCell[0])
   {
      AfsAppLib_GetLocalCell (pbdp->szCell);

      lstrcpy (pbdp->szThreadCell, pbdp->szCell);

      DlgProc_Browse_UpdateCellText (pbdp, pbdp->szThreadCell);
   }

   if (!pbdp->szCell[0])
   {
      if (fCloseDlgIfFail)
         EndDialog (pbdp->hDlg, IDCANCEL);

      MessageBeep (MB_ICONHAND);
      Message (MB_ICONHAND, IDS_BROWSE_BADCELL_TITLE, IDS_BROWSE_BADCELL_DESC);

      if (!fCloseDlgIfFail)
         PostMessage (pbdp->hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (pbdp->hDlg, IDC_BROWSE_CELL), (LPARAM)TRUE);
      return FALSE;
   }

   // Great--we can do the search.  Start a thread to do so.
   //
   pbdp->fCanStopThreadEasily = FALSE;
   pbdp->fShouldStopThread = FALSE;
   pbdp->fThreadActive = FALSE;

   ListView_DeleteAllItems (GetDlgItem (pbdp->hDlg, IDC_BROWSE_LIST));

   pbdp->hThread = CreateThread (NULL, 0,
                                 (LPTHREAD_START_ROUTINE)DlgProc_Browse_ThreadProc,
                                 pbdp, 0, &dwThreadID);

   return (pbdp->hThread == 0) ? FALSE : TRUE;
}


/*
 *** DlgProc_Browse_StopSearch
 *
 * This routine signals the search thread to stop, and will not return
 * until it does.
 *
 */

void DlgProc_Browse_StopSearch (BROWSEDIALOGPARAMS *pbdp)
{
   if (pbdp->fThreadActive)
   {
      if (pbdp->fCanStopThreadEasily)
      {
         pbdp->fShouldStopThread = TRUE;

         WaitForSingleObject (pbdp->hThread, INFINITE);
      }
      else
      {
         TerminateThread (pbdp->hThread, 0);
         pbdp->fThreadActive = FALSE;
      }
   }

   SetDlgItemText (pbdp->hDlg, IDC_BROWSE_STATUS, TEXT(""));
}


/*
 *** DlgProc_Browse_ThreadProc
 *
 * The worker thread for the Browse dialog; this routine enumerates all
 * users or groups on the given cell, posting a WM_FOUNDNAME message to
 * the Browse dialog after every successful find.  Note that LPARAM on
 * such messages points to a string which should be freed with FreeString()
 * when no longer needed.
 *
 */

DWORD _stdcall DlgProc_Browse_ThreadProc (LPARAM lp)
{
   BROWSEDIALOGPARAMS *pbdp;

   if ((pbdp = (BROWSEDIALOGPARAMS *)lp) != NULL)
   {
      pbdp->fThreadActive = TRUE;

      PostMessage (pbdp->hDlg, WM_THREADSTART, 0, 0);

      UINT_PTR idClient;
      if ((idClient = AfsAppLib_GetAdminServerClientID()) != 0)
      {
         EnumeratePrincipalsRemotely (pbdp, idClient);
      }
      else
      {
         if (OpenClientLibrary())
         {
            if (OpenKasLibrary())
            {
               EnumeratePrincipalsLocally (pbdp);
               CloseKasLibrary();
            }

            CloseClientLibrary();
         }
      }

      pbdp->fThreadActive = FALSE;

      PostMessage (pbdp->hDlg, WM_THREADDONE, 0, 0);
   }

   return 0;
}


void EnumeratePrincipalsLocally (LPBROWSEDIALOGPARAMS pbdp)
{
   ULONG status;

   char szCellA[ MAX_PATH ];
   CopyStringToAnsi (szCellA, pbdp->szCell);

   PVOID hCell;
   if (afsclient_CellOpen (szCellA, (PVOID)pbdp->hCreds, &hCell, (afs_status_p)&status))
   {
      // Enumerate the principals recognized by KAS.
      //
      PVOID hEnum;
      if (kas_PrincipalGetBegin (hCell, NULL, &hEnum, (afs_status_p)&status))
      {
         pbdp->fCanStopThreadEasily = TRUE;

         while (!pbdp->fShouldStopThread)
         {
            kas_identity_t who;
            if (!kas_PrincipalGetNext (hEnum, &who, (afs_status_p)&status))
               break;

            LPTSTR pszName;
            if ((pszName = CloneAnsi ((LPSTR)who.principal)) == NULL)
               break;

            PostMessage (pbdp->hDlg, WM_FOUNDNAME, 0, (LPARAM)pszName);
            // pszName freed by DlgProc_Browse when it receives the message
         }

         kas_PrincipalGetDone (hEnum, (afs_status_p)&status);
      }

      afsclient_CellClose (hCell, (afs_status_p)&status);
   }
}


void EnumeratePrincipalsRemotely (LPBROWSEDIALOGPARAMS pbdp, UINT_PTR idClient)
{
   ULONG status;

   // Open the relevant cell
   //
   ASID idCell;
   if (asc_CellOpen (idClient, (PVOID)pbdp->hCreds, pbdp->szThreadCell, AFSADMSVR_SCOPE_USERS, &idCell, &status))
   {
      // Obtain a list of ASIDs from the admin server, each representing
      // a principal which we want to show.
      //
      LPASIDLIST pAsidList;
      if (asc_ObjectFindMultiple (idClient, idCell, TYPE_USER, NULL, NULL, &pAsidList, &status))
      {
         if (pAsidList)
         {
            // Obtain rudimentary properties (e.g., their names) for these ASIDs
            //
            LPASOBJPROPLIST pPropList;
            if (asc_ObjectPropertiesGetMultiple (idClient, GET_RUDIMENTARY_DATA, idCell, pAsidList, &pPropList, &status))
            {
               if (pPropList)
               {
                  // Use the information in {pPropList} to populate the display
                  //
                  for (size_t iEntry = 0; iEntry < pPropList->cEntries; ++iEntry)
                  {
                     LPTSTR pszName;
                     if ((pszName = CloneString (pPropList->aEntries[ iEntry ].ObjectProperties.szName)) != NULL)
                     {
                        PostMessage (pbdp->hDlg, WM_FOUNDNAME, 0, (LPARAM)pszName);
                        // pszName freed by DlgProc_Browse when it receives the message
                     }
                  }

                  asc_ObjPropListFree (&pPropList);
               }
            }

            asc_AsidListFree (&pAsidList);
         }
      }

      asc_CellClose (idClient, idCell, &status);
   }
}

