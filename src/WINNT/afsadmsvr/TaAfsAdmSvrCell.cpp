
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
int AfsAdmSvr_ChangeCell (DWORD idClient, ASID idCell, LPAFSADMSVR_CHANGECELL_PARAMS pChange, ULONG *pStatus)
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
int AfsAdmSvr_SetRefreshRate (DWORD idClient, ASID idCell, ULONG cminRefreshRate, ULONG *pStatus)
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

