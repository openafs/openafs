/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <windows.h>
#include <cpl.h>
#include <WINNT/TaLocale.h>
#include <WINNT/afsreg.h>
#include "cpl_interface.h"
#include "resource.h"


static HINSTANCE hinst = 0;
static HINSTANCE hinstResources = 0;


static BOOL IsWindowsNT (void)
{
   static BOOL fChecked = FALSE;
   static BOOL fIsWinNT = FALSE;

   if (!fChecked)
      {
      OSVERSIONINFO Version;
      memset (&Version, 0x00, sizeof(Version));
      Version.dwOSVersionInfoSize = sizeof(Version);

      if (GetVersionEx (&Version))
         {
         if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT)
            fIsWinNT = TRUE;
         }

      fChecked = TRUE;
      }

   return fIsWinNT;
}


static BOOL IsClientInstalled (void)
{
   static BOOL fChecked = FALSE;
   static BOOL fIsInstalled = FALSE;

   if (!fChecked)
      {
      HKEY hk;
      if (RegOpenKey (HKEY_LOCAL_MACHINE, TEXT(AFSREG_CLT_SW_VERSION_SUBKEY), &hk) == 0)
         {
         TCHAR szPath[ MAX_PATH ];
         DWORD dwSize = sizeof(szPath);
         DWORD dwType = REG_SZ;
         if (RegQueryValueEx (hk, TEXT(AFSREG_CLT_SW_VERSION_DIR_VALUE), 
                              NULL, &dwType, (PBYTE)szPath, &dwSize) == 0)
            fIsInstalled = TRUE;
         RegCloseKey (hk);
         }
      fChecked = TRUE;
      }

   return fIsInstalled;
}


extern "C" LONG APIENTRY CPlApplet(HWND hwndCPl, UINT uMsg, LONG lParam1, LONG lParam2)
{
    LPNEWCPLINFO lpNewCPlInfo;
    LPCPLINFO lpCPlInfo;

    switch (uMsg) {
        case CPL_INIT:      /* first message, sent once  */
            hinst = GetModuleHandle("afs_cpa.cpl");
            hinstResources = TaLocale_LoadCorrespondingModule (hinst);
            return (hinst != 0);

        case CPL_GETCOUNT:  /* second message, sent once */
            return 1;
            break;

        case CPL_INQUIRE:  /* in case we receive this we should indicate that we like NEWINQUIRE better. */
			lpCPlInfo = (CPLINFO *) lParam2;
			lpCPlInfo->idIcon = ((IsClientInstalled() || !IsWindowsNT())? IDI_AFSD : IDI_CCENTER);
			lpCPlInfo->idName = CPL_DYNAMIC_RES;
			lpCPlInfo->idInfo = CPL_DYNAMIC_RES;
			lpCPlInfo->lData = 0;
			break;

        case CPL_NEWINQUIRE: /* third message, sent once per app */
            lpNewCPlInfo = (LPNEWCPLINFO) lParam2;

            lpNewCPlInfo->dwSize = (DWORD) sizeof(NEWCPLINFO);
            lpNewCPlInfo->dwFlags = 0;
            lpNewCPlInfo->dwHelpContext = 0;
            lpNewCPlInfo->lData = 0;
	    if (IsClientInstalled() || !IsWindowsNT())
	       lpNewCPlInfo->hIcon = TaLocale_LoadIcon(IDI_AFSD);
	    else
	       lpNewCPlInfo->hIcon = TaLocale_LoadIcon(IDI_CCENTER);
            lpNewCPlInfo->szHelpFile[0] = '\0';

            GetString (lpNewCPlInfo->szName, (!IsWindowsNT()) ? IDS_CPL_NAME_95 : (!IsClientInstalled()) ? IDS_CPL_NAME_CCENTER : IDS_CPL_NAME_NT);
            GetString (lpNewCPlInfo->szInfo, (!IsWindowsNT()) ? IDS_CPL_DESC_95 : (!IsClientInstalled()) ? IDS_CPL_DESC_CCENTER : IDS_CPL_DESC_NT);
            break;

        case CPL_DBLCLK:		/* applet icon double-clicked */
	    if (IsClientInstalled() || !IsWindowsNT())
	        WinExec("afs_config.exe", SW_SHOW);
	    else
	        WinExec("afs_config.exe /c", SW_SHOW);
            break;

        case CPL_EXIT:
            if (hinstResources)
                FreeLibrary (hinstResources);
            break;
    }

    return 0;
}


