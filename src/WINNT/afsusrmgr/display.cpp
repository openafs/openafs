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
#include "usr_col.h"
#include "grp_col.h"
#include "mch_col.h"


/*
 * ANIMATED ICON ______________________________________________________________
 *
 */

static DWORD l_cReqAnimation = 0;

void Display_StartWorking (void)
{
   if ((++l_cReqAnimation) == 1)
      {
      AfsAppLib_StartAnimation (GetDlgItem (g.hMain, IDC_ANIM));
      }
}


void Display_StopWorking (void)
{
   if (!l_cReqAnimation || !(--l_cReqAnimation))
      {
      AfsAppLib_StopAnimation (GetDlgItem (g.hMain, IDC_ANIM));
      }
}


/*
 * USER/GROUP LISTS ___________________________________________________________
 *
 */

void Display_PopulateList (void)
{
   switch (Display_GetActiveTab())
      {
      case ttUSERS:
         Display_PopulateUserList();
         break;

      case ttGROUPS:
         Display_PopulateGroupList();
         break;

      case ttMACHINES:
         Display_PopulateMachineList();
         break;
      }
}


void Display_PopulateUserList (void)
{
   if (g.idCell)
      {
      HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
      HWND hList = GetDlgItem (hDlg, IDC_USERS_LIST);
      if (IsWindow (hList))
         {
         TCHAR szQuerying[ cchRESOURCE ];
         GetString (szQuerying, IDS_QUERYING_LONG);
         SetDlgItemText (hDlg, IDC_USERS_TITLE, szQuerying);

         Display_StartWorking();
         GetDlgItemText (hDlg, IDC_USERS_PATTERN, g.szPatternUsers, cchNAME);
         StartTask (taskUPD_USERS, g.hMain);
         }
      }
}


void Display_PopulateGroupList (void)
{
   if (g.idCell)
      {
      HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
      HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
      if (IsWindow (hList))
         {
         TCHAR szQuerying[ cchRESOURCE ];
         GetString (szQuerying, IDS_QUERYING_LONG);
         SetDlgItemText (hDlg, IDC_GROUPS_TITLE, szQuerying);

         Display_StartWorking();
         GetDlgItemText (hDlg, IDC_GROUPS_PATTERN, g.szPatternGroups, cchNAME);
         StartTask (taskUPD_GROUPS, g.hMain);
         }
      }
}


void Display_PopulateMachineList (void)
{
   if (g.idCell)
      {
      HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
      HWND hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
      if (IsWindow (hList))
         {
         TCHAR szQuerying[ cchRESOURCE ];
         GetString (szQuerying, IDS_QUERYING_LONG);
         SetDlgItemText (hDlg, IDC_MACHINES_TITLE, szQuerying);

         Display_StartWorking();
         GetDlgItemText (hDlg, IDC_MACHINES_PATTERN, g.szPatternMachines, cchNAME);
         StartTask (taskUPD_MACHINES, g.hMain);
         }
      }
}


void Display_OnEndTask_UpdUsers (LPTASKPACKET ptp)
{
   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   HWND hList = GetDlgItem (hDlg, IDC_USERS_LIST);
   if (IsWindow (hList) && !lstrcmpi (TASKDATA(ptp)->szPattern, g.szPatternUsers))
      {
      FastList_Begin (hList);

      // Update the title above the list to indicate what we're showing
      //
      TCHAR szCell[ cchRESOURCE ];
      asc_CellNameGet_Fast (g.idClient, g.idCell, szCell);

      LPTSTR pszTitle = FormatString ((TASKDATA(ptp)->szPattern[0] || (gr.SearchUsers.SearchType != SEARCH_NO_LIMITATIONS)) ? IDS_USERS_PATTERN : IDS_USERS_ALL, TEXT("%s"), szCell);
      SetDlgItemText (hDlg, IDC_USERS_TITLE, pszTitle);
      FreeString (pszTitle);

      // For faster access, we'll want to use a hashlist to deal with
      // the items in our ASIDLIST (right now it's just a flat array).
      // This lets us remove duplicates--which is no big deal because
      // there shouldn't be any anyway--but more importantly it lets
      // us instantly determine if a particular ASID is in the list
      // (the asc_AsidListTest function is O(n), and we need O(1)).
      //
      LPHASHLIST pAsidList = New (HASHLIST);
      if (TASKDATA(ptp)->pAsidList)
         {
         for (size_t iAsid = 0; iAsid < TASKDATA(ptp)->pAsidList->cEntries; ++iAsid)
            pAsidList->AddUnique ((PVOID)(TASKDATA(ptp)->pAsidList->aEntries[ iAsid ].idObject));
         }

      // Delete any items which are currently in the FastList but
      // which aren't in our AsidList.
      //
      HLISTITEM hItemNext;
      for (HLISTITEM hItem = FastList_FindFirst (hList); hItem; hItem = hItemNext)
         {
         hItemNext = FastList_FindNext (hList, hItem);
         ASID idObject = (ASID)FastList_GetItemParam (hList, hItem);
         if (!pAsidList->fIsInList ((PVOID)idObject))
            FastList_RemoveItem (hList, hItem);
         }

      // Add items for any entries which are in our AsidList but aren't
      // currently in the FastList.
      //
      DWORD dwStyle = GetWindowLong (hList, GWL_STYLE);

      for (LPENUM pEnum = pAsidList->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         ASID idObject = (ASID)( pEnum->GetObject() );

         HLISTITEM hItem;
         if ((hItem = FastList_FindItem (hList, (LPARAM)idObject)) == NULL)
            {
            FASTLISTADDITEM ai;
            memset (&ai, 0x00, sizeof(ai));
            Display_GetImageIcons (dwStyle, gr.ivUsr, idObject, imageUSER, IMAGE_NOIMAGE, &ai.iFirstImage, &ai.iSecondImage);
            ai.lParam = (LPARAM)idObject;
            hItem = FastList_AddItem (hList, &ai);
            }
         }

      Delete (pAsidList);
      FastList_End (hList);
      }

   Display_StopWorking();
}


void Display_OnEndTask_UpdGroups (LPTASKPACKET ptp)
{
   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
   if (IsWindow (hList) && !lstrcmpi (TASKDATA(ptp)->szPattern, g.szPatternGroups))
      {
      FastList_Begin (hList);

      // Update the title above the list to indicate what we're showing
      //
      TCHAR szCell[ cchRESOURCE ];
      asc_CellNameGet_Fast (g.idClient, g.idCell, szCell);

      LPTSTR pszTitle = FormatString ((TASKDATA(ptp)->szPattern[0]) ? IDS_GROUPS_PATTERN : IDS_GROUPS_ALL, TEXT("%s"), szCell);
      SetDlgItemText (hDlg, IDC_GROUPS_TITLE, pszTitle);
      FreeString (pszTitle);

      // For faster access, we'll want to use a hashlist to deal with
      // the items in our ASIDLIST (right now it's just a flat array).
      // This lets us remove duplicates--which is no big deal because
      // there shouldn't be any anyway--but more importantly it lets
      // us instantly determine if a particular ASID is in the list
      // (the asc_AsidListTest function is O(n), and we need O(1)).
      //
      LPHASHLIST pAsidList = New (HASHLIST);
      if (TASKDATA(ptp)->pAsidList)
         {
         for (size_t iAsid = 0; iAsid < TASKDATA(ptp)->pAsidList->cEntries; ++iAsid)
            pAsidList->AddUnique ((PVOID)(TASKDATA(ptp)->pAsidList->aEntries[ iAsid ].idObject));
         }

      // Delete any items which are currently in the FastList but
      // which aren't in our AsidList.
      //
      HLISTITEM hItemNext;
      for (HLISTITEM hItem = FastList_FindFirst (hList); hItem; hItem = hItemNext)
         {
         hItemNext = FastList_FindNext (hList, hItem);
         ASID idObject = (ASID)FastList_GetItemParam (hList, hItem);
         if (!pAsidList->fIsInList ((PVOID)idObject))
            FastList_RemoveItem (hList, hItem);
         }

      // Add items for any entries which are in our AsidList but aren't
      // currently in the FastList.
      //
      DWORD dwStyle = GetWindowLong (hList, GWL_STYLE);

      for (LPENUM pEnum = pAsidList->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         ASID idObject = (ASID)( pEnum->GetObject() );

         HLISTITEM hItem;
         if ((hItem = FastList_FindItem (hList, (LPARAM)idObject)) == NULL)
            {
            FASTLISTADDITEM ai;
            memset (&ai, 0x00, sizeof(ai));
            Display_GetImageIcons (dwStyle, gr.ivGrp, idObject, imageGROUP, IMAGE_NOIMAGE, &ai.iFirstImage, &ai.iSecondImage);
            ai.lParam = (LPARAM)idObject;
            hItem = FastList_AddItem (hList, &ai);
            }
         }

      Delete (pAsidList);
      FastList_End (hList);
      }

   Display_StopWorking();
}


void Display_OnEndTask_UpdMachines (LPTASKPACKET ptp)
{
   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   HWND hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
   if (IsWindow (hList) && !lstrcmpi (TASKDATA(ptp)->szPattern, g.szPatternMachines))
      {
      FastList_Begin (hList);

      // Update the title above the list to indicate what we're showing
      //
      TCHAR szCell[ cchRESOURCE ];
      asc_CellNameGet_Fast (g.idClient, g.idCell, szCell);

      LPTSTR pszTitle = FormatString ((TASKDATA(ptp)->szPattern[0]) ? IDS_MACHINES_PATTERN : IDS_MACHINES_ALL, TEXT("%s"), szCell);
      SetDlgItemText (hDlg, IDC_MACHINES_TITLE, pszTitle);
      FreeString (pszTitle);

      // For faster access, we'll want to use a hashlist to deal with
      // the items in our ASIDLIST (right now it's just a flat array).
      // This lets us remove duplicates--which is no big deal because
      // there shouldn't be any anyway--but more importantly it lets
      // us instantly determine if a particular ASID is in the list
      // (the asc_AsidListTest function is O(n), and we need O(1)).
      //
      LPHASHLIST pAsidList = New (HASHLIST);
      if (TASKDATA(ptp)->pAsidList)
         {
         for (size_t iAsid = 0; iAsid < TASKDATA(ptp)->pAsidList->cEntries; ++iAsid)
            pAsidList->AddUnique ((PVOID)(TASKDATA(ptp)->pAsidList->aEntries[ iAsid ].idObject));
         }

      // Delete any items which are currently in the FastList but
      // which aren't in our AsidList.
      //
      HLISTITEM hItemNext;
      for (HLISTITEM hItem = FastList_FindFirst (hList); hItem; hItem = hItemNext)
         {
         hItemNext = FastList_FindNext (hList, hItem);
         ASID idObject = (ASID)FastList_GetItemParam (hList, hItem);
         if (!pAsidList->fIsInList ((PVOID)idObject))
            FastList_RemoveItem (hList, hItem);
         }

      // Add items for any entries which are in our AsidList but aren't
      // currently in the FastList.
      //
      DWORD dwStyle = GetWindowLong (hList, GWL_STYLE);

      for (LPENUM pEnum = pAsidList->FindFirst(); pEnum; pEnum = pEnum->FindNext())
         {
         ASID idObject = (ASID)( pEnum->GetObject() );

         HLISTITEM hItem;
         if ((hItem = FastList_FindItem (hList, (LPARAM)idObject)) == NULL)
            {
            FASTLISTADDITEM ai;
            memset (&ai, 0x00, sizeof(ai));
            Display_GetImageIcons (dwStyle, gr.ivMch, idObject, imageSERVER, IMAGE_NOIMAGE, &ai.iFirstImage, &ai.iSecondImage);
            ai.lParam = (LPARAM)idObject;
            hItem = FastList_AddItem (hList, &ai);
            }
         }

      Delete (pAsidList);
      FastList_End (hList);
      }

   Display_StopWorking();
}


void Display_RefreshView (LPVIEWINFO pviNew, ICONVIEW ivNew)
{
   // Find the current VIEWINFO and ICONVIEW settings
   //
   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));

   HWND hList;
   LPVIEWINFO pviOld;
   ICONVIEW *pivOld;
   switch (Display_GetActiveTab())
      {
      case ttUSERS:
         pivOld = &gr.ivUsr;
         pviOld = &gr.viewUsr;
         hList = GetDlgItem (hDlg, IDC_USERS_LIST);
         break;

      case ttGROUPS:
         pivOld = &gr.ivGrp;
         pviOld = &gr.viewGrp;
         hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
         break;

      case ttMACHINES:
         pivOld = &gr.ivMch;
         pviOld = &gr.viewMch;
         hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
         break;
      }

   if (IsWindow(hList))
      {
      FastList_Begin (hList);

      // If the VIEWINFO state has changed, fix it. This will change between
      // large icons, small icons and details, as well as correct the currently-
      // displayed columns to match what's in the new VIEWINFO structure.
      //
      BOOL fChangedLayouts = FALSE;

      if (memcmp (pviOld, pviNew, sizeof(VIEWINFO)))
         {
         FL_RestoreView (hList, pviNew);
         fChangedLayouts = ((pviOld->lvsView & FLS_VIEW_MASK) != (pviNew->lvsView & FLS_VIEW_MASK)) ? TRUE : FALSE;
         memcpy (pviOld, pviNew, sizeof(VIEWINFO));
         }

      // If the ICONVIEW state has changed, fix all items to show just
      // the appropriate icons. We'll also have to do this if we just changed
      // from details/small/large icons to another layout (that's what
      // fChangedLayouts indicates)
      //
      if ((*pivOld != ivNew) || (fChangedLayouts))
         {
         DWORD dwStyle = GetWindowLong (hList, GWL_STYLE);

         HLISTITEM hItem = NULL;
         while ((hItem = FastList_FindNext (hList, hItem)) != NULL)
            {
            ASID idObject = (ASID)FastList_GetItemParam (hList, hItem);

            int iFirstImage;
            int iSecondImage;
            if (pviOld == &gr.viewUsr)
               Display_GetImageIcons (dwStyle, ivNew, idObject, imageUSER, IMAGE_NOIMAGE, &iFirstImage, &iSecondImage);
            else if (pviOld == &gr.viewGrp)
               Display_GetImageIcons (dwStyle, ivNew, idObject, imageGROUP, IMAGE_NOIMAGE, &iFirstImage, &iSecondImage);
            else
               Display_GetImageIcons (dwStyle, ivNew, idObject, imageSERVER, IMAGE_NOIMAGE, &iFirstImage, &iSecondImage);

            FastList_SetItemFirstImage (hList, hItem, iFirstImage);
            FastList_SetItemSecondImage (hList, hItem, iSecondImage);
            }

         *pivOld = ivNew;
         }

      FastList_End (hList);
      }
}


void Display_RefreshView_Fast (void)
{
   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_USERS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
   if (IsWindow (hList))
      {
      RECT rWindow;
      GetClientRect (hList, &rWindow);
      InvalidateRect (hList, &rWindow, FALSE);
      UpdateWindow (hList);
      }
}


void Display_SelectAll (void)
{
   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_USERS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
   if (IsWindow (hList))
      {
      // Select all items in the list
      //
      FastList_SelectAll (hList);

      // Simulate a FLN_ITEMSELECT notification (since we changed
      // the selection programmatically, no notification will have been
      // sent).
      //
      FLN_ITEMSELECT_PARAMS fln;
      fln.hdr.hwndFrom = hList;
      fln.hdr.idFrom = GetWindowLong (hList, GWL_ID);
      fln.hdr.code = FLN_ITEMSELECT;
      fln.hItem = FastList_FindFirstSelected (hList);
      SendMessage (GetParent (hList), WM_NOTIFY, (WPARAM)fln.hdr.idFrom, (LPARAM)&fln);
      }
}


LPASIDLIST Display_GetSelectedList (void)
{
   LPASIDLIST pAsidList = NULL;

   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_USERS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
   if (IsWindow (hList))
      {
      if (asc_AsidListCreate (&pAsidList))
         {
         for (HLISTITEM hItem = FastList_FindFirstSelected (hList);
              hItem != NULL;
              hItem = FastList_FindNextSelected (hList, hItem))
            {
            ASID idObject = (ASID)FastList_GetItemParam (hList, hItem);
            if (idObject)
               asc_AsidListAddEntry (&pAsidList, idObject, 0);
            }
         }
      }

   return pAsidList;
}


size_t Display_GetSelectedCount (void)
{
   size_t cSelected = 0;

   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   HWND hList = GetDlgItem (hDlg, IDC_GROUPS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_USERS_LIST);
   if (!IsWindow (hList))
      hList = GetDlgItem (hDlg, IDC_MACHINES_LIST);
   if (IsWindow (hList))
      {
      for (HLISTITEM hItem = FastList_FindFirstSelected (hList);
           hItem != NULL;
           hItem = FastList_FindNextSelected (hList, hItem))
         {
         ++cSelected;
         }
      }

   return cSelected;
}


TABTYPE Display_GetActiveTab (void)
{
   HWND hDlg = GetTabChild (GetDlgItem (g.hMain, IDC_TAB));
   if (IsWindow (GetDlgItem (hDlg, IDC_GROUPS_LIST)))
      return ttGROUPS;
   if (IsWindow (GetDlgItem (hDlg, IDC_MACHINES_LIST)))
      return ttMACHINES;
   return ttUSERS;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL Display_HandleColumnNotify (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp, LPVIEWINFO pvi)
{
   if (msg == WM_NOTIFY)
      {
      HWND hList = GetDlgItem (hDlg, (int)((LPNMHDR)lp)->idFrom);
      if (fIsFastList (hList))
         {
         switch (((LPNMHDR)lp)->code)
            { 
            case FLN_COLUMNRESIZE:
               FL_StoreView (hList, pvi);
               return TRUE;

            case FLN_COLUMNCLICK:
               LPFLN_COLUMNCLICK_PARAMS pp = (LPFLN_COLUMNCLICK_PARAMS)lp;

               int iCol;
               BOOL fRev;
               FastList_GetSortStyle (hList, &iCol, &fRev);

               if (iCol == pp->icol)
                  FastList_SetSortStyle (hList, iCol, !fRev);
               else
                  FastList_SetSortStyle (hList, pp->icol, FALSE);

               FL_StoreView (hList, pvi);
               return TRUE;
            }
         }
      }

   return FALSE;
}


BOOL CALLBACK Display_GetItemText (HWND hList, LPFLN_GETITEMTEXT_PARAMS pfln, UINT_PTR dwCookie)
{ 
   LPVIEWINFO lpvi = (LPVIEWINFO)dwCookie;
   ASID idObject = (ASID)(pfln->item.lParam);

   pfln->item.pszText[0] = TEXT('\0');

   if ((idObject) && (pfln->item.icol < (int)lpvi->nColsShown))
      {
      size_t iCol = lpvi->aColumns[ pfln->item.icol ];

      if (lpvi == &gr.viewUsr)
         {
         User_GetColumn (idObject, (USERCOLUMN)iCol, pfln->item.pszText, NULL, NULL, NULL);
         }
      else if (lpvi == &gr.viewGrp)
         {
         Group_GetColumn (idObject, (GROUPCOLUMN)iCol, pfln->item.pszText, NULL, NULL, NULL);
         }
      else if (lpvi == &gr.viewMch)
         {
         Machine_GetColumn (idObject, (MACHINECOLUMN)iCol, pfln->item.pszText, NULL, NULL, NULL);
         }
      }

   return TRUE;
}


void Display_GetImageIcons (DWORD dwStyle, ICONVIEW iv, ASID idObject, int iImageNormal, int iImageAlert, int *piFirstImage, int *piSecondImage)
{
   BOOL fAlert = FALSE;

   if ((dwStyle & FLS_VIEW_MASK) != FLS_VIEW_LIST)
      iv = ivONEICON;

   switch (iv)
      {
      case ivTWOICONS:
         *piFirstImage = iImageNormal;
         *piSecondImage = (fAlert) ? iImageAlert : IMAGE_BLANKIMAGE;
         break;

      case ivONEICON:
         *piFirstImage = (fAlert) ? iImageAlert : iImageNormal;
         *piSecondImage = IMAGE_NOIMAGE;
         break;

      case ivSTATUS:
         *piFirstImage = (fAlert) ? iImageAlert : IMAGE_BLANKIMAGE;
         *piSecondImage = IMAGE_NOIMAGE;
         break;
      }
}

