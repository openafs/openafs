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

#include <WINNT/afsclass.h>
#include "internal.h"

#define LOCAL_CRITSEC_COUNT


/*
 * VARIABLES __________________________________________________________________
 *
 */

#ifdef LOCAL_CRITSEC_COUNT
size_t cs_EnterCount = 0;
DWORD cs_ThreadID = 0;
#endif

size_t cRefreshAllReq = 0;

CRITICAL_SECTION *pcs = NULL;
BOOL fLongServerNames = FALSE;

DWORD dwWant = 0;

static LPTSTR apszDays[] = {
   TEXT("sun"), TEXT("mon"), TEXT("tue"),
   TEXT("wed"), TEXT("thu"), TEXT("fri"), TEXT("sat")
};


/*
 * PROTOTYPES _________________________________________________________________
 *
 */


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void AfsClass_InitCriticalSection (void)
{
   if (pcs == NULL)
      {
      pcs = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcs);

#ifdef LOCAL_CRITSEC_COUNT
      cs_EnterCount = 0;
      cs_ThreadID = 0;
#endif
      }
}

LPCRITICAL_SECTION AfsClass_GetCriticalSection (void)
{
   AfsClass_InitCriticalSection();
   return pcs;
}

void AfsClass_Enter (void)
{
   AfsClass_InitCriticalSection();
   EnterCriticalSection (pcs);

#ifdef LOCAL_CRITSEC_COUNT
   cs_EnterCount++;
   cs_ThreadID = GetCurrentThreadId();
#endif
}

void AfsClass_Leave (void)
{
#ifdef LOCAL_CRITSEC_COUNT
   if (!ASSERT( cs_EnterCount > 0 ))
      return;
   if (!ASSERT( cs_ThreadID == GetCurrentThreadId() ))
      return;
   cs_EnterCount--;
#else
   if (!ASSERT( pcs->RecursionCount > 0 ))
      return;
   if (!ASSERT( pcs->OwningThread == (PVOID)GetCurrentThreadId() ))
      return;
#endif
   LeaveCriticalSection (pcs);
}

int AfsClass_GetEnterCount (void)
{
#ifdef LOCAL_CRITSEC_COUNT
   return (int)cs_EnterCount;
#else
   return pcs->RecursionCount;
#endif
}

void AfsClass_UnixTimeToSystemTime (LPSYSTEMTIME pst, ULONG ut, BOOL fElapsed)
{
   if (!ut)
      {
      memset (pst, 0x00, sizeof(SYSTEMTIME));
      return;
      }

   // A Unix time is the number of seconds since 1/1/1970.
   // The first step in this conversion is to change that count-of-seconds
   // into a count-of-100ns-intervals...
   //
   LARGE_INTEGER ldw;
   ldw.QuadPart = (LONGLONG)10000000 * (LONGLONG)ut;

   // Then adjust the count to be a count-of-100ns-intervals since
   // 1/1/1601, instead of 1/1/1970. That means adding a *big* number...
   //
   ldw.QuadPart += (LONGLONG)0x019DB1DED53E8000;

   // Now the count is effectively a FILETIME, which we can convert
   // to a SYSTEMTIME with a Win32 API.
   //
   FILETIME ft;
   ft.dwHighDateTime = (DWORD)ldw.HighPart;
   ft.dwLowDateTime = (DWORD)ldw.LowPart;
   FileTimeToSystemTime (&ft, pst);

   if (fElapsed)
      {
      pst->wYear -= 1970;
      pst->wMonth -= 1;
      pst->wDayOfWeek -= 1;
      pst->wDay -= 1;
      }
}


ULONG AfsClass_SystemTimeToUnixTime (LPSYSTEMTIME pst)
{
   SYSTEMTIME st;
   if (pst == NULL)
      GetSystemTime (&st);
   else
      st = *pst;

   if (st.wYear == 1970)
      return 0;

   FILETIME ft;
   if (!SystemTimeToFileTime (&st, &ft))
      return 0;

   LARGE_INTEGER now;
   now.HighPart = (LONG)ft.dwHighDateTime;
   now.LowPart = (ULONG)ft.dwLowDateTime;

   LARGE_INTEGER offset;
   offset.HighPart = 0x019db1de;
   offset.LowPart = 0xd53e8000;

   LARGE_INTEGER result;
   result.QuadPart = (now.QuadPart - offset.QuadPart) / 10000000;
   return (ULONG)result.LowPart;
}


void AfsClass_ElapsedTimeToSeconds (ULONG *pcSeconds, LPSYSTEMTIME pet)
{
   *pcSeconds = 0;
   if (pet)
      {
      *pcSeconds += pet->wSecond;
      *pcSeconds += pet->wMinute * 60L;
      *pcSeconds += pet->wHour   * 60L * 60L;
      *pcSeconds += pet->wDay    * 60L * 60L * 24L;
      }
}


double AfsClass_FileTimeToDouble (FILETIME *pft)
{
   double rc;
   rc = (double)pft->dwHighDateTime * (double)0x100000000;
   rc += (double)pft->dwLowDateTime;
   return rc;
}


void AfsClass_RequestLongServerNames (BOOL fLong)
{
   fLongServerNames = fLong;
}


void AfsClass_ParseRecurringTime (BOOL *pfEver, LPSYSTEMTIME pst, LPTSTR pszTime)
{
   *pfEver = FALSE;
   memset (pst, 0x00, sizeof(SYSTEMTIME));

   if (lstrcmpi (pszTime, TEXT("never")))
      {
      *pfEver = TRUE;

      if (!lstrncmpi (pszTime, TEXT("at"), lstrlen(TEXT("at"))))
         pszTime += lstrlen(TEXT("at"));
      while ((*pszTime == ' ') || (*pszTime == '\t'))
         ++pszTime;

      // first, does it start with a day-of-week?
      //
      pst->wDayOfWeek = (WORD)(-1); // daily until proven otherwise

      for (size_t ii = 0; ii < 7; ++ii)
         {
         if (!lstrncmpi (apszDays[ii], pszTime, 3))
            {
            pst->wDayOfWeek = ii;
            break;
            }
         }

      // next, find the hour
      //
      while (*pszTime && !isdigit(*pszTime))
         ++pszTime;

      TCHAR szComponent[ cchRESOURCE ];
      lstrcpy (szComponent, pszTime);
      LPTSTR pch;
      for (pch = szComponent; isdigit(*pch); ++pch)
         ;
      *pch = TEXT('\0');

      pst->wHour = atoi(szComponent);

      // next, find the minutes
      //
      while (isdigit(*pszTime))
         ++pszTime;
      while (*pszTime && !isdigit(*pszTime))
         ++pszTime;

      lstrcpy (szComponent, pszTime);
      for (pch = szComponent; isdigit(*pch); ++pch)
         ;
      *pch = TEXT('\0');

      pst->wMinute = atoi(szComponent);

      // next, check for a 'am' or 'pm' marker
      //
      for ( ; *pszTime; ++pszTime)
         {
         if (tolower(*pszTime) == TEXT('p'))
            {
            if (pst->wHour < 12)
               pst->wHour += 12;
            break;
            }
         if (tolower(*pszTime) == TEXT('a'))
            {
            if (pst->wHour >= 12)
               pst->wHour -= 12;
            break;
            }
         }
      }
}


void AfsClass_FormatRecurringTime (LPTSTR pszTarget, SYSTEMTIME *pst)
{
   if (pst == NULL)
      {
      wsprintf (pszTarget, TEXT("never"));
      }
   else
      {
      WORD wHour = ((pst->wHour % 12) == 0) ? 12 : (pst->wHour % 12);
      TCHAR szAMPM[3];
      lstrcpy (szAMPM, (pst->wHour >= 12) ? TEXT("pm") : TEXT("am"));

      if (pst->wDayOfWeek == (WORD)(-1))  // daily at specified time?
         {
         wsprintf (pszTarget, TEXT("%u:%02u %s"), wHour, pst->wMinute, szAMPM);
         }
      else // weekly on specified date at specified time?
         {
         wsprintf (pszTarget, TEXT("%s %u:%02u %s"), apszDays[ pst->wDayOfWeek ], wHour, pst->wMinute, szAMPM);
         }
      }
}


void AfsClass_SplitFilename (LPSTR pszDirectoryA, LPSTR pszFilenameA, LPTSTR pszFullName)
{
   CopyStringToAnsi (pszDirectoryA, pszFullName);

   LPSTR pszLastSlashA = NULL;
   for (LPSTR pszA = pszDirectoryA; *pszA; ++pszA)
      {
      if ((*pszA == '/') || (*pszA == '\\'))
         pszLastSlashA = pszA;
      }

   if (!pszLastSlashA)
      {
      strcpy (pszFilenameA, pszDirectoryA);
      *pszDirectoryA = 0;
      }
   else
      {
      strcpy (pszFilenameA, 1+pszLastSlashA);
      *pszLastSlashA = 0;
      }
}


void AfsClass_SystemTimeToRestartTime (bos_RestartTime_p prt, BOOL fEnable, LPSYSTEMTIME pst)
{
   memset (prt, 0x00, sizeof(bos_RestartTime_t));
   if (!fEnable)
      prt->mask = BOS_RESTART_TIME_NEVER;
   else
      {
      prt->mask = (bos_RestartTimeFields_t)(BOS_RESTART_TIME_HOUR | BOS_RESTART_TIME_MINUTE);
      prt->hour = pst->wHour;
      prt->min = pst->wMinute;

      if (pst->wDayOfWeek != (WORD)-1)
         {
         prt->mask = (bos_RestartTimeFields_t)(prt->mask | BOS_RESTART_TIME_DAY);
         prt->day = pst->wDayOfWeek;
         }
      }
}


void AfsClass_RestartTimeToSystemTime (BOOL *pfEnable, LPSYSTEMTIME pst, bos_RestartTime_p prt)
{
   memset (pst, 0x00, sizeof(SYSTEMTIME));

   if ((!prt->mask) || (prt->mask & BOS_RESTART_TIME_NEVER))
      {
      *pfEnable = FALSE;
      }
   else
      {
      *pfEnable = TRUE;
      pst->wHour = (prt->mask & BOS_RESTART_TIME_HOUR) ? prt->hour : 0;
      pst->wMinute = (prt->mask & BOS_RESTART_TIME_MINUTE) ? prt->min : 0;
      pst->wDayOfWeek = (prt->mask & BOS_RESTART_TIME_DAY) ? prt->day : (WORD)-1;
      }
}


void AfsClass_IntToAddress (LPSOCKADDR_IN pAddr, int IntAddr)
{
   memset (pAddr, 0x00, sizeof(SOCKADDR_IN));
   pAddr->sin_family = AF_INET;
   pAddr->sin_addr.s_addr = htonl(IntAddr);
}


void AfsClass_AddressToInt (int *pIntAddr, LPSOCKADDR_IN pAddr)
{
   *pIntAddr = ntohl(pAddr->sin_addr.s_addr);
}


void AfsClass_SpecifyRefreshDomain (DWORD dwWantUser)
{
   dwWant = dwWantUser;
}


BOOL AfsClass_Initialize (ULONG *pStatus)
{
   if (!Worker_Initialize (pStatus))
      return FALSE;

   return TRUE;
}


void AfsClass_GenFullUserName (LPTSTR pszTarget, LPCTSTR pszPrincipal, LPCTSTR pszInstance)
{
   lstrcpy (pszTarget, pszPrincipal);
   if (pszInstance && *pszInstance)
      wsprintf (&pszTarget[ lstrlen(pszTarget) ], TEXT(".%s"), pszInstance);
}

