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

#ifndef EXPORTED
#define EXPORTED __declspec(dllexport)
#endif

/*
 * PROTOTYPES _________________________________________________________________
 *
 */

extern EXPORTED void GetString (LPTSTR pszTarget, int idsSource, int cchMax = cchRESOURCE);
extern EXPORTED size_t GetStringLength (int ids);

extern EXPORTED BOOL SearchMultiString (LPCTSTR pmsz, LPCTSTR pszString, BOOL fCaseSensitive = FALSE);

extern EXPORTED LPTSTR cdecl FormatString  (LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern EXPORTED LPTSTR cdecl FormatString  (int idsTemplate,     LPCTSTR pszFormat = NULL, ...);
extern EXPORTED LPTSTR cdecl vFormatString (LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern EXPORTED LPTSTR cdecl vFormatString (int idsTemplate,     LPCTSTR pszFormat, va_list arg);

extern EXPORTED void cdecl FormatMultiString  (LPTSTR *ppszTarget, BOOL fAddHead, LPCTSTR pszTemplate, LPCTSTR pszFormat = NULL, ...);
extern EXPORTED void cdecl FormatMultiString  (LPTSTR *ppszTarget, BOOL fAddHead, int idsTemplate,     LPCTSTR pszFormat = NULL, ...);
extern EXPORTED void cdecl vFormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg);
extern EXPORTED void cdecl vFormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, int idsTemplate,     LPCTSTR pszFormat, va_list arg);

extern EXPORTED void FormatBytes (LPTSTR pszTarget, LPTSTR pszFormat, double cb);
extern EXPORTED void FormatDouble (LPTSTR pszTarget, LPTSTR pszFormat, double lfValue);
extern EXPORTED BOOL FormatTime (LPTSTR pszTarget, LPTSTR pszFormat, SYSTEMTIME *pst, BOOL fShowDate = TRUE, BOOL fShowTime = TRUE);
extern EXPORTED BOOL FormatElapsed (LPTSTR pszTarget, LPTSTR pszFormat, SYSTEMTIME *pst);
extern EXPORTED BOOL FormatError (LPTSTR pszTarget, LPTSTR pszFmt, DWORD dwError);
extern EXPORTED void FormatSockAddr (LPTSTR pszTarget, LPTSTR pszFmt, SOCKADDR_IN *paddr);
extern EXPORTED void FormatLargeInt (LPTSTR pszTarget, LPTSTR pszFormatUser, LARGE_INTEGER *pldw);

typedef BOOL (CALLBACK* LPERRORPROC)(LPTSTR psz, ULONG dwErr, LANGID idLanguage);
extern EXPORTED void SetErrorTranslationFunction (LPERRORPROC pfnGetErrText);

extern EXPORTED LPCTSTR FindExtension (LPCTSTR pszToSearch);
extern EXPORTED LPCTSTR FindBaseFileName (LPCTSTR pszToSearch);
extern EXPORTED void ChangeExtension (LPTSTR pszTarget, LPCTSTR pszSource, LPCTSTR pszNewExt, BOOL fForce = TRUE);
extern EXPORTED void CopyBaseFileName (LPTSTR pszTarget, LPCTSTR pszSource);

extern EXPORTED void CopyUnicodeToAnsi (LPSTR pszTargetA, LPCWSTR pszOriginalW, size_t cchMax = (size_t)1024);
extern EXPORTED void CopyUnicodeToString (LPTSTR pszTarget, LPCWSTR pszOriginalW, size_t cchMax = (size_t)1024);
extern EXPORTED void CopyAnsiToUnicode (LPWSTR pszTargetW, LPCSTR pszOriginalA, size_t cchMax = (size_t)1024);
extern EXPORTED void CopyAnsiToString (LPTSTR pszTarget, LPCSTR pszOriginalA, size_t cchMax = (size_t)1024);
extern EXPORTED void CopyStringToUnicode (LPWSTR pszTargetW, LPCTSTR pszOriginal, size_t cchMax = (size_t)1024);
extern EXPORTED void CopyStringToAnsi (LPSTR pszTargetA, LPCTSTR pszOriginal, size_t cchMax = (size_t)1024);

#define AllocateAnsi(_cch)     ((LPSTR)Allocate ((max((_cch),cchRESOURCE)+1) * sizeof(CHAR)))
#define AllocateUnicode(_cch) ((LPWSTR)Allocate ((max((_cch),cchRESOURCE)+1) * sizeof(WCHAR)))
#define AllocateString(_cch)  ((LPTSTR)Allocate ((max((_cch),cchRESOURCE)+1) * sizeof(TCHAR)))
extern EXPORTED void FreeString (LPCVOID pszString, LPCVOID pszOriginalString = NULL);

extern EXPORTED LPSTR StringToAnsi (LPCTSTR pszOriginal);
extern EXPORTED LPTSTR AnsiToString (LPCSTR pszOriginalA);
extern EXPORTED LPWSTR StringToUnicode (LPCTSTR pszOriginal);
extern EXPORTED LPTSTR UnicodeToString (LPCWSTR pszOriginalW);
extern EXPORTED LPWSTR AnsiToUnicode (LPCSTR pszOriginalA);
extern EXPORTED LPSTR UnicodeToAnsi (LPCWSTR pszOriginalW);

extern EXPORTED LPTSTR CloneAnsi (LPSTR pszOriginalA);
extern EXPORTED LPTSTR CloneUnicode (LPWSTR pszOriginalW);
extern EXPORTED LPTSTR CloneString (LPTSTR pszOriginal);
extern EXPORTED LPTSTR CloneMultiString (LPCSTR mszOriginal);


/*
 *** The lstr* functions that Win32 forgot to include
 *
 */

extern EXPORTED void lstrupr (LPTSTR pszToChange);
extern EXPORTED LPCTSTR lstrchr (LPCTSTR pszSearch, TCHAR chToSearchFor);
extern EXPORTED LPCTSTR lstrrchr (LPCTSTR pszSearch, TCHAR chToSearchFor);
extern EXPORTED int lstrncmpi (LPCTSTR pszA, LPCTSTR pszB, size_t cchMax);
extern EXPORTED void lstrncpy (LPTSTR pszTarget, LPCTSTR pszSource, size_t cchMax);
extern EXPORTED void lstrzcpy (LPTSTR pszTarget, LPCTSTR pszSource, size_t cchMax);

extern EXPORTED void lsplitpath (LPCTSTR pszSource,
                 LPTSTR pszDrive, LPTSTR pszPath, LPTSTR pszBase, LPTSTR pszExt);
#endif

