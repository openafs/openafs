#ifndef AGG_TAB_H
#define AGG_TAB_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Aggregates_DlgProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Aggregates_ShowPopupMenu (HWND hList, POINT ptList, POINT ptScreen);
void Aggregates_ShowParticularPopupMenu (HWND hParent, POINT ptScreen, LPIDENT lpiAggregate);


#endif

