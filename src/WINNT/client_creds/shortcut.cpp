extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <objbase.h>
#include <initguid.h>
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
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


BOOL Shortcut_Create (LPTSTR pszTarget, LPCTSTR pszSource, LPTSTR pszDesc)
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
#ifdef UNICODE
               rc = ppf->Save (pszTarget, TRUE);
#else
               WORD wsz[ MAX_PATH ];
               MultiByteToWideChar (CP_ACP, 0, pszTarget, -1, wsz, MAX_PATH);
               rc = ppf->Save (wsz, TRUE);
#endif
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
   TCHAR szShortcut[ MAX_PATH ] = TEXT("");

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
      Shortcut_Create (szShortcut, szSource);
      }
   else // (!g.fAutoStart)
      {
      DeleteFile (szShortcut);
      }
}

