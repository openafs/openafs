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

#include "svrmgr.h"
#include "general.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */

/*
 *** InterlockedIncrementByWindow
 *** InterlockedDecrementByWindow
 *
 * Associates a zero-initialized LONG with an HWND, and calls Interlocked*()
 * on that LONG.
 *
 */

#define cREALLOC_WINDOWLIST 16

static struct
   {
   HWND hWnd;
   LONG dw;
   } *aWindowList = NULL;

static size_t cWindowList = 0;
static LPCRITICAL_SECTION pcsWindowList = NULL;

LONG *FindLongByWindow (HWND hWnd)
{
   LONG *lpdw = NULL;

   if (pcsWindowList == NULL)
      {
      pcsWindowList = New (CRITICAL_SECTION);
      InitializeCriticalSection (pcsWindowList);
      }

   EnterCriticalSection (pcsWindowList);

   size_t ii;
   for (ii = 0; !lpdw && ii < cWindowList; ++ii)
      {
      if (aWindowList[ ii ].hWnd == hWnd)
         lpdw = &aWindowList[ ii ].dw;
      }
   if (ii == cWindowList)
      {
      if (REALLOC( aWindowList, cWindowList, 1+ii, cREALLOC_WINDOWLIST ))
         {
         aWindowList[ ii ].hWnd = hWnd;
         aWindowList[ ii ].dw = 0;
         lpdw = &aWindowList[ ii ].dw;
         }
      }

   LeaveCriticalSection (pcsWindowList);
   return lpdw;
}

LONG InterlockedIncrementByWindow (HWND hWnd)
{
   LONG *lpdw;
   if ((lpdw = FindLongByWindow (hWnd)) == NULL)
      return 0;
   return InterlockedIncrement (lpdw);
}

LONG InterlockedDecrementByWindow (HWND hWnd)
{
   LONG *lpdw;
   if ((lpdw = FindLongByWindow (hWnd)) == NULL)
      return 0;
   return InterlockedDecrement (lpdw);
}

