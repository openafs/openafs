#ifndef TAB_PREFS_H
#define TAB_PREFS_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK PrefsTab_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

BOOL PrefsTab_CommitChanges (BOOL fForce);


#endif

