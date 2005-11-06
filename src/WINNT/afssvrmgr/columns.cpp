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
#include "columns.h"
#include "svr_window.h"
#include "propcache.h"
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
BOOL Columns_OnApply (HWND hDlg, LPVIEWINFO lpviDefault);


/*
 * COLUMNS DIALOG _____________________________________________________________
 *
 */

typedef enum
   {
   ceSERVERS = 0,     // gr.di*.viewSvr
   ceFILESETS,        // gr.viewSet
   ceAGGREGATES,      // gr.viewAgg
   ceSERVICES,        // gr.viewSvc
   ceREPLICAS,        // gr.viewRep
   ceAGGS_MOVE,       // gr.viewAggMove
   ceAGGS_CREATE,     // gr.viewAggCreate
   ceAGGS_RESTORE     // gr.viewAggRestore
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
      { IDS_COL_SERVERS,      FALSE,	{0},	FALSE  },  // ceSERVERS
      { IDS_COL_FILESETS,     FALSE,	{0},	FALSE  },  // ceFILESETS
      { IDS_COL_AGGREGATES,   FALSE,	{0},	FALSE  },  // ceAGGREGATES
      { IDS_COL_SERVICES,     FALSE,	{0},	FALSE  },  // ceSERVICES
      { IDS_COL_REPLICAS,     FALSE,	{0},	FALSE  },  // ceREPLICAS
      { IDS_COL_AGGS_MOVE,    TRUE,		{0},	FALSE  },  // ceAGG_MOVE
      { IDS_COL_AGGS_CREATE,  TRUE,		{0},	FALSE  },  // ceAGG_CREATE
      { IDS_COL_AGGS_RESTORE, TRUE,		{0},	FALSE  }   // ceAGG_RESTORE
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

   if (gr.fPreview && !gr.fVert)
      memcpy (&COLUMNS[ ceSERVERS   ].vi, &gr.diHorz.viewSvr, sizeof(VIEWINFO));
    else
      memcpy (&COLUMNS[ ceSERVERS   ].vi, &gr.diVert.viewSvr, sizeof(VIEWINFO));

   memcpy (&COLUMNS[ ceFILESETS     ].vi, &gr.viewSet, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceAGGREGATES   ].vi, &gr.viewAgg, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceSERVICES     ].vi, &gr.viewSvc, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceREPLICAS     ].vi, &gr.viewRep, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceAGGS_MOVE    ].vi, &gr.viewAggMove, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceAGGS_CREATE  ].vi, &gr.viewAggCreate, sizeof(VIEWINFO));
   memcpy (&COLUMNS[ ceAGGS_RESTORE ].vi, &gr.viewAggRestore, sizeof(VIEWINFO));

   if (lpvi == &gr.viewSet)
      lpvi = &COLUMNS[ ceFILESETS     ].vi;
   else if (lpvi == &gr.viewAgg)
      lpvi = &COLUMNS[ ceAGGREGATES   ].vi;
   else if (lpvi == &gr.viewSvc)
      lpvi = &COLUMNS[ ceSERVICES     ].vi;
   else if (lpvi == &gr.viewRep)
      lpvi = &COLUMNS[ ceREPLICAS     ].vi;
   else if (lpvi == &gr.viewAggMove)
      lpvi = &COLUMNS[ ceAGGS_MOVE    ].vi;
   else if (lpvi == &gr.viewAggCreate)
      lpvi = &COLUMNS[ ceAGGS_CREATE  ].vi;
   else if (lpvi == &gr.viewAggRestore)
      lpvi = &COLUMNS[ ceAGGS_RESTORE ].vi;
   else
      lpvi = &COLUMNS[ ceSERVERS      ].vi;

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
      SetWindowLongPtr (hDlg, DWLP_USER, ((LPPROPSHEETPAGE)lp)->lParam);

   ShowColumnsParams *pscp;
   pscp = (ShowColumnsParams *)GetWindowLongPtr (hDlg, DWLP_USER);

   switch (msg)
      {
      case WM_INITDIALOG_SHEET:
         PropCache_Add (pcGENERAL, NULL, hDlg);
         break;

      case WM_DESTROY_SHEET:
         PropCache_Delete (hDlg);
         break;

      case WM_INITDIALOG:
         Columns_OnInitDialog (hDlg, (pscp) ? pscp->lpviDefault : NULL);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDAPPLY:
               if (Columns_OnApply (hDlg, (pscp) ? pscp->lpviDefault : NULL))
                  {
                  if (pscp)
                     {
                     PostMessage (pscp->hParent, WM_COLUMNS_CHANGED, 0, 0);
                     }
                  }
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
   if (!lpviDefault)
      {
      lpviDefault = (gr.fPreview && !gr.fVert) ? &gr.diHorz.viewSvr : &gr.diVert.viewSvr;
      }

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
      int iAvail = (int) LB_GetData (hList, ii);
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


BOOL Columns_OnApply (HWND hDlg, LPVIEWINFO lpviDefault)
{
   BOOL fPostMessage = FALSE;

   for (size_t iCol = 0; iCol < nCOLUMNS; ++iCol)
      {
      if (COLUMNS[ iCol ].fChanged)
         {
         CHILDTAB iTab = (CHILDTAB)-1;
         int idcList;

         VIEWINFO vi;
         memcpy (&vi, &COLUMNS[ iCol ].vi, sizeof(VIEWINFO));

         LPVIEWINFO lpviTarget = NULL;

         switch (iCol)
            {
            case ceSERVERS:
               if (gr.fPreview && !gr.fVert)
                  lpviTarget = &gr.diHorz.viewSvr;
               else
                  lpviTarget = &gr.diVert.viewSvr;
               FL_RestoreView (GetDlgItem (g.hMain, IDC_SERVERS), &COLUMNS[ iCol ].vi);
               UpdateDisplay_Servers (FALSE, NULL, 0);
               break;

            case ceFILESETS:
               lpviTarget = &gr.viewSet;
               iTab = tabFILESETS;
               idcList = IDC_SET_LIST;
               break;

            case ceAGGREGATES:
               lpviTarget = &gr.viewAgg;
               iTab = tabAGGREGATES;
               idcList = IDC_AGG_LIST;
               break;

            case ceSERVICES:
               lpviTarget = &gr.viewSvc;
               iTab = tabSERVICES;
               idcList = IDC_SVC_LIST;
               break;

            case ceREPLICAS:
               lpviTarget = &gr.viewRep;
               break;

            case ceAGGS_MOVE:
               lpviTarget = &gr.viewAggMove;
               break;

            case ceAGGS_CREATE:
               lpviTarget = &gr.viewAggCreate;
               break;

            case ceAGGS_RESTORE:
               lpviTarget = &gr.viewAggRestore;
               break;
            }

         if (lpviTarget != NULL)
            {
            memcpy (lpviTarget, &COLUMNS[ iCol ].vi, sizeof(VIEWINFO));
            if (lpviDefault == &COLUMNS[ iCol ].vi)
               fPostMessage = TRUE;
            }

         if (iTab != (CHILDTAB)-1)
            {
            for (HWND hServer = g.hMain;
                 hServer != NULL;
                 hServer = PropCache_Search (pcSERVER, ANYVALUE, hServer))
               {
               HWND hTab = GetDlgItem (hServer, IDC_TABS);
               int iTabShown = TabCtrl_GetCurSel (hTab);

               if (iTab == iTabShown)
                  {
                  HWND hChild = Server_GetCurrentTab (hServer);
                  HWND hList = GetDlgItem (hChild, idcList);

                  TCHAR szClassName[ cchRESOURCE ];
                  if (GetClassName (hList, szClassName, cchRESOURCE))
                     {
                     if (lstrcmp (szClassName, WC_FASTLIST))
                        continue;
                     }

                  FL_StoreView (hList, &vi);
                  COLUMNS[ iCol ].vi.lvsView = vi.lvsView;
                  FL_RestoreView (hList, &COLUMNS[ iCol ].vi);
                  Server_ForceRedraw (hServer);
                  }

               if (hServer == g.hMain)
                  hServer = NULL;
               }
            }
         }
      }

   return fPostMessage;
}

