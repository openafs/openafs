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
