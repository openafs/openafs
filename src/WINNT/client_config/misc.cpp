extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include "afs_config.h"


/*
 * REALLOC ____________________________________________________________________
 *
 */

BOOL AfsConfigReallocFunction (LPVOID *ppTarget, size_t cbElement, size_t *pcTarget, size_t cReq, size_t cInc)
{
   LPVOID pNew;
   size_t cNew;

   if (cReq <= *pcTarget)
      return TRUE;

   if ((cNew = cInc * ((cReq + cInc-1) / cInc)) <= 0)
      return FALSE;

   if ((pNew = (LPVOID)Allocate (cbElement * cNew)) == NULL)
      return FALSE;
   memset (pNew, 0x00, cbElement * cNew);

   if (*pcTarget != 0)
      {
      memcpy (pNew, *ppTarget, cbElement * (*pcTarget));
      Free (*ppTarget);
      }

   *ppTarget = pNew;
   *pcTarget = cNew;
   return TRUE;
}

