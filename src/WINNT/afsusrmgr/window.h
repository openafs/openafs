#ifndef WINDOW_H
#define WINDOW_H


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Main_DialogProc (HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);

void Main_PrepareTabChild (int iTabNew = -1);

void Main_SetMenus (void);
void Main_SetViewMenus (HMENU hm);


#endif

