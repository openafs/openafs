#ifndef SET_GENERAL_H
#define SET_GENERAL_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define ckQUOTA_DEFAULT   5000
#define ckQUOTA_MINIMUM      1           // minimum quota: 1kb
#define ckQUOTA_MAXIMUM    (2L*ck1TB-1L) // maximum quota: 0x7FFFFFFF kb (~2TB)

#define LVIS_ALL (LVIS_FOCUSED | LVIS_SELECTED | LVIS_CUT | LVIS_DROPHILITED)


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

PVOID Filesets_LoadPreferences (LPIDENT lpiFileset);
BOOL Filesets_SavePreferences (LPIDENT lpiFileset);

LPIDENT Filesets_GetSelected (HWND hDlg);
LPIDENT Filesets_GetFocused (HWND hDlg, LPPOINT pptHitTest = NULL);
HTREEITEM Filesets_GetFocusedItem (HWND hDlg, LPPOINT pptHitTest = NULL);

BOOL Filesets_fIsLocked (LPFILESETSTATUS pfs);


#endif

