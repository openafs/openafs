/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include "TaAfsAdmSvrInternal.h"


/*
 * ROUTINES ___________________________________________________________________
 *
 */


      // AfsAdmSvr_ChangeCell
      // ...changes a cell's properties.
      //
extern "C" int AfsAdmSvr_ChangeCell (UINT_PTR idClient, ASID idCell, LPAFSADMSVR_CHANGECELL_PARAMS pChange, ULONG *pStatus)
{
   ASACTION Action;
   Action.Action = ACTION_CELL_CHANGE;
   Action.idClient = idClient;
   Action.idCell = idCell;
   size_t iOp = AfsAdmSvr_BeginOperation (idClient, &Action);

   Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeCell (idCell=0x%08lX)"), idClient, idCell);

   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus,iOp);

   // Call AfsClass to actually do it
   //
   PTSPROPERTIES PtsProperties;
   PtsProperties.idUserMax = (int)(pChange->idUserMax);
   PtsProperties.idGroupMax = (int)(pChange->idGroupMax);

   ULONG status;
   if (!AfsClass_SetPtsProperties ((LPIDENT)idCell, &PtsProperties, &status))
      {
      Print (dlERROR, TEXT("Client 0x%08lX: ChangeCell failed; error 0x%08lX"), idClient, status);
      return FALSE_(status,pStatus,iOp);
      }

   AfsAdmSvr_TestProperties (idCell);

   Print (dlDETAIL, TEXT("Client 0x%08lX: ChangeCell succeeded"), idClient);
   AfsAdmSvr_EndOperation (iOp);
   return TRUE;
}



      // AfsAdmSvr_SetRefreshRate
      // ...changes the refresh rate for a specific cell
      //
extern "C" int AfsAdmSvr_SetRefreshRate (UINT_PTR idClient, ASID idCell, ULONG cminRefreshRate, ULONG *pStatus)
{
   if (!AfsAdmSvr_fIsValidClient (idClient))
      return FALSE_(ERROR_INVALID_PARAMETER,pStatus);

   Print (dlDETAIL, TEXT("Client 0x%08lX: Setting refresh rate to %lu minutes"), idClient, cminRefreshRate);

   if (!cminRefreshRate)
      AfsAdmSvr_StopCellRefreshThread (idCell);
   else
      AfsAdmSvr_SetCellRefreshRate (idCell, cminRefreshRate);

   return TRUE;
}

