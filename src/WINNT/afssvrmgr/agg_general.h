#ifndef AGG_GENERAL_H
#define AGG_GENERAL_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

PVOID Aggregates_LoadPreferences (LPIDENT lpiAggregate);
BOOL Aggregates_SavePreferences (LPIDENT lpiAggregate);

LPIDENT Aggregates_GetFocused (HWND hDlg);
LPIDENT Aggregates_GetSelected (HWND hDlg);


#endif

