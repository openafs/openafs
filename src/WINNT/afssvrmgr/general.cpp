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

   for (size_t ii = 0; !lpdw && ii < cWindowList; ++ii)
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

