#ifndef WINDOW_H
#define WINDOW_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define WM_TRAYICON          (WM_USER+100)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

BOOL CALLBACK Main_DlgProc (HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

void Main_RepopulateTabs (BOOL fDestroyInvalid);
void Main_EnableRemindTimer (BOOL fEnable);
void Main_Show (BOOL fShow);
size_t Main_FindExpiredCreds (void);


#endif

