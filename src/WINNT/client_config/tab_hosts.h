#ifndef TAB_HOSTS_H
#define TAB_HOSTS_H

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK HostsTab_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

BOOL HostsTab_CommitChanges (BOOL fForce);


#endif

