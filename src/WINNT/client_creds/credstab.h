#ifndef CREDSTAB_H
#define CREDSTAB_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

void ShowObtainCreds (BOOL fExpiring, LPTSTR pszCell);

BOOL CALLBACK Creds_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);


#endif

