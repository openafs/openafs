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

#include "TaAfsUsrMgr.h"
#include "winlist.h"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef struct
   {
   HWND hWnd;
   WINDOWLISTTYPE wlt;
   ASID idObject;
   } WINDOWLISTENTRY, *LPWINDOWLISTENTRY;

static WINDOWLISTENTRY *aWindowList = NULL;
static size_t cWindowList = 0;

#define cREALLOC_WINDOWLIST  4


/*
 * ROUTINES ___________________________________________________________________
 *
 */

void WindowList_Add (HWND hWnd, WINDOWLISTTYPE wlt, ASID idObject)
{
   // See if this window is already in the list
   //
   size_t ii;
   for (ii = 0; ii < cWindowList; ++ii)
      {
      if (aWindowList[ ii ].hWnd == hWnd)
         return;
      }

   // Add this window to the list
   //
   for (ii = 0; ii < cWindowList; ++ii)
      {
      if (!aWindowList[ ii ].hWnd)
         break;
      }
   if (REALLOC (aWindowList, cWindowList, 1+ii, cREALLOC_WINDOWLIST))
      {
      aWindowList[ ii ].hWnd = hWnd;
      aWindowList[ ii ].wlt = wlt;
      aWindowList[ ii ].idObject = idObject;
      AfsAppLib_RegisterModelessDialog (hWnd);
      }
}


HWND WindowList_Search (WINDOWLISTTYPE wlt, ASID idObject)
{
   for (size_t ii = 0; ii < cWindowList; ++ii)
      {
      if (!aWindowList[ ii ].hWnd)
         continue;
      if (aWindowList[ ii ].wlt != wlt)
         continue;
      if ((idObject != ASID_ANY) && (aWindowList[ ii ].idObject != idObject))
         continue;
      return aWindowList[ ii ].hWnd;
      }
   return NULL;
}


void WindowList_Remove (HWND hWnd)
{
   for (size_t ii = 0; ii < cWindowList; ++ii)
      {
      if (aWindowList[ ii ].hWnd == hWnd)
         aWindowList[ ii ].hWnd = NULL;
      }
}

