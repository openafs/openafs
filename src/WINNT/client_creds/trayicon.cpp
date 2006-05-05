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

#include "afscreds.h"



/*
 * ROUTINES ___________________________________________________________________
 *
 */

void ChangeTrayIcon (int nim)
{
   static BOOL fAdded = FALSE;
   static BOOL fDeleted = FALSE;
   if ((nim == NIM_MODIFY) && (!fAdded))
      nim = NIM_ADD;
   if ((nim == NIM_MODIFY) && (fDeleted))
      return;

   if ((nim != NIM_DELETE) || (IsWindow (g.hMain)))
      {
      static HICON ICON_CREDS_YES = TaLocale_LoadIcon (IDI_CREDS_YES);
      static HICON ICON_CREDS_NO  = TaLocale_LoadIcon (IDI_CREDS_NO);

      size_t iExpired = Main_FindExpiredCreds();

      NOTIFYICONDATA nid;
      memset (&nid, 0x00, sizeof(NOTIFYICONDATA));
      nid.cbSize = sizeof(NOTIFYICONDATA);
      nid.hWnd = g.hMain;
      nid.uID = 0;
      nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
      nid.uCallbackMessage = WM_TRAYICON;
      lock_ObtainMutex(&g.credsLock);
      nid.hIcon = ((g.cCreds != 0) && (iExpired == (size_t)-1)) ? ICON_CREDS_YES : ICON_CREDS_NO;
      lock_ReleaseMutex(&g.credsLock);
      GetString (nid.szTip, (g.fIsWinNT) ? IDS_TOOLTIP : IDS_TOOLTIP_95);
      Shell_NotifyIcon (nim, &nid);
      }

   if (nim == NIM_ADD)
      fAdded = TRUE;
   if (nim == NIM_DELETE)
      fDeleted = TRUE;
}

