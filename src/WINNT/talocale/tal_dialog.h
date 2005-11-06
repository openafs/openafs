/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAL_DIALOG_H
#define TAL_DIALOG_H


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define MB_MODELESS  0x80000000L   // for Message, vMessage

#ifndef EXPORTED
#define EXPORTED __declspec(dllexport)
#endif


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

extern EXPORTED HWND ModelessDialog (int idd, HWND hWndParent, DLGPROC lpDialogFunc);
extern EXPORTED HWND ModelessDialogParam (int idd, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);

extern EXPORTED INT_PTR ModalDialog (int idd, HWND hWndParent, DLGPROC lpDialogFunc);
extern EXPORTED INT_PTR ModalDialogParam (int idd, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);

extern EXPORTED int cdecl Message (UINT mb_type, LPCTSTR pszTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern EXPORTED int cdecl Message (UINT mb_type, LPCTSTR pszTitle, int idsTemplate, LPCTSTR pszFormat = NULL, ...);
extern EXPORTED int cdecl Message (UINT mb_type, int idsTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern EXPORTED int cdecl Message (UINT mb_type, int idsTitle, int idsTemplate, LPCTSTR pszFormat = NULL, ...);
extern EXPORTED int cdecl vMessage (UINT mb_type, LPCTSTR pszTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern EXPORTED int cdecl vMessage (UINT mb_type, LPCTSTR pszTitle, int idsTemplate, LPCTSTR pszFormat, va_list arg);
extern EXPORTED int cdecl vMessage (UINT mb_type, int idsTitle, LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern EXPORTED int cdecl vMessage (UINT mb_type, int idsTitle, int idsTemplate, LPCTSTR pszFormat, va_list arg);

#endif

