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
#include "command.h"
#include "window.h"
#include "creds.h"
#include "action.h"
#include "columns.h"
#include "cell_prop.h"
#include "options.h"
#include "usr_prop.h"
#include "usr_cpw.h"
#include "usr_create.h"
#include "usr_delete.h"
#include "grp_prop.h"
#include "grp_rename.h"
#include "grp_create.h"
#include "grp_delete.h"
#include "mch_create.h"
#include "mch_delete.h"


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void ShowContextMenu (POPUPMENU pm, HWND hList, LPASIDLIST pAsidList, POINT *pptScreen);

void Command_OnView (WORD wCmd);
void Command_OnShowActions (void);
void Command_OnRefresh (void);
void Command_OnUnlock (void);
void Command_OnProperties (void);
void Command_OnMembership (void);
void Command_OnChangePassword (void);
void Command_OnRename (void);
void Command_OnCreateUser (void);
void Command_OnCreateGroup (void);
void Command_OnCreateMachine (void);
void Command_OnDelete (void);

void Command_OnKey_CtrlTab (BOOL fForward);
void Command_OnKey_Tab (BOOL fForward);
void Command_OnKey_Return (void);
void Command_OnKey_Menu (void);
void Command_OnKey_Esc (void);
void Command_OnKey_Properties (void);


/*
 * POPUP-MENU SUPPORT _________________________________________________________
 *
 */

void OnRightClick (POPUPMENU pm, HWND hList, POINT *pptScreen)
{
   if (IsWindow (hList))
      {
      // Which items are selected in the given list?  (Note: if we're
      // responding to a WM_CONTEXTMENU, then the act of clicking the
      // mouse button has already changed the selection as appropriate.)
      //
      LPASIDLIST pAsidList;
      if ((pAsidList = Display_GetSelectedList()) != NULL)
         {
         if (pAsidList->cEntries == 0)
            {
            asc_AsidListFree (&pAsidList);
            pAsidList = NULL;
            }
         }

      // If we don't have screen coordinates, the user must have clicked
      // the Menu key on the keyboard. Pick the center of the first
      // selected item's region (or the center of the list if none),
      // and display the appropriate menu.
      //
      if (!pptScreen)
         {
         RECT rWindow;
         GetWindowRect (hList, &rWindow);

         HLISTITEM hItemSelected = NULL;
         if (pAsidList)
            hItemSelected = FastList_FindItem (hList, (LPARAM)(pAsidList->aEntries[0].idObject));

         POINT ptScreen;
         if (!hItemSelected)
            {
            ptScreen.x = rWindow.left + (rWindow.right - rWindow.left)/2;
            ptScreen.y = rWindow.top + (rWindow.bottom - rWindow.top)/2;
            }
         else
            {
            FASTLISTITEMREGIONS Reg;
            FastList_GetItemRegions (hList, hItemSelected, &Reg);
            ptScreen.x = rWindow.left + Reg.rLabel.left + (Reg.rLabel.right - Reg.rLabel.left)/2;
            ptScreen.y = rWindow.top + Reg.rLabel.top + (Reg.rLabel.bottom - Reg.rLabel.top)/2;
            ptScreen.x = min (max (ptScreen.x, rWindow.left), rWindow.right);
            ptScreen.y = min (max (ptScreen.y, rWindow.top), rWindow.bottom);
            }

         ShowContextMenu (pm, hList, pAsidList, &ptScreen);
         }
      else
         {
         POINT ptClient = *pptScreen;
         ScreenToClient (hList, &ptClient);

         // Did the user click on the header bar for the list?
         //
         if (FL_HitTestForHeaderBar (hList, ptClient))
            {
            HMENU hm = TaLocale_LoadMenu (MENU_COLUMNS);
            DisplayContextMenu (hm, *pptScreen, GetParent(hList));
            }
         else
            {
            ShowContextMenu (pm, hList, pAsidList, pptScreen);
            }
         }

      if (pAsidList)
         asc_AsidListFree (&pAsidList);
      }
}


void ShowContextMenu (POPUPMENU pm, HWND hList, LPASIDLIST pAsidList, POINT *pptScreen)
{
   HMENU hm = NULL;

   switch (pm)
      {
      case pmUSER:
         if (pAsidList)
            {
            hm = TaLocale_LoadMenu (MENU_USER);
            if (pAsidList->cEntries > 1)
               EnableMenu (hm, M_CPW, FALSE);
            }
         else // (!pAsidList)
            {
            hm = TaLocale_LoadMenu (MENU_USER_NONE);
            Main_SetViewMenus (hm);
            }
         break;

      case pmGROUP:
         if (pAsidList)
            {
            hm = TaLocale_LoadMenu (MENU_GROUP);
            if (pAsidList->cEntries > 1)
               EnableMenu (hm, M_RENAME, FALSE);
            }
         else // (!pAsidList)
            {
            hm = TaLocale_LoadMenu (MENU_GROUP_NONE);
            Main_SetViewMenus (hm);
            }
         break;

      case pmMACHINE:
         if (pAsidList)
            {
            hm = TaLocale_LoadMenu (MENU_MACHINE);
            }
         else // (!pAsidList)
            {
            hm = TaLocale_LoadMenu (MENU_MACHINE_NONE);
            Main_SetViewMenus (hm);
            }
         break;
      }

   if (hm)
      {
      DisplayContextMenu (hm, *pptScreen, GetParent(hList));
      }
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void OnContextCommand (WORD wCmd)
{
   switch (wCmd)
      {
      // Menu commands
      //
      case M_OPENCELL:
         (void)OpenCellDialog();
         break;

      case M_CREDENTIALS:
         (void)NewCredsDialog();
         break;

      case M_EXIT:
         Quit (0);
         break;

      case M_VIEW_LARGE:
      case M_VIEW_SMALL:
      case M_VIEW_DETAILS:
      case M_VIEW_ONE:
      case M_VIEW_TWO:
      case M_VIEW_STATUS:
         Command_OnView (wCmd);
         break;

      case M_ACTIONS:
         Command_OnShowActions();
         break;

      case M_COLUMNS:
         ShowColumnsDialog (g.hMain);
         break;

      case M_OPTIONS:
         ShowOptionsDialog (g.hMain);
         break;

      case M_USER_CREATE:
         Command_OnCreateUser();
         break;

      case M_GROUP_CREATE:
         Command_OnCreateGroup();
         break;

      case M_MACHINE_CREATE:
         Command_OnCreateMachine();
         break;

      case M_RENAME:
         Command_OnRename();
         break;

      case M_DELETE:
         Command_OnDelete();
         break;

      case M_CPW:
         Command_OnChangePassword();
         break;

      case M_SELECTALL:
         Display_SelectAll();
         break;

      case M_REFRESH:
         Command_OnRefresh();
         break;

      case M_MEMBERSHIP:
         Command_OnMembership();
         break;

      case M_UNLOCK:
         Command_OnUnlock();
         break;

      case M_PROPERTIES:
         Command_OnProperties();
         break;

      case M_CELL_PROPERTIES:
         Cell_ShowProperties();
         break;

      case M_REFRESHALL:
         StartTask (taskREFRESH, NULL, (PVOID)g.idCell);
         break;

      case M_CONTENTS:
         WinHelp (g.hMain, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case M_FIND:
         Help_FindCommand();
         break;

      case M_LOOKUP:
         Help_FindError();
         break;

      case M_ABOUT:
         Help_About();
         break;

      // Keyboard handlers (we use accelerator table entries to catch
      // tab/return/esc/etc, and mimic a normal dialog box's behavior)
      //
      case M_KEY_RETURN:
         Command_OnKey_Return();
         break;

      case M_KEY_CTRLTAB:
         Command_OnKey_CtrlTab (TRUE);
         break;

      case M_KEY_CTRLBACKTAB:
         Command_OnKey_CtrlTab (FALSE);
         break;

      case M_KEY_TAB:
         Command_OnKey_Tab (TRUE);
         break;

      case M_KEY_BACKTAB:
         Command_OnKey_Tab (FALSE);
         break;

      case M_KEY_MENU:
         Command_OnKey_Menu();
         break;

      case M_KEY_ESC:
         Command_OnKey_Esc();
         break;

      case M_KEY_PROPERTIES:
         Command_OnKey_Properties();
         break;
      }
}


/*
 * COMMAND HANDLERS _______________________________________________________________________________
 *
 */

void Command_OnView (WORD wCmd)
{
   // Which tab is currently being displayed?
   //
   HWND hTab = GetDlgItem (g.hMain, IDC_TAB);
   int iTab = TabCtrl_GetCurSel (hTab);

   // Find the appropriate current view settings for this tab
   //
   LPVIEWINFO pvi = (iTab == 0) ? &gr.viewUsr : (iTab == 1) ? &gr.viewGrp : &gr.viewMch;
   ICONVIEW iv = (iTab == 0) ? gr.ivUsr : (iTab == 1) ? gr.ivGrp : gr.ivMch;

   // ...and prepare modified versions of them
   //
   VIEWINFO viNew = *pvi;
   ICONVIEW ivNew = iv;

   switch (wCmd)
      {
      case M_VIEW_LARGE:
         viNew.lvsView = (viNew.lvsView & (~FLS_VIEW_MASK)) | FLS_VIEW_LARGE;
         break;
      case M_VIEW_SMALL:
         viNew.lvsView = (viNew.lvsView & (~FLS_VIEW_MASK)) | FLS_VIEW_SMALL;
         break;
      case M_VIEW_DETAILS:
         viNew.lvsView = (viNew.lvsView & (~FLS_VIEW_MASK)) | FLS_VIEW_LIST;
         break;
      case M_VIEW_TWO:
         ivNew = ivTWOICONS;
         break;
      case M_VIEW_ONE:
         ivNew = ivONEICON;
         break;
      case M_VIEW_STATUS:
         ivNew = ivSTATUS;
         break;
      }

   if ((viNew.lvsView & FLS_VIEW_MASK) != FLS_VIEW_LIST)
      {
      viNew.iSort = 0; // sort alphabetically by name
      }

   // Apply the changes, and update the main window's menu items
   //
   Display_RefreshView (&viNew, ivNew);
   Main_SetMenus ();
}


void Command_OnShowActions (void)
{
   if ((gr.fShowActions = !gr.fShowActions) == TRUE)
      Actions_OpenWindow();
   else
      Actions_CloseWindow();
   Main_SetMenus();
}


void Command_OnRefresh (void)
{
   LPASIDLIST pAsidList;
   if ((pAsidList = Display_GetSelectedList()) != NULL)
      {
      StartTask (taskREFRESHMULT, NULL, pAsidList);
      }
}


void Command_OnUnlock (void)
{
   LPASIDLIST pAsidList;
   if ((pAsidList = Display_GetSelectedList()) != NULL)
      {
      StartTask (taskUSER_UNLOCK, NULL, pAsidList);
      }
}


void Command_OnProperties (void)
{
   LPASIDLIST pAsidList;
   if ((pAsidList = Display_GetSelectedList()) != NULL)
      {
      // See what kind of objects are selected
      //
      BOOL fAnyUsers = FALSE;
      BOOL fAnyGroups = FALSE;

      for (size_t ii = 0; ii < pAsidList->cEntries; ++ii)
         {
         ASOBJTYPE Type;
         if (!asc_ObjectTypeGet_Fast (g.idClient, g.idCell, pAsidList->aEntries[ ii ].idObject, &Type))
            continue;
         switch (Type)
            {
            case TYPE_USER:
               fAnyUsers = TRUE;
               break;
            case TYPE_GROUP:
               fAnyGroups = TRUE;
               break;
            }
         }

      // If they're homogenous, display the appropriate properties dialog
      //
      if (fAnyUsers && !fAnyGroups)
         User_ShowProperties (pAsidList);
      else if (fAnyGroups && !fAnyUsers)
         Group_ShowProperties (pAsidList);
      else
         asc_AsidListFree (&pAsidList);
      }
}


void Command_OnMembership (void)
{
   LPASIDLIST pAsidList;
   if ((pAsidList = Display_GetSelectedList()) != NULL)
      {
      // See what kind of objects are selected
      //
      BOOL fAnyUsers = FALSE;
      BOOL fAnyGroups = FALSE;

      for (size_t ii = 0; ii < pAsidList->cEntries; ++ii)
         {
         ASOBJTYPE Type;
         if (!asc_ObjectTypeGet_Fast (g.idClient, g.idCell, pAsidList->aEntries[ ii ].idObject, &Type))
            continue;
         switch (Type)
            {
            case TYPE_USER:
               fAnyUsers = TRUE;
               break;
            case TYPE_GROUP:
               fAnyGroups = TRUE;
               break;
            }
         }

      // If they're homogenous, display the appropriate membership dialog
      //
      if (fAnyUsers && !fAnyGroups)
         User_ShowProperties (pAsidList, uptMEMBERSHIP);
      else if (fAnyGroups && !fAnyUsers)
         Group_ShowProperties (pAsidList, gptMEMBERS);
      else
         asc_AsidListFree (&pAsidList);
      }
}


void Command_OnChangePassword (void)
{
   LPASIDLIST pAsidList;
   if ((pAsidList = Display_GetSelectedList()) != NULL)
      {
      if (pAsidList->cEntries == 1)
         {
         ASOBJTYPE Type;
         if (asc_ObjectTypeGet_Fast (g.idClient, g.idCell, pAsidList->aEntries[0].idObject, &Type))
            {
            if (Type == TYPE_USER)
               User_ShowChangePassword (g.hMain, pAsidList->aEntries[0].idObject);
            }
         }
      }
}


void Command_OnRename (void)
{
   LPASIDLIST pAsidList;
   if ((pAsidList = Display_GetSelectedList()) != NULL)
      {
      if (pAsidList->cEntries == 1)
         {
         ASOBJTYPE Type;
         if (asc_ObjectTypeGet_Fast (g.idClient, g.idCell, pAsidList->aEntries[0].idObject, &Type))
            {
            if (Type == TYPE_GROUP)
               Group_ShowRename (g.hMain, pAsidList->aEntries[0].idObject);
            }
         }
      }
}


void Command_OnCreateUser (void)
{
   User_ShowCreate (g.hMain);
}


void Command_OnCreateGroup (void)
{
   Group_ShowCreate (g.hMain);
}


void Command_OnCreateMachine (void)
{
   Machine_ShowCreate (g.hMain);
}


void Command_OnDelete (void)
{
   LPASIDLIST pAsidList;
   if ((pAsidList = Display_GetSelectedList()) != NULL)
      {
      // See what kind of objects are selected
      //
      BOOL fAnyUsers = FALSE;
      BOOL fAnyGroups = FALSE;
      BOOL fAnyMachines = FALSE;

      for (size_t ii = 0; ii < pAsidList->cEntries; ++ii)
         {
         ASOBJTYPE Type;
         if (!asc_ObjectTypeGet_Fast (g.idClient, g.idCell, pAsidList->aEntries[ ii ].idObject, &Type))
            continue;
         switch (Type)
            {
            case TYPE_USER:
               if (fIsMachineAccount (pAsidList->aEntries[ ii ].idObject))
                  fAnyMachines = TRUE;
               else
                  fAnyUsers = TRUE;
               break;
            case TYPE_GROUP:
               fAnyGroups = TRUE;
               break;
            }
         }

      // If they're homogenous, display a matching delete confirmation dialog
      //
      if (fAnyUsers && !fAnyGroups && !fAnyMachines)
         User_ShowDelete (pAsidList);
      else if (fAnyGroups && !fAnyUsers && !fAnyMachines)
         Group_ShowDelete (pAsidList);
      else if (fAnyMachines && !fAnyUsers && !fAnyGroups)
         Machine_ShowDelete (pAsidList);
      else
         asc_AsidListFree (&pAsidList);
      }
}


/*
 * KEYBOARD HANDLERS ______________________________________________________________________________
 *
 */

void Command_OnKey_Tab (BOOL fForward)
{
   // The tab-cycle should go:
   //    TabControl <-> TabChildControls
   //
   HWND hFocus = GetFocus();
   HWND hTabChild = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));

   if (fForward)
      {
      if (hFocus == GetDlgItem (g.hMain, IDC_TAB))
         {
         PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, NULL, FALSE), TRUE);
         }
      else
         {
         if (GetNextDlgTabItem (hTabChild, hFocus, FALSE) == GetNextDlgTabItem (hTabChild, NULL, FALSE))
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (g.hMain, IDC_TAB), TRUE);
         else
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, hFocus, FALSE), TRUE);
         }
      }
   else // (!fForward)
      {
      if (hFocus == GetDlgItem (g.hMain, IDC_TAB))
         {
         PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetLastDlgTabItem (hTabChild), TRUE);
         }
      else
         {
         if (hFocus == GetNextDlgTabItem (hTabChild, NULL, FALSE))
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetDlgItem (g.hMain, IDC_TAB), TRUE);
         else
            PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)GetNextDlgTabItem (hTabChild, hFocus, TRUE), TRUE);
         }
      }
}


void Command_OnKey_CtrlTab (BOOL fForward)
{
   HWND hTabs = GetDlgItem (g.hMain, IDC_TAB);
   size_t iTab = TabCtrl_GetCurSel (hTabs);
   size_t cTabs = TabCtrl_GetItemCount (hTabs);

   if (fForward)
      iTab = (iTab == cTabs-1) ? (0) : (iTab+1);
   else
      iTab = (iTab == 0) ? (cTabs-1) : (iTab-1);

   Main_PrepareTabChild (iTab);

   PostMessage (g.hMain, WM_NEXTDLGCTL, (WPARAM)hTabs, TRUE);
}


void Command_OnKey_Return (void)
{
   static NMHDR hdr;
   hdr.hwndFrom = GetFocus();
   hdr.idFrom = GetWindowLong (GetFocus(), GWL_ID);
   hdr.code = FLN_LDBLCLICK;
   PostMessage (GetParent (GetFocus()), WM_NOTIFY, 0, (LPARAM)&hdr);
}


void Command_OnKey_Menu (void)
{
   HWND hFocus = GetFocus();
   if (fIsFastList (hFocus))
      {
      // Is the user or group tab showing?
      //
      HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
      HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
      POPUPMENU pm = pmGROUP;
      if (!IsWindow (hList))
         {
         hList = GetDlgItem (hDlg, IDC_USERS_LIST);
         pm = pmUSER;
         }
      if (!IsWindow (hList))
         {
         hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
         pm = pmMACHINE;
         }

      if (IsWindow (hList))
         {
         // Pretend the user right-clicked
         //
         OnRightClick (pm, hList);
         }
      }
}


void Command_OnKey_Esc (void)
{
   HWND hFocus = GetFocus();
   if (fIsFastList (hFocus))
      {
      FastList_SelectNone (hFocus);
      }
}


void Command_OnKey_Properties (void)
{
   OnContextCommand (M_PROPERTIES);
}

