/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAAFSADMSVRCALLBACK_H
#define TAAFSADMSVRCALLBACK_H

#include <WINNT/TaAfsAdmSvr.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

typedef enum
   {
   cbtACTION
   } CALLBACKTYPE;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void AfsAdmSvr_CallbackManager (void);

void AfsAdmSvr_PostCallback (CALLBACKTYPE Type, BOOL fFinished, LPASACTION pAction);

void AfsAdmSvr_StopCallbackManagers (void);


#endif // TAAFSADMSVRCALLBACK_H
