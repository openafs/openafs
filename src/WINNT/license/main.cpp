/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "multistring.h"

#ifndef iswhite
#define iswhite(_ch) (((_ch)==' ') || ((_ch)=='\t'))
#endif
#ifndef iseol
#define iseol(_ch) (((_ch)=='\n') || ((_ch)=='\r'))
#endif
#ifndef iswhiteeol
#define iswhiteeol(_ch) (iswhite(_ch) || iseol(_ch))
#endif

namespace g
   {
   int CodePage;
   };


LPTSTR EscapeSpecialCharacters (LPTSTR pachIn, size_t cchIn)
{
   static LPTSTR pszOut = NULL;
   static size_t cchOut = 0;

   size_t cchReq = cchIn * 2 + 1;
   if (pszOut && (cchOut < cchReq))
      {
      GlobalFree ((HGLOBAL)pszOut);
      pszOut = NULL;
      cchOut = 0;
      }

   if (!pszOut)
      {
      pszOut = (LPTSTR)GlobalAlloc (GMEM_FIXED, cchReq);
      cchOut = cchReq;
      }

   if (pszOut)
      {
      LPTSTR pchOut = pszOut;

      for ( ; cchIn; --cchIn)
         {
         if (*pachIn == '}')
            {
            *pchOut++ = TEXT('\\');
            *pchOut++ = TEXT('\'');
            *pchOut++ = TEXT('7');
            *pchOut++ = TEXT('D');
            pachIn++;
            }
         else
            {
            if (strchr ("\\{", *pachIn))
               *pchOut++ = TEXT('\\');
            *pchOut++ = *pachIn++;
            }
         }

      *pchOut = TEXT('\0');
      }

   return pszOut;
}


BOOL FormatFile (LPTSTR pszName, LPTSTR pszText)
{
   // Find the appropriate output filename
   //
   TCHAR szName[ MAX_PATH ];
   lstrcpy (szName, pszName);

   LPTSTR pchSlash = NULL;
   for (LPTSTR pch = szName; *pch; ++pch)
      {
      if (*pch == TEXT('\\'))
         pchSlash = NULL;
      else if (*pch == TEXT('.'))
         pchSlash = pch;
      }
   if (pchSlash)
      lstrcpy (pchSlash, TEXT(".rtf"));
   else // (!pchSlash)
      lstrcat (szName, TEXT(".rtf"));

   // Open an output file handle
   //
   HANDLE hFile;
   if ((hFile = CreateFile (szName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL)) == INVALID_HANDLE_VALUE)
      {
      printf ("failed to create %s; error %lu\n", szName, GetLastError());
      return FALSE;
      }

   // Write the RTF prolog
   //
   char *pszPROLOG = "{\\rtf1\\ansi\\deff0\\deftab720\\ansicpg%lu\n"
                     "{\\colortbl\\red0\\green0\\blue0;}\\pard";

   char szProlog[ 1024 ];
   wsprintf (szProlog, pszPROLOG, g::CodePage);

   DWORD dwWrote;
   WriteFile (hFile, szProlog, lstrlen(szProlog), &dwWrote, NULL);

   // Translate the file itself
   //
   BOOL fAllowCRLF = FALSE;
   BOOL fInFormatted = FALSE;
   size_t cFormatted = FALSE;
   LPTSTR pchNext = NULL;
   for (LPTSTR pchRead = pszText; pchRead && *pchRead; pchRead = pchNext)
      {
      while (iswhiteeol(*pchRead))
         ++pchRead;
      if (!*pchRead)
         break;

      if (*pchRead == '<')
         {
         pchNext = &pchRead[1];
         while (*pchNext && (*pchNext != '>'))
            ++pchNext;
         if (*pchNext == '>')
            ++pchNext;

         // If this was a "<p>", write an EOL.
         // If this was a "<d>", write paragraph-header formatting info.
         // If this was a "<?>", write an EOL.
         //
         if (tolower(pchRead[1]) == '?')
            {
            if (fAllowCRLF)
               WriteFile (hFile, "\r\n\\par ", lstrlen("\r\n\\par "), &dwWrote, NULL);
            }
         else if (tolower(pchRead[1]) == 'p')
            {
            if (fAllowCRLF)
               WriteFile (hFile, "\r\n\\par \r\n\\par ", lstrlen("\r\n\\par \r\n\\par "), &dwWrote, NULL);
            if (fInFormatted)
               {
               char *pszPLAIN = "\\plain\\fs20 ";
               WriteFile (hFile, pszPLAIN, lstrlen(pszPLAIN), &dwWrote, NULL);
               }
            fInFormatted = FALSE;
            }
         else if (tolower(pchRead[1]) == 'd')
            {
            if (fAllowCRLF)
               WriteFile (hFile, "\r\n\\par \r\n\\par ", lstrlen("\r\n\\par \r\n\\par "), &dwWrote, NULL);

            char *pszWrite;
            if ((++cFormatted) <= 2)
               pszWrite = "\\plain\\fs28\\b ";
            else // (cFormatted > 2)
               pszWrite = "\\plain\\fs24\\b ";

            WriteFile (hFile, pszWrite, lstrlen(pszWrite), &dwWrote, NULL);

            fInFormatted = TRUE;
            }
         }
      else // (*pchRead != '<')
         {
         pchNext = &pchRead[1];
         while (*pchNext && (*pchNext != '<') && !iseol(*pchNext))
            ++pchNext;

         LPTSTR pszEscaped;
         if ((pszEscaped = EscapeSpecialCharacters (pchRead, pchNext - pchRead)) == NULL)
            break;

         WriteFile (hFile, pszEscaped, lstrlen(pszEscaped), &dwWrote, NULL);
         fAllowCRLF = TRUE;
         }
      }

   // Write the RTF trailer
   //
   char *pszTRAILER = "\\par }";

   WriteFile (hFile, pszTRAILER, lstrlen(pszTRAILER), &dwWrote, NULL);

   SetEndOfFile (hFile);
   CloseHandle (hFile);
   return TRUE;
}


BOOL TranslateFile (LPTSTR psz)
{
   BOOL rc = TRUE;

   // First open the file and read it into memory.
   //
   HANDLE hFile;
   if ((hFile = CreateFile (psz, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
      {
      printf ("failed to open %s; error %lu\n", psz, GetLastError());
      return FALSE;
      }

   size_t cbSource;
   if ((cbSource = GetFileSize (hFile, NULL)) != 0)
      {
      LPTSTR abSource = (LPTSTR)GlobalAlloc (GMEM_FIXED, cbSource + 5);

      DWORD dwRead;
      if (!ReadFile (hFile, abSource, cbSource, &dwRead, NULL))
         {
         printf ("failed to read %s; error %lu\n", psz, GetLastError());
         rc = FALSE;
         }
      else
         {
         abSource[ dwRead ] = 0;
         size_t cbTarget = dwRead * 4;
         LPSTR abTarget = (LPSTR)GlobalAlloc (GMEM_FIXED, cbTarget);
         memset (abTarget, 0x00, cbTarget);

         BOOL fDefault = FALSE;
         WideCharToMultiByte (g::CodePage, 0, (LPCWSTR)abSource, dwRead, abTarget, cbTarget, TEXT(" "), &fDefault);

         rc = FormatFile (psz, abTarget);

         GlobalFree ((HGLOBAL)abTarget);
         }

      GlobalFree ((HGLOBAL)abSource);
      }

   CloseHandle (hFile);
   return rc;
}


LPTSTR FindFullPath (LPTSTR pszFormat, LPTSTR pszBaseName)
{
   static TCHAR szOut[ MAX_PATH ];
   lstrcpy (szOut, pszFormat);

   LPTSTR pchSlash = NULL;
   for (LPTSTR pch = szOut; *pch; ++pch)
      if ((*pch == TEXT('\\')) || (*pch == TEXT('/')))
         pchSlash = pch;
   if (pchSlash)
      lstrcpy (pchSlash, TEXT("\\"));
   else
      szOut[0] = TEXT('\0');
   lstrcat (szOut, pszBaseName);
   return szOut;
}


void main (int argc, char **argv)
{
   int cFilesRequested = 0;

   g::CodePage = CP_ACP;

   for (--argc,++argv; argc; --argc,++argv)
      {
      if ((argv[0][0] == '-') || (argv[0][0] == '/'))
         {
         g::CodePage = atol(&argv[0][1]);
         }
      else // ((argv[0][0] != '-') && (argv[0][0] != '/'))
         {
         WIN32_FIND_DATA Data;

         HANDLE hFind;
         if ((hFind = FindFirstFile (argv[0], &Data)) == INVALID_HANDLE_VALUE)
            {
            printf ("file %s not found\n", argv[0]);
            }
         else
            {
            LPTSTR pszNames = NULL;

            do {
               if (!(Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                  mstrcat (&pszNames, TEXT('\0'), FindFullPath (argv[0], Data.cFileName));
               } while (FindNextFile (hFind, &Data));

            FindClose (hFind);

            if (pszNames)
               {
               for (LPTSTR psz = pszNames; psz && *psz; psz += 1+lstrlen(psz))
                  {
                  printf ("translating %s into rtf...\n", psz);
                  TranslateFile (psz);
                  }
               mstrfree (pszNames);
               }
            }

         cFilesRequested++;
         }
      }

   if (!cFilesRequested)
      {
      printf ("format : sgml2rtf filename {...}\r\n\r\n");
      }

   exit(0);
}

