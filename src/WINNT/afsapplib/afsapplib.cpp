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

#include <WINNT/afsapplib.h>


/*
 * VARIABLES __________________________________________________________________
 *
 */

static HWND g_hMain = NULL;
static TCHAR g_szAppName[ cchNAME ] = TEXT("");


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

extern void OnCoverWindow (WPARAM wp, LPARAM lp);
extern void OnExpiredCredentials (WPARAM wp, LPARAM lp);
extern void OnCreateErrorDialog (WPARAM wp, LPARAM lp);

BOOL CALLBACK AfsAppLib_MainHook (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void AfsAppLib_SetAppName (LPTSTR pszName)
{
   lstrcpy (g_szAppName, pszName);
}


void AfsAppLib_GetAppName (LPTSTR pszName)
{
   lstrcpy (pszName, g_szAppName);
}


void AfsAppLib_SetMainWindow (HWND hMain)
{
   if (g_hMain != NULL)
      Subclass_RemoveHook (g_hMain, AfsAppLib_MainHook);

   if ((g_hMain = hMain) != NULL)
      Subclass_AddHook (g_hMain, AfsAppLib_MainHook);
}


HWND AfsAppLib_GetMainWindow (void)
{
   return g_hMain;
}


BOOL CALLBACK AfsAppLib_MainHook (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hWnd, AfsAppLib_MainHook);

   switch (msg)
      {
      case WM_COVER_WINDOW:
         OnCoverWindow (wp, lp);
         break;

      case WM_EXPIRED_CREDENTIALS:
         OnExpiredCredentials (wp, lp);
         break;

      case WM_CREATE_ERROR_DIALOG:
         OnCreateErrorDialog (wp, lp);
         break;

      case WM_DESTROY:
         AfsAppLib_SetMainWindow (NULL);
         break;
      }

   if (oldProc)
      return (CallWindowProc ((WNDPROC)oldProc, hWnd, msg, wp, lp)==NULL?FALSE:TRUE);
   else
      return (DefWindowProc (hWnd, msg, wp, lp)==NULL?FALSE:TRUE);
}

