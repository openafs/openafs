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

#include <windows.h>
#include <winerror.h>
#include <WINNT/TaLocale.h>
#include "settings.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

BOOL RestoreSettings (HKEY hkParent,
                      LPCTSTR pszBase,
                      LPCTSTR pszValue,
                      PVOID pStructure,
                      size_t cbStructure,
                      WORD wVerExpected)
{
   BOOL rc = FALSE;

   size_t cbStored;
   if ((cbStored = GetRegValueSize (hkParent, pszBase, pszValue)) != 0)
      {
      if (cbStored >= sizeof(WORD)+cbStructure)
         {
         PVOID pStructureFinal;
         if ((pStructureFinal = Allocate (cbStored)) != NULL)
            {
            if (GetBinaryRegValue (hkParent, pszBase, pszValue, pStructureFinal, cbStored))
               {
               WORD wVerStored = *(LPWORD)pStructureFinal;

               if ( (HIBYTE(wVerStored) == HIBYTE(wVerExpected)) &&
                    (LOBYTE(wVerStored) >= LOBYTE(wVerExpected)) )
                  {
                  memcpy (pStructure, &((LPBYTE)pStructureFinal)[ sizeof(WORD) ], cbStructure);
                  rc = TRUE;
                  }
               }

            Free (pStructureFinal);
            }
         }
      }

   return rc;
}


BOOL StoreSettings (HKEY hkParent,
                    LPCTSTR pszBase,
                    LPCTSTR pszValue,
                    PVOID pStructure,
                    size_t cbStructure,
                    WORD wVersion)
{
   BOOL rc = FALSE;

   PVOID pStructureFinal;
   if ((pStructureFinal = Allocate (sizeof(WORD) + cbStructure)) != NULL)
      {
      *(LPWORD)pStructureFinal = wVersion;
      memcpy (&((LPBYTE)pStructureFinal)[ sizeof(WORD) ], pStructure, cbStructure);

      rc = SetBinaryRegValue (hkParent, pszBase, pszValue, pStructureFinal, sizeof(WORD) + cbStructure);

      Free (pStructureFinal);
      }

   return rc;
}


void EraseSettings (HKEY hkParent, LPCTSTR pszBase, LPCTSTR pszValue)
{
   HKEY hk;
   if (RegOpenKey (hkParent, pszBase, &hk) == 0)
      {
      RegDeleteValue (hk, pszValue);
      RegCloseKey (hk);
      }
}


BOOL GetBinaryRegValue (HKEY hk,
                        LPCTSTR pszBase,
                        LPCTSTR pszValue,
                        PVOID pData,
                        size_t cbData)
{
   BOOL rc = FALSE;

   HKEY hkFinal;
   if (RegOpenKey (hk, pszBase, &hkFinal) == ERROR_SUCCESS)
      {
      DWORD dwType;
      DWORD dwSize = (DWORD)cbData;

      if (RegQueryValueEx (hkFinal, pszValue, NULL, &dwType, (LPBYTE)pData, &dwSize) == ERROR_SUCCESS)
         rc = TRUE;

      RegCloseKey (hk);
      }

   return rc;
}


size_t GetRegValueSize (HKEY hk,
                        LPCTSTR pszBase,
                        LPCTSTR pszValue)
{
   size_t cb = 0;

   HKEY hkFinal;
   if (RegOpenKey (hk, pszBase, &hkFinal) == ERROR_SUCCESS)
      {
      DWORD dwType;
      DWORD dwSize = 0;

      if (RegQueryValueEx (hkFinal, pszValue, NULL, &dwType, NULL, &dwSize) == ERROR_SUCCESS)
         {
         cb = (size_t)dwSize;
         }

      RegCloseKey (hk);
      }

   return cb;
}


BOOL SetBinaryRegValue (HKEY hk,
                        LPCTSTR pszBase,
                        LPCTSTR pszValue,
                        PVOID pData,
                        size_t cbData)
{
   BOOL rc = FALSE;

   HKEY hkFinal;
   if (RegCreateKey (hk, pszBase, &hkFinal) == ERROR_SUCCESS)
      {
      DWORD dwSize = (DWORD)cbData;

      if (RegSetValueEx (hkFinal, pszValue, NULL, REG_BINARY, (LPBYTE)pData, dwSize) == ERROR_SUCCESS)
         rc = TRUE;

      RegCloseKey (hk);
      }

   return rc;
}


// Under Windows NT, RegDeleteKey() is not recursive--under Windows 95,
// it is.  Sigh.  This routine works recursively on either OS.
//
BOOL RegDeltreeKey (HKEY hk, LPTSTR pszKey)
{
   HKEY hkSub;
   if (RegOpenKey (hk, pszKey, &hkSub) == 0)
      {
      TCHAR szFound[ MAX_PATH ];
      while (RegEnumKey (hkSub, 0, szFound, MAX_PATH) == 0)
         {
         if (!RegDeltreeKey (hkSub, szFound))
            {
            RegCloseKey (hkSub);
            return FALSE;
            }
         }

      RegCloseKey (hkSub);
      }

   if (RegDeleteKey (hk, pszKey) != 0)
      return FALSE;

   return TRUE;
}

