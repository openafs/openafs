#ifndef ACTION_H
#define ACTION_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void Actions_SetDefaultView (LPVIEWINFO lpvi);

void Actions_OpenWindow (void);
void Actions_CloseWindow (void);
void Actions_WindowToTop (BOOL fTop);

void Actions_OnNotify (WPARAM wp, LPARAM lp);


#endif

