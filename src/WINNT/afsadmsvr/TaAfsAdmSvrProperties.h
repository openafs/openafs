/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

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
