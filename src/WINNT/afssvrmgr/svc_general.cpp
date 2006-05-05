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
#include "svc_general.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Services_GuessLogName (LPTSTR pszLogFile, LPIDENT lpiService)
{
   TCHAR szService[ cchRESOURCE ];
   lpiService->GetServiceName (szService);
   Services_GuessLogName (pszLogFile, szService);
}

void Services_GuessLogName (LPTSTR pszLogFile, LPTSTR pszService)
{
   if (!lstrcmpi (pszService, TEXT("BOS")))
      lstrcpy (pszLogFile, TEXT("BosLog"));
   else if (!lstrcmpi (pszService, TEXT("kaserver")))
      lstrcpy (pszLogFile, TEXT("AuthLog"));
   else if (!lstrcmpi (pszService, TEXT("buserver")))
      lstrcpy (pszLogFile, TEXT("BackupLog"));
   else if (!lstrcmpi (pszService, TEXT("fileserver")))
      lstrcpy (pszLogFile, TEXT("FileLog"));
   else if (!lstrcmpi (pszService, TEXT("fs")))
      lstrcpy (pszLogFile, TEXT("FileLog"));
   else if (!lstrcmpi (pszService, TEXT("volserver")))
      lstrcpy (pszLogFile, TEXT("VolserLog"));
   else if (!lstrcmpi (pszService, TEXT("ptserver")))
      lstrcpy (pszLogFile, TEXT("PtLog"));
   else if (!lstrcmpi (pszService, TEXT("salvager")))
      lstrcpy (pszLogFile, TEXT("SalvageLog"));
   else if (!lstrcmpi (pszService, TEXT("vlserver")))
      lstrcpy (pszLogFile, TEXT("VLLog"));
   else if (!lstrcmpi (pszService, TEXT("upclient")))
      pszLogFile[0] = TEXT('\0');
   else if (!lstrcmpi (pszService, TEXT("upserver")))
      pszLogFile[0] = TEXT('\0');
   else
      pszLogFile[0] = TEXT('\0');
}


PVOID Services_LoadPreferences (LPIDENT lpiService)
{
   LPSERVICE_PREF psp = New (SERVICE_PREF);

   if (!RestorePreferences (lpiService, psp, sizeof(SERVICE_PREF)))
      {
      psp->fWarnSvcStop = TRUE;
      Alert_SetDefaults (&psp->oa);

      Services_GuessLogName (psp->szLogFile, lpiService);

      // write the logfile down so we won't have to guess again.
      StorePreferences (lpiService, psp, sizeof(SERVICE_PREF));
      }

   Alert_Initialize (&psp->oa);
   return psp;
}


BOOL Services_SavePreferences (LPIDENT lpiService)
{
   BOOL rc = FALSE;

   PVOID psp = lpiService->GetUserParam();
   if (psp != NULL)
      {
      rc = StorePreferences (lpiService, psp, sizeof(SERVICE_PREF));
      }

   return rc;
}

