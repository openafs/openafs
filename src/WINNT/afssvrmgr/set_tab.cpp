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
#include "svr_window.h"
#include "svr_general.h"
#include "svc_tab.h"
#include "agg_tab.h"
#include "agg_general.h"
#include "set_tab.h"
#include "set_general.h"
#include "set_move.h"
#include "set_repprop.h"
#include "set_createrep.h"
#include "display.h"
#include "columns.h"
#include "command.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdFilesets[] = {
    { IDC_SET_DESC,        raSizeX,		0,	0 },
    { IDC_SET_LIST,        raSizeX | raSizeY,	0,	0 },
    { IDC_SET_CREATE,      raMoveX | raMoveY,	0,	0 },
    { IDC_SET_DELETE,      raMoveX | raMoveY,	0,	0 },
    { IDC_SET_REP,         raMoveX | raMoveY,	0,	0 },
    { IDC_SET_SETQUOTA,    raMoveX | raMoveY,	0,	0 },
    { idENDLIST,           0,			0,	0 }
 };


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct // l
   {
   BOOL fDragging;
   BOOL fRightBtn;
   HIMAGELIST hDragImage;
   LPIDENT lpiDrag;
   LPIDENT lpiTarget;
   HWND hwndTarget;
   HLISTITEM hItemTarget;
   } l;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Filesets_OnView (HWND hDlg);
void Filesets_OnSelect (HWND hDlg);
void Filesets_OnEndTask_Select (HWND hDlg, LPTASKPACKET ptp);
void Filesets_OnEndTask_BeginDrag (HWND hDlg, LPTASKPACKET ptp);
void Filesets_OnEndTask_DragMenu (HWND hDlg, LPTASKPACKET ptp);
void Filesets_OnEndTask_Menu (HWND hDlg, LPTASKPACKET ptp);

void Filesets_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns);

LPIDENT IdentifyPoint (HWND hTarget, POINT ptClient, HLISTITEM *phItemTarget);

void Filesets_SubclassList (HWND hDlg);
void Filesets_Subclass_OnCommand (HWND hList, UINT msg, WPARAM wp, LPARAM lp);

BOOL Filesets_BeginDrag (HWND hDlg, BOOL fRightBtn);
void Filesets_ContinueDrag (HWND hDlg);
void Filesets_FinishDrag (HWND hDlg, BOOL fDropped, BOOL fMenu);
void Filesets_CancelDrag (HWND hDlg);

void Filesets_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Filesets_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewSet))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         ResizeWindow (hDlg, awdFilesets, rwaMoveToHere, &rTab);

         FL_RestoreView (GetDlgItem (hDlg, IDC_SET_LIST), &gr.viewSet);
         FastList_SetTextCallback (GetDlgItem (hDlg, IDC_SET_LIST), GetItemText, &gr.viewSet);

         Filesets_SubclassList (hDlg);

         Filesets_OnView (hDlg);
         Filesets_OnSelect (hDlg);
         }
         break;

      case WM_HELP:
         WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case WM_DESTROY:
         FL_StoreView (GetDlgItem (hDlg, IDC_SET_LIST), &gr.viewSet);
         DontNotifyMeEver (hDlg);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdFilesets, rwaFixupGuts);
         break;

      case WM_CONTEXTMENU:
         {
         POINT ptScreen;
         POINT ptClient;
         ptScreen.x = LOWORD(lp);
         ptScreen.y = HIWORD(lp);

         ptClient = ptScreen;
         ScreenToClient ((HWND)wp, &ptClient);

         if ((HWND)wp == GetDlgItem (hDlg, IDC_SET_LIST))
            Filesets_ShowPopupMenu ((HWND)wp, ptClient, ptScreen);
         }
         break; 

      case WM_SERVER_CHANGED:
         {
         LPIDENT lpiServer = Server_GetServerForChild (hDlg);
         DontNotifyMeEver (hDlg);
         NotifyMe (WHEN_OBJECT_CHANGES, NULL, hDlg, 0);

         // Fix the text at the top of the Aggregates tab:
         //
         TCHAR szName[ cchRESOURCE ];
         LPTSTR pszText;

         if (lpiServer != NULL)
            {
            LPSERVER_PREF lpsp = (LPSERVER_PREF)lpiServer->GetUserParam();
            lpiServer->GetServerName (szName);
            if (lpsp && !lpsp->fIsMonitored)
               pszText = FormatString (IDS_FILESET_UNMON, TEXT("%s"), szName);
            else
               pszText = FormatString (IDS_FILESETS_IN_SERVER, TEXT("%s"), szName);
            }
         else if (g.lpiCell != NULL)
            {
            g.lpiCell->GetCellName (szName);
            if (g.sub)
               pszText = FormatString (IDS_FILESET_SOME, TEXT("%s"), szName);
            else
               pszText = FormatString (IDS_FILESETS_IN_CELL, TEXT("%s"), szName);
            }
         else
            {
            pszText = FormatString (IDS_FILESETS_IN_NOTHING);
            }

         SetDlgItemText (hDlg, IDC_SET_DESC, pszText);
         FreeString (pszText);

         UpdateDisplay_Filesets (FALSE, GetDlgItem (hDlg, IDC_SET_LIST), NULL, 0, NULL, NULL, NULL);
         }
         break;

      case WM_NOTIFY_FROM_DISPATCH:
         Filesets_OnNotifyFromDispatch ((LPNOTIFYSTRUCT)lp);
         Delete ((LPNOTIFYSTRUCT)lp);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_SET_CREATE:
               SendMessage (GetDlgItem (hDlg, IDC_SET_LIST), WM_COMMAND, M_SET_CREATE, 0);
               break;

            case IDC_SET_DELETE:
               SendMessage (GetDlgItem (hDlg, IDC_SET_LIST), WM_COMMAND, M_SET_DELETE, 0);
               break;

            case IDC_SET_REP:
               SendMessage (GetDlgItem (hDlg, IDC_SET_LIST), WM_COMMAND, M_SET_REPLICATION, 0);
               break;

            case IDC_SET_SETQUOTA:
               SendMessage (GetDlgItem (hDlg, IDC_SET_LIST), WM_COMMAND, M_SET_SETQUOTA, 0);
               break;
            }
         break;

      case WM_MOUSEMOVE:
         Filesets_ContinueDrag (hDlg);
         break;

      case WM_LBUTTONDOWN:
         if (l.fDragging && l.fRightBtn)
            Filesets_FinishDrag (hDlg, FALSE, FALSE);
         break;

      case WM_RBUTTONDOWN:
         if (l.fDragging && !l.fRightBtn)
            Filesets_FinishDrag (hDlg, FALSE, FALSE);
         break;

      case WM_LBUTTONUP:
         if (l.fDragging && !l.fRightBtn)
            Filesets_FinishDrag (hDlg, TRUE, FALSE);
         break;

      case WM_RBUTTONUP:
         if (l.fDragging && l.fRightBtn)
            Filesets_FinishDrag (hDlg, TRUE, TRUE);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskSET_SELECT)
               Filesets_OnEndTask_Select (hDlg, ptp);
            else if (ptp->idTask == taskSET_BEGINDRAG)
               Filesets_OnEndTask_BeginDrag (hDlg, ptp);
            else if (ptp->idTask == taskSET_DRAGMENU)
               Filesets_OnEndTask_DragMenu (hDlg, ptp);
            else if (ptp->idTask == taskSET_MENU)
               Filesets_OnEndTask_Menu (hDlg, ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_ITEMSELECT:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_SET_LIST))
                  {
                  Filesets_OnSelect (hDlg);
                  }
               break;

            case FLN_ITEMEXPAND:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_SET_LIST))
                  {
                  HLISTITEM hItem = ((LPFLN_ITEMEXPAND_PARAMS)lp)->hItem;
                  LPIDENT lpi = (LPIDENT)FastList_GetItemParam (GetDlgItem (hDlg, IDC_SET_LIST), hItem);
                  BOOL fExpanded = ((LPFLN_ITEMEXPAND_PARAMS)lp)->fExpanded;

                  if (lpi && lpi->fIsServer())
                     {
                     LPSERVER_PREF lpsp;
                     if ((lpsp = (LPSERVER_PREF)lpi->GetUserParam()) != NULL)
                        {
                        lpsp->fExpandTree = fExpanded;
                        Server_SavePreferences (lpi);
                        }
                     }
                  else if (lpi && lpi->fIsAggregate())
                     {
                     LPAGGREGATE_PREF lpap;
                     if ((lpap = (LPAGGREGATE_PREF)lpi->GetUserParam()) != NULL)
                        {
                        lpap->fExpandTree = fExpanded;
                        Aggregates_SavePreferences (lpi);
                        }
                     }
                  }
               break;

            case FLN_BEGINDRAG:
               return Filesets_BeginDrag (hDlg, ((LPFLN_DRAG_PARAMS)lp)->fRightButton);

            case FLN_LDBLCLICK:
               if (((LPNMHDR)lp)->hwndFrom == GetDlgItem (hDlg, IDC_SET_LIST))
                  {
                  BOOL fShowProperties = TRUE;

                  HLISTITEM hItem;
                  if ((hItem = FastList_GetFocus (GetDlgItem (hDlg, IDC_SET_LIST))) != NULL)
                     {
                     if (FastList_FindFirstChild (GetDlgItem (hDlg, IDC_SET_LIST), hItem))
                        fShowProperties = FALSE;
                     }

                  LPIDENT lpi = Filesets_GetFocused (hDlg);
                  if (lpi && !lpi->fIsCell() && fShowProperties)
                     {
                     PostMessage (GetDlgItem (hDlg, IDC_SET_LIST), WM_COMMAND, M_PROPERTIES, 0);
                     return TRUE;
                     }
                  }
               break;
            }
         break;
      }

   return FALSE;
}


void Filesets_OnSelect (HWND hDlg)
{
   LPIDENT lpi = Filesets_GetSelected (hDlg);

   if (!lpi || !lpi->fIsFileset())
      {
      EnableWindow (GetDlgItem (hDlg, IDC_SET_REP), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_DELETE), FALSE);
      EnableWindow (GetDlgItem (hDlg, IDC_SET_SETQUOTA), FALSE);
      }
   else
      {
      StartTask (taskSET_SELECT, hDlg, lpi);
      }
}


void Filesets_OnEndTask_Select (HWND hDlg, LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   if (!ptp->rc)
      EnableWindow (GetDlgItem (hDlg, IDC_SET_DELETE), FALSE);
   else
      EnableWindow (GetDlgItem (hDlg, IDC_SET_DELETE), TRUE);

   if ((!ptp->rc) || (TASKDATA(ptp)->fs.Type == ftCLONE))
      EnableWindow (GetDlgItem (hDlg, IDC_SET_REP), FALSE);
   else
      EnableWindow (GetDlgItem (hDlg, IDC_SET_REP), TRUE);

   if ((!ptp->rc) || (TASKDATA(ptp)->fs.Type != ftREADWRITE))
      EnableWindow (GetDlgItem (hDlg, IDC_SET_SETQUOTA), FALSE);
   else
      EnableWindow (GetDlgItem (hDlg, IDC_SET_SETQUOTA), TRUE);
}


void Filesets_OnNotifyFromDispatch (LPNOTIFYSTRUCT lpns)
{
   switch (lpns->evt)
      {
      case evtRefreshFilesetsEnd:
      case evtRefreshStatusEnd:
      case evtAlertsChanged:
      case evtDestroy:
         if ( (!lpns->Params.lpi1) ||
              (lpns->Params.lpi1->fIsServer()) ||
              (lpns->Params.lpi1->fIsAggregate()) ||
              (lpns->Params.lpi1->fIsFileset()) )
            {
            UpdateDisplay_Filesets (FALSE, GetDlgItem (lpns->hwndTarget, IDC_SET_LIST), lpns->Params.lpi1, lpns->Params.status, NULL, NULL, NULL);
            }
         break;
      }
}


static LONG_PTR procFilesetsList = 0;

LRESULT CALLBACK Filesets_SubclassListProc (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procFilesetsList == 0)
      rc = DefWindowProc (hList, msg, wp, lp);
   else
      rc = (LRESULT) CallWindowProc ((WNDPROC)procFilesetsList, hList, msg, wp, lp);

   switch (msg)
      {
      case WM_DESTROY:
         if (procFilesetsList != 0)
            SetWindowLongPtr (hList, GWLP_WNDPROC, procFilesetsList);
         break;

      case WM_COMMAND: 
         Filesets_Subclass_OnCommand (hList, msg, wp, lp);
         break; 
      }

   return rc;
}


void Filesets_Subclass_OnCommand (HWND hList, UINT msg, WPARAM wp, LPARAM lp)
{
   HWND hChild = GetParent (hList);
   LPIDENT lpi = Filesets_GetSelected (hChild);

   switch (LOWORD(wp))
      {
      case M_COLUMNS:
         ShowColumnsDialog (hChild, &gr.viewSet);
         break;

      case M_SET_VIEW_TREELIST:
         gr.viewSet.lvsView = FLS_VIEW_TREELIST;
         Filesets_OnView (hChild);
         break;

      case M_SET_VIEW_TREE:
         gr.viewSet.lvsView = FLS_VIEW_TREE;
         Filesets_OnView (hChild);
         break;

      case M_SET_VIEW_REPORT:
         gr.viewSet.lvsView = FLS_VIEW_LIST;
         Filesets_OnView (hChild);
         break;

      case M_SET_MOVEHERE:
         if (l.lpiDrag && l.lpiDrag->fIsFileset() && l.lpiTarget)
            Filesets_ShowMoveTo (l.lpiDrag, l.lpiTarget);
         break;

      case M_SET_REPHERE:
         if (l.lpiDrag && l.lpiDrag->fIsFileset() && l.lpiTarget)
            {
            Filesets_CreateReplica (l.lpiDrag, l.lpiTarget);
            }
         break;

      default:
         StartContextCommand (Server_GetWindowForChild (GetParent(hList)),
                              Server_GetServerForChild (GetParent(hList)),
                              lpi,
                              LOWORD(wp));
         break;
      }
}


void Filesets_SubclassList (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_SET_LIST);
   procFilesetsList = GetWindowLongPtr (hList, GWLP_WNDPROC);
   SetWindowLongPtr (hList, GWLP_WNDPROC, (LONG_PTR)Filesets_SubclassListProc);
}


void Filesets_OnView (HWND hDlg)
{
   HWND hList = GetDlgItem (hDlg, IDC_SET_LIST);

   DWORD dwStyle = GetWindowLong (hList, GWL_STYLE);
   dwStyle &= (~FLS_VIEW_MASK);
   dwStyle |= gr.viewSet.lvsView;

   SetWindowLong (hList, GWL_STYLE, dwStyle);
}


BOOL Filesets_BeginDrag (HWND hDlg, BOOL fRightBtn)
{
   LPIDENT lpi = Filesets_GetFocused (hDlg);

   if (!lpi || !lpi->fIsFileset())  // can only drag filesets!
      return FALSE;

   l.fRightBtn = fRightBtn;
   StartTask (taskSET_BEGINDRAG, hDlg, lpi);
   return TRUE;
}


void Filesets_OnEndTask_BeginDrag (HWND hDlg, LPTASKPACKET ptp)
{
   LPIDENT lpi = (LPIDENT)(ptp->lpUser);

   if (ptp->rc && TASKDATA(ptp)->fs.Type != ftCLONE)
      {
      HWND hList = GetDlgItem (hDlg, IDC_SET_LIST);
      HLISTITEM hItem = FL_GetSelected (hList);

      // When creating a drag image, we'll temporarily reset the object's
      // images so we can be sure it'll draw just the Fileset icon.
      //
      FastList_Begin (hList);
      int iImage1 = FastList_GetItemFirstImage (hList, hItem);
      int iImage2 = FastList_GetItemSecondImage (hList, hItem);
      FastList_SetItemFirstImage (hList, hItem, imageFILESET);
      FastList_SetItemSecondImage (hList, hItem, IMAGE_NOIMAGE);

      l.lpiDrag = lpi;
      l.lpiTarget = NULL;
      l.fDragging = TRUE;
      l.hDragImage = FastList_CreateDragImage (hList, hItem);

      FastList_SetItemFirstImage (hList, hItem, iImage1);
      FastList_SetItemSecondImage (hList, hItem, iImage2);
      FastList_End (hList);

      // Now we've got a drag image; start dragging.
      //
      ShowCursor (FALSE);
      SetCapture (hDlg);

      DWORD dw = GetMessagePos();
      POINT pt = { LOWORD(dw), HIWORD(dw) };
      ScreenToClient (NULL, &pt);

      ImageList_BeginDrag (l.hDragImage, 0, 8, 8);
      ImageList_DragEnter (NULL, pt.x, pt.y);
      }
}


void Filesets_ContinueDrag (HWND hDlg)
{
   if (l.fDragging)
      {
      LPIDENT lpi = NULL;
      HLISTITEM hItemTarget;

      DWORD dw = GetMessagePos();
      POINT pt = { LOWORD(dw), HIWORD(dw) };
      HWND hTarget = WindowFromPoint (pt);

      if (hTarget != NULL)
         {
         POINT ptClient = pt;
         ScreenToClient (hTarget, &ptClient);

         if ((lpi = IdentifyPoint (hTarget, ptClient, &hItemTarget)) != NULL)
            {
            if (!lpi->fIsServer() && !lpi->fIsAggregate())
               lpi = NULL;
            }
         }

      if (lpi != l.lpiTarget)
         {
         ImageList_DragLeave (NULL);

         if (l.hItemTarget)
            {
            LPARAM dwFlags = FastList_GetItemFlags (l.hwndTarget, l.hItemTarget);
            FastList_SetItemFlags (l.hwndTarget, l.hItemTarget, dwFlags & (~FLIF_DROPHIGHLIGHT));
            l.hItemTarget = NULL;
            l.lpiTarget = NULL;
            }

         if ((l.lpiTarget = lpi) != NULL)
            {
            l.hwndTarget = hTarget;
            l.hItemTarget = hItemTarget;
            LPARAM dwFlags = FastList_GetItemFlags (l.hwndTarget, l.hItemTarget);
            FastList_SetItemFlags (l.hwndTarget, l.hItemTarget, dwFlags | FLIF_DROPHIGHLIGHT);
            }

         ScreenToClient (NULL, &pt);
         ImageList_DragEnter (NULL, pt.x, pt.y);
         }

      ImageList_DragMove (LOWORD(dw), HIWORD(dw));
      }
}


void Filesets_FinishDrag (HWND hDlg, BOOL fDropped, BOOL fMenu)
{
   if (l.fDragging)
      {
      Filesets_ContinueDrag (hDlg);
      ReleaseCapture();
      l.fDragging = FALSE;

      ImageList_EndDrag ();
      ImageList_DragLeave (NULL);
      ShowCursor (TRUE);

      if (l.lpiTarget != NULL)
         {
         if (!fDropped)
            {
            Filesets_CancelDrag (hDlg);
            }
         else if (fDropped && !fMenu)
            {
            SendMessage (GetDlgItem (hDlg, IDC_SET_LIST), WM_COMMAND, M_SET_MOVEHERE, 0);
            Filesets_CancelDrag (hDlg);
            }
         else // (fDropped && fMenu)
            {
            DWORD dw = GetMessagePos();
            POINT pt = { LOWORD(dw), HIWORD(dw) };

            LPMENUTASK lpp = New (MENUTASK);
            lpp->hParent = GetDlgItem (hDlg, IDC_SET_LIST);
            lpp->ptScreen = pt;
            lpp->lpi = l.lpiDrag;

            StartTask (taskSET_DRAGMENU, hDlg, lpp);
            }
         }
      }
}


void Filesets_OnEndTask_DragMenu (HWND hDlg, LPTASKPACKET ptp)
{
   HMENU hm = TaLocale_LoadMenu (MENU_SET_DRAGDROP);

   if (!ptp->rc)
      EnableMenu (hm, M_SET_MOVEHERE, FALSE);

   if (!ptp->rc || (TASKDATA(ptp)->fs.Type != ftREADWRITE))
      EnableMenu (hm, M_SET_REPHERE, FALSE);

   DisplayContextMenu (hm, TASKDATA(ptp)->mt.ptScreen, TASKDATA(ptp)->mt.hParent);
   Filesets_CancelDrag (hDlg);
}


void Filesets_CancelDrag (HWND hDlg)
{
   if (l.hItemTarget != NULL)
      {
      LPARAM dwFlags = FastList_GetItemFlags (l.hwndTarget, l.hItemTarget);
      FastList_SetItemFlags (l.hwndTarget, l.hItemTarget, dwFlags & (~FLIF_DROPHIGHLIGHT));
      l.hItemTarget = NULL;
      }
}


LPIDENT IdentifyPoint (HWND hTarget, POINT ptClient, HLISTITEM *phItemTarget)
{
   if ((*phItemTarget = FastList_ItemFromPoint (hTarget, &ptClient, TRUE)) == NULL)
      return NULL;

   return (LPIDENT)FL_GetData (hTarget, *phItemTarget);
}


void Filesets_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen)
{
   if (!ptScreen.x && !ptScreen.y)
      {
      RECT rWindow;
      GetWindowRect (hList, &rWindow);
      ptScreen.x = rWindow.left + (rWindow.right -rWindow.left)/2;
      ptScreen.y = rWindow.top + (rWindow.bottom -rWindow.top)/2;
      Filesets_ShowParticularPopupMenu (hList, ptScreen, NULL);
      }
   else if (FL_HitTestForHeaderBar (hList, ptList))
      {
      HMENU hm = TaLocale_LoadMenu (MENU_COLUMNS);
      DisplayContextMenu (hm, ptScreen, hList);
      }
   else
      {
      LPIDENT lpi = Filesets_GetFocused (GetParent (hList), &ptList);
      if (lpi && (lpi != Filesets_GetSelected (GetParent (hList))))
         {
         lpi = NULL; // right-click on item other than highlighted one?
         }

      if (lpi && lpi->fIsFileset())
         {
         Filesets_ShowParticularPopupMenu (hList, ptScreen, lpi);
         }
      else if (lpi && lpi->fIsAggregate())
         {
         Aggregates_ShowParticularPopupMenu (hList, ptScreen, lpi);
         }
      else if (lpi && lpi->fIsServer())
         {
         Server_ShowParticularPopupMenu (hList, ptScreen, lpi);
         }
      else if (!lpi) // display the _NONE menu? (no menu if it's a cell ident)
         {
         Filesets_ShowParticularPopupMenu (hList, ptScreen, lpi);
         }
      }
}


void Filesets_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiFileset)
{
   LPMENUTASK pmt;
   if ((pmt = New (MENUTASK)) != NULL)
      {
      pmt->hParent = hParent;
      pmt->ptScreen = ptScreen;
      pmt->lpi = lpiFileset;
      StartTask (taskSET_MENU, GetParent(hParent), pmt);
      }
}


void Filesets_OnEndTask_Menu (HWND hDlg, LPTASKPACKET ptp)
{
   LPIDENT lpiFileset = (LPIDENT)(TASKDATA(ptp)->mt.lpi);

   HMENU hm;
   if (lpiFileset != NULL)
      hm = TaLocale_LoadMenu (MENU_SET);
   else
      hm = TaLocale_LoadMenu (MENU_SET_NONE);

   if (hm != NULL)
      {
      if (lpiFileset == NULL)
         {
         HMENU hmView = GetSubMenu (hm, 0);
         DWORD dwStyle = GetWindowLong (TASKDATA(ptp)->mt.hParent, GWL_STYLE);

         CheckMenuRadioItem (hmView,
                             M_SET_VIEW_REPORT, M_SET_VIEW_TREE,
                             ((dwStyle & FLS_VIEW_MASK) == FLS_VIEW_TREELIST) ? M_SET_VIEW_TREELIST :
                             ((dwStyle & FLS_VIEW_MASK) == FLS_VIEW_TREE)     ? M_SET_VIEW_TREE : M_SET_VIEW_REPORT,
                             MF_BYCOMMAND);

         CheckMenuRadioItem (hmView,
                             M_VIEW_ONEICON, M_VIEW_STATUS,
                             (gr.ivSet == ivTWOICONS) ? M_VIEW_TWOICONS :
                             (gr.ivSet == ivONEICON)  ? M_VIEW_ONEICON : M_VIEW_STATUS,
                             MF_BYCOMMAND);
         }
      else
         {
         if (!ptp->rc)
            {
            EnableMenu (hm, M_SET_REPLICATION, FALSE);
            EnableMenu (hm, M_SET_SETQUOTA,    FALSE);
            EnableMenu (hm, M_SET_RELEASE,     FALSE);
            EnableMenu (hm, M_SET_CLONE,       FALSE);
            EnableMenu (hm, M_SET_MOVETO,      FALSE);
            EnableMenu (hm, M_SET_DUMP,        FALSE);
            EnableMenu (hm, M_SET_RESTORE,     FALSE);
            }
         else
            {
            if (TASKDATA(ptp)->fs.Type == ftCLONE)
               {
               EnableMenu (hm, M_SET_REPLICATION, FALSE);
               EnableMenu (hm, M_SET_RELEASE,     FALSE);
               EnableMenu (hm, M_SET_CLONE,       FALSE);
               EnableMenu (hm, M_SET_MOVETO,      FALSE);
               }

            if (TASKDATA(ptp)->fs.Type != ftREADWRITE)
               {
               EnableMenu (hm, M_SET_CLONE, FALSE);
               EnableMenu (hm, M_SET_RELEASE, FALSE);
               EnableMenu (hm, M_SET_SETQUOTA, FALSE);
               EnableMenu (hm, M_SET_RESTORE, FALSE);
               }
            }
         }

      DisplayContextMenu (hm, TASKDATA(ptp)->mt.ptScreen, TASKDATA(ptp)->mt.hParent);
      }
}

