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
#include "set_col.h"


/*
 * FILESET-VIEW COLUMNS _______________________________________________________
 *
 */

void Filesets_SetDefaultView (LPVIEWINFO lpvi)
{
   lpvi->lvsView = FLS_VIEW_LIST;
   lpvi->nColsAvail = nFILESETCOLUMNS;

   for (size_t iCol = 0; iCol < nFILESETCOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = FILESETCOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = FILESETCOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = setcolNAME;

   lpvi->nColsShown = 7;
   lpvi->aColumns[0] = (int)setcolNAME;
   lpvi->aColumns[1] = (int)setcolAGGREGATE;
   lpvi->aColumns[2] = (int)setcolTYPE;
   lpvi->aColumns[3] = (int)setcolQUOTA_USED;
   lpvi->aColumns[4] = (int)setcolQUOTA_TOTAL;
   lpvi->aColumns[5] = (int)setcolQUOTA_USED_PER;
   lpvi->aColumns[6] = (int)setcolSTATUS;
}


size_t Filesets_GetAlertCount (LPFILESET lpFileset)
{
   return Alert_GetCount (lpFileset->GetIdentifier());
}


LPTSTR Filesets_GetColumnText (LPIDENT lpi, FILESETCOLUMN setcol, BOOL fShowServerName)
{
   static TCHAR aszBuffer[ nFILESETCOLUMNS ][ cchRESOURCE ];
   static size_t iBufferNext = 0;
   LPTSTR pszBuffer = aszBuffer[ iBufferNext++ ];
   if (iBufferNext == nFILESETCOLUMNS)
      iBufferNext = 0;
   *pszBuffer = TEXT('\0');

   LPFILESETSTATUS lpfs = NULL;
   LPIDENT lpiRW = NULL;
   LPFILESET_PREF lpfp;
   if ((lpfp = (LPFILESET_PREF)lpi->GetUserParam()) != NULL)
      {
      lpfs = &lpfp->fsLast;
      lpiRW = lpfp->lpiRW;
      }

   switch (setcol)
      {
      case setcolNAME:
         lpi->GetFilesetName (pszBuffer);
         break;

      case setcolAGGREGATE:
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

      case setcolTYPE:
         if (lpfs)
            {
            switch (lpfs->Type)
               {
               case ftREADWRITE:
                  GetString (pszBuffer, IDS_FILESETTYPE_RW);
                  break;

               case ftCLONE:
                  GetString (pszBuffer, IDS_FILESETTYPE_CLONE);
                  break;

               case ftREPLICA:
                  if (lpiRW == NULL)
                     GetString (pszBuffer, IDS_FILESETTYPE_RO);
                  else if (lpiRW->GetServer() != lpi->GetServer())
                     GetString (pszBuffer, IDS_FILESETTYPE_RO);
                  else
                     GetString (pszBuffer, IDS_FILESETTYPE_RO_STAGE);
                  break;
               }
            }
         break;

      case setcolDATE_CREATE:
         if (lpfs)
            {
            if (!FormatTime (pszBuffer, TEXT("%s"), &lpfs->timeCreation))
               pszBuffer[0] = TEXT('\0');
            }
         break;

      case setcolDATE_UPDATE:
         if (lpfs)
            {
            if (!FormatTime (pszBuffer, TEXT("%s"), &lpfs->timeLastUpdate))
               pszBuffer[0] = TEXT('\0');
            }
         break;

      case setcolDATE_ACCESS:
         if (lpfs)
            {
            if (!FormatTime (pszBuffer, TEXT("%s"), &lpfs->timeLastAccess))
               pszBuffer[0] = TEXT('\0');
            }
         break;

      case setcolDATE_BACKUP:
         if (lpfs)
            {
            if (!FormatTime (pszBuffer, TEXT("%s"), &lpfs->timeLastBackup))
               pszBuffer[0] = TEXT('\0');
            }
         break;

      case setcolQUOTA_USED:
         if (lpfs)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%.1B"), 1024.0 * lpfs->ckUsed);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case setcolQUOTA_USED_PER:
         if (lpfs && lpfs->Type == ftREADWRITE)
            {
            DWORD dwPer = 100;
            if (lpfs->ckQuota != 0)
               dwPer = (DWORD)( 100.0 * lpfs->ckUsed / lpfs->ckQuota );

            dwPer = max( 0, dwPer );
            LPTSTR psz = FormatString (IDS_PERCENTAGE, TEXT("%lu"), dwPer);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case setcolQUOTA_FREE:
         if (lpfs && lpfs->Type == ftREADWRITE)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%.1B"), 1024.0 * (lpfs->ckQuota - lpfs->ckUsed));
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case setcolQUOTA_TOTAL:
         if (lpfs && lpfs->Type == ftREADWRITE)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%.1B"), 1024.0 * lpfs->ckQuota);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case setcolSTATUS:
         LPTSTR pszDesc;
         if ((pszDesc = Alert_GetQuickDescription (lpi)) != NULL)
            {
            lstrcpy (pszBuffer,pszDesc);
            FreeString (pszDesc);
            }
         else if (!lpfs)
            {
            GetString (pszBuffer, IDS_STATUS_NOALERTS);
            }
         else
            {
            if (lpfs->State & fsBUSY)
               GetString (pszBuffer, IDS_SETSTATUS_BUSY);
            else if (lpfs->State & fsSALVAGE)
               GetString (pszBuffer, IDS_SETSTATUS_SALVAGE);
            else if (lpfs->State & fsMOVED)
               GetString (pszBuffer, IDS_SETSTATUS_MOVED);
            else if (lpfs->State & fsLOCKED)
               GetString (pszBuffer, IDS_SETSTATUS_LOCKED);
            else if (lpfs->State & fsNO_VOL)
               GetString (pszBuffer, IDS_SETSTATUS_NO_VOL);
            else
               GetString (pszBuffer, IDS_STATUS_NOALERTS);
            }
         break;

      case setcolID:
         if (lpfs)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%ld"), lpfs->id);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;

      case setcolFILES:
         if (lpfs)
            {
            LPTSTR psz = FormatString (TEXT("%1"), TEXT("%ld"), lpfs->nFiles);
            lstrcpy (pszBuffer, psz);
            FreeString (psz);
            }
         break;
      }

   return pszBuffer;
}


/*
 * REPLICA-VIEW COLUMNS _______________________________________________________
 *
 */

void Replicas_SetDefaultView (LPVIEWINFO lpvi)
{
   lpvi->lvsView = FLS_VIEW_LIST;
   lpvi->nColsAvail = nREPLICACOLUMNS;

   for (size_t iCol = 0; iCol < nREPLICACOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = REPLICACOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = REPLICACOLUMNS[ iCol ].idsColumn;
      }

   lpvi->nColsShown = 3;
   lpvi->aColumns[0] = (int)repcolSERVER;
   lpvi->aColumns[1] = (int)repcolAGGREGATE;
   lpvi->aColumns[2] = (int)repcolDATE_UPDATE;
}


LPTSTR Replicas_GetColumnText (LPIDENT lpi, REPLICACOLUMN repcol)
{
   static TCHAR aszBuffer[ nREPLICACOLUMNS ][ cchRESOURCE ];
   static size_t iBufferNext = 0;
   LPTSTR pszBuffer = aszBuffer[ iBufferNext++ ];
   if (iBufferNext == nREPLICACOLUMNS)
      iBufferNext = 0;

   LPFILESETSTATUS lpfs = NULL;
   LPFILESET_PREF lpfp;
   if ((lpfp = (LPFILESET_PREF)lpi->GetUserParam()) != NULL)
      {
      lpfs = &lpfp->fsLast;
      }

   switch (repcol)
      {
      case repcolSERVER:
         lpi->GetServerName (pszBuffer);
         break;

      case repcolAGGREGATE:
         lpi->GetAggregateName (pszBuffer);
         break;

      case repcolDATE_UPDATE:
         if (!lpfs)
            *pszBuffer = TEXT('\0');
         else if (!FormatTime (pszBuffer, TEXT("%s"), &lpfs->timeLastUpdate))
            *pszBuffer = TEXT('\0');
         break;

      default:
         pszBuffer = NULL;
         break;
      }

   return pszBuffer;
}

