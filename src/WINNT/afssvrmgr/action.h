#ifndef ACTION_H
#define ACTION_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void ActionNotification_MainThread (NOTIFYEVENT evt, PNOTIFYPARAMS pParams);

void Action_OpenWindow (void);
void Action_CloseWindow (void);
void Action_SetDefaultView (LPVIEWINFO lpvi);
void Action_WindowToTop (BOOL fTop);

BOOL Action_fAnyActive (void);

void Action_ShowConfirmations (BOOL fShow);


#endif

