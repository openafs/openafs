/*
 * Copyright (C) 1997  Transarc Corporation.
 * All rights reserved.
 *
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "stdafx.h"
#include "help.h"


static CString strHelpPath;


static BOOL IsWindowsNT (void)
{
	static BOOL fChecked = FALSE;
	static BOOL fIsWinNT = FALSE;

	if (!fChecked) {
		fChecked = TRUE;

		OSVERSIONINFO Version;
		memset (&Version, 0x00, sizeof(Version));
		Version.dwOSVersionInfoSize = sizeof(Version);

		if (GetVersionEx (&Version)) {
			if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT)
				fIsWinNT = TRUE;
		}
	}

	return fIsWinNT;
}



void SetHelpPath(const char *pszDefaultHelpFilePath)
{
	CString str = pszDefaultHelpFilePath;
	int nIndex = str.ReverseFind('\\');
	ASSERT(nIndex >= 0);

	if (IsWindowsNT())
		strHelpPath = str.Left(nIndex + 1) + HELPFILE_NATIVE;
	else
		strHelpPath = str.Left(nIndex + 1) + HELPFILE_LIGHT;
}

void ShowHelp(HWND hWnd, DWORD nHelpID)
{
	::WinHelp(hWnd, strHelpPath, HELPTYPE, nHelpID);
}

