/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * INCLUDES _________________________________________________________________
 *
 */
#include <windows.h>
#include <cpl.h>
#include <stdio.h>
#include "cpl_interface.h"
#include "resource.h"
#include <WINNT/afsreg.h>
#include <WINNT/TaLocale.h>



/*
 * DEFINITIONS _______________________________________________________________
 *
 */
#define APP_INSTALL_DIR_REG_SUBKEY	AFSREG_SVR_SW_VERSION_SUBKEY
#define APP_INSTALL_DIR_REG_VALUE	AFSREG_SVR_SW_VERSION_DIR_VALUE
#define	APP_EXE_PATH			"\\usr\\afs\\bin\\afssvrcfg.exe"



/*
 * VARIABLES _________________________________________________________________
 *
 */
static HINSTANCE hinst = 0;
static HINSTANCE hinstResources = 0;



/*
 * STATIC FUNCTIONS ___________________________________________________________
 *
 */
static char *LoadResString(UINT uID)
{
	static char str[256];
	GetString (str, uID);
	return str;
}

static char *GetInstallDir()
{
	HKEY hKey;
	LONG nResult;
	DWORD dwType;
	static char szInstallDir[256];
	DWORD dwSize;

	dwSize = sizeof(szInstallDir);

	nResult = RegOpenKeyAlt(HKEY_LOCAL_MACHINE, APP_INSTALL_DIR_REG_SUBKEY, KEY_READ, FALSE, &hKey, 0);
	if (nResult == ERROR_SUCCESS) {
		nResult = RegQueryValueEx(hKey, APP_INSTALL_DIR_REG_VALUE, 0, &dwType, (PBYTE)szInstallDir, &dwSize);
		RegCloseKey(hKey);
	}

	if (nResult != ERROR_SUCCESS)
		szInstallDir[0] = 0;

	return szInstallDir;
}



/*
 * EXPORTED FUNCTIONS _________________________________________________________
 *
 */
extern "C" LONG APIENTRY CPlApplet(HWND hwndCPl, UINT uMsg, LONG lParam1, LONG lParam2)
{
    int i;
    LPNEWCPLINFO lpNewCPlInfo;
	HICON hIcon;
	static char szAppName[64];
	static char szAppPath[MAX_PATH];


    i = (int)lParam1;

    switch (uMsg) {
        case CPL_INIT:      /* first message, sent once  */
            hinst = GetModuleHandle("afsserver.cpl");
            hinstResources = TaLocale_LoadCorrespondingModule (hinst);
			strcpy(szAppName, LoadResString(IDS_APP_NAME));
			sprintf(szAppPath, "%s%s", GetInstallDir(), APP_EXE_PATH);
			return (hinst != 0);

        case CPL_GETCOUNT:  /* second message, sent once */
            return 1;
            break;

        case CPL_NEWINQUIRE: /* third message, sent once per app */
            lpNewCPlInfo = (LPNEWCPLINFO) lParam2;

            lpNewCPlInfo->dwSize = (DWORD) sizeof(NEWCPLINFO);
            lpNewCPlInfo->dwFlags = 0;
            lpNewCPlInfo->dwHelpContext = 0;
            lpNewCPlInfo->lData = 0;
            hIcon = TaLocale_LoadIcon(IDI_AFSD);
			if (hIcon == 0)
				MessageBox(0, LoadResString(IDS_ERROR_LOADING_ICON), szAppName, MB_ICONEXCLAMATION);
			lpNewCPlInfo->hIcon = hIcon;
            lpNewCPlInfo->szHelpFile[0] = '\0';
			strcpy(lpNewCPlInfo->szName, szAppName);
			strcpy(lpNewCPlInfo->szInfo, LoadResString(IDS_CPA_TITLE));
            break;

        case CPL_SELECT:		/* applet icon selected */
            break;

        case CPL_DBLCLK:		/* applet icon double-clicked */
			if (WinExec(szAppPath, SW_SHOW) < 32)
				MessageBox(0, LoadResString(IDS_EXECUTION_ERROR), szAppName, MB_ICONSTOP);
            break;

        case CPL_STOP:      /* sent once per app. before CPL_EXIT */
            break;

        case CPL_EXIT:    /* sent once before FreeLibrary called */
            if (hinstResources)
                FreeLibrary (hinstResources);
            break;

        default:
            break;
    }

    return 0;
}

