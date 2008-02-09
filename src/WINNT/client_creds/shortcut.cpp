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

#include <objbase.h>
#include <initguid.h>
#include <windows.h>
#include <windowsx.h>
#undef INITGUID
#include <shlobj.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlguid.h>
#include "afscreds.h"
#include "shortcut.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void Shortcut_Init (void)
{
   CoInitialize(0);
}


void Shortcut_Exit (void)
{
   CoUninitialize();
}

BOOL Shortcut_Create (LPTSTR pszTarget, LPCTSTR pszSource, LPTSTR pszDesc, LPTSTR pszArgs)
{
   IShellLink *psl;
   HRESULT rc = CoCreateInstance (CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);
   if (SUCCEEDED (rc))
      {
      IPersistFile *ppf;
      rc = psl->QueryInterface (IID_IPersistFile, (void **)&ppf);
      if (SUCCEEDED (rc))
         { 
         rc = psl->SetPath (pszSource);
         if (SUCCEEDED (rc))
            {
            rc = psl->SetDescription (pszDesc ? pszDesc : pszSource);
            if (SUCCEEDED (rc))
               {
               if ( pszArgs )
                   rc = psl->SetArguments (pszArgs);
                   if (SUCCEEDED (rc))
                   {
#ifdef UNICODE
                   rc = ppf->Save (pszTarget, TRUE);
#else
                   WORD wsz[ MAX_PATH ];
                   MultiByteToWideChar (CP_ACP, 0, pszTarget, -1, (LPWSTR)wsz, MAX_PATH);
                   rc = ppf->Save ((LPCOLESTR)wsz, TRUE);
#endif
                   }
               }
            }
         ppf->Release ();
         }
      psl->Release ();
      }
   return SUCCEEDED(rc) ? TRUE : FALSE;
} 


void Shortcut_FixStartup (LPCTSTR pszLinkName, BOOL fAutoStart)
{
   TCHAR szShortcut[ MAX_PATH + 10 ] = TEXT("");

   HKEY hk;
   if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"), &hk) == 0)
      {
      DWORD dwSize = sizeof(szShortcut);
      DWORD dwType = REG_SZ;
      RegQueryValueEx (hk, TEXT("Common Startup"), NULL, &dwType, (LPBYTE)szShortcut, &dwSize);
      if (szShortcut[0] == TEXT('\0'))
         {
         dwSize = sizeof(szShortcut);
         dwType = REG_SZ;
         RegQueryValueEx (hk, TEXT("Startup"), NULL, &dwType, (LPBYTE)szShortcut, &dwSize);
         }
      RegCloseKey (hk);
      }
   if (szShortcut[0] == TEXT('\0'))
      {
      GetWindowsDirectory (szShortcut, MAX_PATH);
      lstrcat (szShortcut, TEXT("\\Start Menu\\Programs\\Startup"));
      }
   lstrcat (szShortcut, TEXT("\\"));
   lstrcat (szShortcut, pszLinkName);

   TCHAR szSource[ MAX_PATH ];
   GetModuleFileName (GetModuleHandle(NULL), szSource, MAX_PATH);

   if (fAutoStart)
   {
       DWORD code, len, type; 
       TCHAR szParams[ 64 ] = TEXT(AFSCREDS_SHORTCUT_OPTIONS);

       code = RegOpenKeyEx(HKEY_CURRENT_USER, AFSREG_USER_OPENAFS_SUBKEY,
                            0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &hk);
       if (code == ERROR_SUCCESS) {
           len = sizeof(szParams);
           type = REG_SZ;
           code = RegQueryValueEx(hk, "AfscredsShortcutParams", NULL, &type,
                                   (BYTE *) &szParams, &len);
           RegCloseKey (hk);
       }
       if (code != ERROR_SUCCESS) {
           code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY,
                                0, (IsWow64()?KEY_WOW64_64KEY:0)|KEY_QUERY_VALUE, &hk);
           if (code == ERROR_SUCCESS) {
               len = sizeof(szParams);
               type = REG_SZ;
               code = RegQueryValueEx(hk, "AfscredsShortcutParams", NULL, &type,
                                       (BYTE *) &szParams, &len);
               RegCloseKey (hk);
           }
       }
       Shortcut_Create (szShortcut, szSource, "Autostart Authentication Agent", szParams);
   }
   else // (!g.fAutoStart)
   {
      DeleteFile (szShortcut);
   }
}

