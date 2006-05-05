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

#include <windows.h>
#include <stdlib.h>
#include <WINNT\afsreg.h>
#include "pagesize.h"


ULONG ExtractPageSize (LPCTSTR psz)
{
   LPCTSTR pch = &psz[ lstrlen(psz) ];
   while ((pch > psz) && (isdigit(pch[-1])))
      pch--;
   return atol(pch);
}


ULONG GetPagingSpace (void)
{
   ULONG ckPageSpace = 0;

   HKEY hk;
   if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), 0, KEY_QUERY_VALUE, &hk) == ERROR_SUCCESS)
      {
      TCHAR mszData[1024] = TEXT("");
      DWORD dwSize = sizeof(mszData);
      DWORD dwType = REG_MULTI_SZ;

      if (RegQueryValueEx (hk, TEXT("PagingFiles"), 0, &dwType, (PBYTE)mszData, &dwSize) == ERROR_SUCCESS)
         {
         for (LPTSTR psz = mszData; *psz; psz += 1+lstrlen(psz))
            {
            ckPageSpace += ExtractPageSize (psz);
            }
         }

      RegCloseKey (hk);
      }

   return ckPageSpace * 1024;
}

