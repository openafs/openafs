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
#include "grp_tab.h"
#include "grp_col.h"
#include "command.h"
#include "window.h"


/*
 * RESIZING WINDOWS ___________________________________________________________
 *
 */

rwWindowData awdGroupsTab[] = {
    { IDC_GROUPS_TITLE, raRepaint | raSizeX,		0,	0 },
    { IDC_ADVANCED, raMoveX,				0,	0 },
    { IDC_GROUPS_PATTERN, raMoveX,			0,	0 },
    { IDC_GROUPS_PATTERN_PROMPT, raRepaint | raMoveX,	0,	0 },
    { IDC_GROUPS_LIST, raSizeX | raSizeY,		0,	0 },
    { M_GROUP_CREATE, raMoveX | raMoveY,		0,	0 },
    { M_MEMBERSHIP, raMoveX | raMoveY,			0,	0 },
    { M_PROPERTIES, raMoveX | raMoveY,			0,	0 },
    { IDC_STATIC, raRepaint,				0,	0 },
    { idENDLIST, 0,					0,	0 }
 };


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

void Groups_EnableButtons (HWND hDlg);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL CALLBACK Groups_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (Display_HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewGrp))
      return FALSE;

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         RECT rTab;
         GetClientRect (GetParent(hDlg), &rTab);
         TabCtrl_AdjustRect (GetParent (hDlg), FALSE, &rTab); 
         ResizeWindow (hDlg, awdGroupsTab, rwaMoveToHere, &rTab);

         HIMAGELIST hSmall = AfsAppLib_CreateImageList (FALSE);
         HIMAGELIST hLarge = AfsAppLib_CreateImageList (TRUE);
         FastList_SetImageLists (GetDlgItem (hDlg, IDC_GROUPS_LIST), hSmall, hLarge);

         FastList_SetSortFunction (GetDlgItem (hDlg, IDC_GROUPS_LIST), General_ListSortFunction);

         FL_RestoreView (GetDlgItem (hDlg, IDC_GROUPS_LIST), &gr.viewGrp);
         FastList_SetTextCallback (GetDlgItem (hDlg, IDC_GROUPS_LIST), Display_GetItemText, &gr.viewGrp);
         SetDlgItemText (hDlg, IDC_GROUPS_PATTERN, g.szPatternGroups);
         Groups_EnableButtons(hDlg);
         Display_PopulateGroupList();

         l.dwTickLastType = 0;
         }
         break;

      case WM_HELP:
         WinHelp (hDlg, cszHELPFILENAME, HELP_FINDER, 0);
         break;

      case WM_DESTROY:
         FL_StoreView (GetDlgItem (hDlg, IDC_GROUPS_LIST), &gr.viewGrp);
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            ResizeWindow (hDlg, awdGroupsTab, rwaFixupGuts);
         break;

      case WM_TIMER:
         switch (wp)
            {
            case ID_SEARCH_TIMER:
               if ( (l.dwTickLastType) && (GetTickCount() > l.dwTickLastType + msecSEARCH_TIMER) )
                  {
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  Display_PopulateGroupList();
                  }
               break;
            }
         break;

      case WM_CONTEXTMENU:
         POINT ptScreen;
         ptScreen.x = LOWORD(lp);
         ptScreen.y = HIWORD(lp);
         OnRightClick (pmGROUP, GetDlgItem (hDlg, IDC_GROUPS_LIST), &ptScreen);
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDC_GROUPS_PATTERN:
               if (HIWORD(wp) == EN_UPDATE)
                  {
                  l.dwTickLastType = GetTickCount();
                  KillTimer (hDlg, ID_SEARCH_TIMER);
                  SetTimer (hDlg, ID_SEARCH_TIMER, msecSEARCH_TIMER +15, NULL);
                  }
               break;

            default:
               OnContextCommand (LOWORD(wp));
               break;
            }
         break;

      case WM_NOTIFY:
         switch (((LPNMHDR)lp)->code)
            {
            case FLN_ITEMSELECT:
               Main_SetMenus();
               Groups_EnableButtons(hDlg);
               break;

            case FLN_LDBLCLICK:
               PostMessage (hDlg, WM_COMMAND, MAKELONG(M_PROPERTIES,BN_CLICKED), (LPARAM)GetDlgItem (hDlg, M_PROPERTIES));
               break;
            }
         break;
      }

   return FALSE;
}


void Groups_EnableButtons (HWND hDlg)
{
   BOOL fEnable = (Display_GetSelectedCount() != 0) ? TRUE : FALSE;
   EnableWindow (GetDlgItem (hDlg, M_PROPERTIES), fEnable);
   EnableWindow (GetDlgItem (hDlg, M_MEMBERSHIP), fEnable);
}

