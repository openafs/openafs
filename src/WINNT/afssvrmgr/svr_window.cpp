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
#include "propcache.h"
#include "set_tab.h"
#include "agg_tab.h"
#include "svc_tab.h"
#include "display.h"
#include "command.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

#define cxMIN_SERVER 100
#define cyMIN_SERVER 100

static rwWindowData awdServer[] = {
    { IDC_TABS,  raSizeX | raSizeY, 	MAKELONG(cxMIN_SERVER,cyMIN_SERVER), 	0 },
    { idENDLIST, 0,			0,					0 }
 };

static rwWindowData awdTabChild[] = {
    { 0,         raSizeX | raSizeY,	0,	0 },
    { idENDLIST, 0,			0,	0  }
 };


/*
 * CHILD TABS _________________________________________________________________
 *
 */

static struct // CHILDTABINFO
   {
   CHILDTAB tab;
   int idsTabTitle;
   int iImage;
   }
CHILDTABINFO[] =
   {
      { tabFILESETS,   IDS_TAB_FILESETS,   imageFILESET   },
      { tabAGGREGATES, IDS_TAB_AGGREGATES, imageAGGREGATE },
      { tabSERVICES,   IDS_TAB_SERVICES,   imageSERVICE   },
   };

#define nCHILDTABS (sizeof(CHILDTABINFO)/sizeof(CHILDTABINFO[0]))



/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Server_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Server_SaveRect (HWND hDlg, BOOL fOpen);

BOOL Server_HandleDialogKeys (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_Open (LPIDENT lpiServer, LPRECT prWindow)
{
   HWND hServer;

   if ((hServer = PropCache_Search (pcSERVER, lpiServer)) != NULL)
      {
      SetFocus (hServer);
      }
   else
      {
      // First off, if this server isn't being monitored, we have
      // to start monitoring it or we can't open a window (nothing to
      // show otherwise).
      //
      LPSERVER_PREF lpsp;
      if ((lpsp = (LPSERVER_PREF)lpiServer->GetUserParam()) != NULL)
         {
         BOOL fCanShowWindow = (gr.fOpenMonitors || lpsp->fIsMonitored);

         if (gr.fOpenMonitors && !lpsp->fIsMonitored)
            {
            StartTask (taskSVR_MONITOR_ONOFF, NULL, lpiServer);
            }

         if (fCanShowWindow)
            {
            hServer = ModelessDialogParam (IDD_SERVER, NULL, (DLGPROC)Server_DlgProc, (LPARAM)lpiServer);
            Server_SelectServer (hServer, lpiServer);

            if (prWindow->right != 0)
               {
               SetWindowPos (hServer, NULL, prWindow->left, prWindow->top,
                             cxRECT(*prWindow), cyRECT(*prWindow),
                             SWP_NOZORDER | SWP_NOACTIVATE);
               }

            ShowWindow (hServer, SW_NORMAL);
            }
         }
      }
}


void Server_Close (LPIDENT lpiServer)
{
   HWND hWnd;

   if ((hWnd = PropCache_Search (pcSERVER, lpiServer)) != NULL)
      {
      Server_SaveRect (hWnd, FALSE);
      DestroyWindow (hWnd);

      // If there's a subset active, the user may want us to stop
      // monitoring this server once its window is closed.
      //
      LPSERVER_PREF lpsp;
      if ((lpsp = (LPSERVER_PREF)lpiServer->GetUserParam()) != NULL)
         {
         if (g.sub && gr.fCloseUnmonitors && lpsp->fIsMonitored && !gr.fPreview)
            {
            StartTask (taskSVR_MONITOR_ONOFF, NULL, lpiServer);
            }
         }
      }
}


void Server_CloseAll (BOOL fUserRequested)
{
   HWND hWnd;

   while ((hWnd = PropCache_Search (pcSERVER, ANYVALUE)) != NULL)
      {
      LPIDENT lpiServer = Server_GetServer (hWnd);

      Server_SaveRect (hWnd, !fUserRequested);
      DestroyWindow (hWnd);

      if (lpiServer && fUserRequested)
         {
         // If there's a subset active, the user may want us to stop
         // monitoring this server once its window is closed.
         //
         LPSERVER_PREF lpsp;
         if ((lpsp = (LPSERVER_PREF)lpiServer->GetUserParam()) != NULL)
            {
            if (g.sub && gr.fCloseUnmonitors && lpsp->fIsMonitored)
               {
               StartTask (taskSVR_MONITOR_ONOFF, NULL, lpiServer);
               }
            }
         }
      }
}


HWND Server_GetCurrentTab (HWND hWnd)
{
   return GetTabChild (GetDlgItem (hWnd, IDC_TABS));
}

CHILDTAB Server_GetDisplayedTab (HWND hWnd)
{
   HWND hTabs = GetDlgItem (hWnd, IDC_TABS);
   return (hTabs) ? (CHILDTAB)TabCtrl_GetCurSel(hTabs) : tabINVALID;
}

HWND Server_GetWindowForChild (HWND hChild)
{
   // hChild is the Filesets_DlgProc dialog (or whatever)
   // Its parent is the tab control on the server window
   // Its grandparent is the Popup HWND (may be g.hMain) that we want.
   //
   return GetParent (GetParent (hChild));
}

void Server_SelectServer (HWND hDlg, LPIDENT lpiNew, BOOL fForceRedraw)
{
   LPIDENT lpiOld = Server_GetServer (hDlg);
   if (lpiNew != lpiOld)
      {
      SetWindowLongPtr (hDlg, DWLP_USER, (LONG_PTR)lpiNew);

      if (hDlg != g.hMain)
         {
         if (lpiOld)
            PropCache_Delete (pcSERVER, lpiOld);

         if (lpiNew)
            {
            PropCache_Add (pcSERVER, lpiNew, hDlg);

            TCHAR szName[ cchNAME ];
            lpiNew->GetServerName (szName);
            LPTSTR pszTitle = FormatString (IDS_SERVER_TITLE, TEXT("%s"), szName);
            SetWindowText (hDlg, pszTitle);
            FreeString (pszTitle);
            }
         }

      fForceRedraw = TRUE;
      }

   if (fForceRedraw)
      {
      Server_ForceRedraw (hDlg);
      }
}

LPIDENT Server_GetServer (HWND hDlg)
{
   return (LPIDENT)GetWindowLongPtr (hDlg, DWLP_USER);
}

LPIDENT Server_GetServerForChild (HWND hChild)
{
   if (GetParent(hChild) == NULL)
      return Server_GetServer (hChild);
   else
      return Server_GetServer (GetParent(GetParent(hChild)));
}


void Server_ForceRedraw (HWND hDlg)
{
   HWND hChild = Server_GetCurrentTab (hDlg);

   if (hChild && IsWindow (hChild))
      {
      PostMessage (hChild, WM_SERVER_CHANGED, 0, 0);
      }
}


/*
 * SERVER DIALOG ______________________________________________________________
 *
 */

BOOL CALLBACK Server_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   LPIDENT lpiServer = Server_GetServer (hDlg);

   if (Server_HandleDialogKeys (hDlg, msg, wp, lp))
      return TRUE;

   switch (msg)
      {
      case WM_INITDIALOG:
         RECT rWindow;
         Server_PrepareTabControl (GetDlgItem (hDlg, IDC_TABS));
         GetWindowRect (hDlg, &rWindow);
         ResizeWindow (hDlg, awdServer, rwaMoveToHere, &rWindow);
         break;

      case WM_HELP:
         WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case WM_DESTROY:
         GetWindowRect (hDlg, &gr.rServerLast);

         if (lpiServer)
            PropCache_Delete (pcSERVER, lpiServer);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            {
            ResizeWindow (hDlg, awdServer, rwaFixupGuts);
            Server_SaveRect (hDlg, TRUE);
            }
         break;

      case WM_MOVE:
         Server_SaveRect (hDlg, TRUE);
         break;

      case WM_SETFOCUS:
         gr.tabLast = Server_GetDisplayedTab (hDlg);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
               LPIDENT lpi;
               if ((lpi = Server_GetServer (hDlg)) != NULL)
                  Server_Close (lpi);
               else
                  DestroyWindow (hDlg);
               break;
            }
         break;

      case WM_NOTIFY: 
         switch (((LPNMHDR)lp)->code)
            { 
            case TCN_SELCHANGE:
               { 
               HWND hTab = GetDlgItem (hDlg, IDC_TABS);
               int iPage = TabCtrl_GetCurSel (hTab); 

               Server_DisplayTab (hDlg, (CHILDTAB)iPage);
               gr.tabLast = (CHILDTAB)iPage;
               }
               break;
            }
         break;
      }

   return FALSE;
}


static LONG_PTR procTabControl = 0;

LRESULT CALLBACK Server_SubclassTabControlProc (HWND hTab, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc;

   if (procTabControl == 0)
      rc = DefWindowProc (hTab, msg, wp, lp);
   else
      rc = (LRESULT) CallWindowProc ((WNDPROC)procTabControl, hTab, msg, wp, lp);

   switch (msg)
      {
      // Since this is a subclass proc, it's not around when the window
      // is created.  Any WM_CREATE processing we'd ordinarily do here
      // must therefore be done at the point where the tab control is
      // subclassed.
      //
      // case WM_CREATE:

      case WM_SIZE:
         if (lp != 0)
            ResizeWindow (hTab, awdTabChild, rwaFixupGuts);
         break;

      case WM_DESTROY:
         if (procTabControl != 0)
            SetWindowLongPtr (hTab, GWLP_WNDPROC, procTabControl);
         break;
      }

   return rc;
}


void Server_PrepareTabControl (HWND hTab)
{
   TabCtrl_SetImageList (hTab, AfsAppLib_CreateImageList (FALSE));

   TCHAR szText[ cchRESOURCE ];
   TC_ITEM tci;
   tci.mask = TCIF_TEXT | TCIF_PARAM | TCIF_IMAGE;
   tci.pszText = szText;

   for (int iTab = 0; iTab < nCHILDTABS; ++iTab)
      {
      tci.iImage = CHILDTABINFO[ iTab ].iImage;
      tci.lParam = (LPARAM)CHILDTABINFO[ iTab ].tab;
      GetString (tci.pszText, CHILDTABINFO[ iTab ].idsTabTitle);
      TabCtrl_InsertItem (hTab, iTab, &tci);
      }

   // subclass the TCN_ window, so that it will start sending WM_SIZE
   // messages to its child window.
   //
   procTabControl = GetWindowLongPtr (hTab, GWLP_WNDPROC);
   SetWindowLongPtr (hTab, GWLP_WNDPROC, (LONG_PTR)Server_SubclassTabControlProc);

   // Since we just subclassed this control, our new wndproc wasn't around
   // when the window was created.  Any WM_CREATE processing we'd ordinarily
   // in that wndproc must therefore be done here.

   // WM_CREATE:
   RECT rWindow;
   GetRectInParent (hTab, &rWindow);
   ResizeWindow (hTab, awdTabChild, rwaMoveToHere, &rWindow);

   // Finally, select an appropriate tab and display it.
   //
   Server_DisplayTab (GetParent (hTab), gr.tabLast);
}


void Server_DisplayTab (HWND hDlg, CHILDTAB iTab)
{
   HWND hTab = GetDlgItem (hDlg, IDC_TABS);
   int idd = -1;
   DLGPROC dlgproc = NULL;

   switch (iTab)
      {
      case tabSERVICES:
         idd = IDD_SERVICES;
         dlgproc = (DLGPROC)Services_DlgProc;
         break;
      case tabAGGREGATES:
         idd = IDD_AGGREGATES;
         dlgproc = (DLGPROC)Aggregates_DlgProc;
         break;
      case tabFILESETS:
         idd = IDD_FILESETS;
         dlgproc = (DLGPROC)Filesets_DlgProc;
         break;
      }

   if (idd != -1)
      {
      HWND hDialogOld = GetTabChild (hTab);
      HWND hDialogNew = ModelessDialog (idd, hTab, dlgproc);

      if (hDialogNew != NULL)
         {
         TabCtrl_SetCurSel (hTab, iTab);
         ShowWindow (hDialogNew, SW_SHOW);

         if (hDialogOld != NULL)
            {
            DestroyWindow (hDialogOld);
            }

         Server_ForceRedraw (hDlg);
         }
      }
}


void Server_Uncover (HWND hWnd)
{
   if (hWnd == g.hMain) // uncover the preview pane?
      {
      AfsAppLib_Uncover (GetDlgItem (g.hMain, IDC_TABS));
      }
   else // uncover a standalone server window?
      {
      AfsAppLib_Uncover (hWnd);
      }
}


void Server_SaveRect (HWND hDlg, BOOL fOpen)
{
   LPSVR_SETWINDOWPOS_PARAMS lpp = New (SVR_SETWINDOWPOS_PARAMS);
   GetWindowRect (hDlg, &lpp->rWindow);
   lpp->lpi = Server_GetServer (hDlg);
   lpp->fOpen = fOpen;

   StartTask (taskSVR_SETWINDOWPOS, NULL, lpp);
}


BOOL Server_HandleDialogKeys (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (msg == WM_COMMAND)
      {
      switch (LOWORD(wp))
         {
         case M_KEY_RETURN:
            Server_OnKey_Return();
            return TRUE;

         case M_KEY_CTRLTAB:
            Server_OnKey_CtrlTab (hDlg, TRUE);
            return TRUE;

         case M_KEY_CTRLBACKTAB:
            Server_OnKey_CtrlTab (hDlg, FALSE);
            return TRUE;

         case M_KEY_TAB:
            Server_OnKey_Tab (hDlg, TRUE);
            return TRUE;

         case M_KEY_BACKTAB:
            Server_OnKey_Tab (hDlg, FALSE);
            return TRUE;

         case M_KEY_MENU:
            Server_OnKey_Menu();
            return TRUE;

         case M_KEY_ESC:
            Server_OnKey_Esc();
            return TRUE;

         case M_KEY_PROPERTIES:
            Server_OnKey_Properties();
            return TRUE;
         }
      }

   return FALSE;
}


void Server_OnKey_Return (void)
{
   static NMHDR hdr;
   hdr.hwndFrom = GetFocus();
   hdr.idFrom = GetWindowLong (GetFocus(), GWL_ID);
   hdr.code = FLN_LDBLCLICK;
   PostMessage (GetParent (GetFocus()), WM_NOTIFY, 0, (LPARAM)&hdr);
}


void Server_OnKey_Menu (void)
{
   HWND hFocus = GetFocus();
   if (fIsFastList (hFocus))
      {
      POINT ptScreen = { 0, 0 };

      HLISTITEM hItem;
      if ((hItem = FastList_FindFirstSelected (hFocus)) != NULL)
         {
         FASTLISTITEMREGIONS reg;
         FastList_GetItemRegions (hFocus, hItem, &reg);

         ptScreen.x = reg.rItem.left + (reg.rItem.right -reg.rItem.left) /2;
         ptScreen.y = reg.rItem.top + (reg.rItem.bottom -reg.rItem.top) /2;
         ClientToScreen (GetFocus(), &ptScreen);
         }

      PostMessage (GetParent (GetFocus()), WM_CONTEXTMENU, (WPARAM)GetFocus(), MAKELONG(ptScreen.x,ptScreen.y));
      }
}


void Server_OnKey_Esc (void)
{
   HWND hFocus = GetFocus();
   if (fIsFastList (hFocus))
      {
      FastList_SelectNone (hFocus);
      }
}


void Server_OnKey_Properties (void)
{
   HWND hFocus = GetFocus();
   if (fIsFastList (hFocus))
      {
      LPIDENT lpi;
      if ((lpi = (LPIDENT)FL_GetSelectedData (hFocus)) != NULL)
         {
         StartContextCommand (GetParent(hFocus), NULL, lpi, M_PROPERTIES);
         }
      }
}


void Server_OnKey_Tab (HWND hDlg, BOOL fForward)
{
   // The tab-cycle should go:
   //    TabControl <-> TabChildControls
   //
   HWND hFocus = GetFocus();
   HWND hTabChild = GetTabChild (GetDlgItem (hDlg, IDC_TABS));

   if (fForward)
      {
      if (hFocus == GetDlgItem (hDlg, IDC_TABS))
         {
         PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, NULL, FALSE), TRUE);
         }
      else
         {
         if (GetNextDlgTabItem (hTabChild, hFocus, FALSE) == GetNextDlgTabItem (hTabChild, NULL, FALSE))
            PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (hDlg, IDC_TABS), TRUE);
         else
            PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, hFocus, FALSE), TRUE);
         }
      }
   else // (!fForward)
      {
      if (hFocus == GetDlgItem (hDlg, IDC_TABS))
         {
         PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetLastDlgTabItem (hTabChild), TRUE);
         }
      else
         {
         if (hFocus == GetNextDlgTabItem (hTabChild, NULL, FALSE))
            PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (hDlg, IDC_TABS), TRUE);
         else
            PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, hFocus, TRUE), TRUE);
         }
      }
}


void Server_OnKey_CtrlTab (HWND hDlg, BOOL fForward)
{
   HWND hTabs = GetDlgItem (hDlg, IDC_TABS);
   int iTab = TabCtrl_GetCurSel(hTabs);

   if (fForward)
      iTab = (iTab == nCHILDTABS-1) ? (0) : (iTab+1);
   else
      iTab = (iTab == 0) ? (nCHILDTABS-1) : (iTab-1);

   TabCtrl_SetCurSel (hTabs, iTab);
   Server_DisplayTab (hDlg, (CHILDTAB)iTab);
   gr.tabLast = (CHILDTAB)iTab;

   PostMessage (hDlg, WM_NEXTDLGCTL, (WPARAM)hTabs, TRUE);
}

