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
#include "svr_col.h"


/*
 * SERVER-VIEW COLUMNS ________________________________________________________
 *
 */

void Server_SetDefaultView_Horz (LPVIEWINFO lpviHorz)
{
   lpviHorz->lvsView = FLS_VIEW_LIST;
   lpviHorz->nColsAvail = nSERVERCOLUMNS;

   for (size_t iCol = 0; iCol < nSERVERCOLUMNS; ++iCol)
      {
      lpviHorz->cxColumns[ iCol ]  = SERVERCOLUMNS[ iCol ].cxWidth;
      lpviHorz->idsColumns[ iCol ] = SERVERCOLUMNS[ iCol ].idsColumn;
      }

   lpviHorz->iSort = svrcolNAME;

   lpviHorz->nColsShown = 3;
   lpviHorz->aColumns[0] = (int)svrcolNAME;
   lpviHorz->aColumns[1] = (int)svrcolADDRESS;
   lpviHorz->aColumns[2] = (int)svrcolSTATUS;
}


void Server_SetDefaultView_Vert (LPVIEWINFO lpviVert)
{
   lpviVert->lvsView = FLS_VIEW_LARGE;
   lpviVert->nColsAvail = nSERVERCOLUMNS;

   for (size_t iCol = 0; iCol < nSERVERCOLUMNS; ++iCol)
      {
      lpviVert->cxColumns[ iCol ]  = SERVERCOLUMNS[ iCol ].cxWidth;
      lpviVert->idsColumns[ iCol ] = SERVERCOLUMNS[ iCol ].idsColumn;
      }

   lpviVert->iSort = svrcolNAME;

   lpviVert->nColsShown = 3;
   lpviVert->aColumns[0] = (int)svrcolNAME;
   lpviVert->aColumns[1] = (int)svrcolADDRESS;
   lpviVert->aColumns[2] = (int)svrcolSTATUS;
}


/*
 * ROUTINES ___________________________________________________________________
 *
 */

size_t Server_GetAlertCount (LPSERVER lpServer)
{
   return Alert_GetCount (lpServer->GetIdentifier());
}


LPTSTR Server_GetColumnText (LPIDENT lpi, SERVERCOLUMN svrcol)
{
   static TCHAR aszBuffer[ nSERVERCOLUMNS ][ cchRESOURCE ];
   static size_t iBufferNext = 0;
   LPTSTR pszBuffer = aszBuffer[ iBufferNext++ ];
   if (iBufferNext == nSERVERCOLUMNS)
      iBufferNext = 0;
   *pszBuffer = TEXT('\0');

   LPSERVERSTATUS lpss = NULL;
   LPSERVER_PREF lpsp;
   if ((lpsp = (LPSERVER_PREF)lpi->GetUserParam()) != NULL)
      {
      lpss = &lpsp->ssLast;
      }

   switch (svrcol)
      {
      case svrcolNAME:
         lpi->GetServerName (pszBuffer);
         break;

      case svrcolADDRESS:
         if (lpss)
            FormatSockAddr (pszBuffer, TEXT("%a"), &lpss->aAddresses[0]);
         break;

      case svrcolSTATUS:
         {
         LPTSTR pszDesc;
         if ((pszDesc = Alert_GetQuickDescription (lpi)) == NULL)
            GetString (pszBuffer, IDS_STATUS_NOALERTS);
         else
            {
            lstrcpy (pszBuffer, pszDesc);
            FreeString (pszDesc);
            }
         }
         break;
      }

   return pszBuffer;
}

