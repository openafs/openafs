#ifndef SVC_TAB_H
#define SVC_TAB_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Services_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Services_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiService);


#endif

