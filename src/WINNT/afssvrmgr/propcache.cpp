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
#include "propcache.h"


/*
 * PROPERTIES-DIALOG CACHE ____________________________________________________
 *
 */

typedef struct
   {
   BOOL fInUse;
   PropCache pcType;
   PVOID pv;
   HWND hDialog;
   } PropCacheEntry;

static size_t PropCache_nEntries = 0;
static PropCacheEntry *PropCache_apce = NULL;

void PropCache_Add (PropCache pcType, PVOID pv, HWND hDialog)
{
   if (!PropCache_Search (pcType, pv))
      {
      size_t iEntry;
      for (iEntry = 0; iEntry < PropCache_nEntries; ++iEntry)
         {
         if (!PropCache_apce[ iEntry ].fInUse)
            break;
         }

      if (iEntry == PropCache_nEntries)
         {
         if (!REALLOC (PropCache_apce, PropCache_nEntries, 1+iEntry, 16))
            return;
         }

      PropCache_apce[ iEntry ].fInUse = TRUE;
      PropCache_apce[ iEntry ].pcType = pcType;
      PropCache_apce[ iEntry ].pv = pv;
      PropCache_apce[ iEntry ].hDialog = hDialog;

      if (pcType != pcSERVER)
         AfsAppLib_RegisterModelessDialog (hDialog);
      }
}


HWND PropCache_Search (PropCache pcType, PVOID pv, HWND hwndStart)
{
   size_t iEntry = 0;

   if (hwndStart != NULL)
      {
      for ( ; iEntry < PropCache_nEntries; ++iEntry)
         {
         if (!PropCache_apce[ iEntry ].fInUse)
            continue;
         if (PropCache_apce[ iEntry ].hDialog == hwndStart)
            {
            ++iEntry;
            break;
            }
         }
      }

   for ( ; iEntry < PropCache_nEntries; ++iEntry)
      {
      if (!PropCache_apce[ iEntry ].fInUse)
         continue;

      if ( (PropCache_apce[ iEntry ].pcType == pcType) &&
           ((pv == ANYVALUE) || (PropCache_apce[ iEntry ].pv == pv)) )
         {
         if (!IsWindow (PropCache_apce[ iEntry ].hDialog))
            {
            PropCache_apce[ iEntry ].fInUse = FALSE;
            continue;
            }

         return PropCache_apce[ iEntry ].hDialog;
         }
      }

   return NULL;
}


void PropCache_Delete (PropCache pcType, PVOID pv)
{
   for (size_t iEntry = 0; iEntry < PropCache_nEntries; ++iEntry)
      {
      if (!PropCache_apce[ iEntry ].fInUse)
         continue;

      if ( (PropCache_apce[ iEntry ].pcType == pcType) &&
           ((pv == ANYVALUE) || (PropCache_apce[ iEntry ].pv == pv)) )
         {
         PropCache_apce[ iEntry ].fInUse = FALSE;
         }
      }
}


void PropCache_Delete (HWND hDialog)
{
   for (size_t iEntry = 0; iEntry < PropCache_nEntries; ++iEntry)
      {
      if (!PropCache_apce[ iEntry ].fInUse)
         continue;

      if (PropCache_apce[ iEntry ].hDialog == hDialog)
         {
         PropCache_apce[ iEntry ].fInUse = FALSE;
         }
      }
}

