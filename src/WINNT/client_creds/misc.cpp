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

#include "afscreds.h"


/*
 * REALLOC ____________________________________________________________________
 *
 */

BOOL AfsCredsReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = Allocate (cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget != 0)
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      Free (*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}


/*
 * REGISTRY SETTINGS __________________________________________________________
 *
 */

void LoadRemind (size_t iCreds)
{
   g.aCreds[ iCreds ].fRemind = TRUE;

   HKEY hk;
   if (RegOpenKey (HKEY_CURRENT_USER, AFSREG_USER_OPENAFS_SUBKEY "\\Reminders", &hk) == 0)
      {
      DWORD dwValue = 1;
      DWORD dwSize = sizeof(dwValue);
      DWORD dwType = REG_DWORD;
      if (RegQueryValueEx (hk, g.aCreds[ iCreds ].szCell, NULL, &dwType, (PBYTE)&dwValue, &dwSize) == 0)
         g.aCreds[ iCreds ].fRemind = dwValue;
      RegCloseKey (hk);
      }
}


void SaveRemind (size_t iCreds)
{
   HKEY hk;
   if (RegCreateKey (HKEY_CURRENT_USER, AFSREG_USER_OPENAFS_SUBKEY "\\Reminders", &hk) == 0)
      {
      DWORD dwValue = g.aCreds[ iCreds ].fRemind;
      RegSetValueEx (hk, g.aCreds[ iCreds ].szCell, NULL, REG_DWORD, (PBYTE)&dwValue, sizeof(DWORD));
      RegCloseKey (hk);
      }
}


void TimeToSystemTime (SYSTEMTIME *pst, time_t TimeT)
{
   memset (pst, 0x00, sizeof(SYSTEMTIME));

   struct tm *pTime;
   if ((pTime = localtime (&TimeT)) != NULL)
      {
      pst->wYear = pTime->tm_year + 1900;
      pst->wMonth = pTime->tm_mon + 1;
      pst->wDayOfWeek = pTime->tm_wday;
      pst->wDay = pTime->tm_mday;
      pst->wHour = pTime->tm_hour;
      pst->wMinute = pTime->tm_min;
      pst->wSecond = pTime->tm_sec;
      pst->wMilliseconds = 0;
      }
}


LPARAM GetTabParam (HWND hTab, int iTab)
{
   TC_ITEM Item;
   memset (&Item, 0x00, sizeof(Item));
   Item.mask = TCIF_PARAM;
   if (!TabCtrl_GetItem (hTab, iTab, &Item))
      return NULL;
   return Item.lParam;
}


HWND GetTabChild (HWND hTab)
{
   for (HWND hChild = GetWindow (hTab, GW_CHILD);
        hChild != NULL;
        hChild = GetWindow (hChild, GW_HWNDNEXT))
      {
      TCHAR szClassName[ cchRESOURCE ];

      if (GetClassName (hChild, szClassName, cchRESOURCE))
         {
         if (!lstrcmp (szClassName, TEXT("#32770"))) // WC_DIALOG
            return hChild;
         }
      }

   return NULL;
}

