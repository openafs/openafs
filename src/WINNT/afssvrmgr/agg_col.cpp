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
#include "agg_col.h"


/*
 * AGGREGATE-VIEW COLUMNS _____________________________________________________
 *
 */

void Aggregates_SetDefaultView (LPVIEWINFO lpvi)
{
   lpvi->lvsView = FLS_VIEW_LIST;
   lpvi->nColsAvail = nAGGREGATECOLUMNS;

   for (size_t iCol = 0; iCol < nAGGREGATECOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = AGGREGATECOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = AGGREGATECOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = aggcolNAME;

   lpvi->nColsShown = 7;
   lpvi->aColumns[0] = (int)aggcolNAME;
   lpvi->aColumns[1] = (int)aggcolID;
   lpvi->aColumns[2] = (int)aggcolDEVICE;
   lpvi->aColumns[3] = (int)aggcolUSED;
   lpvi->aColumns[4] = (int)aggcolTOTAL;
   lpvi->aColumns[5] = (int)aggcolUSED_PER;
   lpvi->aColumns[6] = (int)aggcolSTATUS;
}


size_t Aggregates_GetAlertCount (LPAGGREGATE lpAggregate)
{
   return Alert_GetCount (lpAggregate->GetIdentifier());
}


LPTSTR Aggregates_GetColumnText (LPIDENT lpi, AGGREGATECOLUMN aggcol, BOOL fShowServerName)
{
   static TCHAR aszBuffer[ nAGGREGATECOLUMNS ][ cchRESOURCE ];
   static size_t iBufferNext = 0;
   LPTSTR pszBuffer = aszBuffer[ iBufferNext++ ];
   if (iBufferNext == nAGGREGATECOLUMNS)
      iBufferNext = 0;
   pszBuffer[0] = TEXT('\0');

   LPAGGREGATE_PREF lpap;
   LPAGGREGATESTATUS lpas = NULL;
   LPTSTR pszDevice = NULL;
   if ((lpap = (LPAGGREGATE_PREF)lpi->GetUserParam()) != NULL)
      {
      lpas = &lpap->asLast;
      pszDevice = lpap->szDevice;
      }

   switch (aggcol)
      {
      case aggcolNAME:
         if (!fShowServerName)
            lpi->GetAggregateName (pszBuffer);
         else
            {
            TCHAR szNameSvr[ cchNAME ];
            TCHAR szNameAgg[ cchNAME ];
            lpi->GetServerName (szNameSvr);
            lpi->GetAggregateName (szNameAgg);
            LPTSTR pszName = FormatString (IDS_SERVER_AGGREGATE, TEXT("%s%s"), szNameSvr, szNameAgg);
            lstrcpy (pszBuffer, pszName);
            FreeString (pszName);
            }
         break;

      case aggcolID:
         if (lpas)
            wsprintf (pszBuffer, TEXT("%lu"), lpas->dwID);
         break;

      case aggcolDEVICE:
         if (pszDevice)
            lstrcpy (pszBuffer, pszDevice);
         break;

      case aggcolUSED:
         if (lpas)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%.1B"), 1024.0 * (lpas->ckStorageTotal - lpas->ckStorageFree));
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case aggcolUSED_PER:
         if (lpas)
            {
            DWORD dwPer = 100;
            if (lpas->ckStorageTotal != 0)
               dwPer = (DWORD)( 100.0 * (lpas->ckStorageTotal - lpas->ckStorageFree) / lpas->ckStorageTotal );

            dwPer = limit( 0, dwPer, 100 );

            LPTSTR psz = FormatString (IDS_PERCENTAGE, TEXT("%lu"), dwPer);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case aggcolALLOCATED:
         if (lpas)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%.1B"), 1024.0 * lpas->ckStorageAllocated);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case aggcolFREE:
         if (lpas)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%.1B"), 1024.0 * lpas->ckStorageFree);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case aggcolTOTAL:
         if (lpas)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%.1B"), 1024.0 * lpas->ckStorageTotal);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case aggcolSTATUS:
         LPTSTR pszDesc;
         if ((pszDesc = Alert_GetQuickDescription (lpi)) == NULL)
            GetString (pszBuffer, IDS_STATUS_NOALERTS);
         else
            {
            lstrcpy (pszBuffer, pszDesc);
            FreeString (pszDesc);
            }
         break;
      }

   return pszBuffer;
}

