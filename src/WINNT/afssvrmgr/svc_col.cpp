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

#include "svrmgr.h"
#include "svc_col.h"


/*
 * SERVICE-VIEW COLUMNS _______________________________________________________
 *
 */

void Services_SetDefaultView (LPVIEWINFO lpvi)
{
   lpvi->lvsView = FLS_VIEW_LIST;
   lpvi->nColsAvail = nSERVICECOLUMNS;

   for (size_t iCol = 0; iCol < nSERVICECOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = SERVICECOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = SERVICECOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = svccolNAME;

   lpvi->nColsShown = 3;
   lpvi->aColumns[0] = (int)svccolNAME;
   lpvi->aColumns[1] = (int)svccolSTATUS;
   lpvi->aColumns[2] = (int)svccolDATE_STARTSTOP;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

size_t Services_GetAlertCount (LPSERVICE lpService)
{
   return Alert_GetCount (lpService->GetIdentifier());
}


LPTSTR Services_GetColumnText (LPIDENT lpi, SERVICECOLUMN svccol, BOOL fShowServerName)
{
   static TCHAR aszBuffer[ nSERVICECOLUMNS ][ cchRESOURCE ];
   static size_t iBufferNext = 0;
   LPTSTR pszBuffer = aszBuffer[ iBufferNext++ ];
   if (iBufferNext == nSERVICECOLUMNS)
      iBufferNext = 0;
   *pszBuffer = TEXT('\0');

   LPSERVICESTATUS lpss = NULL;
   LPSERVICE_PREF lpsp;
   if ((lpsp = (LPSERVICE_PREF)lpi->GetUserParam()) != NULL)
      lpss = &lpsp->ssLast;

   switch (svccol)
      {
      case svccolNAME:
         if (!fShowServerName)
            lpi->GetServiceName (pszBuffer);
         else
            {
            TCHAR szNameSvr[ cchNAME ];
            TCHAR szNameSvc[ cchNAME ];
            lpi->GetServerName (szNameSvr);
            lpi->GetServiceName (szNameSvc);
            LPTSTR pszName = FormatString (IDS_SERVER_SERVICE, TEXT("%s%s"), szNameSvr, szNameSvc);
            lstrcpy (pszBuffer, pszName);
            FreeString (pszName);
            }
         break;

      case svccolTYPE:
         if (lpss)
            {
            if (lpss->type == SERVICETYPE_SIMPLE)
               GetString (pszBuffer, IDS_SERVICETYPE_SIMPLE);
            else if (lpss->type == SERVICETYPE_FS)
               GetString (pszBuffer, IDS_SERVICETYPE_FS);
            else
               GetString (pszBuffer, IDS_SERVICETYPE_CRON);
            }
         break;

      case svccolPARAMS:
         if (lpss)
            {
            lstrncpy (pszBuffer, lpss->szParams, cchRESOURCE-1);
            pszBuffer[ cchRESOURCE-1 ] = TEXT('\0');

            for (LPTSTR pch = pszBuffer; *pch; ++pch)
               {
               if (*pch == TEXT('\r') || *pch == TEXT('\t') || *pch == TEXT('\n'))
                  *pch = TEXT(' ');
               }
            }
         break;

      case svccolNOTIFIER:
         if (lpss)
            {
            lstrncpy (pszBuffer, lpss->szNotifier, cchRESOURCE-1);
            pszBuffer[ cchRESOURCE-1 ] = TEXT('\0');

            for (LPTSTR pch = pszBuffer; *pch; ++pch)
               {
               if (*pch == TEXT('\r') || *pch == TEXT('\t') || *pch == TEXT('\n'))
                  *pch = TEXT(' ');
               }
            }
         break;

      case svccolSTATUS:
         if (lpss)
            {
            if (lpss->state == SERVICESTATE_RUNNING)
               GetString (pszBuffer, IDS_SERVICESTATE_RUNNING);
            else if (lpss->state == SERVICESTATE_STOPPING)
               GetString (pszBuffer, IDS_SERVICESTATE_STOPPING);
            else if (lpss->state == SERVICESTATE_STARTING)
               GetString (pszBuffer, IDS_SERVICESTATE_STARTING);
            else
               GetString (pszBuffer, IDS_SERVICESTATE_STOPPED);
            }
         break;

      case svccolDATE_START:
         if (lpss)
            {
            if (!FormatTime (pszBuffer, TEXT("%s"), &lpss->timeLastStart))
               pszBuffer[0] = TEXT('\0');
            }
         break;

      case svccolDATE_STOP:
         if (lpss)
            {
            if (!FormatTime (pszBuffer, TEXT("%s"), &lpss->timeLastStop))
               pszBuffer[0] = TEXT('\0');
            }
         break;

      case svccolDATE_STARTSTOP:
         if (lpss)
            {
            LPSYSTEMTIME pst;
            int ids;
            if ((lpss->state == SERVICESTATE_RUNNING) || (lpss->state == SERVICESTATE_STARTING))
               {
               ids = IDS_SERVICE_STARTDATE;
               pst = &lpss->timeLastStart;
               }
            else
               {
               ids = IDS_SERVICE_STOPDATE;
               pst = &lpss->timeLastStop;
               }

            TCHAR szDate[ cchRESOURCE ];
            if (FormatTime (szDate, TEXT("%s"), pst))
               {
               LPTSTR psz = FormatString (ids, TEXT("%s"), szDate);
               lstrcpy (pszBuffer, psz);
               FreeString (psz);
               }
            }
         break;

      case svccolDATE_FAILED:
         if (lpss)
            {
            if (!FormatTime (pszBuffer, TEXT("%s"), &lpss->timeLastFail))
               pszBuffer[0] = TEXT('\0');
            }
         break;

      case svccolLASTERROR:
         if (lpss)
            {
            LPTSTR psz = FormatString (IDS_SERVICE_LASTERROR, TEXT("%ld"), lpss->dwErrLast);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;
      }

   return pszBuffer;
}

