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
#include "usr_col.h"


/*
 * USER-VIEW COLUMNS __________________________________________________________
 *
 */

void User_SetDefaultView (LPVIEWINFO lpvi, ICONVIEW *piv)
{
   lpvi->lvsView = FLS_VIEW_SMALL;
   lpvi->nColsAvail = nUSERCOLUMNS;

   for (size_t iCol = 0; iCol < nUSERCOLUMNS; ++iCol)
      {
      lpvi->cxColumns[ iCol ]  = USERCOLUMNS[ iCol ].cxWidth;
      lpvi->idsColumns[ iCol ] = USERCOLUMNS[ iCol ].idsColumn;
      }

   lpvi->iSort = usrcolNAME;

   lpvi->nColsShown = 5;
   lpvi->aColumns[0] = (int)usrcolNAME;
   lpvi->aColumns[1] = (int)usrcolUID;
   lpvi->aColumns[2] = (int)usrcolCHANGEPW;
   lpvi->aColumns[3] = (int)usrcolREUSEPW;
   lpvi->aColumns[4] = (int)usrcolEXPIRES;

   *piv = ivSTATUS;
}


void User_GetColumn (ASID idObject, USERCOLUMN iCol, LPTSTR pszText, LPSYSTEMTIME pstDate, LONG *pcsec, COLUMNTYPE *pcType)
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
         case usrcolNAME:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (pszText)
               User_GetDisplayName (pszText, &Properties);
            break;

         case usrcolFLAGS:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (pszText)
               {
               if (Properties.u.UserProperties.fHaveKasInfo)
                  {
                  if (Properties.u.UserProperties.KASINFO.fIsAdmin)
                     {
                     GetString (pszText, IDS_USRFLAG_ADMIN);
                     pszText = &pszText[ lstrlen(pszText) -1 ];
                     }
                  if (Properties.u.UserProperties.KASINFO.fCanGetTickets)
                     {
                     GetString (pszText, IDS_USRFLAG_TICKET);
                     pszText = &pszText[ lstrlen(pszText) -1 ];
                     }
                  if (Properties.u.UserProperties.KASINFO.fEncrypt)
                     {
                     GetString (pszText, IDS_USRFLAG_ENCRYPT);
                     pszText = &pszText[ lstrlen(pszText) -1 ];
                     }
                  if (Properties.u.UserProperties.KASINFO.fCanChangePassword)
                     {
                     GetString (pszText, IDS_USRFLAG_CHANGEPW);
                     pszText = &pszText[ lstrlen(pszText) -1 ];
                     }
                  if (Properties.u.UserProperties.KASINFO.fCanReusePasswords)
                     {
                     GetString (pszText, IDS_USRFLAG_REUSEPW);
                     pszText = &pszText[ lstrlen(pszText) -1 ];
                     }
                  }
               }
            break;

         case usrcolADMIN:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  GetString (pszText, (Properties.u.UserProperties.KASINFO.fIsAdmin) ? IDS_YES : IDS_NO);
               }
            break;

         case usrcolTICKET:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  GetString (pszText, (Properties.u.UserProperties.KASINFO.fCanGetTickets) ? IDS_YES : IDS_NO);
               }
            break;

         case usrcolSYSTEM:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  GetString (pszText, (Properties.u.UserProperties.KASINFO.fEncrypt) ? IDS_NO : IDS_YES);
               }
            break;

         case usrcolCHANGEPW:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  GetString (pszText, (Properties.u.UserProperties.KASINFO.fCanChangePassword) ? IDS_YES : IDS_NO);
               }
            break;

         case usrcolREUSEPW:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  GetString (pszText, (Properties.u.UserProperties.KASINFO.fCanReusePasswords) ? IDS_YES : IDS_NO);
               }
            break;

         case usrcolEXPIRES:
            if (pcType)
               *pcType = ctDATE;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  FormatTime (pszText, TEXT("%t"), &Properties.u.UserProperties.KASINFO.timeExpires);
               if (pstDate)
                  memcpy (pstDate, &Properties.u.UserProperties.KASINFO.timeExpires, sizeof(SYSTEMTIME));
               }
            break;

         case usrcolLASTPW:
            if (pcType)
               *pcType = ctDATE;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  FormatTime (pszText, TEXT("%t"), &Properties.u.UserProperties.KASINFO.timeLastPwChange);
               if (pstDate)
                  memcpy (pstDate, &Properties.u.UserProperties.KASINFO.timeLastPwChange, sizeof(SYSTEMTIME));
               }
            break;

         case usrcolLASTMOD:
            if (pcType)
               *pcType = ctDATE;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  FormatTime (pszText, TEXT("%t"), &Properties.u.UserProperties.KASINFO.timeLastMod);
               if (pstDate)
                  memcpy (pstDate, &Properties.u.UserProperties.KASINFO.timeLastMod, sizeof(SYSTEMTIME));
               }
            break;

         case usrcolLASTMODBY:
            if (pcType)
               *pcType = ctALPHABETIC;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  lstrcpy (pszText, Properties.u.UserProperties.KASINFO.szUserLastMod);
               }
            break;

         case usrcolLIFETIME:
            if (pcType)
               *pcType = ctELAPSED;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  FormatElapsedSeconds (pszText, Properties.u.UserProperties.KASINFO.csecTicketLifetime);
               if (pcsec)
                  *pcsec = Properties.u.UserProperties.KASINFO.csecTicketLifetime;
               }
            break;

         case usrcolCDAYPW:
            if (pcType)
               *pcType = ctELAPSED;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  FormatElapsedSeconds (pszText, Properties.u.UserProperties.KASINFO.cdayPwExpire * csec1DAY);
               if (pcsec)
                  *pcsec = Properties.u.UserProperties.KASINFO.cdayPwExpire * csec1DAY;
               }
            break;

         case usrcolCFAILLOGIN:
            if (pcType)
               *pcType = ctNUMERIC;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  wsprintf (pszText, TEXT("%lu"), Properties.u.UserProperties.KASINFO.cFailLogin);
               }
            break;

         case usrcolCSECLOCK:
            if (pcType)
               *pcType = ctELAPSED;
            if (Properties.u.UserProperties.fHaveKasInfo)
               {
               if (pszText)
                  FormatElapsedSeconds (pszText, Properties.u.UserProperties.KASINFO.csecFailLoginLock);
               if (pcsec)
                  *pcsec = Properties.u.UserProperties.KASINFO.csecFailLoginLock;
               }
            break;

         case usrcolCGROUPMAX:
            if (pcType)
               *pcType = ctNUMERIC;
            if (Properties.u.UserProperties.fHavePtsInfo)
               {
               if (pszText)
                  wsprintf (pszText, TEXT("%lu"), Properties.u.UserProperties.PTSINFO.cgroupCreationQuota);
               }
            break;

         case usrcolUID:
            if (pcType)
               *pcType = ctNUMERIC;
            if (Properties.u.UserProperties.fHavePtsInfo)
               {
               if (pszText)
                  wsprintf (pszText, TEXT("%ld"), Properties.u.UserProperties.PTSINFO.uidName);
               }
            break;

         case usrcolOWNER:
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

         case usrcolCREATOR:
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


BOOL User_GetDisplayName (LPTSTR pszText, LPASOBJPROP pProperties)
{
   lstrcpy (pszText, pProperties->szName);

   if (pProperties->u.UserProperties.szInstance[0])
      {
      // Don't show the Instance value for certain entries
      if ( (lstrcmpi (pProperties->szName, TEXT("admin"))) &&
           (lstrcmpi (pProperties->szName, TEXT("krbtgt"))) )
         {
         wsprintf (&pszText[ lstrlen(pszText) ], TEXT(".%s"), pProperties->u.UserProperties.szInstance);
         }
      }

   return !!*pszText;
}


BOOL User_GetDisplayName (LPTSTR pszText, ASID idUser)
{
   ASOBJPROP Properties;
   if (asc_ObjectPropertiesGet_Fast (g.idClient, g.idCell, idUser, &Properties))
      {
      return User_GetDisplayName (pszText, &Properties);
      }
   else
      {
      return asc_ObjectNameGet_Fast (g.idClient, g.idCell, idUser, pszText);
      }
}


void User_SplitDisplayName (LPTSTR pszFull, LPTSTR pszName, LPTSTR pszInstance)
{
   if (pszName)
      lstrcpy (pszName, pszFull);
   if (pszInstance)
      lstrcpy (pszInstance, TEXT(""));

   if (!fIsMachineAccount (pszFull))
      {
      if (pszName && pszInstance)
         {
         LPTSTR pchDot;
         if ((pchDot = (LPTSTR)lstrchr (pszName, TEXT('.'))) != NULL)
            {
            *pchDot = TEXT('\0');
            lstrcpy (pszInstance, &pchDot[1]);
            }
         }
      }
}

