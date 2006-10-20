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
#include "action.h"
#include "window.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ID_ACTION_TIMER  100

typedef enum
   {
   actcolOPERATION,
   actcolELAPSED,
   } ACTIONCOLUMN;

static struct
   {
   int idsColumn;
   int cxWidth;
   }
ACTIONCOLUMNS[] =
   {
      { IDS_ACTCOL_OPERATION, 100 }, // actcolOPERATIONS
      { IDS_ACTCOL_ELAPSED,   100 }, // actcolELAPSED
   };

#define nACTIONCOLUMNS  (sizeof(ACTIONCOLUMNS)/sizeof(ACTIONCOLUMNS[0]))

#define cxMIN_ACTION  75
#define cyMIN_ACTION  50


/*
 * VARIABLES __________________________________________________________________
 *
 */

static struct
   {
   HWND hAction;
   LPASACTIONLIST pActionList;
   } l;

rwWindowData awdActions[] = {
    { IDC_ACTION_DESC, raSizeX | raRepaint,					0,	0 },
    { IDC_ACTION_LIST, raSizeX | raSizeY, MAKELONG(cxMIN_ACTION,cyMIN_ACTION),	0 },
    { idENDLIST, 0,								0,	0 }
 };


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void TicksToElapsedTime (LPSYSTEMTIME pst, DWORD dwTicks);
LPTSTR GetActionDescription (LPASACTION pAction);

BOOL CALLBACK Actions_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
void Actions_OnEndTask_GetActions (LPTASKPACKET ptp);
void Actions_Refresh (void);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void TicksToElapsedTime (LPSYSTEMTIME pst, DWORD dwTicks)
{
#define msecSECOND  (1000L)
#define msecMINUTE  (1000L * 60L)
#define msecHOUR    (1000L * 60L * 60L)
#define msecDAY     (1000L * 60L * 60L * 24L)

   memset (pst, 0x00, sizeof(SYSTEMTIME));

   pst->wDay = (int)( dwTicks / msecDAY );
   dwTicks %= msecDAY;
   pst->wHour = (int)( dwTicks / msecHOUR );
   dwTicks %= msecHOUR;
   pst->wMinute = (int)( dwTicks / msecMINUTE );
   dwTicks %= msecMINUTE;
   pst->wSecond = (int)( dwTicks / msecSECOND );
   dwTicks %= msecSECOND;
   pst->wMilliseconds = (int)( dwTicks );
}


void FixActionTime (DWORD *pcsec)
{
   DWORD dwTickNow = GetTickCount();
   DWORD cTickActive = 1000L * (*pcsec);
   (*pcsec) = dwTickNow - cTickActive;
}


void Actions_SetDefaultView (LPVIEWINFO lpvi)
{
   lpvi->lvsView = FLS_VIEW_LIST;
   lpvi->nColsAvail = nACTIONCOLUMNS;

   for (size_t iCol = 0; iCol < nACTIONCOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = ACTIONCOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = ACTIONCOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = actcolELAPSED | COLUMN_SORTREV;

   lpvi->nColsShown = 2;
   lpvi->aColumns[0] = (int)actcolOPERATION;
   lpvi->aColumns[1] = (int)actcolELAPSED;
}


void Actions_OpenWindow (void)
{
   if (!IsWindow (l.hAction))
      {
      l.hAction = ModelessDialog (IDD_ACTIONS, NULL, (DLGPROC)Actions_DlgProc);
      ShowWindow (l.hAction, SW_SHOW);
      Actions_WindowToTop (TRUE);
      }
}


void Actions_CloseWindow (void)
{
   if (IsWindow (l.hAction))
      {
      DestroyWindow (l.hAction);
      l.hAction = NULL;
      }
}


void Actions_WindowToTop (BOOL fTop)
{
   if (IsWindow(l.hAction))
      {
      if (fTop)
         {
         SetWindowPos (l.hAction, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
         }
      else //(!fTop)
         {
         SetWindowPos (l.hAction, g.hMain, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
         }
      }
}


BOOL CALLBACK Actions_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   if (Display_HandleColumnNotify (hDlg, msg, wp, lp, &gr.viewAct))
      return FALSE;

   if (msg == WM_INITDIALOG)
      l.hAction = hDlg;

   switch (msg)
      {
      case WM_INITDIALOG:
         {
         HWND hList = GetDlgItem (hDlg, IDC_ACTION_LIST);
         FL_RestoreView (hList, &gr.viewAct);

         if (gr.rActions.right == 0)
            GetWindowRect (hDlg, &gr.rActions);
         ResizeWindow (hDlg, awdActions, rwaMoveToHere, &gr.rActions);

         SetTimer (hDlg, ID_ACTION_TIMER, 1000, NULL);  // timer message every sec

         StartTask (taskGET_ACTIONS, l.hAction);
         gr.fShowActions = TRUE;
         break;
         }

      case WM_DESTROY:
         gr.fShowActions = FALSE;
         l.hAction = NULL;
         Main_SetMenus();
         KillTimer (hDlg, ID_ACTION_TIMER);

         if (l.pActionList)
            asc_ActionListFree (&l.pActionList);
         break;

      case WM_TIMER:
         if ((FastList_GetItemCount (GetDlgItem (hDlg, IDC_ACTION_LIST))) || (l.pActionList && l.pActionList->cEntries))
            Actions_Refresh();
         break;

      case WM_SIZE:
         // if (lp==0), we're minimizing--don't call ResizeWindow().
         //
         if (lp != 0)
            {
            ResizeWindow (hDlg, awdActions, rwaFixupGuts);
            GetWindowRect (hDlg, &gr.rActions);
            }
         break;

      case WM_MOVE:
         GetWindowRect (hDlg, &gr.rActions);
         break;

      case WM_ENDTASK:
         LPTASKPACKET ptp;
         if ((ptp = (LPTASKPACKET)lp) != NULL)
            {
            if (ptp->idTask == taskGET_ACTIONS)
               Actions_OnEndTask_GetActions (ptp);
            FreeTaskPacket (ptp);
            }
         break;

      case WM_COMMAND:
         switch (LOWORD(wp))
            {
            case IDOK:
            case IDCANCEL:
               DestroyWindow (hDlg);
               break;
            }
         break;
      }

   return FALSE;
}


void Actions_OnNotify (WPARAM wp, LPARAM lp)
{
   LPASACTION pAction = (LPASACTION)lp;
   BOOL fFinished = (BOOL)wp;

   if (pAction)
      {
      // We've just been told something happened;
      //
      switch (pAction->Action)
         {
         case ACTION_REFRESH:
            // If we get a Finished Refreshing notification, it's a safe bet
            // that the admin server has done a significant refresh to its
            // cache; so, we'll use that as a trigger to repopulate the lists
            // on the main dialog.
            //
            if (fFinished)
               Display_PopulateList();
            break;
         }

      // If the Actions window is being displayed, use this notification
      // to update our stored list-of-actions.
      //
      if (IsWindow (l.hAction))
         {
         if (!fFinished)
            FixActionTime (&pAction->csecActive);

         if (!l.pActionList)
            asc_ActionListCreate (&l.pActionList);

         if (l.pActionList)
            {
            if (fFinished)
               asc_ActionListRemoveEntry (&l.pActionList, pAction->idAction);
            else if (!asc_ActionListTest (&l.pActionList, pAction->idAction))
               asc_ActionListAddEntry (&l.pActionList, pAction);
            }

         Actions_Refresh();
         }

      Delete (pAction);
      }
}


void Actions_OnEndTask_GetActions (LPTASKPACKET ptp)
{
   if (l.pActionList)
      {
      asc_ActionListFree (&l.pActionList);
      }
   if (ptp->rc && TASKDATA(ptp)->pActionList)
      {
      l.pActionList = TASKDATA(ptp)->pActionList;
      TASKDATA(ptp)->pActionList = NULL; // don't let FreeTaskPacket free this

      // Zip through the listed actions and change the reported csec-elapsed
      // into an estimated starting tick.
      //
      for (size_t ii = 0; ii < l.pActionList->cEntries; ++ii)
         {
         FixActionTime (&l.pActionList->aEntries[ ii ].Action.csecActive);
         }
      }
   Actions_Refresh();
}


void Actions_Refresh (void)
{
   size_t nItems = 0;
   TCHAR szText[ cchRESOURCE ];

   HWND hList;
   if ((hList = GetDlgItem (l.hAction, IDC_ACTION_LIST)) != NULL)
      {
      LPARAM lpOld = FL_StartChange (hList, TRUE);

      if (l.pActionList)
         {
         for (size_t ii = 0; ii < l.pActionList->cEntries; ++ii)
            {
            LPTSTR pszDesc;
            if ((pszDesc = GetActionDescription (&l.pActionList->aEntries[ ii ].Action)) != NULL)
               {
               SYSTEMTIME st;
               TicksToElapsedTime (&st, GetTickCount() - l.pActionList->aEntries[ ii ].Action.csecActive);
               FormatElapsed (szText, TEXT("%s"), &st);

               FASTLISTADDITEM flai;
               memset (&flai, 0x00, sizeof(flai));
               flai.hParent = NULL;
               flai.iFirstImage = IMAGE_NOIMAGE;
               flai.iSecondImage = IMAGE_NOIMAGE;
               flai.pszText = pszDesc;
               flai.lParam = (LPARAM)ii;
               flai.dwFlags = FLIF_DISALLOW_SELECT;

               HLISTITEM hItem;
               if ((hItem = FastList_AddItem (hList, &flai)) != NULL)
                  FastList_SetItemText (hList, hItem, 1, szText);

               ++nItems;

               FreeString (pszDesc);
               }
            }
         }

      FL_EndChange (hList, lpOld);
      }

   if (nItems == 0)
      GetString (szText, IDS_ACTION_DESC_NONE);
   else if (nItems == 1)
      GetString (szText, IDS_ACTION_DESC_ONE);
   else // (nItems >= 2)
      GetString (szText, IDS_ACTION_DESC_MULT);
   SetDlgItemText (l.hAction, IDC_ACTION_DESC, szText);
}


LPTSTR GetActionDescription (LPASACTION pAction)
{
   LPTSTR pszDesc = NULL;

   ULONG status;
   ASOBJPROP Properties;
   ASOBJPROP Properties2;

   switch (pAction->Action)
      {
      case ACTION_REFRESH:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Refresh.idScope, &Properties, &status))
            {
            if (Properties.Type == TYPE_CELL)
               pszDesc = FormatString (IDS_ACTION_REFRESH_CELL, TEXT("%s"), Properties.szName);
            else if (Properties.Type == TYPE_SERVER)
               pszDesc = FormatString (IDS_ACTION_REFRESH_SERVER, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_SCOUT:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Refresh.idScope, &Properties, &status))
            {
            if (Properties.Type == TYPE_CELL)
               pszDesc = FormatString (IDS_ACTION_SCOUT_CELL, TEXT("%s"), Properties.szName);
            else if (Properties.Type == TYPE_SERVER)
               pszDesc = FormatString (IDS_ACTION_SCOUT_SERVER, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_USER_CHANGE:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.User_Change.idUser, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_USER_CHANGE, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_USER_PW_CHANGE:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.User_Pw_Change.idUser, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_USER_PW_CHANGE, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_USER_UNLOCK:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.User_Unlock.idUser, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_USER_UNLOCK, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_USER_CREATE:
         pszDesc = FormatString (IDS_ACTION_USER_CREATE, TEXT("%s"), pAction->u.User_Create.szUser);
         break;

      case ACTION_USER_DELETE:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.User_Delete.idUser, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_USER_DELETE, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_GROUP_CHANGE:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Group_Change.idGroup, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_GROUP_CHANGE, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_GROUP_MEMBER_ADD:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Group_Member_Add.idGroup, &Properties, &status))
            {
            if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Group_Member_Add.idUser, &Properties2, &status))
               {
               pszDesc = FormatString (IDS_ACTION_GROUP_MEMBER_ADD, TEXT("%s%s"), Properties.szName, Properties2.szName);
               }
            }
         break;

      case ACTION_GROUP_MEMBER_REMOVE:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Group_Member_Remove.idGroup, &Properties, &status))
            {
            if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Group_Member_Remove.idUser, &Properties2, &status))
               {
               pszDesc = FormatString (IDS_ACTION_GROUP_MEMBER_REMOVE, TEXT("%s%s"), Properties.szName, Properties2.szName);
               }
            }
         break;

      case ACTION_GROUP_RENAME:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Group_Rename.idGroup, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_GROUP_RENAME, TEXT("%s%s"), Properties.szName, pAction->u.Group_Rename.szNewName);
            }
         break;

      case ACTION_GROUP_CREATE:
         pszDesc = FormatString (IDS_ACTION_GROUP_CREATE, TEXT("%s"), pAction->u.Group_Create.szGroup);
         break;

      case ACTION_GROUP_DELETE:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, pAction->u.Group_Delete.idGroup, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_GROUP_DELETE, TEXT("%s"), Properties.szName);
            }
         break;

      case ACTION_CELL_CHANGE:
         if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, g.idCell, &Properties, &status))
            {
            pszDesc = FormatString (IDS_ACTION_CELL_CHANGE, TEXT("%s"), Properties.szName);
            }
         break;
      }

   return pszDesc;
}

