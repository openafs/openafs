/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <windowsx.h>
#include <WINNT/talocale.h>
#include <stdlib.h>


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#ifndef _MAX_DRIVE
#define _MAX_DRIVE    3
#define _MAX_DIR    260
#define _MAX_EXT      5
#define _MAX_FNAME  260
#endif

#define _ttoupper(_ch)    (TCHAR)CharUpper((LPTSTR)_ch)

#define isfmtgarbage(_ch) (isdigit(_ch) || (_ch==TEXT('.')) || (_ch==TEXT(',')) || (_ch==TEXT('-')))


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

LPTSTR cdecl vFormatString (LONG, LPCTSTR, va_list);

BOOL TranslateErrorFunc (LPTSTR pszText, ULONG code, LANGID idLanguage);
BOOL TranslateError (LPTSTR pszText, ULONG status);

LPTSTR FixFormatString (LPTSTR pszFormat);


/*
 * ROUTINES ___________________________________________________________________
 *
 */


/*** GetString - Loads one or more consecutive strings from a resource file
 *
 */


void GetString (LPTSTR psz, int ids, int cchMax)
{
   *psz = TEXT('\0');

   for (;;)
      {
      LPCSTRINGTEMPLATE pst;
      if ((pst = TaLocale_GetStringResource (ids)) == NULL)
         break;

      CopyUnicodeToString (psz, (LPWSTR)pst->achString, min(cchMax,pst->cchString+1));
      if (psz[ lstrlen(psz)-1 ] != TEXT('+'))
         break;
      psz[ lstrlen(psz)-1 ] = TEXT('\0');

      cchMax -= lstrlen(psz);
      psz += lstrlen(psz);
      ++ids;
      }
}


/*** GetStringLength() - Returns the number of chars in a string resource
 *
 */

size_t GetStringLength (int ids)
{
   size_t cch = 0;

   for (;;)
      {
      LPCSTRINGTEMPLATE pst;
      if ((pst = TaLocale_GetStringResource (ids)) == NULL)
         break;

      char szAnsi[ cchRESOURCE * 2 + 1 ];
      CopyUnicodeToAnsi (szAnsi, pst->achString, pst->cchString+1);
      cch += 1+ lstrlenA(szAnsi);

      if ((!pst->cchString) || (pst->achString[ pst->cchString-1 ] != L'+'))
         break;

      ++ids;
      }

   return cch;
}



/*** SearchMultiString() - scans multistrings for a given string
 *
 */

BOOL SearchMultiString (LPCTSTR pmsz, LPCTSTR pszString, BOOL fCaseSensitive)
{
   if (pmsz)
      {
      for (LPCTSTR psz = pmsz; *psz; psz += 1+lstrlen(psz))
         {
         if (!*pszString && !lstrlen(psz))
            return TRUE;
         else if (pszString && fCaseSensitive && !lstrcmp (psz, pszString))
            return TRUE;
         else if (pszString && !fCaseSensitive && !lstrcmpi (psz, pszString))
            return TRUE;
         }
      }

   return FALSE;
}


/*** FormatMultiString() - manipulates multistrings ("EXA\0MP\0LE\0\0")
 *
 */

void cdecl vFormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, LONG ids, LPCTSTR pszFormat, va_list arg)
{
   static const TCHAR szMultiStringNULL[] = cszMultiStringNULL;
   LPTSTR pszNew = vFormatString (ids, pszFormat, arg);

   if (!pszNew || !*pszNew)
      {
      FreeString (pszNew);
      pszNew = (LPTSTR)szMultiStringNULL;
      }

   size_t cchOld = 1; // for the second NUL-terminator
   for (LPTSTR psz = *ppszTarget; psz && *psz; psz += 1+lstrlen(psz))
      cchOld += lstrlen(psz) +1;

   LPTSTR pszFinal;
   if ((pszFinal = AllocateString (cchOld +lstrlen(pszNew) +1)) != NULL)
      {
      if (!*ppszTarget)
         {
         lstrcpy (pszFinal, pszNew);
         }
      else if (fAddHead)
         {
         lstrcpy (pszFinal, pszNew);
         memcpy (&pszFinal[ lstrlen(pszNew)+1 ], *ppszTarget, sizeof(TCHAR) * cchOld);
         }
      else // (*ppszTarget && !fAddHead)
         {
         memcpy (pszFinal, *ppszTarget, sizeof(TCHAR) * cchOld);
         lstrcpy (&pszFinal[ cchOld-1 ], pszNew);
         }
      pszFinal[ cchOld +lstrlen(pszNew) ] = TEXT('\0');

      FreeString (*ppszTarget);
      *ppszTarget = pszFinal;
      }

   if (pszNew != (LPTSTR)szMultiStringNULL)
      FreeString (pszNew);
}


void cdecl FormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, LPCTSTR pszTemplate, LPCTSTR pszFormat, ...)
{
   va_list arg;
   //if (pszFormat != NULL)
      va_start (arg, pszFormat);
   vFormatMultiString (ppszTarget, fAddHead, PtrToLong(pszTemplate), pszFormat, arg);
}

void cdecl FormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, int idsTemplate, LPCTSTR pszFormat, ...)
{
   va_list arg;
   //if (pszFormat != NULL)
      va_start (arg, pszFormat);
   vFormatMultiString (ppszTarget, fAddHead, (LONG)idsTemplate, pszFormat, arg);
}

void cdecl vFormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, LPCTSTR pszTemplate, LPCTSTR pszFormat, va_list arg)
{
   vFormatMultiString (ppszTarget, fAddHead, PtrToLong(pszTemplate), pszFormat, arg);
}

void cdecl vFormatMultiString (LPTSTR *ppszTarget, BOOL fAddHead, int idsTemplate, LPCTSTR pszFormat, va_list arg)
{
   vFormatMultiString (ppszTarget, fAddHead, (LONG)idsTemplate, pszFormat, arg);
}


/*** FormatString() - replacable-parameter version of wsprintf()
 *
 */

LPTSTR cdecl FormatString (LPCTSTR psz, LPCTSTR pszFmt, ...)
{
   va_list arg;
   //if (pszFmt != NULL)
      va_start (arg, pszFmt);
   return vFormatString (PtrToLong(psz), pszFmt, arg);
}

LPTSTR cdecl FormatString (int ids, LPCTSTR pszFmt, ...)
{
   va_list  arg;
   //if (pszFmt != NULL)
      va_start (arg, pszFmt);
   return vFormatString ((LONG)ids, pszFmt, arg);
}

LPTSTR cdecl vFormatString (LPCTSTR psz, LPCTSTR pszFmt, va_list arg)
{
   return vFormatString (PtrToLong(psz), pszFmt, arg);
}

LPTSTR cdecl vFormatString (int ids, LPCTSTR pszFmt, va_list arg)
{
   return vFormatString ((LONG)ids, pszFmt, arg);
}


typedef enum	// vartype
   {
   vtINVALID,
   vtWORD,
   vtDWORD,
   vtFLOAT,
   vtDOUBLE,
   vtSTRINGW,
   vtSTRINGA,
   vtMESSAGE,
   vtBYTES_DOUBLE,
   vtBYTES,
   vtSYSTEMTIME,
   vtELAPSEDTIME,
   vtERROR,
   vtADDRESS,
   vtLARGEINT,
   } vartype;

#ifdef UNICODE
#define vtSTRING  vtSTRINGW	// %s gives vtSTRING
#else
#define vtSTRING  vtSTRINGA
#endif

#define cchMaxNUMBER  40
#define cszNULL       TEXT("(null)")

LPTSTR cdecl vFormatString (LONG pszSource, LPCTSTR pszFmt, va_list arg)
{
   LPTSTR  *apszArgs;
   LPTSTR   psz;
   LPTSTR   pszOut = NULL;
   LPTSTR   pchOut;
   LPTSTR   pszTemplate;
   size_t   cch;
   int      nArgs;
   int      argno;
   TCHAR    szFmt[ cchRESOURCE ];

   if (pszSource == 0)
      return NULL;

   if (HIWORD(pszSource) != 0)	// It's a string
      {
      pszTemplate = (LPTSTR)LongToPtr(pszSource);
      }
   else	// It's a message
      {
      cch = GetStringLength((INT)pszSource);

      if ((pszTemplate = AllocateString (1+cch)) == NULL)
         return NULL;

      GetString (pszTemplate, (int)pszSource, (INT)cch);
      }

            //
            // First off, count the number of arguments given in {pszFmt}.
            //

   nArgs = 0;

   if (pszFmt != NULL)
      {
      for (psz = (LPTSTR)pszFmt; *psz; psz++)
         {
         if (*psz == TEXT('%'))
            nArgs++;
         }
      }

            //
            // If there are no arguments, then we're just expanding {pszSource}.
            //

   if (nArgs == 0)
      {
      if (HIWORD(pszSource) == 0)	// It's a message
         {
         psz = pszTemplate;
         }
      else // (HIWORD(pszSource) != 0)	// It's a string
         {
         psz = CloneString (pszTemplate);
         }
      return psz;
      }

            //
            // Otherwise, allocate an array of LPTSTR's, one for each
            // argument.
            //

   apszArgs = (LPTSTR*)Allocate (sizeof(LPTSTR) * nArgs);

   if (apszArgs == NULL)
      {
      if (pszSource != PtrToLong(pszTemplate))
         FreeString (pszTemplate);
      return NULL;
      }

   memset (apszArgs, 0x00, sizeof(LPTSTR) * nArgs);

            //
            // Extract each argument in turn, using {pszFmt} and {arg}.
            //

   for (argno = 0; pszFmt && *pszFmt; argno++)
      {
      size_t   cchMin;
      vartype  vt;

      double   arg_f;
      LONG     arg_d;
      LPTSTR   arg_psz;
      SYSTEMTIME *arg_t;
      SOCKADDR_IN *arg_a;
      LARGE_INTEGER *arg_ldw;

               //
               // At this point, {pstFmt} points to the start of an argument's
               // decription within the user-supplied format string
               // (which may look like, say, "%-12.30ls%40lu").  Parse
               // the "%-12.30" part of each argument first...
               //

      if ((*pszFmt != TEXT('%')) || (*(1+pszFmt) == TEXT('\0')))
         break;

      lstrcpy (szFmt, pszFmt);
      if ((psz = (LPTSTR)lstrchr (&szFmt[1], TEXT('%'))) != NULL)
         *psz = 0;

      int ii;
      for (ii = 1; szFmt[ii] == TEXT('-') || szFmt[ii] == TEXT(','); ii++)
         ;
      cchMin = _ttol (&szFmt[ii]);

      for (pszFmt++; *pszFmt && isfmtgarbage(*pszFmt); ++pszFmt)
         {
         if (*pszFmt == TEXT('%'))
            break;
         }

      if (!pszFmt || !*pszFmt)
         break;
      if (*pszFmt == TEXT('%'))
         continue;

               //
               // Yuck.  At this point,
               //    cchMin is either 0 or whatever appeared as X in "%X.##s"
               //    pszFmt points to the first character after "%##.##"
               //
               // Step through the format string's definition ("c", "lu", etc)
               // to determine what kind of argument this one is, setting {vt}
               // accordingly.
               //

      for (vt = vtINVALID; *pszFmt && *pszFmt != TEXT('%'); pszFmt++)
         {
         switch (*pszFmt)
            {
            case TEXT('c'):
               vt = vtWORD;
               break;

            case TEXT('x'):
            case TEXT('X'):
            case TEXT('h'):
            case TEXT('u'):
            case TEXT('d'):
               if (vt != vtDWORD)
                  vt = vtWORD;
               break;

            case TEXT('l'):
               if (vt == vtFLOAT)
                  vt = vtDOUBLE;
               else
                  vt = vtDWORD;
               break;

            case TEXT('i'):	// "%i"==int, "%li"==large_integer
               if (vt != vtDWORD)
                  vt = vtWORD;
               else
                  vt = vtLARGEINT;
               break;

            case TEXT('g'):
            case TEXT('f'):
               if (vt == vtDWORD)
                  vt = vtDOUBLE;
               else
                  vt = vtFLOAT;
               break;

            case TEXT('s'):
               if (vt == vtDWORD)
                  vt = vtSTRINGW;
               else if (vt == vtWORD)
                  vt = vtSTRINGA;
               else
                  vt = vtSTRING;
               break;

            case TEXT('m'):
               vt = vtMESSAGE;
               *(LPTSTR)(lstrchr(szFmt, *pszFmt)) = TEXT('s'); // prints as %s
               break;

            case TEXT('B'):	// %B = double cb
               vt = vtBYTES_DOUBLE;
               *(LPTSTR)(lstrchr(szFmt, *pszFmt)) = TEXT('s'); // prints as %s
               break;

            case TEXT('b'):	// %b = ULONG cb
               vt = vtBYTES;
               *(LPTSTR)(lstrchr(szFmt, *pszFmt)) = TEXT('s'); // prints as %s
               break;

            case TEXT('t'):
               if (vt == vtERROR)	// %et == systemtime* (shown as elapsed)
                  vt = vtELAPSEDTIME;
               else // (vt != vtERROR)	// %t == systemtime* (shown as elapsed)
                  vt = vtSYSTEMTIME;
               break;

            case TEXT('e'):	// %e == error
               vt = vtERROR;
               break;

            case TEXT('a'):	// %a = LPSOCKADDR_IN
               vt = vtADDRESS;
               *(LPTSTR)(lstrchr(szFmt, *pszFmt)) = TEXT('s'); // prints as %s
               break;
            }
         }

      if (vt == vtINVALID)
         continue;

            //
            // Now the fun part.  Pop the appropriate argument off the
            // stack; note that we had to know how big the argument is,
            // and in what format, in order to do this.
            //

      switch (vt)
         {
         case vtWORD:
            arg_d = (LONG)(va_arg (arg, short));
            cch = cchMaxNUMBER;
            break;

         case vtDWORD:
            arg_d = va_arg (arg, LONG);
            cch = cchMaxNUMBER;
            break;

         case vtFLOAT:
            arg_f = (double)( va_arg (arg, float) );
            cch = cchMaxNUMBER;
            break;

         case vtDOUBLE:
            arg_f = va_arg (arg, double);
            cch = cchMaxNUMBER;
            break;

         case vtSTRINGA:   
            {
            LPSTR arg_pszA = va_arg (arg, LPSTR);

            if (arg_pszA == NULL)
               arg_psz = CloneString (cszNULL);
            else
               arg_psz = CloneAnsi (arg_pszA);

            cch = lstrlen(arg_psz);
            break;
            }

         case vtSTRINGW:   
            {
            LPWSTR arg_pszW = va_arg (arg, LPWSTR);

            if (arg_pszW == NULL)
               arg_psz = CloneString (cszNULL);
            else
               arg_psz = CloneUnicode (arg_pszW);

            cch = lstrlen(arg_psz);
            break;
            }

         case vtMESSAGE:
            {
            int arg_ids = va_arg (arg, int);

            if ((cch = GetStringLength (arg_ids)) == 0)
               arg_psz = CloneString (cszNULL);
            else
               {
               if ((arg_psz = AllocateString (cch)) == NULL)
                  goto lblDONE;
               GetString (arg_psz, arg_ids, (INT)cch);
               }

            cch = lstrlen(arg_psz);
            break;
            }

         case vtBYTES_DOUBLE:
            arg_f = va_arg (arg, double);
            cch = cchMaxNUMBER;
            break;

         case vtBYTES:
            arg_f = (double)va_arg (arg, LONG);
            cch = cchMaxNUMBER;
            break;

         case vtELAPSEDTIME:
         case vtSYSTEMTIME:
            arg_t = va_arg (arg, SYSTEMTIME*);
            cch = cchRESOURCE;
            break;

         case vtERROR:
            arg_d = va_arg (arg, DWORD);
            cch = cchRESOURCE;
            break;

         case vtADDRESS:
            arg_a = va_arg (arg, SOCKADDR_IN*);
            cch = cchRESOURCE;
            break;

         case vtLARGEINT:
            arg_ldw = va_arg (arg, LARGE_INTEGER*);
            cch = cchMaxNUMBER * 2;
            break;
         }

               //
               // Okay; now, depending on what {vt} is, one of
               // {arg_psz, arg_d, arg_f} has been set to match
               // this argument.  Allocate an string for apszArgs[]
               // to represent the formatted, ready-for-output version
               // of this argument.
               //

      cch = max( cch, cchMin );

      if ((apszArgs[ argno ] = AllocateString (cch+1)) == NULL)
         goto lblDONE;

      switch (vt)
         {
         case vtWORD:
            wsprintf (apszArgs[ argno ], szFmt, (short)arg_d);
            break;

         case vtDWORD:
            wsprintf (apszArgs[ argno ], szFmt, arg_d);
            break;

         case vtFLOAT:
            FormatDouble (apszArgs[ argno ], szFmt, arg_f);
            break;

         case vtDOUBLE:
            FormatDouble (apszArgs[ argno ], szFmt, arg_f);
            break;

         case vtSTRINGA:
         case vtSTRINGW:
         case vtMESSAGE:
            wsprintf (apszArgs[ argno ], szFmt, arg_psz);
            FreeString (arg_psz);
            break;

         case vtBYTES:
         case vtBYTES_DOUBLE:
            FormatBytes (apszArgs[ argno ], szFmt, arg_f);
            break;

         case vtELAPSEDTIME:
            FormatElapsed (apszArgs[ argno ], szFmt, arg_t);
            break;

         case vtSYSTEMTIME:
            FormatTime (apszArgs[ argno ], szFmt, arg_t);
            break;

         case vtERROR:
            FormatError (apszArgs[ argno ], szFmt, arg_d);
            break;

         case vtADDRESS:
            FormatSockAddr (apszArgs[ argno ], szFmt, arg_a);
            break;

         case vtLARGEINT:
            FormatLargeInt (apszArgs[ argno ], szFmt, arg_ldw);
            break;
         }
      }

            //
            // Determine how big the final string will be once the arguments
            // are inserted, and allocate space for it.
            //

   cch = 0;

   for (psz = pszTemplate; *psz; )
      {
      if (*psz != TEXT('%'))
         {
         ++cch;
         ++psz;
         continue;
         }

      if (*(++psz) == TEXT('\0'))
         break;

      if (!isdigit(*psz))
         {
         cch++;
         psz++;
         continue;
         }

      for (argno = 0; *psz && isdigit (*psz); psz++)
         {
         argno *= 10;
         argno += (int)(*psz -'0');
         }
      if (*psz == '~')
         psz++;
      if (argno == 0 || argno > nArgs || apszArgs[argno -1] == NULL)
         cch += lstrlen (cszNULL);
      else
         cch += lstrlen (apszArgs[argno -1]);
      }

   if ((pszOut = AllocateString (cch+1)) == NULL)
      goto lblDONE;

            //
            // Then generate a final output, by copying over the template
            // string into the final string, inserting arguments wherever
            // a "%1" (or "%43" etc) is found.
            //

   for (psz = pszTemplate, pchOut = pszOut; *psz; )
      {
      if (*psz != TEXT('%'))
         {
         *pchOut++ = *psz++;
         continue;
         }

      if (*(++psz) == TEXT('\0'))
         break;

      if (!isdigit(*psz))
         {
         *pchOut++ = *psz++;
         continue;
         }

      for (argno = 0; *psz && isdigit (*psz); psz++)
         {
         argno *= 10;
         argno += (int)(*psz -'0');
         }
      if (*psz == '~')
         psz++;
      if (argno == 0 || argno > nArgs || apszArgs[argno -1] == NULL)
         lstrcpy (pchOut, cszNULL);
      else
         lstrcpy (pchOut, apszArgs[argno -1]);
      pchOut = &pchOut[ lstrlen(pchOut) ];
      }

   *pchOut = 0;

lblDONE:
   if (apszArgs != NULL)
      {
      for (argno = 0; argno < nArgs; argno++)
         {
         if (apszArgs[ argno ] != NULL)
            FreeString (apszArgs[ argno ]);
         }
      Free (apszArgs);
      }

   if (HIWORD(pszSource) == 0)	// Did we allocate {pszTemplate}?
      {
      if (pszTemplate != NULL)
         {
         FreeString (pszTemplate);
         }
      }

   return pszOut;
}


/*** FormatSockAddr() - formats a SOCKADDR_IN as text
 *
 */

void FormatSockAddr (LPTSTR pszTarget, LPTSTR pszFmt, SOCKADDR_IN *paddr)
{
   TCHAR szText[ cchRESOURCE ];

   LPSTR pszSockAddrA = inet_ntoa (*(struct in_addr *)&paddr->sin_addr.s_addr);
   LPTSTR pszSockAddr = AnsiToString (pszSockAddrA);

   lstrcpy (szText, pszSockAddr);

   FreeString (pszSockAddr, pszSockAddrA);

   wsprintf (pszTarget, FixFormatString (pszFmt), szText);
}


/*** FormatElapsed() - formats a SYSTEMTIME* as HH:MM:SS
 *
 */

BOOL FormatElapsed (LPTSTR pszTarget, LPTSTR pszFormatUser, SYSTEMTIME *pst)
{
   TCHAR szTimeSep[ cchRESOURCE ];
   if (!GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_STIME, szTimeSep, cchRESOURCE))
      lstrcpy (szTimeSep, TEXT(":"));

   TCHAR szElapsed[ cchRESOURCE ];
   wsprintf (szElapsed, TEXT("%02lu%s%02lu%s%02lu"), 
             pst->wHour + (pst->wDay * 24),
             szTimeSep,
             pst->wMinute,
             szTimeSep,
             pst->wSecond);

   wsprintf (pszTarget, FixFormatString (pszFormatUser), szElapsed);
   return TRUE;
}


/*** FormatTime() - formats a SYSTEMTIME* according to the current locale
 *
 */

BOOL FormatTime (LPTSTR pszTarget, LPTSTR pszFormatUser, SYSTEMTIME *pst, BOOL fShowDate, BOOL fShowTime)
{
   BOOL rc = TRUE;

   SYSTEMTIME lt;
   FILETIME ft;
   FILETIME lft;
   SystemTimeToFileTime (pst, &ft);
   FileTimeToLocalFileTime (&ft, &lft);
   FileTimeToSystemTime (&lft, &lt);

   TCHAR szTime[ cchRESOURCE ];
   TCHAR szDate[ cchRESOURCE ];

   if ( ((pst->wYear == 1970) && (pst->wMonth == 1) && (pst->wDay == 1)) ||
        ((pst->wYear == 0)    && (pst->wMonth == 0) && (pst->wDay == 0)) )
      {
      szTime[0] = TEXT('\0');
      rc = FALSE;
      }
   else
      {
      GetTimeFormat (LOCALE_USER_DEFAULT, 0, &lt, "HH:mm:ss", szTime, cchRESOURCE);
      GetDateFormat (LOCALE_USER_DEFAULT, 0, &lt, "yyyy-MM-dd", szDate, cchRESOURCE);

      if (fShowTime && fShowDate)
         {
         lstrcat (szDate, TEXT(" "));
         lstrcat (szDate, szTime);
         }
      else if (!fShowDate && fShowTime)
         {
         lstrcpy (szDate, szTime);
         }
      }

   wsprintf (pszTarget, FixFormatString (pszFormatUser), szDate);
   return rc;
}


/*** FormatError() - formats an error code as text
 *
 */

LPERRORPROC pfnTranslateError = NULL;
void SetErrorTranslationFunction (LPERRORPROC pfn)
{
   pfnTranslateError = pfn;
}

BOOL FormatError (LPTSTR pszTarget, LPTSTR pszFmt, DWORD dwError)
{
   TCHAR szError[ cchRESOURCE ];
   BOOL rc = TranslateError (szError, dwError);

   LPTSTR pchTarget = szError;
   for (LPTSTR pchSource = szError; *pchSource; ++pchSource)
      {
      if (*pchSource == TEXT('\n'))
         *pchTarget++ = TEXT(' ');
      else if (*pchSource != TEXT('\r'))
         *pchTarget++ = *pchSource;
      }
   *pchTarget = TEXT('\0');

   wsprintf (pszTarget, FixFormatString (pszFmt), szError);
   return rc;
}


BOOL TranslateErrorFunc (LPTSTR pszText, ULONG code, LANGID idLanguage)
{
   BOOL rc = FALSE;

   // Guess an appropriate language ID if none was supplied explicitly
   //
   if (idLanguage == 0)
      {
      idLanguage = TaLocale_GetLanguage();
      }

   // See if the caller supplied a function for us to use.
   //
   if (pfnTranslateError)
      {
      rc = (*pfnTranslateError)(pszText, code, idLanguage);
      }

   // Try to get Windows to translate the thing...
   //
   if (!rc)
      {
      if (FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, idLanguage, pszText, cchRESOURCE, NULL))
         {
         rc = TRUE;
         }
      }

   // See if the message is buried in NTDLL...
   //
   if (!rc)
      {
      HINSTANCE hiNTDLL;
      if ((hiNTDLL = LoadLibrary (TEXT("NTDLL.DLL"))) != NULL)
         {
         if (FormatMessage (FORMAT_MESSAGE_FROM_HMODULE, hiNTDLL, code, idLanguage, pszText, cchRESOURCE, NULL))
            {
            rc = TRUE;
            }
         FreeLibrary (hiNTDLL);
         }
      }

   return rc;
}


BOOL TranslateError (LPTSTR pszText, ULONG status)
{
   BOOL rc;

   if ((rc = TranslateErrorFunc (pszText, status, 0)) == TRUE)
      {
      wsprintf (&pszText[ lstrlen(pszText) ], TEXT(" (0x%08lX)"), status);
      }
   else
      {
      wsprintf (pszText, TEXT("0x%08lX"), status);
      }

   return rc;
}


/*** FormatBytes() - formats L1048576 as "1.0 MB" (etc)
 *
 * Wherever "%1" appears is where the number will show up (formatted
 * according to {pszFormat}).
 *
 */

void FormatBytes (LPTSTR pszTarget, LPTSTR pszFormatUser, double cb)
{
   TCHAR szFormat[ cchRESOURCE ];
   TCHAR szTemplate[ cchRESOURCE ];
   TCHAR szJustTheNumber[ cchRESOURCE ];
   LPTSTR pszOut;

            //
            // The first task is to generate a useful {pszFormat}; it may
            // come in looking like "%-15.15s" or "%0.2b" or "%lu" --who knows.
            // We'll need it to produce a formatted string from a DWORD, so
            // make sure it ends in "lf".
            //

   if (*pszFormatUser != TEXT('%'))
      {
      lstrcpy (szFormat, TEXT("%.2lf")); // sigh--totally bogus format string.
      }
   else
      {
      lstrcpy (szFormat, pszFormatUser);

      LPTSTR pch;
      for (pch = &szFormat[1]; *pch; ++pch)
         {
         if (!isfmtgarbage(*pch))
            {
            lstrcpy (pch, TEXT("lf"));
            break;
            }
         }
      if (!*pch)
         {
         lstrcat (pch, TEXT("lf"));
         }
      }

            //
            // The next step is to generate just the number portion of the
            // output; that will look like "0.90" or "5.3", or whatever.
            //

   FormatDouble (szJustTheNumber, szFormat, cb/1048576.0);
   lstrcpy (szTemplate, TEXT("%1 MB"));

            //
            // Cheesy bit: if all we have are 0's and "."'s, just
            // make the number read "0".
            //
   TCHAR * pch;
   for (pch = szJustTheNumber; *pch; ++pch)
      {
      if (*pch != TEXT('0') && *pch != TEXT('.'))
         break;
      }
   if (!*pch)
      {
      lstrcpy (szJustTheNumber, TEXT("0"));
      }

            //
            // Not particularly hard.  Now let FormatString() generate
            // the final output, by tacking on the necessary units.
            //

   if ((pszOut = FormatString (szTemplate, TEXT("%s"), szJustTheNumber)) == NULL)
      {
      // whoops--out of memory?
      // well, just give it back in bytes--we can still do that.
      //
      FormatDouble (pszTarget, szFormat, cb);
      }
   else
      {
      lstrcpy (pszTarget, pszOut);
      FreeString (pszOut);
      }
}


/*** FormatLargeInt() - formats a large int as "hidword,,lodword"
 *
 */

void FormatLargeInt (LPTSTR pszTarget, LPTSTR pszFormatUser, LARGE_INTEGER *pldw)
{
   // Generate just the number portion of the output;
   // that will look like "###,,###"
   //
   TCHAR szJustTheNumber[ cchRESOURCE ];
   wsprintf (szJustTheNumber, TEXT("%lu,,%lu"), (DWORD)pldw->HighPart, (DWORD)pldw->LowPart);

   // Not particularly hard.  Now let FormatString() generate
   // the final output, by tacking on the necessary units
   // description.
   //
   wsprintf (pszTarget, FixFormatString (pszFormatUser), szJustTheNumber);
}


/*** FormatDouble() - like wsprintf(), but for one argument (a double)
 *
 * since {wsprintf} doesn't understand "%lf", "%f", "%g", "%e", etc,
 * use this function instead.  {pszFormat} should consist of nothing
 * except the formatting string: thus, "%3.15lf" is okay, but "hi %lf there"
 * isn't.
 *
 */

void FormatDouble (LPTSTR pszTarget, LPTSTR pszFormat, double lfValue)
{
   BOOL fPlusUpFront = FALSE;
   BOOL fZeroUpFront = FALSE;
   int  cchMin = 0;
   int  cchMax = 8;

   // First, parse the format string: look for "%{zero|plus}{min}.{max}",
   // and ignore any other characters.
   //
   while (*pszFormat && *pszFormat != TEXT('%'))
      ++pszFormat;

   while (*pszFormat == TEXT('%') ||
          *pszFormat == TEXT('0') ||
          *pszFormat == TEXT('+') ||
          *pszFormat == TEXT('-') )
      {
      if (*pszFormat == TEXT('0'))
         {
         fZeroUpFront = TRUE;
         }
      else if (*pszFormat == TEXT('+'))
         {
         fPlusUpFront = TRUE;
         }
      ++pszFormat;
      }

   cchMin = _ttol( pszFormat );

   while (isdigit(*pszFormat))
      ++pszFormat;

   if (*pszFormat == TEXT('.'))
      {
      ++pszFormat;
      cchMax = _ttol( pszFormat );
      }

   // Then generate the output string, based on what we just learned
   // and what {lfValue} is.
   //
   if (lfValue >= 0 && fPlusUpFront)
      {
      *pszTarget++ = TEXT('+');
      }
   else if (lfValue < 0)
      {
      *pszTarget++ = TEXT('-');
      lfValue = 0 - lfValue;
      }

   if (lfValue >= 1 || fZeroUpFront)
      {
      unsigned long ulValue = (unsigned long)lfValue;
      wsprintf (pszTarget, TEXT("%lu"), ulValue);
      pszTarget = &pszTarget[ lstrlen(pszTarget) ];
      lfValue -= (double)ulValue;
      }

   if (cchMax != 0)
      {
      unsigned long ulValue;
      TCHAR szTemp[ cchRESOURCE ];
      LPTSTR pszDecimalPoint = pszTarget;
      *pszTarget++ = TEXT('.');

      for (int ii = 0; ii < cchMax; ++ii)
         {
         lfValue *= 10.0;
         *pszTarget++ = TEXT('0');
         }
      ulValue = (unsigned long)lfValue;

      wsprintf (szTemp, TEXT("%lu"), ulValue);
      szTemp[ cchMax ] = 0;
      lstrcpy (&pszDecimalPoint[ 1 + cchMax - lstrlen(szTemp) ], szTemp);

      pszTarget = &pszTarget[ lstrlen(pszTarget) ];
      }

   *pszTarget = TEXT('\0');
}


void lstrupr (LPTSTR psz)
{
   for ( ; psz && *psz; psz = CharNext (psz))
      *psz = _ttoupper (*psz);
}


LPCTSTR lstrchr (LPCTSTR pszTarget, TCHAR ch)
{
   for ( ; pszTarget && *pszTarget; pszTarget = CharNext(pszTarget))
      {
      if (*pszTarget == ch)
         return pszTarget;
      }
   return NULL;
}


LPCTSTR lstrrchr (LPCTSTR pszTarget, TCHAR ch)
{
   LPCTSTR pszLast = NULL;

   for ( ; pszTarget && *pszTarget; pszTarget = CharNext(pszTarget))
      {
      if (*pszTarget == ch)
         pszLast = pszTarget;
      }
   return pszLast;
}


int lstrncmpi (LPCTSTR pszA, LPCTSTR pszB, size_t cch)
{
   if (!pszA || !pszB)
      {
      return (!pszB) - (!pszA);   // A,!B:1, !A,B:-1, !A,!B:0
      }

   for ( ; cch > 0; cch--, pszA = CharNext(pszA), pszB = CharNext(pszB))
      {
      TCHAR chA = _ttoupper( *pszA );
      TCHAR chB = _ttoupper( *pszB );

      if (!chA || !chB)
         return (!chB) - (!chA);    // A,!B:1, !A,B:-1, !A,!B:0

      if (chA != chB)
         return (int)(chA) - (int)(chB);   // -1:A<B, 0:A==B, 1:A>B
      }

   return 0;  // no differences before told to stop comparing, so A==B
}


void lstrncpy (LPTSTR pszTarget, LPCTSTR pszSource, size_t cch)
{
   size_t ich;
   for (ich = 0; ich < cch; ich++)
      {
      if ((pszTarget[ich] = pszSource[ich]) == TEXT('\0'))
         break;
      }
}


void lstrzcpy (LPTSTR pszTarget, LPCTSTR pszSource, size_t cch)
{
   cch = min(cch, (size_t)(1+lstrlen(pszSource)));
   lstrncpy (pszTarget, pszSource, cch-1);
   pszTarget[ cch-1 ] = TEXT('\0');
}


void lsplitpath (LPCTSTR pszSource,
                 LPTSTR pszDrive, LPTSTR pszPath, LPTSTR pszName, LPTSTR pszExt)
{
   LPCTSTR  pszLastSlash = NULL;
   LPCTSTR  pszLastDot = NULL;
   LPCTSTR  pch;
   size_t   cchCopy;

        /*
         * NOTE: This routine was snitched out of USERPRI.LIB 'cause the
         * one in there doesn't split the extension off the name properly.
         *
         * We assume that the path argument has the following form, where any
         * or all of the components may be missing.
         *
         *      <drive><dir><fname><ext>
         *
         * and each of the components has the following expected form(s)
         *
         *  drive:
         *      0 to _MAX_DRIVE-1 characters, the last of which, if any, is a
         *      ':'
         *  dir:
         *      0 to _MAX_DIR-1 characters in the form of an absolute path
         *      (leading '/' or '\') or relative path, the last of which, if
         *      any, must be a '/' or '\'.  E.g -
         *      absolute path:
         *          \top\next\last\     ; or
         *          /top/next/last/
         *      relative path:
         *          top\next\last\      ; or
         *          top/next/last/
         *      Mixed use of '/' and '\' within a path is also tolerated
         *  fname:
         *      0 to _MAX_FNAME-1 characters not including the '.' character
         *  ext:
         *      0 to _MAX_EXT-1 characters where, if any, the first must be a
         *      '.'
         *
         */

             // extract drive letter and :, if any
             //
   if (*(pszSource + _MAX_DRIVE - 2) == TEXT(':'))
      {
      if (pszDrive)
         {
         lstrncpy (pszDrive, pszSource, _MAX_DRIVE-1);
         pszDrive[ _MAX_DRIVE-1 ] = TEXT('\0');
         }
      pszSource += _MAX_DRIVE-1;
      }
   else if (pszDrive)
      {
      *pszDrive = TEXT('\0');
      }

          // extract path string, if any.  pszSource now points to the first
          // character of the path, if any, or the filename or extension, if
          // no path was specified.  Scan ahead for the last occurence, if
          // any, of a '/' or '\' path separator character.  If none is found,
          // there is no path.  We will also note the last '.' character found,
          // if any, to aid in handling the extension.
          //
   for (pch = pszSource; *pch != TEXT('\0'); pch++)
      {
      if (*pch == TEXT('/') || *pch == TEXT('\\'))
         pszLastSlash = pch;
      else if (*pch == TEXT('.'))
         pszLastDot = pch;
      }

          // if we found a '\\' or '/', fill in pszPath
          //
   if (pszLastSlash)
      {
      if (pszPath)
         {
         cchCopy = min( _MAX_DIR -1, pszLastSlash -pszSource +1 );
         lstrncpy (pszPath, pszSource, cchCopy);
         pszPath[ cchCopy ] = 0;
         }
      pszSource = pszLastSlash +1;
      }
   else if (pszPath)
      {
      *pszPath = TEXT('\0');
      }

             // extract file name and extension, if any.  Path now points to
             // the first character of the file name, if any, or the extension
             // if no file name was given.  Dot points to the '.' beginning the
             // extension, if any.
             //

   if (pszLastDot && (pszLastDot >= pszSource))
      {
               // found the marker for an extension -
               // copy the file name up to the '.'.
               //
      if (pszName)
         {
         cchCopy = min( _MAX_DIR-1, pszLastDot -pszSource );
         lstrncpy (pszName, pszSource, cchCopy);
         pszName[ cchCopy ] = 0;
         }

               // now we can get the extension
               //
      if (pszExt)
         {
         lstrncpy (pszExt, pszLastDot, _MAX_EXT -1);
         pszExt[ _MAX_EXT-1 ] = TEXT('\0');
         }
      }
   else
      {
               // found no extension, give empty extension and copy rest of
               // string into fname.
               //
      if (pszName)
         {
         lstrncpy (pszName, pszSource, _MAX_FNAME -1);
         pszName[ _MAX_FNAME -1 ] = TEXT('\0');
         }

      if (pszExt)
         {
         *pszExt = TEXT('\0');
         }
      }
}


LPCTSTR FindExtension (LPCTSTR name)
{
   LPCTSTR pch;

   if ((pch = lstrchr (FindBaseFileName(name), TEXT('.'))) != NULL)
      return pch;

   return &name[ lstrlen(name) ];
}


LPCTSTR FindBaseFileName (LPCTSTR name)
{
   LPCTSTR pszBase;

   if ((pszBase = lstrrchr (name, TEXT('\\'))) != NULL)
      return CharNext(pszBase);
   else
      return name;
}


void ChangeExtension (LPTSTR pszOut, LPCTSTR pszIn, LPCTSTR pszExt, BOOL fForce)
{
   LPCTSTR pch;

   if (pszOut != pszIn)
      lstrcpy (pszOut, pszIn);

   if ( (*(pch = FindExtension (pszOut)) != TEXT('\0')) && (!fForce) )
      return;

   if (pszExt[0] == TEXT('.') || pszExt[0] == TEXT('\0'))
      {
      lstrcpy ((LPTSTR)pch, pszExt);
      }
   else
      {
      lstrcpy ((LPTSTR)pch, TEXT("."));
      lstrcat ((LPTSTR)pch, pszExt);
      }
}


void CopyBaseFileName (LPTSTR pszTarget, LPCTSTR pszSource)
{
   lstrcpy (pszTarget, FindBaseFileName (pszSource));

   LPTSTR pszExt;
   if ((pszExt = (LPTSTR)FindExtension(pszTarget)) != NULL)
      *pszExt = TEXT('\0');
}


/*
 * ANSI/UNICODE _______________________________________________________________
 *
 */

void CopyUnicodeToAnsi (LPSTR pszTargetA, LPCWSTR pszOriginalW, size_t cchMax)
{
   static size_t cchBufferW = 0;
   static LPWSTR pszBufferW = NULL;

   size_t cchSource = min( cchMax, (size_t)lstrlenW(pszOriginalW)+1 );
   if (cchSource > cchBufferW)
      {
      if (pszBufferW)
         FreeString (pszBufferW);
      pszBufferW = AllocateUnicode (cchSource);
      cchBufferW = cchSource;
      }

   memcpy ((LPBYTE)pszBufferW, (LPBYTE)pszOriginalW, cchSource * sizeof(WCHAR));
   pszBufferW[ cchSource-1 ] = L'\0';

   UINT cpTarget = CP_ACP;
   BOOL fDefault = FALSE;
   size_t cchOut = WideCharToMultiByte (cpTarget, 0, pszOriginalW, (INT)cchSource-1, pszTargetA, (INT)cchMax * 2, TEXT(" "), &fDefault);
   pszTargetA[ cchOut ] = 0;
}

void CopyAnsiToUnicode (LPWSTR pszTargetW, LPCSTR pszOriginalA, size_t cchMax)
{
   static size_t cchBufferA = 0;
   static LPSTR  pszBufferA = NULL;

   cchMax = min( cchMax, (size_t)lstrlenA(pszOriginalA)+1 );
   if (cchMax > cchBufferA)
      {
      if (pszBufferA)
         FreeString (pszBufferA);
      pszBufferA = AllocateAnsi (cchMax);
      cchBufferA = cchMax;
      }

   memcpy ((LPBYTE)pszBufferA, (LPBYTE)pszOriginalA, cchMax);
   pszBufferA[ cchMax-1 ] = L'\0';

   wsprintfW (pszTargetW, L"%hs", pszBufferA);
}

void CopyAnsiToString (LPTSTR pszTarget, LPCSTR pszOriginalA, size_t cchMax)
{
#ifdef UNICODE
   CopyAnsiToUnicode ((LPWSTR)pszTarget, pszOriginalA, cchMax);
#else
   lstrzcpy ((LPSTR)pszTarget, pszOriginalA, cchMax);
#endif
}

void CopyUnicodeToString (LPTSTR pszTarget, LPCWSTR pszOriginalW, size_t cchMax)
{
#ifdef UNICODE
   lstrzcpy ((LPWSTR)pszTarget, pszOriginalW, cchMax);
#else
   CopyUnicodeToAnsi ((LPSTR)pszTarget, pszOriginalW, cchMax);
#endif
}

void CopyStringToUnicode (LPWSTR pszTargetW, LPCTSTR pszOriginal, size_t cchMax)
{
#ifdef UNICODE
   lstrzcpy (pszTargetW, (LPWSTR)pszOriginal, cchMax);
#else
   CopyAnsiToUnicode (pszTargetW, (LPSTR)pszOriginal, cchMax);
#endif
}

void CopyStringToAnsi (LPSTR pszTargetA, LPCTSTR pszOriginal, size_t cchMax)
{
#ifdef UNICODE
   CopyUnicodeToAnsi (pszTargetA, (LPWSTR)pszOriginal, cchMax);
#else
   lstrzcpy (pszTargetA, (LPSTR)pszOriginal, cchMax);
#endif
}


void FreeString (LPCVOID pszString, LPCVOID pszOriginalString)
{
   if ( (pszString != NULL) && (pszString != pszOriginalString) )
      {
      Free ((PVOID)pszString);
      }
}


LPSTR StringToAnsi (LPCTSTR pszOriginal)
{
    if (!pszOriginal)
        return NULL;
    LPSTR pszTargetA;
    int len = lstrlen(pszOriginal);
    if ((pszTargetA = AllocateAnsi (1+len)) != NULL) {
#ifdef UNICODE
        CopyUnicodeToAnsi (pszTargetA, pszOriginal);
#else
        lstrcpy (pszTargetA, (LPSTR)pszOriginal);
#endif
    }
    return pszTargetA;
}

LPTSTR AnsiToString (LPCSTR pszOriginalA)
{
    if (!pszOriginalA)
        return NULL;
    LPTSTR pszTarget;
    int lenA = lstrlenA(pszOriginalA);
    if ((pszTarget = AllocateString (1+lenA)) != NULL) {
#ifdef UNICODE
        CopyAnsiToUnicode (pszTarget, pszOriginalA);
#else
        lstrcpy (pszTarget, (LPSTR)pszOriginalA);
#endif
    }
    return pszTarget;
}


LPWSTR StringToUnicode (LPCTSTR pszOriginal)
{
    if (!pszOriginal)
        return NULL;
    LPWSTR pszTargetW;
    int len = lstrlen(pszOriginal);
    if ((pszTargetW = AllocateUnicode (1+len)) != NULL) {
#ifdef UNICODE
        lstrcpyW ((LPWSTR)pszTargetW, (LPWSTR)pszOriginal);
#else
        CopyAnsiToUnicode (pszTargetW, pszOriginal);
#endif
    }
    return pszTargetW;
}

LPTSTR UnicodeToString (LPCWSTR pszOriginalW)
{
    if (!pszOriginalW)
        return NULL;
    LPTSTR pszTarget;
    if ((pszTarget = AllocateString (1+lstrlenW(pszOriginalW))) != NULL) {
#ifdef UNICODE
        lstrcpyW ((LPWSTR)pszTargetW, (LPWSTR)pszOriginal);
#else
        CopyUnicodeToAnsi (pszTarget, pszOriginalW);
#endif
    }
    return pszTarget;
}


LPWSTR AnsiToUnicode (LPCSTR pszOriginalA)
{
   if (!pszOriginalA)
      return NULL;
   LPWSTR pszTargetW;
   if ((pszTargetW = AllocateUnicode (1+lstrlenA(pszOriginalA))) != NULL)
      CopyAnsiToUnicode (pszTargetW, pszOriginalA);
   return pszTargetW;
}

LPSTR UnicodeToAnsi (LPCWSTR pszOriginalW)
{
   if (!pszOriginalW)
      return NULL;
   LPSTR pszTargetA;
   if ((pszTargetA = AllocateAnsi (1+lstrlenW(pszOriginalW))) != NULL)
      CopyUnicodeToAnsi (pszTargetA, pszOriginalW);
   return pszTargetA;
}


LPTSTR CloneAnsi (LPSTR pszOriginalA)
{
   if (!pszOriginalA)
      return NULL;

   LPTSTR pszTarget;

   if ((pszTarget = AllocateString (1+lstrlenA(pszOriginalA))) != NULL)
      {
#ifdef UNICODE
      CopyAnsiToUnicode ((LPWSTR)pszTarget, pszOriginalA);
#else
      lstrcpyA ((LPSTR)pszTarget, pszOriginalA);
#endif
      }

   return pszTarget;
}


LPTSTR CloneUnicode (LPWSTR pszOriginalW)
{
   if (!pszOriginalW)
      return NULL;

   LPTSTR pszTarget;

   if ((pszTarget = AllocateString (1+lstrlenW(pszOriginalW))) != NULL)
      {
#ifdef UNICODE
      lstrcpyW ((LPWSTR)pszTarget, pszOriginalW);
#else
      CopyUnicodeToAnsi ((LPSTR)pszTarget, pszOriginalW);
#endif
      }

   return pszTarget;
}


LPTSTR CloneMultiString (LPCTSTR mszOriginal)
{
   if (!mszOriginal)
      return NULL;

   size_t cchOld = 1; // for the second NUL-terminator
   for (LPCTSTR psz = mszOriginal; psz && *psz; psz += 1+lstrlen(psz))
      cchOld += lstrlen(psz) +1;

   LPTSTR pszTarget;
   if ((pszTarget = AllocateString (cchOld)) != NULL)
      memcpy (pszTarget, mszOriginal, cchOld);

   return pszTarget;
}


LPTSTR CloneString (LPTSTR pszOriginal)
{
   if (!pszOriginal)
      return NULL;

   LPTSTR pszTarget;
   if ((pszTarget = AllocateString (1+lstrlen(pszOriginal))) != NULL)
      lstrcpy (pszTarget, pszOriginal);

   return pszTarget;
}


LPTSTR FixFormatString (LPTSTR pszFormat)
{
   static TCHAR szFormat[ 256 ];
   lstrcpy (szFormat, TEXT("%s"));

   if (*pszFormat == TEXT('%'))
      {
      lstrcpy (szFormat, pszFormat);

      LPTSTR pch;
      for (pch = &szFormat[1]; *pch; ++pch)
         {
         if (!isfmtgarbage(*pch))
            {
            lstrcpy (pch, TEXT("s"));
            break;
            }
         }
      if (!*pch)
         {
         lstrcat (pch, TEXT("s"));
         }
      }

   return szFormat;
}


