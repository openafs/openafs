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
#include "grp_col.h"


/*
 * GROUP-VIEW COLUMNS _________________________________________________________
 *
 */

void Group_SetDefaultView (LPVIEWINFO lpvi, ICONVIEW *piv)
{
   lpvi->lvsView = FLS_VIEW_SMALL;
   lpvi->nColsAvail = nGROUPCOLUMNS;

   for (size_t iCol = 0; iCol < nGROUPCOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = GROUPCOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = GROUPCOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = grpcolNAME;

   lpvi->nColsShown = 3;
   lpvi->aColumns[0] = (int)grpcolNAME;
   lpvi->aColumns[1] = (int)grpcolUID;
   lpvi->aColumns[2] = (int)grpcolCMEMBERS;

   *piv = ivSTATUS;
}


void Group_GetColumn (ASID idObject, GROUPCOLUMN iCol, LPTSTR pszText, LPSYSTEMTIME pstDate, LONG *pcsec, COLUMNTYPE *pcType)
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
         case grpcolNAME:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (pszText)
               lstrcpy (pszText, Properties.szName);
            break;

         case grpcolCMEMBERS:
            if (pcType)
               *pcType = ctNUMERIC;
            if (pszText)
               wsprintf (pszText, TEXT("%lu"), Properties.u.GroupProperties.nMembers);
            break;

         case grpcolUID:
            if (pcType)
               *pcType = ctNUMERIC;
            if (pszText)
               wsprintf (pszText, TEXT("%ld"), Properties.u.GroupProperties.uidName);
            break;

         case grpcolOWNER:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (pszText)
               {
               if (Properties.u.GroupProperties.szOwner[0])
                  wsprintf (pszText, TEXT("%s (%ld)"), Properties.u.GroupProperties.szOwner, Properties.u.GroupProperties.uidOwner);
               else
                  wsprintf (pszText, TEXT("%ld"), Properties.u.GroupProperties.uidOwner);
               }
            break;

         case grpcolCREATOR:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (pszText)
               {
               if (Properties.u.GroupProperties.szCreator[0])
                  wsprintf (pszText, TEXT("%s (%ld)"), Properties.u.GroupProperties.szCreator, Properties.u.GroupProperties.uidCreator);
               else
                  wsprintf (pszText, TEXT("%ld"), Properties.u.GroupProperties.uidCreator);
               }
            break;
         }
      }
}

