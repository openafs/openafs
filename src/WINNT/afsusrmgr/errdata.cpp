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


/*
 * ROUTINES ___________________________________________________________________
 *
 */

LPERRORDATA ED_Create (int idsSingle, int idsMultiple)
{
   LPERRORDATA ped = New (ERRORDATA);
   memset (ped, 0x00, sizeof(ERRORDATA));
   ped->idsSingle = idsSingle;
   ped->idsMultiple = idsMultiple;
   return ped;
}


void ED_Free (LPERRORDATA ped)
{
   if (ped)
      {
      if (ped->pAsidList)
         asc_AsidListFree (&ped->pAsidList);
      Delete (ped);
      }
}


void ED_RegisterStatus (LPERRORDATA ped, ASID idObject, BOOL fSuccess, ULONG status)
{
   if (!fSuccess)
      {
      if (!ped->pAsidList)
         asc_AsidListCreate (&ped->pAsidList);
      if (!asc_AsidListTest (&ped->pAsidList, idObject))
         asc_AsidListAddEntry (&ped->pAsidList, idObject, status);
      if (!ped->status)
         ped->status = status;
      ped->cFailures ++;
      }
}


ULONG ED_GetFinalStatus (LPERRORDATA ped)
{
   return (ped->cFailures) ? ped->status : 0;
}


void ED_ShowErrorDialog (LPERRORDATA ped)
{
   if (ped->pAsidList)
      {
      LPTSTR pszNames = CreateNameList (ped->pAsidList);

      if (ped->pAsidList->cEntries == 1)
         {
         if (!pszNames)
            ErrorDialog (ped->status, ped->idsSingle, TEXT("%m"), IDS_UNKNOWN_NAME);
         else
            ErrorDialog (ped->status, ped->idsSingle, TEXT("%s"), pszNames);
         }
      else if (ped->pAsidList->cEntries > 1)
         {
         if (!pszNames)
            ErrorDialog (ped->status, ped->idsMultiple, TEXT("%m"), IDS_UNKNOWN_NAME);
         else
            ErrorDialog (ped->status, ped->idsMultiple, TEXT("%s"), pszNames);
         }

      FreeString (pszNames);
      }
}

