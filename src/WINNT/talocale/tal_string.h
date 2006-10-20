/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef TAL_STRING_H
#define TAL_STRING_H

#include <winsock2.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef cchRESOURCE
#define cchRESOURCE 256
#endif

#ifndef cchLENGTH
#define cchLENGTH(_sz) (sizeof(_sz) / sizeof(_sz[0]))
#endif

#define cszMultiStringNULL TEXT("---")  // added instead of "" in multistrings


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

extern void GetString (LPTSTR pszTarget, int idsSource, int cchMax = cchRESOURCE);
extern size_t GetStringLength (int ids);

extern BOOL SearchMultiString (LPCTSTR pmsz, LPCTSTR pszString, BOOL fCaseSensitive = FALSE);

extern LPTSTR cdecl FormatString  (LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern LPTSTR cdecl FormatString  (int idsTemplate,     LPCTSTR pszFormat = NULL, ...);
extern LPTSTR cdecl vFormatString (LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern LPTSTR cdecl vFormatString (int idsTemplate,     LPCTSTR pszFormat, va_list arg);

extern void cdecl FormatMultiString  (LPTSTR *ppszTarget, BOOL fAddHead, LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern void cdecl FormatMultiString  (LPTSTR *ppszTarget, BOOL fAddHead, int idsTemplate,     LPCTSTR pszFormat = NULL, ...);
extern void cdecl vFormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern void cdecl vFormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, int idsTemplate,     LPCTSTR pszFormat, va_list arg);

extern void FormatBytes (LPTSTR pszTarget, LPTSTR pszFormat, double cb);
extern void FormatDouble (LPTSTR pszTarget, LPTSTR pszFormat, double lfValue);
extern BOOL FormatTime (LPTSTR pszTarget, LPTSTR pszFormat, SYSTEMTIME *pst, BOOL fShowDate = TRUE, BOOL fShowTime = TRUE);
extern BOOL FormatElapsed (LPTSTR pszTarget, LPTSTR pszFormat, SYSTEMTIME *pst);
extern BOOL FormatError (LPTSTR pszTarget, LPTSTR pszFmt, DWORD dwError);
extern void FormatSockAddr (LPTSTR pszTarget, LPTSTR pszFmt, SOCKADDR_IN *paddr);
extern void FormatLargeInt (LPTSTR pszTarget, LPTSTR pszFormatUser, LARGE_INTEGER *pldw);

typedef BOOL (CALLBACK* LPERRORPROC)(LPTSTR psz, ULONG dwErr, LANGID idLanguage);
extern void SetErrorTranslationFunction (LPERRORPROC pfnGetErrText);

extern LPCTSTR FindExtension (LPCTSTR pszToSearch);
extern LPCTSTR FindBaseFileName (LPCTSTR pszToSearch);
extern void ChangeExtension (LPTSTR pszTarget, LPCTSTR pszSource, LPCTSTR pszNewExt, BOOL fForce = TRUE);
extern void CopyBaseFileName (LPTSTR pszTarget, LPCTSTR pszSource);

extern void CopyUnicodeToAnsi (LPSTR pszTargetA, LPCWSTR pszOriginalW, size_t cchMax = (size_t)1024);
extern void CopyUnicodeToString (LPTSTR pszTarget, LPCWSTR pszOriginalW, size_t cchMax = (size_t)1024);
extern void CopyAnsiToUnicode (LPWSTR pszTargetW, LPCSTR pszOriginalA, size_t cchMax = (size_t)1024);
extern void CopyAnsiToString (LPTSTR pszTarget, LPCSTR pszOriginalA, size_t cchMax = (size_t)1024);
extern void CopyStringToUnicode (LPWSTR pszTargetW, LPCTSTR pszOriginal, size_t cchMax = (size_t)1024);
extern void CopyStringToAnsi (LPSTR pszTargetA, LPCTSTR pszOriginal, size_t cchMax = (size_t)1024);

#define AllocateAnsi(_cch)     ((LPSTR)Allocate ((max((_cch),cchRESOURCE)+1) * sizeof(CHAR)))
#define AllocateUnicode(_cch) ((LPWSTR)Allocate ((max((_cch),cchRESOURCE)+1) * sizeof(WCHAR)))
#define AllocateString(_cch)  ((LPTSTR)Allocate ((max((_cch),cchRESOURCE)+1) * sizeof(TCHAR)))
extern void FreeString (LPCVOID pszString, LPCVOID pszOriginalString = NULL);

extern LPSTR StringToAnsi (LPCTSTR pszOriginal);
extern LPTSTR AnsiToString (LPCSTR pszOriginalA);
extern LPWSTR StringToUnicode (LPCTSTR pszOriginal);
extern LPTSTR UnicodeToString (LPCWSTR pszOriginalW);
extern LPWSTR AnsiToUnicode (LPCSTR pszOriginalA);
extern LPSTR UnicodeToAnsi (LPCWSTR pszOriginalW);

extern LPTSTR CloneAnsi (LPSTR pszOriginalA);
extern LPTSTR CloneUnicode (LPWSTR pszOriginalW);
extern LPTSTR CloneString (LPTSTR pszOriginal);
extern LPTSTR CloneMultiString (LPCSTR mszOriginal);


/*
 *** The lstr* functions that Win32 forgot to include
 *
 */

extern void lstrupr (LPTSTR pszToChange);
extern LPCTSTR lstrchr (LPCTSTR pszSearch, TCHAR chToSearchFor);
extern LPCTSTR lstrrchr (LPCTSTR pszSearch, TCHAR chToSearchFor);
extern int lstrncmpi (LPCTSTR pszA, LPCTSTR pszB, size_t cchMax);
extern void lstrncpy (LPTSTR pszTarget, LPCTSTR pszSource, size_t cchMax);
extern void lstrzcpy (LPTSTR pszTarget, LPCTSTR pszSource, size_t cchMax);

extern void lsplitpath (LPCTSTR pszSource,
                 LPTSTR pszDrive, LPTSTR pszPath, LPTSTR pszBase, LPTSTR pszExt);
#endif

