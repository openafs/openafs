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
#include "mch_col.h"


/*
 * USER-VIEW COLUMNS __________________________________________________________
 *
 */

void Machine_SetDefaultView (LPVIEWINFO lpvi, ICONVIEW *piv)
{
   lpvi->lvsView = FLS_VIEW_SMALL;
   lpvi->nColsAvail = nMACHINECOLUMNS;

   for (size_t iCol = 0; iCol < nMACHINECOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = MACHINECOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = MACHINECOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = mchcolNAME;

   lpvi->nColsShown = 2;
   lpvi->aColumns[0] = (int)mchcolNAME;
   lpvi->aColumns[1] = (int)mchcolUID;

   *piv = ivSTATUS;
}


void Machine_GetColumn (ASID idObject, MACHINECOLUMN iCol, LPTSTR pszText, LPSYSTEMTIME pstDate, LONG *pcsec, COLUMNTYPE *pcType)
{
   if (pszText)
      *pszText = TEXT('\0');
   if (pstDate)
      memset (pstDate, 0x00, sizeof(SYSTEMTIME));
   if (pcsec)
      *pcsec = 0;
   if (pcType)
      *pcType = ctALPHABETIC;

   ASOBJPROP Properties;
   if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, idObject, &Properties))
      {
      switch (iCol)
         {
         case mchcolNAME:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (pszText)
               lstrcpy (pszText, Properties.szName);
            break;

         case mchcolCGROUPMAX:
            if (pcType)
               *pcType = ctNUMERIC;
            if (Properties.u.UserProperties.fHavePtsInfo)
               {
               if (pszText)
                  wsprintf (pszText, TEXT("%ld"), Properties.u.UserProperties.PTSINFO.cgroupCreationQuota);
               }
            break;

         case mchcolUID:
            if (pcType)
               *pcType = ctNUMERIC;
            if (Properties.u.UserProperties.fHavePtsInfo)
               {
               if (pszText)
                  wsprintf (pszText, TEXT("%ld"), Properties.u.UserProperties.PTSINFO.uidName);
               }
            break;

         case mchcolOWNER:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHavePtsInfo)
               {
               if (pszText)
                  {
                  if (Properties.u.UserProperties.PTSINFO.szOwner[0])
                     wsprintf (pszText, TEXT("%s (%ld)"), Properties.u.UserProperties.PTSINFO.szOwner, Properties.u.UserProperties.PTSINFO.uidOwner);
                  else
                     wsprintf (pszText, TEXT("%ld"), Properties.u.UserProperties.PTSINFO.uidOwner);
                  }
               }
            break;

         case mchcolCREATOR:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHavePtsInfo)
               {
               if (pszText)
                  {
                  if (Properties.u.UserProperties.PTSINFO.szCreator[0])
                     wsprintf (pszText, TEXT("%s (%ld)"), Properties.u.UserProperties.PTSINFO.szCreator, Properties.u.UserProperties.PTSINFO.uidCreator);
                  else
                     wsprintf (pszText, TEXT("%ld"), Properties.u.UserProperties.PTSINFO.uidCreator);
                  }
               }
            break;
         }
      }
}

