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
#include "prefs.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define SETTINGS_KW    TEXT("Settings")
#define PREFERENCES_KW TEXT("Preferences")
#define SERVICES_KW    TEXT("Services")
#define AGGREGATES_KW  TEXT("Aggregates")
#define FILESETS_KW    TEXT("Filesets")
#define SUBSETS_KW     TEXT("Server Subsets")


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL GetPreferencesInfo (LPIDENT lpi, LPTSTR pszPath, WORD *pwVer)
{
   if (lpi == NULL)
      return FALSE;

   // HKCU\Software\...\SVRMgr + \CellName
   //
   lstrcpy (pszPath, REGSTR_SETTINGS_PREFS);
   lstrcat (pszPath, TEXT("\\"));
   lpi->GetCellName (&pszPath[ lstrlen(pszPath) ]);

   // HKCU\Software\...\SVRMgr\CellName + \ServerName
   //
   lstrcat (pszPath, TEXT("\\"));
   lpi->GetLongServerName (&pszPath[ lstrlen(pszPath) ]);

   *pwVer = wVerSERVER_PREF;

   if (lpi->fIsService())
      {
      // HKCU\Software\...\SVRMgr\CellName\ServerName + \Services\ServiceName
      //
      lstrcat (pszPath, TEXT("\\"));
      lstrcat (pszPath, SERVICES_KW);
      lstrcat (pszPath, TEXT("\\"));
      lpi->GetServiceName (&pszPath[ lstrlen(pszPath) ]);
      *pwVer = wVerSERVICE_PREF;
      }
   else if (lpi->fIsAggregate())
      {
      // HKCU\Software\...\SVRMgr\CellName\ServerName + \Aggs\AggregateName
      //
      lstrcat (pszPath, TEXT("\\"));
      lstrcat (pszPath, AGGREGATES_KW);
      lstrcat (pszPath, TEXT("\\"));
      lpi->GetAggregateName (&pszPath[ lstrlen(pszPath) ]);
      *pwVer = wVerAGGREGATE_PREF;
      }
   else if (lpi->fIsFileset())
      {
      // HKCU\Software\...\SVRMgr\CellName\ServerName + \Filesets\FilesetName
      //
      lstrcat (pszPath, TEXT("\\"));
      lstrcat (pszPath, FILESETS_KW);
      lstrcat (pszPath, TEXT("\\"));
      lpi->GetFilesetName (&pszPath[ lstrlen(pszPath) ]);
      *pwVer = wVerFILESET_PREF;
      }

   return TRUE;
}


BOOL RestorePreferences (LPIDENT lpi, void *pData, size_t cbData)
{
   TCHAR szPath[ MAX_PATH ];
   WORD wVer;

   if (!GetPreferencesInfo (lpi, szPath, &wVer))
      return FALSE;

   return RestoreSettings (REGSTR_SETTINGS_BASE, szPath, SETTINGS_KW, pData, cbData, wVer);
}


BOOL StorePreferences (LPIDENT lpi, void *pData, size_t cbData)
{
   TCHAR szPath[ MAX_PATH ];
   WORD wVer;

   if (!GetPreferencesInfo (lpi, szPath, &wVer))
      return FALSE;

   return StoreSettings (REGSTR_SETTINGS_BASE, szPath, SETTINGS_KW, pData, cbData, wVer);
}


void ErasePreferences (LPTSTR pszCell, LPTSTR pszServer)
{
   BOOL fWildcard = FALSE;
   LPTSTR pszDelete;

   // HKCU\Software\...\SVRMgr
   //
   TCHAR szPath[ MAX_PATH ];
   lstrcpy (szPath, REGSTR_SETTINGS_PATH);

   if (!pszCell)
      pszDelete = PREFERENCES_KW;
   else
      {
      lstrcat (szPath, TEXT("\\"));
      lstrcat (szPath, PREFERENCES_KW);

      if (!pszServer)
         pszDelete = pszCell;
      else
         {
         lstrcat (szPath, TEXT("\\"));
         lstrcat (szPath, pszCell);
         pszDelete = pszServer;
         fWildcard = TRUE;
         }
      }

   HKEY hk;
   if (RegOpenKey (HKCU, szPath, &hk) == 0)
      {
      if (!fWildcard)
         {
         RegDeltreeKey (hk, pszDelete);
         }
      else
         {
         TCHAR szFound[ MAX_PATH ];
         for (int ii = 0; RegEnumKey (hk, ii, szFound, MAX_PATH) == 0; ++ii)
            {
            if (lstrncmpi (szFound, pszDelete, lstrlen(pszDelete)))
               continue;

            if (RegDeltreeKey (hk, szFound))
               ii = -1; // restart search
            }
         }

      RegCloseKey (hk);
      }
}


HKEY OpenSubsetsKey (LPTSTR pszCell, BOOL fCreate)
{
   return OpenSubsetsSubKey (pszCell, NULL, fCreate);
}


HKEY OpenSubsetsSubKey (LPTSTR pszCell, LPTSTR pszSubset, BOOL fCreate)
{
   // HKCU\Software\...\SVRMgr + \CellName
   //
   TCHAR szPath[ MAX_PATH ];
   lstrcpy (szPath, REGSTR_SETTINGS_PREFS);
   lstrcat (szPath, TEXT("\\"));

   if (pszCell)
      lstrcat (szPath, pszCell);
   else if (g.lpiCell)
      g.lpiCell->GetCellName (&szPath[ lstrlen(szPath) ]);

   // HKCU\Software\...\SVRMgr\CellName + \"Server Subsets"
   //
   lstrcat (szPath, TEXT("\\"));
   lstrcat (szPath, SUBSETS_KW);

   // HKCU\Software\...\SVRMgr\CellName + \"Server Subsets" + \Subset
   //
   if (pszSubset)
      {
      lstrcat (szPath, TEXT("\\"));
      lstrcat (szPath, pszSubset);
      }

   // Open or create that key.
   //
   HKEY hk = NULL;

   if (fCreate)
      {
      if (pszSubset)  // destroy and recreate a subset key?
         RegDeleteKey (HKCU, szPath);
      if (fCreate != 2)  // okay, ugly hack: pass 2 to just delete the key
         {
         if (RegCreateKey (HKCU, szPath, &hk) != 0)
            hk = NULL;
         }
      }
   else
      {
      if (RegOpenKey (HKCU, szPath, &hk) != 0)
         hk = NULL;
      }

   return hk;
}

