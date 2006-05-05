/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "svrmgr.h"
#include "svr_general.h"
#include "propcache.h"
#include "display.h"


/*
 * SERVER PREFERENCES _________________________________________________________
 *
 */

PVOID Server_LoadPreferences (LPIDENT lpiServer)
{
   LPSERVER_PREF psp = New (SERVER_PREF);

   if (!RestorePreferences (lpiServer, psp, sizeof(SERVER_PREF)))
      {
      Alert_SetDefaults (&psp->oa);

      psp->perWarnAggFull = perDEFAULT_AGGFULL_WARNING;
      psp->perWarnSetFull = perDEFAULT_SETFULL_WARNING;
      psp->fWarnSvcStop = fDEFAULT_SVCSTOP_WARNING;
      psp->fWarnSvrTimeout = TRUE;
      psp->fWarnSetNoVLDB = TRUE;
      psp->fWarnSetNoServ = TRUE;
      psp->fWarnAggNoServ = TRUE;
      psp->fWarnAggAlloc = FALSE;

      psp->rLast.right = 0;
      psp->fOpen = FALSE;
      psp->fExpandTree = TRUE;
      }

   psp->fIsMonitored = TRUE; // until dispatch.cpp sets it otherwise

   Alert_Initialize (&psp->oa);
   return psp;
}


BOOL Server_SavePreferences (LPIDENT lpiServer)
{
   BOOL rc = FALSE;

   PVOID psp = lpiServer->GetUserParam();
   if (psp != NULL)
      {
      rc = StorePreferences (lpiServer, psp, sizeof(SERVER_PREF));
      }

   return rc;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Server_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen)
{
   if (!ptScreen.x && !ptScreen.y)
      {
      RECT rWindow;
      GetWindowRect (hList, &rWindow);
      ptScreen.x = rWindow.left + (rWindow.right -rWindow.left)/2;
      ptScreen.y = rWindow.top + (rWindow.bottom -rWindow.top)/2;
      Server_ShowParticularPopupMenu (hList, ptScreen, NULL);
      }
   else if (FL_HitTestForHeaderBar (hList, ptList))
      {
      HMENU hm = TaLocale_LoadMenu (MENU_COLUMNS);
      DisplayContextMenu (hm, ptScreen, hList);
      }
   else
      {
      LPIDENT lpiServer = NULL;

      HLISTITEM hItem;
      if ((hItem = FastList_ItemFromPoint (hList, &ptList, TRUE)) != NULL)
         lpiServer = (LPIDENT)FL_GetData (hList, hItem);

      if (lpiServer && (lpiServer != (LPIDENT)FL_GetSelectedData (hList)))
         lpiServer = NULL;

      Server_ShowParticularPopupMenu (hList, ptScreen, lpiServer);
      }
}


void Server_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiServer)
{
   HMENU hm;
   
   if (lpiServer == NULL)
      hm = TaLocale_LoadMenu (MENU_SVR_NONE);
   else
      hm = TaLocale_LoadMenu (MENU_SVR);

   if (hm != NULL)
      {
      if (lpiServer == NULL)
         {
         int lvs = (gr.fPreview && !gr.fVert) ? gr.diHorz.viewSvr.lvsView : gr.diVert.viewSvr.lvsView;
         HMENU hmView = GetSubMenu (hm, 0);

         CheckMenuRadioItem (hmView,
                             M_SVR_VIEW_LARGE, M_SVR_VIEW_REPORT,
                             ( (lvs == FLS_VIEW_SMALL) ? M_SVR_VIEW_SMALL :
                               (lvs == FLS_VIEW_LIST)  ? M_SVR_VIEW_REPORT :
                                                         M_SVR_VIEW_LARGE ),
                             MF_BYCOMMAND);

         ICONVIEW ivSvr = Display_GetServerIconView();

         CheckMenuRadioItem (hmView,
                             M_SVR_VIEW_ONEICON, M_SVR_VIEW_STATUS,
                             (ivSvr == ivTWOICONS) ? M_SVR_VIEW_TWOICONS :
                             (ivSvr == ivONEICON)  ? M_SVR_VIEW_ONEICON : M_SVR_VIEW_STATUS,
                             MF_BYCOMMAND);

         if (lvs != FLS_VIEW_LIST)
            {
            EnableMenu (hmView, M_SVR_VIEW_ONEICON,  FALSE);
            EnableMenu (hmView, M_SVR_VIEW_TWOICONS, FALSE);
            EnableMenu (hmView, M_SVR_VIEW_STATUS,   FALSE);
            }

         if (!PropCache_Search (pcSERVER, ANYVALUE))
            EnableMenu (hmView, M_SVR_CLOSEALL, FALSE);
         }
      else
         {
         BOOL fOpenNow = (BOOL)!!(PropCache_Search (pcSERVER, lpiServer));

         LPSERVER_PREF lpsp;
         if ( ((lpsp = (LPSERVER_PREF)lpiServer->GetUserParam()) == NULL) ||
              (lpsp->fIsMonitored) )
            {
            EnableMenu (hm, M_SVR_OPEN,  !fOpenNow);
            EnableMenu (hm, M_SVR_CLOSE,  fOpenNow);
            CheckMenu (hm, M_SVR_MONITOR, TRUE);
            }
         else
            {
            if (!fOpenNow)
               EnableMenu (hm, M_SVR_CLOSE, FALSE);
            if (fOpenNow || !gr.fOpenMonitors)
               EnableMenu (hm, M_SVR_OPEN, FALSE);

            CheckMenu (hm, M_SVR_MONITOR, FALSE);

            EnableMenu (hm, M_SVR_SECURITY,  FALSE);
            EnableMenu (hm, M_SALVAGE,  FALSE);
            EnableMenu (hm, M_SET_CLONE,  FALSE);
            EnableMenu (hm, M_SET_UNLOCK, FALSE);
            EnableMenu (hm, M_SYNCVLDB,   FALSE);
            }
         }

      DisplayContextMenu (hm, ptScreen, hParent);
      }
}

