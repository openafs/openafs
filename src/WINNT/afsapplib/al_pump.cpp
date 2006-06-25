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
 * DEFINITIONS ________________________________________________________________
 *
 */

#define cREALLOC_MODELESS 16
#define cREALLOC_WINDOWDATA_FIELDS 4
#define cREALLOC_WINDOWDATA_WINDOWS 16
#define cREALLOC_WINDOWDATA_DATA 8

#define GWD_IS_MODELESS  TEXT("afsapplib/al_pump.cpp - is window modeless?")


/*
 * VARIABLES __________________________________________________________________
 *
 */

         // Modeless-dialog support
         //
static HWND  *aModeless = NULL;
static size_t cModeless = 0;
static LPCRITICAL_SECTION pcsPump = NULL;

static void (*g_fnPump)(MSG *lpm) = NULL;

         // Window-data support
         //
static struct
   {
   TCHAR szField[ cchNAME ];
   } *aFields = NULL;
static size_t cFields = 0;

static struct
   {
   HWND hWnd;
   UINT_PTR *adwData;
   size_t cdwData;
   } *aWindows = NULL;
static size_t cWindows = 0;
static LPCRITICAL_SECTION pcsData = NULL;


/*
 * DIALOG ROUTINES ____________________________________________________________
 *
 */

BOOL CALLBACK Modeless_HookProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hDlg, Modeless_HookProc);

   if (msg == WM_DESTROY)
      {
      EnterCriticalSection (pcsPump);
      for (size_t ii = 0; ii < cModeless; ++ii)
         {
         if (aModeless[ ii ] == hDlg)
            aModeless[ ii ] = 0;
         }
      LeaveCriticalSection (pcsPump);
      Subclass_RemoveHook (hDlg, Modeless_HookProc);
      }

   if (oldProc)
      return (BOOL)CallWindowProc ((WNDPROC)oldProc, hDlg, msg, wp, lp);
   else
      return (BOOL)DefWindowProc (hDlg, msg, wp, lp);
}


BOOL AfsAppLib_IsModelessDialogMessage (MSG *lpm)
{
   BOOL rc = FALSE;

   if (pcsPump)
      {
      EnterCriticalSection (pcsPump);

      for (size_t ii = 0; (!rc) && (ii < cModeless); ++ii)
         {
         if (aModeless[ ii ] != 0)
            {
            if (IsPropSheet (aModeless[ ii ]))
               {
               if (!PropSheet_GetCurrentPageHwnd(aModeless[ ii ]))
                  DestroyWindow (aModeless[ ii ]);
               if (PropSheet_IsDialogMessage(aModeless[ ii ], lpm))
                  rc = TRUE;
               }
            else
               {
               if (IsDialogMessage (aModeless[ ii ], lpm))
                  rc = TRUE;
               }
            }
         }

      LeaveCriticalSection (pcsPump);
      }

   return rc;
}


void AfsAppLib_RegisterModelessDialog (HWND hDlg)
{
   if (pcsPump == NULL)
      {
      pcsPump = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsPump);
      }

   EnterCriticalSection (pcsPump);

   size_t ii;
   for (ii = 0; ii < cModeless; ++ii)
      {
      if (aModeless[ ii ] == hDlg)
         break;
      }
   if (ii == cModeless)
      {
      for (ii = 0; ii < cModeless; ++ii)
         {
         if (aModeless[ ii ] == 0)
            break;
         }
      if (ii == cModeless)
         {
         (void)REALLOC (aModeless, cModeless, 1+ii, cREALLOC_MODELESS);
         }
      if (ii < cModeless)
         {
         aModeless[ ii ] = hDlg;
         Subclass_AddHook (hDlg, Modeless_HookProc);
         }
      }

   LeaveCriticalSection (pcsPump);
}


void AfsAppLib_SetPumpRoutine (void (*fnPump)(MSG *lpm))
{
   g_fnPump = fnPump;
}


void AfsAppLib_MainPump (void)
{
   MSG msg;

   while (GetMessage (&msg, NULL, 0, 0))
      {
      if (AfsAppLib_IsModelessDialogMessage (&msg))
         continue;

      if (g_fnPump)
         (*g_fnPump)(&msg);
      else
         {
         TranslateMessage (&msg);
         DispatchMessage (&msg);
         }
      }
}


/*
 * WINDOW-DATA ________________________________________________________________
 *
 */

BOOL CALLBACK WindowData_HookProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
   PVOID oldProc = Subclass_FindNextHook (hWnd, WindowData_HookProc);

   if (msg == WM_DESTROY)
      {
      EnterCriticalSection (pcsData);
      for (size_t ii = 0; ii < cWindows; ++ii)
         {
         if (aWindows[ ii ].hWnd == hWnd)
            {
            if (aWindows[ ii ].adwData)
               Free (aWindows[ ii ].adwData);
            memset (&aWindows[ii], 0x00, sizeof(aWindows[ii]));
            }
         }
      LeaveCriticalSection (pcsData);
      Subclass_RemoveHook (hWnd, WindowData_HookProc);
      }

   if (oldProc)
      return (BOOL)CallWindowProc ((WNDPROC)oldProc, hWnd, msg, wp, lp);
   else
      return (BOOL)DefWindowProc (hWnd, msg, wp, lp);
}


size_t GetWindowDataField (LPTSTR pszField)
{
   size_t rc = (size_t)-1;

   if (pcsData == NULL)
      {
      pcsData = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsData);
      }
   EnterCriticalSection (pcsData);

   size_t ii;
   for (ii = 0; ii < cFields; ++ii)
      {
      if (!lstrcmpi (aFields[ ii ].szField, pszField))
         break;
      }
   if (ii == cFields)
      {
      for (ii = 0; ii < cFields; ++ii)
         {
         if (aFields[ ii ].szField[0] == TEXT('\0'))
            break;
         }
      if (ii == cFields)
         {
         (void)REALLOC (aFields, cFields, 1+ii, cREALLOC_WINDOWDATA_FIELDS);
         }
      if (ii < cFields)
         {
         lstrcpy (aFields[ ii ].szField, pszField);
         }
      }
   if (ii < cFields)
      {
      rc = ii;
      }

   LeaveCriticalSection (pcsData);
   return rc;
}


UINT_PTR GetWindowData (HWND hWnd, LPTSTR pszField)
{
   UINT_PTR rc = 0;

   if (pcsData == NULL)
      {
      pcsData = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsData);
      }
   EnterCriticalSection (pcsData);

   size_t iField;
   if ((iField = GetWindowDataField (pszField)) != (size_t)-1)
      {
      for (size_t ii = 0; ii < cWindows; ++ii)
         {
         if (aWindows[ ii ].hWnd == hWnd)
            {
            if (iField < aWindows[ ii ].cdwData)
               rc = aWindows[ ii ].adwData[ iField ];
            break;
            }
         }
      }

   LeaveCriticalSection (pcsData);
   return rc;
}


UINT_PTR SetWindowData (HWND hWnd, LPTSTR pszField, UINT_PTR dwNewData)
{
   UINT_PTR rc = 0;

   if (pcsData == NULL)
      {
      pcsData = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsData);
      }
   EnterCriticalSection (pcsData);

   size_t iField;
   if ((iField = GetWindowDataField (pszField)) != (size_t)-1)
      {
      size_t ii;
      for (ii = 0; ii < cWindows; ++ii)
         {
         if (aWindows[ ii ].hWnd == hWnd)
            {
            if (iField < aWindows[ ii ].cdwData)
               rc = aWindows[ ii ].adwData[ iField ];
            break;
            }
         }
      if (ii == cWindows)
         {
         for (ii = 0; ii < cWindows; ++ii)
            {
            if (aWindows[ ii ].hWnd == 0)
               break;
            }
         if (ii == cWindows)
            {
            (void)REALLOC (aWindows, cWindows, 1+ii, cREALLOC_WINDOWDATA_WINDOWS);
            }
         }
      if (ii < cWindows)
         {
         if (aWindows[ ii ].hWnd == 0)
            {
            aWindows[ ii ].hWnd = hWnd;
            Subclass_AddHook (hWnd, WindowData_HookProc);
            }
         if ((dwNewData) && (iField >= aWindows[ ii ].cdwData))
            {
            (void)REALLOC (aWindows[ ii ].adwData, aWindows[ ii ].cdwData, 1+iField, cREALLOC_WINDOWDATA_DATA);
            }
         if (iField < aWindows[ ii ].cdwData)
            {
            aWindows[ ii ].adwData[ iField ] = dwNewData;
            }
         }
      }

   LeaveCriticalSection (pcsData);
   return rc;
}

