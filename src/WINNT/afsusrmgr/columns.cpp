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
#include "columns.h"
#include "display.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Columns_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Columns_OnInitDialog (HWND hDlg, LPVIEWINFO lpviDefault);
void Columns_OnSelect (HWND hDlg);
void Columns_OnSelAvail (HWND hDlg);
void Columns_OnSelShown (HWND hDlg);
void Columns_OnInsert (HWND hDlg);
void Columns_OnDelete (HWND hDlg);
void Columns_OnMoveUp (HWND hDlg);
void Columns_OnMoveDown (HWND hDlg);
void Columns_OnApply (HWND hDlg);


/*
 * COLUMNS DIALOG _____________________________________________________________
 *
 */

typedef enum
   {
   ceUSERS,           // gr.viewUsr
   ceGROUPS,          // gr.viewGrp
   ceMACHINES,        // gr.viewMch
   } COLUMNS_ENTRY;

static struct
   {
   int ids;
   BOOL fUsuallyHidden;
   VIEWINFO vi;
   BOOL fChanged;
   }
COLUMNS[] =
   {
      { IDS_COL_USERS,        FALSE,	{0},	FALSE },  // ceUSERS
      { IDS_COL_GROUPS,       FALSE,	{0},	FALSE },  // ceGROUPS
      { IDS_COL_MACHINES,     FALSE,	{0},	FALSE },  // ceMACHINES
   };

#define nCOLUMNS (sizeof(COLUMNS)/sizeof(COLUMNS[0]))

typedef struct
   {
   HWND hParent;
   LPVIEWINFO lpviDefault;
   } ShowColumnsParams;

void ShowColumnsDialog (HWND hParent, LPVIEWINFO lpvi)
{
   for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
      {
      COLUMNS[ iCol ].fChanged = FALSE;
      }

   memcpy (&COLUMNS[ ceUSERS    ].vi, &gr.viewUsr, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceGROUPS   ].vi, &gr.viewGrp, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceMACHINES ].vi, &gr.viewMch, sizeof(VIEWINFO));

   if (lpvi == &gr.viewUsr)
      lpvi = &COLUMNS[ ceUSERS  ].vi;
   else if (lpvi == &gr.viewGrp)
      lpvi = &COLUMNS[ ceGROUPS ].vi;
   else if (lpvi == &gr.viewMch)
      lpvi = &COLUMNS[ ceMACHINES ].vi;

   // If the caller didn't specify which VIEWINFO structure to adjust
   // by default, pick whichever one is on the currently-displayed tab
   //
   if (!lpvi)
      {
      switch (Display_GetActiveTab())
         {
         case ttUSERS:
            lpvi = &COLUMNS[ ceUSERS ].vi;
            break;
         case ttGROUPS:
            lpvi = &COLUMNS[ ceGROUPS ].vi;
            break;
         case ttMACHINES:
            lpvi = &COLUMNS[ ceMACHINES ].vi;
            break;
         }
      }

   // Show the Columns dialog
   //
   LPPROPSHEET psh = PropSheet_Create (IDS_COLUMNS_TITLE, FALSE);
   psh->sh.hwndParent = hParent;

   ShowColumnsParams *pscp = New (ShowColumnsParams);
   pscp->hParent = hParent;
   pscp->lpviDefault = lpvi;

   PropSheet_AddTab (psh, 0, IDD_COLUMNS, (DLGPROC)Columns_DlgProc, (LPARAM)pscp, TRUE);
   PropSheet_ShowModal (psh, PumpMessage);
}


BOOL CALLBACK Columns_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (AfsAppLib_HandleHelp (IDD_COLUMNS, hDlg, msg, wp, lp))
      return TRUE;

   if (msg == WM_INITDIALOG)
      SetWindowLong (hDlg, DWL_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   ShowColumnsParams *pscp;
   pscp = (ShowColumnsParams *)GetWindowLong (hDlg, DWL_USER);

   switch (msg)
      {
      case WM_INITDIALOG:
         Columns_OnInitDialog (hDlg, (pscp) ? pscp->lpviDefault : NULL);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               Columns_OnApply (hDlg);
               break;

            case IDOK:
            case IDCANCEL:
               EndDialog (hDlg, LOWORD(wp));
               break;

            case IDC_COLUMNS:
               if (HIWORD(wp) == CBN_SELCHANGE)
                  {
                  Columns_OnSelect (hDlg);
                  }
               break;

            case IDC_COL_AVAIL:
               if (HIWORD(wp) == LBN_SELCHANGE)
                  {
                  Columns_OnSelAvail (hDlg);
                  }
               break;

            case IDC_COL_SHOWN:
               if (HIWORD(wp) == LBN_SELCHANGE)
                  {
                  Columns_OnSelShown (hDlg);
                  }
               break;

            case IDC_COL_INSERT:
               Columns_OnInsert (hDlg);
               break;

            case IDC_COL_DELETE:
               Columns_OnDelete (hDlg);
               break;

            case IDC_COL_UP:
               Columns_OnMoveUp (hDlg);
               break;

            case IDC_COL_DOWN:
               Columns_OnMoveDown (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Columns_OnInitDialog (HWND hDlg, LPVIEWINFO lpviDefault)
{
   HWND hList = GetDlgItem (hDlg, IDC_COLUMNS);
   CB_StartChange (hList);

   for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
      {
      if ((COLUMNS[iCol].fUsuallyHidden) && (lpviDefault != &COLUMNS[iCol].vi))
         continue;

      CB_AddItem (hList, COLUMNS[iCol].ids, (LPARAM)&COLUMNS[iCol].vi);
      }

   CB_EndChange (hList, (LPARAM)lpviDefault);

   Columns_OnSelect (hDlg);
}


void Columns_OnSelect (HWND hDlg)
{
   HWND hList;
   LPVIEWINFO lpvi = (LPVIEWINFO)CB_GetSelectedData (GetDlgItem(hDlg,IDC_COLUMNS));

   // Fill in the Available list...
   //
   hList = GetDlgItem (hDlg, IDC_COL_AVAIL);
   LB_StartChange (hList);

   size_t iAvail;
   size_t iShown;
   for (iAvail = 0; iAvail < lpvi->nColsAvail; ++iAvail)
      {
      for (iShown = 0; iShown < lpvi->nColsShown; ++iShown)
         {
         if (lpvi->aColumns[ iShown ] == iAvail)
            break;
         }
      if (iShown == lpvi->nColsShown)
         {
         LB_AddItem (hList, lpvi->idsColumns[ iAvail ], (LPARAM)iAvail);
         }
      }

   LB_EndChange (hList);
   Columns_OnSelAvail (hDlg);

   // Fill in the Shown list...
   //
   hList = GetDlgItem (hDlg, IDC_COL_SHOWN);
   LB_StartChange (hList);

   for (iShown = 0; iShown < lpvi->nColsShown; ++iShown)
      {
      iAvail = lpvi->aColumns[ iShown ];
      LB_AddItem (hList, lpvi->idsColumns[ iAvail ], (LPARAM)iAvail);
      }

   LB_EndChange (hList);
   Columns_OnSelShown (hDlg);
}


void Columns_OnSelAvail (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_COL_AVAIL);
   int ii = LB_GetSelected (hList);

   EnableWindow (GetDlgItem (hDlg, IDC_COL_INSERT), (ii != -1));
}


void Columns_OnSelShown (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_COL_SHOWN);
   int ii = LB_GetSelected (hList);

   if (ii == -1)
      {
      EnableWindow (GetDlgItem (hDlg, IDC_COL_DELETE), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_COL_UP),     FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_COL_DOWN),   FALSE);
      }
   else
      {
      int iAvail = LB_GetData (hList, ii);
      EnableWindow (GetDlgItem (hDlg, IDC_COL_DELETE), (iAvail != 0));

      int ci = (int)SendMessage (hList, LB_GETCOUNT, 0, 0);
      EnableWindow (GetDlgItem (hDlg, IDC_COL_UP),   (ii > 1));
      EnableWindow (GetDlgItem (hDlg, IDC_COL_DOWN), (ii > 0 && ii < ci-1));
      }
}


void Columns_OnInsert (HWND hDlg)
{
   LPVIEWINFO lpvi = (LPVIEWINFO)CB_GetSelectedData (GetDlgItem (hDlg, IDC_COLUMNS));
   HWND hAvail = GetDlgItem (hDlg, IDC_COL_AVAIL);
   HWND hShown = GetDlgItem (hDlg, IDC_COL_SHOWN);
   int ii = LB_GetSelected (hAvail);

   if (ii != -1)
      {
      size_t iAvail = LB_GetData (hAvail, ii);
      int iShown = 1+ LB_GetSelected (hShown);

      TCHAR szText[ cchRESOURCE ];
      SendMessage (hAvail, LB_GETTEXT, ii, (LPARAM)szText);

      LB_AddItem (hShown, szText, iAvail);
      SendMessage (hAvail, LB_DELETESTRING, ii, 0);

      lpvi->aColumns[ lpvi->nColsShown ] = iAvail;
      lpvi->nColsShown ++;

      for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
         {
         if (lpvi == &COLUMNS[ iCol ].vi)
            COLUMNS[ iCol ].fChanged = TRUE;
         }

      Columns_OnSelAvail (hDlg);
      Columns_OnSelShown (hDlg);
      PropSheetChanged (hDlg);
      }
}


void Columns_OnDelete (HWND hDlg)
{
   LPVIEWINFO lpvi = (LPVIEWINFO)CB_GetSelectedData (GetDlgItem (hDlg, IDC_COLUMNS));
   HWND hAvail = GetDlgItem (hDlg, IDC_COL_AVAIL);
   HWND hShown = GetDlgItem (hDlg, IDC_COL_SHOWN);
   int ii = LB_GetSelected (hShown);

   if (ii != -1)
      {
      TCHAR szText[ cchRESOURCE ];
      SendMessage (hShown, LB_GETTEXT, ii, (LPARAM)szText);

      size_t iAvail = (size_t)LB_GetData (hShown, ii);
      LB_AddItem (hAvail, szText, iAvail);
      SendMessage (hShown, LB_DELETESTRING, ii, 0);

      for ( ; ii < (int)lpvi->nColsShown-1; ++ii)
         {
         lpvi->aColumns[ ii ] = lpvi->aColumns[ ii+1 ];
         }
      lpvi->nColsShown --;

      for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
         {
         if (lpvi == &COLUMNS[ iCol ].vi)
            COLUMNS[ iCol ].fChanged = TRUE;
         }

      Columns_OnSelAvail (hDlg);
      Columns_OnSelShown (hDlg);
      PropSheetChanged (hDlg);
      }
}


void Columns_OnMoveUp (HWND hDlg)
{
   LPVIEWINFO lpvi = (LPVIEWINFO)CB_GetSelectedData (GetDlgItem (hDlg, IDC_COLUMNS));
   HWND hAvail = GetDlgItem (hDlg, IDC_COL_AVAIL);
   HWND hShown = GetDlgItem (hDlg, IDC_COL_SHOWN);
   int ii = LB_GetSelected (hShown);

   if (ii > 0)
      {
      size_t iAvail = (size_t)LB_GetData (hShown, ii);
      lpvi->aColumns[ ii ] = lpvi->aColumns[ ii-1 ];
      lpvi->aColumns[ ii-1 ] = iAvail;

      for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
         {
         if (lpvi == &COLUMNS[ iCol ].vi)
            COLUMNS[ iCol ].fChanged = TRUE;
         }

      Columns_OnSelect (hDlg);
      LB_SetSelectedByData (hShown, (LPARAM)iAvail);
      Columns_OnSelShown (hDlg);
      PropSheetChanged (hDlg);
      }
}


void Columns_OnMoveDown (HWND hDlg)
{
   LPVIEWINFO lpvi = (LPVIEWINFO)CB_GetSelectedData (GetDlgItem (hDlg, IDC_COLUMNS));
   HWND hAvail = GetDlgItem (hDlg, IDC_COL_AVAIL);
   HWND hShown = GetDlgItem (hDlg, IDC_COL_SHOWN);
   int ii = LB_GetSelected (hShown);

   if (ii != -1)
      {
      size_t iAvail = (size_t)LB_GetData (hShown, ii);
      lpvi->aColumns[ ii ] = lpvi->aColumns[ ii+1 ];
      lpvi->aColumns[ ii+1 ] = iAvail;

      for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
         {
         if (lpvi == &COLUMNS[ iCol ].vi)
            COLUMNS[ iCol ].fChanged = TRUE;
         }

      Columns_OnSelect (hDlg);
      LB_SetSelectedByData (hShown, (LPARAM)iAvail);
      Columns_OnSelShown (hDlg);
      PropSheetChanged (hDlg);
      }
}


void Columns_OnApply (HWND hDlg)
{
   for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
      {
      if (COLUMNS[ iCol ].fChanged)
         {
         switch (iCol)
            {
            case ceUSERS:
               if (Display_GetActiveTab() == ttUSERS)
                  Display_RefreshView (&COLUMNS[ iCol ].vi, gr.ivUsr);
               else
                  memcpy (&gr.viewUsr, &COLUMNS[ iCol ].vi, sizeof(VIEWINFO));
               break;

            case ceGROUPS:
               if (Display_GetActiveTab() == ttGROUPS)
                  Display_RefreshView (&COLUMNS[ iCol ].vi, gr.ivGrp);
               else
                  memcpy (&gr.viewGrp, &COLUMNS[ iCol ].vi, sizeof(VIEWINFO));
               break;

            case ceMACHINES:
               if (Display_GetActiveTab() == ttMACHINES)
                  Display_RefreshView (&COLUMNS[ iCol ].vi, gr.ivMch);
               else
                  memcpy (&gr.viewMch, &COLUMNS[ iCol ].vi, sizeof(VIEWINFO));
               break;
            }
         }
      }
}

