#ifndef TAAFSADMSVRPROPERTIES_H
#define TAAFSADMSVRPROPERTIES_H


/*
 * INCLUSIONS _________________________________________________________________
 *
 */

#include <WINNT/TaAfsAdmSvr.h>


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK AfsAdmSvr_NotifyCallback (NOTIFYEVENT evt, PNOTIFYPARAMS pParams);

void AfsAdmSvr_ObtainRudimentaryProperties (LPASOBJPROP pProperties);
BOOL AfsAdmSvr_ObtainFullProperties (LPASOBJPROP pProperties, ULONG *pStatus = NULL);

LPASOBJPROP AfsAdmSvr_GetCurrentProperties (ASID idObject, ULONG *pStatus = NULL);
BOOL AfsAdmSvr_InvalidateObjectProperties (ASID idObject, ULONG *pStatus = NULL);
void AfsAdmSvr_TestProperties (ASID idObject);

void AfsAdmSvr_SetCellRefreshRate (ASID idCell, ULONG cminRefreshRate);
void AfsAdmSvr_StopCellRefreshThread (ASID idCell);
void AfsAdmSvr_MarkRefreshThread (ASID idCell);


#endif // TAAFSADMSVRPROPERTIES_H
