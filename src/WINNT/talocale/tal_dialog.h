#ifndef TAL_DIALOG_H
#define TAL_DIALOG_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define MB_MODELESS  0x80000000L   // for Message, vMessage


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

extern HWND ModelessDialog (int idd, HWND hWndParent, DLGPROC lpDialogFunc);
extern HWND ModelessDialogParam (int idd, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);

extern int ModalDialog (int idd, HWND hWndParent, DLGPROC lpDialogFunc);
extern int ModalDialogParam (int idd, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);

extern int cdecl Message (UINT mb_type, LPCTSTR pszTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern int cdecl Message (UINT mb_type, LPCTSTR pszTitle, int idsTemplate, LPCTSTR pszFormat = NULL, ...);
extern int cdecl Message (UINT mb_type, int idsTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern int cdecl Message (UINT mb_type, int idsTitle, int idsTemplate, LPCTSTR pszFormat = NULL, ...);
extern int cdecl vMessage (UINT mb_type, LPCTSTR pszTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern int cdecl vMessage (UINT mb_type, LPCTSTR pszTitle, int idsTemplate, LPCTSTR pszFormat, va_list arg);
extern int cdecl vMessage (UINT mb_type, int idsTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern int cdecl vMessage (UINT mb_type, int idsTitle, int idsTemplate, LPCTSTR pszFormat, va_list arg);

#endif

