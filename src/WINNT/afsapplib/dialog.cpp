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

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <WINNT/subclass.h>
#include <WINNT/talocale.h>
#include <WINNT/dialog.h>
#include <WINNT/fastlist.h>

#ifdef FORWARD_WM_COMMAND
#undef FORWARD_WM_COMMAND
#endif
#define FORWARD_WM_COMMAND(hwnd, id, hwndCtl, codeNotify, fn) \
    (fn)((hwnd), WM_COMMAND, MAKEWPARAM((UINT)(id),(UINT)(codeNotify)), (LPARAM)(HWND)(hwndCtl))


/*
 * MISCELLANEOUS ______________________________________________________________
 *
 */

#ifndef REALLOC
#define REALLOC(_a,_c,_r,_i) DialogReallocFunction ((LPVOID*)&_a,sizeof(*_a),&_c,_r,_i)
BOOL DialogReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)Allocate (cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget != 0)
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      Free (*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}
#endif


void DialogGetRectInParent (HWND hWnd, RECT *pr)
{
   POINT pt;

   GetWindowRect (hWnd, pr);

   pr->right -= pr->left;
   pr->bottom -= pr->top;	// right/bottom == width/height for now

   pt.x = pr->left;
   pt.y = pr->top;

   ScreenToClient (GetParent (hWnd), &pt);

   pr->left = pt.x;
   pr->top = pt.y;
   pr->right += pr->left;
   pr->bottom += pr->top;
}


/*
 * PROPERTY SHEETS ____________________________________________________________
 *
 */

BOOL PropSheet_HandleNotify (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   switch (msg)
      {
      case WM_NOTIFY:
         switch (((NMHDR FAR *)lp)->code)
            {
            case PSN_KILLACTIVE:
               FORWARD_WM_COMMAND (hDlg, IDOK, 0, 0, SendMessage);
               return TRUE;
            case PSN_APPLY:
               FORWARD_WM_COMMAND (hDlg, IDAPPLY, 0, 0, SendMessage);    
               return TRUE;
            case PSN_HELP:
               FORWARD_WM_COMMAND (hDlg, IDHELP, 0, 0, SendMessage);
               return TRUE;
            case PSN_SETACTIVE:
               FORWARD_WM_COMMAND (hDlg, IDINIT, wp, lp, SendMessage);
               return TRUE;
            case PSN_RESET:
               FORWARD_WM_COMMAND (hDlg, IDCANCEL, 0, 0, SendMessage);
               return TRUE;
            }
         break;
      }

   return FALSE;
}


static struct
   {
   BOOL fInUse;
   HWND hSheet;
   LPPROPSHEET psh;
   size_t cRef;
   } *aPropSheets = NULL;

static size_t cPropSheets = 0;


BOOL CALLBACK PropTab_HookProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (PropSheet_HandleNotify (hDlg, msg, wp, lp))
      return TRUE;

   HWND hSheet = GetParent (hDlg);
   HWND hTab = GetDlgItem (hSheet, IDC_PROPSHEET_TABCTRL);

   if (IsWindow (hTab))
      {
      size_t ii;
      for (ii = 0; ii < cPropSheets; ++ii)
         {
         if (aPropSheets[ii].fInUse && (aPropSheets[ii].hSheet == hSheet))
            break;
         }
      if (ii >= cPropSheets)
         {
         for (size_t ii = 0; ii < cPropSheets; ++ii)
            {
            if (aPropSheets[ii].fInUse && !aPropSheets[ii].hSheet)
               {
               aPropSheets[ii].hSheet = hSheet;
               break;
               }
            }
         }
      if (ii < cPropSheets)
         {
         size_t iTab;
         for (iTab = 0; iTab < aPropSheets[ii].psh->cTabs; ++iTab)
            {
            if (aPropSheets[ii].psh->aTabs[iTab].hDlg == hDlg)
               break;
            }
         if (iTab == aPropSheets[ii].psh->cTabs)
            {
            iTab = (size_t)TabCtrl_GetCurSel (hTab);
            }
         if (iTab < aPropSheets[ii].psh->cTabs)
            {
            aPropSheets[ii].psh->aTabs[iTab].hDlg = hDlg;
            BOOL rc = CallWindowProc ((WNDPROC)(aPropSheets[ ii ].psh->aTabs[ iTab ].dlgproc), hDlg, msg, wp, lp);

            switch (msg)
               {
               case WM_INITDIALOG:
                  ++ aPropSheets[ ii ].cRef;
                  break;

               case WM_DESTROY:
                  if (!(-- aPropSheets[ ii ].cRef))
                     {
                     PropSheet_Free (aPropSheets[ ii ].psh);
                     aPropSheets[ ii ].fInUse = FALSE;
                     }
                  break;
               }

            return rc;
            }
         }
      }

   return DefWindowProc (hDlg, msg, wp, lp);

}


LPPROPSHEET PropSheet_Create (LONG idsTitle, BOOL fContextHelp, HWND hParent, LPARAM lp)
{
   LPPROPSHEET psh = New (PROPSHEET);
   memset (psh, 0x00, sizeof(PROPSHEET));
   psh->sh.dwSize = sizeof(PROPSHEETHEADER);
   psh->sh.dwFlags = PSH_MODELESS | ((fContextHelp) ? PSH_HASHELP : 0);
   psh->sh.hwndParent = hParent;
   psh->sh.hInstance = THIS_HINST;
   psh->sh.pszCaption = (HIWORD(idsTitle)) ? (LPTSTR)idsTitle : FormatString(TEXT("%1"),TEXT("%m"),idsTitle);
   psh->fMadeCaption = (HIWORD(idsTitle)) ? FALSE : TRUE;
   psh->lpUser = lp;
   return psh;
}

LPPROPSHEET PropSheet_Create (int idsTitle, BOOL fContextHelp, HWND hParent, LPARAM lp)
{
   return PropSheet_Create ((LONG)idsTitle, fContextHelp, hParent, lp);
}

LPPROPSHEET PropSheet_Create (LPTSTR pszTitle, BOOL fContextHelp, HWND hParent, LPARAM lp)
{
   return PropSheet_Create ((LONG)pszTitle, fContextHelp, hParent, lp);
}


BOOL PropSheet_AddTab (LPPROPSHEET psh, LONG idsTitle, int idd, DLGPROC dlgproc, LPARAM lpUser, BOOL fHelpButton, BOOL fStartPage)
{
   TCHAR szTitle[ cchRESOURCE ];

   PROPSHEETPAGE psp;
   memset (&psp, 0x00, sizeof(psp));
   psp.dwSize = sizeof(PROPSHEETPAGE);
   psp.dwFlags = PSP_DEFAULT | PSP_DLGINDIRECT | (fHelpButton ? PSP_HASHELP : 0);
   psp.pResource = TaLocale_GetDialogResource (idd, &psp.hInstance);
   psp.pfnDlgProc = (DLGPROC)PropTab_HookProc;
   psp.lParam = lpUser;

   // When first creating the PROPSHEET structure, we had to guess about
   // which module the tabs would be coming from. Now that we have at least
   // one tab, we can correct that guess.
   //
   psh->sh.hInstance = psp.hInstance;

   if (HIWORD(idsTitle))
      {
      psp.pszTitle = (LPTSTR)idsTitle;
      psp.dwFlags |= PSP_USETITLE;
      }
   else if (idsTitle != 0)
      {
      GetString (szTitle, idsTitle);
      psp.pszTitle = szTitle;
      psp.dwFlags |= PSP_USETITLE;
      }

   HPROPSHEETPAGE hp;
   if ((hp = CreatePropertySheetPage (&psp)) == 0)
      return FALSE;

   if (!REALLOC( psh->sh.phpage, psh->sh.nPages, 1+psh->sh.nPages, 1))
      return FALSE;

   if (!REALLOC( psh->aTabs, psh->cTabs, psh->sh.nPages, 1))
      return FALSE;

   psh->aTabs[ psh->sh.nPages-1 ].dlgproc = dlgproc;
   psh->aTabs[ psh->sh.nPages-1 ].lpUser = lpUser;
   psh->aTabs[ psh->sh.nPages-1 ].hDlg = 0;

   psh->sh.phpage[ psh->sh.nPages-1 ] = hp;
   if (fStartPage)
      psh->sh.nStartPage = psh->sh.nPages-1;

   return TRUE;
}

BOOL PropSheet_AddTab (LPPROPSHEET psh, int idsTitle, int idd, DLGPROC dlgproc, LPARAM lpUser, BOOL fHelpButton, BOOL fStartPage)
{
   return PropSheet_AddTab (psh, (LONG)idsTitle, idd, dlgproc, lpUser, fHelpButton, fStartPage);
}

BOOL PropSheet_AddTab (LPPROPSHEET psh, LPTSTR pszTitle, int idd, DLGPROC dlgproc, LPARAM lpUser, BOOL fHelpButton, BOOL fStartPage)
{
   return PropSheet_AddTab (psh, (LONG)pszTitle, idd, dlgproc, lpUser, fHelpButton, fStartPage);
}


void PropSheet_NotifyAllTabs (LPPROPSHEET psh, HWND hDlg, UINT msg)
{
   for (size_t ii = 0; ii < psh->cTabs; ++ii)
      {
      if (psh->aTabs[ ii ].dlgproc)
         {
         CallWindowProc ((WNDPROC)(psh->aTabs[ ii ].dlgproc), hDlg, msg, (WPARAM)psh, psh->aTabs[ ii ].lpUser);
         }
      }
}


BOOL CALLBACK PropSheet_HookProc (HWND hSheet, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldproc = Subclass_FindNextHook (hSheet, PropSheet_HookProc);

   BOOL rc;
   if (oldproc)
      rc = CallWindowProc ((WNDPROC)oldproc, hSheet, msg, wp, lp);
   else
      rc = DefWindowProc (hSheet, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         size_t ii;
         for (ii = 0; ii < cPropSheets; ++ii)
            {
            if (aPropSheets[ii].fInUse && (aPropSheets[ii].hSheet == hSheet))
               break;
            }
         if (ii < cPropSheets)
            {
            for (size_t iTab = 0; iTab < aPropSheets[ii].psh->cTabs; ++iTab)
               {
               if ( (aPropSheets[ii].psh->aTabs[iTab].hDlg) &&
                    (IsWindow (aPropSheets[ii].psh->aTabs[iTab].hDlg)) &&
                    (aPropSheets[ii].psh->aTabs[iTab].dlgproc != NULL) )
                  {
                  DestroyWindow (aPropSheets[ii].psh->aTabs[iTab].hDlg);
                  }
               }

            PropSheet_NotifyAllTabs (aPropSheets[ ii ].psh, hSheet, WM_DESTROY_SHEET);

            if (!(-- aPropSheets[ ii ].cRef))
               {
               PropSheet_Free (aPropSheets[ ii ].psh);
               aPropSheets[ ii ].fInUse = FALSE;
               }
            }

         Subclass_RemoveHook (hSheet, PropSheet_HookProc);
         break;
      }

   return rc;
}


LPARAM PropSheet_FindTabParam (HWND hTab)
{
   size_t ii;
   for (ii = 0; ii < cPropSheets; ++ii)
      {
      if (aPropSheets[ii].fInUse && (aPropSheets[ii].hSheet == hTab))
         return aPropSheets[ii].psh->lpUser;
      }

   HWND hSheet = GetParent (hTab);
   for (ii = 0; ii < cPropSheets; ++ii)
      {
      if (aPropSheets[ii].fInUse && (aPropSheets[ii].hSheet == hSheet))
         break;
      }
   if (ii < cPropSheets)
      {
      for (size_t iTab = 0; iTab < aPropSheets[ ii ].psh->cTabs; ++iTab)
         {
         if (hTab == aPropSheets[ ii ].psh->aTabs[ iTab ].hDlg)
            return aPropSheets[ ii ].psh->aTabs[ iTab ].lpUser;
         }
      }

   return 0;
}


BOOL PropSheet_ShowModal (LPPROPSHEET psh, void (*fnPump)(MSG *lpm))
{
   HWND hSheet;
   if ((hSheet = PropSheet_ShowModeless (psh, SW_SHOW)) == NULL)
      return FALSE;

   BOOL fWasEnabled = TRUE;
   HWND hParent = psh->sh.hwndParent;
   if (hParent && IsWindow (hParent))
      {
      fWasEnabled = IsWindowEnabled (hParent);
      EnableWindow (hParent, FALSE);
      }

   MSG msg;
   while (GetMessage (&msg, NULL, 0, 0))
      {
      if (IsMemoryManagerMessage (&msg))
         continue;

      if (!PropSheet_GetCurrentPageHwnd (hSheet))
         {
         if (hParent && IsWindow (hParent))
            EnableWindow (hParent, fWasEnabled);
         DestroyWindow (hSheet);
         }

      if (!IsWindow (hSheet))
         {
         if (fnPump)
            (*fnPump)(&msg);
         else
            {
            TranslateMessage (&msg);
            DispatchMessage (&msg);
            }
         break;
         }

      if (PropSheet_IsDialogMessage (hSheet, &msg))
         continue;

      if (fnPump)
         (*fnPump)(&msg);
      else
         {
         TranslateMessage (&msg);
         DispatchMessage (&msg);
         }
      }

   if (hParent && IsWindow (hParent))
      EnableWindow (hParent, fWasEnabled);

   return TRUE;
}


HWND PropSheet_ShowModeless (LPPROPSHEET psh, int nCmdShow)
{
   // Write down that we're opening a property sheet. Since we
   // don't know the window handle yet, we'll set it to zero--
   // when PropSheetTab_HookProc gets called for a property sheet
   // that doesn't seem to be listed, it will write down hSheet for us.
   // This way, we get the aPropSheets[] array filled in--complete with
   // window handle--*before* the tabs themselves get any messages.
   // Otherwise, we'd have to wait until the WM_INITDIALOG/IDINIT
   // messages were finished.
   //
   size_t ii;
   for (ii = 0; ii < cPropSheets; ++ii)
      {
      if (!aPropSheets[ ii ].fInUse)
         break;
      }
   if (!REALLOC (aPropSheets, cPropSheets, 1+ii, 4))
      return NULL;
   aPropSheets[ ii ].fInUse = TRUE;
   aPropSheets[ ii ].hSheet = 0;
   aPropSheets[ ii ].psh = psh;
   aPropSheets[ ii ].cRef = 1;

   // Open the property sheet dialog.
   //
   psh->sh.dwFlags |= PSH_MODELESS;

   HWND hSheet;
   if ((hSheet = (HWND)PropertySheet (&psh->sh)) == NULL)
      return NULL;

   // Tell all tabs (not just the first one) that the sheet has been
   // created. We'll also want to hook the sheet itself here, so we can
   // clean up when it's destroyed.
   //
   Subclass_AddHook (hSheet, PropSheet_HookProc);
   PropSheet_NotifyAllTabs (psh, hSheet, WM_INITDIALOG_SHEET);
   ShowWindow (hSheet, nCmdShow);

   return hSheet;
}


void PropSheet_Free (LPPROPSHEET psh)
{
   if (psh)
      {
      if (psh->sh.phpage)
         {
         Free (psh->sh.phpage);
         }
      if (psh->fMadeCaption && psh->sh.pszCaption)
         {
         FreeString (psh->sh.pszCaption);
         }
      if (psh->aTabs)
         {
         Free (psh->aTabs);
         }

      Delete (psh);
      }
}


HWND PropSheet_FindTabWindow (LPPROPSHEET psh, DLGPROC dlgproc)
{
   if (psh)
      {
      for (size_t iTab = 0; iTab < psh->cTabs; ++iTab)
         {
         if (dlgproc == psh->aTabs[ iTab ].dlgproc)
            return psh->aTabs[ iTab ].hDlg;
         }
      }
   return NULL;
}


void PropSheetChanged (HWND hDlg)
{
   PropSheet_Changed (GetParent(hDlg), hDlg);
}


void TabCtrl_SwitchToTab (HWND hTab, int iTab)
{
   TabCtrl_SetCurSel (hTab, iTab);

   NMHDR nm;
   nm.hwndFrom = hTab;
   nm.idFrom = IDC_PROPSHEET_TABCTRL;
   nm.code = TCN_SELCHANGE;
   SendMessage (GetParent (hTab), WM_NOTIFY, 0, (LPARAM)&nm);
}


/*
 * LISTVIEW ___________________________________________________________________
 *
 */

LPARAM FL_StartChange (HWND hList, BOOL fReset)
{
   LPARAM dwSelected = FL_GetSelectedData (hList);
   FastList_Begin (hList);
   if (fReset)
      FastList_RemoveAll (hList);
   return dwSelected;
}


void FL_EndChange (HWND hList, LPARAM lpToSelect)
{
   if (lpToSelect != 0)
      FL_SetSelectedByData (hList, lpToSelect);
   FastList_EndAll (hList);
}


void FL_ResizeColumns (HWND hList, WORD nCols, WORD *acx)
{
   for (WORD ii = 0; ii < nCols; ++ii)
      {
      FASTLISTCOLUMN flc;
      FastList_GetColumn (hList, ii, &flc);

      flc.cxWidth = acx[ ii ];
      FastList_SetColumn (hList, ii, &flc);
      }
}


void cdecl FL_SetColumns (HWND hList, WORD nCols, WORD *acx, ...)
{
   va_list   arg;
   va_start (arg, acx);

   for (WORD ii = 0; ii < nCols; ii++)
      {
      FASTLISTCOLUMN flc;
      GetString (flc.szText, va_arg (arg, WORD));
      flc.dwFlags = FLCF_JUSTIFY_LEFT;
      flc.cxWidth = acx[ ii ];            // width of the column, in pixels
      FastList_SetColumn (hList, ii, &flc);
      }
}


void FL_GetColumns (HWND hList, WORD nCols, WORD *acx)
{
   for (WORD ii = 0; ii < nCols; ii++)
      {
      FASTLISTCOLUMN flc;
      FastList_GetColumn (hList, ii, &flc);
      acx[ ii ] = (WORD)flc.cxWidth;
      }
}


HLISTITEM cdecl FL_AddItem (HWND hList, WORD nCols, LPARAM lp, int iImage1, ...)
{
   va_list   arg;
   va_start (arg, iImage1);

   FASTLISTADDITEM flai;
   memset (&flai, 0x00, sizeof(flai));
   flai.hParent = NULL;
   flai.iFirstImage = iImage1;
   flai.iSecondImage = IMAGE_NOIMAGE;
   flai.pszText = va_arg (arg, LPTSTR);
   flai.lParam = lp;
   flai.dwFlags = 0;

   FastList_Begin (hList);

   HLISTITEM hItem;
   if ((hItem = FastList_AddItem (hList, &flai)) != NULL)
      {
      for (WORD ii = 1; ii < nCols; ii++)
         {
         FastList_SetItemText (hList, hItem, ii, va_arg (arg, LPTSTR));
         }
      }

   FastList_End (hList);
   return hItem;
}


void FL_GetItemText (HWND hList, HLISTITEM hItem, int iCol, LPTSTR psz)
{
   LPCTSTR pszText = FastList_GetItemText (hList, hItem, iCol);
   if (!pszText)
      *psz = TEXT('\0');
   else
      lstrcpy (psz, pszText);
}


void FL_SetItemText (HWND hList, HLISTITEM hItem, int iCol, LPCTSTR psz)
{
   FastList_SetItemText (hList, hItem, iCol, psz);
}


void FL_SetSelectedByData (HWND hList, LPARAM lp)
{
   HLISTITEM hItem;
   if ((hItem = FastList_FindItem (hList, lp)) != NULL)
      {
      FL_SetSelected (hList, hItem);
      }
}


void FL_SetSelected (HWND hList, HLISTITEM hItem)
{
   FastList_Begin (hList);
   FastList_SelectNone (hList);
   if (hItem)
      {
      FastList_SelectItem (hList, hItem, TRUE);
      FastList_EnsureVisible (hList, hItem);
      }
   FastList_SetFocus (hList, hItem);
   FastList_End (hList);
}


HLISTITEM FL_GetSelected (HWND hList, HLISTITEM hItem)
{
   return FastList_FindNextSelected (hList, hItem);
}


HLISTITEM FL_GetFocused (HWND hList)
{
   return FastList_GetFocus (hList);
}


LPARAM FL_GetFocusedData (HWND hList)
{
   HLISTITEM hItem;
   if ((hItem = FastList_GetFocus (hList)) == NULL)
      return 0;

   return FastList_GetItemParam (hList, hItem);
}


LPARAM FL_GetData (HWND hList, HLISTITEM hItem)
{
   return FastList_GetItemParam (hList, hItem);
}


HLISTITEM FL_GetIndex (HWND hList, LPARAM lp)
{
   return FastList_FindItem (hList, lp);
}


LPARAM FL_GetSelectedData (HWND hList)
{
   HLISTITEM hItem;
   if ((hItem = FastList_FindFirstSelected (hList)) == NULL)
      return 0;

   return FastList_GetItemParam (hList, hItem);
}


void FL_RestoreView (HWND hList, LPVIEWINFO lpvi)
{
   FastList_Begin (hList);

   for (size_t cColumns = FastList_GetColumnCount(hList); cColumns; --cColumns)
      {
      FastList_RemoveColumn (hList, cColumns-1);
      }

   for (size_t ii = 0; ii < lpvi->nColsShown; ++ii)
      {
      if (lpvi->aColumns[ ii ] >= lpvi->nColsAvail)
         continue;

      size_t iColumn = lpvi->aColumns[ ii ];

      FASTLISTCOLUMN flc;
      GetString (flc.szText, lpvi->idsColumns[ iColumn ]);

      flc.dwFlags = (lpvi->cxColumns[ iColumn ] & COLUMN_RIGHTJUST) ? FLCF_JUSTIFY_RIGHT : (lpvi->cxColumns[ iColumn ] & COLUMN_CENTERJUST) ? FLCF_JUSTIFY_CENTER : FLCF_JUSTIFY_LEFT;
      flc.cxWidth = (lpvi->cxColumns[ iColumn ] & (~COLUMN_JUSTMASK));
      FastList_SetColumn (hList, ii, &flc);
      }

   // Restore the fastlist's sort style
   //
   FastList_SetSortStyle (hList, (lpvi->iSort & (~COLUMN_SORTREV)), !!(lpvi->iSort & COLUMN_SORTREV));

   // Switch the FastList into the proper mode--Large Icons,
   // Small Icons, List or Details.
   //
   LONG dw = GetWindowLong (hList, GWL_STYLE);
   dw &= ~FLS_VIEW_MASK;
   dw |= lpvi->lvsView;
   SetWindowLong (hList, GWL_STYLE, dw);

   FastList_End (hList);
}


void FL_StoreView (HWND hList, LPVIEWINFO lpvi)
{
   // First, remember what mode the the FastList is in--Large,
   // Small, List, Tree, or TreeList.
   //
   lpvi->lvsView  = GetWindowLong (hList, GWL_STYLE);
   lpvi->lvsView &= FLS_VIEW_MASK;

   // Record the fastlist's sort style
   //
   int iCol;
   BOOL fRev;
   FastList_GetSortStyle (hList, &iCol, &fRev);
   lpvi->iSort = (size_t)iCol;
   if (fRev)
      lpvi->iSort |= COLUMN_SORTREV;

   // Then remember the columns' widths; we expect that the columns
   // which are shown (ie, nColsShown and aColumns[]) are up-to-date
   // already.
   //
   if (lpvi->lvsView & FLS_VIEW_LIST)
      {
      for (size_t ii = 0; ii < lpvi->nColsShown; ++ii)
         {
         if (lpvi->aColumns[ ii ] >= lpvi->nColsAvail)
            continue;

         size_t iColumn = lpvi->aColumns[ii];

         int dwJust = (lpvi->cxColumns[ iColumn ] & COLUMN_JUSTMASK);

         FASTLISTCOLUMN flc;
         FastList_GetColumn (hList, ii, &flc);
         lpvi->cxColumns[ iColumn ] = flc.cxWidth | dwJust;
         }
      }
}


HLISTITEM cdecl FL_AddItem (HWND hList, LPVIEWINFO lpvi, LPARAM lp, int iImage1, ...)
{
   va_list   arg;
   va_start (arg, iImage1);

   LPTSTR apszColumns[ nCOLUMNS_MAX ];
   size_t iColumn;
   for (iColumn = 0; iColumn < lpvi->nColsAvail; ++iColumn)
      {
      apszColumns[ iColumn ] = va_arg (arg, LPTSTR);
      }

   FastList_Begin (hList);

   HLISTITEM hItem = NULL;
   for (size_t ii = 0; ii < lpvi->nColsShown; ++ii)
      {
      if ((iColumn = lpvi->aColumns[ ii ]) >= lpvi->nColsAvail)
         continue;

      if (ii == 0)
         {
         FASTLISTADDITEM flai;
         memset (&flai, 0x00, sizeof(flai));
         flai.hParent = NULL;
         flai.iFirstImage = iImage1;
         flai.iSecondImage = IMAGE_NOIMAGE;
         flai.pszText = apszColumns[ iColumn ];
         flai.lParam = lp;
         flai.dwFlags = 0;

         hItem = FastList_AddItem (hList, &flai);
         }
      else
         {
         FastList_SetItemText (hList, hItem, ii, apszColumns[ iColumn ]);
         }
      }

   FastList_End (hList);
   return hItem;
}


BOOL FL_HitTestForHeaderBar (HWND hList, POINT ptClient)
{
   HWND hHeader;
   for (hHeader = GetWindow (hList, GW_CHILD);
        hHeader != NULL;
        hHeader = GetWindow (hHeader, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];

      if (GetClassName (hHeader, szClassName, cchRESOURCE))
         {
         if (!lstrcmp (szClassName, WC_HEADER))
            break;
         }
      }

   if (hHeader && IsWindow(hHeader) && IsWindowVisible(hHeader))
      {
      RECT rHeader;
      DialogGetRectInParent (hHeader, &rHeader);
      if (PtInRect (&rHeader, ptClient))
         return TRUE;
      }

   return FALSE;
}



/*
 * LISTVIEW ___________________________________________________________________
 *
 */

LPARAM LV_StartChange (HWND hList, BOOL fReset)
{
   LPARAM dwSelected = LV_GetSelectedData (hList);
   SendMessage (hList, WM_SETREDRAW, FALSE, 0);
   if (fReset)
      ListView_DeleteAllItems (hList);
   return dwSelected;
}


void LV_EndChange (HWND hList, LPARAM lpToSelect)
{
   SendMessage (hList, WM_SETREDRAW, TRUE, 0);

   if (lpToSelect != (LPARAM)NULL)
      LV_SetSelectedByData (hList, lpToSelect);
}


void LV_ResizeColumns (HWND hList, WORD nCols, WORD *acx)
{
   for (WORD ii = 0; ii < nCols; ++ii)
      {
      ListView_SetColumnWidth (hList, ii, acx[ ii ]);
      }
}

void cdecl LV_SetColumns (HWND hList, WORD nCols, WORD *acx, ...)
{
   va_list   arg;
   va_start (arg, acx);

   for (WORD ii = 0; ii < nCols; ii++)
      {
      LV_COLUMN lvC;
      TCHAR     text[ cchRESOURCE ];

      GetString (text, va_arg (arg, WORD));

      lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
      lvC.fmt = LVCFMT_LEFT;
      lvC.pszText = text;
      lvC.cx = acx[ ii ];            // width of the column, in pixels
      lvC.iSubItem = ii;
      ListView_InsertColumn (hList, ii, &lvC);
      }
}


void LV_GetColumns (HWND hList, WORD nCols, WORD *acx)
{
   for (WORD ii = 0; ii < nCols; ii++)
      {
      acx[ ii ] = ListView_GetColumnWidth (hList, ii);
      }
}


void cdecl LV_AddItem (HWND hList, WORD nCols, int index, LPARAM lp, int iImage, ...)
{
   LV_ITEM   lvI;
   va_list   arg;
   va_start (arg, iImage);

   lvI.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE | LVIF_IMAGE;
   lvI.state = 0;
   lvI.stateMask = 0;
   lvI.iItem = index;
   lvI.iSubItem = 0;
   lvI.pszText = va_arg (arg, LPTSTR);
   lvI.cchTextMax = cchRESOURCE;
   lvI.iImage = iImage;
   lvI.lParam = lp;

   if ((index == INDEX_SORT) && (lvI.pszText != LPSTR_TEXTCALLBACK))
      {
      lvI.iItem = LV_PickInsertionPoint (hList, lvI.pszText);
      }

   DWORD dw = ListView_InsertItem (hList, &lvI);

   for (WORD ii = 1; ii < nCols; ii++)
      {
      ListView_SetItemText (hList, dw, ii, va_arg (arg, LPTSTR));
      }
}


void cdecl LV_GetItemText (HWND hList, int index, short col, LPTSTR psz)
{
   *psz = TEXT('\0');
   ListView_GetItemText (hList, index, col, (LPTSTR)psz, cchRESOURCE);
}


void cdecl LV_SetItemText (HWND hList, int index, short col, LPCTSTR psz)
{
   ListView_SetItemText (hList, index, col, (LPTSTR)psz);
}


void LV_SetSelectedByData (HWND hList, LPARAM lp)
{
   int index;

   if ((index = LV_GetIndex (hList, lp)) != -1)
      {
      LV_SetSelected (hList, index);
      }
}


void LV_SetSelected (HWND hList, int index)
{
   ListView_EnsureVisible (hList, index, FALSE);

   ListView_SetItemState (hList, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}


int LV_GetSelected (HWND hList, int index)
{
   return ListView_GetNextItem (hList, index, LVNI_SELECTED);
}


int LV_GetFocused (HWND hList)
{
   return ListView_GetNextItem (hList, -1, LVNI_FOCUSED);
}


LPARAM LV_GetFocusedData (HWND hList)
{
   int idxFocus;

   if ((idxFocus = LV_GetFocused (hList)) == -1)
      return (LPARAM)0;

   return LV_GetData (hList, idxFocus);
}


LPARAM LV_GetData (HWND hList, int index)
{
   LV_ITEM lvI;

   memset (&lvI, 0x00, sizeof(LV_ITEM));
   lvI.mask = LVIF_PARAM;
   lvI.iItem = index;

   if (! ListView_GetItem (hList, &lvI))
      return (LPARAM)0;

   return lvI.lParam;
}


int LV_GetIndex (HWND hList, LPARAM lp)
{
   LV_FINDINFO  lvfi;

   memset (&lvfi, 0x00, sizeof(lvfi));

   lvfi.flags = LVFI_PARAM;
   lvfi.lParam = lp;

   return ListView_FindItem (hList, -1, &lvfi);
}


LPARAM LV_GetSelectedData (HWND hList)
{
   int index;

   if ((index = LV_GetSelected (hList)) == -1)
      return (LPARAM)0;

   return LV_GetData (hList, index);
}


void LV_RestoreView (HWND hList, LPVIEWINFO lpvi)
{
   // Add columns to the listview; remember that we'll have to remove any
   // existing columns first.
   //
   while (ListView_DeleteColumn (hList, 0))
      ;

   for (size_t ii = 0; ii < lpvi->nColsShown; ++ii)
      {
      if (lpvi->aColumns[ ii ] >= lpvi->nColsAvail)
         continue;

      size_t iColumn = lpvi->aColumns[ii];

      LV_COLUMN lvc;

      TCHAR szText[ cchRESOURCE ];
      GetString (szText, lpvi->idsColumns[ iColumn ]);

      lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
      lvc.fmt = (lpvi->cxColumns[ iColumn ] & COLUMN_RIGHTJUST) ? LVCFMT_RIGHT : (lpvi->cxColumns[ iColumn ] & COLUMN_CENTERJUST) ? LVCFMT_CENTER : LVCFMT_LEFT;
      lvc.pszText = szText;
      lvc.cx = (lpvi->cxColumns[ iColumn ] & (~COLUMN_JUSTMASK));
      lvc.iSubItem = ii;
      ListView_InsertColumn (hList, ii, &lvc);
      }

   // Switch the ListView into the proper mode--Large Icons,
   // Small Icons, List or Details.
   //
   LONG dw = GetWindowLong (hList, GWL_STYLE);
   dw &= ~LVS_TYPEMASK;
   dw |= lpvi->lvsView;
   dw |= LVS_AUTOARRANGE;
   SetWindowLong (hList, GWL_STYLE, dw);
   ListView_Arrange (hList, LVA_DEFAULT);
}


void LV_StoreView (HWND hList, LPVIEWINFO lpvi)
{
   // First, remember what mod the the ListView is in--Large Icons,
   // Small Icons, List or Details.
   //
   lpvi->lvsView  = GetWindowLong (hList, GWL_STYLE);
   lpvi->lvsView &= LVS_TYPEMASK;

   // Then remember the columns' widths; we expect that the columns
   // which are shown (ie, nColsShown and aColumns[]) are up-to-date
   // already.
   //
   if (lpvi->lvsView == LVS_REPORT)
      {
      for (size_t ii = 0; ii < lpvi->nColsShown; ++ii)
         {
         if (lpvi->aColumns[ ii ] >= lpvi->nColsAvail)
            continue;

         size_t iColumn = lpvi->aColumns[ii];

         int dwJust = (lpvi->cxColumns[ iColumn ] & COLUMN_JUSTMASK);

         lpvi->cxColumns[ iColumn ] = ListView_GetColumnWidth (hList, ii);
         lpvi->cxColumns[ iColumn ] |= dwJust;
         }
      }
}


typedef struct // VIEWSORTINFO
   {
   HWND hList;
   LPVIEWINFO lpvi;
   size_t iColSort;
   BOOL fAscending;
   } VIEWSORTINFO, *LPVIEWSORTINFO;

BOOL CALLBACK LV_SortView_Numeric (LPARAM lp1, LPARAM lp2, LPARAM lpSort)
{
   LPVIEWSORTINFO lpvsi = (LPVIEWSORTINFO)lpSort;
   TCHAR szText[ cchRESOURCE ];
   double d1;
   double d2;

   int ii1 = LV_GetIndex (lpvsi->hList, lp1);
   int ii2 = LV_GetIndex (lpvsi->hList, lp2);

   LV_GetItemText (lpvsi->hList, ii1, lpvsi->iColSort, szText);
   d1 = atof (szText);

   LV_GetItemText (lpvsi->hList, ii2, lpvsi->iColSort, szText);
   d2 = atof (szText);

   if (lpvsi->fAscending)
      return (d2 <  d1) ? ((BOOL)-1) : (d2 == d1) ? ((BOOL)0) : (BOOL)1;
   else
      return (d1 <  d2) ? ((BOOL)-1) : (d1 == d2) ? ((BOOL)0) : (BOOL)1;
}


BOOL CALLBACK LV_SortView_Alphabetic (LPARAM lp1, LPARAM lp2, LPARAM lpSort)
{
   LPVIEWSORTINFO lpvsi = (LPVIEWSORTINFO)lpSort;
   TCHAR szText1[ cchRESOURCE ];
   TCHAR szText2[ cchRESOURCE ];

   int ii1 = LV_GetIndex (lpvsi->hList, lp1);
   int ii2 = LV_GetIndex (lpvsi->hList, lp2);

   LV_GetItemText (lpvsi->hList, ii1, lpvsi->iColSort, szText1);
   LV_GetItemText (lpvsi->hList, ii2, lpvsi->iColSort, szText2);

   if (lpvsi->fAscending)
      return lstrcmp (szText2, szText1);
   else
      return lstrcmp (szText1, szText2);
}


void LV_SortView (HWND hList, LPVIEWINFO lpvi)
{
   size_t iColSort;
   for (iColSort = 0; iColSort < lpvi->nColsShown; ++iColSort)
      {
      if ((lpvi->iSort & (~COLUMN_SORTREV)) == lpvi->aColumns[ iColSort ])
         break;
      }

   if (iColSort < lpvi->nColsShown)
      {
      VIEWSORTINFO vsi;
      vsi.hList = hList;
      vsi.lpvi = lpvi;
      vsi.iColSort = iColSort;
      vsi.fAscending = (lpvi->iSort & COLUMN_SORTREV) ? TRUE : FALSE;

      if (lpvi->cxColumns[ lpvi->iSort ] & COLUMN_RIGHTJUST)
         ListView_SortItems (hList, (PFNLVCOMPARE)LV_SortView_Numeric, (LPARAM)&vsi);
      else
         ListView_SortItems (hList, (PFNLVCOMPARE)LV_SortView_Alphabetic, (LPARAM)&vsi);
      }
}


void cdecl LV_AddItem (HWND hList, LPVIEWINFO lpvi, int index, LPARAM lp, int iImage, ...)
{
   va_list   arg;
   va_start (arg, iImage);

   LPTSTR apszColumns[ nCOLUMNS_MAX ];
   size_t iColumn;
   for (iColumn = 0; iColumn < lpvi->nColsAvail; ++iColumn)
      {
      apszColumns[ iColumn ] = va_arg (arg, LPTSTR);
      }

   if ((index == INDEX_SORT) && (iColumn) && (apszColumns[0] != LPSTR_TEXTCALLBACK))
      {
      index = LV_PickInsertionPoint (hList, apszColumns[0]);
      }

   DWORD dw;
   for (size_t ii = 0; ii < lpvi->nColsShown; ++ii)
      {
      if ((iColumn = lpvi->aColumns[ ii ]) >= lpvi->nColsAvail)
         continue;

      if (ii == 0)
         {
         LV_ITEM lvitem;
         lvitem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE | LVIF_IMAGE;
         lvitem.state = 0;
         lvitem.stateMask = 0;
         lvitem.iItem = index;
         lvitem.iSubItem = 0;
         lvitem.pszText = apszColumns[ iColumn ];
         lvitem.cchTextMax = cchRESOURCE;
         lvitem.iImage = iImage;
         lvitem.lParam = lp;

         dw = ListView_InsertItem (hList, &lvitem);
         }
      else
         {
         ListView_SetItemText (hList, dw, ii, apszColumns[ iColumn ]);
         }
      }
}


BOOL LV_HitTestForHeaderBar (HWND hList, POINT ptClient)
{
   HWND hHeader;
   for (hHeader = GetWindow (hList, GW_CHILD);
        hHeader != NULL;
        hHeader = GetWindow (hHeader, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];

      if (GetClassName (hHeader, szClassName, cchRESOURCE))
         {
         if (!lstrcmp (szClassName, WC_HEADER))
            break;
         }
      }

   if (hHeader && IsWindow(hHeader) && IsWindowVisible(hHeader))
      {
      RECT rHeader;
      DialogGetRectInParent (hHeader, &rHeader);
      if (PtInRect (&rHeader, ptClient))
         return TRUE;
      }

   return FALSE;
}


int LV_PickInsertionPoint (HWND hList, LPTSTR pszNewItem, int (__stdcall *fnSort)(LPCTSTR, LPCTSTR))
{
   int iItemLow  = 0;
   int iItemHigh = ListView_GetItemCount (hList) -1;

   if (fnSort == 0)
      fnSort = lstrcmpi;

   while (iItemLow <= iItemHigh)
      {
      int iItemTest = iItemLow + (iItemHigh - iItemLow) / 2;

      TCHAR szItemTest[ 256 ];
      ListView_GetItemText (hList, iItemTest, 0, szItemTest, 256);

      int iCompare = (*fnSort)(pszNewItem, szItemTest);
      if (iCompare <= 0)
         {
         if ((iItemHigh = iItemTest-1) < iItemLow)
            return iItemHigh +1;
         }
      if (iCompare >= 0)
         {
         if ((iItemLow = iItemTest+1) > iItemHigh)
            return iItemLow;
         }
      }

   return 0;
}



/*
 * COMBO BOXES ________________________________________________________________
 *
 */

LPARAM CB_StartChange (HWND hCombo, BOOL fReset)
{
   LPARAM dwSelected = CB_GetSelectedData (hCombo);
   SendMessage (hCombo, WM_SETREDRAW, FALSE, 0);
   if (fReset)
      SendMessage (hCombo, CB_RESETCONTENT, 0, 0);
   return dwSelected;
}


void CB_EndChange (HWND hCombo, LPARAM lpToSelect)
{
   SendMessage (hCombo, WM_SETREDRAW, TRUE, 0);

   if (lpToSelect != (LPARAM)NULL)
      CB_SetSelectedByData (hCombo, lpToSelect);
}


UINT CB_AddItem (HWND hCombo, int nsz, LPARAM lp)
{
   LPTSTR  psz;
   UINT    rc;

   psz = FormatString (nsz);
   rc = CB_AddItem (hCombo, psz, lp);
   FreeString (psz);

   return rc;
}


UINT CB_AddItem (HWND hCombo, LPCTSTR psz, LPARAM lp)
{
   UINT index;
   if ((index = (UINT)SendMessage (hCombo, CB_ADDSTRING, 0, (LPARAM)psz)) != (UINT)-1)
      {
      SendMessage (hCombo, CB_SETITEMDATA, index, lp);
      }
   return index;
}


void CB_SetSelected (HWND hCombo, UINT index)
{
   SendMessage (hCombo, CB_SETCURSEL, index, 0);
}


void CB_SetSelectedByData (HWND hCombo, LPARAM lpMatch)
{
   CB_SetSelected (hCombo, CB_GetIndex (hCombo, lpMatch));
}


UINT CB_GetSelected (HWND hCombo)
{
   return (UINT)SendMessage (hCombo, CB_GETCURSEL, 0, 0);
}


LPARAM CB_GetSelectedData (HWND hCombo)
{
   UINT  index;

   if ((index = CB_GetSelected (hCombo)) == -1)
      return 0;

   return CB_GetData (hCombo, index);
}


LPARAM CB_GetData (HWND hCombo, UINT index)
{
   return (LPARAM)SendMessage (hCombo, CB_GETITEMDATA, index, 0);
}


UINT CB_GetIndex (HWND hCombo, LPARAM lpMatch)
{
   UINT  idxMax = (UINT)SendMessage (hCombo, CB_GETCOUNT, 0, 0);

   for (UINT index = 0; index < idxMax; index++)
      {
      if (SendMessage (hCombo, CB_GETITEMDATA, index, 0) == lpMatch)
         return index;
      }

   return (UINT)-1;
}


/*
 * LIST BOXES _________________________________________________________________
 *
 */

LPARAM LB_StartChange (HWND hList, BOOL fReset)
{
   LPARAM dwSelected = LB_GetSelectedData (hList);
   SendMessage (hList, WM_SETREDRAW, FALSE, 0);
   if (fReset)
      SendMessage (hList, LB_RESETCONTENT, 0, 0);
   return dwSelected;
}


void LB_EndChange (HWND hList, LPARAM lpToSelect)
{
   SendMessage (hList, WM_SETREDRAW, TRUE, 0);

   if (lpToSelect != (LPARAM)NULL)
      LB_SetSelectedByData (hList, lpToSelect);
}


UINT LB_AddItem (HWND hList, int nsz, LPARAM lp)
{
   LPTSTR  psz;
   UINT    rc;

   psz = FormatString (nsz);
   rc = LB_AddItem (hList, psz, lp);
   FreeString (psz);

   return rc;
}


UINT LB_AddItem (HWND hList, LPCTSTR psz, LPARAM lp)
{
   UINT  index;
   if ((index = (UINT)SendMessage (hList, LB_ADDSTRING, 0, (LPARAM)psz)) != -1)
      {
      SendMessage (hList, LB_SETITEMDATA, index, lp);
      }
   return index;
}


void LB_EnsureVisible (HWND hList, UINT index)
{
   int cyItem;
   if ((cyItem = SendMessage (hList, LB_GETITEMHEIGHT, 0, 0)) != 0)
      {
      RECT rClient;
      GetClientRect (hList, &rClient);
      int cWindow;
      if ((cWindow = cyRECT(rClient) / cyItem) == 0)
         cWindow = 1;

      int idxTop = SendMessage (hList, LB_GETTOPINDEX, 0, 0);
      if (index < (UINT)idxTop)
         {
         SendMessage (hList, LB_SETTOPINDEX, index, 0);
         }
      else if (index >= (UINT)(idxTop +cWindow))
         {
         SendMessage (hList, LB_SETTOPINDEX, index -cWindow +1, 0);
         }
      }
}


void LB_SetSelected (HWND hList, UINT index)
{
   SendMessage (hList, LB_SETCURSEL, index, 0);
   LB_EnsureVisible (hList, index);
}


void LB_SetSelectedByData (HWND hList, LPARAM lpMatch)
{
   LB_SetSelected (hList, LB_GetIndex (hList, lpMatch));
}


UINT LB_GetSelected (HWND hList)
{
   return (UINT)SendMessage (hList, LB_GETCURSEL, 0, 0);
}


LPARAM LB_GetSelectedData (HWND hList)
{
   UINT  index;

   if ((index = LB_GetSelected (hList)) == (UINT)-1)
      return 0;

   return LB_GetData (hList, index);
}


LPARAM LB_GetData (HWND hList, UINT index)
{
   return (LPARAM)SendMessage (hList, LB_GETITEMDATA, index, 0);
}


UINT LB_GetIndex (HWND hList, LPARAM lpMatch)
{
   UINT  idxMax = (UINT)SendMessage (hList, LB_GETCOUNT, 0, 0);

   for (UINT index = 0; index < idxMax; index++)
      {
      if (SendMessage (hList, LB_GETITEMDATA, index, 0) == lpMatch)
         return index;
      }

   return (UINT)-1;
}


WORD LB_GetStringExtent (HWND hList, LPTSTR pszString)
{
   HDC hdc = GetDC (hList);

   SIZE sz;
   GetTextExtentPoint32 (hdc, pszString, lstrlen(pszString), &sz);

   ReleaseDC (hList, hdc);
   return (WORD)( sz.cx + 2 * GetSystemMetrics (SM_CXBORDER) );
}


BOOL CALLBACK ListBox_HScrollHook (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hList, ListBox_HScrollHook);

   switch (msg)
      {
      case WM_DESTROY:
         Subclass_RemoveHook (hList, ListBox_HScrollHook);
         break;

      case LB_ADDSTRING:
         {
         LPTSTR pszString = (LPTSTR)lp;
         WORD cxString = LB_GetStringExtent (hList, pszString);
         WORD cxExtent = (WORD)SendMessage (hList, LB_GETHORIZONTALEXTENT, 0, 0);
         if (cxString > cxExtent)
            SendMessage (hList, LB_SETHORIZONTALEXTENT, (WPARAM)cxString, 0);
         }
         break;

      case LB_DELETESTRING:
         {
         int iItemSkip = (int)wp;
         int iItemMax = SendMessage (hList, LB_GETCOUNT, 0, 0);
         int cchMax = 0;
         int iItem;

         for (iItem = 0; iItem < iItemMax; iItem++)
            {
            if (iItem == iItemSkip)
               continue;
            int cch = SendMessage (hList, LB_GETTEXTLEN, (WPARAM)iItem, 0);
            cchMax = max (cch, cchMax);
            }

         WORD cxExtent = (WORD)SendMessage (hList, LB_GETHORIZONTALEXTENT, 0, 0);
         WORD cxStringMax = 0;
         LPTSTR pszString = AllocateString (cchMax +1);

         for (iItem = 0; iItem < iItemMax; iItem++)
            {
            if (iItem == iItemSkip)
               continue;
            SendMessage (hList, LB_GETTEXT, (WPARAM)iItem, (LPARAM)pszString);
            WORD cxString = LB_GetStringExtent (hList, pszString);
            cxStringMax = max (cxString, cxStringMax);
            }

         FreeString (pszString);
         SendMessage (hList, LB_SETHORIZONTALEXTENT, (WPARAM)cxStringMax, 0);
         }
         break;
      }

   if (oldProc)
      return CallWindowProc ((WNDPROC)oldProc, hList, msg, wp, lp);
   else
      return DefWindowProc (hList, msg, wp, lp);
}


void LB_EnableHScroll (HWND hList)
{
   Subclass_AddHook (hList, ListBox_HScrollHook);
}


/*
 * TREEVIEWS __________________________________________________________________
 *
 */

LPARAM TV_GetData (HWND hTree, HTREEITEM hti)
{
   TV_ITEM tvi;
   tvi.lParam = 0;
   tvi.hItem = hti;
   tvi.mask = TVIF_PARAM;

   if (!TreeView_GetItem (hTree, &tvi))
      return (LPARAM)NULL;

   return tvi.lParam;
}


LPARAM TV_GetSelectedData (HWND hTree)
{
   HTREEITEM hti = TreeView_GetSelection (hTree);

   if (hti == NULL)
      return NULL;

   return TV_GetData (hTree, hti);
}


LPARAM TV_StartChange (HWND hTree, BOOL fReset)
{
   LPARAM dwSelected = TV_GetSelectedData (hTree);
   SendMessage (hTree, WM_SETREDRAW, FALSE, 0);
   if (fReset)
      TreeView_DeleteAllItems (hTree);
   return dwSelected;
}


void TV_EndChange (HWND hTree, LPARAM lpToSelect)
{
   SendMessage (hTree, WM_SETREDRAW, TRUE, 0);

   if (lpToSelect != NULL)
      {
      HTREEITEM hti = TV_RecursiveFind (hTree, TVI_ROOT, lpToSelect);
      if (hti != NULL)
         TreeView_SelectItem (hTree, hti);
      }
}


HTREEITEM TV_RecursiveFind (HWND hTree, HTREEITEM htiRoot, LPARAM lpToFind)
{
   for (HTREEITEM hti = (htiRoot == TVI_ROOT) ? TreeView_GetRoot (hTree) : TreeView_GetChild (hTree, htiRoot);
        hti != NULL;
        hti = TreeView_GetNextSibling (hTree, hti))
      {
      if (lpToFind == TV_GetData (hTree, hti))
         return hti;

      HTREEITEM htiFound;
      if ((htiFound = TV_RecursiveFind (hTree, hti, lpToFind)) != NULL)
         return htiFound;
      }

   return NULL;
}


/*
 * COMMON DIALOGS _____________________________________________________________
 *
 */

BOOL Browse_Open (HWND hParent, LPTSTR pszFilename, LPTSTR pszLastDirectory, int idsFilter, int iddTemplate, LPOFNHOOKPROC lpfnHook, DWORD lCustData)
{
   TCHAR szFilter[ cchRESOURCE ];
   GetString (szFilter, idsFilter);
   TCHAR chFilter = szFilter[ lstrlen(szFilter)-1 ];
   for (LPTSTR pszFilter = szFilter;
        (*pszFilter) && ((pszFilter = (LPTSTR)lstrchr (pszFilter, chFilter)) != NULL);
        ++pszFilter)
      {
      *pszFilter = TEXT('\0');
      }

   OPENFILENAME ofn;
   memset (&ofn, 0x00, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = hParent;
   ofn.hInstance = THIS_HINST;
   ofn.lpstrFilter = szFilter;
   ofn.nFilterIndex = 1;
   ofn.lpstrFile = pszFilename;
   ofn.nMaxFile = MAX_PATH;
   ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
   ofn.lCustData = lCustData;

   if (iddTemplate != 0)
      {
      ofn.lpTemplateName = MAKEINTRESOURCE( iddTemplate );
      ofn.Flags |= OFN_ENABLETEMPLATE | OFN_EXPLORER;
      }
   if (lpfnHook != NULL)
      {
      ofn.lpfnHook = lpfnHook;
      ofn.Flags |= OFN_ENABLEHOOK | OFN_EXPLORER;
      }

   TCHAR szPath[ MAX_PATH ];
   GetCurrentDirectory (MAX_PATH, szPath);
   if (pszLastDirectory && *pszLastDirectory)
      SetCurrentDirectory (pszLastDirectory);

   BOOL rc = GetOpenFileName (&ofn);

   if (pszLastDirectory)
      GetCurrentDirectory (MAX_PATH, pszLastDirectory);
   SetCurrentDirectory (szPath);

   return rc;
}


BOOL Browse_Save (HWND hParent, LPTSTR pszFilename, LPTSTR pszLastDirectory, int idsFilter, int iddTemplate, LPOFNHOOKPROC lpfnHook, DWORD lCustData)
{
   TCHAR szFilter[ cchRESOURCE ];
   GetString (szFilter, idsFilter);
   TCHAR chFilter = szFilter[ lstrlen(szFilter)-1 ];
   for (LPTSTR pszFilter = szFilter;
        (*pszFilter) && ((pszFilter = (LPTSTR)lstrchr (pszFilter, chFilter)) != NULL);
        ++pszFilter)
      {
      *pszFilter = TEXT('\0');
      }

   OPENFILENAME sfn;
   memset (&sfn, 0x00, sizeof(sfn));
   sfn.lStructSize = sizeof(sfn);
   sfn.hwndOwner = hParent;
   sfn.hInstance = THIS_HINST;
   sfn.lpstrFilter = szFilter;
   sfn.nFilterIndex = 1;
   sfn.lpstrFile = pszFilename;
   sfn.nMaxFile = MAX_PATH;
   sfn.Flags = OFN_HIDEREADONLY | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
   sfn.lCustData = lCustData;

   if (iddTemplate != 0)
      {
      sfn.lpTemplateName = MAKEINTRESOURCE( iddTemplate );
      sfn.Flags |= OFN_ENABLETEMPLATE | OFN_EXPLORER;
      }
   if (lpfnHook != NULL)
      {
      sfn.lpfnHook = lpfnHook;
      sfn.Flags |= OFN_ENABLEHOOK | OFN_EXPLORER;
      }

   TCHAR szPath[ MAX_PATH ];
   GetCurrentDirectory (MAX_PATH, szPath);
   if (pszLastDirectory && *pszLastDirectory)
      SetCurrentDirectory (pszLastDirectory);

   BOOL rc = GetSaveFileName (&sfn);

   if (pszLastDirectory)
      GetCurrentDirectory (MAX_PATH, pszLastDirectory);
   SetCurrentDirectory (szPath);

   return rc;
}


/*
 * HOURGLASS CURSOR ETC _______________________________________________________
 *
 */

static LONG nHourGlassRequests = 0;

void StartHourGlass (void)
{
   if ((++nHourGlassRequests) == 1)
      {
      SetCursor (LoadCursor (NULL, IDC_WAIT));
      }
}

void StopHourGlass (void)
{
   if ((nHourGlassRequests == 0) || ((--nHourGlassRequests) == 0))
      {
      SetCursor (LoadCursor (NULL, IDC_ARROW));
      }
}



void DisplayContextMenu (HMENU hm, POINT ptScreen, HWND hParent)
{
   HMENU hmDummy = CreateMenu();
   InsertMenu (hmDummy, 0, MF_POPUP, (UINT)hm, NULL);

   TrackPopupMenu (GetSubMenu (hmDummy, 0),
                   TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                   ptScreen.x, ptScreen.y,
                   0, hParent, NULL);

   DestroyMenu (hmDummy);
}


size_t CountChildren (HWND hParent, LPTSTR pszClass)
{
   size_t nFound = 0;

   for (HWND hChild = GetWindow (hParent, GW_CHILD);
        hChild != NULL;
        hChild = GetWindow (hChild, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];

      if (GetClassName (hChild, szClassName, cchRESOURCE))
         {
         if (!lstrcmp (szClassName, pszClass))
            ++nFound;
         }
      }

   return nFound;
}


WORD NextControlID (HWND hParent)
{
   WORD wNext = 1;

   for (HWND hChild = GetWindow (hParent, GW_CHILD);
        hChild != NULL;
        hChild = GetWindow (hChild, GW_HWNDNEXT))
      {
      LONG wChild = GetWindowLong (hChild, GWL_ID);
      if ((wChild < 0) || (wChild >= 0xF000))
         continue;

      wNext = max( wNext, (WORD) wChild+1 );
      }

   return wNext;
}


BOOL IsAncestor (HWND hParent, HWND hChild)
{
   for ( ; hChild; hChild = GetParent(hChild))
      {
      if (hParent == hChild)
         return TRUE;
      }
   return FALSE;
}


HWND GetTabChild (HWND hTab)
{
   for (HWND hChild = GetWindow (hTab, GW_CHILD);
        hChild != NULL;
        hChild = GetWindow (hChild, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];

      if (GetClassName (hChild, szClassName, cchRESOURCE))
         {
         if (!lstrcmp (szClassName, TEXT("#32770"))) // WC_DIALOG
            return hChild;
         }
      }

   return NULL;
}


HWND GetLastDlgTabItem (HWND hDlg)
{
   HWND hFirst = NULL;
   HWND hLast = NULL;

   for (HWND hSearch = GetNextDlgTabItem (hDlg, NULL, FALSE);
        (hSearch != NULL) && (hSearch != hFirst);
        hSearch = GetNextDlgTabItem (hDlg, hSearch, FALSE))
      {
      if (!hFirst)
         hFirst = hSearch;
      hLast = hSearch;
      }

   return hLast;
}


BOOL IsPropSheet (HWND hSheet)
{
   HWND hTab;
   if ((hTab = GetDlgItem (hSheet, IDC_PROPSHEET_TABCTRL)) == NULL)
      return FALSE;

   TCHAR szClassName[ cchRESOURCE ];
   if (!GetClassName (hTab, szClassName, cchRESOURCE))
      return FALSE;

   if (lstrcmp (szClassName, WC_TABCONTROL))
      return FALSE;

   return TRUE;
}

