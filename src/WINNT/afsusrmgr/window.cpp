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
#include "messages.h"
#include "window.h"
#include "usr_tab.h"
#include "grp_tab.h"
#include "mch_tab.h"
#include "command.h"
#include "action.h"
#include "creds.h"


/*
 * TABS _______________________________________________________________________
 *
 */

static struct {
   TABTYPE tt;
   int idiImage;
   int idsText;
   int iddChild;
   DLGPROC procChild;
} aTABS[] = {
   { ttUSERS, imageUSER, IDS_TAB_USERS, IDD_TAB_USERS, (DLGPROC)Users_DlgProc },
   { ttGROUPS, imageGROUP, IDS_TAB_GROUPS, IDD_TAB_GROUPS, (DLGPROC)Groups_DlgProc },
   { ttMACHINES, imageSERVER, IDS_TAB_MACHINES, IDD_TAB_MACHINES, (DLGPROC)Machines_DlgProc },
};

static size_t cTABS = sizeof(aTABS) / sizeof(aTABS[0]);


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cxMIN_TABS    150
#define cyMIN_TABS    100

rwWindowData awdMain[] = {
   { IDC_TAB, raSizeX | raSizeY, 		MAKELONG(cxMIN_TABS,cyMIN_TABS),	0 },
   { IDC_CELL, raSizeX | raRepaint,		0,									0 },
   { IDC_CREDS, raSizeX | raRepaint,	0,									0 },
   { IDC_ANIM, raMoveX,					0,									0 },
   { IDC_BAR, raSizeX,					0,									0 },
   { IDC_STATIC, raRepaint,				0,									0 },
   { idENDLIST, 0,						0,									0 }
};

rwWindowData awdTabChild[] = {
    { 0, raSizeX | raSizeY,								0,	0 },
    { idENDLIST, 0,										0,	0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Main_OnInitDialog (HWND hDlg);
LRESULT CALLBACK Main_TabHookProc (HWND hTab, UINT msg, WPARAM wp, LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Main_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (msg == WM_INITDIALOG)
      {
      g.hMain = hDlg;
      AfsAppLib_SetMainWindow (g.hMain);
      }

   switch (msg)
      {
      case WM_INITDIALOG:
         Main_OnInitDialog (hDlg);
         break;

      case WM_SHOW_YOURSELF:
         if (lp || g.idCell)
            {
            ShowWindow (g.hMain, SW_SHOW);
            SetForegroundWindow (g.hMain);
            Actions_WindowToTop (TRUE);
            }
         break;

      case WM_SHOW_ACTIONS:
         if (gr.fShowActions)
            Actions_OpenWindow();
         break;

      case WM_ACTIVATEAPP:
         Actions_WindowToTop ((wp) ? TRUE : FALSE);
         if (wp)
            StartTask (taskEXPIRED_CREDS);
         break;

      case WM_EXPIRED_CREDENTIALS:
      case WM_REFRESHED_CREDENTIALS:
         g.hCreds = (PVOID)lp;
         StartTask (taskUPD_CREDS);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdMain, rwaFixupGuts);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskUPD_USERS)
               Display_OnEndTask_UpdUsers (ptp);
            else if (ptp->idTask == taskUPD_GROUPS)
               Display_OnEndTask_UpdGroups (ptp);
            else if (ptp->idTask == taskUPD_MACHINES)
               Display_OnEndTask_UpdMachines (ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_ASC_NOTIFY_ACTION:
         Actions_OnNotify (wp, lp);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDCANCEL:
               Quit (0);
               break;

            default:
               OnContextCommand (LOWORD(wp));
               break;
            }
         break;

      case WM_NOTIFY: 
         switch (((LPNMHDR)lp)->code)
            { 
            case TCN_SELCHANGE:
               Main_PrepareTabChild();
               break; 
            }
         break;

      case WM_HELP:
         if ((lp == 0) || (IsAncestor (g.hMain, (HWND)(((LPHELPINFO)lp)->hItemHandle))))
            {
            WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
            }
         break;
      }

   return FALSE;
}


void Main_OnInitDialog (HWND hDlg)
{
   // Resize and reposition the main window
   //
   RECT rWindow = gr.rMain;
   if (!rWindow.right)
      GetWindowRect (hDlg, &rWindow);
   ResizeWindow (hDlg, awdMain, rwaMoveToHere, &rWindow);

   // Add tabs to the main window's tab control
   //
   HWND hTab = GetDlgItem (hDlg, IDC_TAB);
   TabCtrl_SetImageList (hTab, AfsAppLib_CreateImageList (FALSE));

   TCHAR szText[ cchRESOURCE ];
   TC_ITEM tci;
   tci.mask = TCIF_TEXT | TCIF_PARAM | TCIF_IMAGE;
   tci.pszText = szText;

   for (size_t iTab = 0; iTab < cTABS; ++iTab)
      {
      tci.iImage = aTABS[ iTab ].idiImage;
      tci.lParam = (LPARAM)aTABS[ iTab ].tt;
      GetString (tci.pszText, aTABS[ iTab ].idsText);
      TabCtrl_InsertItem (hTab, iTab, &tci);
      }

   // Subclass the tab control, so that we can make it forward any WM_SIZE
   // messages to its child window.
   //
   Subclass_AddHook (hTab, Main_TabHookProc);

   // Since we just subclassed this control, our new wndproc wasn't around
   // when the window was created.  Any WM_CREATE processing we'd ordinarily
   // in that wndproc must therefore be done here.

   // WM_CREATE:
   RECT rTab;
   GetRectInParent (hTab, &rTab);
   ResizeWindow (hTab, awdTabChild, rwaMoveToHere, &rTab);

   // Select an appropriate tab and display it.
   //
   Main_PrepareTabChild (gr.iTabLast);

   // Tell the admin client that we're interested in any actions that occur
   //
   ULONG status;
   asc_ActionListen (g.idClient, g.hMain, &status);
}


LRESULT CALLBACK Main_TabHookProc (HWND hTab, UINT msg, WPARAM wp, LPARAM lp)
{
   LRESULT rc = CallWindowProc ((WNDPROC)Subclass_FindNextHook (hTab, Main_TabHookProc), hTab, msg, wp, lp);

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
         Subclass_RemoveHook (hTab, Main_TabHookProc);
         break;
      }

   return rc;
}


void Main_PrepareTabChild (int iTabNew)
{
   HWND hTab = GetDlgItem (g.hMain, IDC_TAB);
   int iTabOld = TabCtrl_GetCurSel (hTab);
   if (iTabNew == -1)
      iTabNew = iTabOld;

   HWND hDlg;
   if ((hDlg = GetTabChild (hTab)) != NULL)
      DestroyWindow (hDlg);
   if ((hDlg = ModelessDialog (aTABS[ iTabNew ].iddChild, hTab, aTABS[ iTabNew ].procChild)) != NULL)
      {
      if (iTabNew != iTabOld)
         TabCtrl_SetCurSel (hTab, iTabNew);
      ShowWindow (hDlg, SW_SHOW);
      }

   // Remember this tab as the last one the user saw, so that if the app gets
   // closed we can re-open it to the same tab.
   //
   gr.iTabLast = iTabNew;

   // Check/uncheck/enable/disable our dialog's menu items as appropriate.
   // We do this whenever the current tab changes, because some of the
   // menu items will change state depending on which tab is visible.
   //
   Main_SetMenus();
}


void Main_SetMenus (void)
{
   HMENU hm = GetMenu (g.hMain);
   LPASIDLIST pSel = Display_GetSelectedList();

   Main_SetViewMenus (hm);

   // Fix the Operations In Progress entry
   //
   CheckMenu (hm, M_ACTIONS, gr.fShowActions);

   // Fix the CellProperties and Edit.* menu items. Many get disabled if there
   // is no selection
   //
   BOOL fEnable = (pSel && pSel->cEntries) ? TRUE : FALSE;
   EnableMenu (hm, M_DELETE, fEnable);
   EnableMenu (hm, M_REFRESH, fEnable);
   EnableMenu (hm, M_MEMBERSHIP, fEnable);
   EnableMenu (hm, M_PROPERTIES, fEnable);

   if ((fEnable = (pSel && (pSel->cEntries == 1))) == TRUE)
      {
      // Make sure it's a group
      ASOBJTYPE Type;
      if (!asc_ObjectTypeGet_Fast (g.idClient, g.idCell, pSel->aEntries[0].idObject, &Type))
         fEnable = FALSE;
      else if (Type != TYPE_GROUP)
         fEnable = FALSE;
      }

   EnableMenu (hm, M_RENAME, fEnable);

   if (pSel)
      asc_AsidListFree (&pSel);
}


void Main_SetViewMenus (HMENU hm)
{
   // Which tab is currently being displayed?
   //
   HWND hTab = GetDlgItem (g.hMain, IDC_TAB);
   int iTab = TabCtrl_GetCurSel (hTab);
   TABTYPE tt = aTABS[ iTab ].tt;

   // Fix the Icons.Large/Small/Details entries
   //
   LPVIEWINFO pvi = (tt == ttUSERS) ? &gr.viewUsr : (tt == ttMACHINES) ? &gr.viewMch : &gr.viewGrp;
   ICONVIEW iv = (tt == ttUSERS) ? gr.ivUsr : (tt == ttMACHINES) ? gr.ivMch : gr.ivGrp;

   int idm = ((pvi->lvsView & FLS_VIEW_MASK) == FLS_VIEW_LARGE) ? M_VIEW_LARGE : ((pvi->lvsView & FLS_VIEW_MASK) == FLS_VIEW_SMALL) ? M_VIEW_SMALL : M_VIEW_DETAILS;
   CheckMenuRadioItem (hm, M_VIEW_LARGE, M_VIEW_DETAILS, idm, MF_BYCOMMAND);

   // Fix the Icons.One/Two/Status entries
   //
   BOOL fEnable = ((pvi->lvsView & FLS_VIEW_MASK) == FLS_VIEW_LIST) ? TRUE : FALSE;
   EnableMenu (hm, M_VIEW_ONE, fEnable);
   EnableMenu (hm, M_VIEW_TWO, fEnable);
   EnableMenu (hm, M_VIEW_STATUS, fEnable);

   idm = (iv == ivTWOICONS) ? M_VIEW_TWO : (iv == ivONEICON) ? M_VIEW_ONE : M_VIEW_STATUS;
   CheckMenuRadioItem (hm, M_VIEW_ONE, M_VIEW_STATUS, idm, MF_BYCOMMAND);
}

